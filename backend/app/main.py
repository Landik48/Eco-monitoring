import logging
from contextlib import asynccontextmanager

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from sqlalchemy import select

from .config import get_config
from .db import Base, SessionLocal, engine
from .device_settings import DEFAULTS
from .models import Device, User
from .mqtt import bridge
from .routers import analysis, auth, devices, ingest, measurements, misc
from .security import hash_device_token, hash_password

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(name)s %(message)s")
log = logging.getLogger(__name__)


async def bootstrap() -> None:
    cfg = get_config()
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)

    async with SessionLocal() as db:
        if not await db.scalar(select(User).limit(1)):
            db.add(User(
                email=cfg.admin_email.lower(),
                password_hash=hash_password(cfg.admin_password),
            ))
            log.info("Создан первый пользователь: %s", cfg.admin_email)

        device = await db.get(Device, cfg.device_id)
        if device is None:
            db.add(Device(
                device_id=cfg.device_id,
                name=cfg.device_name,
                token_hash=hash_device_token(cfg.device_token),
                settings={**DEFAULTS, "name": cfg.device_name},
                settings_rev=1,
            ))
            log.info("Зарегистрирована станция: %s", cfg.device_id)
        else:
            device.token_hash = hash_device_token(cfg.device_token)
        await db.commit()


@asynccontextmanager
async def lifespan(_: FastAPI):
    await bootstrap()
    bridge.start()
    yield
    await bridge.stop()
    await engine.dispose()


app = FastAPI(
    title="Мониторинг окружающей среды",
    version="1.0.0",
    lifespan=lifespan,
    docs_url="/api/docs",
    openapi_url="/api/openapi.json",
)

_origins = get_config().cors_origins
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"] if _origins == "*" else [o.strip() for o in _origins.split(",")],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(auth.router)
app.include_router(ingest.router)
app.include_router(devices.router)
app.include_router(measurements.router)
app.include_router(analysis.router)
app.include_router(misc.router)


@app.get("/api/health")
async def health():
    return {"ok": True, "mqtt": bridge.connected}
