#include "fb.h"

#include <stdio.h>
#include <string.h>

#include <vector>

#include "NewDisplay/CompressionRLE.h"

// Firmware fonts (core/libs/fonts), FT8xx "legacy" format: a 148-byte metric
// block (widths[128], format, stride, width, height, ptr as uint32) followed
// by one stride*height glyph per character from the first non-zero width
// upwards, 4 bits/pixel.
extern const CompressedData Roboto_Mono_Regular_14_L4_compressed[3487];
extern const CompressedData Roboto_Mono_Bold_17_L4_compressed[4884];
extern const CompressedData LiberationSans_Regular_12_L4_compressed[3305];

namespace proto {

static uint32_t le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
}

bool Font::load(FontId id) {
    static std::vector<uint8_t> storage[(int)FontId::Count];
    const CompressedData* src = nullptr;
    int count = 0;
    switch (id) {
        case FontId::Mono14: src = Roboto_Mono_Regular_14_L4_compressed; count = 3487; break;
        case FontId::MonoBold17: src = Roboto_Mono_Bold_17_L4_compressed; count = 4884; break;
        case FontId::Sans12: src = LiberationSans_Regular_12_L4_compressed; count = 3305; break;
        default: return false;
    }
    size_t total = 0;
    for (int i = 0; i < count; i++) total += src[i].count;
    auto& buf = storage[(int)id];
    buf.assign(total, 0);
    decompress(src, count, buf.data());
    memcpy(widths, buf.data(), 128);
    stride = (int)le32(buf.data() + 132);
    width = (int)le32(buf.data() + 136);
    height = (int)le32(buf.data() + 140);
    glyphs = buf.data() + 148;
    // Tables start at the first character with a width: 32 for plain fonts,
    // 24 for the bold font (hardware icons), 17 for the small font (Polish).
    first = 32;
    for (int c = 1; c < 128; c++)
        if (widths[c]) {
            first = c;
            break;
        }
    return stride > 0 && height > 0;
}

Framebuffer::Framebuffer(int w_, int h_) : w(w_), h(h_), px((size_t)w_ * h_) {
    for (int i = 0; i < (int)FontId::Count; i++) fonts_[i].load((FontId)i);
    clear(0);
}

void Framebuffer::clear(uint32_t c) {
    for (auto& p : px) p = c;
}

void Framebuffer::rect(int x, int y, int rw, int rh, uint32_t c) {
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + rw > w ? w : x + rw, y1 = y + rh > h ? h : y + rh;
    for (int j = y0; j < y1; j++)
        for (int i = x0; i < x1; i++) px[j * w + i] = c;
}

void Framebuffer::disc(int cx, int cy, int r, uint32_t c) {
    for (int j = -r; j <= r; j++)
        for (int i = -r; i <= r; i++)
            if (i * i + j * j <= r * r) rect(cx + i, cy + j, 1, 1, c);
}

void Framebuffer::blit(const Framebuffer& s, int x, int y) {
    for (int j = 0; j < s.h; j++) {
        int yy = y + j;
        if (yy < 0 || yy >= h) continue;
        for (int i = 0; i < s.w; i++) {
            int xx = x + i;
            if (xx >= 0 && xx < w) px[yy * w + xx] = s.px[j * s.w + i];
        }
    }
}

void Framebuffer::frame(int x, int y, int fw, int fh, uint32_t c) {
    rect(x, y, fw, 1, c);
    rect(x, y + fh - 1, fw, 1, c);
    rect(x, y, 1, fh, c);
    rect(x + fw - 1, y, 1, fh, c);
}

static uint32_t blend(uint32_t bg, uint32_t fg, int a /*0..15*/) {
    uint32_t r = (((fg >> 16) & 255) * a + ((bg >> 16) & 255) * (15 - a)) / 15;
    uint32_t g = (((fg >> 8) & 255) * a + ((bg >> 8) & 255) * (15 - a)) / 15;
    uint32_t b = ((fg & 255) * a + (bg & 255) * (15 - a)) / 15;
    return (r << 16) | (g << 8) | b;
}

int Framebuffer::text(int x, int y, const char* s, uint32_t c, FontId fid, int n) {
    const Font& f = fonts_[(int)fid];
    for (int k = 0; s[k] && (n < 0 || k < n); k++) {
        unsigned ch = (unsigned char)s[k];
        if (ch < 32 || ch > 126) ch = '?';
        const uint8_t* g = f.glyphs + (ch - f.first) * f.stride * f.height;
        for (int j = 0; j < f.height; j++) {
            int yy = y + j;
            if (yy < 0 || yy >= h) continue;
            for (int i = 0; i < f.stride * 2; i++) {
                int xx = x + i;
                if (xx < 0 || xx >= w) continue;
                uint8_t b = g[j * f.stride + i / 2];
                int a = (i & 1) ? (b & 15) : (b >> 4);
                if (a) px[yy * w + xx] = blend(px[yy * w + xx], c, a);
            }
        }
        x += f.widths[ch];
    }
    return x;
}

int Framebuffer::textWidth(const char* s, FontId fid, int n) {
    const Font& f = fonts_[(int)fid];
    int w = 0;
    for (int k = 0; s[k] && (n < 0 || k < n); k++) {
        unsigned ch = (unsigned char)s[k];
        w += f.widths[(ch < 32 || ch > 126) ? '?' : ch];
    }
    return w;
}

// ---- PNG (stored deflate blocks, no compression) ----------------------------
static uint32_t crcTable[256];
static void crcInit() {
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crcTable[n] = c;
    }
}
static uint32_t crc(const uint8_t* b, size_t n, uint32_t c = 0xFFFFFFFFu) {
    for (size_t i = 0; i < n; i++) c = crcTable[(c ^ b[i]) & 255] ^ (c >> 8);
    return c;
}

bool Framebuffer::savePng(const char* path) const {
    crcInit();
    std::vector<uint8_t> raw;
    raw.reserve((size_t)(w * 3 + 1) * h);
    for (int y = 0; y < h; y++) {
        raw.push_back(0);
        for (int x = 0; x < w; x++) {
            uint32_t c = px[y * w + x];
            raw.push_back((c >> 16) & 255);
            raw.push_back((c >> 8) & 255);
            raw.push_back(c & 255);
        }
    }
    std::vector<uint8_t> z = {0x78, 0x01};
    size_t pos = 0;
    while (pos < raw.size()) {
        size_t n = raw.size() - pos > 65535 ? 65535 : raw.size() - pos;
        z.push_back(pos + n == raw.size() ? 1 : 0);
        z.push_back(n & 255);
        z.push_back(n >> 8);
        z.push_back(~n & 255);
        z.push_back((~n >> 8) & 255);
        z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
        pos += n;
    }
    uint32_t a = 1, b = 0;
    for (uint8_t v : raw) {
        a = (a + v) % 65521;
        b = (b + a) % 65521;
    }
    uint32_t adler = (b << 16) | a;
    for (int k = 3; k >= 0; k--) z.push_back((adler >> (k * 8)) & 255);

    FILE* f = fopen(path, "wb");
    if (!f) return false;
    static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    fwrite(sig, 1, 8, f);
    auto chunk = [&](const char* type, const uint8_t* data, size_t n) {
        uint8_t hdr[8] = {(uint8_t)(n >> 24), (uint8_t)(n >> 16), (uint8_t)(n >> 8), (uint8_t)n,
                          (uint8_t)type[0], (uint8_t)type[1], (uint8_t)type[2], (uint8_t)type[3]};
        fwrite(hdr, 1, 8, f);
        if (n) fwrite(data, 1, n, f);
        uint32_t c = crc(hdr + 4, 4);
        c = crc(data, n, c) ^ 0xFFFFFFFFu;
        uint8_t cb[4] = {(uint8_t)(c >> 24), (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c};
        fwrite(cb, 1, 4, f);
    };
    uint8_t ihdr[13] = {0, 0, (uint8_t)(w >> 8), (uint8_t)(w & 255), 0, 0, (uint8_t)(h >> 8), (uint8_t)(h & 255),
                        8, 2, 0, 0, 0};
    chunk("IHDR", ihdr, 13);
    chunk("IDAT", z.data(), z.size());
    chunk("IEND", nullptr, 0);
    return fclose(f) == 0;
}

}  // namespace proto
