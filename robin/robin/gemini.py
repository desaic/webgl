from __future__ import annotations

import json
import re
import time
from typing import Any

from robin.config import GEMINI_MODEL, logger

_MAX_RETRIES = 4
_RETRY_BASE_DELAY = 2.0

_SCRIPT_GEN_SYSTEM = (
    "You are a conservative, low-frequency equity strategy engineer. The user is "
    "long-term, capital-preservation focused, and wants minimal trading. "
    "You write Python CHECK SCRIPTS that run every poll and flag only situations "
    "that need human attention: hard stop-losses, trailing stop-losses from a "
    "recent high, downside accelerations, large single-day moves, and positions "
    "that grew dangerously concentrated. Do NOT flag normal small moves."
)

_SCRIPT_GEN_INSTR = (
    "Write one check script per holding that needs guarding. Each script must "
    "define a function `check(ctx)` and a module-level list `TARGETS` of the "
    "symbols it applies to (use [] to apply to every holding). `ctx` is a dict "
    "with keys: symbol, name, quantity, avg_cost, current_price, prev_close, "
    "day_high, day_low, market_value, unrealized_pl, unrealized_pl_pct, "
    "equity_pct, history (list of recent close floats, oldest->newest), as_of, "
    "and portfolio (dict with total_market_value, total_cost, "
    "total_unrealized_pl, total_unrealized_pl_pct). `check` must return None when "
    "all is well, or a dict with keys: kind (e.g. 'stop_loss','trailing_stop',"
    "'concentration','large_move'), severity ('low'|'medium'|'high'), message "
    "(short human sentence), and any extra data. Only available globals: math, "
    "statistics, datetime, timedelta, Event, and common safe builtins (no file or "
    "network access). Return STRICT JSON: "
    '{"scripts":[{"name":str,"targets":[str],"source":str}]}. '
    "Keep each script small and defensive."
)

_REASON_SYSTEM = (
    "You are a conservative long-term investing assistant. An automated trigger "
    "fired and the user needs a brief, calm assessment. Do not panic. Prefer hold "
    "or review over selling unless capital is genuinely at risk. "
    "You have access to Google Search to look up recent news about the stock. "
    "Use it to give context for why a trigger may have fired (earnings, FDA, "
    "macro events, sector moves). Always cite the news source briefly."
)

_ASK_SYSTEM = (
    "You are a conservative long-term investing assistant for a US-only equity "
    "portfolio. The user's portfolio snapshot (holdings, prices, gain/loss) is "
    "provided with each question. You have Google Search access to look up "
    "latest news, earnings, analyst ratings, and market context. "
    "Keep answers concise and actionable. Prefer hold or review over selling."
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


class GeminiClient:
    _PORTFOLIO_REFRESH_INTERVAL = 10
    _MAX_HISTORY = 10

    def __init__(self, api_key: str) -> None:
        from google import genai

        self._genai = genai
        self.client = genai.Client(api_key=api_key)
        self.model = GEMINI_MODEL
        self._search_tool = genai.types.Tool(google_search=genai.types.GoogleSearch())
        self._search_enabled = False
        self._chat = self._create_chat()
        self._msgs_since_portfolio = self._PORTFOLIO_REFRESH_INTERVAL

    def _create_chat(self, history: list[Any] | None = None) -> Any:
        config_kwargs: dict[str, Any] = {
            "system_instruction": _ASK_SYSTEM,
            "temperature": 0.3,
        }
        if self._search_enabled:
            config_kwargs["tools"] = [self._search_tool]
        return self.client.chats.create(
            model=self.model,
            config=self._genai.types.GenerateContentConfig(**config_kwargs),
            history=history,
        )

    def _trim_chat(self) -> None:
        """Recreate the chat with only the last _MAX_HISTORY messages to cap
        token usage and avoid hitting free-tier rate limits.
        """
        try:
            history = self._chat.get_history()
        except Exception:
            return
        if len(history) <= self._MAX_HISTORY:
            return
        trimmed = history[-self._MAX_HISTORY :]
        self._chat = self._create_chat(trimmed)
        logger.info("trimmed chat history to last %d messages", len(trimmed))

    def set_search_enabled(self, enabled: bool) -> None:
        if enabled != self._search_enabled:
            self._search_enabled = enabled
            try:
                history = self._chat.get_history()
            except Exception:
                history = None
            self._chat = self._create_chat(history)
            self._msgs_since_portfolio = self._PORTFOLIO_REFRESH_INTERVAL
            logger.info("gemini google search %s", "enabled" if enabled else "disabled")

    def clear_chat(self) -> None:
        """Start a fresh chat session with no history."""
        self._chat = self._create_chat()
        self._msgs_since_portfolio = self._PORTFOLIO_REFRESH_INTERVAL
        logger.info("gemini chat cleared")

    def _generate(
        self,
        contents: str,
        *,
        system: str,
        json_out: bool,
        search: bool = False,
    ) -> str:
        config_kwargs: dict[str, Any] = {"temperature": 0.2}
        if json_out and not search:
            config_kwargs["response_mime_type"] = "application/json"
        if search:
            config_kwargs["tools"] = [self._search_tool]
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
            "Search for recent news about this stock to explain the trigger, "
            "then return STRICT JSON: "
            '{"severity":"low|medium|high","action":"hold|review|sell|rebalance",'
            '"message":"one or two sentences with news context"}'
        )
        try:
            raw = self._generate(prompt, system=_REASON_SYSTEM, json_out=True, search=self._search_enabled)
        except GeminiError:
            if self._search_enabled:
                logger.warning("reason_on_event: disabling google search and retrying")
                self.set_search_enabled(False)
                raw = self._generate(prompt, system=_REASON_SYSTEM, json_out=True, search=False)
            else:
                raise
        try:
            return json.loads(_strip_json(raw))
        except json.JSONDecodeError:
            return {
                "severity": event.get("severity", "low"),
                "action": "review",
                "message": raw[:300],
            }

    def ask(self, prompt: str, portfolio: dict[str, Any]) -> str:
        if self._msgs_since_portfolio >= self._PORTFOLIO_REFRESH_INTERVAL:
            summary = json.dumps(portfolio, default=str)
            full = f"Portfolio snapshot (holdings, prices, gain/loss):\n{summary}\n\nUser question: {prompt}"
            self._msgs_since_portfolio = 0
        else:
            full = prompt
        self._msgs_since_portfolio += 1
        last_exc: Exception | None = None
        for attempt in range(_MAX_RETRIES):
            try:
                resp = self._chat.send_message(full)
                self._trim_chat()
                return resp.text or ""
            except Exception as e:
                last_exc = e
                msg = str(e).lower()
                if ("429" in msg or "resource_exhausted" in msg) and self._search_enabled:
                    logger.warning("gemini rate-limited; disabling google search and retrying without it")
                    self.set_search_enabled(False)
                    continue
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
