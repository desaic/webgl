from __future__ import annotations

import asyncio
import contextlib
import time
from typing import Any

import anyio

from robin.config import POLL_INTERVAL_SECONDS, logger
from robin.market_hours import is_market_open, next_market_open, seconds_until_open
from robin.notifier import Event, EventBus
from robin.rh_client import RobinhoodClient
from robin.strategy import StrategyEngine


class Poller:
    def __init__(
        self,
        client: RobinhoodClient,
        strategy: StrategyEngine,
        gemini: Any | None,
        bus: EventBus,
        interval: int = POLL_INTERVAL_SECONDS,
    ) -> None:
        self.client = client
        self.strategy = strategy
        self.gemini = gemini
        self.bus = bus
        self.interval = interval
        self.latest_portfolio: dict[str, Any] = {}
        self.last_tick: float = 0.0
        self.last_error: str = ""
        self.market_open: bool = False
        self._task: asyncio.Task[None] | None = None

    def set_gemini(self, gemini: Any | None) -> None:
        self.gemini = gemini

    def start(self) -> None:
        if self._task is None or self._task.done():
            self._task = asyncio.create_task(self._run(), name="robin-poller")

    async def stop(self) -> None:
        if self._task and not self._task.done():
            self._task.cancel()
            with contextlib.suppress(asyncio.CancelledError):
                await self._task

    async def _run(self) -> None:
        while True:
            try:
                self.market_open = is_market_open()
                if self.market_open:
                    await self.tick()
                else:
                    wait = seconds_until_open()
                    logger.info(
                        "market closed; sleeping %.0fs until next open (%s)",
                        wait,
                        next_market_open().strftime("%a %H:%M ET"),
                    )
                    self.bus.publish(
                        Event(
                            symbol="",
                            kind="market_closed",
                            severity="low",
                            message=f"market closed; next open {next_market_open().strftime('%a %b %d %H:%M ET')}",
                        )
                    )
                    await asyncio.sleep(wait)
                    continue
            except asyncio.CancelledError:
                raise
            except Exception as e:
                logger.exception("poller tick failed: %s", e)
                self.last_error = str(e)
                self.bus.publish(
                    Event(symbol="", kind="error", severity="medium", message=f"poller error: {e}")
                )
            await asyncio.sleep(self.interval)

    async def tick(self) -> None:
        t0 = time.time()
        portfolio = await anyio.to_thread.run_sync(self.client.get_portfolio)
        portfolio_dict = portfolio.to_dict()
        self.latest_portfolio = portfolio_dict
        self.bus.publish_raw("portfolio", portfolio_dict)

        events: list[Event] = []
        if self.strategy.scripts:
            histories: dict[str, list[float]] = {}
            for h in portfolio_dict["holdings"]:
                try:
                    histories[h["symbol"]] = await anyio.to_thread.run_sync(
                        self.client.get_price_history, h["symbol"]
                    )
                except Exception as e:
                    logger.debug("history fetch failed for %s: %s", h["symbol"], e)
                    histories[h["symbol"]] = []
            events = await anyio.to_thread.run_sync(self.strategy.evaluate, portfolio_dict, histories)
            for event in events:
                if self.gemini is not None:
                    holding = next(
                        (h for h in portfolio_dict["holdings"] if h["symbol"] == event.symbol), None
                    )
                    if holding:
                        ctx = self.strategy._ctx_for(holding, portfolio_dict, histories.get(event.symbol, []))
                        try:
                            event.llm = await anyio.to_thread.run_sync(
                                self.gemini.reason_on_event, event.to_dict(), ctx
                            )
                        except Exception as e:
                            event.data["llm_error"] = str(e)
                            logger.warning("llm reasoning failed: %s", e)
                self.bus.publish(event)
        self.last_tick = t0
        self.last_error = ""
        logger.info(
            "tick ok: mode=%s holdings=%d events=%d (%.2fs)",
            portfolio_dict["mode"],
            len(portfolio_dict["holdings"]),
            len(events),
            time.time() - t0,
        )
