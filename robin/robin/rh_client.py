from __future__ import annotations

import contextlib
import random
import time
from dataclasses import asdict, dataclass, field
from datetime import UTC, datetime
from typing import Any

from robin.config import POSITIONS_TTL_SECONDS, RECENT_HISTORY_POINTS, SIMULATE, logger
from robin.mcp_client import MCPError, RobinhoodMCPClient
from robin.prices import PublicPriceSource, Quote, holding_from_quote


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
    direction: str
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
    covered: bool = False

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)


@dataclass
class Portfolio:
    holdings: list[Holding] = field(default_factory=list)
    options: list[OptionPosition] = field(default_factory=list)
    watchlist: list[dict[str, Any]] = field(default_factory=list)
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
            "watchlist": self.watchlist,
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
            "watchlist_count": len(self.watchlist),
        }


def _f(value: Any, default: float = 0.0) -> float:
    if value is None:
        return default
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


class RobinhoodClient:
    """Read-only portfolio access via Robinhood's official MCP server.

    Replaces the old robin_stocks (unofficial) approach. Uses OAuth 2.1
    for authentication and JSON-RPC for tool calls. Prices come from the
    public Yahoo source; holdings/options/watchlists/account info come
    from the MCP server (cached 30 min).
    """

    def __init__(self, auth: Any | None = None) -> None:
        self.auth = auth
        self.mode: str = "simulate" if SIMULATE == "1" else "auto"
        self._sim: dict[str, dict[str, Any]] = {}
        self._sim_inited = False
        self._rng = random.Random(20240714)
        self.prices = PublicPriceSource()
        self.mcp = RobinhoodMCPClient()
        self._account_number: str | None = None
        self._positions_cache: tuple[float, list[dict[str, Any]], dict[str, float]] = (0.0, [], {})
        self._options_cache: tuple[float, list[dict[str, Any]]] = (0.0, [])
        self._watchlist_cache: tuple[float, list[dict[str, Any]]] = (0.0, [])
        self._positions_ttl = float(POSITIONS_TTL_SECONDS)

    def resolve_mode(self) -> str:
        if self.mode == "auto":
            if self.mcp.is_authenticated():
                return "real"
            return "simulate"
        return self.mode

    def refresh_holdings(self) -> None:
        self._positions_cache = (0.0, [], {})
        self._options_cache = (0.0, [])
        self._watchlist_cache = (0.0, [])
        logger.info("holdings cache busted; next poll will re-fetch from MCP")

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
            watchlist=[
                {"symbol": s, "name": s, "current_price": d["price"]} for s, d in self._sim.items()
            ],
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

    def _get_account_number(self) -> str:
        if self._account_number:
            return self._account_number
        result = self.mcp.call_tool("get_accounts")
        accounts = result.get("data", {}).get("accounts", []) if isinstance(result, dict) else []
        for acc in accounts:
            if acc.get("is_default"):
                self._account_number = acc["account_number"]
                return self._account_number
        if accounts:
            self._account_number = accounts[0]["account_number"]
            return self._account_number
        raise MCPError("no accounts found")

    def _load_positions(self) -> tuple[list[dict[str, Any]], dict[str, float]]:
        now = time.time()
        cached_at, cached_rows, cached_account = self._positions_cache
        if cached_rows and (now - cached_at) < self._positions_ttl:
            return cached_rows, cached_account

        acct = self._get_account_number()

        result = self.mcp.call_tool("get_equity_positions", {"account_number": acct})
        data = result.get("data", result) if isinstance(result, dict) else {}
        positions = data.get("positions", []) if isinstance(data, dict) else []
        rows: list[dict[str, Any]] = []
        for p in positions:
            qty = _f(p.get("quantity"))
            if qty <= 0:
                continue
            rows.append(
                {
                    "symbol": p.get("symbol", "?"),
                    "quantity": qty,
                    "avg_cost": _f(p.get("average_buy_price")),
                }
            )

        acct_result = self.mcp.call_tool("get_portfolio", {"account_number": acct})
        acct_data = acct_result.get("data", {}) if isinstance(acct_result, dict) else {}
        bp = acct_data.get("buying_power", {})
        account = {
            "cash": _f(acct_data.get("cash")),
            "buying_power": _f(bp.get("buying_power") if isinstance(bp, dict) else bp),
            "unsettled_funds": 0.0,
        }

        self._positions_cache = (now, rows, account)
        return rows, account

    def _load_options(self, stock_symbols: set[str] | None = None) -> list[dict[str, Any]]:
        stock_symbols = stock_symbols or set()
        now = time.time()
        cached_at, cached = self._options_cache
        if cached and (now - cached_at) < self._positions_ttl:
            return cached

        acct = self._get_account_number()
        result = self.mcp.call_tool(
            "get_option_positions", {"account_number": acct, "nonzero": True}
        )
        positions = result.get("data", {}).get("positions", []) if isinstance(result, dict) else []

        opt_ids = [p["option_id"] for p in positions if p.get("option_id")]
        instruments: dict[str, dict[str, Any]] = {}
        if opt_ids:
            with contextlib.suppress(Exception):
                inst_result = self.mcp.call_tool(
                    "get_option_instruments", {"ids": ",".join(opt_ids)}
                )
                insts = (
                    inst_result.get("data", {}).get("instruments", [])
                    if isinstance(inst_result, dict)
                    else []
                )
                if not insts:
                    insts = (
                        inst_result.get("instruments", []) if isinstance(inst_result, dict) else []
                    )
                for inst in insts:
                    instruments[inst.get("id", "")] = inst

        quotes: dict[str, dict[str, Any]] = {}
        if opt_ids:
            with contextlib.suppress(Exception):
                q_result = self.mcp.call_tool("get_option_quotes", {"instrument_ids": opt_ids})
                results = (
                    q_result.get("data", {}).get("results", [])
                    if isinstance(q_result, dict)
                    else []
                )
                for r in results:
                    q = r.get("quote", {})
                    quotes[q.get("instrument_id", "")] = q

        rows: list[dict[str, Any]] = []
        for pos in positions:
            qty = _f(pos.get("quantity"))
            if qty <= 0:
                continue
            chain_symbol = pos.get("chain_symbol", "?")
            direction = pos.get("type", "?")
            expiration = pos.get("expiration_date", "")
            avg_price = _f(pos.get("average_price"))
            tvm = _f(pos.get("trade_value_multiplier"), 100.0)
            opt_id = pos.get("option_id", "")

            inst = instruments.get(opt_id, {})
            strike = _f(inst.get("strike_price"))
            opt_type = inst.get("type", "?")

            q = quotes.get(opt_id, {})
            mark_price = _f(q.get("mark_price"))

            is_short = direction == "short"
            credit_or_cost = abs(avg_price) * qty
            market_value = mark_price * qty * tvm
            unrealized_pl = (
                credit_or_cost - market_value if is_short else market_value - credit_or_cost
            )
            covered = is_short and opt_type == "call" and chain_symbol in stock_symbols

            rows.append(
                {
                    "symbol": chain_symbol,
                    "option_type": opt_type,
                    "direction": direction,
                    "strike": strike,
                    "expiration": expiration,
                    "quantity": qty,
                    "avg_cost": round(abs(avg_price) / tvm, 4),
                    "current_price": mark_price,
                    "market_value": round(market_value, 2),
                    "total_cost": round(credit_or_cost, 2),
                    "unrealized_pl": round(unrealized_pl, 2),
                    "intraday_pl": 0.0,
                    "covered": covered,
                }
            )
        self._options_cache = (now, rows)
        return rows

    def _load_watchlists(self) -> list[dict[str, Any]]:
        now = time.time()
        cached_at, cached = self._watchlist_cache
        if cached and (now - cached_at) < self._positions_ttl:
            # Cache hit: only refresh prices if the watchlist is small (<=50).
            # For large watchlists, prices refresh on the 30-min cycle to
            # avoid hammering Yahoo with 50+ requests every 5 min.
            if len(cached) <= 50:
                for w in cached:
                    quote = self.prices.fetch(w["symbol"], light=True)
                    if quote:
                        w["current_price"] = quote.price
                        w["name"] = quote.name
            return cached

        result = self.mcp.call_tool("get_watchlists")
        data = result.get("data", result) if isinstance(result, dict) else {}
        lists = data.get("watchlists", []) if isinstance(data, dict) else []
        watchlist: list[dict[str, Any]] = []
        seen: set[str] = set()
        for wl in lists:
            allowed = wl.get("allowed_object_types", [])
            if "instrument" not in allowed:
                continue
            list_id = wl.get("id")
            if not list_id:
                continue
            with contextlib.suppress(Exception):
                items_result = self.mcp.call_tool("get_watchlist_items", {"list_id": list_id})
                items = (
                    items_result.get("data", {}).get("items", [])
                    if isinstance(items_result, dict)
                    else []
                )
                for item in items:
                    if item.get("object_type") != "instrument":
                        continue
                    symbol = item.get("symbol", "?")
                    if not symbol or symbol in seen:
                        continue
                    seen.add(symbol)
                    quote = self.prices.fetch(symbol, light=True)
                    watchlist.append(
                        {
                            "symbol": symbol,
                            "name": quote.name if quote else symbol,
                            "current_price": quote.price if quote else 0.0,
                        }
                    )
        self._watchlist_cache = (now, watchlist)
        return watchlist

    def get_portfolio_cached(self, old: dict[str, Any] | None = None) -> Portfolio:
        """Build Portfolio from MCP cache + price cache only (no Yahoo calls).

        Used by the poller to show data immediately before fresh prices arrive.
        If ``old`` is provided (the previous portfolio dict), stale prices are
        carried over for symbols whose price cache is empty.
        """
        old_holdings: dict[str, dict[str, Any]] = {}
        if old:
            for h in old.get("holdings", []):
                old_holdings[h["symbol"]] = h

        rows, account = self._load_positions()

        if not rows and old and old.get("holdings"):
            holdings: list[Holding] = []
            total_mv = 0.0
            total_cost = 0.0
            day_pl = 0.0
            for oh in old["holdings"]:
                symbol = oh["symbol"]
                cached = self.prices._cache.get(symbol.upper())
                if cached:
                    quote = cached[1]
                else:
                    quote = Quote(
                        symbol=symbol,
                        name=oh.get("name", symbol),
                        price=oh.get("current_price", 0.0),
                        prev_close=oh.get("prev_close", 0.0),
                        day_high=oh.get("day_high", 0.0),
                        day_low=oh.get("day_low", 0.0),
                    )
                d = holding_from_quote(symbol, oh["quantity"], oh["avg_cost"], quote)
                holdings.append(Holding(**d))
                total_mv += d["market_value"]
                total_cost += d["total_cost"]
                day_pl += d["day_pl"]
            holdings.sort(key=lambda h: h.market_value, reverse=True)
            for h in holdings:
                h.equity_pct = round((h.market_value / total_mv) * 100, 2) if total_mv else 0.0
            return Portfolio(
                holdings=holdings,
                total_market_value=round(total_mv, 2),
                total_cost=round(total_cost, 2),
                total_unrealized_pl=round(total_mv - total_cost, 2),
                total_unrealized_pl_pct=(
                    round(((total_mv - total_cost) / total_cost) * 100, 2) if total_cost else 0.0
                ),
                cash=old.get("cash", 0.0),
                buying_power=old.get("buying_power", 0.0),
                unsettled_funds=old.get("unsettled_funds", 0.0),
                day_pl=round(day_pl, 2),
                extended_hours=False,
                updated_at=datetime.now(UTC).isoformat(),
                mode="real",
            )

        holdings: list[Holding] = []
        total_mv = 0.0
        total_cost = 0.0
        day_pl = 0.0
        for row in rows:
            symbol = row["symbol"]
            cached = self.prices._cache.get(symbol.upper())
            if cached:
                quote = cached[1]
                d = holding_from_quote(symbol, row["quantity"], row["avg_cost"], quote)
            elif symbol in old_holdings:
                oh = old_holdings[symbol]
                d = holding_from_quote(
                    symbol,
                    row["quantity"],
                    row["avg_cost"],
                    Quote(
                        symbol=symbol,
                        name=oh.get("name", symbol),
                        price=oh.get("current_price", 0.0),
                        prev_close=oh.get("prev_close", 0.0),
                        day_high=oh.get("day_high", 0.0),
                        day_low=oh.get("day_low", 0.0),
                    ),
                )
            else:
                d = holding_from_quote(
                    symbol,
                    row["quantity"],
                    row["avg_cost"],
                    Quote(
                        symbol=symbol,
                        name=symbol,
                        price=0.0,
                        prev_close=0.0,
                        day_high=0.0,
                        day_low=0.0,
                    ),
                )
            holdings.append(Holding(**d))
            total_mv += d["market_value"]
            total_cost += d["total_cost"]
            day_pl += d["day_pl"]
        holdings.sort(key=lambda h: h.market_value, reverse=True)
        for h in holdings:
            h.equity_pct = round((h.market_value / total_mv) * 100, 2) if total_mv else 0.0

        stock_symbols = {h.symbol.upper() for h in holdings}
        options: list[OptionPosition] = []
        try:
            opt_rows = self._load_options(stock_symbols)
            for o in opt_rows:
                cost = o["total_cost"]
                upl = o["unrealized_pl"]
                label = o["option_type"]
                if o["direction"] == "short":
                    label += " short"
                    if o["covered"]:
                        label += " (covered)"
                options.append(
                    OptionPosition(
                        symbol=o["symbol"],
                        option_type=label,
                        direction=o["direction"],
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
                        covered=o["covered"],
                    )
                )
        except Exception as e:
            logger.warning("could not load option positions: %s", e)

        now_ts = time.time()
        watchlist: list[dict[str, Any]] = []
        try:
            cached_at, cached = self._watchlist_cache
            if cached and (now_ts - cached_at) < self._positions_ttl:
                watchlist = cached
        except Exception:
            pass

        return Portfolio(
            holdings=holdings, options=options, watchlist=watchlist,
            total_market_value=round(total_mv, 2), total_cost=round(total_cost, 2),
            total_unrealized_pl=round(total_mv - total_cost, 2),
            total_unrealized_pl_pct=round(((total_mv - total_cost) / total_cost) * 100, 2)
            if total_cost else 0.0,
            cash=account.get("cash", 0.0), buying_power=account.get("buying_power", 0.0),
            unsettled_funds=account.get("unsettled_funds", 0.0), day_pl=round(day_pl, 2),
            extended_hours=False, updated_at=datetime.now(UTC).isoformat(), mode="real",
        )

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

        stock_symbols = {h.symbol.upper() for h in holdings}
        options: list[OptionPosition] = []
        try:
            opt_rows = self._load_options(stock_symbols)
            for o in opt_rows:
                cost = o["total_cost"]
                upl = o["unrealized_pl"]
                label = o["option_type"]
                if o["direction"] == "short":
                    label += " short"
                    if o["covered"]:
                        label += " (covered)"
                options.append(
                    OptionPosition(
                        symbol=o["symbol"],
                        option_type=label,
                        direction=o["direction"],
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
                        covered=o["covered"],
                    )
                )
        except Exception as e:
            logger.warning("could not load option positions: %s", e)

        watchlist: list[dict[str, Any]] = []
        try:
            watchlist = self._load_watchlists()
        except Exception as e:
            logger.warning("could not load watchlists: %s", e)

        return Portfolio(
            holdings=holdings,
            options=options,
            watchlist=watchlist,
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

    def fetch_prices_incremental(self, portfolio: Portfolio) -> Portfolio:
        """Fetch fresh prices one symbol at a time, updating the portfolio in place.

        Returns the updated portfolio. Each symbol that gets a fresh quote
        updates the corresponding holding and recalculates totals.
        """
        for holding in portfolio.holdings:
            quote = self.prices.fetch(holding.symbol, light=True)
            if quote is None:
                continue
            d = holding_from_quote(holding.symbol, holding.quantity, holding.avg_cost, quote)
            holding.current_price = d["current_price"]
            holding.prev_close = d["prev_close"]
            holding.day_high = d["day_high"]
            holding.day_low = d["day_low"]
            holding.market_value = d["market_value"]
            holding.unrealized_pl = d["unrealized_pl"]
            holding.unrealized_pl_pct = d["unrealized_pl_pct"]
            holding.day_pl = d["day_pl"]
            holding.day_pl_pct = d["day_pl_pct"]
        self.prices.flush_cache()
        # Recalculate totals
        total_mv = sum(h.market_value for h in portfolio.holdings)
        total_cost = sum(h.total_cost for h in portfolio.holdings)
        portfolio.total_market_value = round(total_mv, 2)
        portfolio.total_cost = round(total_cost, 2)
        portfolio.total_unrealized_pl = round(total_mv - total_cost, 2)
        portfolio.total_unrealized_pl_pct = (
            round(((total_mv - total_cost) / total_cost) * 100, 2) if total_cost else 0.0
        )
        portfolio.day_pl = round(sum(h.day_pl for h in portfolio.holdings), 2)
        portfolio.updated_at = datetime.now(UTC).isoformat()
        for h in portfolio.holdings:
            h.equity_pct = round((h.market_value / total_mv) * 100, 2) if total_mv else 0.0
        return portfolio

    def get_transactions(self, limit: int = 30) -> list[dict[str, Any]]:
        """Fetch recent transactions from Robinhood MCP.

        Tries get_equity_orders, get_option_orders, get_pnl_trade_history,
        and get_realized_pnl. Returns a merged, newest-first list capped at
        ``limit``.
        """
        if self.resolve_mode() == "simulate":
            return []
        try:
            acct = self._get_account_number()
        except Exception:
            return []

        tools = ["get_equity_orders", "get_option_orders"]
        out: list[dict[str, Any]] = []
        for tool in tools:
            try:
                result = self.mcp.call_tool(tool, {"account_number": acct})
            except Exception as e:
                logger.debug("tool %s failed: %s", tool, e)
                continue
            data = result.get("data", result) if isinstance(result, dict) else {}
            rows = data.get("orders", data.get("results", data.get("items", [])))
            if not isinstance(rows, list):
                if isinstance(data, list):
                    rows = data
                else:
                    continue
            for r in rows[:limit]:
                if tool == "get_option_orders":
                    raw = r.get("direction", "")
                    side = {"debit": "buy", "credit": "sell"}.get(raw, raw)
                    symbol = r.get("chain_symbol", "")
                else:
                    side = r.get("side", "")
                    symbol = r.get("symbol", "")
                out.append({
                    "id": r.get("id", ""),
                    "side": side,
                    "type": r.get("type", ""),
                    "symbol": symbol,
                    "quantity": _f(r.get("quantity")),
                    "price": _f(r.get("average_price", r.get("price"))),
                    "total": _f(r.get("processed_premium", r.get("premium", r.get("total", 0)))),
                    "status": r.get("state", ""),
                    "executed_at": r.get("last_transaction_at") or r.get("created_at") or "",
                })
        out.sort(key=lambda x: x.get("executed_at", ""), reverse=True)
        if out:
            logger.info("loaded %d transactions from MCP", len(out))
        else:
            logger.warning("no transactions returned from MCP order tools")
        return out[:limit]
