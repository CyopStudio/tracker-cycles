# Tracker Cycles - Claude Code notes

## Project
A TidalCycles-style live-coding sequencer for the Polyend Tracker, forked
from Tracker CE. Targets the **Original Tracker (NXP MK66, Cortex-M4F @
180 MHz)**. This fork will never be merged back to upstream CE.

## Design decisions (settled)
- Clean break: no pattern editor, no song mode, no legacy project loading.
- A "project" is a .tidal file plus associated sample banks / synth presets.
- Code blocks (d1..d8 for sample channels, 8 MIDI channels) are evaluated
  individually, like GHCi: evaluating a block starts/replaces its pattern.
- The Tracker sample engine and FX chain replace SuperCollider/SuperDirt.
- Synths may be dropped to free flash/RAM for more Tidal functions.
- Goal: support as many Tidal functions as memory/CPU allow. Table-driven
  function library, one small isolated node type per function.
- Syntax: real Tidal syntax (subset); unsupported functions give a clear error.
- Patterns are functions from a time span to events (queryArc-style).
- Editing on device: structured tree editor (pads + scroll wheel); the text
  is rendered from the tree. Parser only runs when loading .tidal files.

## Plan
1. Host-side C++ engine (tidal/ dir) with desktop test runner and golden
   tests (event lists captured from real Tidal output).
2. Memory/CPU audit: fixed arenas, bounded worst-case query cost.
3. Editor prototype (web) against real Tidal + SuperDirt.
4. Swap backend to our engine -> OSC -> SuperDirt to validate it.
5. Integrate into firmware sequencer clock, then device editor UI.

## Engine constraints
- No malloc/new in audio or sequencer real-time paths. Fixed node pools,
  capped nesting depth and events per query.
- Double-buffer pattern arenas per channel; swap at cycle boundary.
- Time: rationals (32-bit, GCD-normalized) unless benchmarks say otherwise.
- Keep engine code portable (no hardware deps) so it builds on the host.

## Licensing
- Do NOT translate or copy TidalCycles (GPL) source. Implement from
  documented behavior and verify against real Tidal output.
- Do not copy Strudel (AGPL) code either.
- Precompiled synth/lib/*.a are Polyend-licensed: don't edit or redistribute
  beyond their license terms.

## Environment
- WSL2 Ubuntu. Repo lives in ~/tracker-cycles (NOT under /mnt/c).
- Toolchain: GNU Arm Embedded 10.3-2021.10 (arm-none-eabi-gcc on PATH).
- Needs make, python3.

## Build
- `make original` -> build/PolyendTracker_CE_<ver>.ptf
- `make clean`, `make version`
- Confirm `make original` still builds after firmware-side changes.

## Layout (inherited from Tracker CE)
- core/      shared app, audio engine, UI, synth engines
- platforms/mk66/  Original Tracker HAL, drivers, linker scripts
- synth/lib/ precompiled libPolySynth.a, libPolyDSP.a
- docs/developer_guide.md  architecture, hardware limits, gotchas

## Rules / gotchas
- Do NOT change flash ORIGIN addresses in linker scripts.
- .ptf filenames must be 31 characters or fewer.
- Recovery: known-good .ptf in /Firmware/ on the SD card + recessed
  reset/bootloader pinhole.

## Git
- main mirrors upstream (ff-only), develop branches off main,
  feature/tidal is the working branch. Commit small and often.

## TODO (fill in after reading docs/developer_guide.md)
- Flash/RAM budget for the Original Tracker
- CPU budget split (audio / UI / sequencer)
- How the sample engine triggers voices; which Tidal controls map to it
