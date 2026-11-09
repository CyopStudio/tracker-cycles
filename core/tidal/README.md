# core/tidal: the Tidal engine inside the firmware

`tc_*.cpp` each `#include` one source from `tidal/core` (the shared,
host-tested engine), so the core makefile builds them into libTracker.a
with the firmware's flags. `tidal_config.h` is included first: it puts the
engine's event arena in SDRAM (`__NOINIT(EXTERNAL_RAM)`).

For now everything here compiles only with `-DTIDAL_BENCH`; a normal
`make original` contains none of it.

## Timing build (plan step 2)

```bash
make clean
make original CUSTOM_DEFINES="-DTIDAL_BENCH"
cp build/PolyendTracker_CE_*.ptf build/PolyendTrac_TidalBench.ptf
```

`tidal_bench.cpp` runs the shared bench patterns (`tidal/core/bench.cpp`,
the same as `make -C tidal bench`) 10 s after boot and again at 60 s,
shows a popup before and after each run, and writes `/tidal_bench.txt` on
the SD card (run 1 creates it, run 2 appends). Start a project playing
between the two runs so run 2 measures with the audio interrupt busy.
While a run is going the main loop is blocked for a few seconds: the
screen and SD streaming pause; audio keeps running.

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
