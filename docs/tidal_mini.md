# Tracker Cycles on the Tracker Mini

Branch `feature/mini` (off `develop`, 2026-11-21). The Mini image is the
same editor firmware as the Original Tracker's. CE's IMX platform
(`platforms/imx`, i.MX RT1051, Cortex-M7 @ 600 MHz, 64 MB SDRAM, 16 MB
QSPI) was already in the tree, so the port is the Tidal glue, the RAM
budget and the controls.

Build: `make mini` -> `build/TrackerMini_Cycles_<ver>.ptf`. Mini test
images: `build/TrackerMini_<Name>RC<n>.ptf`. The Mini's updater lists
`/Firmware/*.ptf` files whose names start with `TrackerMini`
(platforms/imx/sdmmc/sd/SD.cpp), so the Original's `PolyendTrac` rule does
not apply here.

## What changed

| Area | Original | Mini |
| :--- | :--- | :--- |
| Build flags | `-DTIDAL_EDITOR -DTC_EVENT_SRC=1` (PLATFORM=MK66) | the same for PRODUCT=MINI (core/makefile) |
| Runtime gates | `#ifndef PLATFORM_IMX` | `#if !defined(PLATFORM_IMX) \|\| defined(TIDAL_EDITOR)` |
| Players per track | 3 (`# voices` 1-3, governor 16 -> 8) | **1 for now**: the IMX voice is a stereo L / R pair, no player sum yet. `# voices N` plays as 1 |
| Note volume | player gain into the sum, track amp at unity | the stock `stepVelocity` path |
| Scheduler clock | 750000 / 17 Hz (44117.647) | **44100 Hz** (SAI1 MCLK 11.2896 MHz = 256 x 44100) |
| Query working set (28 KB) | internal RAM (MK66 SDRAM is uncached) | SDRAM: SRAM_DTC is full, and the RT1051's SDRAM is cached (MPU region 8, write-back) |
| Stack painting | `__brkval` .. `_estack` | `_vStackBase` .. `_vStackTop` (fixed heap and stack regions) |
| Sample memory | 11.25 MB | 56 MB (was 60): room for the Tidal pools (~1.6 MB) |
| Synths | not built | still built (FEATURE_SYNTH); Tidal doesn't play them |

Budgets (`make mini`, 2026-11-21): QSPI 10.7 %, SRAM_DTC 94.4 % (stock CE
~94 %), SRAM_ITC 80.9 %, SRAM_OC 70.8 %, EXTERNAL_RAM 94.6 %.

SDRAM on the Mini is set up before the image starts (nothing in the
firmware initialises SEMC), and the image has no `.bss_RAM4`. Keep the raw
storage + placement-new rule anyway: the same sources build for the
Original.

## Controls

The Mini has no pads, no jog wheel and no Pattern / Song / File / Config /
Master / Sample keys. `tidal/editor/mini.h` (`MiniInput`, host-tested in
tidal/tests/test_mini.cpp) maps its keys onto the Original controls, so
everything else in controls.cpp is unchanged.

| Mini key | Does |
| :--- | :--- |
| Screen 1-8, Note, Instrument, FX1, FX2, Play, Rec, Copy, Insert, Delete, Shift, arrows | as on the Original |
| +1 / -1 / +10 / -10 | the jog wheel (the stock firmware sends them as pot steps) |
| Fn1 | Pattern: the mute modifier (Shift+Fn1: solo / hush) |
| Fn2 | Song: launch menu (Shift: setcps) |
| Fn3 | File (Shift: save) |
| Fn4 | Config (Shift: the block's comment) |
| Menu (hold, pick, release) | the stock module menu (Sample Loader, Params, Master, ...) |
| Shift+Menu | the Tidal sample folders (Original: Shift+Smp Load) |
| Middle | OK in menus and the keyboard; elsewhere the chord menu (Original: Shift+Note) |
| Shift+Middle | the status view (Original: Shift+Master) |
| **Shift+Note** | **note entry on / off** (the user's choice): Screen 1-8 are 8 pads of a window |
| (note entry) Left / Right | move the window 1 pad |
| (note entry) Up / Down | numbers / notes: an octave (12); instruments: 8 slots; elsewhere: d1-d8 / d9-d16 |
| (note entry) Shift+Screen 1-8 | select d1-d8 (or d9-d16) |
| Fn1 held + Screen 1-8 | mute d1-d8 (Shift: solo); Up / Down switch to d9-d16; Middle: unmute all |

The window starts at 0 (c4 for `n` / `note`) for numbers and at
instrument 1 for `s` patterns. The rest pad (`~`) is Screen 8 at the top of
the number window. A held screen key releases the pad it pressed even if
the window has moved since, so chords work by holding screen keys. The
soft-key labels show what each screen key presses now (numbers, instrument
names, dN). The Fn keys get their stock Mini functions back when the editor
is left (`initSpecialMiniTrackerButtons()` in stop()).

**Unverified: the Mini's key set.** It is read from CE's Mini key table
(platforms/imx/source/ButtonsImpl.cpp, convertToControlButtons) and its
Mini input code (core/interface/core/interfaceInputs.cpp). That points to
Screen 1-8, Note, Instr, FX1, FX2, Play, Rec, Copy, Insert, Delete, Shift,
the D-pad + Middle, Fn1-4, Menu and +-1 / +-10, with no Pattern key. If a
key is missing or extra on the hardware, `miniKeyOf()` in
core/interface/tidalEditor/tidalEditor.cpp is the only place to change.

## Not done yet

- Polyphony on IMX: 3 stereo players per track summed into the chain
  (AudioVoice's IMX branch, `kTidalPlayers` / `DeviceVoice::kPlayers`).
- The prototype has no Mini panel (tidal/proto/panel.cpp draws the
  Original). The Mini layer is covered by host tests only.
- Pad LEDs: none on the Mini; the editor skips them there.
- USB storage mode: the Tidal hooks (hush, flush, hold the card) are on
  `feature/usb` and are platform-neutral. The Mini needs them before a
  release, so test images are built on `test/mini-acid` (feature/acid,
  which has usb merged, + feature/mini).
- Tracker+ (PRODUCT=PLUS) is untouched: it would need the same makefile
  flag, and it has pads and a jog wheel, so no Mini layer.

## Device checklist (RC1)

1. Boots to the editor; the example project / last project loads.
2. Evaluate a d1 block with Play: sound, highlights in time (44.1 kHz clock).
3. `setcps` changes tempo; MIDI clock out follows (Config).
4. Shift+Note: soft keys show numbers; Screen keys enter them; Up / Down
   move an octave; holding two keys makes a chord.
5. In an `s` pattern, note entry shows instrument names.
6. Fn1 hold + Screen keys mute; Middle unmutes all; Shift+Fn1 solo.
7. Fn2 launch menu, Shift+Fn2 setcps; Shift+Fn3 saves; Fn3 / Fn4 / Menu
   leave and come back with the Fn keys working in the other modules.
8. +1 / -1 / +10 / -10 change values and move in menus.
9. Shift+Menu: Tidal sample folders load (mono instruments in the stereo pool).
10. USB storage mode (Config): playback hushes, the card mounts, the code
    is saved first.
11. Status view (Shift+Middle): audio load, stack depth.
