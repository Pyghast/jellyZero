/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace model {

// Written once by tools/spotify_login.py on the user's own computer, then
// copied onto the device. No Client Secret is ever stored here — PKCE only.
struct SpotifyTokens {
    std::string client_id;
    std::string access_token;
    std::string refresh_token;
    std::string scope;
    std::int64_t expires_at{0}; // unix epoch seconds
};

// <user config dir>/tokens.json — matches the .gitignore "tokens*.json" rule.
std::filesystem::path spotify_token_file_path();

bool load_spotify_tokens(const std::filesystem::path& path, SpotifyTokens& tokens, std::string& error);
bool save_spotify_tokens(const std::filesystem::path& path, const SpotifyTokens& tokens, std::string& error);

// True when the access token is already expired or expires within
// safety_margin_seconds, meaning a refresh-token exchange is needed before use.
bool spotify_tokens_need_refresh(const SpotifyTokens& tokens, std::int64_t safety_margin_seconds = 60);

} // namespace model
