// Mock Original Tracker front panel around the 800x480 screen: screen
// buttons, function buttons, jog wheel and the 48 pads, each labelled with
// what it does right now and which keyboard key reaches it.
//
// The physical arrangement follows the firmware's button groups
// (interfaceDefs.h); see tidal/README.md for what is approximate.
#pragma once
#include <string>

#include "editor.h"
#include "fb.h"
#include "hardware.h"

namespace proto {

constexpr int kPanelW = 1290, kPanelH = 880;

struct PanelState {
    bool shift = false;          // Shift held (or latched by a click)
    Ctl lit = Ctl::Count;        // control to show as pressed
    std::string last;            // "key r -> Screen 1 -> Replace"
};

// Keyboard hint for a control's normal layer, e.g. "Space", "r", "1".
std::string keyHint(const Editor& ed, Ctl c);
// Draws the panel with `screen` (already drawn by drawEditor) inside it.
void drawPanel(const Editor& ed, const Framebuffer& screen, Framebuffer& out, const PanelState& st);
// Control under a panel pixel, or false.
bool panelHit(int x, int y, Ctl& out);

}  // namespace proto
