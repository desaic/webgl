from __future__ import annotations

import math
from pathlib import Path

import pytest

from robin.auth_robinhood import RobinhoodAuth
from robin.market_hours import (
    ET,
    is_market_open,
    is_trading_day,
    next_market_open,
    nyse_holidays,
)
from robin.prices import PublicPriceSource, Quote, holding_from_quote
from robin.rh_client import RobinhoodClient
from robin.strategy import StrategyEngine


def client():
    c = RobinhoodClient(auth=None)
    c.mode = "simulate"
    return c


def test_simulated_portfolio_math_is_consistent():
    p = client().get_portfolio().to_dict()
    mv_sum = sum(h["market_value"] for h in p["holdings"])
    cost_sum = sum(h["total_cost"] for h in p["holdings"])
    pl_sum = sum(h["unrealized_pl"] for h in p["holdings"])
    weight_sum = sum(h["equity_pct"] for h in p["holdings"])
    assert math.isclose(mv_sum, p["total_market_value"], abs_tol=0.02)
    assert math.isclose(cost_sum, p["total_cost"], abs_tol=0.02)
    assert math.isclose(pl_sum, p["total_unrealized_pl"], abs_tol=0.02)
    assert math.isclose(weight_sum, 100.0, abs_tol=0.05)
    for h in p["holdings"]:
        assert h["equity_pct"] >= 0
        assert h["quantity"] > 0


def test_strategy_script_detects_stop_loss(tmp_path: Path):
    eng = StrategyEngine(scripts_dir=tmp_path)
    eng.add_script(
        "stop_loss",
        "def check(ctx):\n"
        "    if ctx['current_price'] <= ctx['avg_cost'] * 0.92:\n"
        "        return {'kind':'stop_loss','severity':'high',\n"
        "                'message':f\"{ctx['symbol']} hit stop at {ctx['current_price']}\"}\n"
        "    return None\n",
        targets=None,
    )
    portfolio = {
        "updated_at": "2026-01-01T00:00:00Z",
        "total_market_value": 1000,
        "total_cost": 1100,
        "total_unrealized_pl": -100,
        "total_unrealized_pl_pct": -9.0,
        "holdings": [
            {
                "symbol": "AAPL",
                "name": "Apple",
                "quantity": 10,
                "avg_cost": 100,
                "current_price": 90,
                "prev_close": 95,
                "day_high": 96,
                "day_low": 89,
                "market_value": 900,
                "unrealized_pl": -100,
                "unrealized_pl_pct": -10,
                "equity_pct": 90,
            },
            {
                "symbol": "MSFT",
                "name": "Microsoft",
                "quantity": 2,
                "avg_cost": 400,
                "current_price": 420,
                "prev_close": 415,
                "day_high": 425,
                "day_low": 414,
                "market_value": 840,
                "unrealized_pl": 40,
                "unrealized_pl_pct": 5,
                "equity_pct": 84,
            },
        ],
    }
    events = eng.evaluate(portfolio, histories={"AAPL": [], "MSFT": []})
    assert len(events) == 1
    assert events[0].symbol == "AAPL"
    assert events[0].kind == "stop_loss"
    assert events[0].script == "stop_loss"


def test_strategy_script_targets_filter(tmp_path: Path):
    eng = StrategyEngine(scripts_dir=tmp_path)
    eng.add_script(
        "aapl_only",
        "def check(ctx):\n    return {'kind':'signal','severity':'low','message':'x'}\n",
        targets=["AAPL"],
    )
    portfolio = {
        "holdings": [
            {
                "symbol": "AAPL",
                "quantity": 1,
                "avg_cost": 1,
                "current_price": 1,
                "market_value": 1,
                "unrealized_pl": 0,
                "unrealized_pl_pct": 0,
                "equity_pct": 50,
            },
            {
                "symbol": "MSFT",
                "quantity": 1,
                "avg_cost": 1,
                "current_price": 1,
                "market_value": 1,
                "unrealized_pl": 0,
                "unrealized_pl_pct": 0,
                "equity_pct": 50,
            },
        ]
    }
    events = eng.evaluate(portfolio, histories={"AAPL": [], "MSFT": []})
    assert [e.symbol for e in events] == ["AAPL"]


def test_sandbox_blocks_import(tmp_path: Path):
    eng = StrategyEngine(scripts_dir=tmp_path)
    with pytest.raises(ImportError):
        eng.add_script("evil", "import os\ndef check(ctx):\n    return None\n")


def test_script_persistent_state_across_polls(tmp_path: Path, monkeypatch):
    """Verify that ctx['state'] persists to disk and is loaded on the next poll."""
    state_dir = tmp_path / "script_state"
    monkeypatch.setattr("robin.strategy._STATE_DIR", state_dir)

    eng = StrategyEngine(scripts_dir=tmp_path)
    eng.add_script(
        "trailing_stop",
        "def check(ctx):\n"
        "    s = ctx['state']\n"
        "    hi = s.get('high', ctx['current_price'])\n"
        "    s['high'] = max(hi, ctx['current_price'])\n"
        "    if ctx['current_price'] < s['high'] * 0.95:\n"
        "        return {'kind':'trailing_stop','severity':'high',\n"
        "                'message':f\"{ctx['symbol']} below 5% trailing stop\"}\n"
        "    return None\n",
        targets=["AAPL"],
    )
    portfolio = {
        "holdings": [
            {
                "symbol": "AAPL",
                "quantity": 10,
                "avg_cost": 100,
                "current_price": 100,
                "market_value": 1000,
                "unrealized_pl": 0,
                "unrealized_pl_pct": 0,
                "equity_pct": 100,
            },
        ]
    }
    # Poll 1: price 100, high becomes 100, no trigger
    events = eng.evaluate(portfolio, histories={"AAPL": []})
    assert events == []
    state_file = state_dir / "trailing_stop.json"
    assert state_file.exists()
    import json

    saved = json.loads(state_file.read_text())
    assert saved["AAPL"]["high"] == 100

    # Poll 2: price rises to 110, high becomes 110, no trigger
    portfolio["holdings"][0]["current_price"] = 110
    events = eng.evaluate(portfolio, histories={"AAPL": []})
    assert events == []
    saved = json.loads(state_file.read_text())
    assert saved["AAPL"]["high"] == 110

    # Poll 3: price drops to 104 (below 110 * 0.95 = 104.5), trigger fires
    portfolio["holdings"][0]["current_price"] = 104
    events = eng.evaluate(portfolio, histories={"AAPL": []})
    assert len(events) == 1
    assert events[0].kind == "trailing_stop"
    assert events[0].symbol == "AAPL"
    saved = json.loads(state_file.read_text())
    assert saved["AAPL"]["high"] == 110  # high preserved


def test_sandbox_blocks_open_at_runtime(tmp_path: Path):
    eng = StrategyEngine(scripts_dir=tmp_path)
    eng.add_script("evil2", "def check(ctx):\n    open('/etc/passwd')\n    return None\n")
    portfolio = {
        "holdings": [
            {
                "symbol": "X",
                "quantity": 1,
                "avg_cost": 1,
                "current_price": 1,
                "market_value": 1,
                "unrealized_pl": 0,
                "unrealized_pl_pct": 0,
                "equity_pct": 100,
            }
        ]
    }
    events = eng.evaluate(portfolio, histories={"X": []})
    assert len(events) == 1
    assert events[0].kind == "script_error"


def test_robinhood_auth_read_only_guard():
    auth = RobinhoodAuth()
    auth.assert_read_only()


def test_mcp_credentials_detection(monkeypatch, tmp_path):
    """Verify the MCP client detects stored credentials."""
    import json
    import time

    creds_path = tmp_path / "rh_mcp_creds.json"
    monkeypatch.setattr("robin.mcp_client.CREDS_PATH", creds_path)

    # No creds → not authenticated
    from robin.mcp_client import RobinhoodMCPClient

    client = RobinhoodMCPClient()
    assert not client.has_credentials()
    assert not client.is_authenticated()

    # Write valid creds
    now = time.time()
    creds_path.write_text(
        json.dumps(
            {
                "access_token": "test_token",
                "refresh_token": "test_refresh",
                "access_token_expires_at": int(now + 3600),
                "token_type": "Bearer",
                "scope": "internal",
            }
        )
    )

    client2 = RobinhoodMCPClient()
    assert client2.has_credentials()
    assert client2.is_authenticated()


def test_mcp_logout_deletes_credentials(monkeypatch, tmp_path):
    """Verify logout deletes the MCP credentials file."""
    import json
    import time

    creds_path = tmp_path / "rh_mcp_creds.json"
    now = time.time()
    creds_path.write_text(
        json.dumps(
            {
                "access_token": "tok",
                "refresh_token": "ref",
                "access_token_expires_at": int(now + 3600),
                "token_type": "Bearer",
                "scope": "internal",
            }
        )
    )
    monkeypatch.setattr("robin.mcp_client.CREDS_PATH", creds_path)

    from robin.mcp_client import RobinhoodMCPClient

    client = RobinhoodMCPClient()
    assert client.has_credentials()

    client.logout()
    assert not client.has_credentials()
    assert not creds_path.exists()


def test_gemini_key_manager_set_and_clear(monkeypatch, tmp_path):
    key_path = tmp_path / "key.txt"
    monkeypatch.setattr("robin.config.GEMINI_KEY_FILE", key_path)
    monkeypatch.setattr("robin.config.GEMINI_API_KEY_ENV", "")
    monkeypatch.setattr("robin.app.GEMINI_KEY_FILE", key_path)
    monkeypatch.setattr("robin.app.GEMINI_API_KEY_ENV", "")

    from robin.app import GeminiKeyManager

    km = GeminiKeyManager()
    assert not km.has_key()

    km.set_key("AIzaSyTestKey123")
    assert km.has_key()
    assert km.get_key() == "AIzaSyTestKey123"
    assert key_path.read_text() == "AIzaSyTestKey123"

    km2 = GeminiKeyManager()
    assert km2.has_key()
    assert km2.get_key() == "AIzaSyTestKey123"

    km.clear()
    assert not km.has_key()
    assert not key_path.exists()


def test_holding_from_quote_math():
    q = Quote(
        symbol="AAPL",
        name="Apple",
        price=120.0,
        prev_close=115.0,
        day_high=122.0,
        day_low=114.0,
        history=[115.0, 118.0, 120.0],
    )
    d = holding_from_quote("AAPL", qty=10, avg_cost=100.0, quote=q)
    assert d["market_value"] == 1200.0
    assert d["total_cost"] == 1000.0
    assert d["unrealized_pl"] == 200.0
    assert d["unrealized_pl_pct"] == 20.0
    assert d["day_pl"] == 50.0
    assert d["current_price"] == 120.0
    assert d["prev_close"] == 115.0


def test_public_price_source_caches_and_uses(monkeypatch, tmp_path):
    monkeypatch.setattr("robin.prices._DISK_CACHE_PATH", tmp_path / "prices.json")
    calls: list[str] = []

    def fake_fetch_one(self, symbol: str, light: bool = False) -> Quote:
        calls.append(symbol)
        return Quote(
            symbol=symbol,
            name=symbol,
            price=100.0,
            prev_close=99.0,
            day_high=101.0,
            day_low=98.0,
            history=[99.0, 100.0],
        )

    monkeypatch.setattr(PublicPriceSource, "_fetch_one", fake_fetch_one)
    src = PublicPriceSource(cache_ttl=10.0)
    a = src.fetch("AAPL")
    b = src.fetch("AAPL")
    assert a is not None and b is not None
    assert a.price == 100.0
    assert calls == ["AAPL"]  # second call served from cache


def test_real_mode_uses_public_prices_not_mcp(monkeypatch):
    """Prices come from Yahoo, not the MCP server. MCP is only for holdings."""
    c = RobinhoodClient(auth=None)
    c.mode = "real"

    monkeypatch.setattr(
        c,
        "_load_positions",
        lambda: (
            [{"symbol": "AAPL", "quantity": 10, "avg_cost": 100.0}],
            {"cash": 500.0, "buying_power": 1000.0, "unsettled_funds": 0.0},
        ),
    )
    monkeypatch.setattr(c, "_load_options", lambda: [])
    monkeypatch.setattr(c, "_load_watchlists", lambda: [])

    def fake_fetch_many(symbols):
        return {
            s.upper(): Quote(
                symbol=s.upper(),
                name=s,
                price=200.0,
                prev_close=190.0,
                day_high=205.0,
                day_low=188.0,
                history=[190.0, 200.0],
            )
            for s in symbols
        }

    monkeypatch.setattr(c.prices, "fetch_many", fake_fetch_many)

    p = c.get_portfolio()
    assert p.mode == "real"
    assert p.holdings[0].current_price == 200.0
    assert p.holdings[0].unrealized_pl == 1000.0


def test_refresh_holdings_busts_cache(monkeypatch):
    import time

    c = RobinhoodClient(auth=None)
    c.mode = "real"

    def fake_fetch_many(symbols):
        return {
            s.upper(): Quote(
                symbol=s.upper(),
                name=s,
                price=200.0,
                prev_close=190.0,
                day_high=205.0,
                day_low=188.0,
                history=[],
            )
            for s in symbols
        }

    monkeypatch.setattr(c.prices, "fetch_many", fake_fetch_many)

    # Pre-seed the positions cache with qty=10; while the cache is valid,
    # _load_positions returns early (no MCP call needed).
    c._positions_cache = (
        time.time(),
        [{"symbol": "AAPL", "quantity": 10, "avg_cost": 100.0}],
        {"cash": 0.0, "buying_power": 0.0, "unsettled_funds": 0.0},
    )
    c._options_cache = (time.time(), [])
    monkeypatch.setattr(c, "_load_options", lambda: [])
    monkeypatch.setattr(c, "_load_watchlists", lambda: [])
    p1 = c.get_portfolio()
    assert p1.holdings[0].quantity == 10

    # After a trade: bust the cache.
    c.refresh_holdings()
    assert c._positions_cache[1] == []


def test_google_fallback_used_when_yahoo_fails(monkeypatch, tmp_path):
    monkeypatch.setattr("robin.prices._DISK_CACHE_PATH", tmp_path / "prices.json")
    src = PublicPriceSource(cache_ttl=0.0)

    monkeypatch.setattr(PublicPriceSource, "_fetch_yahoo", lambda self, s, light=False: None)
    monkeypatch.setattr(
        PublicPriceSource,
        "_fetch_google",
        lambda self, s: Quote(
            symbol=s, name=s, price=333.0, prev_close=0.0, day_high=0.0, day_low=0.0, history=[]
        ),
    )
    q = src.fetch("AAPL")
    assert q is not None
    assert q.price == 333.0
    assert q.history == []


def test_yahoo_preferred_over_google(monkeypatch, tmp_path):
    monkeypatch.setattr("robin.prices._DISK_CACHE_PATH", tmp_path / "prices.json")
    src = PublicPriceSource(cache_ttl=0.0)
    monkeypatch.setattr(
        PublicPriceSource,
        "_fetch_yahoo",
        lambda self, s, light=False: Quote(
            symbol=s,
            name=s,
            price=200.0,
            prev_close=199.0,
            day_high=201.0,
            day_low=198.0,
            history=[199.0, 200.0],
        ),
    )
    google_called: list[str] = []
    monkeypatch.setattr(
        PublicPriceSource,
        "_fetch_google",
        lambda self, s: (
            google_called.append(s)
            or Quote(symbol=s, name=s, price=999.0, prev_close=0, day_high=0, day_low=0, history=[])
        ),
    )
    q = src.fetch("AAPL")
    assert q is not None
    assert q.price == 200.0
    assert google_called == []


# --- market hours tests ---


def test_weekends_are_not_trading_days():
    from datetime import date

    assert not is_trading_day(date(2026, 7, 11))  # Saturday
    assert not is_trading_day(date(2026, 7, 12))  # Sunday


def test_weekdays_without_holidays_are_trading_days():
    from datetime import date

    assert is_trading_day(date(2026, 7, 13))  # Monday, no holiday


def test_2026_holidays():
    from datetime import date

    h = nyse_holidays(2026)
    assert date(2026, 1, 1) in h  # New Year's Day (Thu)
    assert date(2026, 1, 19) in h  # MLK Day
    assert date(2026, 2, 16) in h  # Presidents' Day
    assert date(2026, 4, 3) in h  # Good Friday
    assert date(2026, 5, 25) in h  # Memorial Day
    assert date(2026, 6, 19) in h  # Juneteenth (Fri)
    assert date(2026, 7, 4) not in h  # Jul 4 is Saturday, not observed
    assert date(2026, 7, 3) not in h  # Fri before Sat Jul 4 -- NYSE does NOT observe, market open
    assert date(2026, 9, 7) in h  # Labor Day
    assert date(2026, 11, 26) in h  # Thanksgiving
    assert date(2026, 12, 25) in h  # Christmas (Fri)


def test_market_open_during_trading_hours():
    from datetime import datetime

    wed = datetime(2026, 7, 15, 12, 0, tzinfo=ET)  # Wed noon ET
    assert is_market_open(wed) is True


def test_market_closed_before_open():
    from datetime import datetime

    wed = datetime(2026, 7, 15, 9, 0, tzinfo=ET)  # Wed 9:00 AM ET
    assert is_market_open(wed) is False


def test_market_closed_after_close():
    from datetime import datetime

    wed = datetime(2026, 7, 15, 16, 30, tzinfo=ET)  # Wed 4:30 PM ET
    assert is_market_open(wed) is False


def test_market_closed_on_holiday():
    from datetime import datetime

    mlk = datetime(2026, 1, 19, 12, 0, tzinfo=ET)  # MLK Day noon
    assert is_market_open(mlk) is False


def test_market_closed_weekend():
    from datetime import datetime

    sat = datetime(2026, 7, 11, 12, 0, tzinfo=ET)
    assert is_market_open(sat) is False


def test_next_market_open_from_weekend():
    from datetime import datetime

    sat_evening = datetime(2026, 7, 11, 18, 0, tzinfo=ET)
    nxt = next_market_open(sat_evening)
    assert nxt.weekday() == 0  # Monday
    assert nxt.hour == 9 and nxt.minute == 30


def test_next_market_open_from_after_close():
    from datetime import datetime

    wed_close = datetime(2026, 7, 15, 16, 30, tzinfo=ET)
    nxt = next_market_open(wed_close)
    assert nxt.day == 16  # Thursday
    assert nxt.hour == 9 and nxt.minute == 30


def test_next_market_open_skips_holiday():
    from datetime import datetime

    christmas_eve = datetime(2026, 12, 24, 16, 30, tzinfo=ET)  # Thu after close
    nxt = next_market_open(christmas_eve)
    assert nxt.day == 28  # Monday (Dec 25 Fri is Christmas, Dec 26-27 weekend)
    assert nxt.month == 12
