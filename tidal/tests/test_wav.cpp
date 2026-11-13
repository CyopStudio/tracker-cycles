// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// core/tidal/wav_parse.h (the sample-folder loader's forgiving WAV walk) on
// hand-made headers: the cases the stock reader refuses.
#include <stdio.h>
#include <string.h>

#include <vector>

#include "../../core/tidal/wav_parse.h"

namespace {
int gPass = 0, gFail = 0;
#define WEXPECT(c, ...)                                   \
    do {                                                  \
        if (c) gPass++;                                   \
        else {                                            \
            gFail++;                                      \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);   \
            printf(__VA_ARGS__);                          \
            printf("\n");                                 \
        }                                                 \
    } while (0)

struct Wav {
    std::vector<uint8_t> b;
    void id(const char* s) { b.insert(b.end(), s, s + 4); }
    void u32(uint32_t v) {
        for (int k = 0; k < 4; k++) b.push_back((uint8_t)(v >> (8 * k)));
    }
    void u16(uint16_t v) {
        b.push_back((uint8_t)v);
        b.push_back((uint8_t)(v >> 8));
    }
    void bytes(int n, uint8_t v = 0) { b.insert(b.end(), n, v); }
    void fmt(uint16_t format, uint16_t ch, uint32_t rate, uint16_t bits, bool ext = false) {
        id("fmt ");
        u32(ext ? 40 : 16);
        u16(ext ? 0xFFFE : format);
        u16(ch);
        u32(rate);
        u32(rate * ch * bits / 8);
        u16((uint16_t)(ch * bits / 8));
        u16(bits);
        if (ext) {
            u16(22);
            u16(bits);
            u32(0);
            u16(format);  // sub-format GUID starts with the tag
            bytes(14);
        }
    }
    bool parse(wavp::Info& f, char* why) const {
        return wavp::parse(
          (uint32_t)b.size(),
          [&](uint32_t at, uint8_t* buf, uint32_t n) {
              if (at + n > b.size()) return false;
              memcpy(buf, b.data() + at, n);
              return true;
          },
          f, why, 64);
    }
};

Wav plain(uint32_t riffSize, uint32_t dataLen, int dataBytes) {
    Wav w;
    w.id("RIFF");
    w.u32(riffSize);
    w.id("WAVE");
    w.fmt(1, 1, 44100, 16);
    w.id("data");
    w.u32(dataLen);
    w.bytes(dataBytes, 7);
    return w;
}
}  // namespace

void testWavParse(int& pass, int& fail) {
    char why[64];
    wavp::Info f;
    Wav w = plain(36 + 100, 100, 100);
    WEXPECT(w.parse(f, why) && f.dataOffset == 44 && f.dataBytes == 100 && f.rate == 44100 && f.align == 2,
            "plain: %s", why);
    w = plain(999999, 100, 100);  // RIFF size past the end (the stock reader refuses)
    WEXPECT(w.parse(f, why) && f.dataBytes == 100, "RIFF size wrong: %s", why);
    w = plain(36 + 100, 5000, 100);  // data size past the end: cut
    WEXPECT(w.parse(f, why) && f.dataBytes == 100, "data size past the end: %s (%u)", why, f.dataBytes);
    w = plain(36, 0, 64);  // data size 0 (streamed writers)
    WEXPECT(w.parse(f, why) && f.dataBytes == 64, "data size 0: %s", why);
    {  // an odd chunk without its pad byte before fmt
        Wav o;
        o.id("RIFF");
        o.u32(0);
        o.id("WAVE");
        o.id("LIST");
        o.u32(3);
        o.bytes(3, 'x');  // no pad
        o.fmt(1, 2, 22050, 16);
        o.id("data");
        o.u32(8);
        o.bytes(8);
        WEXPECT(o.parse(f, why) && f.channels == 2 && f.rate == 22050 && f.dataBytes == 8, "odd, no pad: %s", why);
    }
    {  // the same with its pad byte
        Wav o;
        o.id("RIFF");
        o.u32(0);
        o.id("WAVE");
        o.id("LIST");
        o.u32(3);
        o.bytes(4, 'x');  // 3 + pad
        o.fmt(1, 1, 44100, 24);
        o.id("data");
        o.u32(9);
        o.bytes(9);
        WEXPECT(o.parse(f, why) && f.bits == 24 && f.dataBytes == 9, "odd, padded: %s", why);
    }
    {  // extensible, float sub-format
        Wav o;
        o.id("RIFF");
        o.u32(0);
        o.id("WAVE");
        o.fmt(3, 1, 48000, 32, true);
        o.id("data");
        o.u32(16);
        o.bytes(16);
        WEXPECT(o.parse(f, why) && f.format == 3 && f.bits == 32 && f.rate == 48000, "extensible: %s (%u)", why,
                f.format);
    }
    {  // refusals say why
        Wav o;
        o.id("RIFX");
        o.u32(0);
        o.id("WAVE");
        WEXPECT(!o.parse(f, why) && strstr(why, "RIFF"), "not RIFF: %s", why);
        Wav n;
        n.id("RIFF");
        n.u32(0);
        n.id("WAVE");
        n.id("data");
        n.u32(4);
        n.bytes(4);
        WEXPECT(!n.parse(f, why) && strstr(why, "fmt"), "no fmt: %s", why);
    }
    pass += gPass;
    fail += gFail;
}
