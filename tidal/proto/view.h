#pragma once
#include "editor.h"
#include "fb.h"

namespace proto {
using namespace tc;  // the editor model and controls (tidal/editor)

// Draw the whole editor screen into the framebuffer.
void drawEditor(const Editor& ed, Framebuffer& fb);

}  // namespace proto
