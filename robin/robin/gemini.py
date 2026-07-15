from __future__ import annotations

import json
import re
import time
from typing import Any

from robin.config import GEMINI_MODEL, logger

_MAX_RETRIES = 4
_RETRY_BASE_DELAY = 2.0

_SCRIPT_GEN_SYSTEM = (
    "You are a casual equity strategy engineer. The user is "
    "medium-term, risk averting, and wants maybe one trade per week or less."
    "You can write Python CHECK SCRIPTS that run every poll 5 minutes and flag only situations "
    "that need human attention: hard stop-losses, trailing stop-losses from a "
    "recent high, downside accelerations, sufficient gains, large single-day moves, and positions "
    "that grew dangerously concentrated. Do NOT flag normal small moves."
)

_SCRIPT_GEN_INSTR = (
    "Write one check script per significant holding that needs guarding. Each script must "
    "define a function `check(ctx)` and a module-level list `TARGETS` of the "
    "symbols it applies to (use [] to apply to every holding). `ctx` is a dict "
    "with keys: symbol, name, quantity, avg_cost, current_price, prev_close, "
    "day_high, day_low, market_value, unrealized_pl, unrealized_pl_pct, "
    "equity_pct, history (list of recent close floats, oldest->newest), as_of, "
    "portfolio (dict with total_market_value, total_cost, "
    "total_unrealized_pl, total_unrealized_pl_pct), and state (a persistent dict "
    "that survives across polls -- use it to track things like high_water_mark "
    "for trailing stops; read from ctx['state'] and write back to it, e.g. "
    "ctx['state']['high'] = max(ctx['state'].get('high', 0), ctx['current_price'])). "
    "`check` must return None when all is well, or a dict with keys: kind (e.g. "
    "'stop_loss','trailing_stop','concentration','large_move'), severity ('low'|"
    "'medium'|'high'), message (short human sentence), and any extra data. Only "
    "available globals: math, statistics, datetime, timedelta, Event, and common "
    "safe builtins (no file or network access). Return STRICT JSON: "
    '{"scripts":[{"name":str,"targets":[str],"source":str}]}. '
    "Keep each script small and defensive."
)

_REASON_SYSTEM = (
    "You are a casual medium-term investing assistant. An automated trigger "
    "fired and the user needs a brief, calm assessment. Do not panic. Prefer hold "
    "or review over selling unless capital is genuinely at risk. "
    "Be succinct. One or two sentences max."
)

_ASK_SYSTEM = (
    "You are a medium-term investing assistant for a US-only equity "
    "portfolio. The portfolio snapshot (holdings, prices, gain/loss) is "
    "provided. You can request news for specific stocks by "
    "returning a JSON response with a 'news_requests' field listing ticker "
    "symbols. If you need news, return ONLY the JSON and the system will fetch "
    "news and ask you again. If you have enough information to answer, respond "
    "normally. Be succinct. "
    "Strategy suggestions must be short bullet points: stock name, condition met, "
    "and action. Example: 'NVDL: down 12% from high — review stop level'. "
    "Prefer hold or review over selling."
)

_ASK_NEWS_INSTR = (
    "If you need recent news to answer the user's question, return STRICT JSON: "
    '{"news_requests":["AAPL","MSFT",...]}. List 1-4 ticker symbols you want '
    "news for. The system will fetch public news headlines for those stocks and "
    "pass them back to you. If you don't need news, just answer the question "
    "directly."
)


class GeminiError(Exception):
    pass


def _strip_json(text: str) -> str:
    text = text.strip()
    if text.startswith("```"):
        text = re.sub(r"^```(?:json)?\s*", "", text)
        text = re.sub(r"\s*```$", "", text)
    start = text.find("{")
    end = text.rfind("}")
    if start != -1 and end != -1 and end > start:
        text = text[start : end + 1]
    return text


def _try_parse_news_request(text: str) -> list[str] | None:
    """Check if Gemini's response is a news request JSON. Returns tickers or None."""
    try:
        data = json.loads(_strip_json(text))
    except json.JSONDecodeError:
        return None
    tickers = data.get("news_requests")
    if isinstance(tickers, list) and all(isinstance(t, str) for t in tickers):
        return [t.upper() for t in tickers[:4]]
    return None


class GeminiClient:
    _PORTFOLIO_REFRESH_INTERVAL = 10
    _MAX_HISTORY = 10

    def __init__(self, api_key: str) -> None:
        from google import genai

        self._genai = genai
        self.client = genai.Client(api_key=api_key)
        self.model = GEMINI_MODEL
        self._chat = self._create_chat()
        self._msgs_since_portfolio = self._PORTFOLIO_REFRESH_INTERVAL

    def _create_chat(self, history: list[Any] | None = None) -> Any:
        return self.client.chats.create(
            model=self.model,
            config=self._genai.types.GenerateContentConfig(
                system_instruction=_ASK_SYSTEM,
                temperature=0.3,
            ),
            history=history,
        )

    def _trim_chat(self) -> None:
        try:
            history = self._chat.get_history()
        except Exception:
            return
        if len(history) <= self._MAX_HISTORY:
            return
        trimmed = history[-self._MAX_HISTORY :]
        self._chat = self._create_chat(trimmed)
        logger.info("trimmed chat history to last %d messages", len(trimmed))

    def clear_chat(self) -> None:
        self._chat = self._create_chat()
        self._msgs_since_portfolio = self._PORTFOLIO_REFRESH_INTERVAL
        logger.info("gemini chat cleared")

    def _generate(
        self,
        contents: str,
        *,
        system: str,
        json_out: bool,
    ) -> str:
        config_kwargs: dict[str, Any] = {"temperature": 0.2}
        if json_out:
            config_kwargs["response_mime_type"] = "application/json"
        config = self._genai.types.GenerateContentConfig(system_instruction=system, **config_kwargs)
        last_exc: Exception | None = None
        for attempt in range(_MAX_RETRIES):
            try:
                resp = self.client.models.generate_content(
                    model=self.model, contents=contents, config=config
                )
                return resp.text or ""
            except Exception as e:
                last_exc = e
                msg = str(e).lower()
                if "429" in msg or "resource_exhausted" in msg or "quota" in msg:
                    delay = _RETRY_BASE_DELAY * (2 ** attempt)
                    logger.warning(
                        "gemini rate-limited (attempt %d/%d); retrying in %.0fs",
                        attempt + 1, _MAX_RETRIES, delay,
                    )
                    time.sleep(delay)
                    continue
                raise GeminiError(f"gemini request failed: {e}") from e
        raise GeminiError(
            f"gemini rate-limited after {_MAX_RETRIES} retries (free tier quota). "
            f"Wait a minute and try again. Last error: {last_exc}"
        ) from last_exc

    def generate_check_scripts(self, portfolio: dict[str, Any]) -> list[dict[str, Any]]:
        lines = []
        for h in portfolio.get("holdings", []):
            lines.append(
                f"- {h['symbol']} ({h['name']}): qty={h['quantity']}, "
                f"avg_cost={h['avg_cost']}, current_price={h['current_price']}, "
                f"unrealized_pl_pct={h['unrealized_pl_pct']}%, "
                f"equity_pct={h['equity_pct']}%"
            )
        summary = "\n".join(lines) or "- (no holdings)"
        prompt = (
            f"Current portfolio (total market value {portfolio.get('total_market_value')}, "
            f"total unrealized P/L {portfolio.get('total_unrealized_pl')} "
            f"({portfolio.get('total_unrealized_pl_pct')}%)):\n{summary}\n\n"
            f"{_SCRIPT_GEN_INSTR}"
        )
        raw = self._generate(prompt, system=_SCRIPT_GEN_SYSTEM, json_out=True)
        try:
            data = json.loads(_strip_json(raw))
        except json.JSONDecodeError as e:
            raise GeminiError(f"could not parse scripts JSON: {e}\nraw: {raw}") from e
        scripts = data.get("scripts", [])
        if not isinstance(scripts, list):
            raise GeminiError("scripts JSON missing 'scripts' list")
        return scripts

    def reason_on_event(self, event: dict[str, Any], ctx: dict[str, Any]) -> dict[str, Any]:
        prompt = (
            f"An automated trigger fired for {ctx.get('symbol')}.\n"
            f"Trigger: kind={event.get('kind')}, severity={event.get('severity')}, "
            f"message={event.get('message')}\n"
            f"Position: qty={ctx.get('quantity')}, avg_cost={ctx.get('avg_cost')}, "
            f"current_price={ctx.get('current_price')}, "
            f"unrealized_pl_pct={ctx.get('unrealized_pl_pct')}%, "
            f"equity_pct={ctx.get('equity_pct')}%\n"
            f"Recent closes: {ctx.get('history')}\n\n"
            "Return STRICT JSON: "
            '{"severity":"low|medium|high","action":"hold|review|sell|rebalance",'
            '"message":"one or two sentences"}'
        )
        raw = self._generate(prompt, system=_REASON_SYSTEM, json_out=True)
        try:
            return json.loads(_strip_json(raw))
        except json.JSONDecodeError:
            return {
                "severity": event.get("severity", "low"),
                "action": "review",
                "message": raw[:300],
            }

    def _send_chat(self, message: str) -> str:
        """Send a message to the chat session with retry logic."""
        last_exc: Exception | None = None
        for attempt in range(_MAX_RETRIES):
            try:
                resp = self._chat.send_message(message)
                self._trim_chat()
                return resp.text or ""
            except Exception as e:
                last_exc = e
                msg = str(e).lower()
                if "429" in msg or "resource_exhausted" in msg or "quota" in msg:
                    delay = _RETRY_BASE_DELAY * (2 ** attempt)
                    logger.warning(
                        "gemini rate-limited (attempt %d/%d); retrying in %.0fs",
                        attempt + 1, _MAX_RETRIES, delay,
                    )
                    time.sleep(delay)
                    continue
                raise GeminiError(f"gemini request failed: {e}") from e
        raise GeminiError(
            f"gemini rate-limited after {_MAX_RETRIES} retries (free tier quota). "
            f"Wait a minute and try again. Last error: {last_exc}"
        ) from last_exc

    def ask(self, prompt: str, portfolio: dict[str, Any], news_source: Any | None = None) -> str:
        """Two-turn flow: send the question, if Gemini requests news for specific
        stocks, fetch it from public sources and send back for a final answer.
        """
        if self._msgs_since_portfolio >= self._PORTFOLIO_REFRESH_INTERVAL:
            summary = json.dumps(portfolio, default=str)
            full = f"Portfolio snapshot (holdings, prices, gain/loss):\n{summary}\n\n{_ASK_NEWS_INSTR}\n\nUser question: {prompt}"
            self._msgs_since_portfolio = 0
        else:
            full = f"{_ASK_NEWS_INSTR}\n\nUser question: {prompt}"
        self._msgs_since_portfolio += 1

        resp1 = self._send_chat(full)

        if news_source is None:
            return resp1

        tickers = _try_parse_news_request(resp1)
        if tickers is None:
            return resp1

        logger.info("gemini requested news for: %s", tickers)
        general = news_source.general_summary()
        stock_news = news_source.stocks_summary(tickers)
        news_msg = f"Here are the latest news headlines:\n\n{general}\n\n{stock_news}\n\nPlease answer the user's question now with this news context."
        resp2 = self._send_chat(news_msg)
        return resp2
