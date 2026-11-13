// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// Text -> tree. Only used when loading .tidal files (and in tests).
#pragma once
#include "tree.h"

namespace tc {

// Parse one expression (e.g. `fast 2 $ s "bd*2"`). Returns the root node or
// 0 with err set. `base` is added to error positions.
uint16_t parseExpr(Program& prog, const char* text, int len, Error& err, int base = 0);

// Parse the contents of a mini-notation string (without the quotes).
uint16_t parseMini(Program& prog, const char* text, int len, Error& err, int base = 0);

// Parse a whole .tidal file into prog (cleared first). Blocks are separated
// by blank lines or by a new statement starting in column 0.
bool parseFile(Program& prog, const char* text, int len, Error& err);

// Convert an error position into 1-based line and column.
void errorLineCol(const char* text, int pos, int& line, int& col);

}  // namespace tc
