// Editor timing bench (plan 4d, docs/tidal_device_editor.md section 4),
// shared by the host (`make edbench`) and the on-device editor timing build
// (core/tidal/editor_bench.cpp, -DTIDAL_EDITOR_BENCH). Measures what the
// prototype editor does per key press, done three ways:
//   full   - copy the whole Program, edit the copy, check it (the prototype)
//   block  - copy only the edited block into a scratch Program, edit, check
//   inplace- edit the Program itself, check, undo the edit (rollback)
// for single edits and for menus built by trial (every item edited and
// type-checked). Plain C++, no heap; the caller provides the Programs.
#pragma once
#include <stdint.h>

#include "render.h"
#include "tree.h"

namespace tc {

typedef uint32_t (*EditBenchClock)();  // microseconds, wrapping is fine

// a: the editor's program; b: scratch for whole copies; c: scratch for
// block copies; spans: kMaxNodes entries; text: render buffer (>= 8 KB).
// Appends the report to out (at most cap bytes, NUL-terminated); returns
// its length. `reps` repeats the short operations.
int runEditBench(Program* a, Program* b, Program* c, Span* spans, char* text, int textCap, EditBenchClock clock,
                 int reps, char* out, int cap);

// The block texts the bench uses (also drawn by the device's display test).
int editBenchCaseCount();
const char* editBenchCase(int i);
const char* editBenchCaseName(int i);

}  // namespace tc
