// 800x480 software framebuffer (the Tracker's screen size) with the
// firmware's own L4 fonts, and a minimal PNG writer for screenshots.
#pragma once
#include <stdint.h>

namespace proto {

constexpr int kW = 800;
constexpr int kH = 480;

enum class FontId { Mono14, MonoBold17, Sans12, Count };

struct Font {
    int width = 0, height = 0, stride = 0;  // cell width, height, bytes/row
    uint8_t widths[128] = {};
    const uint8_t* glyphs = nullptr;        // glyph for char `first` first
    int first = 32;                         // first character in the table
    bool load(FontId id);
};

class Framebuffer {
public:
    Framebuffer();
    uint32_t px[kW * kH];  // 0xRRGGBB

    void clear(uint32_t c);
    void rect(int x, int y, int w, int h, uint32_t c);
    void frame(int x, int y, int w, int h, uint32_t c);
    // Draws text; returns the x after the last glyph.
    int text(int x, int y, const char* s, uint32_t c, FontId f = FontId::Mono14, int n = -1);
    int textWidth(const char* s, FontId f = FontId::Mono14, int n = -1);
    const Font& font(FontId f) const { return fonts_[(int)f]; }
    bool savePng(const char* path) const;

private:
    Font fonts_[(int)FontId::Count];
};

}  // namespace proto
