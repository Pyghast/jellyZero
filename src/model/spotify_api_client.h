/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "spotify_token_store.h"

#include <curl/curl.h>

#include <filesystem>
#include <string>

namespace model {

// Every function here takes a caller-owned CURL* easy handle rather than
// creating its own. Create one handle per worker thread (curl_easy_init()),
// reuse it across every call for that thread's lifetime, and
// curl_easy_cleanup() it once the thread is done. Reusing the handle lets
// curl keep the underlying TCP/TLS connection to Spotify alive between
// calls instead of renegotiating a full TLS handshake every few seconds —
// meaningful CPU/battery/network cost on hardware with no crypto
// acceleration. Each function resets the handle's options at the start, so
// passing the same handle between different calls (GET polls, PUT/POST
// commands, token refresh) is safe. Handles are not thread-safe: only ever
// use one from a single thread at a time.

struct SpotifyProfile {
    std::string display_name;
    std::string user_id;
};

// PKCE refresh-token grant: POST https://accounts.spotify.com/api/token.
// On success, updates tokens.access_token/expires_at in place (the refresh
// token itself may also rotate; that's applied too when Spotify returns one).
// Blocking network call — run off the LVGL thread.
bool spotify_refresh_access_token(CURL* curl, SpotifyTokens& tokens, std::string& error);

// GET https://api.spotify.com/v1/me using the current access token.
// Blocking network call — run off the LVGL thread.
bool spotify_fetch_profile(CURL* curl, const SpotifyTokens& tokens, SpotifyProfile& profile, std::string& error);

struct SpotifyPlaybackState {
    // False when Spotify reports no active session (HTTP 204) or no track
    // item — a normal, expected state, not an error.
    bool has_active_playback{false};
    bool is_playing{false};
    std::string track_id;
    std::string title;
    std::string artist;      // joined with ", " when there are several
    std::string album;
    std::string artwork_url; // smallest available cover image; empty if none
    int duration_ms{0};
    int progress_ms{0};
    bool has_volume{false};  // the active device reported a volume level
    int volume_percent{0};
};

// GET https://api.spotify.com/v1/me/player. Blocking network call.
bool spotify_fetch_playback_state(CURL* curl, const SpotifyTokens& tokens, SpotifyPlaybackState& state, std::string& error);

// A player transport command: method is "PUT" or "POST"; path_and_query is
// appended to https://api.spotify.com/v1/me/player, e.g. "/pause", "/next",
// "/seek?position_ms=1000". Blocking network call.
bool spotify_player_command(CURL* curl, const SpotifyTokens& tokens, const std::string& method,
                             const std::string& path_and_query, std::string& error);

// Downloads url's bytes to dest (overwriting it). No auth header — Spotify's
// album art URLs are public CDN links. Blocking network call.
bool spotify_download_artwork(CURL* curl, const std::string& url, const std::filesystem::path& dest, std::string& error);

} // namespace model
