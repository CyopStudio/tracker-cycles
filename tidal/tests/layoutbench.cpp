// Host timing of the code view's layout (tidal/editor/layout.*): what a
// highlight-only redraw costs before the display draws it. `make layoutbench`.
// Host numbers only show relative changes; the device's SDRAM is uncached.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "editor.h"
#include "layout.h"

static double nowUs() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e6 + ts.tv_nsec / 1e3;
}

static tc::Program gProg;
static tc::Checkpoint gCp;
static uint8_t gUndo[64 * 1024];
static tc::CodeScreen gScr;
static tc::LayoutScratch gScratch;

int main() {
    const char* file =
        "setcps (138/60/4)\n\n"
        "d1 $ every 4 (fast 2) $ s \"bd*2 [~ sd] bd sd\" # gain 1\n\n"
        "d2 $ jux rev $ s \"hh*8\" # gain 0.8 # pan sine\n\n"
        "d3 $ n \"0 .. 7\" # s \"arpy\" # speed \"<1 2>\" # room 0.3\n\n"
        "d4 $ every' 10 9 (jux rev) $ every' 4 3 (slow \"[1 4]\") $ (every'\n"
        "    8 6 (ply \"[2 1 1 2]\") $ n \"{0 1 [2 3] 7 5 2 4 1 0 0}%8\")\n"
        "    # s \"pad\" # voices 2\n\n"
        "d5 $ stut 3 0.5 0.125 $ s \"[bd sd]*2 [~ hh] <cp [cp cp]>\" # crush 6\n\n"
        "d6 $ off 0.25 (# crush 4) $ n \"c'maj e'min\" # s \"superpiano\" # legato 0.5\n\n"
        "d7 $ sometimesBy 0.3 (# speed 2) $ s \"drum*8\" # n (irand 8) # pan rand\n\n"
        "d8 $ chunk 4 (hurry 2) $ s \"{bd sd hh, cp cp}%4\" # gain 0.9 # delay 0.3\n";
    static tc::Editor ed(gProg, gCp, gUndo, (int)sizeof gUndo);
    tc::Label err;
    if (!ed.loadText(file, (int)strlen(file), err)) {
        printf("load: %s\n", err.c_str());
        return 1;
    }
    ed.gotoSlot(3);
    if (getenv("TC_CURSOR")) ed.setCursorMode(true);
    const tc::CodeGeometry g{96, 17, 8, 398};
    const int n = 2000;
    tc::layoutCode(ed, 0, g, gScr, gScratch);  // warm up
    const double t0 = nowUs();
    for (int i = 0; i < n; i++) tc::layoutCode(ed, i * 0.01, g, gScr, gScratch);
    const double us = (nowUs() - t0) / n;
    printf("layoutCode: %.1f us per call (%d lines)\n", us, gScr.n);
    // edit frames: a jog step on a number (d4's 10), then the layout
    ed.setCursorMode(true);  // still in d4 (gotoSlot(3) again would start a new block)
    ed.act(tc::Action::Right);  // every' | 10
    printf("on: %s\n", ed.cursorInfo().c_str());
    const int m = 500;
    double t1 = nowUs();
    for (int i = 0; i < m; i++) ed.act(i % 2 ? tc::Action::WheelDown : tc::Action::WheelUp);
    const double editUs = (nowUs() - t1) / m;
    t1 = nowUs();
    for (int i = 0; i < m; i++) {
        ed.act(i % 2 ? tc::Action::WheelDown : tc::Action::WheelUp);
        tc::layoutCode(ed, i * 0.01, g, gScr, gScratch);
    }
    const double both = (nowUs() - t1) / m;
    printf("after an edit: %.1f us per layout (the edit itself %.1f us)\n", both - editUs, editUs);
    return 0;
}
