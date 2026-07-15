from __future__ import annotations

import asyncio
import contextlib
import json
from contextlib import asynccontextmanager
from dataclasses import dataclass
from datetime import UTC
from typing import Any

import anyio
from fastapi import FastAPI, HTTPException
from fastapi.responses import FileResponse, StreamingResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel

from robin.auth_robinhood import (
    NeedLogin,
    RobinhoodAuth,
    RobinhoodAuthError,
)
from robin.config import (
    DATA_DIR,
    GEMINI_API_KEY_ENV,
    GEMINI_KEY_FILE,
    GEMINI_MODEL,
    STATIC_DIR,
    ensure_dirs,
    logger,
    setup_logging,
)
from robin.gemini import GeminiClient, GeminiError
from robin.news import NewsSource
from robin.notifier import EventBus
from robin.poller import Poller
from robin.rh_client import RobinhoodClient
from robin.strategy import StrategyEngine


class GeminiKeyManager:
    """Reads the Gemini API key from a plain text file outside the repo.

    Priority: GEMINI_API_KEY env var, then the key file (default
    ~/gemini_free_api_key/key.txt, overridable via GEMINI_KEY_FILE env var).
    The key is never copied into the repo — it stays at whatever location you
    chose. The "Set Gemini Key" UI button writes to that same file so the key
    remains in your folder, not in the project tree.
    """

    def __init__(self) -> None:
        self._key: str | None = self._load()

    def _load(self) -> str | None:
        if GEMINI_API_KEY_ENV:
            return GEMINI_API_KEY_ENV
        if GEMINI_KEY_FILE.exists():
            try:
                return GEMINI_KEY_FILE.read_text().strip()
            except OSError:
                return None
        return None

    def has_key(self) -> bool:
        return bool(self._key)

    def get_key(self) -> str:
        if not self._key:
            raise GeminiError("no Gemini API key set; call /api/auth/gemini")
        return self._key

    def set_key(self, key: str) -> None:
        self._key = key.strip()
        GEMINI_KEY_FILE.parent.mkdir(parents=True, exist_ok=True)
        GEMINI_KEY_FILE.write_text(self._key)

    def clear(self) -> None:
        self._key = None
        with contextlib.suppress(FileNotFoundError):
            GEMINI_KEY_FILE.unlink()


@dataclass
class AppState:
    gemini_keys: GeminiKeyManager
    rh_auth: RobinhoodAuth
    client: RobinhoodClient
    strategy: StrategyEngine
    bus: EventBus
    poller: Poller
    news: NewsSource
    gemini: GeminiClient | None = None


def _make_gemini(state: AppState) -> GeminiClient | None:
    if not state.gemini_keys.has_key():
        return None
    try:
        return GeminiClient(state.gemini_keys.get_key())
    except Exception as e:
        logger.warning("could not init gemini: %s", e)
        return None


@asynccontextmanager
async def lifespan(app: FastAPI):
    setup_logging()
    ensure_dirs()
    gemini_keys = GeminiKeyManager()
    rh_auth = RobinhoodAuth()
    client = RobinhoodClient(auth=rh_auth)
    strategy = StrategyEngine()
    bus = EventBus()
    news = NewsSource()
    state = AppState(
        gemini_keys=gemini_keys,
        rh_auth=rh_auth,
        client=client,
        strategy=strategy,
        bus=bus,
        news=news,
        poller=None,  # type: ignore[arg-type]
    )
    state.gemini = _make_gemini(state)
    try:
        rh_auth.auto_login()
    except NeedLogin:
        logger.info("robinhood: no valid session, running in simulate mode until login")
    except Exception as e:
        logger.warning("robinhood auto-login failed: %s", e)
    state.poller = Poller(client, strategy, state.gemini, bus)
    state.poller.start()
    app.state.robin = state
    try:
        yield
    finally:
        await state.poller.stop()


app = FastAPI(title="Robin", lifespan=lifespan)
if STATIC_DIR.exists():
    app.mount("/static", StaticFiles(directory=str(STATIC_DIR)), name="static")


def _state(app: FastAPI) -> AppState:
    return app.state.robin


class AskRequest(BaseModel):
    prompt: str


class GeminiKeyRequest(BaseModel):
    api_key: str


class UpdateScriptRequest(BaseModel):
    source: str


@app.get("/")
async def index() -> FileResponse:
    path = STATIC_DIR / "index.html"
    if not path.exists():
        raise HTTPException(404, "index.html not found")
    return FileResponse(str(path))


@app.get("/api/status")
async def status() -> dict[str, Any]:
    state = _state(app)
    return {
        "robinhood_logged_in": state.rh_auth.logged_in,
        "robinhood_has_session": state.rh_auth.has_session(),
        "robinhood_username": state.rh_auth.username,
        "robinhood_mode": state.client.resolve_mode(),
        "gemini_has_key": state.gemini_keys.has_key(),
        "gemini_model": GEMINI_MODEL,
        "gemini_ready": state.gemini is not None,
        "market_open": state.poller.market_open,
        "scripts": len(state.strategy.scripts),
        "last_tick": state.poller.last_tick,
        "last_error": state.poller.last_error,
        "simulate_env": state.client.mode,
    }


@app.get("/api/portfolio")
async def portfolio() -> dict[str, Any]:
    state = _state(app)
    if not state.poller.latest_portfolio:
        p = await anyio.to_thread.run_sync(state.client.get_portfolio)
        return p.to_dict()
    return state.poller.latest_portfolio


@app.get("/api/holdings")
async def holdings() -> list[dict[str, Any]]:
    state = _state(app)
    if not state.poller.latest_portfolio:
        p = await anyio.to_thread.run_sync(state.client.get_portfolio)
        return [h.to_dict() for h in p.holdings]
    return state.poller.latest_portfolio.get("holdings", [])


@app.post("/api/refresh")
async def refresh() -> dict[str, Any]:
    """Manually bust the holdings cache and re-poll immediately.

    Overrides market-hours restrictions so you can pull fresh data right
    after placing a trade, even if the market is closed.
    """
    state = _state(app)
    state.client.refresh_holdings()
    await state.poller.tick()
    return {
        "status": "ok",
        "portfolio": state.poller.latest_portfolio,
        "mode": state.client.resolve_mode(),
    }


@app.get("/api/events")
async def events(limit: int = 50) -> list[dict[str, Any]]:
    return _state(app).bus.recent(limit)


@app.get("/api/scripts")
async def scripts() -> list[dict[str, Any]]:
    return _state(app).strategy.list_scripts()


@app.put("/api/scripts/{name}")
async def update_script(name: str, req: UpdateScriptRequest) -> dict[str, Any]:
    try:
        script = await anyio.to_thread.run_sync(
            _state(app).strategy.update_script, name, req.source
        )
    except Exception as e:
        return {"status": "error", "error": str(e)}
    return {"status": "ok", "name": script.name, "targets": script.targets}


@app.delete("/api/scripts/{name}")
async def delete_script(name: str) -> dict[str, Any]:
    await anyio.to_thread.run_sync(_state(app).strategy.delete_script, name)
    return {"status": "ok"}


@app.post("/api/auth/robinhood")
async def auth_robinhood() -> dict[str, Any]:
    """Trigger Robinhood OAuth browser login flow."""
    state = _state(app)
    try:
        await anyio.to_thread.run_sync(state.rh_auth.login_oauth)
    except RobinhoodAuthError as e:
        raise HTTPException(401, str(e)) from e
    return {"status": "ok", "mode": state.client.resolve_mode()}


@app.post("/api/auth/gemini")
async def auth_gemini(req: GeminiKeyRequest) -> dict[str, Any]:
    state = _state(app)
    state.gemini_keys.set_key(req.api_key)
    state.gemini = _make_gemini(state)
    state.poller.set_gemini(state.gemini)
    return {"status": "ok", "gemini_ready": state.gemini is not None}


@app.post("/api/auth/logout")
async def logout() -> dict[str, Any]:
    """Invalidate all auth: delete the Gemini API key file and clear the
    Robinhood JSON session. You'll need to re-set both to use the app again.
    """
    state = _state(app)
    state.gemini_keys.clear()
    await anyio.to_thread.run_sync(state.rh_auth.logout)
    state.gemini = None
    state.poller.set_gemini(None)
    state.client.refresh_holdings()
    return {
        "status": "ok",
        "gemini_has_key": state.gemini_keys.has_key(),
        "robinhood_logged_in": state.rh_auth.logged_in,
        "mode": state.client.resolve_mode(),
    }


@app.post("/api/strategy/refresh")
async def strategy_refresh() -> dict[str, Any]:
    state = _state(app)
    if state.gemini is None:
        raise HTTPException(409, "Gemini API key not set. Enter it in the UI first.")
    if not state.poller.latest_portfolio:
        p = await anyio.to_thread.run_sync(state.client.get_portfolio)
        state.poller.latest_portfolio = p.to_dict()
    try:
        added = await anyio.to_thread.run_sync(
            state.strategy.generate_with_llm, state.gemini, state.poller.latest_portfolio
        )
    except GeminiError as e:
        raise HTTPException(502, str(e)) from e
    return {"status": "ok", "scripts": added, "total": len(state.strategy.scripts)}


@app.post("/api/llm/ask")
async def llm_ask(req: AskRequest) -> dict[str, Any]:
    state = _state(app)
    if state.gemini is None:
        raise HTTPException(409, "Gemini API key not set. Enter it in the UI first.")
    snapshot = state.poller.latest_portfolio
    if not snapshot:
        p = await anyio.to_thread.run_sync(state.client.get_portfolio)
        snapshot = p.to_dict()
        state.poller.latest_portfolio = snapshot
    try:
        answer = await anyio.to_thread.run_sync(
            state.gemini.ask, req.prompt, snapshot, state.news
        )
    except GeminiError as e:
        raise HTTPException(502, str(e)) from e
    _save_chat(req.prompt, answer)
    return {"answer": answer}


def _save_chat(prompt: str, answer: str) -> None:
    """Append the exchange to data/chat_YYYY_MMDD.txt (one file per day)."""
    from datetime import datetime

    now = datetime.now(UTC)
    ts = now.strftime("%Y-%m-%d %H:%M:%S")
    filename = f"chat_{now.strftime('%Y_%m%d')}.txt"
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    with (DATA_DIR / filename).open("a", encoding="utf-8") as f:
        f.write(f"\n[{ts}] USER:\n{prompt}\n\n[{ts}] GEMINI:\n{answer}\n")


@app.post("/api/chat/clear")
async def chat_clear() -> dict[str, Any]:
    """Clear the Gemini chat session history (starts a fresh conversation)."""
    state = _state(app)
    if state.gemini is None:
        raise HTTPException(409, "Gemini API key not set.")
    state.gemini.clear_chat()
    return {"status": "ok"}


@app.get("/api/stream")
async def stream() -> StreamingResponse:
    state = _state(app)
    q = state.bus.subscribe()

    async def gen():
        try:
            while True:
                try:
                    data = await asyncio.wait_for(q.get(), timeout=15.0)
                    yield f"data: {json.dumps(data)}\n\n"
                except TimeoutError:
                    yield ": keepalive\n\n"
        except asyncio.CancelledError:
            pass
        finally:
            state.bus.unsubscribe(q)

    return StreamingResponse(
        gen(),
        media_type="text/event-stream",
        headers={"Cache-Control": "no-cache", "X-Accel-Buffering": "no"},
    )


def main() -> None:
    import uvicorn

    uvicorn.run("robin.app:app", host="127.0.0.1", port=8888, reload=False)


if __name__ == "__main__":
    main()
