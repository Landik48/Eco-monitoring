import asyncio
import json
import logging
from datetime import datetime, timezone

import aiomqtt
from sqlalchemy import select

from .config import get_config
from .db import SessionLocal
from .ingest_service import (apply_status, store_ack, store_log, store_measurement,
                             take_pending_commands)
from .models import Device
from .schemas import AckIn, LogIn, MeasurementIn

log = logging.getLogger(__name__)


def topics(device_id: str) -> dict[str, str]:
    base = f"env/{device_id}"
    return {
        "cmd": f"{base}/cmd",
        "settings": f"{base}/settings",
        "status": f"{base}/status",
        "time": f"{base}/time",
        "timereq": f"{base}/timereq",
    }


class MqttBridge:
    def __init__(self) -> None:
        self._client: aiomqtt.Client | None = None
        self._task: asyncio.Task | None = None
        self._stop = asyncio.Event()
        self.connected = False

    def start(self) -> None:
        self._stop.clear()
        self._task = asyncio.create_task(self._run_forever(), name="mqtt-bridge")

    async def stop(self) -> None:
        self._stop.set()
        if self._task:
            self._task.cancel()
            try:
                await self._task
            except asyncio.CancelledError:
                pass

    async def _run_forever(self) -> None:
        cfg = get_config()
        backoff = 2
        while not self._stop.is_set():
            try:
                async with aiomqtt.Client(
                    hostname=cfg.mqtt_host,
                    port=cfg.mqtt_port,
                    username=cfg.mqtt_user or None,
                    password=cfg.mqtt_password or None,
                    identifier=cfg.mqtt_client_id,
                    keepalive=30,
                ) as client:
                    self._client = client
                    self.connected = True
                    backoff = 2
                    log.info("MQTT подключён: %s:%s", cfg.mqtt_host, cfg.mqtt_port)

                    await client.subscribe("env/+/#", qos=1)
                    await self._resync_all_settings()

                    async for message in client.messages:
                        try:
                            await self._dispatch(message)
                        except Exception:
                            log.exception("Ошибка обработки сообщения %s", message.topic)
            except aiomqtt.MqttError as exc:
                log.warning("MQTT недоступен (%s), повтор через %s с", exc, backoff)
            except asyncio.CancelledError:
                raise
            except Exception:
                log.exception("Неожиданная ошибка моста MQTT")
            finally:
                self._client = None
                self.connected = False

            if self._stop.is_set():
                break
            await asyncio.sleep(backoff)
            backoff = min(backoff * 2, 60)

    async def publish(self, topic: str, payload: dict, retain: bool = False) -> bool:
        if self._client is None:
            return False
        try:
            await self._client.publish(topic, json.dumps(payload, ensure_ascii=False),
                                       qos=1, retain=retain)
            return True
        except aiomqtt.MqttError as exc:
            log.warning("Не удалось опубликовать в %s: %s", topic, exc)
            return False

    async def push_settings(self, device: Device) -> bool:
        payload = dict(device.settings or {})
        payload["rev"] = device.settings_rev
        return await self.publish(topics(device.device_id)["settings"], payload, retain=True)

    async def push_command(self, device_id: str, command_id: str, cmd: str,
                           args: dict, confirm: bool) -> bool:
        return await self.publish(topics(device_id)["cmd"], {
            "id": command_id, "cmd": cmd, "args": args or {}, "confirm": confirm,
        })

    async def push_time(self, device_id: str, nonce: int | None = None) -> bool:
        payload: dict = {"ts": int(datetime.now(timezone.utc).timestamp())}
        if nonce is not None:
            payload["nonce"] = nonce
        return await self.publish(topics(device_id)["time"], payload, retain=False)

    async def _resync_all_settings(self) -> None:
        async with SessionLocal() as db:
            devices = (await db.scalars(select(Device))).all()
            for device in devices:
                if device.settings_rev != device.device_rev:
                    await self.push_settings(device)

    async def _dispatch(self, message) -> None:
        parts = str(message.topic).split("/")
        if len(parts) < 3 or parts[0] != "env":
            return
        device_id, kind = parts[1], parts[-1]


        if kind in ("settings", "cmd", "time"):
            return

        try:
            data = json.loads(message.payload)
        except (ValueError, TypeError):
            log.warning("Невалидный JSON в %s", message.topic)
            return
        if not isinstance(data, dict):
            return

        now = datetime.now(timezone.utc)
        async with SessionLocal() as db:
            device = await db.get(Device, device_id)
            if device is None:
                log.warning("Сообщение от незарегистрированной станции %s", device_id)
                return

            if kind in ("telemetry", "backlog"):
                await self._on_measurement(db, device, data, now, buffered=(kind == "backlog"))
            elif kind == "status":
                await self._on_status(db, device, data, now)
            elif kind == "timereq":
                nonce = data.get("nonce")
                await self.push_time(device_id, nonce if isinstance(nonce, int) else None)
                device.last_seen = now
            elif kind == "log":
                await store_log(db, device_id, LogIn(**data), now)
                device.last_seen = now
            elif kind == "ack":
                await store_ack(db, device_id, AckIn(**data), now)
                device.last_seen = now
            else:
                return

            await db.commit()

    async def _on_measurement(self, db, device: Device, data: dict,
                              now: datetime, buffered: bool) -> None:
        m = MeasurementIn(**data)
        status = device.last_status or {}
        uptime = status.get("uptime_s")
        if not isinstance(uptime, (int, float)):
            uptime = None
        await store_measurement(
            db, device, m, now,
            buffered=buffered,
            device_uptime_s=None if buffered else uptime,
            current_boot=status.get("boot"),
        )
        device.last_seen = now

    async def _on_status(self, db, device: Device, data: dict, now: datetime) -> None:
        apply_status(device, data, now)

        if data.get("online") is False:
            log.info("Станция %s отключилась", device.device_id)
            return


        if data.get("time_valid") is False:
            log.info("У станции %s недостоверные часы, отправляем время",
                     device.device_id)
            await self.push_time(device.device_id)

        if device.device_rev != device.settings_rev:
            await self.push_settings(device)

        for cmd in await take_pending_commands(db, device.device_id, now):
            await self.push_command(device.device_id, cmd.id, cmd.cmd, cmd.args, cmd.confirm)


bridge = MqttBridge()
