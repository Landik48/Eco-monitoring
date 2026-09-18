from datetime import datetime, timedelta, timezone

from fastapi import APIRouter, Depends
from sqlalchemy import func, select
from sqlalchemy.ext.asyncio import AsyncSession

from ..analysis import LIMITS, build_report
from ..db import get_db
from ..deps import current_user
from ..models import Location, Measurement, User

router = APIRouter(prefix="/api/analysis", tags=["analysis"])

KEYS = list(LIMITS.keys())


@router.get("")
async def analysis(
    device_id: str,
    hours: int = 24,
    location_id: int | None = None,
    _: User = Depends(current_user),
    db: AsyncSession = Depends(get_db),
):
    now = datetime.now(timezone.utc)
    start = now - timedelta(hours=hours)
    middle = now - timedelta(hours=hours / 2)

    columns = [getattr(Measurement, key) for key in KEYS]

    def base(stmt):
        stmt = stmt.where(Measurement.device_id == device_id)
        if location_id is not None:
            stmt = stmt.where(Measurement.location_id == location_id)
        return stmt

    async def averages_between(frm, to=None):
        stmt = select(*[func.avg(c) for c in columns]).where(Measurement.ts >= frm)
        if to is not None:
            stmt = stmt.where(Measurement.ts < to)
        row = (await db.execute(base(stmt))).one()
        return {key: (float(v) if v is not None else None) for key, v in zip(KEYS, row)}

    count_stmt = select(func.count(Measurement.id)).where(Measurement.ts >= start)
    samples = await db.scalar(base(count_stmt))

    averages = await averages_between(start)
    first_half = await averages_between(start, middle)
    second_half = await averages_between(middle)

    last = await db.scalar(base(select(Measurement).order_by(Measurement.ts.desc()).limit(1)))
    latest = {}
    for key in KEYS:
        latest[key] = getattr(last, key) if last else None

    trends: dict[str, float] = {}
    for key in KEYS:
        before = first_half[key]
        after = second_half[key]
        if before is None or after is None or abs(before) < 0.000001:
            continue
        trends[key] = (after - before) / abs(before) * 100

    location_name = None
    if location_id is not None:
        loc = await db.get(Location, location_id)
        if loc is not None:
            location_name = loc.name

    return build_report(
        latest=latest,
        averages=averages,
        trends=trends,
        samples=samples,
        window_hours=hours,
        location_name=location_name,
    )
