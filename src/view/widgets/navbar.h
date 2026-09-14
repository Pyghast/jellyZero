/* SPDX-License-Identifier: MIT */
#pragma once
#include "base_widget.h"
#include "base_viewmodel.h"
#include "icon_button.h"
#include <array>
#include <memory>

namespace app { class AssetManager; }
namespace view::widgets {
class NavBar : public BaseWidgets {
public:
    NavBar(lv_obj_t* parent, viewmodel::BaseViewModel& view_model, app::AssetManager& assets);
    ~NavBar() override;
    void build() override;
private:
    void refresh();
    static void click_cb(lv_event_t* event);
    static void changed_cb(lv_observer_t* observer, lv_subject_t* subject);
    viewmodel::BaseViewModel& view_model_;
    app::AssetManager& assets_;
    std::array<std::unique_ptr<IconButton>, 5> buttons_;
    std::array<size_t, 5> registered_slots_{{0, 1, 2, 3, 4}};
    lv_font_t* exit_font_{nullptr};
    const lv_font_t* text_font_{nullptr};
    lv_observer_t* music_observer_{nullptr};
    lv_observer_t* page_observer_{nullptr};
};
} // namespace view::widgets
