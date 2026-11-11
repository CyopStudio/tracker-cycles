// SDL2 window for the prototype. Every input goes through a Tracker control
// (editor/controls.h): physical keys press a fixed control (arrows, Space = Play,
// digits = pads ...), shortcut keys (r, w, Enter ...) press whichever
// control does that job right now. The mock panel (panel.h) shows which.
// Key hints in panel.cpp must match the tables here.
#include <SDL.h>
#include <vector>
#include <ctype.h>

#include "editor.h"
#include "fb.h"
#include "controls.h"
#include "output.h"
#include "panel.h"
#include "view.h"

namespace proto {
using namespace tc;  // the editor model (tidal/editor)

namespace {

struct Key {
    bool isCtl = false;
    Control ctl = Control::Count;
    Action action = Action::Cancel;
    bool forceShift = false;  // key implies Shift (e.g. Shift+wheel)
};

bool mapKey(const SDL_KeyboardEvent& k, bool shift, Key& out) {
    auto ctl = [&](Control c) {
        out.isCtl = true;
        out.ctl = c;
        return true;
    };
    auto intent = [&](Action a) {
        out.isCtl = false;
        out.action = a;
        return true;
    };
    bool ctrl = k.keysym.mod & KMOD_CTRL;
    SDL_Keycode s = k.keysym.sym;
    if (s >= SDLK_1 && s <= SDLK_8) return ctl(padCtl((shift ? kPadCols : 0) + (s - SDLK_1)));
    if (s >= SDLK_F1 && s <= SDLK_F8) return ctl((Control)((int)Control::Screen0 + (s - SDLK_F1)));
    switch (s) {
        case SDLK_LEFT: return ctl(Control::Left);
        case SDLK_RIGHT: return ctl(Control::Right);
        case SDLK_UP: return ctl(Control::Up);
        case SDLK_DOWN: return ctl(Control::Down);
        case SDLK_PAGEUP:
        case SDLK_EQUALS:
        case SDLK_PLUS:
        case SDLK_KP_PLUS: return ctl(Control::JogUp);
        case SDLK_PAGEDOWN:
        case SDLK_MINUS:
        case SDLK_KP_MINUS: return ctl(Control::JogDown);
        case SDLK_SPACE: return ctl(Control::Play);
        case SDLK_i: return ctl(Control::Insert);
        case SDLK_x:
        case SDLK_DELETE:
        case SDLK_BACKSPACE: return ctl(Control::Delete);
        case SDLK_t: return ctl(Control::Config);
        case SDLK_m: return ctl(Control::Rec);  // cursor / tree mode
        case SDLK_s: return ctl(ctrl ? Control::File : Control::Instr);
        case SDLK_n: return ctl(Control::Note);
        case SDLK_l: return ctl(Control::Song);
        case SDLK_p: return intent(Action::EditComment);
        case SDLK_e: return ctl(Control::Pattern);
        case SDLK_f: return ctl(Control::Fx1);
        case SDLK_c: return ctl(ctrl ? Control::Copy : Control::Fx2);
        case SDLK_v: return ctrl ? intent(Action::Paste) : false;
        case SDLK_HOME: return intent(Action::Home);
        case SDLK_r: return intent(Action::Replace);
        case SDLK_w: return intent(Action::Wrap);
        case SDLK_u: return intent(Action::Unwrap);
        case SDLK_z: return intent(Action::Undo);
        case SDLK_h: return intent(Action::Hush);
        case SDLK_RETURN:
        case SDLK_KP_ENTER: return intent(Action::Confirm);
        case SDLK_ESCAPE: return intent(Action::Cancel);
        case SDLK_TAB: return intent(shift ? Action::PrevSlot : Action::NextSlot);
        default: return false;
    }
}

std::string keyName(const SDL_KeyboardEvent& k) {
    std::string n = SDL_GetKeyName(k.keysym.sym);
    if (k.keysym.mod & KMOD_CTRL) n = "Ctrl+" + n;
    else if (k.keysym.mod & KMOD_SHIFT) n = "Shift+" + n;
    return n;
}

// Press a control, record what happened for the panel's status line.
void pressCtl(Editor& ed, Control c, bool shift, const std::string& via, PanelState& st, Uint32& litUntil) {
    Command cmd = controlCommand(ed, c, shift);
    bool did = press(ed, c, shift);
    // a click is a press and a release; Ctrl+click holds a pad (a chord)
    // until the next plain click on a pad
    static std::vector<Control> held;
    if (isPad(c) && (SDL_GetModState() & KMOD_CTRL)) {
        held.push_back(c);
    } else {
        release(ed, c);
        if (isPad(c)) {
            for (Control h : held) release(ed, h);
            held.clear();
        }
    }
    st.lit = c;
    litUntil = SDL_GetTicks() + 180;
    st.last = via + " -> " + (shift && c != Control::Shift ? "Shift+" : "") + ctlName(c) + "  [" + ctlFwName(c) +
              "] -> " + (did ? std::string(cmd.label.c_str()) : std::string("(nothing here)"));
}

}  // namespace

int runWindow(Editor& ed, Framebuffer& screen, int scale, bool withPanel) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    if (scale < 1) scale = 1;
    const int W = withPanel ? kPanelW : kW, H = withPanel ? kPanelH : kH;
    SDL_Window* win = SDL_CreateWindow("Tracker Cycles", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W * scale,
                                       H * scale, SDL_WINDOW_RESIZABLE);
    SDL_Renderer* ren = win ? SDL_CreateRenderer(win, -1, 0) : nullptr;
    if (ren) SDL_RenderSetLogicalSize(ren, W, H);  // mouse coordinates in panel pixels
    SDL_Texture* tex =
        ren ? SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, W, H) : nullptr;
    if (!tex) {
        fprintf(stderr, "SDL window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    Framebuffer panel(withPanel ? kPanelW : 1, withPanel ? kPanelH : 1);
    PanelState st;
    bool latched = false;  // Shift clicked on the panel
    Uint32 litUntil = 0;
    bool running = true, dirty = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            bool kbShift = SDL_GetModState() & KMOD_SHIFT;
            bool shift = kbShift || latched;
            if (e.type == SDL_QUIT) {
                running = false;
            } else if (e.type == SDL_TEXTINPUT && ed.keyboardOpen()) {
                // keyboard screen: each typed character presses its pad
                for (const char* t = e.text.text; *t; t++) {
                    if (typeChar(ed, *t)) st.last = std::string("type '") + *t + "' -> its pad";
                    else st.last = std::string("no Tracker key types '") + *t + "'";
                }
                dirty = true;
            } else if (e.type == SDL_KEYDOWN && ed.keyboardOpen() && e.key.keysym.sym < 128 &&
                       isprint((int)e.key.keysym.sym) && !(e.key.keysym.mod & KMOD_CTRL)) {
                // printable keys arrive as SDL_TEXTINPUT (above)
            } else if (e.type == SDL_KEYDOWN && ed.keyboardOpen() &&
                       (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_KP_ENTER ||
                        e.key.keysym.sym == SDLK_ESCAPE)) {
                const bool ok = e.key.keysym.sym != SDLK_ESCAPE;
                pressCtl(ed, ok ? Control::Screen7 : Control::Screen6, false, ok ? "key Enter" : "key Esc", st, litUntil);
                dirty = true;
            } else if (e.type == SDL_KEYDOWN) {
                Key k;
                if (e.key.keysym.sym == SDLK_q && (e.key.keysym.mod & KMOD_CTRL)) {
                    running = false;
                } else if (e.key.keysym.sym == SDLK_h) {  // hush = Shift+Play pressed and released alone
                    if (!e.key.repeat) {
                        press(ed, Control::Play, true);
                        release(ed, Control::Play);
                        st.last = "key h -> Shift+Play (alone) -> hush";
                    }
                } else if (e.key.keysym.sym == SDLK_SPACE && e.key.repeat) {
                    // Play is held: key repeat must not press it again
                } else if (mapKey(e.key, kbShift, k)) {
                    std::string via = "key " + keyName(e.key);
                    if (k.isCtl) {
                        // digits are Shift+pad (slot select); Shift+digit
                        // already chose the second pad row
                        bool sh = isPad(k.ctl) ? true : shift;
                        pressCtl(ed, k.ctl, sh, via, st, litUntil);
                    } else {
                        Control c;
                        bool sh;
                        if (controlFor(ed, k.action, c, sh)) {
                            pressCtl(ed, c, sh, via, st, litUntil);
                        } else {
                            st.last = via + " -> no Tracker control does '" + std::string(actionName(k.action)) +
                                      "' here";
                        }
                    }
                    latched = false;
                }
                dirty = true;
            } else if (e.type == SDL_KEYUP) {
                if (e.key.keysym.sym == SDLK_SPACE && ed.playHeld()) {  // Play released (v2: acts on release)
                    release(ed, Control::Play);
                    st.last = "key Space released -> Play";
                }
                dirty = true;  // Shift display
            } else if (e.type == SDL_MOUSEWHEEL) {
                int dy = e.wheel.y;
                for (int n = 0; n < (dy > 0 ? dy : -dy); n++)
                    pressCtl(ed, dy > 0 ? Control::JogUp : Control::JogDown, shift, "wheel", st, litUntil);
                dirty = true;
            } else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT && withPanel) {
                Control c;
                if (panelHit(e.button.x, e.button.y, c)) {
                    if (c == Control::Shift) {
                        latched = !latched;
                        st.last = latched ? "click -> Shift latched for the next press" : "click -> Shift released";
                    } else if (c == Control::Play && ed.playHeld() && !ed.menuOpen()) {
                        release(ed, Control::Play);  // second click releases the latched Play
                        st.last = "click -> Play released";
                    } else if (c == Control::Play && !ed.menuOpen()) {
                        press(ed, Control::Play, shift);
                        st.last = "click -> Play latched: click pads to mute, Play again to release";
                        latched = false;
                    } else {
                        pressCtl(ed, c, shift, "click", st, litUntil);
                        latched = false;
                    }
                    dirty = true;
                }
            } else if (e.type == SDL_WINDOWEVENT) {
                dirty = true;
            }
        }
        if (st.lit != Control::Count && SDL_GetTicks() > litUntil) {
            st.lit = Control::Count;
            dirty = true;
        }
        bool shiftNow = (SDL_GetModState() & KMOD_SHIFT) || latched;
        if (shiftNow != st.shift) {
            st.shift = shiftNow;
            dirty = true;
        }
        if (appBackend()) {
            size_t before = appBackend()->log.size();
            appBackend()->poll();
            if (appBackend()->log.size() != before) dirty = true;
        }
        if (ed.pollHighlights(steadySeconds())) dirty = true;
        if (dirty) {
            drawEditor(ed, screen);
            const Framebuffer* out = &screen;
            if (withPanel) {
                drawPanel(ed, screen, panel, st);
                out = &panel;
            }
            SDL_UpdateTexture(tex, nullptr, out->px.data(), out->w * 4);
            SDL_RenderClear(ren);
            SDL_RenderCopy(ren, tex, nullptr, nullptr);
            SDL_RenderPresent(ren);
            dirty = false;
        }
        SDL_Delay(16);
    }
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}

}  // namespace proto
