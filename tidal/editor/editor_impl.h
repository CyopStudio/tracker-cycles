// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// Internal to the editor's sources (editor*.cpp, cursor.cpp): the includes
// they share and the helpers more than one of them uses. Not part of the
// editor's interface (that is editor.h).
//
// The device builds all of them as one translation unit
// (core/tidal/tc_editor.cpp), so a file-static name must be unique across
// these files; anything used by two files is declared here, in namespace
// tc::ed, and defined once.
#pragma once

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>

#include "../core/check.h"
#include "../core/library.h"
#include "../core/mapping.h"
#include "../core/parse.h"
#include "../core/render.h"
#include "editor.h"
#include "keyboard.h"

namespace tc {
uint16_t stepOf(const Program& p, uint16_t n);  // editor_pads.cpp (also used by cursor.cpp)

namespace ed {
uint16_t firstNumber(const Program& p, uint16_t n);  // editor_edits.cpp
EditSpec spec(EditKind k, uint16_t target, const Text& code);  // editor.cpp
uint16_t valueOf(const Program& p, uint16_t step);  // editor_pads.cpp
const char* controlDefault(const char* name);  // editor.cpp
uint16_t findControlApp(const Program& p, uint16_t n, const char* name);  // editor_pads.cpp
bool nameIn(const char* n, std::initializer_list<const char*> names);  // editor_jog.cpp
}  // namespace ed
using namespace ed;
}  // namespace tc
