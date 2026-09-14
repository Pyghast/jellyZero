// Exercises libpng's real longjmp error boundary with deterministic write failure.
#include <png.h>
#include <cstdlib>
#include <new>
#include <iostream>

namespace {
constexpr std::size_t row_bytes = 257 * 4;
bool track_row = false;
void* live_row = nullptr;
void fail_png_row(png_structrp png, png_const_bytep) {
    png_error(png, "intentional write failure for regression test");
}
}
void* operator new(std::size_t size) {
    void* p = std::malloc(size ? size : 1);
    if (!p) throw std::bad_alloc();
    if (track_row && size == row_bytes) live_row = p;
    return p;
}
void operator delete(void* p) noexcept {
    if (p == live_row) live_row = nullptr;
    std::free(p);
}
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }

#define png_write_row fail_png_row
#include "../src/platform/screenshot_service.cpp"
#undef png_write_row

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    static uint8_t pixels[row_bytes]{};
    lv_draw_buf_t snapshot{};
    snapshot.header.w = 257;
    snapshot.header.h = 1;
    snapshot.header.stride = row_bytes;
    snapshot.header.cf = LV_COLOR_FORMAT_ARGB8888;
    snapshot.data = pixels;
    std::string error;
    track_row = true;
    const bool ok = platform::screenshot::write_png(argv[1], &snapshot, error);
    track_row = false;
    const bool passed = !ok && error == "PNG encoding failed" && live_row == nullptr;
    if (live_row) { std::free(live_row); live_row = nullptr; }
    std::filesystem::remove(argv[1]);
    std::cout << (passed ? "PASS: PNG error releases row buffer" : "FAIL: PNG error leaks row buffer") << std::endl;
    return passed ? 0 : 1;
}
