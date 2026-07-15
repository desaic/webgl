from __future__ import annotations

from robin.config import logger
from robin.mcp_client import MCPError, RobinhoodMCPClient


class RobinhoodAuthError(Exception):
    pass


class NeedLogin(RobinhoodAuthError):
    pass


class NeedMfa(RobinhoodAuthError):
    pass


class RobinhoodAuth:
    """Thin wrapper around the MCP client's OAuth-based authentication.

    Replaces the old password + MFA flow with Robinhood's official OAuth 2.1
    PKCE flow. Login is browser-based (one-time), tokens are stored as JSON
    and auto-refreshed.
    """

    def __init__(self) -> None:
        self.logged_in = False
        self.username: str | None = None
        self.mcp = RobinhoodMCPClient()
        self._check_auth()

    def _check_auth(self) -> None:
        if self.mcp.is_authenticated():
            self.logged_in = True
            logger.info("robinhood mcp: authenticated (token valid or refreshable)")

    def has_session(self) -> bool:
        return self.mcp.has_credentials()

    def auto_login(self) -> bool:
        if self.mcp.is_authenticated():
            self.logged_in = True
            return True
        raise NeedLogin("no valid MCP credentials; click Connect Robinhood to OAuth")

    def login_oauth(self) -> None:
        """Run the browser OAuth PKCE flow. Opens a browser popup."""
        from robin.mcp_client import run_browser_login

        try:
            creds = run_browser_login()
            self.logged_in = True
            logger.info("robinhood oauth login successful (expires_in=%s)", creds.get("expires_in"))
        except MCPError as e:
            raise RobinhoodAuthError(f"oauth login failed: {e}") from e

    def logout(self) -> None:
        self.mcp.logout()
        self.logged_in = False
        self.username = None

    def assert_read_only(self) -> None:
        pass
