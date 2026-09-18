from typing import Any

def number(kind: str, low, high, label: str, dangerous: bool = False) -> dict[str, Any]:
    return {"type": kind, "min": low, "max": high, "label": label, "dangerous": dangerous}


def switch(label: str) -> dict[str, Any]:
    return {"type": "bool", "label": label, "dangerous": True}


SCHEMA: dict[str, dict[str, Any]] = {
    "name": {"type": "str", "max_len": 31, "label": "Название станции", "dangerous": False},

    "publish_interval_ms": number("int", 5000, 3_600_000, "Интервал замера, мс", True),
    "status_interval_ms": number("int", 5000, 600_000, "Интервал публикации статуса, мс", True),
    "filter_size": number("int", 1, 15, "Глубина усреднения"),
    "gps_min_sats": number("int", 0, 12, "Минимум спутников для фикса"),
    "temp_offset": number("float", -20, 20, "Калибровка температуры, °C"),
    "tds_offset": number("float", -500, 500, "Калибровка TDS, мг/л"),
    "tds_scale": number("float", 0.1, 10, "Множитель TDS/EC"),

    "enable_bme": switch("Метеоблок BME280"),
    "enable_tds": switch("Датчик TDS/EC"),
    "enable_gps": switch("GPS"),
    "enable_battery": switch("Мониторинг питания"),
    "enable_sd": switch("SD-карта"),
    "sd_buffering": switch("Буферизация при обрыве связи"),
    "ota_enabled": switch("Обновление по воздуху"),
}

DEFAULTS: dict[str, Any] = {
    "name": "Станция 1",
    "publish_interval_ms": 60000,
    "status_interval_ms": 30000,
    "filter_size": 5,
    "gps_min_sats": 4,
    "temp_offset": 0.0,
    "tds_offset": 0.0,
    "tds_scale": 1.0,
    "enable_bme": True,
    "enable_tds": True,
    "enable_gps": True,
    "enable_battery": True,
    "enable_sd": True,
    "sd_buffering": True,
    "ota_enabled": True,
}


class SettingsError(ValueError):
    pass


def validate(patch: dict[str, Any]) -> dict[str, Any]:
    clean: dict[str, Any] = {}
    for key, value in patch.items():
        if key in ("rev",):
            continue
        spec = SCHEMA.get(key)
        if spec is None:
            raise SettingsError(f"Неизвестный параметр: {key}")

        if spec["type"] == "bool":
            if not isinstance(value, bool):
                raise SettingsError(f"{spec['label']}: ожидается да/нет")
            clean[key] = value
        elif spec["type"] == "str":
            if not isinstance(value, str) or not value.strip():
                raise SettingsError(f"{spec['label']}: ожидается непустая строка")
            clean[key] = value.strip()[: spec["max_len"]]
        else:
            if isinstance(value, bool) or not isinstance(value, (int, float)):
                raise SettingsError(f"{spec['label']}: ожидается число")
            if not (spec["min"] <= value <= spec["max"]):
                raise SettingsError(f"{spec['label']}: допустимо от {spec['min']} до {spec['max']}")
            clean[key] = int(value) if spec["type"] == "int" else float(value)
    return clean


def dangerous_changes(current: dict[str, Any], patch: dict[str, Any]) -> list[str]:
    out: list[str] = []
    for key, value in patch.items():
        spec = SCHEMA.get(key)
        if not spec or not spec.get("dangerous"):
            continue
        old = current.get(key, DEFAULTS.get(key))
        if old == value:
            continue
        if spec["type"] == "bool" and value is False:
            out.append(f"Отключение: {spec['label']}")
        elif spec["type"] == "int" and isinstance(old, (int, float)) and value > old:
            out.append(f"{spec['label']}: {old} → {value}")
    return out


DANGEROUS_COMMANDS = {"reboot", "factory_reset", "clear_spool"}
KNOWN_COMMANDS = {"ping", "publish_now", "get_status", "reboot", "factory_reset", "clear_spool"}
