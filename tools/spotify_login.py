#!/usr/bin/env python3
"""One-time Spotify sign-in helper for JellyZero (Option B: manual token file).

Run this on your own computer, NOT on the Cardputer Zero. It performs the
full OAuth 2.0 Authorization Code + PKCE flow against a Spotify app you
control, using a brief local HTTP server on 127.0.0.1 to catch the redirect
(desktop browsers handle localhost redirects natively; nothing is exposed to
your network). It never asks for or stores a Client Secret -- PKCE is the
only credential a device like this should ever hold.

Usage:
    python tools/spotify_login.py --client-id <your Spotify app's Client ID>

The Spotify app's Redirect URI must be registered as exactly:
    http://127.0.0.1:8888/callback
(or whatever --port you pass, e.g. http://127.0.0.1:<port>/callback)

Writes tokens.json (default: current directory) with the access/refresh
tokens JellyZero needs. Copy that file onto the device -- see
docs/spotify-setup.md for exactly where.
"""

import argparse
import base64
import hashlib
import http.server
import json
import secrets
import sys
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
import webbrowser

AUTHORIZE_URL = "https://accounts.spotify.com/authorize"
TOKEN_URL = "https://accounts.spotify.com/api/token"
DEFAULT_SCOPES = (
    "user-read-private user-read-email "
    "user-read-playback-state user-modify-playback-state "
    "user-read-currently-playing"
)


def generate_code_verifier() -> str:
    """RFC 7636 code_verifier: 43-128 chars from [A-Za-z0-9-._~]."""
    return secrets.token_urlsafe(64)[:128]


def code_challenge_from_verifier(verifier: str) -> str:
    """RFC 7636 S256 code_challenge: BASE64URL(SHA256(ASCII(verifier))), no padding."""
    digest = hashlib.sha256(verifier.encode("ascii")).digest()
    return base64.urlsafe_b64encode(digest).decode("ascii").rstrip("=")


class _CallbackResult:
    def __init__(self):
        self.code = None
        self.error = None
        self.event = threading.Event()


def _make_handler(expected_state, result: _CallbackResult):
    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):  # noqa: N802 (stdlib method name)
            parsed = urllib.parse.urlparse(self.path)
            if parsed.path != "/callback":
                self.send_response(404)
                self.end_headers()
                return

            params = urllib.parse.parse_qs(parsed.query)
            state = params.get("state", [None])[0]
            if state != expected_state:
                result.error = "state mismatch (possible CSRF); aborting"
            elif "error" in params:
                result.error = params["error"][0]
            elif "code" in params:
                result.code = params["code"][0]
            else:
                result.error = "no code or error in callback"

            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.end_headers()
            message = "Sign-in failed, check the terminal." if result.error else "Signed in — you can close this tab."
            self.wfile.write(f"<html><body><p>{message}</p></body></html>".encode("utf-8"))
            result.event.set()

        def log_message(self, *_args):  # silence default stderr access log
            pass

    return Handler


def run_pkce_flow(client_id: str, port: int, scope: str) -> dict:
    redirect_uri = f"http://127.0.0.1:{port}/callback"
    verifier = generate_code_verifier()
    challenge = code_challenge_from_verifier(verifier)
    state = secrets.token_urlsafe(16)

    authorize_params = {
        "client_id": client_id,
        "response_type": "code",
        "redirect_uri": redirect_uri,
        "code_challenge_method": "S256",
        "code_challenge": challenge,
        "scope": scope,
        "state": state,
    }
    authorize_url = f"{AUTHORIZE_URL}?{urllib.parse.urlencode(authorize_params)}"

    result = _CallbackResult()
    server = http.server.HTTPServer(("127.0.0.1", port), _make_handler(state, result))

    print(f"Opening your browser to sign in to Spotify (redirect_uri={redirect_uri})...")
    print("If it doesn't open automatically, visit:")
    print(authorize_url)
    webbrowser.open(authorize_url)

    server_thread = threading.Thread(target=server.handle_request, daemon=True)
    server_thread.start()

    if not result.event.wait(timeout=180):
        server.server_close()
        raise SystemExit("Timed out waiting for the Spotify sign-in redirect.")
    server.server_close()

    if result.error:
        raise SystemExit(f"Spotify sign-in failed: {result.error}")

    token_params = urllib.parse.urlencode({
        "grant_type": "authorization_code",
        "code": result.code,
        "redirect_uri": redirect_uri,
        "client_id": client_id,
        "code_verifier": verifier,
    }).encode("ascii")

    request = urllib.request.Request(
        TOKEN_URL,
        data=token_params,
        headers={"Content-Type": "application/x-www-form-urlencoded"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(request) as response:
            payload = json.loads(response.read().decode("utf-8"))
    except urllib.error.HTTPError as exc:
        raise SystemExit(f"Token exchange failed: HTTP {exc.code}: {exc.read().decode('utf-8', 'replace')}")

    return {
        "client_id": client_id,
        "access_token": payload["access_token"],
        "refresh_token": payload["refresh_token"],
        "scope": payload.get("scope", scope),
        "expires_at": int(time.time()) + int(payload["expires_in"]),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--client-id", required=True, help="Client ID from your dedicated Spotify app")
    parser.add_argument("--out", default="tokens.json", help="Where to write the token file (default: ./tokens.json)")
    parser.add_argument("--port", type=int, default=8888, help="Local callback port (default: 8888)")
    parser.add_argument("--scope", default=DEFAULT_SCOPES, help="Space-separated OAuth scopes to request")
    args = parser.parse_args()

    tokens = run_pkce_flow(args.client_id, args.port, args.scope)
    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(tokens, f, indent=2)

    print(f"Wrote {args.out}")
    print("Copy this file onto the device as tokens.json in its config directory -- see docs/spotify-setup.md.")
    print("This file grants access to your Spotify account: keep it out of git and don't share it.")


if __name__ == "__main__":
    main()
