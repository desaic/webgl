from __future__ import annotations

import contextlib
import json
from datetime import UTC, datetime
from typing import Any

from robin.config import DATA_DIR, READ_ONLY, ROBINHOOD_SESSION_PATH, logger


class RobinhoodAuthError(Exception):
    pass


class NeedLogin(RobinhoodAuthError):
    pass


class NeedMfa(RobinhoodAuthError):
    pass


_CLIENT_ID = "c82SH0WZOsabOXGP2sxqcj34FxkvfnWRZBKlBjFS"


class NeedVerification(RobinhoodAuthError):
    pass


class RobinhoodAuth:
    """Read-only Robinhood authentication backed by a JSON session file.

    Security: we do NOT use pickle for session storage. pickle.load can execute
    arbitrary code if the file is tampered with; JSON cannot. The session file
    contains only plain strings (access_token, refresh_token, token_type,
    device_token, username) so JSON is a perfect fit.

    First login: we make the OAuth POST ourselves (capturing tokens + device
    token), then store them as JSON. No pickle file is ever created.
    Subsequent starts: we load the JSON, set the Authorization header directly
    via robin_stocks' session helpers, and verify with a lightweight GET. If the
    access token is expired, we refresh it using the refresh_token. No MFA,
    no password re-entry.
    """

    def __init__(self) -> None:
        self.logged_in = False
        self.username: str | None = None

    @property
    def session_path(self) -> Any:
        return ROBINHOOD_SESSION_PATH

    def has_session(self) -> bool:
        return self.session_path.exists()

    def _load_session(self) -> dict[str, Any] | None:
        if not self.session_path.exists():
            return None
        try:
            return json.loads(self.session_path.read_text())
        except (json.JSONDecodeError, OSError) as e:
            logger.warning("could not load session json: %s", e)
            return None

    def _save_session(self, data: dict[str, Any]) -> None:
        DATA_DIR.mkdir(parents=True, exist_ok=True)
        self.session_path.write_text(json.dumps(data, indent=2))

    def _set_auth_header(self, token_type: str, access_token: str) -> None:
        import robin_stocks.robinhood as r

        r.helper.update_session("Authorization", f"{token_type} {access_token}")
        r.helper.set_login_state(True)

    def _verify_token(self) -> bool:
        import robin_stocks.robinhood as r

        try:
            res = r.helper.request_get(
                r.account.positions_url(), "pagination", {"nonzero": "true"}, jsonify_data=False
            )
            res.raise_for_status()
            return True
        except Exception:
            return False

    def _refresh_tokens(self, refresh_token: str) -> dict[str, Any] | None:
        import robin_stocks.robinhood as r

        payload = {
            "client_id": _CLIENT_ID,
            "grant_type": "refresh_token",
            "refresh_token": refresh_token,
            "scope": "internal",
        }
        try:
            data = r.helper.request_post(r.authentication.login_url(), payload)
        except Exception as e:
            logger.warning("token refresh request failed: %s", e)
            return None
        if data and "access_token" in data:
            return data
        return None

    def auto_login(self) -> bool:
        session = self._load_session()
        if session is None:
            raise NeedLogin("no session file; submit credentials via the UI")
        self._set_auth_header(session["token_type"], session["access_token"])
        if self._verify_token():
            self.logged_in = True
            self.username = session.get("username")
            logger.info("robinhood auto-login via JSON succeeded (user=%s)", self.username)
            return True
        logger.info("access token expired; attempting refresh")
        new_data = self._refresh_tokens(session["refresh_token"])
        if new_data is None:
            raise NeedLogin("session expired and token refresh failed; submit credentials")
        session["access_token"] = new_data["access_token"]
        session["refresh_token"] = new_data["refresh_token"]
        session["token_type"] = new_data["token_type"]
        session["saved_at"] = datetime.now(UTC).isoformat()
        self._save_session(session)
        self._set_auth_header(session["token_type"], session["access_token"])
        if not self._verify_token():
            raise NeedLogin("token refresh succeeded but verification still fails")
        self.logged_in = True
        self.username = session.get("username")
        logger.info("robinhood token refresh succeeded (user=%s)", self.username)
        return True

    @staticmethod
    def _post_login(r: Any, payload: dict[str, Any]) -> dict[str, Any] | None:
        try:
            return r.helper.request_post(r.authentication.login_url(), payload)
        except Exception as e:
            raise RobinhoodAuthError(f"login request failed: {e}") from e

    @staticmethod
    def _poll_verification(r: Any, device_token: str, workflow_id: str) -> None:
        """Poll Robinhood's verification workflow until approved or timeout.

        Replaces robin_stocks' _validate_sherrif_id which crashes on None
        responses. Polls the pathfinder inquiry endpoint for up to 2 minutes.
        The user approves the login in the Robinhood app during this window.
        """
        import time

        pathfinder_url = "https://api.robinhood.com/pathfinder/user_machine/"
        machine_payload = {
            "device_id": device_token,
            "flow": "suv",
            "input": {"workflow_id": workflow_id},
        }
        machine_data = r.helper.request_post(pathfinder_url, machine_payload, json=True) or {}
        machine_id = machine_data.get("id") or machine_data.get("machine_id")
        if not machine_id:
            for _key, val in machine_data.items():
                if isinstance(val, str) and len(val) > 10:
                    machine_id = val
                    break
        if not machine_id:
            raise RobinhoodAuthError("verification: could not get machine id from pathfinder")

        inquiries_url = f"https://api.robinhood.com/pathfinder/inquiries/{machine_id}/user_view/"
        deadline = time.time() + 120
        while time.time() < deadline:
            time.sleep(5)
            resp = r.helper.request_get(inquiries_url)
            if not resp or not isinstance(resp, dict):
                continue
            ctx = resp.get("context") or {}
            challenge = ctx.get("sheriff_challenge")
            if challenge:
                status = challenge.get("status")
                ctype = challenge.get("type")
                if status == "validated":
                    logger.info("verification challenge validated")
                    return
                if ctype == "prompt" and status == "issued":
                    prompt_id = challenge.get("id")
                    if prompt_id:
                        prompt_url = f"https://api.robinhood.com/push/{prompt_id}/get_prompts_status/"
                        while time.time() < deadline:
                            time.sleep(5)
                            pr = r.helper.request_get(prompt_url)
                            if pr and isinstance(pr, dict) and pr.get("challenge_status") == "validated":
                                logger.info("verification prompt validated")
                                return
                    return
            workflow = resp.get("verification_workflow") or {}
            ws = workflow.get("workflow_status")
            if ws == "workflow_status_approved":
                logger.info("verification workflow approved")
                return
        raise RobinhoodAuthError("verification challenge timed out after 120s — approve the login in your Robinhood app and try again")

    def login_with_credentials(
        self, username: str, password: str, mfa_code: str | None = None
    ) -> None:
        import robin_stocks.robinhood as r

        device_token = r.authentication.generate_device_token()
        payload: dict[str, Any] = {
            "client_id": _CLIENT_ID,
            "expires_in": 86400,
            "grant_type": "password",
            "password": password,
            "scope": "internal",
            "username": username,
            "device_token": device_token,
            "try_passkeys": False,
            "token_request_path": "/login",
            "create_read_only_secondary_token": True,
        }
        if mfa_code:
            payload["mfa_code"] = mfa_code
        data = self._post_login(r, payload)
        if data is None:
            raise RobinhoodAuthError("login failed: empty response from Robinhood")
        if data.get("mfa_required"):
            raise NeedMfa("MFA code required")
        if "verification_workflow" in data:
            workflow_id = data["verification_workflow"]["id"]
            logger.info("robinhood verification challenge triggered; approve it in your app")
            self._poll_verification(r, device_token, workflow_id)
            data = self._post_login(r, payload)
            if not data or "access_token" not in data:
                raise RobinhoodAuthError("login failed after verification: no access token")
        if "access_token" not in data:
            raise RobinhoodAuthError(f"login failed: {data.get('detail', data)}")
        self._set_auth_header(data["token_type"], data["access_token"])
        self._save_session(
            {
                "username": username,
                "token_type": data["token_type"],
                "access_token": data["access_token"],
                "refresh_token": data["refresh_token"],
                "device_token": device_token,
                "saved_at": datetime.now(UTC).isoformat(),
            }
        )
        self.logged_in = True
        self.username = username
        logger.info("robinhood login succeeded for %s (session stored as JSON)", username)

    def logout(self) -> None:
        """Invalidate the Robinhood session: clear in-memory state and delete
        the JSON session file so auto_login won't work next restart. You'll
        need to re-enter username + password + MFA to log in again.
        """
        if self.logged_in:
            try:
                import robin_stocks.robinhood as r

                r.helper.set_login_state(False)
                r.helper.update_session("Authorization", None)
            except Exception:
                pass
        self.logged_in = False
        self.username = None
        with contextlib.suppress(FileNotFoundError):
            self.session_path.unlink()
        logger.info("robinhood session invalidated and session file deleted")

    def assert_read_only(self) -> None:
        if not READ_ONLY:
            raise RuntimeError("this build is read-only by configuration")
