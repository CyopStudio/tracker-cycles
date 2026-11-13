// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// A forgiving WAV header walk, portable (host-tested in tidal/tests).
// Editor18: the stock reader refused 10 of 26 files (e.g. Dirt-Samples' old
// 808 ones). Here: the RIFF size is ignored (the file size counts), a data
// chunk running past the end (or of size 0) is cut to it, an odd chunk
// without its pad byte is accepted, WAVE_FORMAT_EXTENSIBLE uses its
// sub-format tag.
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace wavp {

struct Info {
    uint32_t dataOffset, dataBytes, rate;
    uint16_t channels, bits, format, align;
};

inline uint32_t le32(const uint8_t* p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
inline uint16_t le16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
inline bool printableId(const uint8_t* p) {
    for (int k = 0; k < 4; k++)
        if (p[k] < 0x20 || p[k] > 0x7e) return false;
    return true;
}

// readAt(offset, buf, n) -> true if n bytes were read. False + why.
template <class ReadAt>
bool parse(uint32_t size, ReadAt readAt, Info& f, char* why, int whyCap) {
    memset(&f, 0, sizeof f);
    uint8_t b[40];
    if (size < 12 || !readAt(0, b, 12) || memcmp(b, "RIFF", 4) || memcmp(b + 8, "WAVE", 4))
        return snprintf(why, whyCap, "not a RIFF / WAVE file"), false;
    uint32_t pos = 12;
    bool fmt = false, data = false;
    while (pos + 8 <= size && !(fmt && data)) {
        if (!readAt(pos, b, 8)) break;
        if (!printableId(b)) return snprintf(why, whyCap, "broken chunk list at byte %lu", (unsigned long)pos), false;
        const uint32_t len = le32(b + 4);
        if (!memcmp(b, "fmt ", 4)) {
            const uint32_t n = len < sizeof b ? len : sizeof b;
            if (n < 16 || pos + 8 + n > size || !readAt(pos + 8, b, n))
                return snprintf(why, whyCap, "short fmt chunk"), false;
            f.format = le16(b);
            f.channels = le16(b + 2);
            f.rate = le32(b + 4);
            f.align = le16(b + 12);
            f.bits = le16(b + 14);
            if (f.format == 0xFFFE && n >= 26) f.format = le16(b + 24);  // extensible: the sub-format GUID's tag
            fmt = true;
        } else if (!memcmp(b, "data", 4)) {
            f.dataOffset = pos + 8;
            const uint32_t room = size - f.dataOffset;
            f.dataBytes = (len == 0 || len > room) ? room : len;
            data = true;
        }
        if (len > size) break;  // a size past the end: nothing follows
        uint32_t next = pos + 8 + len + (len & 1);
        if ((len & 1) && next + 4 <= size) {  // a missing pad byte: the next id one byte earlier
            uint8_t id[4];
            if (readAt(next, id, 4) && !printableId(id)) next--;
        }
        if (next <= pos) break;
        pos = next;
    }
    if (!fmt) return snprintf(why, whyCap, "no fmt chunk"), false;
    if (!data) return snprintf(why, whyCap, "no data chunk"), false;
    if (!f.align) f.align = (uint16_t)(f.channels * (f.bits / 8));
    return true;
}

}  // namespace wavp
