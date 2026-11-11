# Tracker Cycles UI v3: staged plan (draft v0.1, 2026-11-11)

Decided with the user: no release yet. Bugs and UI polish first, and the cursor-mode overhaul goes into 1.0.0 too. Inputs: the user's notes in `docs/UI-changes.md` (1.0.0 device pass, RC2-RC7) and the cursor-mode decisions recorded in CLAUDE.md (token steps, jog and pads as in tree mode).

Each stage ends with a device build (`PolyendTrac_CyclesRCn.ptf`), host tests and smoke sessions green, and the user's device notes before the next stage starts. The order puts audio safety first, then the cheap wins, then the big change.

---

## Stage A: audio and correctness (critical)

Status: A1-A3, B1-B2, C done and confirmed on the device (RC10, user: "all those changes work"). Parked until after D (user): the highlights still stop while a menu checks its items while patterns play ("it's ok for the menu to take a second to load, but when the highlights stop it feels like a crash").

**A1. A fast jog drops audio.**
* Cause (from the code): every jog detent is a complete edit (checkpoint, apply, type-check, Tracker check, undo step), and `functPot` runs up to 16 of them per encoder event. Nothing waits for the scheduler, which queries on the same main loop.
* Fix:
  * The device collects jog detents and applies them once per main-loop pass as one edit of N steps (`Editor::wheel(dir * n)`), with one undo step per burst.
  * The edit is skipped (detents kept) while the scheduler is less than 150 ms ahead (`tidalPlayerAheadSeconds`, as the folder loader does).
  * The display shows the result when it catches up.
* Test: spin the jog wheel hard on a number and on an `s` step while 8 patterns play. Nothing should drop.

**A2. Deleting a playing block leaves it playing.**
* Cause: `deleteNode` sends `silence` only if the slot was unedited since its last evaluation (`state == Playing`). An edited block (amber) kept playing.
* Fix: silence dN whenever the block being deleted is the one playing (`playing_[s] == b`). That covers Delete at the top, deleting all of the code, and the empty-block clean-up.
* Smoke session with the engine backend.

**A3. Menus stall while checking (RC7 known issue).**
* A quick filter first: an item whose result type can't match the cursor's type code (`Node::type`, e.g. a `Pat Double` slot and a `Pat String` item) is dropped without a trial edit. The full trial check runs only for the rest, still about 3 ms per pass.
* Optionally, cache the last checked menu per (mode, cursor kind, type code, block text hash), so reopening the same menu is instant.
* Measure on the device how many trial checks this saves.

## Stage B: value editing

**B1. Context-aware jog steps** (user: "no way to edit `# gain 1` to 0.8").

The step comes from what the number means, not how it's written. A table keyed by control or function argument (shift = the bigger step):

| What | Jog | Shift + jog | Range |
| :--- | :--- | :--- | :--- |
| `gain`, `amp`, `velocity` | 0.05 | 0.25 | 0 – 2 |
| `pan`, `shape`, `room`, `size`, `delay`, `begin`, `end`, `legato`, `resonance`, probabilities (`degradeBy`, `sometimesBy`, `chooseBy` …) | 0.05 | 0.25 | 0 – 1 |
| `cutoff`, `hpf`, `bandf` (Hz) | ×2^(1/12) (a semitone) | ×2 (an octave) | 20 – 20000 |
| `speed` | 0.1 | ×2 / ÷2 | |
| `n`, `note`, `up` (and steps of `n` / `note` patterns) | 1 | 12 | |
| `crush` | 1 | 4 | 1 – 16 |
| `fast`, `slow`, `ply`, `hurry`, `segment`, `linger`, `loopAt` | next value in 1/8, 1/4, 1/3, 1/2, 2/3, 3/4, 1, 3/2, 2, 3, 4, 6, 8, 16 | two values on | |
| time shifts (`<~`, `~>`, `off`, `swingBy`, `within` pairs, `trunc`) | 1/16 | 1/4 | |
| counts (`every`, `iter`, `chunk`, `euclid` k / n, `chop`, `striate`, `stut` …) | 1 | 4 | ≥ 1 |
| `setcps` BPM | 1 BPM | 10 BPM | (as now) |
| anything else | the number's last decimal place | ×10 | (as now) |

The result is written with the decimals the step needs: `1` → `0.95`, not `0.9500001`. Steps are exact rationals where the step is one.

**B2. After adding `#` or `$`, the cursor goes to the new function's first parameter** (user note). After a wrap by menu or Insert (`# gain 1`, `fast 2 $ …`), the cursor lands on the first number or string argument of what was added, so the jog edits it at once. It's already done that way for the Shift+Screen 6-8 operator buttons.

## Stage C: feedback

**C1. Flash the block on evaluation** (user note). Pattern briefly lights the evaluated block's background, about 150 ms: green when accepted, red when refused (type or Tracker-mode error). The layout gets a per-block flash (its box with a colour and an end time). The device draws it within the display-list budget.

## Stage D: cursor mode (Rec toggles cursor / tree)

Status (2026-11-11, RC11 `PolyendTrac_CyclesRC11.ptf`, not yet run): built. Quick controls (Shift+Screen 1-3) and the tree refinements (`$` / `#` selectable, Delete there unwraps, `<CR>` before them) were done first. Cursor mode: `tidal/editor/cursor.cpp`. Tokens come from the block rendered with spans (each character's deepest node). Left / Right go token to token; Up / Down go by hard line and column, then to the blocks around. Delete / Shift+Delete (Backspace) / Insert follow D3. Keys that wrap, copy or paste take a function's name as its whole application. The mode shows in the info box. Unit checks in `test_editor.cpp`; smoke session 32 (`--mode cursor`). Also fixed: tree mode's Delete on the outer `#` of a block cleared the slot. Delete dropped the "on the symbol" state before it acted, so the whole expression went; now it unwraps.

**D1. Two modes, Rec switches.** The info box and soft keys show which mode is on. Tree mode stays as it is today (the refinements still to be named by the user, see the questions). Cursor mode behaves like a smart text editor that never makes invalid code.

**D2. Cursor mode navigation (token by token).**
* Tokens are what you'd select with a double click: names (`every`, `fast`, `gain`), numbers, strings' steps (`bd`, `~`, `[`, `]`, `*2`, `<`, `>`), operators (`#`, `$`, `|+`), brackets and parentheses.
* Left / Right go to the previous / next token in text order, across line breaks, within the block.
* Up / Down go to the token at the same column on the line above / below. On the first / last line they go to the previous / next block. Shift+Up / Down are blocks, as now.
* The cursor shows as a box on the token, or a bar between tokens where something can be inserted.

**D3. Cursor mode editing (context decides).**
* Jog / Shift+jog: change the token's value, with B1 steps (as in tree mode).
* Pads: enter values where the cursor is (as in tree mode).
* Delete: remove the token to the right / under the cursor.
  * A step becomes `~`; on a `~` it's removed.
  * An argument: what it belongs to is unwrapped (never a `??` hole, see the questions).
  * On a function's name or a `$` / `#`: unwrap (keep the pattern, drop the function / control).
* Shift+Delete: the same for the token to the left (backspace).
* Insert, by position:
  * at the end of a pattern (after the last token of an expression): `#` and the controls menu (FX2's list);
  * at the start of a pattern, or on a function name: wrap with `$` and the functions menu (FX1 / Screen 1's list);
  * inside `"..."`: insert a step (as now).
* Replace / Wrap / Unwrap disappear as named keys (user: "should be done based on context"). Replace is the jog (values) plus the menus. Wrap is Insert. Unwrap is Delete on a function. Shift+Screen 1-3 are freed (see the questions).

**D4. Under the hood.** Cursor positions map onto the tree: a token is a node (or an operator / bracket of a node), and a bar is a gap (`gapSeq_`, a list gap, or the start / end of an expression). Every edit is still an `EditSpec` on the tree, so undo, checking, highlighting and the device editor all keep working. The layout already knows each node's span and cells, which is where the token list comes from.

**D5. Tests.** Smoke sessions for each key in cursor mode. Unit checks for the token walk (a round trip of Left / Right over every token of the example file, Up / Down over line breaks).

## Stage E: release

The user's device pass on the last RC, then the release flow in CLAUDE.md (develop, `release/1.0.0`, `main` + tag `v1.0.0`).

---

## Answers (user, 2026-11-11)

1. Shift+Screen 1-3: `# gain 0.9`, `# room 0.3`, `# delay 0.3` (quick controls). Undo stays on Shift+Screen 4.
2. Never a `??` hole (done).
3. Tree mode refinements: `$` (and `#` and the other operators) selectable as tokens, so Delete unwraps them and Shift+Screen 5 puts a line break before them.
4. Cursor mode at boot.
5. Jog step table: fine as is.

## Questions for the user (as first asked)

1. **Freed keys:** with Replace / Wrap / Unwrap gone, what should Shift+Screen 1-3 do (Undo stays on Shift+Screen 4)? For example, Shift+Screen 1 could open the Replace menu of values for the token (still useful for picking a function by name), and Shift+Screen 2-3 could become two more quick buttons.
2. ~~Delete on a function argument~~ **Decided:** never a `??` hole ("very hard to fix"). Where a delete would leave one, what the node belongs to is unwrapped; if that can't be unwrapped, it is deleted instead (an operand: the other side stays), never the whole block. Done in both modes (Editor::deleteNode / tryUnwrap).
3. **Tree mode refinements:** which ones, specifically?
4. **Default mode:** cursor mode or tree mode at boot (the last one used is remembered)?
5. **B1 steps:** any values in the table you'd change?
