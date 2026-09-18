# Станция мониторинга на ESP32: сборка, прошивка, диагностика

Инструкция к прошивке из папки `firmware/`. Ссылки на документацию
производителей приведены в тексте.

---

## 1. Состав железа

| Узел | Модуль | Шина | Адрес / пины |
|---|---|---|---|
| Контроллер | ESP32 DevKit (WROOM-32) | — | — |
| Метео | BME280 | I²C | `0x76` (или `0x77`) |
| Часы | DS3231 + батарейка CR2032 | I²C | `0x68` |
| Вода | TDS/EC-метр Trema Flash-I²C | I²C | `0x09` |
| Питание | iArduino Battery Shield | I²C | `0x35` (или `0x75`) |
| Навигация | GPS-модуль NMEA, 9600 бод | UART2 | GPIO16 / GPIO17 |
| Буфер | microSD-модуль | SPI (VSPI) | CS — GPIO5 |

### Распиновка

Шина I²C общая для четырёх модулей:

```
ESP32 GPIO21 (SDA) ── SDA всех I²C-модулей
ESP32 GPIO22 (SCL) ── SCL всех I²C-модулей
ESP32 GND          ── GND всех модулей (общая земля обязательна)
ESP32 3V3          ── Vcc BME280, DS3231
```

TDS-метр и Battery Shield питаются от 3,3–5 В. Если запитать их от 5 В,
подтяжка линий I²C может уйти к 5 В, а для входов ESP32 это уже за
пределами даташита. Либо питайте всё от 3,3 В, либо ставьте
двунаправленный преобразователь уровней на SDA и SCL.

GPS садится на UART2, RX и TX подключаются накрест:

```
ESP32 GPIO16 (RX2) ── TX модуля GPS
ESP32 GPIO17 (TX2) ── RX модуля GPS
ESP32 GND          ── GND
ESP32 3V3 или 5V   ── Vcc (смотрите даташит своего модуля)
```

microSD подключается к аппаратному VSPI:

```
GPIO18 ── SCK
GPIO19 ── MISO
GPIO23 ── MOSI
GPIO5  ── CS
3V3    ── Vcc     (5 В только если на модуле есть регулятор)
GND    ── GND
```

Отдельно про Battery Shield. Он сделан в форм-факторе платы расширения
Arduino Uno и на ESP32 DevKit физически не садится, так что четыре
провода (SDA, SCL, GND, Vcc) подключаются вручную. Питание ESP32 берётся
с выхода 5 V шилда на вывод VIN платы.

Две вещи из документации производителя, о которых легко забыть: перед
первым использованием после покупки подайте питание на micro-USB шилда
хотя бы на 2 секунды, а перед установкой дважды нажмите кнопку на плате,
чтобы выключить модуль.

---

## 2. Среда и библиотеки

### Поддержка ESP32 в Arduino IDE

Файл → Настройки → «Дополнительные ссылки для менеджера плат»:

```
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

Дальше Инструменты → Плата → Менеджер плат → установить **esp32 by
Espressif Systems**. Плата — `ESP32 Dev Module`, Partition Scheme любая
с поддержкой OTA (та, что по умолчанию, подходит).

Исходники ядра: https://github.com/espressif/arduino-esp32

### Библиотеки

| Библиотека | Версия | Где взять |
|---|---|---|
| **ArduinoJson** | **≥ 7.0** | Менеджер библиотек, либо https://github.com/bblanchon/ArduinoJson |
| **PubSubClient** | любая актуальная | Менеджер библиотек, либо https://github.com/knolleary/pubsubclient |
| Adafruit BME280 | любая актуальная | https://github.com/adafruit/Adafruit_BME280_Library |
| Adafruit Unified Sensor | зависимость BME280 | https://github.com/adafruit/Adafruit_Sensor |
| Adafruit BusIO | зависимость BME280 | https://github.com/adafruit/Adafruit_BusIO |
| RTClib | любая актуальная | https://github.com/adafruit/RTClib |
| TinyGPSPlus | любая актуальная | https://github.com/mikalhart/TinyGPSPlus |
| iarduino_I2C_TDS | ≥ 1.3.0 | https://iarduino.ru/file/431.html · вики: https://wiki.iarduino.ru/page/TDS-EC-i2c/ |
| Battery_Shield | любая | https://github.com/tremaru/Battery_Shield · вики: https://wiki.iarduino.ru/page/Battery_Shield/ |

Библиотеки `WiFi`, `ArduinoOTA`, `Wire`, `SPI`, `SD`, `Preferences` и
`esp_task_wdt` уже входят в ядро ESP32, ставить их не нужно.

Библиотек iArduino может не оказаться в менеджере Arduino IDE. Тогда
качайте ZIP и подключайте через Скетч → Подключить библиотеку → Добавить
.ZIP-библиотеку.

ArduinoJson шестой версии не подойдёт. Код использует `JsonDocument` без
параметра размера и `doc["x"].to<JsonObject>()`, а это синтаксис седьмой
версии. На шестой вы получите ошибки вида `'JsonDocument' was not
declared in this scope`.

---

## 3. Настройка и заливка

Все четыре файла должны лежать в одной папке:

```
firmware/
  firmware.ino
  config.h
  settings.h
  spool.h
```

Имя папки обязано совпадать с именем `.ino`, иначе Arduino IDE не
откроет скетч.

В `config.h` поменяйте:

```c
#define DEVICE_ID     "station-01"     // должен совпасть с DEVICE_ID сервера
#define WIFI_SSID     "ваша_сеть"
#define WIFI_PASSWORD "ваш_пароль"
#define MQTT_SERVER   "адрес-сервера"
#define MQTT_PORT     1883
#define MQTT_USER     "station"        // как MQTT_USER в .env сервера
#define MQTT_PASSWORD "пароль_брокера" // как MQTT_PASSWORD в .env сервера
#define OTA_PASSWORD  "свой_пароль"
```

Если `DEVICE_ID` не совпадёт с `.env` сервера, мост отбросит сообщения
как пришедшие от незарегистрированной станции. Если не совпадут
`MQTT_USER` и `MQTT_PASSWORD`, подключения не будет вовсе.

Первая прошивка делается только по USB. Дальше, когда станция уже в
сети, в Arduino IDE появится сетевой порт `station-01 at <IP>`, и можно
заливать по воздуху, введя `OTA_PASSWORD`.

SD-карта нужна в FAT32, объёмом до 32 ГБ. Перед первым запуском её лучше
отформатировать, папку `/spool` прошивка создаст сама.

Без батарейки CR2032 модуль DS3231 теряет время при каждом отключении.
Прошивка это переживёт, сервер подставит своё время и в логах появится
`rtc_lost_power`, но у истории измерений из SD-буфера будут неточные
метки.

---

## 4. Что должно появиться в мониторе порта

Скорость порта — 115200.

```
=== Станция мониторинга, fw 4.0.0 ===
[info] bme_ok: BME280 инициализирован
Инициализация завершена
WiFi OK, IP: 192.168.1.42
[info] mqtt_connected: Соединение с брокером установлено
{"seq":1,"ts":1757942400,"ts_valid":true,"rssi":-58,"temp":21.4,...}
```

Если строки `bme_ok` нет или появилось что-то с суффиксом `_fail`, модуль
не отозвался на своём адресе. Станция продолжит работать, просто
соответствующие поля телеметрии будут `null`.

---