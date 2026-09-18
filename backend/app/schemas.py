from datetime import datetime
from typing import Any, Literal

from pydantic import BaseModel, ConfigDict, Field, field_validator


class LoginIn(BaseModel):
    email: str
    password: str


class TokenOut(BaseModel):
    access_token: str
    token_type: str = "bearer"


class UserOut(BaseModel):
    model_config = ConfigDict(from_attributes=True)
    id: int
    email: str
    created_at: datetime


class UserCreate(BaseModel):
    email: str = Field(min_length=3, max_length=190)
    password: str = Field(min_length=8)

    @field_validator("email")
    @classmethod
    def _looks_like_email(cls, v: str) -> str:
        if "@" not in v.strip("@") or " " in v:
            raise ValueError("Некорректный адрес")
        return v.strip().lower()


class MeasurementIn(BaseModel):
    seq: int = 0
    ts: int = 0
    ts_valid: bool = True
    up_ms: int = 0
    boot: str | None = None
    temp: float | None = None
    hum: float | None = None
    pres: float | None = None
    tds: float | None = None
    ec: float | None = None
    vbat: float | None = None
    ibat: float | None = None
    lat: float | None = None
    lon: float | None = None
    alt: float | None = None
    speed: float | None = None
    sats: int | None = None
    hdop: float | None = None
    gps_valid: bool = False
    rssi: int | None = None


class LogIn(BaseModel):
    ts: int = 0
    level: Literal["debug", "info", "warn", "error"] = "info"
    code: str = ""
    msg: str = ""


class AckIn(BaseModel):
    id: str
    ok: bool
    error: str | None = None


class IngestIn(BaseModel):
    device_id: str
    fw: str | None = None
    rev: int = 0
    status: dict[str, Any] = Field(default_factory=dict)
    measurements: list[MeasurementIn] = Field(default_factory=list)
    logs: list[LogIn] = Field(default_factory=list)
    acks: list[AckIn] = Field(default_factory=list)


class CommandOut(BaseModel):
    id: str
    cmd: str
    args: dict[str, Any] = Field(default_factory=dict)
    confirm: bool = False


class IngestOut(BaseModel):
    ok: bool = True
    server_time: int
    accepted: int
    duplicates: int
    settings: dict[str, Any] | None = None
    commands: list[CommandOut] = Field(default_factory=list)


class DeviceOut(BaseModel):
    device_id: str
    name: str
    fw: str | None
    online: bool
    last_seen: datetime | None
    settings_rev: int
    device_rev: int
    settings_pending: bool
    status: dict[str, Any]


class SettingsUpdate(BaseModel):
    settings: dict[str, Any]
    confirm: bool = False


class CommandCreate(BaseModel):
    cmd: str
    args: dict[str, Any] = Field(default_factory=dict)
    confirm: bool = False


class LocationOut(BaseModel):
    model_config = ConfigDict(from_attributes=True)
    id: int
    name: str
    city: str | None
    region: str | None
    country: str | None
    lat: float
    lon: float
    points: int
    first_seen: datetime
    last_seen: datetime
