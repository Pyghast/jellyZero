/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_screen.h"
#include <array>
#include <string>

namespace screen {

class AppleScreen : public BaseScreen {
public:
    AppleScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);

private:
    void build_content(lv_obj_t* content) override;
    void refresh();
    static void music_changed_cb(lv_observer_t* observer, lv_subject_t* subject);
    lv_obj_t* title_{nullptr};
    lv_obj_t* artist_{nullptr};
    lv_obj_t* album_{nullptr};
    lv_obj_t* elapsed_{nullptr};
    std::array<lv_obj_t*, model::MusicProvider::queue_size> queue_{};
    lv_obj_t* artwork_{nullptr};
    lv_obj_t* source_badge_{nullptr};
    std::string artwork_source_;
    std::string artwork_key_;
    lv_obj_t* progress_{nullptr};
    lv_obj_t* status_{nullptr};
};

} // namespace screen
