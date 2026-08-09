"""Robinhood MCP client — official OAuth + JSON-RPC to agent.robinhood.com.

Replaces robin_stocks (unofficial reverse-engineered API) with Robinhood's
official MCP server. Uses OAuth 2.1 Authorization Code + PKCE flow for
authentication (browser-based, one-time login), and speaks JSON-RPC over
HTTP to call MCP tools like get_portfolio, get_equity_positions, etc.

No external dependencies — Python stdlib only.
"""

from __future__ import annotations

import base64
import contextlib
import hashlib
import json
import re
import secrets
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
import webbrowser
from http.server import BaseHTTPRequestHandler, HTTPServer
from typing import Any

from robin.config import DATA_DIR, logger

UPSTREAM_URL = "https://agent.robinhood.com/mcp/trading"
REDIRECT_HOST = "localhost"
REDIRECT_PORT = 8765
REDIRECT_PATH = "/callback"
REDIRECT_URI = f"http://{REDIRECT_HOST}:{REDIRECT_PORT}{REDIRECT_PATH}"
REFRESH_MARGIN_S = 60
DEFAULT_TIMEOUT_S = 30
PROTOCOL_VERSION = "2025-06-18"
USER_AGENT = "robin-app/1.0 (MCP client)"

CREDS_PATH = DATA_DIR / "rh_mcp_creds.json"

_discovered: dict[str, Any] | None = None
_discovered_lock = threading.Lock()


class MCPError(Exception):
    pass


class AuthNeededError(MCPError):
    pass


def _b64url(data: bytes) -> str:
    return base64.urlsafe_b64encode(data).rstrip(b"=").decode("ascii")


def _gen_pkce() -> tuple[str, str]:
    verifier = _b64url(secrets.token_bytes(32))
    challenge = _b64url(hashlib.sha256(verifier.encode("ascii")).digest())
    return verifier, challenge


def _http_post_json(
    url: str, data: dict[str, Any], headers: dict[str, str] | None = None
) -> dict[str, Any]:
    body = json.dumps(data).encode("utf-8")
    h = {"Content-Type": "application/json", "Accept": "application/json", "User-Agent": USER_AGENT}
    if headers:
        h.update(headers)
    req = urllib.request.Request(url, data=body, headers=h, method="POST")
    try:
        with urllib.request.urlopen(req, timeout=DEFAULT_TIMEOUT_S) as resp:
            return json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as e:
        err_body = e.read().decode("utf-8", "replace")
        raise MCPError(f"POST to {url} failed: HTTP {e.code}: {err_body[:500]}") from e
    except urllib.error.URLError as e:
        raise MCPError(f"POST to {url} failed: {e.reason}") from e


def _http_get_json(url: str, headers: dict[str, str] | None = None) -> dict[str, Any]:
    h = {"Accept": "application/json", "User-Agent": USER_AGENT}
    if headers:
        h.update(headers)
    req = urllib.request.Request(url, headers=h, method="GET")
    try:
        with urllib.request.urlopen(req, timeout=DEFAULT_TIMEOUT_S) as resp:
            return json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as e:
        err_body = e.read().decode("utf-8", "replace")
        raise MCPError(f"GET to {url} failed: HTTP {e.code}: {err_body[:500]}") from e
    except urllib.error.URLError as e:
        raise MCPError(f"GET to {url} failed: {e.reason}") from e


def _discover_oauth() -> dict[str, Any]:
    """Discover OAuth endpoints via the MCP 401 → metadata → DCR chain."""
    global _discovered
    with _discovered_lock:
        if _discovered is not None:
            return _discovered

    req = urllib.request.Request(
        UPSTREAM_URL,
        data=b'{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}',
        headers={"Content-Type": "application/json", "User-Agent": USER_AGENT},
        method="POST",
    )
    try:
        urllib.request.urlopen(req, timeout=DEFAULT_TIMEOUT_S)
    except urllib.error.HTTPError as e:
        if e.code != 401:
            raise MCPError(f"upstream returned {e.code}, expected 401") from e
        www_auth = e.headers.get("www-authenticate", "")
    else:
        raise MCPError("upstream did not return 401 during discovery")

    m = re.search(r'resource_metadata="([^"]+)"', www_auth)
    if not m:
        raise MCPError(f"no resource_metadata in www-authenticate: {www_auth!r}")
    resource_meta = _http_get_json(m.group(1))
    auth_servers = resource_meta.get("authorization_servers", [])
    if not auth_servers:
        raise MCPError("no authorization_servers in resource metadata")
    auth_server = auth_servers[0]
    scopes = resource_meta.get("scopes_supported", ["internal"])

    parsed = urllib.parse.urlparse(auth_server)
    well_known = f"{parsed.scheme}://{parsed.netloc}/.well-known/oauth-authorization-server"
    as_meta = _http_get_json(well_known)

    cfg = {
        "client_id": None,
        "authorization_endpoint": as_meta.get("authorization_endpoint"),
        "token_endpoint": as_meta.get("token_endpoint"),
        "registration_endpoint": as_meta.get("registration_endpoint"),
        "scopes": scopes[0] if isinstance(scopes, list) else scopes,
    }
    if not (cfg["authorization_endpoint"] and cfg["token_endpoint"]):
        raise MCPError(f"incomplete authorization-server metadata: {as_meta}")

    if cfg["registration_endpoint"]:
        dcr = _http_post_json(cfg["registration_endpoint"], {})
        cfg["client_id"] = dcr.get("client_id")
    if not cfg["client_id"]:
        raise MCPError("could not obtain client_id via dynamic registration")

    _discovered = cfg
    logger.info("mcp oauth discovered: client_id=%s...", cfg["client_id"][:8])
    return cfg


def _save_creds(creds: dict[str, Any]) -> None:
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    tmp = CREDS_PATH.with_suffix(".json.tmp")
    tmp.write_text(json.dumps(creds, indent=2))
    tmp.chmod(0o600)
    import os

    os.replace(tmp, CREDS_PATH)


def _load_creds() -> dict[str, Any]:
    if not CREDS_PATH.exists():
        return {}
    with contextlib.suppress(json.JSONDecodeError, OSError):
        return json.loads(CREDS_PATH.read_text())
    return {}


def _is_token_valid(creds: dict[str, Any]) -> bool:
    exp = creds.get("access_token_expires_at")
    return bool(exp and time.time() < (exp - REFRESH_MARGIN_S))


def _build_creds(token_resp: dict[str, Any]) -> dict[str, Any]:
    now = time.time()
    expires_in = token_resp.get("expires_in", 3600)
    return {
        "access_token": token_resp["access_token"],
        "refresh_token": token_resp.get("refresh_token"),
        "access_token_issued_at": int(now),
        "access_token_expires_at": int(now + expires_in),
        "expires_in": expires_in,
        "token_type": token_resp.get("token_type", "Bearer"),
        "scope": token_resp.get("scope", "internal"),
    }


def _refresh_token() -> dict[str, Any]:
    creds = _load_creds()
    rt = creds.get("refresh_token")
    if not rt:
        raise AuthNeededError("no refresh_token stored — run login")
    oauth = _discover_oauth()
    logger.info("refreshing robinhood mcp access token")
    token_resp = _http_post_json(
        oauth["token_endpoint"],
        {"grant_type": "refresh_token", "refresh_token": rt, "client_id": oauth["client_id"]},
    )
    new_creds = _build_creds(token_resp)
    new_creds["refresh_token"] = token_resp.get("refresh_token", rt)
    _save_creds(new_creds)
    return new_creds


def _get_valid_token() -> str:
    creds = _load_creds()
    if _is_token_valid(creds):
        return creds["access_token"]
    if creds.get("refresh_token"):
        creds = _refresh_token()
        return creds["access_token"]
    raise AuthNeededError("no valid token and no refresh_token")


class _CallbackHandler(BaseHTTPRequestHandler):
    def do_GET(self) -> None:
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path != REDIRECT_PATH:
            self.send_response(404)
            self.end_headers()
            return
        params = urllib.parse.parse_qs(parsed.query)
        self.server.cb_state.received_state = params.get("state", [None])[0]  # type: ignore[attr-defined]
        if "error" in params:
            self.server.cb_state.error = (
                params["error"][0],
                params.get("error_description", [""])[0],
            )  # type: ignore[attr-defined]
        elif "code" in params:
            self.server.cb_state.code = params["code"][0]  # type: ignore[attr-defined]
        else:
            self.server.cb_state.error = ("invalid_request", "no code in callback")  # type: ignore[attr-defined]
        self.server.cb_state.received.set()  # type: ignore[attr-defined]
        self._respond()

    def _respond(self) -> None:
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.end_headers()
        if self.server.cb_state.code:  # type: ignore[attr-defined]
            html = b"<html><body><h2>Robinhood login successful</h2><p>You can close this tab.</p></body></html>"
        else:
            html = b"<html><body><h2>Login failed</h2></body></html>"
        self.wfile.write(html)

    def log_message(self, *a: Any) -> None:
        pass


class _CBState:
    def __init__(self) -> None:
        self.code: str | None = None
        self.error: tuple[str, str] | None = None
        self.received_state: str | None = None
        self.expected_state: str | None = None
        self.received = threading.Event()


def run_browser_login() -> dict[str, Any]:
    """Interactive browser PKCE OAuth flow. Returns creds on success."""
    oauth = _discover_oauth()
    verifier, challenge = _gen_pkce()
    state = secrets.token_urlsafe(16)
    params = {
        "response_type": "code",
        "client_id": oauth["client_id"],
        "redirect_uri": REDIRECT_URI,
        "scope": oauth["scopes"],
        "state": state,
        "code_challenge": challenge,
        "code_challenge_method": "S256",
    }
    auth_url = oauth["authorization_endpoint"] + "?" + urllib.parse.urlencode(params)

    cb_state = _CBState()
    cb_state.expected_state = state
    httpd = HTTPServer((REDIRECT_HOST, REDIRECT_PORT), _CallbackHandler)
    httpd.cb_state = cb_state  # type: ignore[attr-defined]
    httpd.timeout = 1
    thread = threading.Thread(target=httpd.serve_forever, daemon=True)
    thread.start()

    logger.info("opening browser for Robinhood OAuth login")
    with contextlib.suppress(Exception):
        webbrowser.open(auth_url)

    deadline = time.time() + 300
    while not cb_state.received.is_set() and time.time() < deadline:
        time.sleep(0.5)
    httpd.shutdown()
    thread.join(timeout=2)

    if cb_state.received_state != cb_state.expected_state:
        raise MCPError("CSRF check failed: state mismatch")
    if cb_state.error:
        raise MCPError(f"login rejected: {cb_state.error[0]} ({cb_state.error[1]})")
    if not cb_state.code:
        raise MCPError("timed out waiting for browser callback (5 min)")

    token_resp = _http_post_json(
        oauth["token_endpoint"],
        {
            "grant_type": "authorization_code",
            "code": cb_state.code,
            "redirect_uri": REDIRECT_URI,
            "client_id": oauth["client_id"],
            "code_verifier": verifier,
        },
    )
    creds = _build_creds(token_resp)
    _save_creds(creds)
    logger.info("robinhood mcp oauth login successful")
    return creds


def _parse_sse(body: str) -> list[dict[str, Any]]:
    """Parse SSE-framed JSON-RPC responses from Robinhood's MCP server."""
    envelopes: list[dict[str, Any]] = []
    for block in re.split(r"\n\s*\n", body):
        block = block.strip()
        if not block:
            continue
        data_lines: list[str] = []
        for line in block.split("\n"):
            if line.startswith("data:"):
                data_lines.append(line[5:].lstrip())
            elif line.startswith("data"):
                data_lines.append(line[4:].lstrip())
        data = "\n".join(data_lines)
        if data:
            with contextlib.suppress(json.JSONDecodeError):
                envelopes.append(json.loads(data))
    return envelopes


class RobinhoodMCPClient:
    """Direct JSON-RPC client for Robinhood's official MCP server.

    Handles token management, session establishment, and tool calls.
    All methods are synchronous (call from anyio.to_thread.run_sync).
    """

    def __init__(self) -> None:
        self.session_id: str | None = None
        self._initialized = False
        self._id_counter = 0
        self._lock = threading.Lock()

    def _next_id(self) -> int:
        with self._lock:
            self._id_counter += 1
            return self._id_counter

    def _call_upstream(self, envelope: dict[str, Any]) -> list[dict[str, Any]]:
        token = _get_valid_token()
        body = json.dumps(envelope).encode("utf-8")
        headers = {
            "Content-Type": "application/json",
            "Accept": "application/json, text/event-stream",
            "Authorization": f"Bearer {token}",
            "User-Agent": USER_AGENT,
        }
        if self.session_id:
            headers["Mcp-Session-Id"] = self.session_id

        req = urllib.request.Request(UPSTREAM_URL, data=body, headers=headers, method="POST")
        try:
            with urllib.request.urlopen(req, timeout=DEFAULT_TIMEOUT_S) as resp:
                resp_body = resp.read().decode("utf-8", "replace")
                new_session = resp.headers.get("Mcp-Session-Id")
                content_type = resp.headers.get("Content-Type", "")
        except urllib.error.HTTPError as e:
            if e.code == 401:
                creds = _refresh_token()
                return self._retry_with_token(envelope, creds["access_token"])
            raise MCPError(
                f"upstream HTTP {e.code}: {e.read().decode('utf-8', 'replace')[:200]}"
            ) from e
        except urllib.error.URLError as e:
            raise MCPError(f"network error: {e.reason}") from e

        if new_session:
            self.session_id = new_session

        if "text/event-stream" in content_type:
            return _parse_sse(resp_body)
        with contextlib.suppress(json.JSONDecodeError):
            return [json.loads(resp_body)]
        return []

    def _retry_with_token(self, envelope: dict[str, Any], token: str) -> list[dict[str, Any]]:
        body = json.dumps(envelope).encode("utf-8")
        headers = {
            "Content-Type": "application/json",
            "Accept": "application/json, text/event-stream",
            "Authorization": f"Bearer {token}",
            "User-Agent": USER_AGENT,
        }
        if self.session_id:
            headers["Mcp-Session-Id"] = self.session_id
        req = urllib.request.Request(UPSTREAM_URL, data=body, headers=headers, method="POST")
        with urllib.request.urlopen(req, timeout=DEFAULT_TIMEOUT_S) as resp:
            resp_body = resp.read().decode("utf-8", "replace")
            new_session = resp.headers.get("Mcp-Session-Id")
            content_type = resp.headers.get("Content-Type", "")
        if new_session:
            self.session_id = new_session
        if "text/event-stream" in content_type:
            return _parse_sse(resp_body)
        with contextlib.suppress(json.JSONDecodeError):
            return [json.loads(resp_body)]
        return []

    def _ensure_initialized(self) -> None:
        if self._initialized:
            return
        init_envelope = {
            "jsonrpc": "2.0",
            "id": self._next_id(),
            "method": "initialize",
            "params": {
                "protocolVersion": PROTOCOL_VERSION,
                "capabilities": {},
                "clientInfo": {"name": "robin-app", "version": "1.0"},
            },
        }
        self._call_upstream(init_envelope)
        self._call_upstream({"jsonrpc": "2.0", "method": "notifications/initialized"})
        self._initialized = True
        logger.info("mcp session initialized (session_id=%s)", self.session_id)

    def call_tool(self, name: str, arguments: dict[str, Any] | None = None) -> Any:
        """Call an MCP tool by name and return its result content."""
        self._ensure_initialized()
        envelope = {
            "jsonrpc": "2.0",
            "id": self._next_id(),
            "method": "tools/call",
            "params": {"name": name, "arguments": arguments or {}},
        }
        responses = self._call_upstream(envelope)
        for resp in responses:
            if resp.get("id") != envelope["id"]:
                continue
            if "error" in resp:
                raise MCPError(f"tool '{name}' error: {resp['error']}")
            result = resp.get("result", {})
            content = result.get("content", [])
            for item in content:
                if item.get("type") == "text":
                    with contextlib.suppress(json.JSONDecodeError):
                        return json.loads(item["text"])
                    return item["text"]
            return result
        raise MCPError(f"tool '{name}' returned no response")

    def list_tools(self) -> list[dict[str, Any]]:
        """List available MCP tools. Returns list of {name, description, inputSchema}."""
        self._ensure_initialized()
        envelope = {
            "jsonrpc": "2.0",
            "id": self._next_id(),
            "method": "tools/list",
        }
        responses = self._call_upstream(envelope)
        for resp in responses:
            if resp.get("id") != envelope["id"]:
                continue
            if "error" in resp:
                raise MCPError(f"tools/list error: {resp['error']}")
            return resp.get("result", {}).get("tools", [])
        return []

    def has_credentials(self) -> bool:
        creds = _load_creds()
        return bool(creds.get("access_token") or creds.get("refresh_token"))

    def is_authenticated(self) -> bool:
        creds = _load_creds()
        return _is_token_valid(creds) or bool(creds.get("refresh_token"))

    def logout(self) -> None:
        import os

        self._initialized = False
        self.session_id = None
        with contextlib.suppress(FileNotFoundError):
            os.unlink(CREDS_PATH)
        logger.info("robinhood mcp credentials deleted")
