import csv
import io
from datetime import datetime, timedelta, timezone

from fastapi import APIRouter, Depends, HTTPException, Query, status
from fastapi.responses import StreamingResponse
from sqlalchemy import select, text
from sqlalchemy.ext.asyncio import AsyncSession

from ..db import get_db
from ..deps import current_user
from ..models import Measurement, User

router = APIRouter(prefix="/api/measurements", tags=["measurements"])

METRICS = ["temp", "hum", "pres", "tds", "ec", "vbat", "ibat", "speed", "alt", "sats", "hdop"]

CSV_COLUMNS = ["ts", "temp", "hum", "pres", "tds", "ec", "vbat", "ibat",
               "lat", "lon", "alt", "speed", "sats", "gps_valid", "buffered"]


def _parse_range(frm: datetime | None, to: datetime | None, hours: int) -> tuple[datetime, datetime]:
    end = to or datetime.now(timezone.utc)
    start = frm or (end - timedelta(hours=hours))
    if start >= end:
        raise HTTPException(status.HTTP_422_UNPROCESSABLE_ENTITY, "Начало диапазона позже конца")
    return start, end


def _bucket_seconds(start: datetime, end: datetime, requested: int | None) -> int:
    if requested:
        return max(requested, 1)

    hours = (end - start).total_seconds() / 3600
    if hours <= 2:
        return 60
    if hours <= 12:
        return 300
    if hours <= 48:
        return 900
    if hours <= 24 * 7:
        return 3600
    if hours <= 24 * 30:
        return 21600
    return 86400


@router.get("/series")
async def series(
    device_id: str,
    metrics: str = Query("temp,hum", description="Список метрик через запятую"),
    hours: int = 24,
    frm: datetime | None = Query(None, alias="from"),
    to: datetime | None = None,
    location_id: int | None = None,
    bucket: int | None = Query(None, description="Шаг агрегации в секундах"),
    _: User = Depends(current_user),
    db: AsyncSession = Depends(get_db),
):
    wanted = []
    for name in metrics.split(","):
        name = name.strip()
        if not name:
            continue
        if name not in METRICS:
            raise HTTPException(status.HTTP_422_UNPROCESSABLE_ENTITY, f"Неизвестная метрика: {name}")
        wanted.append(name)

    if not wanted:
        raise HTTPException(status.HTTP_422_UNPROCESSABLE_ENTITY, "Не выбрано ни одной метрики")

    start, end = _parse_range(frm, to, hours)
    step = _bucket_seconds(start, end, bucket)

    parts = []
    for m in wanted:
        parts.append(f"avg({m}) AS {m}_avg")
        parts.append(f"min({m}) AS {m}_min")
        parts.append(f"max({m}) AS {m}_max")
    cols = ", ".join(parts)

    sql = text(f"""
        SELECT to_timestamp(floor(extract(epoch FROM ts) / :step) * :step) AS bucket,
               count(*) AS n, {cols}
        FROM measurements
        WHERE device_id = :device_id
          AND ts >= :start AND ts < :end
          AND (CAST(:location_id AS integer) IS NULL OR location_id = :location_id)
        GROUP BY 1
        ORDER BY 1
    """)
    rows = (await db.execute(sql, {
        "step": step,
        "device_id": device_id,
        "start": start,
        "end": end,
        "location_id": location_id,
    })).mappings().all()

    points = []
    for r in rows:
        point: dict = {"t": r["bucket"].isoformat(), "n": r["n"]}
        for m in wanted:
            avg = r[f"{m}_avg"]
            if avg is None:
                point[m] = None
            else:
                point[m] = {
                    "avg": round(float(avg), 3),
                    "min": round(float(r[f"{m}_min"]), 3),
                    "max": round(float(r[f"{m}_max"]), 3),
                }
        points.append(point)

    return {
        "device_id": device_id,
        "metrics": wanted,
        "bucket_seconds": step,
        "from": start.isoformat(),
        "to": end.isoformat(),
        "location_id": location_id,
        "points": points,
    }


@router.get("/latest")
async def latest(
    device_id: str,
    _: User = Depends(current_user),
    db: AsyncSession = Depends(get_db),
):
    row = await db.scalar(
        select(Measurement)
        .where(Measurement.device_id == device_id)
        .order_by(Measurement.ts.desc())
        .limit(1)
    )
    if row is None:
        return None
    return {
        "ts": row.ts, "received_at": row.received_at, "seq": row.seq,
        "temp": row.temp, "hum": row.hum, "pres": row.pres,
        "tds": row.tds, "ec": row.ec, "vbat": row.vbat, "ibat": row.ibat,
        "lat": row.lat, "lon": row.lon, "alt": row.alt, "speed": row.speed,
        "sats": row.sats, "hdop": row.hdop, "gps_valid": row.gps_valid,
        "rssi": row.rssi, "buffered": row.buffered, "ts_valid": row.ts_valid,
        "location_id": row.location_id,
    }


@router.get("/export.csv")
async def export_csv(
    device_id: str,
    hours: int = 24,
    frm: datetime | None = Query(None, alias="from"),
    to: datetime | None = None,
    location_id: int | None = None,
    _: User = Depends(current_user),
    db: AsyncSession = Depends(get_db),
):
    start, end = _parse_range(frm, to, hours)
    stmt = select(Measurement).where(
        Measurement.device_id == device_id,
        Measurement.ts >= start,
        Measurement.ts < end,
    )
    if location_id is not None:
        stmt = stmt.where(Measurement.location_id == location_id)
    rows = (await db.scalars(stmt.order_by(Measurement.ts))).all()

    buf = io.StringIO()
    writer = csv.writer(buf, delimiter=";")
    writer.writerow(CSV_COLUMNS)
    for r in rows:
        line = [r.ts.isoformat()]
        for name in CSV_COLUMNS[1:]:
            line.append(getattr(r, name))
        writer.writerow(line)
    buf.seek(0)

    filename = f"{device_id}_{start:%Y%m%d}_{end:%Y%m%d}.csv"
    return StreamingResponse(
        iter([buf.getvalue()]),
        media_type="text/csv",
        headers={"Content-Disposition": f'attachment; filename="{filename}"'},
    )
