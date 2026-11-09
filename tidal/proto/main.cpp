// Tracker Cycles editor prototype (plan step 1).
//
//   tidal_proto [options] [file.tidal]
//     --backend print|ghci|none   where evaluated code goes (default print)
//     --ghci CMD                  ghci executable (default ghci)
//     --boot FILE                 GHCi boot script (default proto/boot.ghci)
//     --keys "a b c"              run editor actions headless, e.g.
//                                 "down right wheel+ wrap menu:3 ok eval"
//                                 extra tokens: menu:N  shot:FILE.png  wait:MS
//                                 print (file text to stdout)
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <memory>
#include <sstream>
#include <string>

#include "backend.h"
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
                usleep(50000);
            }
        } else if (tok == "print") {
            fputs(ed.fileText().c_str(), stdout);
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
    std::string backendName = "print", ghci = "ghci", boot = "proto/boot.ghci", keys, shot, file;
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
        else if (a == "--keys") keys = next();
        else if (a == "--shot") shot = next();
        else if (a == "--window") window = true;
        else if (a == "--scale") scale = atoi(next().c_str());
        else if (a == "--no-tracker-mode") trackerMode = false;
        else if (a == "--no-panel") panel = false;
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
    else if (backendName != "none") {
        fprintf(stderr, "unknown backend %s\n", backendName.c_str());
        return 2;
    }

    Editor ed;
    ed.setBackend(backend.get());
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
