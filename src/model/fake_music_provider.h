#pragma once
#include <array>
#include <algorithm>
#include <cstddef>

namespace model {

struct Track {
    const char* title;
    const char* artist;
    const char* album;
    int duration_seconds;
    const char* artwork_path; // Cached local image; empty means unknown artwork.
};

// Local fixtures only. No Spotify session, audio output or network access.
class FakeMusicProvider {
public:
    const Track& current_track() const { return tracks_[track_index_]; }
    static constexpr std::size_t queue_size = 5;
    const Track& queued_track(std::size_t offset) const {
        return tracks_[(track_index_ + 1 + offset % tracks_.size()) % tracks_.size()];
    }
    void next() { track_index_ = (track_index_ + 1) % tracks_.size(); elapsed_ = 0; }
    void previous() { track_index_ = (track_index_ + tracks_.size() - 1) % tracks_.size(); elapsed_ = 0; }
    void seek_by(int seconds) {
        elapsed_ = static_cast<int>(std::clamp(static_cast<long long>(elapsed_) + seconds,
            0LL, static_cast<long long>(current_track().duration_seconds)));
    }
    int volume() const { return muted_ ? 0 : volume_; }
    bool muted() const { return muted_; }
    void toggle_mute() { muted_ = !muted_; }
    void change_volume(int delta) {
        muted_ = false;
        volume_ = static_cast<int>(std::clamp(static_cast<long long>(volume_) + delta, 0LL, 100LL));
    }
    bool playing() const { return playing_; }
    void toggle_playback() { playing_ = !playing_; }
    int elapsed_seconds() const { return elapsed_; }
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
