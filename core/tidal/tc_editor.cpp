// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// Builds the shared editor (tidal/editor) into the device editor build
// (-DTIDAL_EDITOR, plan 4d; see core/tidal/README.md).
#ifdef TIDAL_EDITOR
#include "tidal_config.h"
#include "../../tidal/editor/editor.cpp"
#include "../../tidal/editor/editor_menus.cpp"
#include "../../tidal/editor/editor_edits.cpp"
#include "../../tidal/editor/editor_wrap.cpp"
#include "../../tidal/editor/editor_pads.cpp"
#include "../../tidal/editor/editor_nav.cpp"
#include "../../tidal/editor/editor_jog.cpp"
#include "../../tidal/editor/editor_keyboard.cpp"
#include "../../tidal/editor/cursor.cpp"
#include "../../tidal/editor/controls.cpp"
#include "../../tidal/editor/mini.cpp"
#include "../../tidal/editor/layout.cpp"
#endif
