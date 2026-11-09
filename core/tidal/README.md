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
