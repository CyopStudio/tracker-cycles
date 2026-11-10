# tidal/ — Tracker Cycles host code

Plan steps 0–1 (see `../CLAUDE.md` and `../docs/tidal_code_tree.md`).

| Directory | What | Runs on |
| :--- | :--- | :--- |
| `core/` | Code tree, function library, parser, renderer, type checker, and the query engine (`engine.*`, `qtime.h`: tree -> events, 32-bit rational time). The scheduler (`scheduler.*`, docs/tidal_scheduler.md): queries ahead into per-slot rings that an audio-interrupt tick() plays. No heap, fixed pools. Will be shared with the firmware. | host + MK66 |
| `editor/` | The editor model (plan 4d, docs/tidal_device_editor.md): cursor, menus, edits, undo, keyboard screen; no heap (fixed strings / vectors in `fixed.h`), in-place edits with checkpoints, output through `EditorOutput`. The caller provides the Program, Checkpoint and undo arena. `controls.*`: the Tracker control -> editor mapping (`controlCommand()`, `press()`), shared with the device. `layout.*`: the code area as styled lines, drawn by the prototype view and the device's FT812 view. | host + MK66 |
| `proto/` | Editor prototype (step 1): 800×480 view drawn with the firmware's own fonts, backends (print / GHCi / our engine) behind `BackendOutput` (`output.*`: Tidal statements, Save, highlight mapping), optional SDL window. | host |
| `tests/` | Core tests (`test_core.cpp`) and scripted editor sessions (`proto/*.keys`). | host |
| `golden/` | Golden cases captured from real Tidal (see below). | host |

## Build

```bash
cd tidal
make            # core, tests, headless prototype
make test       # core checks
make smoke      # scripted editor sessions
make arm        # cross-compile core + editor for the Cortex-M4 and print sizes
make golden     # recapture golden events from real Tidal (needs GHCi)
make bench      # engine: events, arena peak and time per query, heavy patterns
```

### Golden tests

`golden/cases.txt` lists patterns (`name cycles code`, code is what follows
`d1 $`). `make golden` runs `golden/capture.py`, which queries each one in
real Tidal 1.10.3 (`queryArc` over cycles 0..N) and writes
`golden/expected.txt`: exact rational times (whole and part) and every
control value with its Tidal type. It also writes `golden/types.txt`, the
GHCi `:t` of every library entry. Both are committed; rerun after editing
`cases.txt`.

`make test` checks every case parses, type-checks and renders stably here,
that `expected.txt` matches `cases.txt`, that all times fit 32-bit
rationals, and runs the query engine on every case: its events must match
the captured ones (exact times, exact strings, numbers within 1e-9).
Cases listed in `golden/engine_pass.txt` must keep matching (a ratchet);
`run_tests` names cases that newly match so you can add them.
`GOLDEN_VERBOSE= ./build/run_tests` shows why each other case differs
(`GOLDEN_VERBOSE=stut` limits it to names containing "stut").

Engine status: all 127 cases pass: 113 match Tidal's events exactly, and
the 14 random ones (`?`, `|`, `degradeBy`, `sometimes`..., `rand`,
`irand`) pass the property checks in `golden/random.txt`. Our random
numbers are deterministic like Tidal's (a hash of exact time and a seed)
but not Tidal's values: its generator is only defined in GPL source
(CLAUDE.md "Licensing"). So random cases check behaviour instead: events
are a subset of a reference pattern, counts, the share of events `f` was
applied to, value ranges.

Engine on the MK66 (`make arm`, `make bench`): events are 212 B, the
arena holds 1024 (212 KB, meant for SDRAM: `TC_ENGINE_ARENA_ATTR`), the
engine is ~34 KB of code at -Os (default on ARM; `-DTC_ENGINE_O2` for
-O2, ~67 KB). Values are float or exact rationals, never double. A query
that needs more than the arena fails with "too many events in one query";
the scheduler should then query a shorter window.

Things the captures show the engine must do:
- Control aliases: `hpf` arrives as `hcutoff`.
- Fragments: `|+`, `+|` etc. split events into parts sharing one whole
  (`op_plus_left`); match them, or compare onsets only.
- Floats: compare values with a tolerance (`0.5000000000000001`).
- Signals with no structure (`pan sine` alone) give events without a whole.

Quickest way to open the window (rebuilds first if anything changed):

```bash
tidal/run_proto.sh                     # opens tidal/work.tidal (yours, not in git)
tidal/run_proto.sh my.tidal --backend ghci --boot proto/boot.ghci
make -C tidal run                      # same as the first line
tidal/run_proto.sh --install-shortcut  # "Tracker Cycles" shortcut; on WSL2 on
                                       # the Windows Desktop + Start menu
```

`work.tidal` starts as a copy of `proto/example.tidal`; Ctrl+S saves it.
When started from the shortcut, output goes to `build-sdl/launch.log`.

Building by hand (needs SDL2; one-time `sudo apt install libsdl2-dev`):

```bash
make proto SDL=1 BUILD=build-sdl
./build-sdl/tidal_proto --window proto/example.tidal
```

`BUILD=build-sdl` keeps the SDL build apart from the headless one.

## Run

```bash
# headless: run editor actions, print what would be sent, save a screenshot
./build/tidal_proto proto/example.tidal \
    --keys "down right right right wrap menu:5 ok eval print" --shot /tmp/shot.png

# with real Tidal: needs GHC + Tidal 1.10.3 (docs/host_setup.md) and SuperDirt
./build-sdl/tidal_proto --window --backend ghci --boot proto/boot.ghci my.tidal

# with our engine + scheduler (step 3): OSC straight to SuperDirt, no GHC
./build-sdl/tidal_proto --window --backend engine --osc 127.0.0.1:57120 my.tidal
```

Edit `oAddress` in `proto/boot.ghci` to point at SuperDirt (see
`docs/host_setup.md` section 4). `--backend print` (default) just prints
statements; `--backend none` sends nothing. `--send STMT` (repeatable)
sends a statement at startup, e.g. for scripted runs.

`--backend engine` (proto/engine_backend.cpp) runs the scheduler like the
device: a main-loop thread calls `update()`, an audio thread calls
`tick()` every 128 samples (44117.647 Hz) and sends each due event as a
`/dirt/play` bundle at its exact time + `--latency` (default 0.1 s, as
boot.ghci). `dN $` starts at the next cycle (spec 2.1; Tidal starts
mid-cycle).

`golden/osc_compare.py` checks it against real Tidal: it runs GHCi (with
`streamResetCycles`) and `tidal_proto --backend engine` on the same
statements, captures both OSC streams on local UDP ports (no SuperDirt
needed) and compares every message per slot and cycle (keys, types,
values) plus timing jitter (bundle time - nudge - cycle / cps). Default
statements cover 6 slots; `--stmts FILE` for others, `--keep DIR` saves
both captures. Random functions differ by design (own randomness).

## Controls (window)

The window shows a **mock Original Tracker panel** around the 800×480
screen: 8 screen buttons, the 25 function buttons, jog wheel and 48 pads.
Each control shows its name, what it does right now, and the keyboard key
that reaches it (purple `S:` = with Shift). Click a control to press it;
click Shift to latch it for the next press. The bottom line shows the last
input, e.g. `key r -> Screen 1 [interfaceButton0] -> Replace`.
`--no-panel` gives the bare screen.

Every key goes through a Tracker control (`editor/controls.cpp`, the mapping
the device editor will use), so the keyboard can't do anything the
hardware can't.

| Tracker control | Editing | In a menu | Key |
| :--- | :--- | :--- | :--- |
| ← → | previous / next sibling; on a leaf, or with no sibling on that side: on to the previous / next leaf of the text (into and out of subtrees and `"..."`, whose first / last gap is a stop; within the block); inside `"..."`: step, gap, step, gap … (the gap is a thin bar: pads insert steps there and stay in the gap, Screen 3–7 insert a bracket / alternation / polymeter / Euclid / choice, Delete removes the step before it, ↑ leaves it); inside a Haskell list (`stack [a, b]`, `cat`, `fastcat`, `slowcat`): element, gap, element … (Insert puts a copy of the element before the gap there, Shift+Copy pastes the clipboard there, Shift+Screen 1 opens the Replace list as an "insert" menu, Delete removes the element before the gap (a list keeps one); the new element is selected) | previous / next item | ← → |
| ↑ ↓ | parent / first child; at a block's top (`dN $`): previous / next block | previous / next item | ↑ ↓ |
| → at a block's top | into the block's expression | | → |
| Jog wheel | change value (number, note, sample, function) | move | wheel, `+` `-`, PgUp/PgDn |
| Shift + jog | bigger step (×10, octave, sample `:n`) | ±8 items | Shift + wheel |
| Shift + Screen 1 | Replace (type-correct choices) | Cancel | F1 / `r` / Esc |
| Shift + Screen 2 | Wrap (`fast 2 $ …`, `# gain 1`, `x*2` …) | | F2 / `w` |
| Shift + Screen 3 | Unwrap (keep the inner pattern) | | F3 / `u` |
| Shift + Screen 4 | Undo | | F4 / `z` |
| Shift + Screen 5–8 inside `"..."` | wrap the step in `[ ]`, `< >`, `{ }%4`, `(3,8)`; on the quotes, the whole sequence (`"[bd sd]"`) | | F5–F8 |
| Shift + Screen 5–8 on a pattern | `# <last control>`, `\|+ n 12`, `\|* speed 2`, `0.25 <~` | | F5–F8 |
| Screen 1 `$` | menu: functions applied with `$` (`fast 2 $ …`, `every 4 rev $ …`); from inside `"..."` on the enclosing pattern | | |
| Screen 2 `#` | menu: `# control` (all) | | |
| Screen 3–7 inside `"..."` | menus around the step(s): `[ ]` (group, `[x x]`, `[x x x]`, `[x, x]`), `< >` (`<x>`, `<x ~>`, `<x x>`), `{ }` (`{x}%4` …), `( , )` Euclid (`(3,8)`, `(5,8)`, `(3,8,2)`, `(<3 5>,8)`), `\|` random choice (`[x \| ~]` …) | | |
| Screen 8 `\|+` | menu: `\|+ \|- \|* \|/ \|< \|>` with `n 12`, `note 7`, `speed 2`, `gain 0.8`, `pan 0.5`, `cutoff 1000` | | |
| Screen 8 | | OK | Enter / F8 |
| Play + pad N | mute / unmute dN (keeps its timing, Tidal's `mute`); several pads in one hold are fine | OK | hold Space + digit |
| Shift + Play + pad N | solo dN (everything else muted); again = unmute all | | |
| Play alone (released, no pad) | unmute all | | Space |
| Shift + Play alone | hush | | `h` |
| Delete while Play is held | cancel (releasing Play then does nothing) | | |
| Insert | copy step / element, or add a `# control`; at a block's top: arm a new block (the next slot pad N adds an empty `dN $ silence` above the current block) | | `i` |
| Shift + Insert (Home) | jump to the block's top (`dN $` / `jumpMod N k $`) | | Home |
| Delete | inside `"..."`: a step becomes `~` (timing stays), a `~` is removed, elsewhere: delete (operand, or leave a `??` hole) | | `x` / Del |
| Shift + Delete (Backspace) | unwrap (same as Screen 3) | | |
| Copy | copy the selection (whole pattern on a block) | | Ctrl+C |
| Shift + Copy (Paste) | replace the selection with the copy; a step can be pasted as a pattern (`"bd"`); works on an empty slot | | Ctrl+V |
| Note | jump to the block's `n` / `note` pattern, first step (adds `# n "0"` if there is none): pads then enter notes | | `n` |
| Shift + Note | chord menu on the note at the cursor (in a gap: the step before it): `e'maj`, `e'min`, `'maj7`, `'dom7`, `'min7`, `'dim`, `'dim7`, `'aug`, `'sus2`, `'sus4`, `'six`, `'five`, root as a note name (Tidal: c5 = 0); on a chord also "single note"; not on a range's end. Adds `# voices 3` to a sample block without `voices` | | |
| Pads held together | a chord: a pad entered while another entry pad is still held turns the step before the gap into a stack, `"[0,4,7]"` / `"[bd,sd]"` (up to 8); the first chord on a sample block without `voices` adds `# voices 3` (MIDI blocks play chords anyway). Not on a range or group the entry just ended | | Ctrl+click pads, then a plain click on the last |
| Instrument | jump to the block's `s` / `sound` pattern, first step (adds `# s "bd"` if there is none): pads then pick instruments; not on MIDI blocks | | `s` |
| FX1 | pattern modifiers menu: time, structure, randomness, layering, sample cutting (`fast`, `every`, `jux`, `chop` …) applied with `$` | | `f` |
| FX2 | sound FX menu: `# cutoff`, `# room`, `# delay`, `# crush`, `# shape`, `# pan`, `# gain`, envelopes; from inside `"..."` on the enclosing pattern | | `c` |
| Pattern | evaluate current block | OK | `e` |
| Song | launch menu: `dN $` (now), `jumpMod N 2/4/8/16`, `jumpIn N 1/2/4` (when the next evaluation takes effect) | | `l` |
| Pads in a numeric pattern | enter a number: −12 top left, 0 at the start of the 2nd row, 34, then `~` (rest) bottom right (for `n`/`note`: semitones, a keyboard from c4). Replaces the value, group (`<1 2>`, `0 .. 7`) or quoted pattern at the cursor, then moves to the gap after it, so the next pads insert there (no Delete needed to end entry; Delete in the gap removes the step before). On the quotes this writes a pattern from scratch: pads `0 3 5 7` → `"0 3 5 7"`. On an empty slot, Note / Instrument start a block (`n "0"` / `s "<first instrument>"`) and the pads enter into it. A range fills low, then high. The current value's pad is framed | | click |
| Pads in an `s` / `sound` pattern | instrument mode: pad N = instrument slot N (48, like the Tracker's instruments; names on the pads). Same entry as numbers (replace, then the gap after it); no rest pad, so pad 48 is slot 48. Default slots are Dirt-Samples folders; `--instruments FILE` (one name per line, blank = empty slot) replaces them | | click |
| Pads elsewhere, or Shift + pad | 1–8 (top row) select d1..d8, 13–20 (2nd row) d9..d16: dN's playing block, else its first; the same pad again cycles through dN's blocks | | `1`..`8`, Shift+`1`..`8` |
| | next / previous slot pad | | Tab / Shift+Tab |
| File | save | | Ctrl+S |
| Shift + Song | `setcps` (the Tracker's tempo is on its Song screen; Master stays for the master settings): the cursor goes to its first number (the BPM in `setcps (120/60/4)`; a file without one gets `setcps (138/60/4)`, Tidal's default 0.575); jog changes it, Replace offers tempo numbers, Pattern sends it (the setcps bar turns green; amber = edited, not sent), Undo works; Shift+Song again or a slot pad goes back. Left / Right walk its numbers (120, 60, 4), Up = the setcps line (jog there = the BPM, Replace = whole tempos `130/60/4` ...), Down from the line = the first block, Up from the first block's top = the setcps line, Shift+Insert = the line | | Shift+`l` |
| Shift + Config | edit the comment line above the block on the keyboard screen (the Tracker's naming keyboard: pads are its 42 keys, arrows move the lit key, Screen 1 / Insert type it, Delete = Backspace, jog moves the text cursor, Screen 2 clears, Screen 7 cancels, Screen 8 OK; empty removes the line; only the last comment line is edited) | | `p`; then typing, Enter = OK, Esc = Cancel |
| Config | tracker mode on/off (Params is kept free for the instrument parameters) | | `t` |
| (none) | quit | | Ctrl+Q |

Screen 5–8 are context buttons, labelled on the screen's soft-key bar.
After a wrap that adds a number (`%4`, `(3,8)`, `12`, `2`, `0.25`) the
cursor is on it, so the jog changes it straight away. On a plain value
pattern (e.g. the `"0 .. 7"` in `n "0 .. 7"`) `|+` and `|*` add a bare
number (`|+ 12`); on the quotes themselves the bracket set applies, so use
`|+`/`|*` one level up (`n "0 .. 7" |+ n 12`) or the FX1 menu. `#` adds the control last chosen from a menu (starts as
`gain 1`). No `|-`: use `|+` with a negative number; no `~>`: use `<~`
with a negative shift. Each wrap is type-checked; one that doesn't fit
shows an error.

Unused for now: Perform, Master (kept for the master settings), Smp
Load/Play/Edit/Rec, Rec, pads outside the two slot rows. Purple text on a
button is its Shift function, following the labels printed on the device
(Paste, Home, Backspace). Pads light like on the device: green playing, amber edited, red
error; the current slot has a white frame.

The panel follows the Original Tracker's front: screen top left with the
8 screen buttons and the 12×4 pads below; on the right three rows of five
(Params…Master, Sample Loader…Song, Note/Instrument/FX1/FX2/Pattern), the
Play/Rec block, and the jog wheel at the bottom. FX1 is
`interfaceButtonVol` and FX2 `interfaceButtonFx` in the firmware (see
`fPatternEdit.cpp`). Layout: `ctlRect()` in `proto/panel.cpp`.

Headless `--keys` tokens: `left right up down wheel+ wheel- wheel++ wheel--
replace wrap insert delete unwrap undo eval hush next prev ok cancel save
tracker`, plus `menu:N`, `shot:FILE.png`, `wait:MS`, `print`,
`menulist` (the open menu's items on stdout), `msg` (the status line on
stdout), and
`hw:CONTROL` / `hw:shift+CONTROL` to press a Tracker control (`s1`..`s8`,
`play`, `insert`, `delete`, `jog+`, `jog-`, `pad1`..`pad48`, `file`,
`config`, `copy`, `note`, `instr`, `fx1`, `fx2`, ...; see
`editor/controls.cpp`; `hw:pad13+pad17+pad20` = pads held together, a
chord). New action tokens: `copy paste home notes samples chords
functions controls quick1..quick4 launch cps comment kbdtype kbdok kbdcancel kbdclear`; `type:TEXT` presses the
keyboard screen's pads for TEXT (`_` = space). `--panel` makes screenshots include the mock panel.
`tests/proto/session3.keys` to `session9.keys` use only `hw:` tokens;
`session10.keys` checks Tracker mode (menus per slot kind, the evaluate
error).

## Behaviour notes

- Every edit is type-checked before it's offered; a menu only lists
  choices that check. Holes (`??`) are allowed while editing, but block
  evaluation.
- **Tracker mode** (default on): only what the device can play, by
  `trackerCheck()` in core/mapping.* (the same table the device mapper
  uses, docs/tidal_control_mapping.md section 5): no host-only functions
  (`chop`, `striate`), no `orbit` / `unit`, sample controls (`s`, `pan`,
  filters, `crush`, `speed`, ...) only on d1..d8, MIDI controls
  (`midichan`, `ccn`, `ccv`, `midibend`) only on d9..d16. Menus leave
  out anything that would break that (an edit that would is refused);
  a block that already breaks it (loaded from a file) stays editable and
  evaluate reports the first problem. MIDI slots d9..d16 are sent as
  `dN $ (…) |< s "midi" |< midichan N-9` (a `# midichan` in the code
  wins); that routing is never stored. Not yet emulated: the device's
  voices per channel (Polyphony in CLAUDE.md).
- `setcps` is sent before the first evaluate and whenever it changes.
- **Several blocks per dN** (alternatives): kept in file order with their
  comments; evaluating one makes it dN's playing block (green bar), the
  others are drawn dim. Up / Down at a block's top walk the file's blocks;
  Delete there removes the block and selects dN's other one. A new dN
  (Replace on an empty slot) goes in slot order. Loading plays nothing.
  `session16.keys` covers it.
- **Structure keys on a plain number** (`fast 2`, `every 4`): Screen 3-7
  turn it into a pattern: `"[2 2]"`, `"<2 2>"`, `"{2 2 2}%4"`, `"2(3,8)"`,
  `"[2 | ~]"` (type-checked, so only where a pattern fits).
- **Function arguments** (`often rev`, `every 4 (fast 2)`, sections): on a
  function value the menus that would wrap it list replacements instead:
  Screen 1 / FX1 functions (`fast 2`, `rev`, `(0.25 <~)` ...), Screen 2 /
  FX2 `(# control ...)` sections, Screen 8 `(|+ n 12)` ... `session20.keys`
  covers these and the setcps navigation.
- **Haskell lists**: the `$` menu (Screen 1), FX1 and Wrap end with
  `stack [x, x]`, `cat [x, x]`, `fastcat [x, x]` (the copy is selected to
  edit); list gaps as in the table above; `session18.keys` covers them.
- **Per-event highlighting** (like Pulsar; engine backend only): while
  an event sounds, the mini-notation steps that made it are lit (orange)
  in the slot's playing block: words, numbers, notes, chords, booleans,
  a whole `a .. b` range; `#` / `|+` / struct join both sides (`s "bd sd"
  # n "<1 2>"` lights `bd` and `1`). At least 80 ms, from when it is heard
  (latency included) to its end. Muted slots don't light. A step edited
  since the evaluate stops lighting; the others keep going. How: the
  engine tags each event with its atoms' node ids (`TC_EVENT_SRC`, kMaxSrc
  4; on by default on the host, off on ARM: +8 B per arena event); the
  engine backend maps them to "step k of the statement" (preorder,
  `atomOrder()`) and reports `Highlight`s when the note goes out; the
  editor keeps each evaluated statement's steps (by `stmtHash()`) and maps
  k back to its own nodes. Headless: `lit` prints each playing block with
  the steps lit since the last `lit` as `[[step]]`; `session17` (run with
  `session17.args`: `--backend engine`, OSC to a dead port) covers it.

## Known gaps (next steps)

- Engine backend vs real Tidal (2026-11-09, osc_compare.py): identical
  /dirt/play messages for stut, echo, chunk, iter, arp, whenmod, jux,
  off, every, chop, striate, splice, legato/sustain/nudge/cut/orbit, at
  cps 0.575 and 0.7; timing jitter 0.02 ms (Tidal's own 0.2-0.3 ms).
- GHCi backend checked against real Tidal 1.10.3 (GHC 9.8.2, see
  `docs/host_setup.md`): boot script loads, `setcps`/`dN`/`hush` and the
  d9..d16 MIDI routing are accepted, errors show in the log (`log` key
  token prints it headless). Not yet heard: SuperDirt isn't set up.
- Comments: only the last line above a block is editable on the device
  keyboard (its 42 keys: letters, digits, `- + @`, space); other
  characters from a file are kept. `session19.keys` covers comments and
  `setcps`.
- Candidate menus are built by trial (copy program, check): fine on the
  host, needs a cheaper method on the device.
- Node `type` is a compact code, not the spec's per-program type table.
