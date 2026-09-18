from functools import lru_cache

from pydantic_settings import BaseSettings, SettingsConfigDict


class Config(BaseSettings):
    model_config = SettingsConfigDict(env_file=".env", extra="ignore")

    database_url: str = "postgresql+asyncpg://envmon:envmon@db:5432/envmon"

    jwt_secret: str = "change-me"
    jwt_algorithm: str = "HS256"
    access_token_ttl_min: int = 60 * 12

    admin_email: str = "admin@example.com"
    admin_password: str = "admin"

    device_id: str = "station-01"
    device_token: str = "change-me-device-token"
    device_name: str = "Станция 1"

    geocode_enabled: bool = True
    geocode_url: str = "https://nominatim.openstreetmap.org/reverse"
    geocode_user_agent: str = "env-monitor/1.0"
    location_radius_m: int = 2000

    mqtt_host: str = "mosquitto"
    mqtt_port: int = 1883
    mqtt_user: str = "station"
    mqtt_password: str = "change-me-mqtt-password"
    mqtt_client_id: str = "envmon-server"

    cors_origins: str = "*"


@lru_cache
def get_config() -> Config:
    return Config()
