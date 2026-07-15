from __future__ import annotations

import json
import threading
import time
from dataclasses import dataclass, field
from typing import Any

from robin.config import DATA_DIR, RECENT_HISTORY_POINTS, logger

_UA = "Mozilla/5.0"
_YAHOO_HOSTS = [
    "https://query1.finance.yahoo.com/v8/finance/chart/{symbol}",
    "https://query2.finance.yahoo.com/v8/finance/chart/{symbol}",
]


@dataclass
class Quote:
    symbol: str
    name: str
    price: float
    prev_close: float
    day_high: float
    day_low: float
    history: list[float] = field(default_factory=list)


class PriceError(Exception):
    pass


_DISK_CACHE_PATH = DATA_DIR / "prices_cache.json"


class PublicPriceSource:
    """Public market-data source (Yahoo Finance chart endpoint).

    No API key, no account. One call per symbol returns current price, previous
    close, day high/low, and recent intraday closes. We send a real browser
    User-Agent, cap concurrency, and cache briefly so a 60s poll loop makes at
    most one gentle request per symbol per cycle.

    All fetched quotes are persisted to data/prices_cache.json so the app can
    show last-known prices on startup before fresh data arrives.
    """

    def __init__(self, cache_ttl: float = 20.0, timeout: float = 10.0) -> None:
        self.cache_ttl = cache_ttl
        self.timeout = timeout
        self._cache: dict[str, tuple[float, Quote]] = {}
        self._lock = threading.Lock()
        self._disk_dirty = False
        self._load_disk_cache()

    def _fetch_yahoo(self, symbol: str, light: bool = False) -> Quote | None:
        import httpx

        if light:
            params = {"interval": "1d", "range": "1d"}
        else:
            params = {"interval": "5m", "range": "5d", "includeAdjustedClose": "true"}
        headers = {"User-Agent": _UA, "Accept": "application/json"}
        resp = None
        for url_tpl in _YAHOO_HOSTS:
            for attempt in range(2):
                try:
                    resp = httpx.get(
                        url_tpl.format(symbol=symbol),
                        params=params, headers=headers, timeout=self.timeout,
                    )
                except Exception as e:
                    logger.warning("yahoo fetch failed for %s: %s", symbol, e)
                    continue
                if resp.status_code == 429 and attempt == 0:
                    time.sleep(2.0)
                    continue
                break
            if resp is not None and resp.status_code == 200:
                break
        if resp is None or resp.status_code != 200:
            return None
        try:
            data = resp.json().get("chart", {}).get("result", [])
            if not data:
                return None
            meta = data[0].get("meta", {})
            quotes = data[0].get("indicators", {}).get("quote", [])
            closes = quotes[0].get("close", []) if quotes else []
            history = [float(c) for c in closes if c is not None]
            return Quote(
                symbol=meta.get("symbol", symbol),
                name=meta.get("longName") or meta.get("shortName") or symbol,
                price=float(meta.get("regularMarketPrice", 0.0) or 0.0),
                prev_close=float(
                    meta.get("previousClose") or meta.get("chartPreviousClose") or 0.0
                ),
                day_high=float(meta.get("regularMarketDayHigh", 0.0) or 0.0),
                day_low=float(meta.get("regularMarketDayLow", 0.0) or 0.0),
                history=history[-RECENT_HISTORY_POINTS:] if not light else [],
            )
        except Exception as e:
            logger.warning("could not parse yahoo response for %s: %s", symbol, e)
            return None

    def _fetch_google(self, symbol: str) -> Quote | None:
        """Google Finance fallback (current price + name only).

        Used only when Yahoo rate-limits or is unreachable. Google's quote page
        renders prev close / day range via JS, so we can only pull the live
        price from the static HTML. That's enough to value the portfolio; day
        P/L fields stay 0 until Yahoo is available again.
        """
        import re

        import httpx

        for exchange in ("NASDAQ", "NYSE"):
            try:
                resp = httpx.get(
                    f"https://www.google.com/finance/quote/{symbol}:{exchange}",
                    headers={"User-Agent": _UA},
                    timeout=self.timeout,
                    follow_redirects=True,
                )
            except Exception as e:
                logger.warning("google fetch failed for %s: %s", symbol, e)
                continue
            if resp.status_code != 200:
                continue
            m = re.search(r"<span>\$([0-9.,]+)</span>", resp.text)
            if not m:
                continue
            price = float(m.group(1).replace(",", ""))
            return Quote(
                symbol=symbol,
                name=symbol,
                price=price,
                prev_close=0.0,
                day_high=0.0,
                day_low=0.0,
                history=[],
            )
        return None

    def _fetch_one(self, symbol: str, light: bool = False) -> Quote | None:
        quote = self._fetch_yahoo(symbol, light=light)
        if quote is not None:
            return quote
        logger.info("yahoo unavailable for %s; trying Google Finance fallback", symbol)
        return self._fetch_google(symbol)

    def _load_disk_cache(self) -> None:
        """Load last-known prices from disk so the UI shows data on startup."""
        if not _DISK_CACHE_PATH.exists():
            return
        try:
            data = json.loads(_DISK_CACHE_PATH.read_text())
        except (json.JSONDecodeError, OSError):
            return
        loaded = 0
        for symbol, entry in data.items():
            try:
                ts = float(entry.get("_ts", 0))
                quote = Quote(
                    symbol=entry["symbol"],
                    name=entry.get("name", entry["symbol"]),
                    price=float(entry["price"]),
                    prev_close=float(entry.get("prev_close", 0)),
                    day_high=float(entry.get("day_high", 0)),
                    day_low=float(entry.get("day_low", 0)),
                    history=[float(x) for x in entry.get("history", [])],
                )
                self._cache[symbol.upper()] = (ts, quote)
                loaded += 1
            except (KeyError, TypeError, ValueError):
                continue
        if loaded:
            logger.info("loaded %d cached quotes from disk", loaded)

    def _save_disk_cache(self) -> None:
        """Persist all cached quotes to disk (called after fresh fetches)."""
        DATA_DIR.mkdir(parents=True, exist_ok=True)
        data: dict[str, Any] = {}
        with self._lock:
            for symbol, (ts, quote) in self._cache.items():
                data[symbol] = {
                    "_ts": ts,
                    "symbol": quote.symbol,
                    "name": quote.name,
                    "price": quote.price,
                    "prev_close": quote.prev_close,
                    "day_high": quote.day_high,
                    "day_low": quote.day_low,
                    "history": quote.history,
                }
        tmp = _DISK_CACHE_PATH.with_suffix(".json.tmp")
        tmp.write_text(json.dumps(data, indent=2))
        import os

        os.replace(tmp, _DISK_CACHE_PATH)

    def fetch(self, symbol: str, light: bool = True) -> Quote | None:
        symbol = symbol.upper()
        now = time.time()
        with self._lock:
            cached = self._cache.get(symbol)
            if cached and (now - cached[0]) < self.cache_ttl:
                return cached[1]
        quote = self._fetch_one(symbol, light=light)
        if quote is None:
            # Network failed — return stale cached quote if we have one
            with self._lock:
                cached = self._cache.get(symbol)
                if cached:
                    return cached[1]
            return None
        # Light fetch returns no history; merge from cache if available
        if light and not quote.history:
            with self._lock:
                cached = self._cache.get(symbol)
                if cached and cached[1].history:
                    quote.history = cached[1].history
        with self._lock:
            self._cache[symbol] = (now, quote)
        self._save_disk_cache()
        return quote

    def fetch_many(self, symbols: list[str], light: bool = True) -> dict[str, Quote]:
        out: dict[str, Quote] = {}
        for i, sym in enumerate(symbols):
            if i > 0:
                time.sleep(0.5)
            q = self.fetch(sym, light=light)
            if q is not None:
                out[sym.upper()] = q
        return out

    def history(self, symbol: str, n: int = RECENT_HISTORY_POINTS) -> list[float]:
        symbol = symbol.upper()
        # Check cache first (might have history from a previous full fetch)
        with self._lock:
            cached = self._cache.get(symbol)
            if cached and cached[1].history:
                return cached[1].history[-n:]
        # Cache miss or no history — do a full fetch
        quote = self.fetch(symbol, light=False)
        if quote is None:
            return []
        return quote.history[-n:]


def is_available() -> bool:
    """Cheap reachability check used to decide simulate vs real pricing."""
    import httpx

    try:
        r = httpx.get(
            _YAHOO_HOSTS[0].format(symbol="AAPL"),
            params={"interval": "5m", "range": "1d"},
            headers={"User-Agent": _UA},
            timeout=5.0,
        )
        return r.status_code == 200
    except Exception:
        return False


def holding_from_quote(symbol: str, qty: float, avg_cost: float, quote: Quote) -> dict[str, Any]:
    """Build a holdings row dict from a public quote + Robinhood position data."""
    price = quote.price
    mv = price * qty
    cost = avg_cost * qty
    upl = mv - cost
    upl_pct = (upl / cost) if cost else 0.0
    prev = quote.prev_close
    d_pl = (price - prev) * qty if prev else 0.0
    d_pl_pct = ((price / prev) - 1.0) if prev else 0.0
    return {
        "symbol": symbol,
        "name": quote.name,
        "quantity": qty,
        "avg_cost": avg_cost,
        "current_price": price,
        "prev_close": prev,
        "day_high": quote.day_high,
        "day_low": quote.day_low,
        "market_value": round(mv, 2),
        "total_cost": round(cost, 2),
        "unrealized_pl": round(upl, 2),
        "unrealized_pl_pct": round(upl_pct * 100, 2),
        "day_pl": round(d_pl, 2),
        "day_pl_pct": round(d_pl_pct * 100, 2),
    }
