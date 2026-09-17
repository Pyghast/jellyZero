/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <filesystem>

namespace platform {

// Per-user writable config directory: $XDG_CONFIG_HOME/template-app or
// ~/.config/template-app on device; the repo's config/ directory on desktop
// builds. Callers create the directory before writing into it if needed.
std::filesystem::path user_config_dir();

} // namespace platform
