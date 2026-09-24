from datetime import datetime, timezone
from typing import Any

LEVELS = ["good", "moderate", "elevated", "high", "hazard"]

LEVEL_LABELS = {
    "good": "Норма",
    "moderate": "Удовлетворительно",
    "elevated": "Повышенный уровень",
    "high": "Превышение норматива",
    "hazard": "Опасный уровень",
    "unknown": "Нет данных",
}

LIMITS: dict[str, dict[str, Any]] = {}

LIMITS["tds"] = {
    "label": "Минерализация воды (TDS)",
    "unit": "мг/л",
    "kind": "pollutant",
    "bands": [300, 600, 1000, 1500],
    "pdk": 1000,
    "norm": "ПДК 1000 мг/л, СанПиН 1.2.3685-21",
    "category": "water",
}

LIMITS["ec"] = {
    "label": "Удельная электропроводность",
    "unit": "мкСм/см",
    "kind": "pollutant",
    "bands": [500, 1000, 1800, 2500],
    "pdk": 1800,
    "norm": "Косвенный показатель солесодержания",
    "category": "water",
}

LIMITS["temp"] = {
    "label": "Температура воздуха",
    "unit": "°C",
    "kind": "comfort",
    "optimal": (18, 24),
    "allowed": (12, 28),
    "norm": "Комфортный диапазон по ГОСТ 30494-2011",
    "category": "air",
}

LIMITS["hum"] = {
    "label": "Относительная влажность",
    "unit": "%",
    "kind": "comfort",
    "optimal": (30, 60),
    "allowed": (20, 70),
    "norm": "Оптимум 30–60 %, ГОСТ 30494-2011",
    "category": "air",
}

LIMITS["pres"] = {
    "label": "Атмосферное давление",
    "unit": "гПа",
    "kind": "comfort",
    "optimal": (1000, 1025),
    "allowed": (985, 1040),
    "norm": "Нормальное давление около 1013 гПа",
    "category": "air",


    "drives_overall": False,
}

LIMITS["vbat"] = {
    "label": "Напряжение батареи",
    "unit": "В",
    "kind": "comfort",
    "optimal": (3.7, 4.2),
    "allowed": (3.4, 4.3),
    "norm": "Li-ion: ниже 3,4 В станция скоро отключится",
    "category": "system",
}

LIMITS["ibat"] = {
    "label": "Ток потребления",
    "unit": "А",
    "kind": "comfort",
    "optimal": (-0.5, 0.5),
    "allowed": (-1.5, 1.5),
    "norm": "Отрицательный ток — разряд, положительный — заряд от панели",
    "category": "system",
}


def _pollutant_level(value, bands, pdk):
    if value <= bands[0]:
        level = "good"
    elif value <= bands[1]:
        level = "moderate"
    elif value <= bands[2]:
        level = "elevated"
    elif value <= bands[3]:
        level = "high"
    else:
        level = "hazard"

    index = value / pdk * 100
    if index > 125:
        index = 125.0
    return level, index


def _comfort_level(value, optimal, allowed):
    if optimal[0] <= value <= optimal[1]:
        return "good", 20.0
    if allowed[0] <= value <= allowed[1]:
        return "moderate", 45.0

    if value > allowed[1]:
        away = value - allowed[1]
    else:
        away = allowed[0] - value

    half = (allowed[1] - allowed[0]) / 2
    if away <= half:
        return "high", 80.0
    return "hazard", 110.0


TDS_COMMENTS = {
    "good": "Вода слабоминерализованная, отклонений нет.",
    "moderate": "Солесодержание в пределах нормы.",
    "elevated": "Приближается к ПДК 1000 мг/л, стоит следить за динамикой.",
    "high": "Превышен норматив питьевой воды. Без водоподготовки не использовать.",
    "hazard": "Сильное превышение: вероятен сброс или засоление источника.",
}


def _metric_comment(key, value, level):
    if key == "tds":
        return TDS_COMMENTS.get(level, "")

    if key == "ec":
        return "Электропроводность соответствует измеренной минерализации."

    if key == "temp":
        if value < 0:
            return "Отрицательная температура: возможно обмерзание датчика TDS."
        if level == "good":
            return "Температура в комфортном диапазоне."
        return "Температура вне комфортного диапазона."

    if key == "hum":
        if value > 70:
            return "Высокая влажность: риск конденсата в корпусе станции."
        if value < 20:
            return "Воздух пересушен."
        return "Влажность в норме."

    if key == "pres":
        if level in ("good", "moderate"):
            return "Давление в норме."
        return "Аномальное давление, вероятна смена погоды."

    if key == "vbat":
        if level in ("high", "hazard"):
            return "Батарея разряжена, станция скоро уйдёт в офлайн."
        return "Питание в норме."

    if key == "ibat":
        if value > 0.05:
            return "Идёт заряд от солнечной панели."
        if value < -1.0:
            return "Повышенное потребление: проверьте, нет ли короткого замыкания."
        return "Станция работает от аккумулятора."

    return ""


FLAG_LABELS: dict[int, str] = {
    1 << 0: "просадка напряжения",
    1 << 1: "бросок тока",
    1 << 2: "скачок температуры",
    1 << 3: "скачок влажности",
    1 << 4: "скачок давления",
    1 << 5: "скачок TDS",
    1 << 6: "скачок EC",
    1 << 7: "отказ датчика",
    1 << 8: "несвязанные каналы разом",
    1 << 9: "изменение подтверждено",
}

STABILITY_TITLES = {
    "ok": "Станция работает стабильно",
    "watch": "Замечены признаки нестабильности",
    "unstable": "Станция нестабильна",
}


def describe_flags(mask: int) -> list[str]:
    if not mask:
        return []
    return [text for bit, text in FLAG_LABELS.items() if mask & bit]


def build_stability(
    status: dict[str, Any],
    suspect_samples: int,
    confirmed_samples: int,
    total_samples: int,
    flag_mask: int,
) -> dict[str, Any]:

    reported = status.get("stability") if isinstance(status, dict) else None
    reported = reported if isinstance(reported, dict) else {}

    score = reported.get("score")
    state = reported.get("state")


    if not isinstance(score, (int, float)):
        share = (suspect_samples / total_samples * 100) if total_samples else 0
        score = round(min(share * 2, 100))
    if state not in ("ok", "watch", "unstable"):
        state = "unstable" if score >= 45 else ("watch" if score >= 15 else "ok")

    reasons = describe_flags(flag_mask)

    reasons = [r for r in reasons if r != "изменение подтверждено"]

    if state == "ok":
        summary = ("Показания достоверны, признаков нестабильности "
                   "за окно наблюдения нет.")
    elif suspect_samples:
        summary = (f"Исключено из расчёта недостоверных замеров: {suspect_samples} "
                   f"из {total_samples}. Причины: {', '.join(reasons) or 'не указаны'}.")
    else:
        summary = ("Питание станции нестабильно, но показания пока "
                   "укладываются в допустимые пределы.")

    hints: list[str] = []
    if state != "ok":
        hints.append("Проверьте аккумулятор, разъёмы питания и контакт солнечной панели: "
                     "скачки показаний почти всегда идут от просадки напряжения.")
    if confirmed_samples:
        hints.append(f"Резких изменений, подтверждённых как настоящие: {confirmed_samples}. "
                     "Эти замеры учтены в расчёте.")


    vbat_min = reported.get("vbat_min")
    if not isinstance(vbat_min, (int, float)) or vbat_min <= 0:
        vbat_min = None

    return {
        "state": state,
        "score": int(score),
        "title": STABILITY_TITLES[state],
        "summary": summary,
        "suspect_samples": suspect_samples,
        "confirmed_samples": confirmed_samples,
        "total_samples": total_samples,
        "reasons": reasons,
        "hints": hints,
        "sag_events": reported.get("sag_events"),
        "vbat_min": vbat_min,
    }


def _drives_overall(spec: dict[str, Any]) -> bool:
    if spec.get("category") == "system":
        return False
    return spec.get("drives_overall", True)


def evaluate_metric(key: str, value: float | None) -> dict[str, Any] | None:
    spec = LIMITS.get(key)
    if spec is None:
        return None

    if value is None:
        return {
            "key": key,
            "label": spec["label"],
            "unit": spec["unit"],
            "category": spec["category"],
            "value": None,
            "level": "unknown",
            "index": None,
            "norm": spec["norm"],
            "comment": "Датчик отключён или не отвечает",
            "drives_overall": _drives_overall(spec),
        }

    if spec["kind"] == "pollutant":
        level, index = _pollutant_level(value, spec["bands"], spec["pdk"])
    else:
        level, index = _comfort_level(value, spec["optimal"], spec["allowed"])

    return {
        "key": key,
        "label": spec["label"],
        "unit": spec["unit"],
        "category": spec["category"],
        "value": round(value, 2),
        "level": level,
        "index": round(index, 1),
        "norm": spec["norm"],
        "comment": _metric_comment(key, value, level),
        "drives_overall": _drives_overall(spec),
    }


def _overall(metrics: list[dict[str, Any]]) -> dict[str, Any]:
    scored = []
    for m in metrics:
        if m["index"] is None:
            continue
        if m["category"] == "system":
            continue
        if not m.get("drives_overall", True):
            continue
        scored.append(m)

    if not scored:
        return {
            "index": None,
            "level": "unknown",
            "title": LEVEL_LABELS["unknown"],
            "summary": "Недостаточно данных: ни один профильный датчик не передал значений.",
        }

    worst = scored[0]
    for m in scored[1:]:
        if m["index"] > worst["index"]:
            worst = m
    level = worst["level"]
    driver = worst["label"].lower()

    bad_water = [
        m["label"].lower()
        for m in scored
        if m["category"] == "water" and m["level"] in ("elevated", "high", "hazard")
    ]

    if level in ("good", "moderate"):
        summary = ("Отклонений от нормативов не зафиксировано. "
                   "Показатели воды и микроклимата в допустимых пределах.")
    elif bad_water:
        summary = ("Зафиксировано отклонение по показателям: " + ", ".join(bad_water) +
                   f". Определяющий фактор — {driver}.")
    else:
        summary = f"Условия вне комфортного диапазона, определяющий фактор — {driver}."

    return {
        "index": worst["index"],
        "level": level,
        "title": LEVEL_LABELS[level],
        "summary": summary,
        "driver": worst["key"],
    }


def _recommendations(metrics: list[dict[str, Any]], trends: dict[str, float]) -> list[str]:
    out: list[str] = []
    by_key = {m["key"]: m for m in metrics}

    tds = by_key.get("tds")
    if tds and tds["level"] in ("high", "hazard"):
        out.append("Не использовать воду из источника без обратного осмоса или дистилляции.")
        out.append("Отобрать пробу на лабораторный анализ: TDS показывает сумму солей, "
                   "но не их состав и токсичность.")
    elif tds and tds["level"] == "elevated":
        out.append("Участить замеры TDS и сверить показания с контрольной пробой — "
                   "значение подходит к ПДК.")

    if trends.get("tds", 0) > 15:
        out.append("Минерализация устойчиво растёт — проверьте, нет ли сброса выше по течению.")

    hum = by_key.get("hum")
    if hum and hum["value"] is not None and hum["value"] > 70:
        out.append("Проверить герметичность корпуса и осушитель: при такой влажности "
                   "возможен конденсат на плате.")

    temp = by_key.get("temp")
    if temp and temp["value"] is not None and temp["value"] < 0:
        out.append("Отрицательная температура: показания TDS недостоверны, "
                   "датчик под риском обмерзания.")

    vbat = by_key.get("vbat")
    if vbat and vbat["level"] in ("high", "hazard"):
        out.append("Зарядить или заменить аккумулятор, иначе данные будут копиться "
                   "только на SD-карте.")

    if not out:
        out.append("Действий не требуется. Продолжайте наблюдение в текущем режиме.")
    return out


def build_report(
    latest: dict[str, float | None],
    averages: dict[str, float | None],
    trends: dict[str, float],
    samples: int,
    window_hours: int,
    location_name: str | None = None,
    stability: dict[str, Any] | None = None,
) -> dict[str, Any]:
    metrics = []
    for key in LIMITS:
        m = evaluate_metric(key, averages.get(key))
        if m is None:
            continue
        m["latest"] = latest.get(key)
        m["trend_pct"] = round(trends.get(key, 0.0), 1)
        metrics.append(m)

    recommendations = _recommendations(metrics, trends)
    if stability and stability.get("state") != "ok":
        recommendations = stability.get("hints", []) + recommendations

    return {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "window_hours": window_hours,
        "samples": samples,
        "location": location_name,
        "overall": _overall(metrics),
        "metrics": metrics,
        "stability": stability,
        "recommendations": recommendations,
        "disclaimer": (
            "Оценка построена по показаниям BME280 и TDS/EC: минерализация воды "
            "и микроклимат. Загрязнение воздуха (PM2.5, CO, NO₂) станцией "
            "не измеряется и в сводку не входит."
        ),
    }
