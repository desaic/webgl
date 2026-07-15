from __future__ import annotations

import contextlib
import random
import time
from dataclasses import asdict, dataclass, field
from datetime import UTC, datetime
from typing import Any

from robin.config import POSITIONS_TTL_SECONDS, RECENT_HISTORY_POINTS, SIMULATE, logger
from robin.prices import PublicPriceSource, holding_from_quote


@dataclass
class Holding:
    symbol: str
    name: str
    quantity: float
    avg_cost: float
    current_price: float
    prev_close: float
    day_high: float
    day_low: float
    market_value: float
    total_cost: float
    unrealized_pl: float
    unrealized_pl_pct: float
    day_pl: float
    day_pl_pct: float
    equity_pct: float = 0.0

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)


@dataclass
class OptionPosition:
    symbol: str
    option_type: str
    strike: float
    expiration: str
    quantity: float
    avg_cost: float
    current_price: float
    market_value: float
    total_cost: float
    unrealized_pl: float
    unrealized_pl_pct: float
    intraday_pl: float

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)


@dataclass
class Portfolio:
    holdings: list[Holding] = field(default_factory=list)
    options: list[OptionPosition] = field(default_factory=list)
    total_market_value: float = 0.0
    total_cost: float = 0.0
    total_unrealized_pl: float = 0.0
    total_unrealized_pl_pct: float = 0.0
    cash: float = 0.0
    buying_power: float = 0.0
    unsettled_funds: float = 0.0
    day_pl: float = 0.0
    extended_hours: bool = False
    updated_at: str = ""
    mode: str = "real"

    def to_dict(self) -> dict[str, Any]:
        return {
            "holdings": [h.to_dict() for h in self.holdings],
            "options": [o.to_dict() for o in self.options],
            "total_market_value": self.total_market_value,
            "total_cost": self.total_cost,
            "total_unrealized_pl": self.total_unrealized_pl,
            "total_unrealized_pl_pct": self.total_unrealized_pl_pct,
            "cash": self.cash,
            "buying_power": self.buying_power,
            "unsettled_funds": self.unsettled_funds,
            "day_pl": self.day_pl,
            "extended_hours": self.extended_hours,
            "updated_at": self.updated_at,
            "mode": self.mode,
            "holding_count": len(self.holdings),
            "option_count": len(self.options),
        }


def _f(value: Any, default: float = 0.0) -> float:
    if value is None:
        return default
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


class RobinhoodClient:
    """Read-only portfolio/price access. Real mode talks to robin_stocks;
    simulate mode synthesizes a small portfolio so the rest of the app can run
    without credentials or network.
    """

    def __init__(self, auth: Any | None = None) -> None:
        self.auth = auth
        self.mode: str = "simulate" if SIMULATE == "1" else "auto"
        self._instrument_symbol: dict[str, str] = {}
        self._sim: dict[str, dict[str, Any]] = {}
        self._sim_inited = False
        self._rng = random.Random(20240714)
        self.prices = PublicPriceSource()
        self._positions_cache: tuple[float, list[dict[str, Any]], dict[str, float]] = (0.0, [], {})
        self._options_cache: tuple[float, list[dict[str, Any]]] = (0.0, [])
        self._positions_ttl = float(POSITIONS_TTL_SECONDS)

    def refresh_holdings(self) -> None:
        """Bust the positions and options cache so the next get_portfolio()
        re-fetches from Robinhood. Call this after you place a trade.
        """
        self._positions_cache = (0.0, [], {})
        self._options_cache = (0.0, [])
        logger.info("holdings cache busted; next poll will re-fetch from Robinhood")

    def resolve_mode(self) -> str:
        if self.mode == "auto":
            if self.auth is not None and self.auth.has_session():
                return "real"
            return "simulate"
        return self.mode

    def _ensure_sim(self) -> None:
        if self._sim_inited:
            return
        seeds = [
            ("AAPL", "Apple Inc.", 12, 150.0),
            ("MSFT", "Microsoft Corp.", 5, 380.0),
            ("NVDA", "NVIDIA Corp.", 3, 110.0),
            ("GOOGL", "Alphabet Inc.", 8, 140.0),
            ("TSLA", "Tesla Inc.", 6, 250.0),
        ]
        for symbol, name, qty, cost in seeds:
            price = cost * (1 + self._rng.uniform(-0.05, 0.05))
            history = []
            p = cost * 0.9
            for _ in range(RECENT_HISTORY_POINTS):
                p = max(1.0, p * (1 + self._rng.gauss(0, 0.01)))
                history.append(round(p, 2))
            history[-1] = round(price, 2)
            self._sim[symbol] = {
                "name": name,
                "quantity": float(qty),
                "avg_cost": float(cost),
                "price": round(price, 2),
                "prev_close": round(history[-2], 2) if len(history) > 1 else round(price, 2),
                "history": history,
            }
        self._sim_inited = True

    def _sim_tick(self) -> None:
        for _symbol, s in self._sim.items():
            new_price = max(1.0, s["price"] * (1 + self._rng.gauss(0, 0.004)))
            s["prev_close"] = s["history"][-1] if s["history"] else s["price"]
            s["price"] = round(new_price, 2)
            s["history"].append(s["price"])
            if len(s["history"]) > RECENT_HISTORY_POINTS:
                s["history"] = s["history"][-RECENT_HISTORY_POINTS:]

    def get_portfolio(self) -> Portfolio:
        if self.resolve_mode() == "simulate":
            return self._get_portfolio_simulated()
        return self._get_portfolio_real()

    def _get_portfolio_simulated(self) -> Portfolio:
        self._ensure_sim()
        self._sim_tick()
        holdings: list[Holding] = []
        total_mv = 0.0
        total_cost = 0.0
        day_pl = 0.0
        for symbol, s in self._sim.items():
            price = s["price"]
            qty = s["quantity"]
            mv = price * qty
            cost = s["avg_cost"] * qty
            upl = mv - cost
            upl_pct = (upl / cost) if cost else 0.0
            prev = s["prev_close"]
            d_pl = (price - prev) * qty
            d_pl_pct = (price / prev - 1.0) if prev else 0.0
            holdings.append(
                Holding(
                    symbol=symbol,
                    name=s["name"],
                    quantity=qty,
                    avg_cost=s["avg_cost"],
                    current_price=price,
                    prev_close=prev,
                    day_high=round(max(s["history"][-6:]) if s["history"] else price, 2),
                    day_low=round(min(s["history"][-6:]) if s["history"] else price, 2),
                    market_value=round(mv, 2),
                    total_cost=round(cost, 2),
                    unrealized_pl=round(upl, 2),
                    unrealized_pl_pct=round(upl_pct * 100, 2),
                    day_pl=round(d_pl, 2),
                    day_pl_pct=round(d_pl_pct * 100, 2),
                )
            )
            total_mv += mv
            total_cost += cost
            day_pl += d_pl
        holdings.sort(key=lambda h: h.market_value, reverse=True)
        for h in holdings:
            h.equity_pct = round((h.market_value / total_mv) * 100, 2) if total_mv else 0.0
        return Portfolio(
            holdings=holdings,
            options=[],
            total_market_value=round(total_mv, 2),
            total_cost=round(total_cost, 2),
            total_unrealized_pl=round(total_mv - total_cost, 2),
            total_unrealized_pl_pct=round(((total_mv - total_cost) / total_cost) * 100, 2)
            if total_cost
            else 0.0,
            cash=5000.0,
            buying_power=10000.0,
            unsettled_funds=0.0,
            day_pl=round(day_pl, 2),
            extended_hours=False,
            updated_at=datetime.now(UTC).isoformat(),
            mode="simulate",
        )

    def _symbol_for_instrument(self, url: str) -> str:
        if url in self._instrument_symbol:
            return self._instrument_symbol[url]
        import robin_stocks.robinhood as r

        data = r.stocks.get_instrument_by_url(url) or {}
        symbol = data.get("symbol", "?")
        self._instrument_symbol[url] = symbol
        return symbol

    def _load_positions(self) -> tuple[list[dict[str, Any]], float]:
        """Fetch open stock positions + account info from Robinhood.

        Holdings (what you own, cost basis) and account info (cash, buying
        power) change rarely, so we cache them for several minutes. Prices are
        NOT fetched here -- those come from the public source.
        """
        now = time.time()
        cached_at, cached_rows, cached_account = self._positions_cache
        if cached_rows and (now - cached_at) < self._positions_ttl:
            return cached_rows, cached_account
        import robin_stocks.robinhood as r

        positions = r.account.get_open_stock_positions() or []
        rows: list[dict[str, Any]] = []
        for p in positions:
            qty = _f(p.get("quantity"))
            if qty <= 0:
                continue
            symbol = self._symbol_for_instrument(p.get("instrument", ""))
            rows.append(
                {
                    "symbol": symbol,
                    "quantity": qty,
                    "avg_cost": _f(p.get("average_buy_price")),
                }
            )
        account = {"cash": 0.0, "buying_power": 0.0, "unsettled_funds": 0.0}
        try:
            portfolio_profile = r.load_portfolio_profile() or {}
            account["cash"] = _f(portfolio_profile.get("uninvested_cash"))
        except Exception as e:
            logger.debug("could not load portfolio profile: %s", e)
        try:
            account_profile = r.load_account_profile() or {}
            account["buying_power"] = _f(account_profile.get("buying_power"))
            account["unsettled_funds"] = _f(account_profile.get("unsettled_funds"))
            if not account["cash"]:
                account["cash"] = _f(account_profile.get("cash"))
        except Exception as e:
            logger.debug("could not load account profile: %s", e)
        self._positions_cache = (now, rows, account)
        return rows, account

    def _load_options(self) -> list[dict[str, Any]]:
        """Fetch open option positions from Robinhood (cached 30 min)."""
        now = time.time()
        cached_at, cached = self._options_cache
        if cached and (now - cached_at) < self._positions_ttl:
            return cached
        import robin_stocks.robinhood as r

        url = r.account.option_positions_url(None)
        data = r.helper.request_get(url, "pagination") or []
        rows: list[dict[str, Any]] = []
        for pos in data:
            qty = _f(pos.get("quantity"))
            if qty <= 0:
                continue
            option_url = pos.get("option") or ""
            option_data = {}
            if option_url:
                with contextlib.suppress(Exception):
                    option_data = r.helper.request_document(option_url) or {}
            rows.append(
                {
                    "symbol": option_data.get("chain_symbol", "?"),
                    "option_type": option_data.get("type", "?"),
                    "strike": _f(option_data.get("strike_price")),
                    "expiration": option_data.get("expiration_date", ""),
                    "quantity": qty,
                    "avg_cost": _f(pos.get("average_price")),
                    "current_price": _f(pos.get("mark_price") or pos.get("current_price")),
                    "market_value": _f(pos.get("market_value")),
                    "total_cost": _f(pos.get("average_price")) * qty,
                    "unrealized_pl": _f(pos.get("market_value"))
                    - _f(pos.get("average_price")) * qty,
                    "intraday_pl": _f(pos.get("intraday_profit_loss")),
                }
            )
        self._options_cache = (now, rows)
        return rows

    def _get_portfolio_real(self) -> Portfolio:
        rows, account = self._load_positions()
        symbols = [row["symbol"] for row in rows]
        quotes = self.prices.fetch_many(symbols)
        holdings: list[Holding] = []
        total_mv = 0.0
        total_cost = 0.0
        day_pl = 0.0
        for row in rows:
            symbol = row["symbol"]
            quote = quotes.get(symbol.upper())
            if quote is None:
                logger.warning("no public quote for %s; skipping this poll", symbol)
                continue
            d = holding_from_quote(symbol, row["quantity"], row["avg_cost"], quote)
            holdings.append(Holding(**d))
            total_mv += d["market_value"]
            total_cost += d["total_cost"]
            day_pl += d["day_pl"]
        holdings.sort(key=lambda h: h.market_value, reverse=True)
        for h in holdings:
            h.equity_pct = round((h.market_value / total_mv) * 100, 2) if total_mv else 0.0

        options: list[OptionPosition] = []
        try:
            opt_rows = self._load_options()
            for o in opt_rows:
                cost = o["total_cost"]
                upl = o["unrealized_pl"]
                options.append(
                    OptionPosition(
                        symbol=o["symbol"],
                        option_type=o["option_type"],
                        strike=o["strike"],
                        expiration=o["expiration"],
                        quantity=o["quantity"],
                        avg_cost=o["avg_cost"],
                        current_price=o["current_price"],
                        market_value=round(o["market_value"], 2),
                        total_cost=round(cost, 2),
                        unrealized_pl=round(upl, 2),
                        unrealized_pl_pct=round((upl / cost) * 100, 2) if cost else 0.0,
                        intraday_pl=round(o["intraday_pl"], 2),
                    )
                )
        except Exception as e:
            logger.warning("could not load option positions: %s", e)

        return Portfolio(
            holdings=holdings,
            options=options,
            total_market_value=round(total_mv, 2),
            total_cost=round(total_cost, 2),
            total_unrealized_pl=round(total_mv - total_cost, 2),
            total_unrealized_pl_pct=round(((total_mv - total_cost) / total_cost) * 100, 2)
            if total_cost
            else 0.0,
            cash=account.get("cash", 0.0),
            buying_power=account.get("buying_power", 0.0),
            unsettled_funds=account.get("unsettled_funds", 0.0),
            day_pl=round(day_pl, 2),
            extended_hours=False,
            updated_at=datetime.now(UTC).isoformat(),
            mode="real",
        )

    def get_price_history(self, symbol: str, n: int = RECENT_HISTORY_POINTS) -> list[float]:
        if self.resolve_mode() == "simulate":
            self._ensure_sim()
            hist = self._sim.get(symbol, {}).get("history", [])
            return [float(x) for x in hist[-n:]]
        return self.prices.history(symbol, n)
