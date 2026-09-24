from fastapi import Depends, Header, HTTPException, status
from fastapi.security import HTTPAuthorizationCredentials, HTTPBearer
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from .db import get_db
from .models import Device, User
from .security import decode_access_token, verify_device_token

bearer = HTTPBearer(auto_error=False)


async def current_user(
    creds: HTTPAuthorizationCredentials | None = Depends(bearer),
    db: AsyncSession = Depends(get_db),
) -> User:
    if creds is None:
        raise HTTPException(status.HTTP_401_UNAUTHORIZED, "Требуется авторизация")
    payload = decode_access_token(creds.credentials)
    if not payload:
        raise HTTPException(status.HTTP_401_UNAUTHORIZED, "Недействительный или истёкший токен")

    user = await db.scalar(select(User).where(User.email == payload.get("sub")))
    if user is None or not user.is_active:
        raise HTTPException(status.HTTP_401_UNAUTHORIZED, "Пользователь не найден")
    return user


async def current_device(
    device_id_hdr: str | None = Header(default=None, alias="X-Device-Id"),
    token: str | None = Header(default=None, alias="X-Device-Token"),
    db: AsyncSession = Depends(get_db),
) -> Device:
    if not token:
        raise HTTPException(status.HTTP_401_UNAUTHORIZED, "Нет заголовка X-Device-Token")

    stmt = select(Device)
    if device_id_hdr:
        stmt = stmt.where(Device.device_id == device_id_hdr)
    devices = (await db.scalars(stmt)).all()

    for device in devices:
        if verify_device_token(token, device.token_hash):
            return device
    raise HTTPException(status.HTTP_401_UNAUTHORIZED, "Неизвестный токен устройства")
