from __future__ import annotations

import time
from dataclasses import dataclass
from datetime import UTC, datetime

from robin.config import logger

_UA = "Mozilla/5.0"
_SEARCH_URL = "https://query1.finance.yahoo.com/v1/finance/search"


@dataclass
class NewsItem:
    title: str
    publisher: str
    published: str
    link: str
    related_tickers: list[str]

    def to_summary(self) -> str:
        tickers = f" [{', '.join(self.related_tickers)}]" if self.related_tickers else ""
        return f"- {self.title} ({self.publisher}, {self.published}){tickers}"


class NewsSource:
    """Public stock news from Yahoo Finance's search endpoint. No API key."""

    def __init__(self, cache_ttl: float = 300.0, timeout: float = 10.0) -> None:
        self.cache_ttl = cache_ttl
        self.timeout = timeout
        self._cache: dict[str, tuple[float, list[NewsItem]]] = {}

    def _fetch(self, query: str, count: int) -> list[NewsItem]:
        import httpx

        try:
            resp = httpx.get(
                _SEARCH_URL,
                params={"q": query, "newsCount": count, "quotesCount": 0},
                headers={"User-Agent": _UA},
                timeout=self.timeout,
            )
        except Exception as e:
            logger.warning("news fetch failed for '%s': %s", query, e)
            return []
        if resp.status_code != 200:
            logger.warning("news fetch '%s' -> %s", query, resp.status_code)
            return []
        items: list[NewsItem] = []
        for n in resp.json().get("news", []):
            ts = n.get("providerPublishTime", 0)
            published = datetime.fromtimestamp(ts, tz=UTC).strftime("%b %d %H:%M") if ts else ""
            items.append(
                NewsItem(
                    title=n.get("title", ""),
                    publisher=n.get("publisher", ""),
                    published=published,
                    link=n.get("link", ""),
                    related_tickers=n.get("relatedTickers", []),
                )
            )
        return items

    def _cached_fetch(self, key: str, query: str, count: int) -> list[NewsItem]:
        now = time.time()
        cached = self._cache.get(key)
        if cached and (now - cached[0]) < self.cache_ttl:
            return cached[1]
        items = self._fetch(query, count)
        self._cache[key] = (now, items)
        return items

    def general_news(self, count: int = 8) -> list[NewsItem]:
        return self._cached_fetch("__general__", "stock market today", count)

    def stock_news(self, symbol: str, count: int = 4) -> list[NewsItem]:
        return self._cached_fetch(symbol.upper(), symbol.upper(), count)

    def general_summary(self) -> str:
        items = self.general_news()
        if not items:
            return "(no general market news available)"
        return "General market news:\n" + "\n".join(n.to_summary() for n in items)

    def stock_summary(self, symbol: str) -> str:
        items = self.stock_news(symbol)
        if not items:
            return f"(no recent news for {symbol})"
        return f"News for {symbol}:\n" + "\n".join(n.to_summary() for n in items)

    def stocks_summary(self, symbols: list[str]) -> str:
        parts = [self.stock_summary(s) for s in symbols]
        return "\n\n".join(parts)
