import uuid
from datetime import datetime

from sqlalchemy import (
    BigInteger,
    Boolean,
    DateTime,
    Float,
    ForeignKey,
    Index,
    Integer,
    String,
    Text,
    UniqueConstraint,
    func,
    text,
)
from sqlalchemy.dialects.postgresql import JSONB, UUID
from sqlalchemy.orm import Mapped, mapped_column

from .db import Base


class User(Base):
    __tablename__ = "users"

    id: Mapped[int] = mapped_column(primary_key=True)
    email: Mapped[str] = mapped_column(String(190), unique=True, index=True)
    password_hash: Mapped[str] = mapped_column(String(255))
    is_active: Mapped[bool] = mapped_column(Boolean, default=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now())


class Device(Base):
    __tablename__ = "devices"

    device_id: Mapped[str] = mapped_column(String(64), primary_key=True)
    name: Mapped[str] = mapped_column(String(190), default="Станция")
    token_hash: Mapped[str] = mapped_column(String(255))
    fw: Mapped[str | None] = mapped_column(String(32), nullable=True)

    settings: Mapped[dict] = mapped_column(JSONB, default=dict)
    settings_rev: Mapped[int] = mapped_column(Integer, default=0)
    device_rev: Mapped[int] = mapped_column(Integer, default=0)

    last_seen: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    last_status: Mapped[dict] = mapped_column(JSONB, default=dict)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now())


class Location(Base):
    __tablename__ = "locations"

    id: Mapped[int] = mapped_column(primary_key=True)
    device_id: Mapped[str] = mapped_column(ForeignKey("devices.device_id", ondelete="CASCADE"), index=True)
    name: Mapped[str] = mapped_column(String(190))
    city: Mapped[str | None] = mapped_column(String(190), nullable=True)
    region: Mapped[str | None] = mapped_column(String(190), nullable=True)
    country: Mapped[str | None] = mapped_column(String(190), nullable=True)
    lat: Mapped[float] = mapped_column(Float)
    lon: Mapped[float] = mapped_column(Float)
    points: Mapped[int] = mapped_column(Integer, default=0)
    first_seen: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now())
    last_seen: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now())


class Measurement(Base):
    __tablename__ = "measurements"

    id: Mapped[int] = mapped_column(BigInteger, primary_key=True, autoincrement=True)
    device_id: Mapped[str] = mapped_column(ForeignKey("devices.device_id", ondelete="CASCADE"))
    ts: Mapped[datetime] = mapped_column(DateTime(timezone=True))
    received_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now())
    seq: Mapped[int] = mapped_column(Integer, default=0)

    temp: Mapped[float | None] = mapped_column(Float, nullable=True)
    hum: Mapped[float | None] = mapped_column(Float, nullable=True)
    pres: Mapped[float | None] = mapped_column(Float, nullable=True)
    tds: Mapped[float | None] = mapped_column(Float, nullable=True)
    ec: Mapped[float | None] = mapped_column(Float, nullable=True)
    vbat: Mapped[float | None] = mapped_column(Float, nullable=True)
    ibat: Mapped[float | None] = mapped_column(Float, nullable=True)

    lat: Mapped[float | None] = mapped_column(Float, nullable=True)
    lon: Mapped[float | None] = mapped_column(Float, nullable=True)
    alt: Mapped[float | None] = mapped_column(Float, nullable=True)
    speed: Mapped[float | None] = mapped_column(Float, nullable=True)
    sats: Mapped[int | None] = mapped_column(Integer, nullable=True)
    hdop: Mapped[float | None] = mapped_column(Float, nullable=True)
    gps_valid: Mapped[bool] = mapped_column(Boolean, default=False)

    rssi: Mapped[int | None] = mapped_column(Integer, nullable=True)
    boot: Mapped[str | None] = mapped_column(String(16), nullable=True)
    up_ms: Mapped[int | None] = mapped_column(BigInteger, nullable=True)
    buffered: Mapped[bool] = mapped_column(Boolean, default=False)
    ts_valid: Mapped[bool] = mapped_column(Boolean, default=True)

    location_id: Mapped[int | None] = mapped_column(
        ForeignKey("locations.id", ondelete="SET NULL"), nullable=True, index=True
    )

    __table_args__ = (
        UniqueConstraint("device_id", "ts", "seq", name="uq_measurement_point"),
        Index("uq_measurement_boot", "device_id", "boot", "seq",
              unique=True, postgresql_where=text("boot IS NOT NULL")),
        Index("ix_measurements_device_ts", "device_id", "ts"),
    )


class DeviceLog(Base):
    __tablename__ = "device_logs"

    id: Mapped[int] = mapped_column(BigInteger, primary_key=True, autoincrement=True)
    device_id: Mapped[str] = mapped_column(String(64), index=True)
    ts: Mapped[datetime] = mapped_column(DateTime(timezone=True))
    received_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now())
    level: Mapped[str] = mapped_column(String(16), default="info")
    code: Mapped[str] = mapped_column(String(64), default="")
    msg: Mapped[str] = mapped_column(Text, default="")
    source: Mapped[str] = mapped_column(String(16), default="device")
    actor: Mapped[str | None] = mapped_column(String(190), nullable=True)

    __table_args__ = (Index("ix_logs_device_ts", "device_id", "ts"),)


class Command(Base):
    __tablename__ = "commands"

    id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    device_id: Mapped[str] = mapped_column(String(64), index=True)
    cmd: Mapped[str] = mapped_column(String(64))
    args: Mapped[dict] = mapped_column(JSONB, default=dict)
    confirm: Mapped[bool] = mapped_column(Boolean, default=False)
    status: Mapped[str] = mapped_column(String(16), default="pending")
    error: Mapped[str | None] = mapped_column(Text, nullable=True)
    created_by: Mapped[str | None] = mapped_column(String(190), nullable=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now())
    sent_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    acked_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
