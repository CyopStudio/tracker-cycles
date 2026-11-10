# core/tidal: the Tidal engine inside the firmware

`tc_*.cpp` each `#include` one source from `tidal/core` (the shared,
host-tested engine), so the core makefile builds them into libTracker.a
with the firmware's flags. `tidal_config.h` is included first: it puts the
engine's event arena in SDRAM (`__NOINIT(EXTERNAL_RAM)`).

For now everything here compiles only with `-DTIDAL_BENCH` or
`-DTIDAL_TICK`; a normal `make original` contains none of it.

## Timing build (plan step 2)

```bash
make clean
make original CUSTOM_DEFINES="-DTIDAL_BENCH"
cp build/PolyendTracker_CE_*.ptf build/PolyendTrac_TidalBench.ptf
# must print nothing: a static constructor on SDRAM data hangs the boot
for o in core/build-MK66-ORIGINAL/obj/tidal/*.o; do
  arm-none-eabi-objdump -h $o | grep -q init_array && echo "static init: $o"; done
```

`tidal_bench.cpp` runs the shared bench patterns (`tidal/core/bench.cpp`,
the same as `make -C tidal bench`) 10 s after boot and again at 60 s,
shows a popup before and after each run, and writes `/tidal_bench.txt` on
the SD card (run 1 creates it, run 2 appends). Start a project playing
between the two runs so run 2 measures with the audio interrupt busy.
While a run is going the main loop is blocked for a few seconds: the
screen and SD streaming pause; audio keeps running.

Each run (v6) then repeats the quarter-cycle queries with the engine's
whole working set (256 events, 1024 controls, 128 parameter values,
~25 KB) in internal RAM (engineUseMemory), as a real build could.

`-O2` variant (engine at -O2 instead of the -Os pragma; ~24 KB more
flash), writes `/tidal_bench_o2.txt`:

```bash
make clean
make original CUSTOM_DEFINES="-DTIDAL_BENCH -DTC_ENGINE_O2"
cp build/PolyendTracker_CE_*.ptf build/PolyendTrac_TidalBenchO2.ptf
cp platforms/mk66/build/PolyendTracker.axf build/PolyendTrac_TidalBenchO2.axf
```

Each run (v4+) also records a PC-sampling profile: the SysTick vector
(1 kHz, above the audio interrupt) is swapped at run time for a handler
that bins the interrupted instruction address (32-byte bins), and the
evaluator's deepest stack point (TC_TRACK_STACK) against the heap end.
Keep the ELF of the build and name the profile with:

```bash
cp platforms/mk66/build/PolyendTracker.axf build/PolyendTrac_TidalBench.axf
python3 core/tidal/bench_profile.py tidal_bench.txt [build/PolyendTrac_TidalBench.axf]
```

Cost of the bench build (2026-11-09): flash +65.5 KB (903,364 -> 970,484 B,
92.6 %), internal RAM +29 KB (mostly the checker's type pool), SDRAM
+403 KB (arena 212 KB, program 70 KB, output buffer 106 KB, report).

## Audio profiler (CPU cost of the stock audio path)

```bash
make clean
make original CUSTOM_DEFINES="-DAUDIO_PROFILE"
cp build/PolyendTracker_CE_*.ptf build/PolyendTrac_AudioProf.ptf
cp platforms/mk66/build/PolyendTracker.axf build/PolyendTrac_AudioProf.axf
```

`audio_profile.cpp` times every stage of the audio callback (midiUpdate,
sequencer.tick, engine.update = the 8 players, AudioStream::update_all =
the audio graph) and every graph node's `update()` with the cycle counter
(hooks in core/source/Tracker.cpp and core/libs/Audio/AudioStream.cpp).
It captures 1024 blocks (~3 s) at 15, 45, 75, 105, 135 and 165 s after
boot, without blocking the main loop, and appends each capture to
`/audio_profile.txt` with the sequencer state; a popup confirms each one.
Change the project / play state / FX between captures.

Name the nodes (class @ owning static + offset) with the matching ELF:

```bash
python3 core/tidal/audio_profile.py audio_profile.txt [build/PolyendTrac_AudioProf.axf]
```

Cost (2026-11-09): flash +2.3 KB, internal RAM +5.3 KB, SDRAM +20 KB. The
timing itself adds ~20 cycles per active node to the graph stage.

## Device tick test (plan step 4b)

```bash
make clean
make original CUSTOM_DEFINES="-DTIDAL_TICK"   # add -DTC_ENGINE_O2 for the -O2 engine
cp build/PolyendTracker_CE_*.ptf build/PolyendTrac_TidalTick2.ptf   # ...TidalTick2O2.ptf
# `make clean` deletes build/: move a finished .ptf out before the next variant
for o in core/build-MK66-ORIGINAL/obj/tidal/*.o; do
  arm-none-eabi-objdump -h $o | grep -q init_array && echo "static init: $o"; done
```

`tidal_tick.cpp` runs the real scheduler (tidal/core/scheduler.*): queries
on the main loop (`update()`, 4 ms budget per pass, 250 ms lookahead), and
`tidalTick()` in the audio callback right after `sequencer.tick()`, which
pops due entries and calls `instrumentPlayer[slot].noteOn` (the
sequencer's 7-argument path). Load a project with samples: once its
instruments are loaded (15 s after boot at the earliest, 60 s at the
latest) it plays four built-in sets, 20 s each (light; everyday on 8
slots; the heavy bench patterns at cps 0.7; all 8 slots hitting together
for the note-start cap). A `/tidal_tick.tidal` on the card replaces them
with one 60 s set. Don't press Play on the stock sequencer meanwhile.

Mapping (a first cut of 4c): `s` = the instrument whose file name
matches (case-insensitive, no extension), else the instrument at that
name's index in the prototype's default list (bd 1, sd 2, hh 3, ...),
else wrapped onto the active instruments; `n` / `note` add to C5; `gain`
x100 = step volume; looped instruments get a noteOff after the event's
length, one-shots play out. d9..d16 (MIDI) are not mapped yet.

v6 (2026-11-09, PolyendTrac_TidalTick6.ptf): the audio floor work
(CLAUDE.md, Polyphony section) is in every build; the tick test adds a
first set of 10 s silence, so its "audio load" line is the idle floor
(v5's sets for comparison: 3 light tracks 59 %, 8 tracks 77-80 %, solo
sets 59-61 %). Results (2026-11-10, docs/tidal_tick.txt): silence 7 %,
light 36 %, solo sets 21-40 %, everyday 74 %, heavy 65 %, 8x x*16 77 %;
no drops. The silent set's stack line now says "not run yet" (it
printed a wrapped number: no query had run).

v5 (2026-11-09, PolyendTrac_TidalTick5.ptf):
- Listening sets one track at a time: 13 sets, 4 timing sets (20 s) then
  set 5's eight tracks each alone (7-11 s, all other tracks hushed first),
  then MIDI (20 s); the popup's second line says what to listen for. A
  solo set starts at the next cycle (up to ~1.7 s of silence first).
  About 3 min 20 s in all.
- MIDI alone: copy docs/tidal_tick_midi.tidal to the card as
  /tidal_tick.tidal; the test then plays only that, 60 s.
- Stack: the type checker (27 KB) lives in SDRAM (TC_CHECKER_ATTR in
  tidal_config.h; it runs on evaluate only) and the stack reserve is 32
  KB. v4 had 1.9 KB of stack left: the reserve only holds at boot, later
  mallocs grew the heap 4 KB into it. Internal RAM 219,524 -> 191,748 B.
  The report times each evaluate (parse / check / scheduler) to show the
  SDRAM checker's cost.

v4 (2026-11-09, PolyendTrac_TidalTick4.ptf): the real control mapping
(plan 4c step 3). The main loop maps events with tidal/core/mapping.*
(`DeviceHost` in tidal_device.* describes mtProject's instruments); the
interrupt plays them with `playerEngine::tidalNoteOn`
(core/AudioEngine/mtAudioTidal.cpp) on tracks 1-8 and MIDI out
(`sendMidiNoteOn/Off`, `sendCC`, `sendPitchbend`) for d9-d16. Six sets:
1-4 as before (timing), then two listening sets, 20 s each:

- Set 5, sample controls, one effect per track:
  - d1 `bd*4 # pan "0 1"`: kicks alternate hard left / hard right.
  - d2 `hh*8 # cutoff (range 300 8000 $ slow 4 sine)`: hats sweep dull to
    bright and back about every 7 s (slight resonance).
  - d3 `sd*2 # crush "<16 8 6 4>"`: snare gets crunchier each cycle.
  - d4 `n "0 .. 7" # s "arpy" # room 0.5 # legato 0.5`: rising 8-note
    run, short notes, in reverb.
  - d5 `bd # speed "<1 2 -1 0.5>"`: per cycle normal, octave up,
    backwards, octave down.
  - d6 `cp*2 # begin "<0 0.25 0.5>" # hpf 400`: thin clap starting later
    into the sample each cycle.
  - d7 `<lt mt ht>*2 # shape 0.6 # delay 0.4`: distorted toms with echoes.
  - d8 `drum*4 # gain "1 0.7 0.85 0.7" # attack "<0 0.05>"`: accent on
    the first hit; every other cycle the hits fade in (50 ms attack).
- Set 6, MIDI next to a kick: d9 on channel 1 plays C E G C (velocity
  alternating soft / loud, short notes); d10 on channel 2 plays C major /
  E minor chords, one per cycle, with CC 74 sweeping 0-127.

Names follow the project's sample file names first; with no match the
test falls back to the default-list position (bd = 1st instrument, ...).
The report adds overrides per note and per-slot mapping warnings
(unknown `s` names, too many controls, filter clashes).

v3 (2026-11-09): update() returns at once while no slot needs a query
(report: "idle update() calls"); build as PolyendTrac_TidalTick3.ptf.

v2 (2026-11-09): scheduler object in internal RAM (ring entries in
SDRAM), mono sample slots (`setMonoSlots`: of several starts due in one
block only the last plays), set 1 timing fixed.

After each set it appends to `/tidal_tick.txt` (`/tidal_tick_o2.txt` for
the -O2 variant): tidalTick cycles per block (all / idle blocks / blocks
that started notes), cycles per noteOn, superseded starts, per-slot query
window time (avg / max, wall time), mapping cycles per event, most starts in one
block, lateness, dropped / capped, update() time and its share of the
main loop, the longest main-loop gap, the scheduler's counters, audio
load, and the evaluator's deepest stack vs the heap end.

v2 cost: flash 996,748 B (-Os) / 1,020,940 B (-O2, 97.4 %), internal RAM
217,156 B (+3.8 KB scheduler object). v1 cost (2026-11-09, -Os): flash +92.8 KB (903,364 -> 996,180 B, 95.0 %),
internal RAM +55 KB (156,548 -> 213,316 B: checker type pool 28 KB, engine
working set 26 KB),
stack reserve 24 KB (Profiler::init), SDRAM: scheduler 73 KB, live +
edit program, query buffer 54 KB, engine arena.
