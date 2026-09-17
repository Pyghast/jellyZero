/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "spotify_api_client.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <fstream>

namespace model {
namespace {

constexpr const char* kTokenUrl = "https://accounts.spotify.com/api/token";
constexpr const char* kProfileUrl = "https://api.spotify.com/v1/me";
constexpr const char* kPlayerUrl = "https://api.spotify.com/v1/me/player";
constexpr long kTimeoutSeconds = 8;

std::size_t write_to_string(char* data, std::size_t size, std::size_t count, void* user_data) {
    auto* out = static_cast<std::string*>(user_data);
    out->append(data, size * count);
    return size * count;
}

struct CurlHandle {
    CURL* handle{curl_easy_init()};
    ~CurlHandle() {
        if (handle) curl_easy_cleanup(handle);
    }
};

std::string url_encode(CURL* curl, const std::string& value) {
    char* escaped = curl_easy_escape(curl, value.c_str(), static_cast<int>(value.size()));
    std::string result = escaped ? escaped : value;
    if (escaped) curl_free(escaped);
    return result;
}

bool perform(CURL* curl, std::string& response_body, long& status_code, std::string& error) {
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, kTimeoutSeconds);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    const auto result = curl_easy_perform(curl);
    if (result != CURLE_OK) {
        error = curl_easy_strerror(result);
        return false;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
    return true;
}

// This device's cover art box is 64x64, so prefer the smallest image that's
// still at least that big (avoids downloading a 640x640 original), falling
// back to the largest available if nothing reaches 64px.
std::string smallest_artwork_url(const nlohmann::json& images) {
    std::string smallest_at_least_64;
    int smallest_at_least_64_width = 1'000'000;
    std::string largest_overall;
    int largest_overall_width = -1;

    for (const auto& image : images) {
        const auto width = image.value("width", 0);
        const auto url = image.value("url", std::string{});
        if (url.empty()) continue;

        if (width > largest_overall_width) {
            largest_overall = url;
            largest_overall_width = width;
        }
        if (width >= 64 && width < smallest_at_least_64_width) {
            smallest_at_least_64 = url;
            smallest_at_least_64_width = width;
        }
    }

    return !smallest_at_least_64.empty() ? smallest_at_least_64 : largest_overall;
}

} // namespace

bool spotify_refresh_access_token(SpotifyTokens& tokens, std::string& error) {
    CurlHandle curl_handle;
    if (!curl_handle.handle) {
        error = "failed to init curl";
        return false;
    }
    auto* curl = curl_handle.handle;

    const std::string body = "grant_type=refresh_token"
                              "&refresh_token=" + url_encode(curl, tokens.refresh_token) +
                              "&client_id=" + url_encode(curl, tokens.client_id);

    curl_easy_setopt(curl, CURLOPT_URL, kTokenUrl);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));

    curl_slist* headers = curl_slist_append(nullptr, "Content-Type: application/x-www-form-urlencoded");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    std::string response_body;
    long status_code = 0;
    const bool ok = perform(curl, response_body, status_code, error);
    curl_slist_free_all(headers);
    if (!ok) {
        return false;
    }

    if (status_code != 200) {
        error = "token refresh failed with HTTP " + std::to_string(status_code) + ": " + response_body;
        return false;
    }

    try {
        const auto json = nlohmann::json::parse(response_body);
        tokens.access_token = json.at("access_token").get<std::string>();
        const auto expires_in = json.at("expires_in").get<std::int64_t>();
        const auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        tokens.expires_at = now + expires_in;
        if (json.contains("refresh_token")) {
            tokens.refresh_token = json.at("refresh_token").get<std::string>();
        }
        if (json.contains("scope")) {
            tokens.scope = json.at("scope").get<std::string>();
        }
    }
    catch (const nlohmann::json::exception& parse_error) {
        error = std::string("malformed token refresh response: ") + parse_error.what();
        return false;
    }

    return true;
}

bool spotify_fetch_profile(const SpotifyTokens& tokens, SpotifyProfile& profile, std::string& error) {
    CurlHandle curl_handle;
    if (!curl_handle.handle) {
        error = "failed to init curl";
        return false;
    }
    auto* curl = curl_handle.handle;

    curl_easy_setopt(curl, CURLOPT_URL, kProfileUrl);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

    const std::string auth_header = "Authorization: Bearer " + tokens.access_token;
    curl_slist* headers = curl_slist_append(nullptr, auth_header.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    std::string response_body;
    long status_code = 0;
    const bool ok = perform(curl, response_body, status_code, error);
    curl_slist_free_all(headers);
    if (!ok) {
        return false;
    }

    if (status_code != 200) {
        error = "profile fetch failed with HTTP " + std::to_string(status_code) + ": " + response_body;
        return false;
    }

    try {
        const auto json = nlohmann::json::parse(response_body);
        profile.display_name = json.value("display_name", std::string{"Spotify user"});
        profile.user_id = json.value("id", std::string{});
    }
    catch (const nlohmann::json::exception& parse_error) {
        error = std::string("malformed profile response: ") + parse_error.what();
        return false;
    }

    return true;
}

bool spotify_fetch_playback_state(const SpotifyTokens& tokens, SpotifyPlaybackState& state, std::string& error) {
    CurlHandle curl_handle;
    if (!curl_handle.handle) {
        error = "failed to init curl";
        return false;
    }
    auto* curl = curl_handle.handle;

    curl_easy_setopt(curl, CURLOPT_URL, kPlayerUrl);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

    const std::string auth_header = "Authorization: Bearer " + tokens.access_token;
    curl_slist* headers = curl_slist_append(nullptr, auth_header.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    std::string response_body;
    long status_code = 0;
    const bool ok = perform(curl, response_body, status_code, error);
    curl_slist_free_all(headers);
    if (!ok) {
        return false;
    }

    // No active session — a normal state (e.g. nothing playing anywhere on
    // this account right now), not an error.
    if (status_code == 204 || response_body.empty()) {
        state = SpotifyPlaybackState{};
        return true;
    }

    if (status_code != 200) {
        error = "playback state fetch failed with HTTP " + std::to_string(status_code) + ": " + response_body;
        return false;
    }

    try {
        const auto json = nlohmann::json::parse(response_body);
        state = SpotifyPlaybackState{};

        if (!json.contains("item") || json.at("item").is_null()) {
            return true; // e.g. an ad is playing, or session has no track loaded
        }

        const auto& item = json.at("item");
        state.has_active_playback = true;
        state.is_playing = json.value("is_playing", false);
        state.progress_ms = json.value("progress_ms", 0);
        state.track_id = item.value("id", std::string{});
        state.title = item.value("name", std::string{});
        state.duration_ms = item.value("duration_ms", 0);

        std::string artists;
        for (const auto& artist : item.value("artists", nlohmann::json::array())) {
            if (!artists.empty()) artists += ", ";
            artists += artist.value("name", std::string{});
        }
        state.artist = artists;

        if (item.contains("album")) {
            state.album = item.at("album").value("name", std::string{});
            if (item.at("album").contains("images")) {
                state.artwork_url = smallest_artwork_url(item.at("album").at("images"));
            }
        }

        if (json.contains("device") && !json.at("device").is_null()) {
            const auto& device = json.at("device");
            if (device.contains("volume_percent") && !device.at("volume_percent").is_null()) {
                state.has_volume = true;
                state.volume_percent = device.value("volume_percent", 0);
            }
        }
    }
    catch (const nlohmann::json::exception& parse_error) {
        error = std::string("malformed playback state response: ") + parse_error.what();
        return false;
    }

    return true;
}

bool spotify_player_command(const SpotifyTokens& tokens, const std::string& method,
                             const std::string& path_and_query, std::string& error) {
    CurlHandle curl_handle;
    if (!curl_handle.handle) {
        error = "failed to init curl";
        return false;
    }
    auto* curl = curl_handle.handle;

    const std::string url = std::string(kPlayerUrl) + path_and_query;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);

    const std::string auth_header = "Authorization: Bearer " + tokens.access_token;
    curl_slist* headers = curl_slist_append(nullptr, auth_header.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    std::string response_body;
    long status_code = 0;
    const bool ok = perform(curl, response_body, status_code, error);
    curl_slist_free_all(headers);
    if (!ok) {
        return false;
    }

    if (status_code == 200 || status_code == 204) {
        return true;
    }
    if (status_code == 404) {
        error = "no active Spotify device — open Spotify and start playing once, then control it from here";
        return false;
    }
    if (status_code == 403) {
        error = "Spotify refused this action (403) — " + response_body;
        return false;
    }

    error = "player command failed with HTTP " + std::to_string(status_code) + ": " + response_body;
    return false;
}

bool spotify_download_artwork(const std::string& url, const std::filesystem::path& dest, std::string& error) {
    CurlHandle curl_handle;
    if (!curl_handle.handle) {
        error = "failed to init curl";
        return false;
    }
    auto* curl = curl_handle.handle;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

    std::string response_body;
    long status_code = 0;
    const bool ok = perform(curl, response_body, status_code, error);
    if (!ok) {
        return false;
    }
    if (status_code != 200) {
        error = "artwork download failed with HTTP " + std::to_string(status_code);
        return false;
    }

    std::error_code dir_error;
    std::filesystem::create_directories(dest.parent_path(), dir_error);

    std::ofstream file(dest, std::ios::binary | std::ios::trunc);
    if (!file) {
        error = "failed to open artwork cache file for write: " + dest.string();
        return false;
    }
    file.write(response_body.data(), static_cast<std::streamsize>(response_body.size()));
    if (!file) {
        error = "failed to write artwork cache file: " + dest.string();
        return false;
    }
    return true;
}

} // namespace model
