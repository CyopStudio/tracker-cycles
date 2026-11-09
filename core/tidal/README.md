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
cp build/PolyendTracker_CE_*.ptf build/PolyendTrac_TidalTick.ptf
for o in core/build-MK66-ORIGINAL/obj/tidal/*.o; do
  arm-none-eabi-objdump -h $o | grep -q init_array && echo "static init: $o"; done
```

`tidal_tick.cpp` runs the real scheduler (tidal/core/scheduler.*): queries
on the main loop (`update()`, 4 ms budget per pass, 250 ms lookahead), and
`tidalTick()` in the audio callback right after `sequencer.tick()`, which
pops due entries and calls `instrumentPlayer[slot].noteOn` (the
sequencer's 7-argument path). Load a project with samples first: 15 s
after boot it plays four built-in sets, 20 s each (light; everyday on 8
slots; the heavy bench patterns at cps 0.7; all 8 slots hitting together
for the note-start cap). A `/tidal_tick.tidal` on the card replaces them
with one 60 s set. Don't press Play on the stock sequencer meanwhile.

Mapping (a first cut of 4c): `s` = the instrument whose file name
matches (case-insensitive, no extension), else the instrument at that
name's index in the prototype's default list (bd 1, sd 2, hh 3, ...),
else wrapped onto the active instruments; `n` / `note` add to C5; `gain`
x100 = step volume; looped instruments get a noteOff after the event's
length, one-shots play out. d9..d16 (MIDI) are not mapped yet.

After each set it appends to `/tidal_tick.txt`: tidalTick cycles per block
(all / blocks that started notes), cycles per noteOn, most starts in one
block, lateness, dropped / capped, update() time and its share of the
main loop, the longest main-loop gap, the scheduler's counters, audio
load, and the evaluator's deepest stack vs the heap end.

Cost (2026-11-09, -Os): flash +92.8 KB (903,364 -> 996,180 B, 95.0 %),
internal RAM +55 KB (156,548 -> 213,316 B: checker type pool 28 KB, engine
working set 26 KB),
stack reserve 24 KB (Profiler::init), SDRAM: scheduler 73 KB, live +
edit program, query buffer 54 KB, engine arena.
