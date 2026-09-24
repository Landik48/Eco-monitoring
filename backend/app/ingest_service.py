import logging
from datetime import datetime, timedelta, timezone

from sqlalchemy import select, text
from sqlalchemy.dialects.postgresql import insert as pg_insert
from sqlalchemy.ext.asyncio import AsyncSession

from .geo import resolve_location
from .models import Command, Device, DeviceLog, Measurement
from .schemas import CommandOut, IngestOut

log = logging.getLogger(__name__)

MAX_CLOCK_SKEW = timedelta(days=370)
MAX_UPTIME_S = 90 * 24 * 3600
BUFFERED_AFTER_S = 60


def _ts_to_dt(
    ts: int,
    received: datetime,
    ts_valid: bool,
    up_ms: int = 0,
    device_uptime_s: float | None = None,
    same_boot: bool = False,
) -> tuple[datetime, bool]:
    if ts_valid and ts > 0:
        dt = datetime.fromtimestamp(ts, tz=timezone.utc)
        if abs(dt - received) <= MAX_CLOCK_SKEW:
            return dt, True

    if same_boot and up_ms > 0 and device_uptime_s is not None:
        elapsed = device_uptime_s - up_ms / 1000.0
        if 0 <= elapsed <= MAX_UPTIME_S:
            return received - timedelta(seconds=elapsed), False

    return received, False


def _age_seconds(up_ms: int, device_uptime_s: float | None) -> float | None:
    if not up_ms or device_uptime_s is None:
        return None
    return device_uptime_s - up_ms / 1000.0


async def _promote_previous_suspect(db: AsyncSession, device_id: str, before: datetime) -> None:
    previous = await db.scalar(
        select(Measurement)
        .where(Measurement.device_id == device_id, Measurement.ts < before)
        .order_by(Measurement.ts.desc())
        .limit(1)
    )
    if previous is not None and previous.quality == "suspect":
        previous.quality = "confirmed"


async def store_measurement(
    db: AsyncSession,
    device: Device,
    m,
    now: datetime,
    buffered: bool,
    device_uptime_s: float | None,
    current_boot: str | None,
) -> bool:
    same_boot = bool(m.boot) and m.boot == current_boot
    ts, ts_ok = _ts_to_dt(m.ts, now, m.ts_valid, m.up_ms, device_uptime_s, same_boot)

    location_id = None
    if m.gps_valid and m.lat is not None and m.lon is not None:
        location = await resolve_location(db, device.device_id, m.lat, m.lon)
        location_id = location.id

    values = {
        "device_id": device.device_id,
        "ts": ts,
        "seq": m.seq,
        "ts_valid": ts_ok,
        "boot": m.boot,
        "up_ms": m.up_ms or None,
        "temp": m.temp, "hum": m.hum, "pres": m.pres,
        "tds": m.tds, "ec": m.ec,
        "vbat": m.vbat, "ibat": m.ibat,
        "lat": m.lat, "lon": m.lon, "alt": m.alt, "speed": m.speed,
        "sats": m.sats, "hdop": m.hdop, "gps_valid": m.gps_valid,
        "rssi": m.rssi,
        "buffered": buffered,
        "location_id": location_id,
        "quality": "suspect" if m.suspect else "ok",
        "flags": m.flags or 0,
        "stability": m.stability,
        "time_source": m.time_source,
        "rejected_raw": m.rejected,
    }

    stmt = pg_insert(Measurement).values(**values)
    if not ts_ok and m.boot:
        stmt = stmt.on_conflict_do_nothing(
            index_elements=["device_id", "boot", "seq"],
            index_where=text("boot IS NOT NULL"),
        )
    else:
        stmt = stmt.on_conflict_do_nothing(index_elements=["device_id", "ts", "seq"])

    result = await db.execute(stmt)
    stored = bool(result.rowcount)


    if stored and m.confirms_previous:
        await _promote_previous_suspect(db, device.device_id, ts)

    return stored


async def store_log(db: AsyncSession, device_id: str, entry, now: datetime) -> None:
    ts, _ = _ts_to_dt(entry.ts, now, entry.ts > 0)
    db.add(DeviceLog(
        device_id=device_id,
        ts=ts,
        level=entry.level,
        code=entry.code,
        msg=entry.msg,
        source="device",
    ))


async def store_ack(db: AsyncSession, device_id: str, ack, now: datetime) -> None:
    cmd = await db.scalar(select(Command).where(Command.id == ack.id))
    if cmd is None:
        return

    cmd.status = "done" if ack.ok else "failed"
    cmd.error = ack.error
    cmd.acked_at = now

    if ack.ok:
        msg = f"Команда {cmd.cmd}: выполнена"
    else:
        msg = f"Команда {cmd.cmd}: {ack.error}"

    db.add(DeviceLog(
        device_id=device_id,
        ts=now,
        level="info" if ack.ok else "error",
        code=f"cmd_{cmd.status}",
        msg=msg,
        source="device",
    ))


def apply_status(device: Device, status: dict, now: datetime) -> None:
    if status.get("fw"):
        device.fw = status["fw"]
    if "rev" in status:
        device.device_rev = int(status.get("rev") or 0)
    device.last_status = status
    if status.get("online") is not False:
        device.last_seen = now


async def take_pending_commands(db: AsyncSession, device_id: str, now: datetime, limit: int = 10):
    pending = (await db.scalars(
        select(Command)
        .where(Command.device_id == device_id, Command.status == "pending")
        .order_by(Command.created_at)
        .limit(limit)
    )).all()

    out = []
    for cmd in pending:
        cmd.status = "sent"
        cmd.sent_at = now
        out.append(CommandOut(
            id=str(cmd.id),
            cmd=cmd.cmd,
            args=cmd.args or {},
            confirm=cmd.confirm,
        ))
    return out


async def handle_payload(db: AsyncSession, device: Device, payload) -> IngestOut:
    now = datetime.now(timezone.utc)
    status = payload.status or {}

    if payload.fw:
        device.fw = payload.fw
    device.device_rev = payload.rev

    if payload.status:
        apply_status(device, status, now)
    else:
        device.last_seen = now

    device_uptime_s = status.get("uptime_s")
    if not isinstance(device_uptime_s, (int, float)):
        device_uptime_s = None
    current_boot = status.get("boot")

    accepted = 0
    duplicates = 0
    for m in payload.measurements:
        age = _age_seconds(m.up_ms, device_uptime_s)
        buffered = age is not None and age > BUFFERED_AFTER_S
        stored = await store_measurement(db, device, m, now, buffered,
                                         device_uptime_s, current_boot)
        if stored:
            accepted += 1
        else:
            duplicates += 1

    for entry in payload.logs:
        await store_log(db, device.device_id, entry, now)
    for ack in payload.acks:
        await store_ack(db, device.device_id, ack, now)

    settings_out = None
    if payload.rev != device.settings_rev:
        settings_out = dict(device.settings or {})
        settings_out["rev"] = device.settings_rev

    commands = await take_pending_commands(db, device.device_id, now)
    await db.commit()

    return IngestOut(
        server_time=int(now.timestamp()),
        accepted=accepted,
        duplicates=duplicates,
        settings=settings_out,
        commands=commands,
    )
