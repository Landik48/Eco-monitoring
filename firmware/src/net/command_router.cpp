#include "command_router.h"

#include "../../config.h"
#include "../core/timekeeper.h"
#include "../diag/logger.h"
#include "mqtt_link.h"
#include "time_sync.h"

namespace {
CommandRouter::Handlers h;

bool isDangerous(const char* cmd) {
  return !strcmp(cmd, "reboot") ||
         !strcmp(cmd, "factory_reset") ||
         !strcmp(cmd, "clear_spool");
}
}

namespace CommandRouter {
void begin(const Handlers& handlers) { h = handlers; }

void sendAck(const String& id, bool ok, const String& error) {
  if (id.isEmpty()) return;

  JsonDocument d;
  d["id"] = id;
  d["ok"] = ok;
  d["ts"] = TimeSync::now();
  if (!ok) d["error"] = error;

  String out;
  serializeJson(d, out);
  MqttLink::publish(TOPIC_ACK, out.c_str(), false);
}

void execute(JsonObjectConst c) {
  const String id = c["id"] | "";
  const char* cmd = c["cmd"] | "";

  if (!*cmd) {
    sendAck(id, false, "не указано поле cmd");
    return;
  }

  if (isDangerous(cmd) && !(c["confirm"] | false)) {
    sendAck(id, false, "требуется confirm: true");
    return;
  }

  if (!strcmp(cmd, "ping")) {
    sendAck(id, true, "");

  } else if (!strcmp(cmd, "publish_now")) {
    if (h.publishNow) h.publishNow();
    sendAck(id, true, "");

  } else if (!strcmp(cmd, "get_status")) {
    if (h.publishStatus) h.publishStatus();
    sendAck(id, true, "");

  } else if (!strcmp(cmd, "sync_time")) {
    if (h.syncTime) h.syncTime();
    sendAck(id, true, "");

  } else if (!strcmp(cmd, "set_time")) {
    JsonVariantConst v = c["args"]["epoch"];
    if (v.isNull()) v = c["epoch"];
    const uint32_t epoch = v | 0UL;

    if (!TimeKeeper::sane(epoch)) {
      sendAck(id, false, "epoch вне допустимого диапазона");
      return;
    }
    const bool ok = h.setTime ? h.setTime(epoch) : false;
    sendAck(id, true, "");
    if (!ok) {
      Logger::info("time_noop", "Время сервера совпало с текущим, правка не нужна");
    }

  } else if (!strcmp(cmd, "reset_stability")) {
    if (h.resetStability) h.resetStability();
    sendAck(id, true, "");
    Logger::info("stability_reset", "Статистика нестабильности сброшена");

  } else if (!strcmp(cmd, "clear_spool")) {
    if (h.clearSpool) h.clearSpool();
    sendAck(id, true, "");
    Logger::warn("spool_cleared", "Буфер SD очищен по команде");

  } else if (!strcmp(cmd, "factory_reset")) {
    if (h.factoryReset) h.factoryReset();
    sendAck(id, true, "");
    if (h.scheduleReboot) h.scheduleReboot(3000);

  } else if (!strcmp(cmd, "reboot")) {
    sendAck(id, true, "");
    if (h.scheduleReboot) h.scheduleReboot(3000);

  } else {
    sendAck(id, false, "неизвестная команда");
  }
}

}
