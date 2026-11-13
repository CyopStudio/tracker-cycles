// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// Tracker Cycles editor prototype (plan step 1).
//
//   tidal_proto [options] [file.tidal]
//     --backend print|ghci|engine|none   where evaluated code goes (default
//                                 print; engine = our engine + scheduler ->
//                                 SuperDirt OSC, plan step 3)
//     --osc HOST:PORT             engine: SuperDirt address (127.0.0.1:57120)
//     --latency S                 engine: OSC timestamp latency (0.1, as boot.ghci)
//     --send STMT                 send a Tidal statement to the backend at
//                                 startup (repeatable; before --keys)
//     --ghci CMD                  ghci executable (default ghci)
//     --boot FILE                 GHCi boot script (default proto/boot.ghci)
//     --keys "a b c"              run editor actions headless, e.g.
//                                 "down right wheel+ wrap menu:3 ok eval"
//                                 extra tokens: menu:N  pick:LABEL (%20 = space)
//                                 shot:FILE.png  wait:MS
//                                 print (file text to stdout)
//                                 log (backend log, e.g. GHCi replies)
//                                 hw:CONTROL / hw:shift+CONTROL presses a
//                                 Tracker control, e.g. hw:s1 hw:jog+
//                                 hw:shift+play hw:pad3 (ids: editor/controls.cpp)
//                                 hw:pad20+pad24+pad27 = pads held together
//                                 pads / shift+pads: the pad lights
//     --shot FILE.png             screenshot after the keys ran
//     --window                    open a window with the mock Tracker panel
//                                 (needs a build with SDL=1)
//     --no-panel                  window/screenshots: screen only
//     --panel                     screenshots include the mock panel
//     --scale N                   window scale (default 1)
//     --no-tracker-mode           allow host-only functions
//     --mode cursor|tree          the editing mode at start (default cursor;
//                                 Rec switches; smoke sessions use tree)
//     --instruments FILE          instrument slots for the pads, one sample
//                                 name per line (max 48; blank = empty slot)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <memory>
#include <sstream>
#include <vector>
#include <string>

#include "backend.h"
#include "engine_backend.h"
#include "editor.h"
#include "fb.h"
#include "controls.h"
#include "output.h"
#include "panel.h"
#include "view.h"

namespace proto {
using tc::Action;
using tc::Editor;
int runWindow(Editor& ed, Framebuffer& fb, int scale, bool withPanel);  // sdl_window.cpp
}

using namespace proto;

#ifndef TC_HAVE_SDL
namespace proto {
int runWindow(Editor&, Framebuffer&, int, bool) {
    fprintf(stderr, "built without SDL: rebuild with `make proto SDL=1` (see tidal/README.md)\n");
    return 2;
}
}  // namespace proto
#endif

static bool gPanelShots = false;

static bool writeShot(const Editor& ed, Framebuffer& fb, const char* path) {
    drawEditor(ed, fb);
    if (!gPanelShots) return fb.savePng(path);
    std::unique_ptr<Framebuffer> panel(new Framebuffer(kPanelW, kPanelH));
    drawPanel(ed, fb, *panel, PanelState());
    return panel->savePng(path);
}

static bool runKeys(Editor& ed, Framebuffer& fb, const std::string& keys) {
    std::istringstream in(keys);
    std::string tok;
    while (in >> tok) {
        if (tok.rfind("menu:", 0) == 0) {
            const int i = atoi(tok.c_str() + 5);
            if (getenv("TC_TRACE_PICKS"))  // one line per menu:N (sessions -> pick:LABEL)
                fprintf(stderr, "MENUPICK %s\n", ed.menuOpen() && i >= 0 && i < (int)ed.menu().size()
                                                     ? ed.menu()[i].label.c_str() : "\x01");
            ed.selectMenu(i);
        } else if (tok.rfind("pick:", 0) == 0) {
            // pick:LABEL = the open menu's item with that label (%20 = space,
            // %25 = %): sessions that survive menu order changes
            std::string want;
            for (size_t k = 5; k < tok.size(); k++) {
                if (tok[k] == '%' && k + 2 < tok.size()) {
                    want += (char)strtol(tok.substr(k + 1, 2).c_str(), nullptr, 16);
                    k += 2;
                } else {
                    want += tok[k];
                }
            }
            int found = -1;
            for (int i = 0; i < (int)ed.menu().size() && found < 0; i++)
                if (want == ed.menu()[i].label.c_str()) found = i;
            if (found < 0) {
                fprintf(stderr, "pick: no menu item '%s'\n", want.c_str());
                printf("pick: no menu item '%s'\n", want.c_str());
            } else {
                ed.selectMenu(found);
            }
        } else if (tok.rfind("shot:", 0) == 0) {
            if (!writeShot(ed, fb, tok.c_str() + 5)) fprintf(stderr, "can't write %s\n", tok.c_str() + 5);
        } else if (tok.rfind("hw:", 0) == 0) {
            std::string id = tok.substr(3);
            bool shift = id.rfind("shift+", 0) == 0;
            if (shift) id = id.substr(6);
            // Pattern acts on press / release (v2, Play's job until RC14): hw:pattern /
            // hw:shift+pattern = pressed and released alone; hw:pattern+pad3 =
            // pad pressed while Pattern is held (hw:pattern+delete cancels)
            if (id == "pattern" || id.rfind("pattern+", 0) == 0) {
                const bool inMenu = ed.menuOpen();
                press(ed, Control::Pattern, shift);
                if (!inMenu) {
                    if (id.size() > 8) {
                        Control c2;
                        if (!ctlFromId(id.substr(8).c_str(), c2)) {
                            fprintf(stderr, "unknown control '%s'\n", id.substr(8).c_str());
                            return false;
                        }
                        press(ed, c2, false);
                    }
                    release(ed, Control::Pattern);
                }
                continue;
            }
            // hw:pad20+pad24+pad27 = pads pressed in turn and held together
            // (a chord), released at the end; every press is released
            std::vector<Control> held;
            const bool chord = id.rfind("pad", 0) == 0 && id.find("+pad") != std::string::npos;
            for (size_t at = 0; at <= id.size();) {
                size_t plus = chord ? id.find('+', at) : std::string::npos;
                if (plus == std::string::npos) plus = id.size();
                const std::string one = id.substr(at, plus - at);
                at = plus + 1;
                Control c;
                if (!ctlFromId(one.c_str(), c)) {
                    fprintf(stderr, "unknown control '%s'\n", one.c_str());
                    return false;
                }
                if (!press(ed, c, shift)) fprintf(stderr, "%-10s | (%s does nothing here)\n", tok.c_str(), ctlName(c));
                held.push_back(c);
            }
            for (Control c : held) release(ed, c);
        } else if (tok.rfind("wait:", 0) == 0) {
            int ms = atoi(tok.c_str() + 5);
            for (int t = 0; t < ms; t += 50) {
                if (appBackend()) appBackend()->poll();
                ed.pollHighlights(steadySeconds());
                usleep(50000);
            }
        } else if (tok.rfind("type:", 0) == 0) {  // keyboard screen: type text ('_' = space)
            for (char ch : tok.substr(5))
                if (!typeChar(ed, ch == '_' ? ' ' : ch)) fprintf(stderr, "no key types '%c'\n", ch);
        } else if (tok == "lit") {  // steps lit since the last `lit` (engine backend; smoke tests)
            ed.pollHighlights(steadySeconds());
            fputs(litReport(ed).c_str(), stdout);
        } else if (tok == "pads" || tok == "shift+pads") {  // pad lights (device LEDs), 4 rows of 12
            static const char kC[] = ".-oO*";
            for (int r = 0; r < kPadRows; r++) {
                printf("pads%s ", r ? "     " : ":");
                for (int c2 = 0; c2 < kPadCols; c2++)
                    putchar(kC[(int)padLight(ed, r * kPadCols + c2, tok[0] == 's')]);
                putchar('\n');
            }
        } else if (tok == "log") {
            if (appBackend())
                for (const auto& l : appBackend()->log) printf("| %s\n", l.c_str());
        } else if (tok == "print") {
            static char text[65536];
            ed.fileText(text, sizeof text);
            fputs(text, stdout);
        } else if (tok == "menulist") {  // the open menu's items, on stdout (smoke tests)
            if (!ed.menuOpen()) {
                printf("menu: (closed)\n");
            } else {
                printf("menu %s:", ed.menuTitle().c_str());
                for (const auto& it : ed.menu()) printf(" [%s]", it.label.c_str());
                printf("\n");
            }
        } else if (tok == "msg") {  // the status line, on stdout (smoke tests)
            printf("msg: %s%s\n", ed.messageIsError() ? "ERROR " : "", ed.message().c_str());
        } else {
            Action a;
            if (!tc::actionFromName(tok.c_str(), a)) {
                fprintf(stderr, "unknown key token '%s'\n", tok.c_str());
                return false;
            }
            ed.act(a);
        }
        if (appBackend()) appBackend()->poll();
        fprintf(stderr, "%-10s | %s%s\n", tok.c_str(), ed.messageIsError() ? "ERROR " : "",
                ed.message().c_str());
    }
    return true;
}

int main(int argc, char** argv) {
    std::string backendName = "print", ghci = "ghci", boot = "proto/boot.ghci", keys, shot, file, instruments;
    std::string osc = "127.0.0.1:57120";
    double latency = 0.1;
    std::vector<std::string> sends;
    bool window = false, trackerMode = true, panel = true, cursorMode = true;
    int scale = 1;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) {
                fprintf(stderr, "%s needs a value\n", a.c_str());
                exit(2);
            }
            return argv[++i];
        };
        if (a == "--backend") backendName = next();
        else if (a == "--ghci") ghci = next();
        else if (a == "--boot") boot = next();
        else if (a == "--osc") osc = next();
        else if (a == "--send") sends.push_back(next());
        else if (a == "--latency") latency = atof(next().c_str());
        else if (a == "--keys") keys = next();
        else if (a == "--shot") shot = next();
        else if (a == "--window") window = true;
        else if (a == "--scale") scale = atoi(next().c_str());
        else if (a == "--no-tracker-mode") trackerMode = false;
        else if (a == "--no-panel") panel = false;
        else if (a == "--instruments") instruments = next();
        else if (a == "--panel") gPanelShots = true;
        else if (a == "--mode") cursorMode = next() != "tree";
        else if (a == "-h" || a == "--help") {
            printf("see the comment at the top of tidal/proto/main.cpp or tidal/README.md\n");
            return 0;
        } else if (!a.empty() && a[0] == '-') {
            fprintf(stderr, "unknown option %s\n", a.c_str());
            return 2;
        } else {
            file = a;
        }
    }

    std::unique_ptr<Backend> backend;
    if (backendName == "print") backend.reset(new PrintBackend(!window));
    else if (backendName == "ghci") backend.reset(new GhciBackend(ghci, boot));
    else if (backendName == "engine") {
        size_t colon = osc.rfind(':');
        std::string host = colon == std::string::npos ? osc : osc.substr(0, colon);
        int port = colon == std::string::npos ? 57120 : atoi(osc.c_str() + colon + 1);
        backend.reset(new EngineBackend(host, port, latency));
    }
    else if (backendName != "none") {
        fprintf(stderr, "unknown backend %s\n", backendName.c_str());
        return 2;
    }

    // the editor's storage (the device puts these in SDRAM)
    static tc::Program prog;
    static tc::Checkpoint checkpoint;
    static uint8_t undoArena[256 * 1024];
    static Editor ed(prog, checkpoint, undoArena, (int)sizeof undoArena);
    setAppBackend(backend.get());
    static BackendOutput out(backend.get());
    ed.setOutput(&out);
    if (backend)
        for (const auto& st : sends) backend->send(st);
    if (!instruments.empty()) {
        FILE* f = fopen(instruments.c_str(), "r");
        if (!f) {
            fprintf(stderr, "can't open %s\n", instruments.c_str());
            return 1;
        }
        std::vector<std::string> names;
        char line[256];
        while (names.size() < 48 && fgets(line, sizeof line, f)) {
            std::string s = line;
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
            while (!s.empty() && s[0] == ' ') s.erase(0, 1);
            names.push_back(s);
        }
        fclose(f);
        for (int i = 0; i < tc::kInstrumentSlots; i++) ed.setInstrument(i, i < (int)names.size() ? names[i].c_str() : "");
    }
    if (!trackerMode) ed.act(Action::ToggleTrackerMode);
    ed.setCursorMode(cursorMode);  // cursor mode at boot (UI v3 D); Rec switches
    if (!file.empty()) {
        std::string err;
        if (access(file.c_str(), F_OK) == 0) {
            if (!loadFile(ed, file, err)) {
                fprintf(stderr, "%s:%s\n", file.c_str(), err.c_str());
                return 1;
            }
        }
        out.path = file;  // saved on `save` (a new file too)
    }

    std::unique_ptr<Framebuffer> fb(new Framebuffer);
    if (!keys.empty() && !runKeys(ed, *fb, keys)) return 1;
    if (!shot.empty()) {
        if (!writeShot(ed, *fb, shot.c_str())) {
            fprintf(stderr, "can't write %s\n", shot.c_str());
            return 1;
        }
    }
    if (window) return runWindow(ed, *fb, scale, panel);
    if (keys.empty() && shot.empty()) {
        fprintf(stderr, "nothing to do: pass --window, --keys or --shot (see --help)\n");
        return 2;
    }
    return 0;
}
