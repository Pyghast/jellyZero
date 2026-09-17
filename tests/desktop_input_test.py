"""Standalone desktop input translation regression (no CMake build or SDL window).

Compile production functions verbatim against real LVGL/SDL headers. This checks
Fn translations, log names and routing with a minimal clock/timer/nav fixture.
This is not pointer hit-testing, an SDL event loop, or hardware delivery.
"""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--lvgl-dir", required=True, type=Path)
parser.add_argument("--sdl-dir", required=True, type=Path)
parser.add_argument("--cxx", default="g++")
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
source = (root / "src/app/desktop_virtual_keypad.cpp").read_text()
platform_source = (root / "src/platform/linux_input.cpp").read_text()


def function(text, signature):
    match = re.search(re.escape(signature) + r" \{.*?^\}", text, re.M | re.S)
    if not match:
        raise RuntimeError("Cannot locate " + signature)
    return match.group(0)


harness = r'''
#include "linux_input.h"
#include <SDL_keycode.h>
#include <cstdio>
#include <string>
#include <utility>
#include <initializer_list>
#include <vector>
''' + function(source, "uint32_t fn_key_for(char key)") + "\n" + function(source, "std::string output_key_name(uint32_t key)") + r'''
// Only the unavailable LVGL clock/timer/nav environment is substituted. The
// production route, global listener invocation and descriptions run verbatim.
uint32_t lv_tick_get() { return 100; }
void lv_timer_pause(lv_timer_t*) {}
namespace platform {
uint32_t last_key = 0;
bool last_key_pressed = false;
uint32_t pressed_key = 0;
uint32_t press_started_at = 0;
bool long_press_sent = false;
bool pressed_key_consumed = false;
lv_timer_t* long_press_timer = nullptr;
KeyReleaseListener key_release_listener = nullptr;
void* key_release_listener_user_data = nullptr;
GlobalKeyListener global_key_listener = nullptr;
void* global_key_listener_user_data = nullptr;
void ensure_long_press_timer() {}
void dispatch_nav_key(uint32_t) {}
''' + function(platform_source, "const char* describe_key(uint32_t key)") + "\n" + function(platform_source, "bool emit_global_key(uint32_t key, bool long_pressed)") + "\n" + function(platform_source, "void route_key_state(uint32_t key, bool pressed)") + r'''
}
std::vector<std::pair<uint32_t, std::string>> pressed_events;
std::vector<std::pair<uint32_t, std::string>> released_events;
bool capture_press(uint32_t key, const char* name, bool, void*) {
    pressed_events.emplace_back(key, name);
    return true;
}
void capture_release(uint32_t key, const char* name, void*) {
    released_events.emplace_back(key, name);
}
int main() {
    int failures = 0;
    static_assert(platform::kKeyMute == (platform::kSpecialKeyBase | 20U));
    static_assert(platform::kKeyVolumeDown == (platform::kSpecialKeyBase | 21U));
    static_assert(platform::kKeyVolumeUp == (platform::kSpecialKeyBase | 22U));
    const std::pair<char, uint32_t> mappings[] = {
        {'a', platform::kKeyMute}, {'s', platform::kKeyVolumeDown}, {'d', platform::kKeyVolumeUp},
        {'1', SDLK_F1}, {'4', SDLK_F4}, {'8', SDLK_F8}, {'0', SDLK_F10},
        {'q', SDLK_AUDIOPLAY}, {'w', SDLK_AUDIOPREV}, {'e', SDLK_AUDIONEXT},
        {'u', SDLK_BRIGHTNESSDOWN}, {'i', SDLK_BRIGHTNESSUP},
        {'f', LV_KEY_UP}, {'z', LV_KEY_LEFT}, {'x', LV_KEY_DOWN}, {'c', LV_KEY_RIGHT},
        {'h', platform::kKeyHelp}, {'j', platform::kKeyPrintScreen},
    };
    for (const auto& [key, expected] : mappings) {
        const auto actual = fn_key_for(key);
        if (actual != expected) {
            std::fprintf(stderr, "FAIL: Fn+%c -> %u; expected %u\n", key, actual, expected);
            ++failures;
        }
    }
    const std::pair<uint32_t, const char*> names[] = {
        {platform::kKeyMute, "mute"}, {platform::kKeyVolumeDown, "volume-down"},
        {platform::kKeyVolumeUp, "volume-up"},
    };
    for (const auto& [key, expected] : names) {
        if (output_key_name(key) != expected) {
            std::fprintf(stderr, "FAIL: output name %u -> %s; expected %s\n",
                         key, output_key_name(key).c_str(), expected);
            ++failures;
        }
    }
    platform::global_key_listener = capture_press;
    platform::key_release_listener = capture_release;
    const std::pair<uint32_t, uint32_t> routed[] = {
        {SDLK_MUTE, platform::kKeyMute}, {SDLK_AUDIOMUTE, platform::kKeyMute},
        {SDLK_VOLUMEDOWN, platform::kKeyVolumeDown}, {SDLK_VOLUMEUP, platform::kKeyVolumeUp},
        {platform::kKeyMute, platform::kKeyMute},
        {platform::kKeyVolumeDown, platform::kKeyVolumeDown},
        {platform::kKeyVolumeUp, platform::kKeyVolumeUp},
        {'4', '4'}, {'5', '5'}, {'6', '6'}, {'7', '7'}, {'8', '8'},
        {LV_KEY_ENTER, LV_KEY_ENTER}, {LV_KEY_LEFT, LV_KEY_LEFT}, {LV_KEY_RIGHT, LV_KEY_RIGHT},
        {SDLK_BRIGHTNESSDOWN, SDLK_BRIGHTNESSDOWN}, {SDLK_BRIGHTNESSUP, SDLK_BRIGHTNESSUP},
    };
    for (const auto& [key, expected] : routed) {
        pressed_events.clear();
        released_events.clear();
        platform::route_key_state(key, true);
        platform::route_key_state(key, true); // Held poll must not duplicate an action.
        platform::route_key_state(key, false);
        if (pressed_events.size() != 1 || released_events.size() != 1 ||
            pressed_events.front().first != expected || released_events.front().first != expected) {
            std::fprintf(stderr, "FAIL: route %u -> pressed %u, released %u; expected %u once each\n",
                         key, pressed_events.empty() ? 0 : pressed_events.front().first,
                         released_events.empty() ? 0 : released_events.front().first, expected);
            ++failures;
        }
    }
    const std::pair<uint32_t, const char*> descriptions[] = {
        {platform::kKeyMute, "Mute"}, {platform::kKeyVolumeDown, "VolumeDown"},
        {platform::kKeyVolumeUp, "VolumeUp"},
    };
    for (const auto& [key, expected] : descriptions) {
        if (std::string(platform::describe_key(key)) != expected) {
            std::fprintf(stderr, "FAIL: describe %u -> %s; expected %s\n",
                         key, platform::describe_key(key), expected);
            ++failures;
        }
    }
    if (!failures) std::puts("PASS: Fn/SDL volume mappings, names, listener press/release and held-key deduplication; existing shortcuts unchanged");
    return failures ? 1 : 0;
}
'''
with tempfile.TemporaryDirectory(prefix="jellyzero-desktop-input-") as directory:
    test_source = Path(directory) / "desktop_input.cpp"
    executable = Path(directory) / "desktop_input.exe"
    test_source.write_text(harness)
    subprocess.run([
        args.cxx, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-DLV_CONF_SKIP=1", "-DUSE_DESKTOP=1",
        "-I", str(args.lvgl_dir.resolve()), "-I", str(args.sdl_dir.resolve()),
        "-I", str(root / "src/platform"), str(test_source), "-o", str(executable),
    ], check=True)
    raise SystemExit(subprocess.run([str(executable)]).returncode)
