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
make test       # 236 core checks
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

Keyboard stands in for the Tracker until the device UI exists.

| Key | Action | Tracker idea |
| :--- | :--- | :--- |
| ← → | previous / next sibling | arrows |
| ↑ ↓ | parent / first child | arrows |
| mouse wheel, `+` `-` | change value: numbers, notes, samples, function name; moves in menus | jog wheel |
| Shift + wheel | bigger step (×10, octave, sample index `:n`) | Shift + jog |
| `r` | replace menu (only type-correct choices) | |
| `w` | wrap menu (`fast 2 $ …`, `# gain 1`, `x*2`, `[x x]` …) | |
| `i` | insert: copy step / list element, or add a `# control` | |
| `x` / Del | delete (step, operand, or leave a `??` hole) | |
| `u` | unwrap (keep the inner pattern) | |
| `z` | undo | |
| Enter | apply menu choice | jog push |
| Esc | close menu | |
| Space | evaluate current block | Play |
| `h` | hush | Stop |
| Tab / Shift+Tab | next / previous slot (d1..d16) | |
| `t` | tracker mode on/off | |
| Ctrl+S / Ctrl+Q | save / quit | |

Headless `--keys` tokens: `left right up down wheel+ wheel- wheel++ wheel--
replace wrap insert delete unwrap undo eval hush next prev ok cancel save
tracker`, plus `menu:N`, `shot:FILE.png`, `wait:MS`, `print`.

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
- Spec deviations to fold back into `docs/tidal_code_tree.md`: `setcps`
  holds an expression; ratios like `(1/8)` are `Op` nodes, not a `Num`
  format; `MRange` has children lo and hi; `MNote.b` keeps the original
  spelling.
