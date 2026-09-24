from datetime import datetime, timedelta, timezone

from fastapi import APIRouter, Depends, Query
from sqlalchemy import func, select
from sqlalchemy.ext.asyncio import AsyncSession

from ..db import get_db
from ..deps import current_user
from ..models import DeviceLog, Location, Measurement, User

router = APIRouter(prefix="/api", tags=["locations", "logs"])


@router.get("/locations")
async def list_locations(
    device_id: str,
    _: User = Depends(current_user),
    db: AsyncSession = Depends(get_db),
):
    rows = (await db.execute(
        select(
            Location,
            func.count(Measurement.id).label("measurements"),
            func.max(Measurement.ts).label("last_measurement"),
        )
        .outerjoin(Measurement, Measurement.location_id == Location.id)
        .where(Location.device_id == device_id)
        .group_by(Location.id)
        .order_by(Location.last_seen.desc())
    )).all()

    return [
        {
            "id": loc.id, "name": loc.name, "city": loc.city, "region": loc.region,
            "country": loc.country, "lat": round(loc.lat, 6), "lon": round(loc.lon, 6),
            "points": loc.points, "first_seen": loc.first_seen, "last_seen": loc.last_seen,
            "measurements": count, "last_measurement": last,
        }
        for loc, count, last in rows
    ]


@router.patch("/locations/{location_id}")
async def rename_location(
    location_id: int,
    name: str,
    _: User = Depends(current_user),
    db: AsyncSession = Depends(get_db),
):
    loc = await db.get(Location, location_id)
    if loc is None:
        return {"ok": False}
    loc.name = name.strip()[:190]
    await db.commit()
    return {"ok": True, "name": loc.name}


@router.get("/logs")
async def list_logs(
    device_id: str,
    level: str | None = None,
    source: str | None = None,
    hours: int = 24,
    limit: int = Query(200, le=1000),
    _: User = Depends(current_user),
    db: AsyncSession = Depends(get_db),
):
    since = datetime.now(timezone.utc) - timedelta(hours=hours)
    stmt = select(DeviceLog).where(DeviceLog.device_id == device_id, DeviceLog.ts >= since)
    if level:
        stmt = stmt.where(DeviceLog.level == level)
    if source:
        stmt = stmt.where(DeviceLog.source == source)

    rows = (await db.scalars(stmt.order_by(DeviceLog.ts.desc(), DeviceLog.id.desc()).limit(limit))).all()
    return [
        {
            "id": r.id, "ts": r.ts, "received_at": r.received_at, "level": r.level,
            "code": r.code, "msg": r.msg, "source": r.source, "actor": r.actor,
        }
        for r in rows
    ]


@router.get("/logs/summary")
async def logs_summary(
    device_id: str,
    hours: int = 24,
    _: User = Depends(current_user),
    db: AsyncSession = Depends(get_db),
):
    since = datetime.now(timezone.utc) - timedelta(hours=hours)
    rows = (await db.execute(
        select(DeviceLog.level, func.count())
        .where(DeviceLog.device_id == device_id, DeviceLog.ts >= since)
        .group_by(DeviceLog.level)
    )).all()
    return {level: count for level, count in rows}
