/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "music_provider.h"
#include "spotify_token_store.h"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace model {

// Real Spotify playback backend. A single background thread handles sign-in
// (Phase 1), then polls GET /v1/me/player every few seconds and executes
// playback commands, all off the LVGL thread. UI-thread reads
// (current_track()/playing()/etc.) never block on the network — they read
// the latest snapshot the worker thread already fetched.
//
// Known limitation: queued_track() always returns empty. Fetching Spotify's
// real queue is a separate API call this phase doesn't make; the queue
// panel just stays blank while Spotify is the live source.
class SpotifyMusicProvider : public MusicProvider {
public:
    SpotifyMusicProvider();
    ~SpotifyMusicProvider() override;

    SpotifyMusicProvider(const SpotifyMusicProvider&) = delete;
    SpotifyMusicProvider& operator=(const SpotifyMusicProvider&) = delete;

    // Call once on the LVGL thread, right after construction. Invoked (on
    // the LVGL thread, via lv_async_call) whenever fresh state arrives.
    void set_on_change(std::function<void()> callback);

    bool signed_in() const;
    std::string status_text() const;

    // MusicProvider — safe no-ops/empty data until signed in and the first
    // poll completes; current_track()/queued_track() are LVGL-thread-only
    // (their pointers are only ever rebuilt inside the async delivery
    // callback, which itself only runs on the LVGL thread).
    const Track& current_track() const override;
    const Track& queued_track(std::size_t offset) const override;
    void next() override;
    void previous() override;
    void seek_by(int seconds) override;
    int volume() const override;
    bool muted() const override;
    void toggle_mute() override;
    void change_volume(int delta) override;
    bool playing() const override;
    void toggle_playback() override;
    int elapsed_seconds() const override;

private:
    enum class AuthState { NotConfigured, Connecting, SignedIn, Error };
    enum class Action { None, TogglePlayback, Next, Previous, SeekBack, SeekForward, ToggleMute, VolumeUp, VolumeDown };

    void worker_main();
    void poll_playback_state();
    void perform_action(Action action);
    void queue_action(Action action);
    void maybe_download_artwork(const std::string& track_id, const std::string& artwork_url);
    static void deliver_result(void* user_data);
    void apply_snapshot_and_notify();

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_requested_{false};
    Action pending_action_{Action::None};

    AuthState auth_state_{AuthState::NotConfigured};
    std::string display_name_;
    std::string error_message_;

    // Live playback snapshot — written by the worker thread, read (under
    // lock) by deliver_result on the LVGL thread.
    bool has_active_playback_{false};
    bool is_playing_{false};
    bool muted_{false};
    int volume_percent_{0};
    int progress_ms_{0};
    int duration_ms_{0};
    std::string track_id_;
    std::string title_;
    std::string artist_;
    std::string album_;
    std::string artwork_path_;

    // LVGL-thread-only storage backing current_track_snapshot_'s pointers,
    // refreshed only inside apply_snapshot_and_notify() (itself only ever
    // called on the LVGL thread via lv_async_call).
    std::string snapshot_title_;
    std::string snapshot_artist_;
    std::string snapshot_album_;
    std::string snapshot_artwork_path_;
    Track current_track_snapshot_{"", "", "", 0, ""};
    bool snapshot_has_active_playback_{false};

    // Worker-thread-only; no synchronization needed (single owner thread).
    SpotifyTokens tokens_;

    std::thread worker_;
    std::function<void()> on_change_;

    const Track empty_track_{"", "", "", 0, ""};
};

} // namespace model
