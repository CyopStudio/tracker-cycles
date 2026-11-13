// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
#pragma once
#include "editor.h"
#include "fb.h"

namespace proto {
using namespace tc;  // the editor model and controls (tidal/editor)

// Draw the whole editor screen into the framebuffer.
void drawEditor(const Editor& ed, Framebuffer& fb);

}  // namespace proto
