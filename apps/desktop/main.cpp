// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// Tracker Cycles desktop: the Tracker Cycles editor on a PC keyboard,
// sending real Tidal (GHCi) to SuperDirt. Plan: apps/desktop/PLAN.md.
//
//   tidal-desktop [options] [file.tidal]
//     --backend ghci|engine|print|none   where evaluated code goes (default
//                                 ghci; engine = our engine -> SuperDirt OSC)
//     --superdirt HOST[:PORT]     SuperDirt's address (default: in WSL the
//                                 Windows host, else 127.0.0.1; port 57120)
//     --ghci CMD                  ghci executable (default ghci)
//     --boot FILE                 GHCi boot script to use as is (default:
//                                 the built-in one with --superdirt filled in)
//     --keymap FILE|default       keymap file (default: the built-in iso-uk)
//     --chord-ms N                chord window for pads (default 40, 0 = off)
//     --no-panel                  window: the screen only, no mock panel
//     --instruments FILE          instrument slots for the pads (one name a line)
//     --keys "TOKENS"             headless: key events, e.g. "F3 shift+F3
//                                 ctrl+L down:Left_Alt 1 up:Left_Alt +200 Q"
//                                 KEY = press + release; down:KEY / up:KEY;
//                                 +MS = advance the clock (tokens without
//                                 one happen at the same time); text:ABC = typed
//                                 text; print (file), msg (status line),
//                                 last (what the last key did), menulist,
//                                 shot:FILE.png, wait:MS (real time),
//                                 log (backend output)
//     --shot FILE.png             screenshot (with the panel) after --keys
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "backend.h"
#include "controls.h"
#include "editor.h"
#include "engine_backend.h"
#include "fb.h"
#include "input.h"
#include "keymap.h"
#include "output.h"
#include "panel.h"
#include "view.h"

using namespace proto;
using namespace desk;

namespace desk {
extern const char* const kBootTemplate;  // tidal/proto/boot.ghci, compiled in
}

namespace {

const Keymap* gKeymap = nullptr;
std::string panelHint(const tc::Editor&, tc::Control c, bool shift) { return gKeymap->hint(c, shift); }

bool readFile(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

bool runningInWsl() {
    if (getenv("WSL_DISTRO_NAME")) return true;
    std::string v;
    return readFile("/proc/version", v) && (v.find("microsoft") != std::string::npos ||
                                            v.find("Microsoft") != std::string::npos);
}

// The default route's gateway (in WSL's NAT mode: the Windows host).
std::string defaultGateway() {
    std::ifstream f("/proc/net/route");
    std::string line;
    std::getline(f, line);  // header
    while (std::getline(f, line)) {
        char iface[64];
        unsigned dest, gw;
        if (sscanf(line.c_str(), "%63s %x %x", iface, &dest, &gw) == 3 && dest == 0 && gw) {
            char buf[32];
            snprintf(buf, sizeof buf, "%u.%u.%u.%u", gw & 255, (gw >> 8) & 255, (gw >> 16) & 255, gw >> 24);
            return buf;
        }
    }
    return "";
}

std::string autoSuperDirtHost() {
    if (!runningInWsl()) return "127.0.0.1";
    std::string mode;
    if (FILE* p = popen("wslinfo --networking-mode 2>/dev/null", "r")) {
        char buf[64] = {};
        if (fgets(buf, sizeof buf, p)) mode = buf;
        pclose(p);
    }
    if (mode.find("mirrored") != std::string::npos) return "127.0.0.1";
    const std::string gw = defaultGateway();
    return gw.empty() ? "127.0.0.1" : gw;
}

// The built-in boot script with SuperDirt's address filled in.
std::string bootScript(const std::string& host, int port) {
    std::string s = kBootTemplate;
    const std::string a = "oAddress = \"127.0.0.1\"", p = "oPort = 57120";
    size_t at = s.find(a);
    if (at != std::string::npos) s.replace(at, a.size(), "oAddress = \"" + host + "\"");
    at = s.find(p);
    if (at != std::string::npos) s.replace(at, p.size(), "oPort = " + std::to_string(port));
    return s;
}

// Settings / generated files: SDL's per-user folder
// (~/.local/share/CyopStudio/TrackerCycles/, %APPDATA%\CyopStudio\TrackerCycles\).
std::string prefDir() {
    char* p = SDL_GetPrefPath("CyopStudio", "TrackerCycles");
    std::string s = p ? p : "./";
    SDL_free(p);
    return s;
}

uint8_t modsOf(SDL_Keymod m) {
    uint8_t r = 0;
    if (m & KMOD_CTRL) r |= kModCtrl;
    if (m & KMOD_LALT) r |= kModAlt;  // right Alt is AltGr on UK / EU layouts
    if (m & KMOD_SHIFT) r |= kModShift;
    return r;
}

// "ctrl+shift+F3" / "Left_Alt" -> scancode + mods
bool parseKeyToken(std::string t, int& sc, uint8_t& mods) {
    mods = 0;
    for (;;) {
        if (!strncasecmp(t.c_str(), "ctrl+", 5)) mods |= kModCtrl, t = t.substr(5);
        else if (!strncasecmp(t.c_str(), "alt+", 4)) mods |= kModAlt, t = t.substr(4);
        else if (!strncasecmp(t.c_str(), "shift+", 6)) mods |= kModShift, t = t.substr(6);
        else break;
    }
    for (char& c : t)
        if (c == '_') c = ' ';
    sc = scancodeFromName(t);
    return sc != SDL_SCANCODE_UNKNOWN;
}

const std::vector<std::string> kLegend = {
    "Button: name / function / key / purple = with Shift",
    "Shift is Shift everywhere (Shift+F3 = Shift+Screen 3)",
    "Pads: the 4 key rows (key in each pad's corner)",
    "Alt held = Pattern: Alt+pad mutes, Shift+Alt = hush",
    "Tab / Shift+Tab: next / prev slot. Ctrl+Q quits",
};

bool writeShot(const tc::Editor& ed, Framebuffer& screen, const std::string& path, const KeyInput& in) {
    drawEditor(ed, screen);
    std::unique_ptr<Framebuffer> panel(new Framebuffer(kPanelW, kPanelH));
    PanelState st;
    st.last = in.last;
    st.legend = kLegend;
    drawPanel(ed, screen, *panel, st);
    return panel->savePng(path.c_str());
}

bool runKeys(tc::Editor& ed, KeyInput& in, Framebuffer& screen, const std::string& keys) {
    std::istringstream ss(keys);
    std::string tok;
    uint32_t ms = 1000;
    while (ss >> tok) {
        if (tok[0] == '+' && tok.size() > 1 && isdigit((unsigned char)tok[1])) {
            ms += (uint32_t)atoi(tok.c_str() + 1);
            continue;
        }
        if (tok == "print") {
            static char text[65536];
            ed.fileText(text, sizeof text);
            fputs(text, stdout);
        } else if (tok == "msg") {
            printf("msg: %s%s\n", ed.messageIsError() ? "ERROR " : "", ed.message().c_str());
        } else if (tok == "last") {
            printf("last: %s\n", in.last.c_str());
        } else if (tok == "menulist") {
            if (!ed.menuOpen()) {
                printf("menu: (closed)\n");
            } else {
                printf("menu %s:", ed.menuTitle().c_str());
                for (const auto& it : ed.menu()) printf(" [%s]", it.label.c_str());
                printf("\n");
            }
        } else if (tok.rfind("shot:", 0) == 0) {
            if (!writeShot(ed, screen, tok.substr(5), in)) fprintf(stderr, "can't write %s\n", tok.c_str() + 5);
        } else if (tok == "log") {  // the backend's output (GHCi replies)
            if (appBackend())
                for (const auto& l : appBackend()->log) printf("| %s\n", l.c_str());
        } else if (tok.rfind("wait:", 0) == 0) {  // real time, polling the backend
            for (int t = 0; t < atoi(tok.c_str() + 5); t += 50) {
                if (appBackend()) appBackend()->poll();
                usleep(50000);
            }
        } else if (tok.rfind("text:", 0) == 0) {
            in.text(tok.c_str() + 5);
        } else {
            const bool down = tok.rfind("down:", 0) == 0, up = tok.rfind("up:", 0) == 0;
            const std::string k = down ? tok.substr(5) : up ? tok.substr(3) : tok;
            int sc;
            uint8_t mods;
            if (!parseKeyToken(k, sc, mods)) {
                fprintf(stderr, "unknown key token '%s'\n", tok.c_str());
                return false;
            }
            if (!up) in.keyDown(sc, mods, false, ms);
            if (!down) in.keyUp(sc, ms);
        }
        if (appBackend()) appBackend()->poll();
        fprintf(stderr, "%-14s | %s\n", tok.c_str(), in.last.c_str());
    }
    return true;
}

int runWindow(tc::Editor& ed, KeyInput& in, Framebuffer& screen, bool withPanel, const std::string& title) {
    SDL_SetHint(SDL_HINT_WINDOWS_NO_CLOSE_ON_ALT_F4, "1");  // Alt = Pattern: Alt+F4 must not quit
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    const int W = withPanel ? kPanelW : kW, H = withPanel ? kPanelH : kH;
    // the biggest size that fits the screen (the screen alone: whole multiples)
    double scale = 1;
    SDL_Rect usable;
    if (SDL_GetDisplayUsableBounds(0, &usable) == 0) {
        const double fit = std::min((usable.w - 40) / (double)W, (usable.h - 60) / (double)H);
        scale = withPanel ? std::max(0.5, std::min(fit, 1.5)) : std::max(1.0, (double)(int)fit);
    }
    SDL_Window* win = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       (int)(W * scale), (int)(H * scale), SDL_WINDOW_RESIZABLE);
    SDL_Renderer* ren = win ? SDL_CreateRenderer(win, -1, 0) : nullptr;
    if (ren) SDL_RenderSetLogicalSize(ren, W, H);
    SDL_Texture* tex = ren ? SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, W, H)
                           : nullptr;
    if (!tex) {
        fprintf(stderr, "SDL window: %s\n", SDL_GetError());
        return 1;
    }
    SDL_StopTextInput();  // only the keyboard screen takes typed text
    bool textOn = false;
    std::unique_ptr<Framebuffer> panel(new Framebuffer(withPanel ? kPanelW : 1, withPanel ? kPanelH : 1));
    PanelState st;
    st.legend = kLegend;
    bool running = true, dirty = true;
    in.onApp = [&](const std::string& app) {
        if (app == "quit") {
            running = false;
            return true;
        }
        return false;  // settings, type: later milestones
    };
    while (running) {
        SDL_Event e;
        if (SDL_WaitEventTimeout(&e, 16)) {
            do {
                const bool kbd = ed.keyboardOpen();
                if (e.type == SDL_QUIT) {
                    running = false;
                } else if (e.type == SDL_TEXTINPUT && kbd) {
                    in.text(e.text.text);
                } else if (e.type == SDL_KEYDOWN) {
                    const SDL_Keysym& k = e.key.keysym;
                    const bool ctrl = k.mod & KMOD_CTRL;
                    if (kbd && !ctrl && k.sym < 128 && isprint((int)k.sym)) {
                        // typed text arrives as SDL_TEXTINPUT
                    } else if (kbd && (k.sym == SDLK_RETURN || k.sym == SDLK_KP_ENTER || k.sym == SDLK_ESCAPE)) {
                        const bool ok = k.sym != SDLK_ESCAPE;  // the keyboard screen's Screen 8 OK / Screen 7 Cancel
                        tc::press(ed, ok ? tc::Control::Screen7 : tc::Control::Screen6, false);
                        in.last = ok ? "key Enter -> OK" : "key Esc -> Cancel";
                    } else if (kbd && k.sym == SDLK_BACKSPACE) {
                        tc::press(ed, tc::Control::Delete, false);
                        in.last = "key Bksp -> Delete (backspace)";
                    } else {
                        in.keyDown(k.scancode, modsOf((SDL_Keymod)k.mod), e.key.repeat != 0, e.key.timestamp);
                    }
                } else if (e.type == SDL_KEYUP) {
                    in.keyUp(e.key.keysym.scancode, e.key.timestamp);
                } else if (e.type == SDL_MOUSEWHEEL) {
                    in.wheel(e.wheel.y, SDL_GetModState() & KMOD_SHIFT);
                } else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT && withPanel) {
                    tc::Control c;
                    if (panelHit(e.button.x, e.button.y, c) && c != tc::Control::Shift) {
                        const bool shift = SDL_GetModState() & KMOD_SHIFT;
                        const tc::Command cmd = tc::controlCommand(ed, c, shift);
                        const bool did = tc::press(ed, c, shift);
                        tc::release(ed, c);
                        in.last = std::string("click -> ") + (shift ? "Shift+" : "") + tc::ctlName(c) + " -> " +
                                  (did ? cmd.label.c_str() : "(nothing here)");
                    }
                } else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                    in.releaseAll();
                }
                dirty = true;
            } while (SDL_PollEvent(&e));
        }
        if (ed.keyboardOpen() != textOn) {
            textOn = ed.keyboardOpen();
            if (textOn) SDL_StartTextInput();
            else SDL_StopTextInput();
        }
        const tc::Control lit = SDL_GetTicks() - in.litAt < 180 ? in.lit : tc::Control::Count;
        const bool shiftNow = SDL_GetModState() & KMOD_SHIFT;
        if (lit != st.lit || shiftNow != st.shift || in.last != st.last) dirty = true;
        st.lit = lit;
        st.shift = shiftNow;
        st.last = in.last;
        if (appBackend()) {
            const size_t before = appBackend()->log.size();
            appBackend()->poll();
            if (appBackend()->log.size() != before) dirty = true;
        }
        if (ed.pollHighlights(steadySeconds())) dirty = true;
        if (dirty) {
            drawEditor(ed, screen);
            const Framebuffer* out = &screen;
            if (withPanel) {
                drawPanel(ed, screen, *panel, st);
                out = panel.get();
            }
            SDL_UpdateTexture(tex, nullptr, out->px.data(), out->w * 4);
            SDL_RenderClear(ren);
            SDL_RenderCopy(ren, tex, nullptr, nullptr);
            SDL_RenderPresent(ren);
            dirty = false;
        }
    }
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::string backendName = "ghci", ghci = "ghci", boot, superdirt, keymapPath = "default", keys, shot, file,
                instruments;
    bool panel = true;
    int chordMs = 40;
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
        else if (a == "--superdirt") superdirt = next();
        else if (a == "--keymap") keymapPath = next();
        else if (a == "--chord-ms") chordMs = atoi(next().c_str());
        else if (a == "--no-panel") panel = false;
        else if (a == "--instruments") instruments = next();
        else if (a == "--keys") keys = next();
        else if (a == "--shot") shot = next();
        else if (a == "-h" || a == "--help") {
            printf("see the comment at the top of apps/desktop/main.cpp\n");
            return 0;
        } else if (!a.empty() && a[0] == '-') {
            fprintf(stderr, "unknown option %s\n", a.c_str());
            return 2;
        } else {
            file = a;
        }
    }
    const bool headless = !keys.empty() || !shot.empty();
    // video for key names under the OS layout; headless runs use the dummy driver
    if (headless && !getenv("SDL_VIDEODRIVER")) setenv("SDL_VIDEODRIVER", "dummy", 1);
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    atexit(SDL_Quit);

    // keymap
    static Keymap keymap;
    {
        std::string text = kDefaultKeymap, err;
        if (keymapPath != "default" && !readFile(keymapPath, text)) {
            fprintf(stderr, "can't read keymap %s\n", keymapPath.c_str());
            return 1;
        }
        if (!keymap.load(text, err))
            fprintf(stderr, "%s:\n%s", keymapPath == "default" ? "built-in keymap" : keymapPath.c_str(), err.c_str());
    }
    gKeymap = &keymap;
    setPanelHints(panelHint);

    // SuperDirt address and the GHCi boot script
    std::string host = superdirt;
    int port = 57120;
    if (size_t colon = host.rfind(':'); colon != std::string::npos) {
        port = atoi(host.c_str() + colon + 1);
        host = host.substr(0, colon);
    }
    std::unique_ptr<Backend> backend;
    if (backendName == "ghci" || backendName == "engine") {
        if (host.empty()) host = autoSuperDirtHost();
        fprintf(stderr, "SuperDirt at %s:%d\n", host.c_str(), port);
    }
    if (backendName == "ghci") {
        if (boot.empty()) {
            boot = prefDir() + "boot.ghci";
            std::ofstream f(boot, std::ios::binary);
            f << bootScript(host, port);
            if (!f) {
                fprintf(stderr, "can't write %s\n", boot.c_str());
                return 1;
            }
        }
        backend.reset(new GhciBackend(ghci, boot));
    } else if (backendName == "engine") {
        backend.reset(new EngineBackend(host, port, 0.1));
    } else if (backendName == "print") {
        backend.reset(new PrintBackend(true));
    } else if (backendName != "none") {
        fprintf(stderr, "unknown backend %s\n", backendName.c_str());
        return 2;
    }

    // the editor (the same storage the prototype uses)
    static tc::Program prog;
    static tc::Checkpoint checkpoint;
    static uint8_t undoArena[256 * 1024];
    static tc::Editor ed(prog, checkpoint, undoArena, (int)sizeof undoArena);
    setAppBackend(backend.get());
    static BackendOutput out(backend.get());
    ed.setOutput(&out);
    if (!instruments.empty()) {
        std::string text;
        if (!readFile(instruments, text)) {
            fprintf(stderr, "can't open %s\n", instruments.c_str());
            return 1;
        }
        std::istringstream ls(text);
        std::string line;
        int i = 0;
        while (i < tc::kInstrumentSlots && std::getline(ls, line)) {
            while (!line.empty() && strchr("\r\n ", line.back())) line.pop_back();
            while (!line.empty() && line[0] == ' ') line.erase(0, 1);
            ed.setInstrument(i++, line.c_str());
        }
        for (; i < tc::kInstrumentSlots; i++) ed.setInstrument(i, "");
    }
    ed.act(tc::Action::ToggleTrackerMode);  // real Tidal: host-only functions allowed (Ctrl+T turns the check on)
    ed.setCursorMode(true);
    if (!file.empty()) {
        std::string err;
        if (access(file.c_str(), F_OK) == 0 && !loadFile(ed, file, err)) {
            fprintf(stderr, "%s:%s\n", file.c_str(), err.c_str());
            return 1;
        }
        out.path = file;
    }

    KeyInput input(ed, keymap);
    input.chordMs = chordMs;
    std::unique_ptr<Framebuffer> screen(new Framebuffer);
    if (!keys.empty() && !runKeys(ed, input, *screen, keys)) return 1;
    if (!shot.empty() && !writeShot(ed, *screen, shot, input)) {
        fprintf(stderr, "can't write %s\n", shot.c_str());
        return 1;
    }
    if (headless) return 0;
    const std::string title = "Tracker Cycles" + (file.empty() ? std::string() : " - " + file);
    return runWindow(ed, input, *screen, panel, title);
}
