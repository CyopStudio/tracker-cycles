// The device editor's model side (plan 4d, -DTIDAL_EDITOR): the shared
// editor (tidal/editor) with its storage in SDRAM, playing through the
// Tidal player (tidal_player.h). The screen is interface/tidalEditor.
#pragma once

class Platform;

#ifdef TIDAL_EDITOR
namespace tc {
class Editor;
struct CodeScreen;
struct CodeGeometry;
}  // namespace tc

// Main loop: creates the editor, loads /project.tidal once the player runs
// (loading plays nothing), checks menu items a few per pass.
void tidalEditorUpdate(Platform* platform);
tc::Editor* tidalEditorModel();  // null until created
// Something changed that the screen should show (consumes the flag).
bool tidalEditorTakeDirty();
void tidalEditorSetDirty();
// The code area laid out for `g` (SDRAM; valid until the next call).
const tc::CodeScreen& tidalEditorLayout(const tc::CodeGeometry& g);
const char* tidalEditorStatus();  // load message / "" when fine
#else
inline void tidalEditorUpdate(Platform*) {}
#endif
