# tidal/ — Tracker Cycles host code

Plan steps 0–1 (see `../CLAUDE.md` and `../docs/tidal_code_tree.md`).

| Directory | What | Runs on |
| :--- | :--- | :--- |
| `core/` | Code tree, function library, parser, renderer, type checker. No heap, fixed pools. Will be shared with the firmware. | host + MK66 |
| `proto/` | Editor prototype (step 1): editor model, 800×480 view drawn with the firmware's own fonts, GHCi backend, optional SDL window. | host |
| `tests/` | Core tests (`test_core.cpp`) and scripted editor sessions (`proto/*.keys`). | host |

## Build

```bash
cd tidal
make            # core, tests, headless prototype
make test       # core checks
make smoke      # scripted editor sessions
make arm        # cross-compile core for the Cortex-M4 and print sizes
```

Window (needs SDL2; one-time `sudo apt install libsdl2-dev`):

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
```

Edit `oAddress` in `proto/boot.ghci` to point at SuperDirt (see
`docs/host_setup.md` section 4). `--backend print` (default) just prints
statements; `--backend none` sends nothing.

## Controls (window)

The window shows a **mock Original Tracker panel** around the 800×480
screen: 8 screen buttons, the 25 function buttons, jog wheel and 48 pads.
Each control shows its name, what it does right now, and the keyboard key
that reaches it (purple `S:` = with Shift). Click a control to press it;
click Shift to latch it for the next press. The bottom line shows the last
input, e.g. `key r -> Screen 1 [interfaceButton0] -> Replace`.
`--no-panel` gives the bare screen.

Every key goes through a Tracker control (`proto/hardware.cpp`, the mapping
the device editor will use), so the keyboard can't do anything the
hardware can't.

| Tracker control | Editing | In a menu | Key |
| :--- | :--- | :--- | :--- |
| ← → | previous / next sibling | previous / next item | ← → |
| ↑ ↓ | parent / first child | previous / next item | ↑ ↓ |
| Jog wheel | change value (number, note, sample, function) | move | wheel, `+` `-`, PgUp/PgDn |
| Shift + jog | bigger step (×10, octave, sample `:n`) | ±8 items | Shift + wheel |
| Screen 1 | Replace (type-correct choices) | Cancel | `r` / Esc |
| Screen 2 | Wrap (`fast 2 $ …`, `# gain 1`, `x*2` …) | | `w` |
| Screen 3 | Unwrap (keep the inner pattern) | | `u` |
| Screen 4 | Undo | | `z` |
| Screen 8 | | OK | Enter |
| Play | evaluate current block | OK | Space |
| Shift + Play | hush | | `h` |
| Insert | copy step / element, or add a `# control` | | `i` |
| Delete | delete (step, operand, or leave a `??` hole) | | `x` / Del |
| Pads 1–8 (top row) | select d1..d8 (sample tracks) | | `1`..`8` |
| Pads 13–20 (2nd row) | select d9..d16 (MIDI) | | Shift+`1`..`8` |
| | next / previous slot pad | | Tab / Shift+Tab |
| File | save | | Ctrl+S |
| Config | tracker mode on/off | | `t` |
| (none) | quit | | Ctrl+Q |

Unused for now: Screen 5–7, Params, Perform, Master, Smp Load/Play/Edit/Rec,
Song, Note, Instrument, FX1, FX2, Pattern, Copy, Rec, pads outside the two slot
rows. Pads light like on the device: green playing, amber edited, red
error; the current slot has a white frame.

The panel follows the Original Tracker's front: screen top left with the
8 screen buttons and the 12×4 pads below; on the right three rows of five
(Params…Master, Sample Loader…Song, Note/Instrument/FX1/FX2/Pattern), the
Play/Rec block, and the jog wheel at the bottom. FX1 is
`interfaceButtonVol` and FX2 `interfaceButtonFx` in the firmware (see
`fPatternEdit.cpp`). Layout: `ctlRect()` in `proto/panel.cpp`.

Headless `--keys` tokens: `left right up down wheel+ wheel- wheel++ wheel--
replace wrap insert delete unwrap undo eval hush next prev ok cancel save
tracker`, plus `menu:N`, `shot:FILE.png`, `wait:MS`, `print`, and
`hw:CONTROL` / `hw:shift+CONTROL` to press a Tracker control (`s1`..`s8`,
`play`, `insert`, `delete`, `jog+`, `jog-`, `pad1`..`pad48`, `file`,
`config`, ...; see `proto/hardware.cpp`). `--panel` makes screenshots
include the mock panel. `tests/proto/session3.keys` uses only `hw:` tokens.

## Behaviour notes

- Every edit is type-checked before it's offered; a menu only lists
  choices that check. Holes (`??`) are allowed while editing, but block
  evaluation.
- **Tracker mode** (default on): refuses host-only functions (`chop`,
  `striate`) on evaluate. MIDI slots d9..d16 are sent as
  `dN $ (…) # s "midi" # midichan N-9`; that routing is never stored.
  Not yet emulated: the device's voices per channel (spec: Polyphony in
  CLAUDE.md) and the limited control set.
- `setcps` is sent before the first evaluate and whenever it changes.

## Known gaps (next steps)

- Window mode not yet run by anyone (built and linked; SDL runtime wasn't
  installed when it was written).
- GHCi backend untested against real Tidal.
- No editing of `setcps` or comments in the UI yet; edit the file.
- Candidate menus are built by trial (copy program, check): fine on the
  host, needs a cheaper method on the device.
- Node `type` is a compact code, not the spec's per-program type table.
