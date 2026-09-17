#include "apple_screen.h"
#include "asset_manager.h"
#include "base_viewmodel.h"
#include "logger.h"
#include "desktop_virtual_keypad.h"
#include "linux_input.h"
#include "screen_manager.h"
#include "help_popup.h"
#include "titlebar.h"
#include <ctime>
#include "ui_const.h"
#include <cstdlib>
#include <cstring>
#include <iostream>

int checks = 0;
void check(bool ok, const char* message) {
    ++checks;
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
lv_obj_t* label(lv_obj_t* obj, const char* text) {
    if (lv_obj_check_type(obj, &lv_label_class) && std::strcmp(lv_label_get_text(obj), text) == 0) return obj;
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); ++i)
        if (auto* found = label(lv_obj_get_child(obj, i), text)) return found;
    return nullptr;
}
lv_obj_t* label_start(lv_obj_t* obj, const char* prefix) {
    if (lv_obj_check_type(obj, &lv_label_class) &&
        std::strncmp(lv_label_get_text(obj), prefix, std::strlen(prefix)) == 0) return obj;
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); ++i)
        if (auto* found = label_start(lv_obj_get_child(obj, i), prefix)) return found;
    return nullptr;
}
lv_obj_t* artwork(lv_obj_t* obj) {
    if (lv_obj_check_type(obj, &lv_image_class)) return obj;
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); ++i)
        if (auto* found = artwork(lv_obj_get_child(obj, i))) return found;
    return nullptr;
}
lv_obj_t* bar(lv_obj_t* obj) {
    if (lv_obj_check_type(obj, &lv_bar_class)) return obj;
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); ++i)
        if (auto* found = bar(lv_obj_get_child(obj, i))) return found;
    return nullptr;
}
void check_layout(lv_obj_t* obj) {
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(obj, &lv_label_class)) {
        lv_area_t area, parent;
        lv_obj_get_coords(obj, &area);
        lv_obj_get_coords(lv_obj_get_parent(obj), &parent);
        check(area.x1 >= parent.x1 && area.y1 >= parent.y1 && area.x2 <= parent.x2 && area.y2 <= parent.y2,
              lv_label_get_text(obj));
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); ++i) check_layout(lv_obj_get_child(obj, i));
}
bool music_key(uint32_t key, const char*, bool held, void* data) {
    return static_cast<viewmodel::BaseViewModel*>(data)->handle_music_key(key, held);
}
int main() {
    logger::Logger::init();
    lv_init();
    auto* display = lv_display_create(320, 170);
    static uint8_t buffer[320 * 170 * 4];
    lv_display_set_color_format(display, LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_buffers(display, buffer, nullptr, sizeof(buffer), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, [](lv_display_t* d, const lv_area_t*, uint8_t*) { lv_display_flush_ready(d); });
    {
        app::AssetManager assets;
        viewmodel::BaseViewModel vm;
        vm.set_dark_mode(true);
        app::DesktopVirtualKeypad keypad(display);
        screen::AppleScreen screen(vm, assets);
        lv_screen_load(screen.root());
        lv_obj_update_layout(screen.root());
        for (const char* text : {"JellyZero", "money for nothing", "dire straits", "brothers in arms", "queue", "50%", "01:42 / 5:06"})
            check(label(screen.root(), text), text);
        check(lv_obj_get_style_text_font(label(screen.root(), "money for nothing"), LV_PART_MAIN)->line_height >
              lv_obj_get_style_text_font(label(screen.root(), "dire straits"), LV_PART_MAIN)->line_height,
              "title must be larger than artist in simulator");
        check(LV_USE_PERF_MONITOR == 0, "performance overlay must not obscure navbar");
        check_layout(screen.root());
        check(bar(screen.root()) && lv_bar_get_value(bar(screen.root())) == 102 &&
              lv_bar_get_max_value(bar(screen.root())) == 306, "initial bar matches elapsed / duration");
        check(label(screen.root(), view::ICON_SIGN_OUT), "exit icon exists");
        check(label(screen.root(), LV_SYMBOL_PREV), "previous icon exists");
        check(label(screen.root(), LV_SYMBOL_NEXT), "next icon exists");
        check(label(screen.root(), "dev"), "device control exists");
        auto* nav = lv_obj_get_parent(lv_obj_get_parent(label(screen.root(), "dev")));
        const char* expected_nav[] = {view::ICON_SIGN_OUT, LV_SYMBOL_PREV, LV_SYMBOL_PAUSE, LV_SYMBOL_NEXT, "dev"};
        for (int i = 0; i < 5; ++i)
            check(label(lv_obj_get_child(nav, i), expected_nav[i]), "bottom controls follow reference order");
        check(label(screen.root(), "1 walk of life"), "five-item local queue starts with next track");
        check(label_start(screen.root(), "5 "), "queue contains five items");
        check(lv_obj_get_height(label_start(screen.root(), "5 ")) <= 15,
            "queue entries truncate on one line, never wrap into adjacent rows");
        check(label(screen.root(), LV_SYMBOL_PAUSE), "playing demo offers pause");
        auto* record = artwork(screen.root());
        check(record && lv_obj_get_width(record) == 64 && lv_obj_get_height(record) == 64, "track artwork uses a square album-cover box");
        check(label(screen.root(), "Spotify / demo"), "Now Playing identifies the source and demo state");
        const std::string first_cover(static_cast<const char*>(lv_image_get_src(record)));
        keypad.emit_key('7');
        check(first_cover == static_cast<const char*>(lv_image_get_src(record)), "tracks on the same album share cover artwork");
        keypad.emit_key('7');
        check(first_cover != static_cast<const char*>(lv_image_get_src(record)), "a track with no cover replaces stale album artwork with a placeholder");
        keypad.emit_key('7');
        check(first_cover != static_cast<const char*>(lv_image_get_src(record)), "a different album changes the artwork source");
        keypad.emit_key('5'); keypad.emit_key('5'); keypad.emit_key('5');
        check(first_cover == static_cast<const char*>(lv_image_get_src(record)), "previous restores the corresponding album artwork");
        lv_tick_inc(80); lv_timer_handler();
        check(lv_image_get_rotation(record) == 0, "album artwork remains upright while playing");
        keypad.emit_key('6');
        const auto paused_angle = lv_image_get_rotation(record);
        lv_tick_inc(160); lv_timer_handler();
        check(lv_image_get_rotation(record) == paused_angle, "pause preserves artwork");
        check(label(screen.root(), LV_SYMBOL_PLAY), "6 changes navbar to play");
        check(label(screen.root(), LV_SYMBOL_PLAY), "6 pauses local playback");
        keypad.emit_key('6');
        check(label(screen.root(), LV_SYMBOL_PAUSE), "6 toggles back to pause");
        lv_tick_inc(80); lv_timer_handler();
        check(lv_image_get_rotation(record) == paused_angle, "resuming does not rotate album artwork");
        keypad.emit_key('7');
        check(label(screen.root(), "walk of life"), "7 advances fixture track");
        check(label(screen.root(), "00:00 / 4:09"), "next resets elapsed");
        check(label(screen.root(), "1 lemonade"), "queue advances with next");
        keypad.emit_key('5');
        check(label(screen.root(), "money for nothing"), "5 selects previous track");
        keypad.emit_key('5');
        check(label(screen.root(), "sultans of swing"), "previous wraps to last track");
        keypad.emit_key('7');
        check(label(screen.root(), "money for nothing"), "next wraps to first track");
        platform::set_global_key_listener(music_key, &vm);
        keypad.emit_key(LV_KEY_RIGHT);
        check(label(screen.root(), "00:10 / 5:06"), "right seeks forward 10 seconds");
        keypad.emit_key(LV_KEY_LEFT);
        keypad.emit_key(LV_KEY_LEFT);
        check(label(screen.root(), "00:00 / 5:06"), "seek clamps at zero");
        for (int i = 0; i < 40; ++i) keypad.emit_key(LV_KEY_RIGHT);
        check(vm.music().elapsed_seconds() == 306, "seek clamps at duration");
        check(vm.handle_music_key(platform::kKeyMute), "mute key is routed to music volume");
        check(vm.music().volume() == 0, "mute sets effective volume to zero");
        vm.handle_music_key(platform::kKeyMute, true);
        check(vm.music().volume() == 0, "held mute does not toggle again");
        vm.handle_music_key(platform::kKeyMute);
        check(vm.music().volume() == 50, "unmute restores prior volume");
        auto fn_key = [&](int column) {
            auto click = [&](int col, int row) {
                const int x = 8 + col * 57 + 25, y = 195 + row * 49 + 23;
                keypad.handle_pointer(x, y, true); keypad.handle_pointer(x, y, false);
            };
            click(0, 3); click(column, 2);
        };
        fn_key(1); check(vm.music().volume() == 0, "physical Fn+A mutes");
        fn_key(1); check(vm.music().volume() == 50, "physical Fn+A restores volume");
        fn_key(2); check(vm.music().volume() == 45, "physical Fn+S decreases volume");
        fn_key(3); check(vm.music().volume() == 50, "physical Fn+D increases volume");
        check(!vm.handle_music_key(LV_KEY_UP) && !vm.handle_music_key(LV_KEY_DOWN),
              "arrows no longer change playback volume");
        keypad.emit_key(platform::kKeyVolumeUp);
        check(label(lv_obj_get_parent(label(screen.root(), "money for nothing")), "55%"), "up changes local volume");
        for (int i = 0; i < 30; ++i) keypad.emit_key(platform::kKeyVolumeUp);
        check(label(lv_obj_get_parent(label(screen.root(), "money for nothing")), "100%"), "volume clamps at 100");
        for (int i = 0; i < 30; ++i) keypad.emit_key(platform::kKeyVolumeDown);
        check(label(lv_obj_get_parent(label(screen.root(), "money for nothing")), "0%"), "volume clamps at zero");
        keypad.emit_key_state('6', true);
        keypad.emit_key_state('6', true);
        vm.handle_music_key('6', true);
        check(!vm.music().playing(), "hold does not double-toggle playback");
        keypad.emit_key_state('6', false);
        keypad.emit_key('6');
        check(vm.music().playing(), "release permits next toggle");
        check(!vm.handle_music_key('9'), "unused template shortcut does nothing");
        check(!lv_subject_get_int(vm.quit_requested_subject()), "music controls never request quit");
        check(lv_bar_get_value(bar(screen.root())) == vm.music().elapsed_seconds(), "seek updates bar");
        platform::clear_global_key_listener(music_key, &vm);
        view::widgets::HelpPopup help(lv_layer_top(), vm, assets);
        help.build(); help.show(model::AppPage::Apple);
        check(label(lv_layer_top(), "Prev / Play-pause / Next"), "help documents playback controls");
        check(label(lv_layer_top(), "5 / 6 / 7"), "help documents corrected playback keys");
        check(label(lv_layer_top(), "8"), "help documents corrected device key");
        check(label(lv_layer_top(), "Fn A/S/D"), "help documents physical volume shortcuts");
        help.show(model::AppPage::Butter);
        check(label(lv_layer_top(), "Select source"), "help documents provider selection");
    }
    {
        app::AssetManager assets;
        viewmodel::BaseViewModel vm;
        app::DesktopVirtualKeypad keypad(display);
        app::ScreenManager manager(vm, assets);
        platform::set_global_key_listener(music_key, &vm);
        manager.start();
        manager.flush_requested_page();
        keypad.emit_key('8');
        manager.flush_requested_page();
        check(vm.current_page() == model::AppPage::Butter, "8 opens device selector");
        check(label(manager.current_screen(), "MUSIC SOURCE / offline preview"), "source chooser discloses offline preview");
        check(label_start(manager.current_screen(), "> Spotify"), "Spotify is the initial source choice");
        check(label_start(manager.current_screen(), "  Jellyfin"), "Jellyfin is a source, not a playback device");
        lv_obj_update_layout(manager.current_screen()); check_layout(manager.current_screen());
        auto* device_nav = lv_obj_get_parent(lv_obj_get_parent(label(manager.current_screen(), "UP")));
        for (int i = 0; i < 5; ++i) {
            auto* button = lv_obj_get_child(device_nav, i);
            lv_area_t area; lv_obj_get_coords(button, &area);
            check(!lv_obj_has_flag(button, LV_OBJ_FLAG_HIDDEN), "device navbar keeps all five physical slots");
            if (i > 0) {
                // Physical key centers: x=8 + column*57 + 25; LCD starts at155.
                const int key_center = 8 + (i + 2) * 57 + 25 - 155;
                check((area.x1 + area.x2 + 1) / 2 == key_center,
                    "device control center matches physical key 4-7, not playback navbar");
            }
        }
        auto* blank = lv_obj_get_child(device_nav, 0);
        check(!lv_obj_has_flag(blank, LV_OBJ_FLAG_CLICKABLE), "blank slot is not clickable");
        lv_area_t blank_area, back_area;
        lv_obj_get_coords(blank, &blank_area);
        lv_obj_get_coords(lv_obj_get_child(device_nav, 4), &back_area);
        check(blank_area.x1 > back_area.x2, "unused slot is to the right, never before key4");
        const char* device_labels[] = {"UP", "SELECT", "DOWN", "BACK"};
        for (int i = 0; i < 4; ++i) {
            lv_area_t text_area; lv_obj_get_coords(label(manager.current_screen(), device_labels[i]), &text_area);
            const int key_center = 8 + (i + 3) * 57 + 25 - 155;
            check(std::abs((text_area.x1 + text_area.x2 + 1) / 2 - key_center) <= 1,
                "visible device label is centered over its physical key");
        }
        lv_obj_send_event(blank, LV_EVENT_CLICKED, nullptr);
        check(!lv_subject_get_int(vm.quit_requested_subject()), "blank slot cannot quit even via a synthetic click");
        // Click the actual simulator key6 hitbox, not a navbar-relative proxy.
        keypad.handle_pointer(8 + 5 * 57 + 25, 195 + 23, true);
        keypad.handle_pointer(8 + 5 * 57 + 25, 195 + 23, false);
        check(vm.source_cursor() == 1, "physical key6 selects next device");
        keypad.handle_pointer(8 + 4 * 57 + 25, 195 + 23, true);
        keypad.handle_pointer(8 + 4 * 57 + 25, 195 + 23, false);
        manager.flush_requested_page();
        check(std::strcmp(vm.source_name(), "Jellyfin") == 0, "selected provider is Jellyfin");
        check(label(manager.current_screen(), "Jellyfin / demo"), "source badge follows provider selection");
        keypad.emit_key(platform::kKeyVolumeDown);
        keypad.emit_key('8'); manager.flush_requested_page();
        const auto cursor_before_volume = vm.source_cursor();
        keypad.emit_key(platform::kKeyMute);
        check(vm.source_cursor() == cursor_before_volume && vm.music().volume() == 0,
              "volume controls act on current source without moving chooser cursor");
        keypad.emit_key(platform::kKeyMute);
        keypad.emit_key('4'); keypad.emit_key('5'); manager.flush_requested_page();
        check(vm.source_index() == 0 && vm.music().volume() == 50, "Spotify retains its own preview volume");
        keypad.emit_key('8'); manager.flush_requested_page();
        keypad.emit_key('6'); keypad.emit_key('5'); manager.flush_requested_page();
        check(vm.source_index() == 1 && vm.music().volume() == 45, "Jellyfin retains its own preview volume");
        keypad.emit_key(platform::kKeyVolumeUp);
        keypad.emit_key('8'); manager.flush_requested_page();
        keypad.emit_key(LV_KEY_DOWN);
        keypad.emit_key(LV_KEY_ESC); manager.flush_requested_page();
        check(std::strcmp(vm.source_name(), "Jellyfin") == 0, "ESC cancels provider change");
        check(label(manager.current_screen(), "01:42 / 5:06"), "page changes preserve playback position");
        for (int i = 0; i < 20; ++i) {
            keypad.emit_key('8'); manager.flush_requested_page();
            keypad.emit_key('4'); keypad.emit_key(LV_KEY_ENTER); manager.flush_requested_page();
        }
        check(vm.current_page() == model::AppPage::Apple, "repeated screen replacement is safe");
        // Production runs lv_timer_handler(), which executes and frees the
        // lv_async_call queued by ScreenManager. Manually flushing every page
        // without pumping LVGL instead accumulates one pending timer per switch.
        auto pump_page = [&]() {
            lv_tick_inc(40);
            lv_timer_handler();
            lv_refr_now(display); // Include rendered-page caches in the sample.
        };
        auto timer_count = []() {
            unsigned count = 0;
            for (auto* timer = lv_timer_get_next(nullptr); timer; timer = lv_timer_get_next(timer)) ++count;
            return count;
        };
        pump_page(); // Drain the earlier synchronous warmup before the baseline.
        const auto timers_before = timer_count();
        lv_mem_monitor_t before{}, after{};
        lv_mem_monitor(&before);
        for (int i = 0; i < 500; ++i) {
            keypad.emit_key('8'); pump_page();
            check(label(manager.current_screen(), "MUSIC SOURCE / offline preview"),
                  "async churn loads the device screen");
            keypad.emit_key('7'); pump_page();
            check(label(manager.current_screen(), "money for nothing"),
                  "async churn returns to the player screen");
            check(timer_count() == timers_before, "page round trips do not retain LVGL timers");
            lv_mem_monitor(&after);
            check(after.free_size + 16384 >= before.free_size,
                  "warm-cache page round trips do not grow LVGL heap by more than 16 KiB");
        }
        std::cout << "LVGL page churn: before=" << (before.total_size - before.free_size)
                  << " after=" << (after.total_size - after.free_size)
                  << " peak=" << after.max_used << " bytes, 500 round trips; timers="
                  << timers_before << " -> " << timer_count() << std::endl;
        check(lv_mem_test() == LV_RESULT_OK, "LVGL heap integrity survives page churn");

        lv_tick_inc(80); lv_timer_handler();
        check(lv_image_get_rotation(artwork(manager.current_screen())) == 0, "artwork remains upright after screen replacement");
        platform::clear_global_key_listener(music_key, &vm);
        keypad.emit_key('6');
        check(!vm.music().playing(), "new navbar remains registered after old screen destruction");
        lv_obj_send_event(lv_obj_get_parent(label(manager.current_screen(), "dev")), LV_EVENT_CLICKED, nullptr);
        manager.flush_requested_page();
        check(vm.current_page() == model::AppPage::Butter, "clicking dev opens ButterScreen");
        vm.show_apple_page(); manager.flush_requested_page();
        lv_obj_send_event(lv_obj_get_parent(label(manager.current_screen(), view::ICON_SIGN_OUT)), LV_EVENT_CLICKED, nullptr);
        check(lv_subject_get_int(vm.quit_requested_subject()), "clicking exit requests app shutdown");
        platform::clear_global_key_listener(music_key, &vm);
    }
    // Exercise the real physical-key hitboxes through both dispatch paths.
    for (bool global : {false, true}) {
        app::AssetManager assets;
        viewmodel::BaseViewModel vm;
        app::DesktopVirtualKeypad keypad(display);
        app::ScreenManager manager(vm, assets);
        if (global) platform::set_global_key_listener(music_key, &vm);
        manager.start(); manager.flush_requested_page();
        auto physical = [&](int number) {
            const int x = 8 + (number - 1) * 57 + 25;
            keypad.handle_pointer(x, 218, true);
            keypad.handle_pointer(x, 218, false);
            manager.flush_requested_page();
        };
        lv_obj_update_layout(manager.current_screen());
        auto* nav = lv_obj_get_parent(lv_obj_get_parent(label(manager.current_screen(), "dev")));
        for (int i = 0; i < 5; ++i) {
            lv_area_t area; lv_obj_get_coords(lv_obj_get_child(nav, i), &area);
            check((area.x1 + area.x2 + 1) / 2 == 8 + (i + 3) * 57 + 25 - 155,
                "AppleScreen icons align to physical keys4-8");
        }
        physical(5);
        check(std::strcmp(vm.music().current_track().title, "SULTANS OF SWING") == 0, "physical5 is previous");
        physical(6); check(!vm.music().playing(), "physical6 pauses");
        physical(6); check(vm.music().playing(), "physical6 resumes");
        physical(7);
        check(std::strcmp(vm.music().current_track().title, "MONEY FOR NOTHING") == 0, "physical7 is next");
        physical(8); check(vm.current_page() == model::AppPage::Butter, "physical8 opens devices");
        const auto cursor = vm.source_cursor();
        physical(8); check(vm.current_page() == model::AppPage::Butter && vm.source_cursor() == cursor,
            "device blank key8 has no action");
        physical(4); check(!lv_subject_get_int(vm.quit_requested_subject()), "device key4 moves selection, never exits");
        physical(5); check(vm.current_page() == model::AppPage::Apple, "device key5 still confirms");
        physical(8); physical(7); check(vm.current_page() == model::AppPage::Apple, "device key7 still goes back");
        physical(6); check(!vm.music().playing(), "key6 remains registered after round trips");
        physical(4); check(lv_subject_get_int(vm.quit_requested_subject()), "physical4 exits AppleScreen");
        platform::clear_global_key_listener(music_key, &vm);
    }
    // An unchanged header must not keep redrawing an idle LCD.
    {
        app::AssetManager assets;
        viewmodel::BaseViewModel vm;
        auto* root = lv_obj_create(nullptr);
        lv_screen_load(root);
        {
            view::widgets::TitleBar header(root, vm, assets);
            header.build();
            lv_obj_update_layout(root);
            lv_refr_now(display);
            int flushes = 0;
            lv_display_set_user_data(display, &flushes);
            lv_display_set_flush_cb(display, [](lv_display_t* d, const lv_area_t*, uint8_t*) {
                ++*static_cast<int*>(lv_display_get_user_data(d));
                lv_display_flush_ready(d);
            });
            const auto minute_before = std::time(nullptr) / 60;
            for (int i = 0; i < 5; ++i) {
                lv_tick_inc(1000); lv_timer_handler(); lv_refr_now(display);
            }
            if (minute_before == std::time(nullptr) / 60)
                check(flushes == 0, "unchanged clock and device status produce no LCD flushes");
            lv_display_set_user_data(display, nullptr);
            lv_display_set_flush_cb(display, [](lv_display_t* d, const lv_area_t*, uint8_t*) { lv_display_flush_ready(d); });
        }
        auto* empty = lv_obj_create(nullptr); lv_screen_load(empty);
        lv_obj_delete(root);
    }
    lv_display_delete(display);
    std::cout << "PASS: " << checks << " checks (metadata, layout, controls, bounds, devices, lifetimes, help)" << std::endl;
}
