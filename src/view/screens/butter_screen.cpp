/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */
#include "butter_screen.h"
#include "asset_manager.h"
#include "bindings.h"

namespace screen {
ButterScreen::ButterScreen(viewmodel::BaseViewModel& vm, app::AssetManager& assets)
    : BaseScreen(vm, assets) { init(); }

void ButterScreen::build_content(lv_obj_t* content) {
    auto* heading = lv_label_create(content);
    lv_label_set_text(heading, "MUSIC SOURCE / offline preview");
    lv_obj_set_pos(heading, 10, 5);
    auto* small = assets().load_standard_font(10);
    lv_obj_set_style_text_font(heading, small ? small : &lv_font_montserrat_10, 0);
    reactive::bind_theme(heading, view_model().dark_mode_subject(), reactive::ThemeRole::Text);
    auto* font = assets().load_standard_font(13, app::StandardFontWeight::Regular);
    for (size_t i = 0; i < sources_.size(); ++i) {
        sources_[i] = lv_label_create(content);
        lv_obj_set_pos(sources_[i], 10, 25 + static_cast<int>(i) * 25);
        lv_obj_set_width(sources_[i], 300);
        lv_label_set_long_mode(sources_[i], LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_font(sources_[i], font ? font : &lv_font_montserrat_12, 0);
        reactive::bind_theme(sources_[i], view_model().dark_mode_subject(), reactive::ThemeRole::Text);
    }
    connection_status_ = lv_label_create(content);
    lv_obj_set_pos(connection_status_, 10, 84);
    lv_obj_set_width(connection_status_, 300);
    lv_label_set_long_mode(connection_status_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(connection_status_, small ? small : &lv_font_montserrat_10, 0);
    reactive::bind_theme(connection_status_, view_model().dark_mode_subject(), reactive::ThemeRole::Text);
    reactive::observe_obj(content, view_model().music_subject(), music_changed_cb, this);
}

void ButterScreen::refresh() {
    if (view_model().source_cursor() == 0) {
        const auto status = view_model().spotify_status_text();
        lv_label_set_text(connection_status_, status.c_str());
    }
    else {
        lv_label_set_text(connection_status_, "Jellyfin: server not configured");
    }
    for (size_t i = 0; i < sources_.size(); ++i) {
        const auto index = static_cast<int>(i);
        lv_label_set_text_fmt(sources_[i], "%s %s%s",
            index == view_model().source_cursor() ? ">" : " ",
            viewmodel::BaseViewModel::source_name(index),
            index == view_model().source_index() ? " [selected]" : "");
    }
}

void ButterScreen::music_changed_cb(lv_observer_t* observer, lv_subject_t*) {
    static_cast<ButterScreen*>(lv_observer_get_user_data(observer))->refresh();
}
} // namespace screen
