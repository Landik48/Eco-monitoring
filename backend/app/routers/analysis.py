from datetime import datetime, timedelta, timezone

from fastapi import APIRouter, Depends, Query
from sqlalchemy import func, select
from sqlalchemy.ext.asyncio import AsyncSession

from ..analysis import LIMITS, build_report, build_stability
from ..db import get_db
from ..deps import current_user
from ..models import Device, Location, Measurement, User

router = APIRouter(prefix="/api/analysis", tags=["analysis"])

KEYS = list(LIMITS.keys())


TRUSTED_QUALITY = ("ok", "confirmed")


@router.get("")
async def analysis(
    device_id: str,
    hours: int = 24,
    location_id: int | None = None,
    include_suspect: bool = Query(
        False,
        description="Учитывать замеры, помеченные как недостоверные",
    ),
    _: User = Depends(current_user),
    db: AsyncSession = Depends(get_db),
):
    now = datetime.now(timezone.utc)
    start = now - timedelta(hours=hours)
    middle = now - timedelta(hours=hours / 2)

    columns = [getattr(Measurement, key) for key in KEYS]

    def base(stmt, trusted_only: bool = True):
        stmt = stmt.where(Measurement.device_id == device_id)
        if location_id is not None:
            stmt = stmt.where(Measurement.location_id == location_id)
        if trusted_only and not include_suspect:
            stmt = stmt.where(Measurement.quality.in_(TRUSTED_QUALITY))
        return stmt

    async def averages_between(frm, to=None):
        stmt = select(*[func.avg(c) for c in columns]).where(Measurement.ts >= frm)
        if to is not None:
            stmt = stmt.where(Measurement.ts < to)
        row = (await db.execute(base(stmt))).one()
        return {key: (float(v) if v is not None else None) for key, v in zip(KEYS, row)}

    count_stmt = select(func.count(Measurement.id)).where(Measurement.ts >= start)
    samples = await db.scalar(base(count_stmt))


    quality_rows = (await db.execute(
        base(
            select(Measurement.quality, func.count(Measurement.id))
            .where(Measurement.ts >= start)
            .group_by(Measurement.quality),
            trusted_only=False,
        )
    )).all()
    by_quality = {q: n for q, n in quality_rows}
    total_samples = sum(by_quality.values())

    flag_mask = await db.scalar(
        base(
            select(func.coalesce(func.bit_or(Measurement.flags), 0))
            .where(Measurement.ts >= start),
            trusted_only=False,
        )
    ) or 0

    averages = await averages_between(start)
    first_half = await averages_between(start, middle)
    second_half = await averages_between(middle)

    last = await db.scalar(base(select(Measurement).order_by(Measurement.ts.desc()).limit(1)))
    latest = {key: (getattr(last, key) if last else None) for key in KEYS}

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

    device = await db.get(Device, device_id)
    stability = build_stability(
        status=(device.last_status if device else {}) or {},
        suspect_samples=by_quality.get("suspect", 0),
        confirmed_samples=by_quality.get("confirmed", 0),
        total_samples=total_samples,
        flag_mask=int(flag_mask),
    )

    return build_report(
        latest=latest,
        averages=averages,
        trends=trends,
        samples=samples,
        window_hours=hours,
        location_name=location_name,
        stability=stability,
    )
