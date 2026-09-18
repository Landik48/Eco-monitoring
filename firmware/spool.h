#pragma once
#include <Arduino.h>
#include <SD.h>
#include <functional>
#include "config.h"

class Spool {
public:
  bool begin() {
    if (!SD.exists(SPOOL_DIR)) SD.mkdir(SPOOL_DIR);

    File dir = SD.open(SPOOL_DIR);
    if (!dir || !dir.isDirectory()) { _ok = false; return false; }

    uint32_t mn = UINT32_MAX, mx = 0;
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
      String n = f.name();
      f.close();
      int slash = n.lastIndexOf('/');
      if (slash >= 0) n = n.substring(slash + 1);
      if (!n.endsWith(".ndj")) continue;
      uint32_t idx = strtoul(n.c_str(), nullptr, 10);
      if (!idx) continue;
      if (idx < mn) mn = idx;
      if (idx > mx) mx = idx;
    }
    dir.close();

    if (!mx) { _readIdx = _writeIdx = 1; _readOff = 0; }
    else {
      _readIdx = mn; _writeIdx = mx; _readOff = 0;
      loadState();
      if (_readIdx < mn || _readIdx > mx) { _readIdx = mn; _readOff = 0; }
    }
    _ok = true;
    return true;
  }

  bool ready() const { return _ok; }
  bool pending()     { return _ok && SD.exists(path(_readIdx)); }

  bool append(const char* line) {
    if (!_ok) return false;
    ensureFreeSpace();

    File f = SD.open(path(_writeIdx), FILE_APPEND);
    if (!f) return false;
    if (f.size() >= SPOOL_MAX_FILE_BYTES) {
      f.close();
      _writeIdx++;
      f = SD.open(path(_writeIdx), FILE_APPEND);
      if (!f) return false;
    }
    size_t written = f.println(line);
    f.close();
    return written > 0;
  }

  uint16_t drainLines(std::function<bool(const char*)> send, uint16_t maxLines) {
    if (!_ok) return 0;
    uint16_t sent = 0;

    while (sent < maxLines) {
      String p = path(_readIdx);
      if (!SD.exists(p)) {
        if (_readIdx >= _writeIdx) { _readOff = 0; break; }
        _readIdx++; _readOff = 0; saveState();
        continue;
      }

      File f = SD.open(p, FILE_READ);
      if (!f) break;
      if (_readOff > f.size()) _readOff = 0;
      f.seek(_readOff);

      bool stopped = false;
      while (sent < maxLines && f.available()) {
        uint32_t before = f.position();
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() >= 2 && line[0] == '{') {
          if (!send(line.c_str())) { _readOff = before; stopped = true; break; }
          sent++;
        }
        _readOff = f.position();
      }
      bool eof = !stopped && !f.available();
      f.close();
      saveState();

      if (stopped) break;
      if (eof) {
        SD.remove(p);
        if (_readIdx < _writeIdx) { _readIdx++; _readOff = 0; saveState(); }
        else { _readOff = 0; }
      } else break;
    }
    return sent;
  }

  void clear() {
    if (!_ok) return;
    File dir = SD.open(SPOOL_DIR);
    if (!dir) return;
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
      String n = f.name();
      f.close();
      if (n.endsWith(".ndj")) SD.remove(n.startsWith("/") ? n : String(SPOOL_DIR "/") + n);
    }
    dir.close();
    _readIdx = _writeIdx = 1;
    _readOff = 0;
    saveState();
  }

  void stats(uint16_t& files, uint32_t& bytes) {
    files = 0; bytes = 0;
    if (!_ok) return;
    File dir = SD.open(SPOOL_DIR);
    if (!dir) return;
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
      String n = f.name();
      if (n.endsWith(".ndj")) { files++; bytes += f.size(); }
      f.close();
    }
    dir.close();
  }

private:
  bool     _ok = false;
  uint32_t _readIdx = 1, _writeIdx = 1, _readOff = 0;

  static String path(uint32_t i) {
    char b[40];
    snprintf(b, sizeof(b), SPOOL_DIR "/%06lu.ndj", (unsigned long)i);
    return String(b);
  }

  void saveState() {
    File f = SD.open(SPOOL_STATE_FILE, FILE_WRITE);
    if (!f) return;
    f.printf("%lu %lu", (unsigned long)_readIdx, (unsigned long)_readOff);
    f.close();
  }

  void loadState() {
    File f = SD.open(SPOOL_STATE_FILE, FILE_READ);
    if (!f) return;
    String s = f.readString();
    f.close();
    int sp = s.indexOf(' ');
    if (sp <= 0) return;
    _readIdx = strtoul(s.substring(0, sp).c_str(), nullptr, 10);
    _readOff = strtoul(s.substring(sp + 1).c_str(), nullptr, 10);
  }

  void ensureFreeSpace() {
    uint64_t total = SD.totalBytes();
    if (!total) return;
    if (total - SD.usedBytes() >= SPOOL_MIN_FREE_BYTES) return;

    String p = path(_readIdx);
    if (SD.exists(p)) SD.remove(p);
    if (_readIdx < _writeIdx) _readIdx++;
    _readOff = 0;
    saveState();
  }
};
