from datetime import datetime, timedelta, timezone

from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from .. import device_settings as ds
from ..db import get_db
from ..deps import current_user
from ..models import Command, Device, DeviceLog, User
from ..mqtt import bridge
from ..schemas import CommandCreate, DeviceOut, SettingsUpdate

router = APIRouter(prefix="/api/devices", tags=["devices"])

OFFLINE_GRACE = timedelta(seconds=45)


def is_online(device: Device) -> bool:
    status = device.last_status or {}
    if status.get("online") is False:
        return False
    if device.last_seen is None:
        return False
    status_ms = status.get("status_interval_ms") or 30000
    window = timedelta(milliseconds=status_ms * 3) + OFFLINE_GRACE
    return datetime.now(timezone.utc) - device.last_seen <= window


def to_out(device: Device) -> DeviceOut:
    return DeviceOut(
        device_id=device.device_id,
        name=device.name,
        fw=device.fw,
        online=is_online(device),
        last_seen=device.last_seen,
        settings_rev=device.settings_rev,
        device_rev=device.device_rev,
        settings_pending=device.settings_rev != device.device_rev,
        status=device.last_status or {},
    )


async def _get(db: AsyncSession, device_id: str) -> Device:
    device = await db.get(Device, device_id)
    if device is None:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "Станция не найдена")
    return device


@router.get("", response_model=list[DeviceOut])
async def list_devices(_: User = Depends(current_user), db: AsyncSession = Depends(get_db)):
    devices = (await db.scalars(select(Device).order_by(Device.device_id))).all()
    return [to_out(d) for d in devices]


@router.get("/{device_id}", response_model=DeviceOut)
async def get_device(device_id: str, _: User = Depends(current_user), db: AsyncSession = Depends(get_db)):
    return to_out(await _get(db, device_id))


@router.get("/{device_id}/settings")
async def get_settings(device_id: str, _: User = Depends(current_user), db: AsyncSession = Depends(get_db)):
    device = await _get(db, device_id)
    return {
        "settings": {**ds.DEFAULTS, **(device.settings or {})},
        "schema": ds.SCHEMA,
        "rev": device.settings_rev,
        "device_rev": device.device_rev,
        "pending": device.settings_rev != device.device_rev,
    }


@router.put("/{device_id}/settings")
async def update_settings(
    device_id: str,
    data: SettingsUpdate,
    user: User = Depends(current_user),
    db: AsyncSession = Depends(get_db),
):
    device = await _get(db, device_id)
    current = {**ds.DEFAULTS, **(device.settings or {})}

    try:
        patch = ds.validate(data.settings)
    except ds.SettingsError as exc:
        raise HTTPException(status.HTTP_422_UNPROCESSABLE_ENTITY, str(exc)) from exc

    risky = ds.dangerous_changes(current, patch)
    if risky and not data.confirm:
        raise HTTPException(
            status.HTTP_409_CONFLICT,
            {"error": "confirmation_required", "changes": risky},
        )

    changed = {k: v for k, v in patch.items() if current.get(k) != v}
    if not changed:
        return {"rev": device.settings_rev, "changed": {}, "pending": False}

    device.settings = {**current, **patch}
    device.settings_rev += 1

    db.add(DeviceLog(
        device_id=device_id, ts=datetime.now(timezone.utc), level="info",
        code="settings_changed", source="panel", actor=user.email,
        msg="Изменены настройки: " + ", ".join(f"{k}={v}" for k, v in changed.items()),
    ))
    await db.commit()

    delivered = await bridge.push_settings(device)
    return {
        "rev": device.settings_rev,
        "changed": changed,
        "pending": True,
        "published": delivered,
    }


@router.post("/{device_id}/commands", status_code=201)
async def send_command(
    device_id: str,
    data: CommandCreate,
    user: User = Depends(current_user),
    db: AsyncSession = Depends(get_db),
):
    device = await _get(db, device_id)
    if data.cmd not in ds.KNOWN_COMMANDS:
        raise HTTPException(status.HTTP_422_UNPROCESSABLE_ENTITY, f"Неизвестная команда: {data.cmd}")
    if data.cmd in ds.DANGEROUS_COMMANDS and not data.confirm:
        raise HTTPException(
            status.HTTP_409_CONFLICT,
            {"error": "confirmation_required", "changes": [f"Команда «{data.cmd}»"]},
        )

    cmd = Command(
        device_id=device_id, cmd=data.cmd, args=data.args,
        confirm=data.confirm, created_by=user.email,
    )
    db.add(cmd)
    db.add(DeviceLog(
        device_id=device_id, ts=datetime.now(timezone.utc), level="info",
        code="cmd_queued", source="panel", actor=user.email,
        msg=f"Поставлена в очередь команда {data.cmd}",
    ))
    await db.commit()

    online = is_online(device)
    if online and await bridge.push_command(device_id, str(cmd.id), cmd.cmd, cmd.args, cmd.confirm):
        cmd.status = "sent"
        cmd.sent_at = datetime.now(timezone.utc)
        await db.commit()

    return {"id": str(cmd.id), "status": cmd.status, "device_online": online}


@router.get("/{device_id}/commands")
async def list_commands(
    device_id: str,
    limit: int = 50,
    _: User = Depends(current_user),
    db: AsyncSession = Depends(get_db),
):
    rows = (await db.scalars(
        select(Command)
        .where(Command.device_id == device_id)
        .order_by(Command.created_at.desc())
        .limit(min(limit, 200))
    )).all()
    return [
        {
            "id": str(c.id), "cmd": c.cmd, "args": c.args, "status": c.status,
            "error": c.error, "created_by": c.created_by,
            "created_at": c.created_at, "sent_at": c.sent_at, "acked_at": c.acked_at,
        }
        for c in rows
    ]
