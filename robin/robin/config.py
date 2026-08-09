from __future__ import annotations

import logging
import os
from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent.parent
DATA_DIR = BASE_DIR / "data"
SCRIPTS_DIR = BASE_DIR / "scripts"
STATIC_DIR = BASE_DIR / "static"
GEMINI_KEY_FILE = Path(
    os.environ.get("GEMINI_KEY_FILE", str(Path.home() / "gemini_free_api_key" / "key.txt"))
)
ROBINHOOD_SESSION_PATH = DATA_DIR / "rh_session.json"

READ_ONLY = True

GEMINI_MODEL = os.environ.get("GEMINI_MODEL", "gemini-flash-lite-latest")
GEMINI_API_KEY_ENV = os.environ.get("GEMINI_API_KEY", "")
POLL_INTERVAL_SECONDS = int(os.environ.get("ROBIN_POLL_INTERVAL", "300"))
POSITIONS_TTL_SECONDS = int(os.environ.get("ROBIN_POSITIONS_TTL", "1800"))
SIMULATE = os.environ.get("ROBIN_SIMULATE", "auto").lower()
MAX_EVENTS_KEPT = 200
RECENT_HISTORY_POINTS = 32

logger = logging.getLogger("robin")


def setup_logging() -> None:
    logging.basicConfig(
        level=os.environ.get("ROBIN_LOG_LEVEL", "INFO"),
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
    )


def ensure_dirs() -> None:
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    SCRIPTS_DIR.mkdir(parents=True, exist_ok=True)
