/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "spotify_token_store.h"

#include "user_config.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <fstream>
#include <sstream>

namespace model {

std::filesystem::path spotify_token_file_path() {
    return platform::user_config_dir() / "tokens.json";
}

bool load_spotify_tokens(const std::filesystem::path& path, SpotifyTokens& tokens, std::string& error) {
    std::error_code exists_error;
    if (!std::filesystem::is_regular_file(path, exists_error)) {
        error = "token file not found: " + path.string();
        return false;
    }

    std::ifstream file(path);
    if (!file) {
        error = "failed to open token file: " + path.string();
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();

    nlohmann::json json;
    try {
        json = nlohmann::json::parse(buffer.str());
    }
    catch (const nlohmann::json::parse_error& parse_error) {
        error = std::string("malformed token file: ") + parse_error.what();
        return false;
    }

    if (!json.contains("client_id") || !json.contains("access_token") ||
        !json.contains("refresh_token") || !json.contains("expires_at")) {
        error = "token file missing required fields (client_id/access_token/refresh_token/expires_at)";
        return false;
    }

    try {
        tokens.client_id = json.at("client_id").get<std::string>();
        tokens.access_token = json.at("access_token").get<std::string>();
        tokens.refresh_token = json.at("refresh_token").get<std::string>();
        tokens.scope = json.value("scope", std::string{});
        tokens.expires_at = json.at("expires_at").get<std::int64_t>();
    }
    catch (const nlohmann::json::exception& type_error) {
        error = std::string("malformed token file: ") + type_error.what();
        return false;
    }

    return true;
}

bool save_spotify_tokens(const std::filesystem::path& path, const SpotifyTokens& tokens, std::string& error) {
    std::error_code dir_error;
    std::filesystem::create_directories(path.parent_path(), dir_error);
    if (dir_error) {
        error = "failed to create config directory: " + dir_error.message();
        return false;
    }

    const nlohmann::json json{
        {"client_id", tokens.client_id},
        {"access_token", tokens.access_token},
        {"refresh_token", tokens.refresh_token},
        {"scope", tokens.scope},
        {"expires_at", tokens.expires_at},
    };

    std::ofstream file(path, std::ios::trunc);
    if (!file) {
        error = "failed to open token file for write: " + path.string();
        return false;
    }
    file << json.dump(2);
    if (!file) {
        error = "failed to write token file: " + path.string();
        return false;
    }
    return true;
}

bool spotify_tokens_need_refresh(const SpotifyTokens& tokens, std::int64_t safety_margin_seconds) {
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return now + safety_margin_seconds >= tokens.expires_at;
}

} // namespace model
