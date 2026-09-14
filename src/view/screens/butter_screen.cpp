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
    lv_label_set_text(heading, "DEMO DEVICES / no Spotify connection");
    lv_obj_set_pos(heading, 10, 5);
    auto* small = assets().load_standard_font(10);
    lv_obj_set_style_text_font(heading, small ? small : &lv_font_montserrat_10, 0);
    reactive::bind_theme(heading, view_model().dark_mode_subject(), reactive::ThemeRole::Text);
    auto* font = assets().load_standard_font(13, app::StandardFontWeight::Regular);
    for (size_t i = 0; i < devices_.size(); ++i) {
        devices_[i] = lv_label_create(content);
        lv_obj_set_pos(devices_[i], 10, 25 + static_cast<int>(i) * 25);
        lv_obj_set_width(devices_[i], 300);
        lv_label_set_long_mode(devices_[i], LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_font(devices_[i], font ? font : &lv_font_montserrat_12, 0);
        reactive::bind_theme(devices_[i], view_model().dark_mode_subject(), reactive::ThemeRole::Text);
    }
    reactive::observe_obj(content, view_model().music_subject(), music_changed_cb, this);
}

void ButterScreen::refresh() {
    for (size_t i = 0; i < devices_.size(); ++i) {
        const auto index = static_cast<int>(i);
        lv_label_set_text_fmt(devices_[i], "%s %s%s",
            index == view_model().device_cursor() ? ">" : " ",
            model::FakeMusicProvider::device_name(index),
            index == view_model().music().device_index() ? " [active]" : "");
    }
}

void ButterScreen::music_changed_cb(lv_observer_t* observer, lv_subject_t*) {
    static_cast<ButterScreen*>(lv_observer_get_user_data(observer))->refresh();
}
} // namespace screen
