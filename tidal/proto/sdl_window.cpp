// SDL2 window for the prototype. Every input goes through a Tracker control
// (hardware.h): physical keys press a fixed control (arrows, Space = Play,
// digits = pads ...), shortcut keys (r, w, Enter ...) press whichever
// control does that job right now. The mock panel (panel.h) shows which.
// Key hints in panel.cpp must match the tables here.
#include <SDL.h>

#include "editor.h"
#include "fb.h"
#include "hardware.h"
#include "panel.h"
#include "view.h"

namespace proto {

namespace {

struct Key {
    bool isCtl = false;
    Ctl ctl = Ctl::Count;
    Action action = Action::Cancel;
    bool forceShift = false;  // key implies Shift (e.g. Shift+wheel)
};

bool mapKey(const SDL_KeyboardEvent& k, bool shift, Key& out) {
    auto ctl = [&](Ctl c) {
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
    if (s >= SDLK_F1 && s <= SDLK_F8) return ctl((Ctl)((int)Ctl::Screen0 + (s - SDLK_F1)));
    switch (s) {
        case SDLK_LEFT: return ctl(Ctl::Left);
        case SDLK_RIGHT: return ctl(Ctl::Right);
        case SDLK_UP: return ctl(Ctl::Up);
        case SDLK_DOWN: return ctl(Ctl::Down);
        case SDLK_PAGEUP:
        case SDLK_EQUALS:
        case SDLK_PLUS:
        case SDLK_KP_PLUS: return ctl(Ctl::JogUp);
        case SDLK_PAGEDOWN:
        case SDLK_MINUS:
        case SDLK_KP_MINUS: return ctl(Ctl::JogDown);
        case SDLK_SPACE: return ctl(Ctl::Play);
        case SDLK_i: return ctl(Ctl::Insert);
        case SDLK_x:
        case SDLK_DELETE:
        case SDLK_BACKSPACE: return ctl(Ctl::Delete);
        case SDLK_t: return ctl(Ctl::Config);
        case SDLK_s: return ctl(ctrl ? Ctl::File : Ctl::Instr);
        case SDLK_n: return ctl(Ctl::Note);
        case SDLK_l: return ctl(Ctl::Pattern);
        case SDLK_f: return ctl(Ctl::Fx1);
        case SDLK_c: return ctl(ctrl ? Ctl::Copy : Ctl::Fx2);
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
void pressCtl(Editor& ed, Ctl c, bool shift, const std::string& via, PanelState& st, Uint32& litUntil) {
    Command cmd = controlCommand(ed, c, shift);
    bool did = press(ed, c, shift);
    st.lit = c;
    litUntil = SDL_GetTicks() + 180;
    st.last = via + " -> " + (shift && c != Ctl::Shift ? "Shift+" : "") + ctlName(c) + "  [" + ctlFwName(c) +
              "] -> " + (did ? cmd.label : std::string("(nothing here)"));
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
            } else if (e.type == SDL_KEYDOWN) {
                Key k;
                if (e.key.keysym.sym == SDLK_q && (e.key.keysym.mod & KMOD_CTRL)) {
                    running = false;
                } else if (mapKey(e.key, kbShift, k)) {
                    std::string via = "key " + keyName(e.key);
                    if (k.isCtl) {
                        // Shift+digit already chose the second pad row
                        bool sh = isPad(k.ctl) ? false : shift;
                        pressCtl(ed, k.ctl, sh, via, st, litUntil);
                    } else {
                        Ctl c;
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
                dirty = true;  // Shift display
            } else if (e.type == SDL_MOUSEWHEEL) {
                int dy = e.wheel.y;
                for (int n = 0; n < (dy > 0 ? dy : -dy); n++)
                    pressCtl(ed, dy > 0 ? Ctl::JogUp : Ctl::JogDown, shift, "wheel", st, litUntil);
                dirty = true;
            } else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT && withPanel) {
                Ctl c;
                if (panelHit(e.button.x, e.button.y, c)) {
                    if (c == Ctl::Shift) {
                        latched = !latched;
                        st.last = latched ? "click -> Shift latched for the next press" : "click -> Shift released";
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
        if (st.lit != Ctl::Count && SDL_GetTicks() > litUntil) {
            st.lit = Ctl::Count;
            dirty = true;
        }
        bool shiftNow = (SDL_GetModState() & KMOD_SHIFT) || latched;
        if (shiftNow != st.shift) {
            st.shift = shiftNow;
            dirty = true;
        }
        if (ed.backend()) {
            size_t before = ed.backend()->log.size();
            ed.backend()->poll();
            if (ed.backend()->log.size() != before) dirty = true;
        }
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
