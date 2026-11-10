// Builds the shared editor (tidal/editor) into the device editor build
// (-DTIDAL_EDITOR, plan 4d; see core/tidal/README.md).
#ifdef TIDAL_EDITOR
#include "tidal_config.h"
#include "../../tidal/editor/editor.cpp"
#include "../../tidal/editor/controls.cpp"
#include "../../tidal/editor/layout.cpp"
#endif
