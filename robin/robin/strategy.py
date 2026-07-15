from __future__ import annotations

import contextlib
import datetime as _dt
import json
import math
import statistics
import threading
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from robin.config import DATA_DIR, SCRIPTS_DIR, logger
from robin.notifier import Event

_STATE_DIR = DATA_DIR / "script_state"

_SAFE_BUILTINS: dict[str, Any] = {
    "abs": abs,
    "round": round,
    "min": min,
    "max": max,
    "len": len,
    "sum": sum,
    "sorted": sorted,
    "range": range,
    "float": float,
    "int": int,
    "str": str,
    "bool": bool,
    "dict": dict,
    "list": list,
    "tuple": tuple,
    "set": set,
    "frozenset": frozenset,
    "zip": zip,
    "enumerate": enumerate,
    "any": any,
    "all": all,
    "map": map,
    "filter": filter,
    "reversed": reversed,
    "print": print,
    "True": True,
    "False": False,
    "None": None,
    "ValueError": ValueError,
    "TypeError": TypeError,
    "ZeroDivisionError": ZeroDivisionError,
    "ArithmeticError": ArithmeticError,
    "Exception": Exception,
}


def _safe_globals() -> dict[str, Any]:
    return {
        "__builtins__": _SAFE_BUILTINS,
        "math": math,
        "statistics": statistics,
        "datetime": _dt,
        "timedelta": _dt.timedelta,
        "Event": Event,
    }


@dataclass
class CheckScript:
    name: str
    path: Path
    source: str
    targets: list[str]
    check: Any

    def applies_to(self, symbol: str) -> bool:
        if not self.targets:
            return True
        return symbol.upper() in [t.upper() for t in self.targets]

    def _state_path(self) -> Path:
        return _STATE_DIR / f"{self.name}.json"

    def load_state(self) -> dict[str, Any]:
        path = self._state_path()
        if not path.exists():
            return {}
        try:
            return json.loads(path.read_text())
        except (json.JSONDecodeError, OSError):
            return {}

    def save_state(self, state: dict[str, Any]) -> None:
        _STATE_DIR.mkdir(parents=True, exist_ok=True)
        self._state_path().write_text(json.dumps(state, indent=2, default=str))

    def run(self, ctx: dict[str, Any]) -> Event | None:
        try:
            result = self.check(ctx)
        except Exception as e:
            return Event(
                symbol=ctx.get("symbol", "?"),
                kind="script_error",
                severity="low",
                message=f"{self.name} raised {type(e).__name__}: {e}",
                script=self.name,
            )
        if result is None:
            return None
        if isinstance(result, Event):
            if not result.script:
                result.script = self.name
            return result
        if isinstance(result, dict):
            return Event(
                symbol=str(result.get("symbol", ctx.get("symbol", "?"))),
                kind=str(result.get("kind", "signal")),
                severity=str(result.get("severity", "low")),
                message=str(result.get("message", "triggered")),
                data={k: v for k, v in result.items() if k not in {"kind", "severity", "message", "symbol"}},
                script=self.name,
            )
        return Event(
            symbol=ctx.get("symbol", "?"),
            kind="signal",
            severity="low",
            message=f"{self.name} returned unexpected type {type(result).__name__}",
            script=self.name,
        )


class StrategyEngine:
    """Loads LLM-authored check scripts from disk, runs them each poll inside a
    restricted namespace, and emits Events. Scripts are plain .py files in
    scripts/ defining `TARGETS` and `check(ctx)`.

    Security: exec of model-generated code is inherently risky. We restrict
    builtins (no open/exec/eval/__import__/getattr) and expose only math,
    statistics, and datetime. This is defense-in-depth, not a full sandbox; the
    scripts directory must be treated as trusted local state.
    """

    def __init__(self, scripts_dir: Path = SCRIPTS_DIR) -> None:
        self.scripts_dir = scripts_dir
        self.scripts: list[CheckScript] = []
        self._lock = threading.Lock()
        self.load_scripts()

    def load_scripts(self) -> list[CheckScript]:
        with self._lock:
            loaded: list[CheckScript] = []
            if self.scripts_dir.exists():
                for path in sorted(self.scripts_dir.glob("*.py")):
                    if path.name.startswith("_"):
                        continue
                    try:
                        loaded.append(self._compile_file(path))
                    except Exception as e:
                        logger.warning("failed to load script %s: %s", path.name, e)
            self.scripts = loaded
            return loaded

    def _compile_file(self, path: Path) -> CheckScript:
        source = path.read_text()
        return self._compile_source(path.stem, source, path)

    def _compile_source(self, name: str, source: str, path: Path) -> CheckScript:
        code = compile(source, str(path), "exec")
        namespace: dict[str, Any] = {}
        exec(code, _safe_globals(), namespace)
        check = namespace.get("check")
        if not callable(check):
            raise ValueError("script must define a callable check(ctx)")
        targets = namespace.get("TARGETS", [])
        if not isinstance(targets, list):
            targets = []
        return CheckScript(name=name, path=path, source=source, targets=[str(t) for t in targets], check=check)

    def add_script(self, name: str, source: str, targets: list[str] | None = None) -> CheckScript:
        self.scripts_dir.mkdir(parents=True, exist_ok=True)
        safe_name = "".join(c if c.isalnum() or c in "-_" else "_" for c in name)[:60] or "rule"
        path = self.scripts_dir / f"{safe_name}.py"
        header = ""
        if targets is not None:
            header = f"TARGETS = {targets!r}\n"
        path.write_text(header + source)
        script = self._compile_source(safe_name, path.read_text(), path)
        with self._lock:
            self._upsert_script(script)
        return script

    def update_script(self, name: str, source: str) -> CheckScript:
        """Recompile and save a script with edited source. Creates the script
        if it doesn't exist yet. Raises the compile error so the caller can
        show it to the user. Preserves list ordering.
        """
        safe_name = "".join(c if c.isalnum() or c in "-_" else "_" for c in name)[:60] or "rule"
        path = self.scripts_dir / f"{safe_name}.py"
        path.write_text(source)
        script = self._compile_source(safe_name, source, path)
        with self._lock:
            self._upsert_script(script)
        return script

    def _upsert_script(self, script: CheckScript) -> None:
        """Replace in place if exists, append if new. Preserves ordering."""
        for i, s in enumerate(self.scripts):
            if s.name == script.name:
                self.scripts[i] = script
                return
        self.scripts.append(script)

    def delete_script(self, name: str) -> None:
        safe_name = "".join(c if c.isalnum() or c in "-_" else "_" for c in name)[:60] or "rule"
        path = self.scripts_dir / f"{safe_name}.py"
        with contextlib.suppress(FileNotFoundError):
            path.unlink()
        with self._lock:
            self.scripts = [s for s in self.scripts if s.name != safe_name]

    def list_scripts(self) -> list[dict[str, Any]]:
        return [
            {"name": s.name, "targets": s.targets, "source": s.source, "path": str(s.path)}
            for s in self.scripts
        ]

    def _ctx_for(
        self,
        holding: dict[str, Any],
        portfolio: dict[str, Any],
        history: list[float],
        state: dict[str, Any] | None = None,
    ) -> dict[str, Any]:
        return {
            "symbol": holding["symbol"],
            "name": holding.get("name", holding["symbol"]),
            "quantity": holding["quantity"],
            "avg_cost": holding["avg_cost"],
            "current_price": holding["current_price"],
            "prev_close": holding.get("prev_close", holding["current_price"]),
            "day_high": holding.get("day_high", holding["current_price"]),
            "day_low": holding.get("day_low", holding["current_price"]),
            "market_value": holding["market_value"],
            "unrealized_pl": holding["unrealized_pl"],
            "unrealized_pl_pct": holding["unrealized_pl_pct"],
            "equity_pct": holding.get("equity_pct", 0.0),
            "history": history,
            "as_of": portfolio.get("updated_at", ""),
            "portfolio": {
                "total_market_value": portfolio.get("total_market_value", 0.0),
                "total_cost": portfolio.get("total_cost", 0.0),
                "total_unrealized_pl": portfolio.get("total_unrealized_pl", 0.0),
                "total_unrealized_pl_pct": portfolio.get("total_unrealized_pl_pct", 0.0),
            },
            "state": state if state is not None else {},
        }

    def evaluate(self, portfolio: dict[str, Any], histories: dict[str, list[float]] | None = None) -> list[Event]:
        histories = histories or {}
        events: list[Event] = []
        if not self.scripts:
            return events
        for holding in portfolio.get("holdings", []):
            symbol = holding["symbol"]
            for script in self.scripts:
                if not script.applies_to(symbol):
                    continue
                all_state = script.load_state()
                sym_state = all_state.get(symbol, {})
                state_copy = dict(sym_state)
                ctx = self._ctx_for(holding, portfolio, histories.get(symbol, []), state_copy)
                event = script.run(ctx)
                new_state = ctx.get("state", state_copy)
                if new_state != sym_state:
                    all_state[symbol] = new_state
                    script.save_state(all_state)
                if event is not None:
                    events.append(event)
        return events

    def generate_with_llm(self, gemini: Any, portfolio: dict[str, Any]) -> list[dict[str, Any]]:
        specs = gemini.generate_check_scripts(portfolio)
        added: list[dict[str, Any]] = []
        for spec in specs:
            try:
                name = str(spec.get("name", "rule"))
                targets = spec.get("targets", [])
                source = str(spec.get("source", ""))
                if "def check" not in source:
                    logger.warning("skipping script %s: no check() function", name)
                    continue
                script = self.add_script(name, source, targets)
                added.append({"name": script.name, "targets": script.targets})
            except Exception as e:
                logger.warning("could not add script %s: %s", spec.get("name"), e)
        return added
