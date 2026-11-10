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
//                                 extra tokens: menu:N  shot:FILE.png  wait:MS
//                                 print (file text to stdout)
//                                 log (backend log, e.g. GHCi replies)
//                                 hw:CONTROL / hw:shift+CONTROL presses a
//                                 Tracker control, e.g. hw:s1 hw:jog+
//                                 hw:shift+play hw:pad3 (ids: hardware.cpp)
//     --shot FILE.png             screenshot after the keys ran
//     --window                    open a window with the mock Tracker panel
//                                 (needs a build with SDL=1)
//     --no-panel                  window/screenshots: screen only
//     --panel                     screenshots include the mock panel
//     --scale N                   window scale (default 1)
//     --no-tracker-mode           allow host-only functions
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
#include "hardware.h"
#include "panel.h"
#include "view.h"

namespace proto {
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
            ed.selectMenu(atoi(tok.c_str() + 5));
        } else if (tok.rfind("shot:", 0) == 0) {
            if (!writeShot(ed, fb, tok.c_str() + 5)) fprintf(stderr, "can't write %s\n", tok.c_str() + 5);
        } else if (tok.rfind("hw:", 0) == 0) {
            std::string id = tok.substr(3);
            bool shift = id.rfind("shift+", 0) == 0;
            if (shift) id = id.substr(6);
            // Play acts on press / release (v2): hw:play / hw:shift+play =
            // pressed and released alone; hw:play+pad3 / hw:shift+play+pad3 =
            // pad pressed while Play is held (hw:play+delete cancels)
            if (id == "play" || id.rfind("play+", 0) == 0) {
                const bool inMenu = ed.menuOpen();
                press(ed, Ctl::Play, shift);
                if (!inMenu) {
                    if (id.size() > 5) {
                        Ctl c2;
                        if (!ctlFromId(id.substr(5), c2)) {
                            fprintf(stderr, "unknown control '%s'\n", id.substr(5).c_str());
                            return false;
                        }
                        press(ed, c2, false);
                    }
                    release(ed, Ctl::Play);
                }
                continue;
            }
            Ctl c;
            if (!ctlFromId(id, c)) {
                fprintf(stderr, "unknown control '%s'\n", id.c_str());
                return false;
            }
            if (!press(ed, c, shift)) fprintf(stderr, "%-10s | (%s does nothing here)\n", tok.c_str(), ctlName(c));
        } else if (tok.rfind("wait:", 0) == 0) {
            int ms = atoi(tok.c_str() + 5);
            for (int t = 0; t < ms; t += 50) {
                if (ed.backend()) ed.backend()->poll();
                ed.pollHighlights(steadySeconds());
                usleep(50000);
            }
        } else if (tok.rfind("type:", 0) == 0) {  // keyboard screen: type text ('_' = space)
            for (char ch : tok.substr(5))
                if (!typeChar(ed, ch == '_' ? ' ' : ch)) fprintf(stderr, "no key types '%c'\n", ch);
        } else if (tok == "lit") {  // steps lit since the last `lit` (engine backend; smoke tests)
            ed.pollHighlights(steadySeconds());
            fputs(ed.litReport().c_str(), stdout);
        } else if (tok == "log") {
            if (ed.backend())
                for (const auto& l : ed.backend()->log) printf("| %s\n", l.c_str());
        } else if (tok == "print") {
            fputs(ed.fileText().c_str(), stdout);
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
            if (!actionFromName(tok, a)) {
                fprintf(stderr, "unknown key token '%s'\n", tok.c_str());
                return false;
            }
            ed.act(a);
        }
        if (ed.backend()) ed.backend()->poll();
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
    bool window = false, trackerMode = true, panel = true;
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

    Editor ed;
    ed.setBackend(backend.get());
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
        ed.setSamples(names);
    }
    if (!trackerMode) ed.act(Action::ToggleTrackerMode);
    if (!file.empty()) {
        std::string err;
        if (access(file.c_str(), F_OK) == 0) {
            if (!ed.load(file, err)) {
                fprintf(stderr, "%s:%s\n", file.c_str(), err.c_str());
                return 1;
            }
        } else {
            ed.path = file;  // new file, saved on `save`
        }
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
