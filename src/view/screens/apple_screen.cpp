/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */
#include "apple_screen.h"
#include "asset_manager.h"
#include "bindings.h"
#include <algorithm>
#include <cctype>
#include <string>

namespace screen {
namespace {
std::string lowercase(const char* text) {
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}
}

AppleScreen::AppleScreen(viewmodel::BaseViewModel& vm, app::AssetManager& assets)
    : BaseScreen(vm, assets) { init(); }

void AppleScreen::build_content(lv_obj_t* content) {
    // Reference layout: 216px player, divider, 92px queue in the 110px body.
    auto make_label = [&](const char* text, int x, int y, int width, int size) {
        auto* obj = lv_label_create(content);
        lv_label_set_text(obj, text);
        lv_obj_set_pos(obj, x, y);
        lv_obj_set_width(obj, width);
        lv_label_set_long_mode(obj, LV_LABEL_LONG_DOT);
        auto* font = assets().load_standard_font(size, app::StandardFontWeight::Regular);
        const lv_font_t* fallback = size >= 20 ? &lv_font_montserrat_20
            : size >= 14 ? &lv_font_montserrat_14
            : size >= 11 ? &lv_font_montserrat_12 : &lv_font_montserrat_10;
        const auto* chosen_font = font ? font : fallback;
        lv_obj_set_style_text_font(obj, chosen_font, 0);
        lv_obj_set_height(obj, chosen_font->line_height);
        reactive::bind_theme(obj, view_model().dark_mode_subject(), reactive::ThemeRole::Text);
        return obj;
    };
    title_ = make_label("", 9, 6, 204, 20);
    artist_ = make_label("", 9, 30, 111, 14);
    album_ = make_label("", 9, 46, 111, 10);
    status_ = make_label("", 10, 58, 110, 10);
    elapsed_ = make_label("", 10, 91, 103, 10);
    lv_obj_set_style_text_align(elapsed_, LV_TEXT_ALIGN_CENTER, 0);

    progress_ = lv_bar_create(content);
    lv_obj_set_pos(progress_, 10, 71);
    lv_obj_set_size(progress_, 102, 12);
    lv_obj_set_style_radius(progress_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_radius(progress_, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(progress_, lv_color_hex(0xe4e4e8), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(progress_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(progress_, lv_color_hex(0x483cde), LV_PART_INDICATOR);

    artwork_ = lv_image_create(content);
    lv_obj_set_pos(artwork_, 128, 33);
    lv_obj_set_size(artwork_, 64, 64);
    source_badge_ = make_label("", 125, 98, 88, 10);

    auto* divider = lv_obj_create(content);
    lv_obj_remove_style_all(divider);
    lv_obj_set_pos(divider, 217, 4);
    lv_obj_set_size(divider, 1, 100);
    lv_obj_set_style_bg_color(divider, lv_color_hex(0xaaaaB6), 0);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);
    lv_obj_remove_flag(divider, LV_OBJ_FLAG_SCROLLABLE);
    auto* heading = make_label("queue", 226, 3, 86, 20);
    lv_obj_set_style_text_align(heading, LV_TEXT_ALIGN_RIGHT, 0);
    for (size_t i = 0; i < queue_.size(); ++i)
        queue_[i] = make_label("", 227, 26 + static_cast<int>(i) * 15, 86, 11);
    reactive::observe_obj(content, view_model().music_subject(), music_changed_cb, this);
}

void AppleScreen::refresh() {
    const auto& music = view_model().music();
    const auto& track = music.current_track();
    lv_label_set_text_fmt(source_badge_, "%s / demo", view_model().source_name());
    // Resolve/decode only when track metadata changes, not on every volume key.
    if (artwork_source_.empty() || artwork_key_ != track.artwork_path) {
        artwork_key_ = track.artwork_path;
        auto cover = artwork_key_.empty() ? std::filesystem::path{} : assets().resolve(artwork_key_);
        if (cover.empty()) cover = assets().resolve("images/no-cover.png");
        const auto source = "A:" + cover.generic_string();
        if (source != artwork_source_) {
            artwork_source_ = source;
            lv_image_set_src(artwork_, artwork_source_.c_str());
        }
    }
    lv_label_set_text(title_, lowercase(track.title).c_str());
    lv_label_set_text(artist_, lowercase(track.artist).c_str());
    lv_label_set_text(album_, lowercase(track.album).c_str());
    lv_label_set_text_fmt(elapsed_, "%02d:%02d / %d:%02d", music.elapsed_seconds() / 60,
        music.elapsed_seconds() % 60, track.duration_seconds / 60, track.duration_seconds % 60);
    lv_bar_set_range(progress_, 0, track.duration_seconds);
    lv_bar_set_value(progress_, music.elapsed_seconds(), LV_ANIM_OFF);
    lv_label_set_text_fmt(status_, "%d%%", music.volume());
    for (size_t i = 0; i < queue_.size(); ++i)
        lv_label_set_text_fmt(queue_[i], "%u %s", static_cast<unsigned>(i + 1),
            lowercase(music.queued_track(i).title).c_str());
}

void AppleScreen::music_changed_cb(lv_observer_t* observer, lv_subject_t*) {
    static_cast<AppleScreen*>(lv_observer_get_user_data(observer))->refresh();
}
} // namespace screen
