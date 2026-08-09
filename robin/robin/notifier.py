from __future__ import annotations

import asyncio
import contextlib
import json
import uuid
from collections import deque
from dataclasses import asdict, dataclass, field
from datetime import UTC, datetime
from typing import Any

from robin.config import MAX_EVENTS_KEPT


@dataclass
class Event:
    symbol: str
    kind: str
    severity: str
    message: str
    data: dict[str, Any] = field(default_factory=dict)
    script: str = ""
    llm: dict[str, Any] | None = None
    id: str = ""
    ts: str = ""

    def __post_init__(self) -> None:
        if not self.id:
            self.id = uuid.uuid4().hex[:12]
        if not self.ts:
            self.ts = datetime.now(UTC).isoformat()
        if self.script and "script" not in self.data:
            self.data["script"] = self.script

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)


class EventBus:
    def __init__(self) -> None:
        self._events: deque[dict[str, Any]] = deque(maxlen=MAX_EVENTS_KEPT)
        self._subs: list[asyncio.Queue[dict[str, Any]]] = []

    def publish(self, event: Event) -> None:
        d = event.to_dict()
        self._events.append(d)
        for q in self._subs:
            with contextlib.suppress(asyncio.QueueFull):
                q.put_nowait(d)

    def publish_raw(self, kind: str, payload: dict[str, Any]) -> None:
        msg = json.dumps(payload)
        d = {
            "id": uuid.uuid4().hex[:12],
            "ts": datetime.now(UTC).isoformat(),
            "kind": kind,
            "payload": payload,
            "message": msg if kind == "portfolio" else payload.get("message", msg),
        }
        if kind != "portfolio":
            self._events.append(d)
        for q in self._subs:
            with contextlib.suppress(asyncio.QueueFull):
                q.put_nowait(d)

    def subscribe(self) -> asyncio.Queue[dict[str, Any]]:
        q: asyncio.Queue[dict[str, Any]] = asyncio.Queue(maxsize=200)
        for d in list(self._events):
            try:
                q.put_nowait(d)
            except asyncio.QueueFull:
                break
        self._subs.append(q)
        return q

    def unsubscribe(self, q: asyncio.Queue[dict[str, Any]]) -> None:
        if q in self._subs:
            self._subs.remove(q)

    def recent(self, n: int = 50) -> list[dict[str, Any]]:
        return list(self._events)[-n:]
