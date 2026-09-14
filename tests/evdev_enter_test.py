"""Run with: python tests/evdev_enter_test.py --lvgl-dir <lvgl source> [--cxx g++]

Compile the actual map_evdev_key function in isolation, without exposing a test API
or requiring the Linux device backend on Windows. Linux input-event ABI constants
below are test fixtures; LVGL and application constants come from their real headers.
This checks translation only, not evdev reads, routing, or hardware delivery.
"""

import argparse
from pathlib import Path
import re
import subprocess
import tempfile


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--lvgl-dir", required=True, type=Path)
parser.add_argument("--cxx", default="g++")
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
source = (root / "src/platform/linux_input.cpp").read_text()
# Extract the existing file-local function verbatim, not a copy of its logic.
match = re.search(r"uint32_t map_evdev_key\(uint16_t code\) \{.*?^\}", source, re.M | re.S)
if not match:
    raise RuntimeError("Cannot locate map_evdev_key in linux_input.cpp")

harness = r"""
#include "linux_input.h"
#include <cstdio>
#include <initializer_list>
// Stable Linux UAPI values from linux/input-event-codes.h.
enum : uint16_t {
    KEY_ESC = 1, KEY_4 = 5, KEY_5 = 6, KEY_6 = 7, KEY_7 = 8, KEY_8 = 9,
    KEY_ENTER = 28, KEY_Z = 44, KEY_C = 46, KEY_KPENTER = 96, KEY_SYSRQ = 99,
    KEY_UP = 103, KEY_LEFT = 105, KEY_RIGHT = 106, KEY_DOWN = 108, KEY_HELP = 138
};
namespace platform {
""" + match.group(0) + r"""
}
int main() {
    int failures = 0;
    for (const auto code : {KEY_ENTER, KEY_KPENTER}) {
        const auto actual = platform::map_evdev_key(code);
        if (actual != LV_KEY_ENTER) {
            std::fprintf(stderr, "FAIL: raw evdev %u -> %u; expected LV_KEY_ENTER (%u)\n",
                         unsigned(code), unsigned(actual), unsigned(LV_KEY_ENTER));
            ++failures;
        }
    }
    if (!failures) std::puts("PASS: KEY_ENTER (28) and KEY_KPENTER (96) -> LV_KEY_ENTER");
    return failures ? 1 : 0;
}
"""
with tempfile.TemporaryDirectory(prefix="jellyzero-evdev-enter-") as directory:
    test_source = Path(directory) / "evdev_enter.cpp"
    executable = Path(directory) / "evdev_enter.exe"
    test_source.write_text(harness)
    subprocess.run([
        args.cxx, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-DLV_CONF_SKIP=1",
        "-I", str(args.lvgl_dir.resolve()), "-I", str(root / "src/platform"),
        str(test_source), "-o", str(executable),
    ], check=True)
    raise SystemExit(subprocess.run([str(executable)]).returncode)
