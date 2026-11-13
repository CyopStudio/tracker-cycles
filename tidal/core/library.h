// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// Function library table (spec section 7). One row per function, control
// or operator. Ids are table indices and must stay stable: only append.
#pragma once
#include <stdint.h>

namespace tc {

enum class Cat : uint8_t { Core, Time, Structure, Random, Signal, Layer, Sample, Harmony, Control, Operator };
enum class Device : uint8_t { Full, Approx, HostOnly };
enum class Assoc : uint8_t { None, Left, Right };

struct LibEntry {
    const char* name;
    const char* sig;       // spec 3 notation, e.g. "Pat Time -> Pat a -> Pat a"
    Cat cat;
    Device device;
    const char* defaults;  // default args when inserted, e.g. "2" or "3 (fast 2)"
    uint8_t prec;          // operators only
    Assoc assoc;           // operators only (None = not an operator)
    bool fixityChecked;    // fixity verified with :info in GHCi
};

int libCount();
const LibEntry& lib(int id);
int libFind(const char* name, int len);   // -1 if unknown
inline bool libIsOp(int id) { return lib(id).assoc != Assoc::None; }
// Another name for a function listed too (somecycles, eoff ...): it works in
// code, the editor's menus and jog leave it out.
bool libIsAlias(int id);

// Haskell operators handled structurally (not in the table)
constexpr int kPrecDollar = 0;   // $  infixr 0
constexpr int kPrecCompose = 9;  // .  infixr 9

}  // namespace tc
