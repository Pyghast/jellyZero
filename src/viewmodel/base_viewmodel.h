/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "subjects.h"
#include "base_model.h"
#include "fake_music_provider.h"

#include "lvgl.h"

#include <memory>
#include <string>

namespace model {
class SpotifyMusicProvider;
}

namespace viewmodel {

class BaseViewModel {
public:
    BaseViewModel();
    ~BaseViewModel();

    BaseViewModel(const BaseViewModel&) = delete;
    BaseViewModel& operator=(const BaseViewModel&) = delete;

    lv_subject_t* title_subject();
    lv_subject_t* greeting_subject();
    lv_subject_t* dark_mode_subject();
    lv_subject_t* current_page_subject();
    lv_subject_t* counter_subject();
    lv_subject_t* bold_text_subject();
    lv_subject_t* info_visible_subject();
    lv_subject_t* quit_requested_subject();

    lv_subject_t* music_subject() { return music_changed_.native(); }
    static constexpr int source_count = 2;
    int source_cursor() const { return source_cursor_; }
    int source_index() const { return source_index_; }
    static const char* source_name(int index) { return index == 1 ? "Jellyfin" : "Spotify"; }
    const char* source_name() const { return source_name(source_index_); }
    bool handle_music_key(uint32_t key, bool long_pressed = false);
    const model::MusicProvider& music() const;

    // Real Spotify sign-in status (Phase 1: auth only, not live playback).
    std::string spotify_status_text() const;

    bool is_dark_mode() const;
    void set_dark_mode(bool enabled);
    void toggle_dark_mode();

    model::AppPage current_page() const;
    void show_apple_page();
    void show_butter_page();
    void toggle_page();
    void increment_counter();
    void decrement_counter();
    void toggle_bold_text();
    void toggle_info();
    void request_quit();

private:
    void publish_all();

    model::BaseModel model_;
    model::MusicProvider& mutable_music();
    std::array<model::FakeMusicProvider, source_count> music_sources_{};
    std::unique_ptr<model::SpotifyMusicProvider> spotify_link_;
    int source_index_{0};
    reactive::IntSubject music_changed_{0};
    int source_cursor_{0};
    reactive::StringSubject<32> title_subject_;
    reactive::StringSubject<32> greeting_subject_;
    reactive::BoolSubject dark_mode_subject_;
    reactive::IntSubject current_page_subject_;
    reactive::IntSubject counter_subject_;
    reactive::BoolSubject bold_text_subject_;
    reactive::BoolSubject info_visible_subject_;
    reactive::BoolSubject quit_requested_subject_;
};

} // namespace viewmodel
