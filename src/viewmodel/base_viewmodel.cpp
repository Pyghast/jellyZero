/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "base_viewmodel.h"
#include "linux_input.h"
#include "spotify_music_provider.h"

namespace viewmodel {
namespace {

int page_to_int(model::AppPage page) {
    return static_cast<int>(page);
}

} // namespace

BaseViewModel::BaseViewModel()
    : title_subject_(model_.app_title()),
      greeting_subject_(model_.greeting()),
      dark_mode_subject_(model_.dark_mode()),
      current_page_subject_(page_to_int(model_.current_page())),
      counter_subject_(0),
      bold_text_subject_(false),
      info_visible_subject_(false),
      quit_requested_subject_(false),
      spotify_link_(std::make_unique<model::SpotifyMusicProvider>()) {
    spotify_link_->set_on_change([this] { music_changed_.notify(); });
}

BaseViewModel::~BaseViewModel() = default;

std::string BaseViewModel::spotify_status_text() const {
    return spotify_link_->status_text();
}

const model::MusicProvider& BaseViewModel::music() const {
    if (source_index_ == 0 && spotify_link_->signed_in()) {
        return *spotify_link_;
    }
    return music_sources_[source_index_];
}

model::MusicProvider& BaseViewModel::mutable_music() {
    if (source_index_ == 0 && spotify_link_->signed_in()) {
        return *spotify_link_;
    }
    return music_sources_[source_index_];
}

lv_subject_t* BaseViewModel::title_subject() {
    return title_subject_.native();
}

lv_subject_t* BaseViewModel::greeting_subject() {
    return greeting_subject_.native();
}

lv_subject_t* BaseViewModel::dark_mode_subject() {
    return dark_mode_subject_.native();
}

lv_subject_t* BaseViewModel::current_page_subject() {
    return current_page_subject_.native();
}

lv_subject_t* BaseViewModel::counter_subject() {
    return counter_subject_.native();
}

lv_subject_t* BaseViewModel::bold_text_subject() {
    return bold_text_subject_.native();
}

lv_subject_t* BaseViewModel::info_visible_subject() {
    return info_visible_subject_.native();
}

lv_subject_t* BaseViewModel::quit_requested_subject() {
    return quit_requested_subject_.native();
}

bool BaseViewModel::is_dark_mode() const {
    return model_.dark_mode();
}

void BaseViewModel::set_dark_mode(bool enabled) {
    model_.set_dark_mode(enabled);
    publish_all();
}

void BaseViewModel::toggle_dark_mode() {
    model_.toggle_dark_mode();
    publish_all();
}

model::AppPage BaseViewModel::current_page() const {
    return model_.current_page();
}

void BaseViewModel::show_apple_page() {
    model_.set_current_page(model::AppPage::Apple);
    publish_all();
}

void BaseViewModel::show_butter_page() {
    model_.set_current_page(model::AppPage::Butter);
    publish_all();
}

void BaseViewModel::toggle_page() {
    model_.toggle_page();
    publish_all();
}

void BaseViewModel::increment_counter() {
    counter_subject_.set(counter_subject_.value() + 1);
}

void BaseViewModel::decrement_counter() {
    const auto next = counter_subject_.value() - 1;
    counter_subject_.set(next < 0 ? 0 : next);
}

void BaseViewModel::toggle_bold_text() {
    bold_text_subject_.toggle();
}

void BaseViewModel::toggle_info() {
    info_visible_subject_.toggle();
}

void BaseViewModel::request_quit() {
    quit_requested_subject_.set(true);
}

bool BaseViewModel::handle_music_key(uint32_t key, bool long_pressed) {
    // Media volume keys apply to the chosen provider on either page.
    if (key == platform::kKeyMute || key == platform::kKeyVolumeDown || key == platform::kKeyVolumeUp) {
        if (!long_pressed) {
            if (key == platform::kKeyMute) mutable_music().toggle_mute();
            else mutable_music().change_volume(key == platform::kKeyVolumeUp ? 5 : -5);
            music_changed_.notify();
        }
        return true;
    }
    // Media playback keys (fn+q/w/e) work like 6/7/5 on either page.
    if (key == platform::kKeyPlayPause || key == platform::kKeyNextTrack || key == platform::kKeyPreviousTrack) {
        if (!long_pressed) {
            if (key == platform::kKeyPlayPause) mutable_music().toggle_playback();
            else if (key == platform::kKeyNextTrack) mutable_music().next();
            else mutable_music().previous();
            music_changed_.notify();
        }
        return true;
    }
    if (current_page() == model::AppPage::Butter) {
        // Consume held keys without repeating one-shot actions.
        switch (key) {
            case '4': case LV_KEY_UP: case LV_KEY_LEFT:
                if (!long_pressed) source_cursor_ = (source_cursor_ + source_count - 1) % source_count;
                break;
            case '6': case LV_KEY_DOWN: case LV_KEY_RIGHT:
                if (!long_pressed) source_cursor_ = (source_cursor_ + 1) % source_count;
                break;
            case '5': case LV_KEY_ENTER:
                if (!long_pressed) { source_index_ = source_cursor_; show_apple_page(); }
                break;
            case '7': case LV_KEY_ESC:
                if (!long_pressed) show_apple_page();
                break;
            default: return false;
        }
        if (!long_pressed) music_changed_.notify();
        return true;
    }
    switch (key) {
        case '4': if (!long_pressed) request_quit(); break;
        case '5': if (!long_pressed) mutable_music().previous(); break;
        case '6': if (!long_pressed) mutable_music().toggle_playback(); break;
        case '7': if (!long_pressed) mutable_music().next(); break;
        case '8':
            if (!long_pressed) { source_cursor_ = source_index_; show_butter_page(); }
            break;
        case LV_KEY_LEFT: if (!long_pressed) mutable_music().seek_by(-10); break;
        case LV_KEY_RIGHT: if (!long_pressed) mutable_music().seek_by(10); break;
        default: return false;
    }
    if (!long_pressed) music_changed_.notify();
    return true;
}

void BaseViewModel::publish_all() {
    title_subject_.set(model_.app_title());
    greeting_subject_.set(model_.greeting());
    dark_mode_subject_.set(model_.dark_mode());
    current_page_subject_.set(page_to_int(model_.current_page()));
}

} // namespace viewmodel
