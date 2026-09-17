#pragma once
#include <cstddef>

namespace model {

struct Track {
    const char* title;
    const char* artist;
    const char* album;
    int duration_seconds;
    const char* artwork_path; // Cached local image; empty means unknown artwork.
};

// Shared playback surface consumed by BaseViewModel/AppleScreen. FakeMusicProvider
// implements it for the offline preview (Jellyfin placeholder, and Spotify
// before/without sign-in); SpotifyMusicProvider implements it for real,
// live playback once signed in.
class MusicProvider {
public:
    static constexpr std::size_t queue_size = 5;

    virtual ~MusicProvider() = default;

    virtual const Track& current_track() const = 0;
    virtual const Track& queued_track(std::size_t offset) const = 0;
    virtual void next() = 0;
    virtual void previous() = 0;
    virtual void seek_by(int seconds) = 0;
    virtual int volume() const = 0;
    virtual bool muted() const = 0;
    virtual void toggle_mute() = 0;
    virtual void change_volume(int delta) = 0;
    virtual bool playing() const = 0;
    virtual void toggle_playback() = 0;
    virtual int elapsed_seconds() const = 0;
};

} // namespace model
