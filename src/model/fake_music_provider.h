#pragma once
#include "music_provider.h"
#include <array>
#include <algorithm>
#include <cstddef>

namespace model {

// Local fixtures only. No Jellyfin session, audio output or network access.
// Used as the Jellyfin placeholder, and as the Spotify slot's fallback
// before/without sign-in (see BaseViewModel::music()).
class FakeMusicProvider : public MusicProvider {
public:
    const Track& current_track() const override { return tracks_[track_index_]; }
    const Track& queued_track(std::size_t offset) const override {
        return tracks_[(track_index_ + 1 + offset % tracks_.size()) % tracks_.size()];
    }
    void next() override { track_index_ = (track_index_ + 1) % tracks_.size(); elapsed_ = 0; }
    void previous() override { track_index_ = (track_index_ + tracks_.size() - 1) % tracks_.size(); elapsed_ = 0; }
    void seek_by(int seconds) override {
        elapsed_ = static_cast<int>(std::clamp(static_cast<long long>(elapsed_) + seconds,
            0LL, static_cast<long long>(current_track().duration_seconds)));
    }
    int volume() const override { return muted_ ? 0 : volume_; }
    bool muted() const override { return muted_; }
    void toggle_mute() override { muted_ = !muted_; }
    void change_volume(int delta) override {
        muted_ = false;
        volume_ = static_cast<int>(std::clamp(static_cast<long long>(volume_) + delta, 0LL, 100LL));
    }
    bool playing() const override { return playing_; }
    void toggle_playback() override { playing_ = !playing_; }
    int elapsed_seconds() const override { return elapsed_; }
private:
    bool playing_{true};
    std::size_t track_index_{0};
    int elapsed_{102};
    int volume_{50};
    bool muted_{false};
    const std::array<Track, 6> tracks_{{
        {"MONEY FOR NOTHING", "Dire Straits", "Brothers in Arms", 306, "images/demo-cover-purple.png"},
        {"WALK OF LIFE", "Dire Straits", "Brothers in Arms", 249, "images/demo-cover-purple.png"},
        {"LEMONADE", "Demo artist", "Demo album", 210, ""},
        {"THUNDERSTRUCK", "AC/DC", "The Razors Edge", 292, "images/demo-cover-blue.png"},
        {"AEIOU", "Demo artist", "Demo album", 240, ""},
        {"SULTANS OF SWING", "Dire Straits", "Dire Straits", 348, ""},
    }};
};

} // namespace model
