#pragma once
#include "editor.h"
#include "fb.h"

namespace proto {

// Draw the whole editor screen into the framebuffer.
void drawEditor(const Editor& ed, Framebuffer& fb);

}  // namespace proto
