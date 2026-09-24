import asyncio
import logging
import math
from datetime import datetime, timezone

import httpx
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from .config import get_config
from .models import Location

log = logging.getLogger(__name__)

EARTH_RADIUS_M = 6371000.0

_geocode_lock = asyncio.Lock()


def haversine_m(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
    dlat = math.radians(lat2 - lat1)
    dlon = math.radians(lon2 - lon1)
    a = math.sin(dlat / 2) ** 2
    b = math.cos(math.radians(lat1)) * math.cos(math.radians(lat2)) * math.sin(dlon / 2) ** 2
    return 2 * EARTH_RADIUS_M * math.asin(math.sqrt(a + b))


async def _reverse_geocode(lat: float, lon: float) -> dict[str, str | None]:
    cfg = get_config()
    if not cfg.geocode_enabled:
        return {}

    async with _geocode_lock:
        try:
            async with httpx.AsyncClient(timeout=10) as client:
                resp = await client.get(
                    cfg.geocode_url,
                    params={
                        "lat": lat,
                        "lon": lon,
                        "format": "jsonv2",
                        "zoom": 12,
                        "accept-language": "ru",
                    },
                    headers={"User-Agent": cfg.geocode_user_agent},
                )
            await asyncio.sleep(1.0)
            if resp.status_code != 200:
                return {}
            addr = resp.json().get("address", {})
        except Exception as exc:
            log.warning("Обратное геокодирование недоступно: %s", exc)
            return {}

    city = addr.get("city") or addr.get("town") or addr.get("village") or addr.get("municipality")
    region = addr.get("state") or addr.get("region") or addr.get("county")
    return {"city": city, "region": region, "country": addr.get("country")}


async def resolve_location(db: AsyncSession, device_id: str, lat: float, lon: float) -> Location:
    cfg = get_config()
    now = datetime.now(timezone.utc)

    known = (await db.scalars(select(Location).where(Location.device_id == device_id))).all()

    nearest = None
    nearest_dist = 0.0
    for loc in known:
        d = haversine_m(lat, lon, loc.lat, loc.lon)
        if nearest is None or d < nearest_dist:
            nearest = loc
            nearest_dist = d

    if nearest is not None and nearest_dist <= cfg.location_radius_m:
        n = nearest.points if nearest.points > 0 else 1
        nearest.lat = (nearest.lat * n + lat) / (n + 1)
        nearest.lon = (nearest.lon * n + lon) / (n + 1)
        nearest.points = n + 1
        nearest.last_seen = now
        return nearest

    place = await _reverse_geocode(lat, lon)
    name = place.get("city") or place.get("region")
    if not name:
        name = f"Точка {lat:.3f}, {lon:.3f}"

    loc = Location(
        device_id=device_id,
        name=name,
        city=place.get("city"),
        region=place.get("region"),
        country=place.get("country"),
        lat=lat,
        lon=lon,
        points=1,
        first_seen=now,
        last_seen=now,
    )
    db.add(loc)
    await db.flush()
    return loc
