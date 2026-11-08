// SDL2 window for the prototype. Keyboard stands in for the Tracker's
// controls until the editor runs on the device (see tidal/README.md).
#include <SDL.h>

#include "editor.h"
#include "fb.h"
#include "view.h"

namespace proto {

static bool mapKey(const SDL_KeyboardEvent& k, Action& a) {
    bool shift = k.keysym.mod & KMOD_SHIFT;
    bool ctrl = k.keysym.mod & KMOD_CTRL;
    switch (k.keysym.sym) {
        case SDLK_LEFT: a = Action::Left; return true;
        case SDLK_RIGHT: a = Action::Right; return true;
        case SDLK_UP: a = Action::Up; return true;
        case SDLK_DOWN: a = Action::Down; return true;
        case SDLK_PAGEUP:
        case SDLK_EQUALS:
        case SDLK_PLUS: a = shift ? Action::WheelUpFast : Action::WheelUp; return true;
        case SDLK_PAGEDOWN:
        case SDLK_MINUS: a = shift ? Action::WheelDownFast : Action::WheelDown; return true;
        case SDLK_r: a = Action::Replace; return true;
        case SDLK_w: a = Action::Wrap; return true;
        case SDLK_i: a = Action::Insert; return true;
        case SDLK_x:
        case SDLK_DELETE:
        case SDLK_BACKSPACE: a = Action::Delete; return true;
        case SDLK_u: a = Action::Unwrap; return true;
        case SDLK_z: a = Action::Undo; return true;
        case SDLK_SPACE: a = Action::Evaluate; return true;
        case SDLK_RETURN:
        case SDLK_KP_ENTER: a = Action::Confirm; return true;
        case SDLK_ESCAPE: a = Action::Cancel; return true;
        case SDLK_h: a = Action::Hush; return true;
        case SDLK_TAB: a = shift ? Action::PrevSlot : Action::NextSlot; return true;
        case SDLK_t: a = Action::ToggleTrackerMode; return true;
        case SDLK_s:
            if (ctrl) {
                a = Action::Save;
                return true;
            }
            return false;
        default: return false;
    }
}

int runWindow(Editor& ed, Framebuffer& fb, int scale) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    if (scale < 1) scale = 1;
    SDL_Window* win = SDL_CreateWindow("Tracker Cycles", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       kW * scale, kH * scale, 0);
    SDL_Renderer* ren = win ? SDL_CreateRenderer(win, -1, 0) : nullptr;
    SDL_Texture* tex =
        ren ? SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, kW, kH) : nullptr;
    if (!tex) {
        fprintf(stderr, "SDL window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    bool running = true, dirty = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            Action a;
            if (e.type == SDL_QUIT) running = false;
            else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_q && (e.key.keysym.mod & KMOD_CTRL)) running = false;
                else if (mapKey(e.key, a)) {
                    ed.act(a);
                    dirty = true;
                }
            } else if (e.type == SDL_MOUSEWHEEL) {
                bool fast = SDL_GetModState() & KMOD_SHIFT;
                int dy = e.wheel.y;
                for (int k = 0; k < (dy > 0 ? dy : -dy); k++)
                    ed.act(dy > 0 ? (fast ? Action::WheelUpFast : Action::WheelUp)
                                  : (fast ? Action::WheelDownFast : Action::WheelDown));
                dirty = true;
            } else if (e.type == SDL_WINDOWEVENT) {
                dirty = true;
            }
        }
        if (ed.backend()) {
            size_t before = ed.backend()->log.size();
            ed.backend()->poll();
            if (ed.backend()->log.size() != before) dirty = true;
        }
        if (dirty) {
            drawEditor(ed, fb);
            SDL_UpdateTexture(tex, nullptr, fb.px, kW * 4);
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
