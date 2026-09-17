/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "spotify_music_provider.h"

#include "spotify_api_client.h"
#include "user_config.h"

#include "lvgl.h"

#include <algorithm>
#include <chrono>

namespace model {
namespace {

constexpr auto kPollInterval = std::chrono::seconds(3);
constexpr int kSeekStepMs = 10000;
constexpr int kVolumeStepPercent = 5;

std::filesystem::path artwork_cache_path(const std::string& track_id) {
    return platform::user_config_dir() / "spotify-art-cache" / (track_id + ".jpg");
}

} // namespace

SpotifyMusicProvider::SpotifyMusicProvider() {
    // Avoid touching the network at all when there's no token file yet —
    // stay NotConfigured until the user runs tools/spotify_login.py and
    // copies tokens.json onto the device.
    SpotifyTokens probe;
    std::string probe_error;
    if (!load_spotify_tokens(spotify_token_file_path(), probe, probe_error)) {
        auth_state_ = AuthState::NotConfigured;
        return;
    }

    auth_state_ = AuthState::Connecting;
    worker_ = std::thread([this] { worker_main(); });
}

SpotifyMusicProvider::~SpotifyMusicProvider() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_requested_ = true;
    }
    cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
    // The worker may have queued a callback for a tick that never ran
    // (e.g. app shutting down); don't let it fire into a destroyed object.
    lv_async_call_cancel(&SpotifyMusicProvider::deliver_result, this);
}

void SpotifyMusicProvider::set_on_change(std::function<void()> callback) {
    on_change_ = std::move(callback);
}

void SpotifyMusicProvider::worker_main() {
    std::string error;
    if (!load_spotify_tokens(spotify_token_file_path(), tokens_, error)) {
        std::lock_guard<std::mutex> lock(mutex_);
        auth_state_ = AuthState::NotConfigured;
        error_message_ = error;
        lv_async_call(&SpotifyMusicProvider::deliver_result, this);
        return;
    }

    bool ok = true;
    if (spotify_tokens_need_refresh(tokens_)) {
        ok = spotify_refresh_access_token(tokens_, error);
        if (ok) {
            std::string save_error;
            save_spotify_tokens(spotify_token_file_path(), tokens_, save_error);
        }
    }

    std::string display_name;
    if (ok) {
        SpotifyProfile profile;
        ok = spotify_fetch_profile(tokens_, profile, error);
        if (ok) display_name = profile.display_name;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auth_state_ = ok ? AuthState::SignedIn : AuthState::Error;
        display_name_ = display_name;
        error_message_ = error;
    }
    lv_async_call(&SpotifyMusicProvider::deliver_result, this);

    if (!ok) {
        return;
    }

    while (true) {
        Action action = Action::None;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait_for(lock, kPollInterval,
                [this] { return stop_requested_ || pending_action_ != Action::None; });
            if (stop_requested_) return;
            if (pending_action_ != Action::None) {
                action = pending_action_;
                pending_action_ = Action::None;
            }
        }
        if (action != Action::None) {
            perform_action(action);
        }
        poll_playback_state();
        lv_async_call(&SpotifyMusicProvider::deliver_result, this);
    }
}

void SpotifyMusicProvider::queue_action(Action action) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_action_ = action;
    }
    cv_.notify_all();
}

void SpotifyMusicProvider::perform_action(Action action) {
    int current_volume;
    int current_progress_ms;
    int current_duration_ms;
    bool currently_playing;
    bool currently_muted;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        current_volume = volume_percent_;
        current_progress_ms = progress_ms_;
        current_duration_ms = duration_ms_;
        currently_playing = is_playing_;
        currently_muted = muted_;
    }

    std::string error;
    switch (action) {
        case Action::None:
            return;
        case Action::TogglePlayback:
            spotify_player_command(tokens_, "PUT", currently_playing ? "/pause" : "/play", error);
            break;
        case Action::Next:
            spotify_player_command(tokens_, "POST", "/next", error);
            break;
        case Action::Previous:
            spotify_player_command(tokens_, "POST", "/previous", error);
            break;
        case Action::SeekBack: {
            const auto position = std::max(0, current_progress_ms - kSeekStepMs);
            spotify_player_command(tokens_, "PUT", "/seek?position_ms=" + std::to_string(position), error);
            break;
        }
        case Action::SeekForward: {
            const auto position = std::min(current_duration_ms, current_progress_ms + kSeekStepMs);
            spotify_player_command(tokens_, "PUT", "/seek?position_ms=" + std::to_string(position), error);
            break;
        }
        case Action::ToggleMute: {
            const auto target = currently_muted ? std::max(current_volume, kVolumeStepPercent) : 0;
            spotify_player_command(tokens_, "PUT", "/volume?volume_percent=" + std::to_string(target), error);
            std::lock_guard<std::mutex> lock(mutex_);
            muted_ = !currently_muted;
            break;
        }
        case Action::VolumeUp:
        case Action::VolumeDown: {
            const auto delta = action == Action::VolumeUp ? kVolumeStepPercent : -kVolumeStepPercent;
            const auto target = std::clamp(current_volume + delta, 0, 100);
            spotify_player_command(tokens_, "PUT", "/volume?volume_percent=" + std::to_string(target), error);
            std::lock_guard<std::mutex> lock(mutex_);
            muted_ = false;
            break;
        }
    }
    // Errors here (e.g. no active device) are intentionally not surfaced as
    // a hard failure state — the next poll just reflects whatever Spotify's
    // actual state is, which is the source of truth.
}

void SpotifyMusicProvider::poll_playback_state() {
    SpotifyPlaybackState state;
    std::string error;
    if (!spotify_fetch_playback_state(tokens_, state, error)) {
        return; // transient failure; keep showing the last known state
    }

    if (state.has_active_playback && !state.track_id.empty()) {
        maybe_download_artwork(state.track_id, state.artwork_url);
    }

    std::lock_guard<std::mutex> lock(mutex_);
    has_active_playback_ = state.has_active_playback;
    is_playing_ = state.is_playing;
    progress_ms_ = state.progress_ms;
    duration_ms_ = state.duration_ms;
    track_id_ = state.track_id;
    title_ = state.title;
    artist_ = state.artist;
    album_ = state.album;
    if (state.has_volume) {
        volume_percent_ = state.volume_percent;
    }
}

void SpotifyMusicProvider::maybe_download_artwork(const std::string& track_id, const std::string& artwork_url) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (track_id == track_id_) {
            return; // already have (or already tried) this track's art
        }
    }
    if (artwork_url.empty()) {
        std::lock_guard<std::mutex> lock(mutex_);
        artwork_path_.clear();
        return;
    }

    const auto dest = artwork_cache_path(track_id);
    std::string error;
    std::lock_guard<std::mutex> lock(mutex_);
    if (spotify_download_artwork(artwork_url, dest, error)) {
        artwork_path_ = dest.string();
    }
    else {
        artwork_path_.clear();
    }
}

void SpotifyMusicProvider::deliver_result(void* user_data) {
    auto* self = static_cast<SpotifyMusicProvider*>(user_data);
    self->apply_snapshot_and_notify();
}

void SpotifyMusicProvider::apply_snapshot_and_notify() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_title_ = title_;
        snapshot_artist_ = artist_;
        snapshot_album_ = album_;
        snapshot_artwork_path_ = artwork_path_;
        current_track_snapshot_ = Track{
            snapshot_title_.c_str(),
            snapshot_artist_.c_str(),
            snapshot_album_.c_str(),
            duration_ms_ / 1000,
            snapshot_artwork_path_.c_str(),
        };
        snapshot_has_active_playback_ = has_active_playback_;
    }
    if (on_change_) {
        on_change_();
    }
}

bool SpotifyMusicProvider::signed_in() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return auth_state_ == AuthState::SignedIn;
}

std::string SpotifyMusicProvider::status_text() const {
    std::lock_guard<std::mutex> lock(mutex_);
    switch (auth_state_) {
        case AuthState::NotConfigured:
            return "Spotify: sign-in not configured";
        case AuthState::Connecting:
            return "Spotify: connecting...";
        case AuthState::SignedIn:
            return "Signed in as " + display_name_;
        case AuthState::Error:
            return "Spotify: sign-in error - " + error_message_;
    }
    return "Spotify: unknown state";
}

const Track& SpotifyMusicProvider::current_track() const {
    return snapshot_has_active_playback_ ? current_track_snapshot_ : empty_track_;
}

const Track& SpotifyMusicProvider::queued_track(std::size_t) const {
    return empty_track_; // Spotify's real queue isn't fetched this phase.
}

void SpotifyMusicProvider::next() { queue_action(Action::Next); }
void SpotifyMusicProvider::previous() { queue_action(Action::Previous); }
void SpotifyMusicProvider::seek_by(int seconds) {
    queue_action(seconds < 0 ? Action::SeekBack : Action::SeekForward);
}
void SpotifyMusicProvider::toggle_mute() { queue_action(Action::ToggleMute); }
void SpotifyMusicProvider::change_volume(int delta) {
    queue_action(delta > 0 ? Action::VolumeUp : Action::VolumeDown);
}
void SpotifyMusicProvider::toggle_playback() { queue_action(Action::TogglePlayback); }

int SpotifyMusicProvider::volume() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return muted_ ? 0 : volume_percent_;
}
bool SpotifyMusicProvider::muted() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return muted_;
}
bool SpotifyMusicProvider::playing() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return has_active_playback_ && is_playing_;
}
int SpotifyMusicProvider::elapsed_seconds() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return progress_ms_ / 1000;
}

} // namespace model
