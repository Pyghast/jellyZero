/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "user_config.h"

#include <cstdlib>

#ifndef APP_CONFIG_FILE
#define APP_CONFIG_FILE "template-app.conf"
#endif

namespace platform {

std::filesystem::path user_config_dir() {
#if USE_DESKTOP
    return std::filesystem::path(APP_CONFIG_FILE).parent_path();
#else
    if (const char* xdg_config_home = std::getenv("XDG_CONFIG_HOME")) {
        const std::filesystem::path root(xdg_config_home);
        if (!root.empty() && root.is_absolute()) {
            return root / "template-app";
        }
    }
    if (const char* home = std::getenv("HOME")) {
        const std::filesystem::path root(home);
        if (!root.empty() && root.is_absolute()) {
            return root / ".config" / "template-app";
        }
    }
    return std::filesystem::path(APP_CONFIG_FILE).parent_path();
#endif
}

} // namespace platform
