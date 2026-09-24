import logging

from sqlalchemy import text
from sqlalchemy.ext.asyncio import AsyncConnection

log = logging.getLogger(__name__)


_COLUMNS: list[tuple[str, str, str]] = [
    ("measurements", "quality", "VARCHAR(16) NOT NULL DEFAULT 'ok'"),
    ("measurements", "flags", "INTEGER NOT NULL DEFAULT 0"),
    ("measurements", "stability", "INTEGER"),
    ("measurements", "time_source", "VARCHAR(16)"),
    ("measurements", "rejected_raw", "JSONB"),
]

_INDEXES: list[tuple[str, str]] = [
    (
        "ix_measurements_quality",
        "CREATE INDEX IF NOT EXISTS ix_measurements_quality "
        "ON measurements (device_id, quality, ts)",
    ),
]


async def run(conn: AsyncConnection) -> None:
    for table, column, ddl in _COLUMNS:
        await conn.execute(
            text(f'ALTER TABLE {table} ADD COLUMN IF NOT EXISTS "{column}" {ddl}')
        )

    for name, ddl in _INDEXES:
        await conn.execute(text(ddl))

    log.info("Схема приведена к текущей версии")
