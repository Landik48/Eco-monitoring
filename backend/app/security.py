import hashlib
import hmac
from datetime import datetime, timedelta, timezone

import jwt
from passlib.context import CryptContext

from .config import get_config

pwd_context = CryptContext(schemes=["bcrypt"], deprecated="auto")


def hash_password(password: str) -> str:
    return pwd_context.hash(password)


def verify_password(password: str, password_hash: str) -> bool:
    return pwd_context.verify(password, password_hash)


def create_access_token(subject: str) -> str:
    cfg = get_config()
    now = datetime.now(timezone.utc)
    payload = {
        "sub": subject,
        "iat": now,
        "exp": now + timedelta(minutes=cfg.access_token_ttl_min),
    }
    return jwt.encode(payload, cfg.jwt_secret, algorithm=cfg.jwt_algorithm)


def decode_access_token(token: str) -> dict | None:
    cfg = get_config()
    try:
        return jwt.decode(token, cfg.jwt_secret, algorithms=[cfg.jwt_algorithm])
    except jwt.PyJWTError:
        return None


def hash_device_token(token: str) -> str:
    return hashlib.sha256(token.encode()).hexdigest()


def verify_device_token(token: str, token_hash: str) -> bool:
    return hmac.compare_digest(hash_device_token(token), token_hash)
