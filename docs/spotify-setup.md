# Spotify sign-in setup

JellyZero signs in to Spotify using OAuth 2.0 Authorization Code + PKCE, the
only pattern that's safe for a device that can't keep a secret. A Client
Secret is never used, stored, or needed anywhere in this flow.

This is **Phase 1**: it gets sign-in working and shows "Signed in as
`<your name>`" on the Butter screen's source picker. Live "currently
playing" data and real playback control over Spotify are a later phase —
until then, selecting Spotify still shows the offline demo preview on the
Now Playing screen, same as before.

## 1. Create a dedicated Spotify app

Go to the [Spotify Developer Dashboard](https://developer.spotify.com/dashboard)
and click **Create app**. Use a name like `jellyzero` — don't reuse an
existing app you have for something else. This keeps the redirect URI and
scopes for this project isolated from your other integrations.

Once created, open the app's settings and add this **Redirect URI** exactly:

```
http://127.0.0.1:8888/callback
```

(If you pass `--port` to the script below, use that port instead of 8888.)

Copy the app's **Client ID**. You will not need the Client Secret — ignore
it, and don't paste it anywhere (not into this repo, not into chat with an
assistant, nowhere).

## 2. Run the sign-in script on your computer

Run this on your own computer (not the Cardputer — it needs a real browser).
It's stdlib-only Python, no extra packages to install:

```bash
python tools/spotify_login.py --client-id <your Client ID>
```

This opens your browser to Spotify's sign-in page, catches the redirect on
a brief local server, exchanges the authorization code for tokens, and
writes `tokens.json` in the current directory.

`tokens.json` contains your access and refresh tokens. Treat it like a
password: don't commit it, don't share it. The repo's `.gitignore` already
excludes `tokens*.json`.

## 3. Copy the token file onto the device

- **Desktop simulator** (for testing this on your own machine): copy
  `tokens.json` to `config/tokens.json` in the repo.
- **Real Cardputer Zero hardware**: copy it to
  `~/.config/template-app/tokens.json` on the device (or
  `$XDG_CONFIG_HOME/template-app/tokens.json` if you've set that).

## 4. Check it worked

Relaunch JellyZero and press **8** to open the source picker. It should show
**"Signed in as `<your Spotify display name>`"** instead of "sign-in not
configured". If it shows an error instead, check the app's logs — the
message includes the HTTP status and response body from whichever Spotify
API call failed.

## Real-hardware build note

Building this feature for the CM0/aarch64 cross target requires `libcurl`
and `nlohmann/json.hpp` to be available in the BSP sysroot (equivalent to
Debian's `libcurl4-openssl-dev` and `nlohmann-json3-dev` packages). If
they're missing, `cmake --preset cp0-cross` fails with a clear error naming
what's missing — there's no automatic fallback for these like there is for
`fmt`, since cross-compiling curl and its TLS backend is a project of its
own.

At runtime, curl also needs a CA certificate bundle on the device (usually
`/etc/ssl/certs/ca-certificates.crt`, from the `ca-certificates` package) to
verify Spotify's TLS certificate — JellyZero never disables certificate
verification. If that bundle is missing, every Spotify call fails and the
Butter screen shows a sign-in error mentioning a certificate problem.

## Re-authenticating

Access tokens expire after about an hour; JellyZero refreshes them silently
using the refresh token, so this is normally invisible. If the refresh token
itself is ever revoked (e.g. you removed the app's access from your Spotify
account), just re-run `tools/spotify_login.py` and copy the new
`tokens.json` over.
