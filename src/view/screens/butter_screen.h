/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_screen.h"
#include <array>

namespace screen {

class ButterScreen : public BaseScreen {
public:
    ButterScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);

private:
    void build_content(lv_obj_t* content) override;
    static void music_changed_cb(lv_observer_t* observer, lv_subject_t* subject);
    void refresh();
    lv_obj_t* connection_status_{nullptr};
    std::array<lv_obj_t*, viewmodel::BaseViewModel::source_count> sources_{};
};

} // namespace screen
