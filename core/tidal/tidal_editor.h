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
// The sounding steps changed (highlighting): only the code view needs a redraw.
bool tidalEditorTakeLitDirty();
// The code area laid out for `g` (SDRAM; valid until the next call).
const tc::CodeScreen& tidalEditorLayout(const tc::CodeGeometry& g);
const char* tidalEditorStatus();  // load message / "" when fine
// Status view (Shift+Master): tempo, audio load, voices, stack, per-slot
// state. Opening it also writes the text to /tidal_status.txt.
void tidalEditorToggleStatus();
bool tidalEditorStatusOpen();
int tidalEditorStatusText(char* buf, int cap);  // lines separated by '\n'; returns the length
const char* tidalEditorStatusLines();            // the same, in the editor's own buffer
// Stack: the free gap between the heap and the stack is painted when the
// editor starts; the deepest point anything reached since (main loop and
// interrupts) is the lowest overwritten word.
int tidalStackDeepest();  // bytes below the top of RAM (0 = not painted)
int tidalHeapGap();       // bytes between the heap end and that deepest point
#else
inline void tidalEditorUpdate(Platform*) {}
#endif
