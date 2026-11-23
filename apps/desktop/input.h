// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// Keyboard -> Tracker controls: the keymap lookup, hold keys (Alt =
// Pattern), the chord window for pads, key repeat. Every key ends in a
// Tracker control press (tidal/editor/controls.cpp) or a desktop command.
// No SDL event types here, so headless tests drive it the same way.
#pragma once
#include <stdint.h>

#include <functional>
#include <string>
#include <vector>

#include "keymap.h"

namespace desk {

class KeyInput {
public:
    KeyInput(tc::Editor& ed, const Keymap& km) : ed_(ed), km_(km) {}

    // mods: kMod* (Ctrl, left Alt, Shift) at the time of the key. ms: the
    // event's time in milliseconds (for the chord window).
    void keyDown(int scancode, uint8_t mods, bool repeat, uint32_t ms);
    void keyUp(int scancode, uint32_t ms);
    void wheel(int steps, bool shift);  // > 0 = jog +
    // The keyboard screen (Shift+Config comment) is open: typed characters
    // press its pads.
    void text(const char* utf8);
    // All keys up (focus lost): releases held pads and hold keys.
    void releaseAll();

    // Pads pressed within this many ms of the first pad of a chord join it
    // (0 = overlapping keys never make a chord).
    int chordMs = 40;
    // Desktop commands (quit, hush, settings, type, ...); return false if
    // not handled. hush / nextslot / prevslot are handled here.
    std::function<bool(const std::string& app)> onApp;

    // For the panel / status line.
    std::string last;                 // "key F3 -> Screen 3 -> [ ]"
    tc::Control lit = tc::Control::Count;
    uint32_t litAt = 0;

private:
    tc::Editor& ed_;
    const Keymap& km_;
    struct HeldPad {
        int scancode;
        tc::Control pad;
    };
    std::vector<HeldPad> pads_;  // pads still held as part of the current chord
    uint32_t chordStart_ = 0;
    struct HeldKey {
        int scancode;
        tc::Control ctl;
    };
    std::vector<HeldKey> holds_;  // hold keys down (Alt = Pattern)

    bool pressCtl(tc::Control c, bool shift, const std::string& via, uint32_t ms);
    bool runTarget(const Target& t, bool passShift, bool repeat, const std::string& via, uint32_t ms);
    void releasePads();
};

}  // namespace desk
