// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
#include "input.h"

namespace desk {

using tc::Action;
using tc::Control;

// Keys that repeat when held (OS key repeat); everything else acts once.
static bool repeats(Control c) {
    switch (c) {
        case Control::Left:
        case Control::Right:
        case Control::Up:
        case Control::Down:
        case Control::JogUp:
        case Control::JogDown:
        case Control::Delete:
            return true;
        default:
            return false;
    }
}

bool KeyInput::pressCtl(Control c, bool shift, const std::string& via, uint32_t ms) {
    const tc::Command cmd = tc::controlCommand(ed_, c, shift);
    bool did;
    if (tc::isPad(c)) {
        // the chord window: a pad joins the held pads only if it went down
        // soon enough after the chord's first pad; otherwise those count as
        // released (a rolled-over key is the next step, not a chord)
        if (!pads_.empty() && (chordMs <= 0 || ms - chordStart_ > (uint32_t)chordMs)) releasePads();
        if (pads_.empty()) chordStart_ = ms;
        did = tc::press(ed_, c, shift);
        pads_.push_back({0, c});  // released on its key up (or by the chord window)
    } else {
        did = tc::press(ed_, c, shift);
        if (c != Control::Pattern) tc::release(ed_, c);
    }
    lit = c;
    litAt = ms;
    last = via + " -> " + (shift ? "Shift+" : "") + tc::ctlName(c) + " -> " +
           (did ? std::string(cmd.label.c_str()) : std::string("(nothing here)"));
    return did;
}

void KeyInput::releasePads() {
    for (const auto& p : pads_) tc::release(ed_, p.pad);
    pads_.clear();
}

bool KeyInput::runTarget(const Target& t, bool passShift, bool repeat, const std::string& via, uint32_t ms) {
    switch (t.kind) {
        case Target::Ctl: {
            if (repeat && !repeats(t.ctl)) return true;  // held: acts once
            const bool shift = t.shift || passShift;
            pressCtl(t.ctl, shift, via, ms);
            return true;
        }
        case Target::Act: {
            Control c;
            bool sh;
            if (!tc::controlFor(ed_, t.action, c, sh)) return false;  // try the next alternative
            if (repeat && !repeats(c)) return true;
            pressCtl(c, sh, via, ms);
            return true;
        }
        case Target::App: {
            if (repeat) return true;
            if (t.app == "hush") {  // = Shift+Pattern pressed and released alone
                tc::press(ed_, Control::Pattern, true);
                tc::release(ed_, Control::Pattern);
                last = via + " -> Shift+Pattern (alone) -> hush";
                return true;
            }
            if (t.app == "nextslot" || t.app == "prevslot") {
                ed_.act(t.app == "nextslot" ? Action::NextSlot : Action::PrevSlot);
                last = via + " -> " + (t.app == "nextslot" ? "next slot" : "previous slot");
                return true;
            }
            if (onApp && onApp(t.app)) {
                last = via + " -> " + t.app;
                return true;
            }
            return false;
        }
    }
    return false;
}

void KeyInput::keyDown(int sc, uint8_t mods, bool repeat, uint32_t ms) {
    if (const Binding* h = km_.holdFor(sc)) {
        if (repeat) return;
        for (const auto& k : holds_)
            if (k.scancode == sc) return;
        const Control c = h->alts[0].ctl;
        const bool inMenu = ed_.menuOpen();
        pressCtl(c, h->alts[0].shift || (mods & kModShift), "key " + bindingName(*h), ms);
        // Pattern in a menu is OK (acts on press); out of one it's held
        if (!inMenu) holds_.push_back({sc, c});
        return;
    }
    bool passShift;
    const Binding* b = km_.find(sc, mods, passShift);
    std::string via = "key ";
    {
        Binding shown;
        shown.scancode = sc;
        shown.mods = mods & (uint8_t)~km_.holdMods();
        via += bindingName(shown);
    }
    if (!b) {
        if (!repeat) last = via + " -> (not mapped)";
        return;
    }
    for (const auto& t : b->alts)
        if (runTarget(t, passShift, repeat, via, ms)) return;
    if (!repeat) last = via + " -> (nothing here)";
}

void KeyInput::keyUp(int sc, uint32_t ms) {
    (void)ms;
    for (size_t i = 0; i < holds_.size(); i++)
        if (holds_[i].scancode == sc) {
            tc::release(ed_, holds_[i].ctl);
            holds_.erase(holds_.begin() + (long)i);
            return;
        }
    // a pad key: released only if it's still part of the chord (else the
    // chord window already let it go)
    bool passShift;
    for (uint8_t mods : {(uint8_t)0, (uint8_t)kModShift}) {
        const Binding* b = km_.find(sc, mods, passShift);
        if (!b) continue;
        for (const auto& t : b->alts) {
            if (t.kind != Target::Ctl || !tc::isPad(t.ctl)) continue;
            for (size_t i = 0; i < pads_.size(); i++)
                if (pads_[i].pad == t.ctl) {
                    tc::release(ed_, pads_[i].pad);
                    pads_.erase(pads_.begin() + (long)i);
                    return;
                }
        }
    }
}

void KeyInput::wheel(int steps, bool shift) {
    const Control c = steps > 0 ? Control::JogUp : Control::JogDown;
    for (int n = 0; n < (steps > 0 ? steps : -steps); n++) pressCtl(c, shift, "wheel", litAt);
}

void KeyInput::text(const char* s) {
    for (; *s; s++) {
        if (tc::typeChar(ed_, *s)) last = std::string("type '") + *s + "' -> its pad";
        else last = std::string("no Tracker key types '") + *s + "'";
    }
}

void KeyInput::releaseAll() {
    releasePads();
    for (const auto& k : holds_) tc::release(ed_, k.ctl);
    holds_.clear();
}

}  // namespace desk
