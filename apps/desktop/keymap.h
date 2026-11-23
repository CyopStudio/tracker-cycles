// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// Keymaps: physical keys (SDL scancodes) -> Tracker controls, loaded from
// text files (keymaps/iso-uk.txt; format in its header and PLAN.md).
#pragma once
#include <stdint.h>

#include <string>
#include <vector>

#include "controls.h"
#include "editor.h"

namespace desk {

enum : uint8_t { kModCtrl = 1, kModAlt = 2, kModShift = 4 };

struct Target {
    enum Kind : uint8_t { Ctl, Act, App } kind = Ctl;
    tc::Control ctl = tc::Control::Count;  // Ctl
    bool shift = false;                    // Ctl: with Shift
    tc::Action action = tc::Action::Cancel;  // Act: via controlFor()
    std::string app;                       // App: desktop command name
};

struct Binding {
    int scancode = 0;
    uint8_t mods = 0;   // kMod*
    bool hold = false;  // pressed while the key is down, released on key up
    std::vector<Target> alts;  // tried left to right
    int line = 0;
};

class Keymap {
public:
    // Parses a keymap file. Returns false with one line per problem in err
    // (bad lines are skipped, the rest still loads).
    bool load(const std::string& text, std::string& err);
    // The binding for a key with these modifiers. A key with Shift held and
    // no binding of its own uses its plain binding; passShift is then true
    // (the control gets Shift, as on the Tracker).
    const Binding* find(int scancode, uint8_t mods, bool& passShift) const;
    // A hold binding for this key (any modifiers), or null.
    const Binding* holdFor(int scancode) const;
    // Modifier keys used as hold keys don't count as modifiers (Alt = Pattern).
    uint8_t holdMods() const { return holdMods_; }
    // Key names that press control c (with Shift or not), "" if none.
    std::string hint(tc::Control c, bool shift) const;
    // The key(s) bound to a desktop command, "" if none.
    std::string appHint(const char* app) const;

    std::vector<Binding> bindings;

private:
    uint8_t holdMods_ = 0;
};

// Scancode names in keymap files: SDL's names, plus NonUSBackslash (the
// ISO key left of Z, which SDL leaves unnamed).
int scancodeFromName(const std::string& name);
std::string scancodeFileName(int scancode);
// What the key shows under the OS layout, short ("Q", "F1", "PgUp", "Alt").
std::string keyCap(int scancode);
// "Ctrl+Shift+L", "hold Alt", ...
std::string bindingName(const Binding& b);

// The built-in default keymap (keymaps/iso-uk.txt, compiled in).
extern const char* const kDefaultKeymap;

}  // namespace desk
