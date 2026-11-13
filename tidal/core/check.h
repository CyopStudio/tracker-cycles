// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// Type checking and mini-notation atom refinement (spec 3 and 5.4).
#pragma once
#include "tree.h"
#include "types.h"

namespace tc {

// Node::type codes (compact; full types are recomputed by the checker when
// the editor needs them).
constexpr uint8_t kTyUnknown = 0;
constexpr uint8_t kTyPatBase = 1;   // 1 + Base -> Pat <Base>
constexpr uint8_t kTyFn = 8;
constexpr uint8_t kTyList = 9;
constexpr uint8_t kTyPatVar = 10;   // Pat a, a unresolved
constexpr uint8_t kTyTuple = 11;    // (a, b)
inline uint8_t tyPat(Base b) { return (uint8_t)(kTyPatBase + (int)b); }
const char* tyName(uint8_t code);

// Check one block (slot) or the setcps expression. On success every node
// in it has a type code and mini atoms are refined (MWord -> MNote etc.).
// Holes are allowed; `holes` returns how many were found.
bool checkBlock(Program& p, int slot, Error& err, int* holes = nullptr);
bool checkBlockNode(Program& p, uint16_t block, Error& err, int* holes = nullptr);
bool checkCps(Program& p, Error& err);
// Check every block; stops at the first error.
bool checkProgram(Program& p, Error& err);

// Note-name helpers ("c", "cs5", "ef3"; c = c5 = 0)
bool parseNoteName(const char* s, int len, int& semitone);

}  // namespace tc
