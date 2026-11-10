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

## Reverb bench (CPU vs RAM for the reverb)

```bash
make clean
make original CUSTOM_DEFINES="-DREVERB_BENCH"
cp build/PolyendTracker_CE_*.ptf build/PolyendTrac_ReverbBench.ptf
```

`reverb_bench.cpp` times the reverb (Polyverb, ~18 % of the block in the
audio profiler) in the audio interrupt, after the callback's own work
(outside the Profiler's audio load): each block one variant processes a
128-sample test block, round robin; output discarded, the live reverb
untouched. Three captures of ~10 s (400 runs per variant) at 20, 60 and
100 s after boot; the popups say what to play next (stopped, light,
heavy: SDRAM contention from sample playback changes the SDRAM rows).
Report: `/reverb_bench.txt`, cycles per block and % of the block per
variant, with the internal RAM / SDRAM each would need.

| Row | Variant | Internal | SDRAM |
| ---: | :--- | ---: | ---: |
| 0 | stock, both buffers SDRAM (as shipped) | 0 | 64 KB |
| 1 / 2 | stock, buffer 1 / buffer 2 internal | 32 KB | 32 KB |
| 3 | stock, both internal (both point at one 32 KB: same cost, garbage sound) | 64 KB | 0 |
| 4 | bench engine, full rate, SDRAM (overhead check vs row 0) | 0 | 64 KB |
| 5 | full rate, the 4 input diffusers in internal RAM | 4 KB | 64 KB |
| 6 | half rate (decimate 2, line lengths halved, linear upsample), SDRAM | 0 | 32 KB |
| 7 | half rate, diffusers internal | 2 KB | 32 KB |
| 8 | half rate, all internal | 32 KB | 0 |

Host check (2026-11-10): the bench engine at full rate matches the stock
`Reverb` within 1 LSB; at half rate the tail decays at the same rate
(-60 dB at ~2.1 s for both). Build: internal RAM +38.8 KB (32 KB pool +
4 KB diffusers + state), flash +10 KB; `make original` unchanged.

Results (2026-11-10, docs/reverb_bench.txt), % of the block, the same in
all three captures (playback doesn't slow the SDRAM rows): 0 stock 17.1,
1 buffer 1 internal 11.3, 2 buffer 2 internal 14.4, 3 both internal 8.7,
4 bench full rate 18.2 (bench engine overhead +1.1: separate wet arrays
+ mix), 5 diffusers internal 15.6, 6 half rate 10.4, 7 half rate +
diffusers 8.9, 8 half rate all internal 6.0.

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

v11 (2026-11-10, PolyendTrac_TidalTick11.ptf): v10's sets unchanged;
Program::clear() no longer zeroes the 70 KB SDRAM program (compare the
"evaluate: parse" lines with v10: the silence set was 5.0 ms).

v10 (2026-11-10, PolyendTrac_TidalTick10.ptf): load-driven cap (max 16,
down to 8 as the smoothed audio load goes 70 -> 85 %). 13 sets (~3 min
50 s): 1-5 as before, 6-8 chords / everyday / heavy with voices 3 on
every track under the governor (solo: hushed first), 9-12 listening,
13 MIDI. Report adds `governor:`. Flash 1,012,628 B (96.6 %).

v9 (2026-11-10, PolyendTrac_TidalTick9.ptf): supersede by each start's
`voices`, cap on the streamed path, default cap 14. 15 sets (~4 min 10 s):
as v8, but sets 6-8 = chords at caps 12 / 14 / 16 and sets 9-10 =
everyday with voices 3 at caps 12 / 14. Flash 1,011,292 B (96.4 %).

v8 (2026-11-10, PolyendTrac_TidalTick8.ptf): `voices` (docs/tidal_voices.md
v0.2: default 1 = each note cuts the last), the cap fix, steals fade out
in 5 ms. 14 sets (~3 min 50 s): 1 silence, 2-5 timing with default voices
1 (compare with v6 on the same project), 6-8 ringing chords with voices 3
at caps 8 / 12 / 16, 9 everyday with voices 3 at cap 12, 10-13 listening
(chords, overlapping bass, echoes, `voices "<1 3>"`), 14 MIDI. The single-
control listening sets of v5-v7 are gone. Flash 1,011,052 B (96.4 %),
RAM 198,436 B.

v7 (2026-11-10, PolyendTrac_TidalTick7.ptf): polyphony (docs/tidal_voices.md):
3 sample players per track into the track's chain, at most 16 sounding,
`setSlotPlayers(3)` instead of mono slots. 18 sets: set 6 (new, 20 s)
overfills the cap with 8 tracks of ringing 3-note chords; sets 15-17 are
polyphony listening sets (d4 chords, d5 overlapping bass, d3 echoes).
New report lines: `players:` (most / average sounding at a noteOn,
retriggers of the track's oldest player, cap steals, stale noteOffs,
audio blocks most in use of 70) and `heap:` (in use / free: the tick
build's heap shrank ~6 KB). Flash 1,010,164 B (96.3 %), RAM 198,180 B.

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
