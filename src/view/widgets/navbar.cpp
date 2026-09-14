/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */
#include "navbar.h"
#include "asset_manager.h"
#include "bindings.h"
#include "linux_input.h"
#include "theme.h"
#include "ui_const.h"

namespace view::widgets {
NavBar::NavBar(lv_obj_t* parent, viewmodel::BaseViewModel& vm, app::AssetManager& assets)
    : BaseWidgets(parent), view_model_(vm), assets_(assets) {}

NavBar::~NavBar() {
    if (music_observer_) lv_observer_remove(music_observer_);
    if (page_observer_) lv_observer_remove(page_observer_);
    for (size_t i = 0; i < buttons_.size(); ++i)
        if (buttons_[i]) platform::unregister_nav_button(registered_slots_[i], buttons_[i]->root());
}

void NavBar::build() {
    if (core_obj_) return;
    core_obj_ = lv_obj_create(parent_);
    lv_obj_remove_style_all(core_obj_);
    lv_obj_set_size(core_obj_, LV_PCT(100), kNavBarHeight);
    lv_obj_align(core_obj_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_clear_flag(core_obj_, LV_OBJ_FLAG_SCROLLABLE);
    reactive::bind_theme(core_obj_, view_model_.dark_mode_subject(), reactive::ThemeRole::Bar);
    lv_obj_set_flex_flow(core_obj_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(core_obj_, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    exit_font_ = assets_.load_font("Phosphor-Fill.ttf", 20);
    text_font_ = assets_.load_standard_font(12, app::StandardFontWeight::Regular);
    if (!text_font_) text_font_ = &lv_font_montserrat_12;
    for (size_t i = 0; i < buttons_.size(); ++i) {
        buttons_[i] = std::make_unique<IconButton>(core_obj_, view_model_, 62, 28, "",
            &lv_font_montserrat_20,
            palette(false).text, palette(true).text, click_cb, this, true);
        buttons_[i]->build();
    }
    platform::set_nav_shortcut_mode(false);
    music_observer_ = reactive::observe_obj(core_obj_, view_model_.music_subject(), changed_cb, this);
    page_observer_ = reactive::observe_obj(core_obj_, view_model_.current_page_subject(), changed_cb, this);
}

void NavBar::refresh() {
    const bool devices = view_model_.current_page() == model::AppPage::Butter;
    const std::array<const char*, 5> playback_labels = {
        exit_font_ ? ICON_SIGN_OUT : LV_SYMBOL_CLOSE, LV_SYMBOL_PREV,
        view_model_.music().playing() ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY, LV_SYMBOL_NEXT, "dev"};
    const std::array<const char*, 5> device_labels = {"", "UP", "SELECT", "DOWN", "BACK"};
    const auto& labels = devices ? device_labels : playback_labels;
    // Clear only our old registrations before installing this page's key mapping.
    for (size_t i = 0; i < buttons_.size(); ++i)
        platform::unregister_nav_button(registered_slots_[i], buttons_[i]->root());
    for (size_t i = 0; i < buttons_.size(); ++i) {
        auto& button = buttons_[i];
        // Key4 starts at LCD x49, subsequent physical keys have a57px pitch.
        const size_t slot = devices ? (i == 0 ? 4 : i - 1) : i;
        registered_slots_[i] = slot;
        platform::register_nav_button(slot, button->root());
        lv_obj_add_flag(button->root(), LV_OBJ_FLAG_IGNORE_LAYOUT);
        const int center = 49 + static_cast<int>(slot) * 57;
        lv_obj_set_pos(button->root(), center - 28 - lv_obj_get_style_border_width(core_obj_, LV_PART_MAIN), 0);
        lv_obj_remove_flag(button->root(), LV_OBJ_FLAG_HIDDEN);
        button->set_enabled(!(devices && i == 0));
        lv_obj_set_width(button->root(), 56);
        const lv_font_t* font = devices ? &lv_font_montserrat_10 : i == 4 ? text_font_ : &lv_font_montserrat_20;
        if (!devices && i == 0 && exit_font_) font = exit_font_;
        button->set_font(font);
        button->set_text(labels[i]);
    }
}

void NavBar::click_cb(lv_event_t* event) {
    // ENTER is handled by the music key router. LVGL also synthesizes a
    // click on the focused button on release; do not execute a second action.
    auto* input = lv_indev_active();
    if (input && lv_indev_get_type(input) == LV_INDEV_TYPE_KEYPAD &&
        lv_indev_get_key(input) == LV_KEY_ENTER) return;
    auto* bar = static_cast<NavBar*>(lv_event_get_user_data(event));
    for (size_t i = 0; i < bar->buttons_.size(); ++i)
        if (lv_event_get_current_target(event) == bar->buttons_[i]->root())
        {
            const bool devices = bar->view_model_.current_page() == model::AppPage::Butter;
            if (devices && i == 0) return; // Unused key8 slot.
            const auto key = static_cast<uint32_t>('4' + (devices ? i - 1 : i));
            bar->view_model_.handle_music_key(key);
            return;
        }
}

void NavBar::changed_cb(lv_observer_t* observer, lv_subject_t*) {
    static_cast<NavBar*>(lv_observer_get_user_data(observer))->refresh();
}
} // namespace view::widgets
