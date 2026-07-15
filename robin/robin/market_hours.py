from __future__ import annotations

from datetime import date, datetime, timedelta
from zoneinfo import ZoneInfo

ET = ZoneInfo("America/New_York")

MARKET_OPEN_HOUR = 9
MARKET_OPEN_MINUTE = 30
MARKET_CLOSE_HOUR = 16
MARKET_CLOSE_MINUTE = 0


def _easter(year: int) -> date:
    """Computus (Meeus/Jones/Butcher) -> Easter Sunday."""
    a = year % 19
    b = year // 100
    c = year % 100
    d = b // 4
    e = b % 4
    f = (b + 8) // 25
    g = (b - f + 1) // 3
    h = (19 * a + b - d - g + 15) % 30
    i = c // 4
    k = c % 4
    el = (32 + 2 * e + 2 * i - h - k) % 7
    m = (a + 11 * h + 22 * el) // 451
    month = (h + el - 7 * m + 114) // 31
    day = ((h + el - 7 * m + 114) % 31) + 1
    return date(year, month, day)


def _observed(holiday: date) -> date | None:
    """NYSE observance rule: Saturday holidays are NOT observed (market is
    closed weekends anyway). Sunday holidays are observed on Monday.
    """
    if holiday.weekday() == 5:  # Saturday
        return None
    if holiday.weekday() == 6:  # Sunday
        return holiday + timedelta(days=1)
    return holiday


def _nth_weekday(year: int, month: int, weekday: int, n: int) -> date:
    """nth (1-based) weekday of month. weekday: Monday=0."""
    d = date(year, month, 1)
    offset = (weekday - d.weekday()) % 7
    return d + timedelta(days=offset + 7 * (n - 1))


def _last_weekday(year: int, month: int, weekday: int) -> date:
    """Last weekday of month. weekday: Monday=0."""
    d = date(year, 12, 31) if month == 12 else date(year, month + 1, 1) - timedelta(days=1)
    offset = (d.weekday() - weekday) % 7
    return d - timedelta(days=offset)


def nyse_holidays(year: int) -> set[date]:
    """All NYSE closed dates for a year (observed rules applied)."""
    raw = [
        date(year, 1, 1),  # New Year's Day
        _nth_weekday(year, 1, 0, 3),  # MLK Day
        _nth_weekday(year, 2, 0, 3),  # Washington's Birthday
        _easter(year) - timedelta(days=2),  # Good Friday
        _last_weekday(year, 5, 0),  # Memorial Day
        date(year, 6, 19),  # Juneteenth
        date(year, 7, 4),  # Independence Day
        _nth_weekday(year, 9, 0, 1),  # Labor Day
        _nth_weekday(year, 11, 3, 4),  # Thanksgiving
        date(year, 12, 25),  # Christmas
    ]
    holidays: set[date] = set()
    for h in raw:
        observed = _observed(h)
        if observed is not None:
            holidays.add(observed)
    return holidays


_holiday_cache: dict[int, set[date]] = {}


def _holidays_for(year: int) -> set[date]:
    if year not in _holiday_cache:
        _holiday_cache[year] = nyse_holidays(year)
    return _holiday_cache[year]


def is_holiday(d: date) -> bool:
    return (
        d in _holidays_for(d.year)
        or d in _holidays_for(d.year - 1)
        or d in _holidays_for(d.year + 1)
    )


def is_trading_day(d: date) -> bool:
    return d.weekday() < 5 and not is_holiday(d)


def is_market_open(now: datetime | None = None) -> bool:
    """True if the US market is currently in a regular trading session."""
    now_et = (now or datetime.now(ET)).astimezone(ET)
    if not is_trading_day(now_et.date()):
        return False
    market_open = now_et.replace(
        hour=MARKET_OPEN_HOUR, minute=MARKET_OPEN_MINUTE, second=0, microsecond=0
    )
    market_close = now_et.replace(
        hour=MARKET_CLOSE_HOUR, minute=MARKET_CLOSE_MINUTE, second=0, microsecond=0
    )
    return market_open <= now_et < market_close


def next_market_open(now: datetime | None = None) -> datetime:
    """Next time the market opens (ET-aware). Used to sleep until then."""
    now_et = (now or datetime.now(ET)).astimezone(ET)
    d = now_et.date()
    if is_trading_day(d):
        today_open = now_et.replace(
            hour=MARKET_OPEN_HOUR, minute=MARKET_OPEN_MINUTE, second=0, microsecond=0
        )
        if now_et < today_open:
            return today_open
    for _ in range(15):
        d = d + timedelta(days=1)
        if is_trading_day(d):
            return datetime(
                d.year, d.month, d.day, MARKET_OPEN_HOUR, MARKET_OPEN_MINUTE, 0, tzinfo=ET
            )
    raise RuntimeError("no market open found within 15 days")


def seconds_until_open(now: datetime | None = None) -> float:
    target = next_market_open(now)
    now_utc = (now or datetime.now(ET)).astimezone(ET)
    return max(1.0, (target - now_utc).total_seconds())
