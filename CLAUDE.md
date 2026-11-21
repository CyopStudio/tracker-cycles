# Tracker Cycles - Claude Code notes

## Project
A TidalCycles-style live-coding sequencer for the Polyend Tracker, forked
from Tracker CE. Targets the **Original Tracker (NXP MK66FX1M0, Cortex-M4F @
180 MHz, 16 MB SDRAM, mono samples)**. This fork will never be merged back
to upstream CE.
Tracker Mini port (IMX, i.MX RT1051): branch `feature/mini`, notes in
**docs/tidal_mini.md** (`make mini`; one player per track, 44100 Hz clock,
Mini keys via tidal/editor/mini.h; test images
`build/TrackerMini_<Name>RC<n>.ptf`, the Mini updater wants a `TrackerMini`
prefix).

Status: **1.0.1 released** (2026-11-13; 1.0.0 on 2026-11-12). Work for 1.1
is on branches off `develop`; changes go in UPCOMING.md as they land.
The development log (RC1-RC25, benches, tick tests, plan steps 0-4e) is in
**docs/history.md**: read it when you need the why behind a design or a
past measurement.

## Design decisions (settled)
- Clean break: no pattern editor, no song mode, no legacy project loading.
- A "project" is a .tidal file plus associated sample banks (instruments).
  There are no synth presets on this target (see below).
- Code blocks (d1..d8 for the 8 sample tracks, plus the 8 MIDI tracks) are
  evaluated individually, like GHCi: evaluating a block starts/replaces its
  pattern.
- The Tracker sample engine and FX chain replace SuperCollider/SuperDirt.
- Synths: the PolySynth engines are NOT built for the Original Tracker
  (`FEATURE_SYNTH` / `GUI_HANDLE_SYNTH` are IMX-only; libPolySynth.a and
  libPolyDSP.a are not linked by platforms/mk66/makefile). There is nothing
  to drop; any Synth UI/plumbing still compiled for MK66 is just dead weight.
- Goal: support as many Tidal functions as memory/CPU allow. Table-driven
  function library, one small isolated node type per function.
- Syntax: real Tidal syntax (subset); unsupported functions give a clear error.
- Patterns are functions from a time span to events (queryArc-style).
- Editing on device: structured editor (pads + scroll wheel); the text is
  rendered from the tree. Parser only runs when loading .tidal files.
- Tidal 1.10.3 is the reference: golden cases captured from real Tidal
  (GHCi) decide engine behaviour. Randomness is our own (deterministic,
  same probabilities, different values).
- No export / sample recording needed (user): don't test them.

## How it fits together
- **tidal/core** (portable, no heap): tree + parser + renderer
  (docs/tidal_code_tree.md), type checker + Tracker-mode check, engine
  (queryArc, rationals, library table), mapper (Tidal controls -> Tracker
  values, docs/tidal_control_mapping.md), scheduler (docs/tidal_scheduler.md).
- **tidal/editor** (portable, no heap): the editor model shared by the
  prototype and the device: `Editor` (tree mode + cursor mode, menus by
  trial edit, undo checkpoints), split by job: editor.cpp (core: actions,
  undo, edit machinery, evaluate, highlights), editor_menus / _edits /
  _wrap / _pads / _nav (tree mode) / _jog / _keyboard.cpp, cursor.cpp
  (cursor mode); shared helpers in editor_impl.h (tc::ed). controls.cpp
  (`controlCommand()`: every Tracker button -> editor action), layout.cpp
  (code view layout used by both the prototype and the FT812 view).
- **tidal/proto**: SDL prototype (mock front panel, backends: GHCi ->
  SuperDirt, or our engine -> OSC).
- **core/tidal**: device glue built into libTracker.a: unity files
  `tc_*.cpp` (include tidal/core and tidal/editor sources), tidal_player
  (evaluate, mute, tempo, MIDI clock out), tidal_editor (SDRAM storage,
  pacing, autosave, project hooks), tidal_device (DeviceHost / DeviceVoice:
  players, governor, MIDI out), tidal_folders (/Tidal/ sample folders),
  test images (`-DTIDAL_TICK`, `-DTIDAL_BENCH`, ...: core/tidal/README.md).
- **core/interface/tidalEditor**: the FT812 view (bitmap-glyph code view,
  menus, info box, soft keys); module slot 2.
- **core/AudioEngine/mtAudioTidal.cpp**: playerEngine::tidalNoteOn.

Runtime: the scheduler queries on the main loop (lookahead 250 ms, windows
<= 125 ms) into per-slot SPSC rings of 48 B entries; the audio interrupt
(every 2.9 ms) runs due noteOffs, then due noteOns (at most 4 sample starts
per block, MIDI uncapped; a slot's later start that would cut a note
supersedes it). Per sample track: 3 players summed into the track's one
FX chain; `# voices N` (1-3, default 1) per note; a load-driven governor
(16 sounding below 70 % audio load, down to 8 at 85 %); the editor adds
`# voices N` when an edit layers notes (Editor::raiseVoices).

## Editor controls
The full, current table is **tidal/README.md, "Controls (window)"**; the
v2 / v3 UI plans are docs/tidal_ui_v2.md and docs/tidal_ui_v3.md; the user's
device notes are docs/UI-changes.md (the user's file: never commit or
revert it). Main points:
- Every input goes through an Original Tracker control (tidal/editor/
  controls.cpp); prototype keys press a control or do nothing. The
  prototype draws the real front panel (tidal/proto/panel.cpp).
  FX1 = `interfaceButtonVol`, FX2 = `interfaceButtonFx`.
- Two modes: cursor mode (default at boot, token by token, purple cursor)
  and tree mode (Rec toggles; blue cursor). Both edit the same tree; the
  code always stays valid. No ?? holes: Delete unwraps or removes by
  operand rules, never the whole block.
- Play = evaluate (Shift: silence); Pattern + pad = mute, Shift+Pattern +
  pad = solo, Pattern + Screen 8 = unmute all, Shift+Pattern = hush.
  Song = launch menu, Shift+Song = setcps. File = save, Config = Tracker
  mode, Shift+Config = comment, Shift+Master = status, Shift+Smp Load =
  Tidal sample folders.
- Screen 1-8: structure ($ # [] <> {} () | operators); Shift+Screen 1-8:
  * / ! @ , "" (? | .. | <CR>) Ctrl+Z (undo). FX1 = functions, FX2 =
  effects, Note / Instrument = the block's n / s pattern.
- Pads: numbers / notes (chromatic for n / note), instruments in s
  patterns, slots d1-d16 elsewhere and with Shift; chords by holding pads.
- Menus are alphabetical except value lists; a menu with one fitting item
  applies at once. Smoke sessions pick items by label (`pick:LABEL`).
- Reserved / free keys: Params (instrument parameters), Master (master
  settings), Perform (free), Smp Play / Edit / Rec.

## Device lessons (keep in mind for UI / main-loop work)
- FT812: a display control has 8 KB of RAM_G and the whole frame's
  appended display lists must stay under ~8172 B (overlay appended last).
  The code view is two controls (text 3000 / 1500 B per half, backgrounds
  1000 / 600 B); highlight-only frames refresh just the backgrounds.
- cDisplay::update does a few steps per main-loop pass; redraw only the
  controls whose fingerprint changed, never mark dirty every pass.
- Main-loop work while patterns play must yield: menu checks, jog edits,
  folder loading and autosave SD writes run only while the scheduler is
  far enough ahead (`tidalPlayerAheadSeconds()` >= 0.15-0.2 s) and in
  small time slices. Stock file manager steps are ~100 ms (256 KB pieces).
- Menus are built by trial edits (copy, apply, type-check, roll back);
  they are checked lazily on the device and cached by type context.
- Stack: the engine recurses on the C stack (~1.3 KB per nesting level);
  the Profiler keeps a 32 KB stack reserve below the heap.

## Editor UI ideas (parked)
- Pad modes: sample index (`bd:3`), step grid (struct / mask / Bool
  patterns: toggle t / f), note names (`c4`, `fs5`), menu items by pad,
  fractions on time arguments.
- Screen elements on / off (info box, scroll bar) from the Config menu.
- More: docs/retrospective_2026-11.md (Config settings, pages, perform
  punch-ins), docs/tidal_voices.md "V2 (parked)", docs/tidal_sample_
  folders.md (V2 sample folders; loader step 1 is built).

## Engine constraints
- No malloc/new in audio or sequencer real-time paths. Fixed node pools,
  capped nesting depth and events per query.
- Time: rationals (32-bit fast paths, 64-bit fallback, lazy normalising).
- Keep engine code portable (no hardware deps) so it builds on the host.
- Device build: engine at -Os (a pragma in engine.cpp; -DTC_ENGINE_O2
  builds it at -O2, ~1.2-1.3x faster, ~+30 KB flash), its query working
  set (~28 KB) in internal RAM; big pools (program, checker, entries) in
  SDRAM. SDRAM data is never cached on the MK66 (uncached system bus).
- TC_EVENT_SRC=1 on the device (events carry their atoms' node ids for
  highlighting): it changes tc::Event's size, so it is a build-wide flag
  (core/makefile), never set in a header.

## Licensing
- Two licences: the Tidal parts (tidal/, core/tidal/, core/interface/
  tidalEditor/, core/AudioEngine/mtAudioTidal.cpp) are GPL-3.0-or-later +
  a section 7 permission for CE (tidal/LICENSE); CE's files (and our edits
  in them) stay MIT + Wholesale Commercial Clone Restriction (LICENSE).
  New Tracker Cycles files: copy the two-line header (CONTRIBUTING.md).
  tidal/editor/keyboard.h and core/tidal/reverb_bench.* keep their CE /
  MIT licences.
- Do NOT translate or copy TidalCycles (GPL) source. Implement from
  documented behavior and verify against real Tidal output.
- Do not copy Strudel (AGPL) code either.
- Precompiled synth/lib/*.a are Polyend-licensed: don't edit or redistribute
  beyond their license terms.
- CE itself is MIT + "no wholesale commercial clones": don't sell compiled
  .ptf builds of this fork.
- Don't redistribute Polyend's stock samples (the 1.0.0 example project
  was withdrawn for that).

## Environment
- WSL2 Ubuntu. Repo lives in ~/tracker-cycles (NOT under /mnt/c).
- Toolchain: GNU Arm Embedded 10.3-2021.10, installed at
  ~/toolchains/gcc-arm-none-eabi-10.3-2021.10 and on PATH
  (alternatives: ./agt/10.3/ or `make TOOLCHAIN=/path/to/arm-none-eabi-`).
- Needs make, python3 (stdlib only). GHC + Tidal 1.10.3 are installed
  (~/.ghcup; docs/host_setup.md) for golden captures and GHCi checks.

## Build
- `make original` -> build/PolyendTracker_Cycles_<ver>.ptf (the editor
  firmware; test images: CUSTOM_DEFINES=-DTIDAL_TICK etc.)
  (version in version.mk, ORIGINAL_VER_*; currently 1.0.1)
- Host: `make -C tidal test smoke arm` (unit + golden tests, scripted
  prototype sessions, ARM size check); prototype: tidal/run_proto.sh.
- `make clean`, `make version`
- Makefiles don't track flags: `make clean` after changing CUSTOM_DEFINES.
- Confirm `make original` still builds after firmware-side changes. In this
  WSL, cc1plus sometimes crashes ("internal compiler error: Segmentation
  fault") at a different line each time, on the editor's unity file
  (core/tidal/tc_editor.cpp); the same source builds on a retry. Retry;
  and after a crash don't copy build/*.ptf (it is the old one).
- Linker prints region usage (`-print-memory-usage`); for a breakdown run
  `arm-none-eabi-size -A platforms/mk66/build/PolyendTracker.axf`.
- Device test builds: copy the image to `build/PolyendTrac_<Name>RC<n>.ptf`
  for the user (see the .ptf name rule below).

## Layout
- core/      shared app, audio engine, UI (+ synth engines, IMX-only)
- core/source/mtSequencer.*      slim Sequencer shim (live pad notes,
  tempo, FX tables, transport the Tidal player sets); the step sequencer,
  pattern / song editors, performance mode, recorder and export are gone
- core/AudioEngine/              playerEngine voices, mixer/FX, streaming
- core/interface/placeholder/    fills the removed modules' slots
- core/tidal/, core/interface/tidalEditor/: see "How it fits together"
- platforms/mk66/  Original Tracker HAL, drivers, linker script
  (platforms/mk66/3rdparty/teensy_core/mk66fx1m0.ld)
- synth/lib/ precompiled libPolySynth.a, libPolyDSP.a (not used on MK66)
- docs/developer_guide.md  CE architecture, hardware limits, gotchas
- tidal/     our host code: core/, editor/, proto/, tests/, golden/ (cases
  + events captured from real Tidal, `make -C tidal golden`). See
  tidal/README.md.

## Rules / gotchas
- Do NOT change flash ORIGIN addresses in linker scripts.
- .ptf filenames must be 31 characters or fewer, end in .ptf and start with
  `PolyendTrac` or the firmware updater won't list them.
- Recovery: known-good .ptf in /Firmware/ on the SD card + recessed
  reset/bootloader pinhole on the bottom of the unit.
- In core/, use `#ifndef PLATFORM_IMX` for MK66-only code, never
  `#ifdef __MK66FX1M0__` (it is defined for IMX builds too). Use
  `#if GUI_HANDLE_SYNTH`, not `#ifdef`.
- `AUDIO_CORE_RAM` (`.audio_ram_functions`) is linked into FLASH on MK66,
  not RAM; it does not cost SRAM but also gives no speed-up there.
- `__NOINIT(EXTERNAL_RAM)` (.noinit_RAM4) is NOLOAD and not zeroed before
  static constructors: memset / placement-new anything placed there in
  TrackerImpl::init().
  Static constructors run BEFORE SDRAM is initialised: an SDRAM object
  whose type has a constructor (even a default member initialiser) hangs
  the boot (screen stuck at the updater's 100%). Use raw `unsigned char`
  storage + placement-new; check objects for `.init_array`
  (core/tidal/README.md).
- Don't format floats (%f) from interrupt context (newlib heap allocation).
- Don't shift `Interface::modules[]` indices (hard-coded by UI navigation).
- Unity builds: core/tidal/tc_*.cpp include several tidal/ sources into one
  translation unit, so file-static names must not clash between them. A new
  tidal/editor/*.cpp file must also be added to core/tidal/tc_editor.cpp.
- Line endings: many upstream files are CRLF (some mixed) and the git
  config has core.autocrlf=input, so `git add` turns them into LF (a
  whole-file diff). Edit them byte-exact (Python 'rb'/'wb', or Edit) and
  stage them with `git -c core.autocrlf=false add`. Helper (untracked):
  .tc_tools/tcedit.py (edit / remove_lines / remove_function, keeps
  each file's line endings). `make clean` deletes build/ entirely: keep
  nothing else there.

## Git
- origin: https://github.com/CyopStudio/tracker-cycles.git (the user's fork,
  not Polyend's). Pushing uses Git Credential Manager from Git for Windows.
  Push only when the user asks.
- Branches: `main` = releases (no longer mirrors upstream), `develop` =
  integration; new work on branches off `develop` (feature/..., fix/...,
  refactor/...). `feature/tidal` was the 1.0 working branch.
- Release flow: release/x.y.z off develop -> after the user's device pass
  merge into main + tag vx.y.z (the workflow builds and publishes the
  GitHub release) -> merge back into develop. Release notes: write
  docs/releases/v<version>.md before tagging (the workflow uses it as the
  release description, else CHANGELOG.md); move UPCOMING.md's entries into
  CHANGELOG.md.
- Commit small and often. AI Co-authored-by trailers are allowed (CE's
  check removed from build.yml, CONTRIBUTING.md says so).

## Original Tracker budgets
Measured from `make original` on 2026-11-20 (after the editor's cold
files went to -Os):

| Region | Size | Used | Free |
| :--- | ---: | ---: | ---: |
| FLASH | 1024 KB | 956,252 B (91.2 %) | **~92 KB** |
| RAM (internal SRAM) | 256 KB | 189,144 B static | ~73 KB, shared by heap and stack (32 KB stack reserve) |
| EXTERNAL_RAM (SDRAM) | 16 MB | 16,121,504 B | ~640 KB |

- Flash is the tightest budget. Done: editor files that run once per key
  press at -Os (tidal/editor/cold.h: 983,356 -> 956,252 B; largest stack
  frame openWrap 6,072 -> 5,008 B).
- **Parked flash savings** (measured 2026-11-20, symbol sizes; come back
  to these when flash runs short, each removes something the user can
  see, so ask first):

  | What | Saves | Loses |
  | :--- | ---: | :--- |
  | Rest of the editor at -Os (menus, cursor, jog, layout, core) | ~16 KB more | slower menu checks / per-frame layout (the RC2-RC13 stalls) |
  | One font set (Roboto Mono 36 KB / Liberation Sans 40 KB, display.cpp fontsData1 / 2) | 36-40 KB | the Config font choice; check which set the code view's glyphs use |
  | Custom themes (ThemeManager JSON parser) | ~12.5 KB, plus part of scanf / strtod (~10 KB) if nothing else uses it | themes loaded from the SD card |
  | Random name generator (randomNameWord1/2Tab) | 15 KB | generated project / sample names (typing them instead) |
  | Factory test (mtTest) | 6 KB | Polyend's production test only |

- SDRAM is mostly `SAMPLE_MEMORY_SIZE` = 11.25 MB (7.5 MB mono sample pool
  + 3.75 MB Sample Editor FX edit buffer at the top), 1.5 MB delay line,
  768 KB `externalRamScratchBuff`, stream rings, the Tidal program /
  checker / undo / entries. Lowering SAMPLE_MEMORY_SIZE to 31*256*1024
  frees ~3.5 MB (must stay > 3.75 MB because of the edit buffer). Samples
  >= 256 KB stream from SD (One-Shot / loop modes only on MK66; Slice and
  Beat Slice stay fully resident).
- CPU: audio 44,117.647 Hz, 128-sample blocks, interrupt every **~2.90
  ms** (priorities: MIDI UART 0 > SysTick > audio in 4 > audio out 5 > SDHC
  6 > USB 7). The scheduler's tick, MIDI and all DSP share that window; the
  main loop (queries, UI, FT812 over SPI, SD) gets what is left. Silence
  sends no audio blocks, so the floor is ~5 %; a busy 8-track set runs
  60-80 % (docs/tidal_tick.txt). Measure with `Profiler` (Config
  diagnostics) or the tick test image.
- Trigger path: DeviceVoice::noteOn -> playerEngine::tidalNoteOn
  (mtAudioTidal.cpp: per-note overrides on the player's FX lane, every
  event from instrument defaults); MIDI via sendMidiNoteOn/Off, CC, bend,
  program, aftertouch. 48 sample instruments shared by all tracks.
