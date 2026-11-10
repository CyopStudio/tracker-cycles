#pragma once
#include "editor.h"
#include "fb.h"

namespace proto {
using tc::Action;  // the editor model (tidal/editor)
using tc::Editor;
using tc::SlotState;

// Draw the whole editor screen into the framebuffer.
void drawEditor(const Editor& ed, Framebuffer& fb);

}  // namespace proto
