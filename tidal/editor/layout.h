// The editor's code area as styled lines (plan 4d step 3): what the
// prototype's framebuffer view and the device's FT812 view both draw.
// Portable, no heap; the caller gives the geometry (monospaced columns,
// line height, block spacing, area height) and scratch buffers.
#pragma once
#include <stdint.h>

#include "editor.h"
#include "../core/render.h"

namespace tc {

constexpr int kCodeCols = 128;  // cells per line (a comment line holds its whole text)
constexpr int kCodeLines = 40;  // lines kept (those of blocks that reach the area)

// Slot*: the block's "dN" by its slot's state (the d1-d16 list is gone, UI
// notes Editor14): playing, edited (not sent), error, muted.
enum class CodeStyle : uint8_t { Text, Fn, Num, Word, Quote, Hole, SlotPlay, SlotEdit, SlotErr, SlotMuted };
constexpr uint8_t kCellSel = 1;  // inside the cursor's selection
constexpr uint8_t kCellLit = 2;  // a sounding step (per-event highlighting)
constexpr uint8_t kCellGap = 4;  // the gap cursor's bar is just before this cell

struct CodeCell {
    char ch;
    CodeStyle style;
    uint8_t flags;
};

enum class LineKind : uint8_t {
    Code,     // cells at their columns
    Cps,      // the setcps line (cells, no highlights / gap)
    Comment,  // the comment above a block: one string, dim
    Hint,     // "dN   (empty: ...)", dim; "dN" selected when it's the current slot
};
enum class LineBar : uint8_t { None, Playing, CpsSent, CpsEdited };

struct CodeLine {
    int16_t y;        // px from the area's top (may be < 0 or past the area: clip)
    uint8_t n;        // cells / characters used
    LineKind kind;
    LineBar bar;      // the 2-px bar left of the code
    bool dim;         // an alternative block that isn't dN's selected one
    CodeCell cell[kCodeCols];  // Code / Cps: the cells; Comment / Hint: the text (style Text)
    uint8_t col[kCodeCols];    // Code / Cps: the column of each cell
};

struct CodeBox {  // the current block's background
    bool on;
    int16_t y, h;  // px from the area's top: the block's top and height
};

struct CodeFlash {  // the evaluate flash behind a block (UI v3 C)
    bool on, ok;      // ok: accepted (green), else refused (red)
    int16_t y, h;     // px from the area's top
};

struct CodeScreen {
    int n;
    CodeLine line[kCodeLines];
    CodeBox box;
    CodeFlash flash;
    int16_t scroll;    // px the content is scrolled up (for a scroll bar)
    int16_t contentH;  // px of all the blocks
};

struct CodeGeometry {
    int cols;      // columns that fit
    int lineH;     // px per line
    int blockGap;  // px between blocks (the prototype: 8)
    int areaH;     // px of the code area
};

// Working memory (~44 KB: SDRAM on the device).
constexpr int kLayoutText = 4096;  // characters of one rendered block
struct LayoutScratch {
    Span spans[kMaxNodes];
    char text[kLayoutText + 1];
    CodeStyle style[kLayoutText];
    uint8_t flags[kLayoutText];
    uint16_t row[kLayoutText];
    uint8_t col[kLayoutText];
};

// Lay out every block (file order, setcps first) and scroll so the current
// block is visible. `now` is the highlight clock (EditorOutput::Lit times).
void layoutCode(const Editor& ed, double now, const CodeGeometry& g, CodeScreen& out, LayoutScratch& s);

}  // namespace tc
