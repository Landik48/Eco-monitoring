from fastapi import APIRouter, Depends
from sqlalchemy.ext.asyncio import AsyncSession

from ..db import get_db
from ..deps import current_device
from ..ingest_service import handle_payload
from ..models import Device
from ..schemas import IngestIn, IngestOut

router = APIRouter(prefix="/api/v1", tags=["ingest"])


@router.post("/ingest", response_model=IngestOut)
async def ingest(
    payload: IngestIn,
    device: Device = Depends(current_device),
    db: AsyncSession = Depends(get_db),
) -> IngestOut:
    return await handle_payload(db, device, payload)
