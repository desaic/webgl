from __future__ import annotations

import json
import os
from datetime import datetime, timezone

from robin.config import ACCOUNT_HISTORY_PATH, logger

MAX_ENTRIES_PER_DAY = 500


def record_account_value(total_market_value: float) -> None:
    now = datetime.now(timezone.utc)
    date_key = now.strftime("%Y-%m-%d")
    ts = now.isoformat()

    history: dict[str, list[list]] = {}
    if ACCOUNT_HISTORY_PATH.exists():
        try:
            history = json.loads(ACCOUNT_HISTORY_PATH.read_text())
        except (json.JSONDecodeError, OSError):
            history = {}

    day_entries = history.get(date_key, [])

    if day_entries:
        last_ts = day_entries[-1][0]
        if last_ts[:16] == ts[:16]:
            return

    entry: list = [ts, round(total_market_value, 2)]
    day_entries.append(entry)
    if len(day_entries) > MAX_ENTRIES_PER_DAY:
        day_entries = day_entries[-MAX_ENTRIES_PER_DAY:]

    history[date_key] = day_entries

    cutoff = now.replace(hour=0, minute=0, second=0, microsecond=0)
    keys_to_keep = sorted(history.keys())[-90:]
    history = {k: v for k, v in history.items() if k in keys_to_keep}

    tmp_path = str(ACCOUNT_HISTORY_PATH) + ".tmp"
    with open(tmp_path, "w", encoding="utf-8") as f:
        json.dump(history, f, indent=2)
    os.replace(tmp_path, str(ACCOUNT_HISTORY_PATH))


def load_account_history() -> dict[str, list[list]]:
    if not ACCOUNT_HISTORY_PATH.exists():
        return {}
    try:
        return json.loads(ACCOUNT_HISTORY_PATH.read_text())
    except (json.JSONDecodeError, OSError):
        return {}
