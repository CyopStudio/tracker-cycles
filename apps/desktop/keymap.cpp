// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
#include "keymap.h"

#include <SDL.h>
#include <ctype.h>
#include <strings.h>

#include <sstream>

namespace desk {

using tc::Control;

int scancodeFromName(const std::string& name) {
    if (!strcasecmp(name.c_str(), "NonUSBackslash")) return SDL_SCANCODE_NONUSBACKSLASH;
    return (int)SDL_GetScancodeFromName(name.c_str());
}

std::string scancodeFileName(int sc) {
    if (sc == SDL_SCANCODE_NONUSBACKSLASH) return "NonUSBackslash";
    return SDL_GetScancodeName((SDL_Scancode)sc);
}

std::string keyCap(int sc) {
    std::string n = SDL_GetKeyName(SDL_GetKeyFromScancode((SDL_Scancode)sc));
    if (n.empty() && sc == SDL_SCANCODE_NONUSBACKSLASH) n = "\\";  // the UK \\ key, left of Z
    if (n.empty()) n = scancodeFileName(sc);
    static const struct { const char* from; const char* to; } kShort[] = {
        {"Left Alt", "Alt"},     {"Right Alt", "AltGr"}, {"PageUp", "PgUp"},      {"PageDown", "PgDn"},
        {"Return", "Enter"},     {"Backspace", "Bksp"},  {"Delete", "Del"},       {"Insert", "Ins"},
        {"Escape", "Esc"},       {"Keypad Enter", "KpEnter"}, {"Left Ctrl", "Ctrl"}, {"Left Shift", "Shift"},
    };
    for (const auto& s : kShort)
        if (n == s.from) return s.to;
    return n;
}

std::string bindingName(const Binding& b) {
    std::string s;
    if (b.hold) s += "hold ";
    if (b.mods & kModCtrl) s += "Ctrl+";
    if (b.mods & kModAlt) s += "Alt+";
    if (b.mods & kModShift) s += "Shift+";
    return s + keyCap(b.scancode);
}

static std::string lower(std::string s) {
    for (char& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

static bool parseTarget(const std::vector<std::string>& t, Target& out, std::string& why) {
    if (t.empty()) {
        why = "empty target";
        return false;
    }
    const std::string w0 = lower(t[0]);
    if (w0 == "action" || w0 == "app") {
        if (t.size() != 2) {
            why = w0 + " needs one name";
            return false;
        }
        if (w0 == "app") {
            out.kind = Target::App;
            out.app = lower(t[1]);
            static const char* kApps[] = {"quit", "hush", "nextslot", "prevslot", "type", "settings"};
            for (const char* a : kApps)
                if (out.app == a) return true;
            why = "unknown app command '" + t[1] + "'";
            return false;
        }
        out.kind = Target::Act;
        if (!tc::actionFromName(lower(t[1]).c_str(), out.action)) {
            why = "unknown action '" + t[1] + "'";
            return false;
        }
        return true;
    }
    size_t i = 0;
    out.kind = Target::Ctl;
    if (w0 == "shift" && t.size() > 1) {
        out.shift = true;
        i = 1;
    }
    if (t.size() != i + 1) {
        why = "one control expected";
        return false;
    }
    if (!tc::ctlFromId(lower(t[i]).c_str(), out.ctl)) {
        why = "unknown control '" + t[i] + "'";
        return false;
    }
    return true;
}

bool Keymap::load(const std::string& text, std::string& err) {
    bindings.clear();
    holdMods_ = 0;
    err.clear();
    std::istringstream in(text);
    std::string line;
    int no = 0;
    auto bad = [&](const std::string& why) { err += "line " + std::to_string(no) + ": " + why + "\n"; };
    while (std::getline(in, line)) {
        no++;
        std::vector<std::string> tok;
        {
            std::istringstream ls(line);
            std::string w;
            while (ls >> w) tok.push_back(w);
        }
        if (tok.empty()) continue;
        // '#' starts a comment, except "# = ..." (the # key)
        if (tok[0][0] == '#' && !(tok[0] == "#" && tok.size() > 1 && tok[1] == "=")) continue;
        size_t eq = 0;
        for (size_t k = 1; k < tok.size() && !eq; k++)
            if (tok[k] == "=") eq = k;
        if (!eq) {
            bad("no ' = ' (KEY = TARGET)");
            continue;
        }
        Binding b;
        b.line = no;
        size_t k = 0;
        for (; k + 1 < eq; k++) {  // modifiers before the key name (the key keeps at least one word)
            const std::string m = lower(tok[k]);
            if (m == "ctrl") b.mods |= kModCtrl;
            else if (m == "alt") b.mods |= kModAlt;
            else if (m == "shift") b.mods |= kModShift;
            else if (m == "hold") b.hold = true;
            else break;
        }
        std::string key;
        for (; k < eq; k++) key += (key.empty() ? "" : " ") + tok[k];
        b.scancode = scancodeFromName(key);
        if (b.scancode == SDL_SCANCODE_UNKNOWN) {
            bad("unknown key '" + key + "'");
            continue;
        }
        std::vector<std::string> cur;
        bool ok = true;
        for (size_t r = eq + 1; r <= tok.size() && ok; r++) {
            const bool end = r == tok.size() || tok[r][0] == '#';
            if (end || tok[r] == "|") {
                Target t;
                std::string why;
                if (!parseTarget(cur, t, why)) {
                    bad(why);
                    ok = false;
                } else {
                    b.alts.push_back(t);
                }
                cur.clear();
                if (end) break;
            } else {
                cur.push_back(tok[r]);
            }
        }
        if (!ok) continue;
        if (b.hold) {
            if (b.alts.size() != 1 || b.alts[0].kind != Target::Ctl) {
                bad("a hold key needs one control");
                continue;
            }
            if (b.scancode == SDL_SCANCODE_LALT || b.scancode == SDL_SCANCODE_RALT) holdMods_ |= kModAlt;
            if (b.scancode == SDL_SCANCODE_LCTRL || b.scancode == SDL_SCANCODE_RCTRL) holdMods_ |= kModCtrl;
        }
        for (auto& o : bindings)
            if (o.scancode == b.scancode && o.mods == b.mods && o.hold == b.hold) {
                bad("'" + bindingName(b) + "' is already bound on line " + std::to_string(o.line));
                ok = false;
                break;
            }
        if (ok) bindings.push_back(b);
    }
    return err.empty();
}

const Binding* Keymap::find(int sc, uint8_t mods, bool& passShift) const {
    mods &= (uint8_t)~holdMods_;
    passShift = false;
    for (const auto& b : bindings)
        if (!b.hold && b.scancode == sc && b.mods == mods) return &b;
    if (mods & kModShift)
        for (const auto& b : bindings)
            if (!b.hold && b.scancode == sc && b.mods == (mods & ~kModShift)) {
                passShift = true;
                return &b;
            }
    return nullptr;
}

const Binding* Keymap::holdFor(int sc) const {
    for (const auto& b : bindings)
        if (b.hold && b.scancode == sc) return &b;
    return nullptr;
}

std::string Keymap::hint(Control c, bool shift) const {
    if (c == Control::Shift) return shift ? "" : "Shift";
    std::vector<std::string> names;
    auto add = [&](const std::string& n) {
        for (const auto& x : names)
            if (x == n) return;
        names.push_back(n);
    };
    for (const auto& b : bindings)
        for (const auto& t : b.alts) {
            if (t.kind != Target::Ctl || t.ctl != c) continue;
            const bool sh = t.shift || (b.mods & kModShift);
            if (sh == shift) add(bindingName(b));
        }
    if (c == Control::JogUp || c == Control::JogDown) add(shift ? "Shift+wheel" : "wheel");
    std::string out;
    // the panel has room for one key (the jog: its key and the wheel). Shift
    // passes through, so a control's Shift function only shows keys of its own
    // (Home, Bksp, Ctrl+V ...)
    const size_t n = (c == Control::JogUp || c == Control::JogDown) ? 2 : 1;
    for (size_t i = 0; i < names.size() && i < n; i++) out += (i ? " / " : "") + names[i];
    return out;
}

std::string Keymap::appHint(const char* app) const {
    for (const auto& b : bindings)
        for (const auto& t : b.alts)
            if (t.kind == Target::App && t.app == app) return bindingName(b);
    return "";
}

}  // namespace desk
