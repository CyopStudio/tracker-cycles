# Tracker Cycles - Claude Code notes

## Project
A TidalCycles-style live-coding sequencer for the Polyend Tracker, forked
from Tracker CE. Targets the **Original Tracker (NXP MK66FX1M0, Cortex-M4F @
180 MHz, 16 MB SDRAM, mono samples)**. This fork will never be merged back
to upstream CE.

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
- Editing on device: structured tree editor (pads + scroll wheel); the text
  is rendered from the tree. Parser only runs when loading .tidal files.

## Editor controls (prototype, agreed with the user)
- Every input goes through an Original Tracker control: the button ->
  editor mapping lives in tidal/proto/hardware.cpp (`controlCommand()`) and
  is meant to be reused by the device editor (4d). Keyboard keys either
  press a fixed control or the control that does that job right now; a key
  with no Tracker equivalent does nothing. Full table: tidal/README.md.
- The prototype window draws a mock front panel (tidal/proto/panel.cpp)
  matching the real unit: screen top left, 8 screen buttons under it,
  12x4 pads below; right side three rows of five (Params..Master,
  Sample Loader..Song, Note/Instrument/FX1/FX2/Pattern), then Play/Copy/
  Insert/Up/Delete and Rec/Shift/Left/Down/Right, jog wheel bottom right.
  FX1 = `interfaceButtonVol`, FX2 = `interfaceButtonFx` (fPatternEdit.cpp).
- Mapping (editing / in a menu):
  - Arrows: siblings, parent / first child (menus: move). Jog: change
    value; Shift+jog bigger step.
  - Screen 1-4: Replace / Wrap / Unwrap / Undo (menus: Screen 1 Cancel,
    Screen 8 and Play = OK).
  - Screen 5-8 depend on context: inside "..." (a step, or the quotes =
    whole sequence) `[ ]` `< >` `{ }%4` `(3,8)`; on a pattern
    `# <last control>` `|+ n 12` `|* speed 2` `0.25 <~` (bare numbers on
    plain value patterns). Cursor lands on the new number for the jog.
    No `|-` / `~>`: use negative numbers.
  - Play = evaluate block, Shift+Play = hush.
  - Delete inside "...": step -> `~` (keeps timing), `~` -> step removed.
  - Shift functions follow the printed labels: Copy / Shift+Copy = Paste,
    Insert / Shift+Insert = Home (block top), Delete / Shift+Delete =
    Backspace = Unwrap.
  - Note / Instrument / FX1 / FX2: notes, samples, functions, # control
    menus (filtered, type-checked). Pattern: launch menu (`dN $`,
    `jumpMod N k $`, `jumpIn N k $`; spec 2.1).
  - Pads by context: inside a numeric pattern (Time/Double/Int/Note) or on
    a number argument they enter numbers, -12 top left .. 34, bottom right
    = rest `~` (0 starts row 2; for n/note a chromatic keyboard), replacing
    the value / group at the cursor, then selecting the next step's value
    or appending a copy at the end (tracker-style entry); Delete removes
    that appended copy and ends entry. In an s / sound pattern the pads are
    the 48 instrument slots (same entry, no rest pad; prototype default =
    Dirt-Samples names, `--instruments FILE`). Elsewhere, and always with Shift, pads
    1-8 = d1..d8, 13-20 = d9..d16 (lit by slot state). Keyboard digits are
    Shift+pad.
  - File = save, Config = tracker mode. Free: Params, Perform, Master,
    Sample *, Song, Rec, other pads.
- Launch: tidal/run_proto.sh (rebuilds, opens tidal/work.tidal, gitignored)
  or `make -C tidal run`; `--install-shortcut` makes Windows Desktop +
  Start menu shortcuts via wslg.exe (WSLg doesn't list .desktop apps here).
  `--panel` screenshots and `hw:` key tokens for headless sessions.

## Editor UI TODO (parked)
Parked until we know the engine + editor fit on the MK66 (flash, RAM,
query CPU; plan step 2 timing check). Don't build these before then.
- Pad modes (pads already do numbers, instruments, slot select):
  - Sample-index mode: on a sample step, pads set the `:n` (`bd:3`).
  - Step-grid mode: on a whole sequence or a `struct` / `mask` pattern,
    the first 16 pads are steps; a press toggles hit / rest (`t` / `f`).
  - Note-name mode: on `note` patterns, same keyboard layout but entering
    `c4`, `fs5` ... instead of numbers.
  - Menu mode: while a menu is open, pad N picks item N directly.
  - Fraction mode: on time arguments (`fast`, `<~`, `%`), pads hold
    common values (1/8, 1/4, 1/3, 1/2, 3/4, 1, 1.5, 2, 3, 4, 8, 16).
- Device cost to check first: menus are built by trial (copy program +
  type-check per candidate), fine on the host, too slow / big for the
  MK66 as is.

## V2 idea: Tidal sample folders (not for V1)
V1 = 48 instruments loaded the normal Tracker way. V2 could load Tidal-style
folders listed in the project (e.g. a manifest) on project load.
- Folder of short files -> ONE sliced instrument: files sorted by name (as
  SuperDirt does), mono, joined end to end, a slice point per file
  (MAX_SLICE_NUMBER = 48, slice positions are uint16 fractions of the
  length: pad joins with a little silence). `s` picks the instrument by
  name, `n` picks the slice modulo the file count (SuperDirt semantics),
  slicePlayThru = 0 gives "play this file to its end".
- Long files -> one streamed instrument each (only a ~743 ms RAM head,
  `kStreamPreCacheFrames`, 64 KB mono). Uses up instrument slots faster.
- On MK66, Slice / Beat Slice can't stream (`isPlayModeStreamable()`); the
  IMX has per-slice pre-cache. Porting it gives ~680 frames (~15 ms) of
  head per slice with 48 slices: likely too short to hide an SD seek
  without prefetch.
- Budget: 7.5 MB sample pool ~ 89 s mono 44.1 kHz (~125 s with the
  SAMPLE_MEMORY_SIZE reclaim). A small host tool should pre-convert
  folders to mono 44.1 kHz WAV so project load is just reads.
- Streaming helps Tidal: events shorter than the RAM head never read SD;
  mid-file starts (`begin`, `chop`, `striate`, `splice`) do. Our look-ahead
  scheduler (4b) knows upcoming starts, so a `VoiceStreamManager` prefetch
  call could load them before the event fires.
- Measure first: random 8 KB SD read latency on MK66, and how many
  streaming voices run without underrun (16 voices ~ 1.4 MB/s + seeks).
- Keep in V1: the engine resolves `s` name -> instrument, then `n` ->
  slice on a sliced instrument, else pitch. Then V2 is mostly a loader.

## Plan
0. Shared code-tree spec, used by both the editor and the engine: node
   types, tree -> Tidal text rendering, text -> tree parsing, per-node size
   limits. Do this before 1 so step 3 isn't a rewrite.
   Spec: docs/tidal_code_tree.md (draft v0.1, targets Tidal 1.10.3).
1. Editor prototype: standalone C++ app -> real Tidal (GHCi) -> SuperDirt.
   - Draw against a small display interface at the Tracker's 800x480; map
     pads/encoder/buttons to keys. Only the drawing layer gets rewritten
     for the FT812 later.
   - Tidal text output sits behind a "backend" interface (swapped in step 3).
   - No allocation in the editor model (same rules as the engine).
   - "Tracker mode": restrict SuperDirt to what the device can do (voices
     per channel as in Polyphony below, only mapped controls, no arbitrary
     playback rate); anything else is an error.
2. Host-side engine (tidal/ dir), can run in parallel with 1:
   - Desktop test runner + golden tests: event lists captured from real
     Tidal by a capture script (needs host GHC + Tidal; no SuperDirt).
     Done: tidal/golden/ (capture.py, types.txt) and a first query engine
     (tidal/core/engine.*): 113/127 golden cases match exactly; the 14
     random ones pass property checks (golden/random.txt). Decided: our
     own deterministic randomness (hash of exact time + seed; each ? / |
     seeded by its position), same probabilities as Tidal's docs, values
     differ from Tidal's. Shrunk for the MK66 (2026-11-09): events 212 B
     (values float or exact rational, control map entries 8 B), arena
     1024 events = 212 KB (put it in SDRAM via TC_ENGINE_ARENA_ATTR),
     engine ~34 KB text at -Os (pragma on ARM; TC_ENGINE_O2 for -O2,
     ~67 KB), no double. `make -C tidal bench`: heaviest pattern peaks at
     384 events per one-cycle query (97 per quarter cycle), host ~175 us /
     cycle at -O2 (-Os ~1.5x slower on the host).
     Timing build (core/tidal/README.md, `-DTIDAL_BENCH`; flash +65.5 KB,
     internal RAM +29 KB, SDRAM +403 KB). First device result (2026-11-09,
     -Os, run 1 ~50% audio load / run 2 heavy project ~77%): ~600-1000x
     slower than the host. Worst (stut 8, 128 events) 215 ms per cycle
     (run 2: 772 ms; quarter cycle 202 ms); simplest pattern 2.4 ms / 8 ms.
     Run 2 / run 1 = 3.6x, more than the CPU share alone (~2.2x): memory
     contention suspected (arena + program in SDRAM). Main suspect: every
     Q op does a 64-bit Euclid GCD (`qgcd`) = software __aeabi_ldivmod on
     the M4. Needs >= 20-50x before 4b; fix rationals first, then re-bench.
     Rationals fixed (2026-11-09): 32-bit hardware-divide fast paths, no
     GCD for integer + fraction, cross-reduced * and /, 64-bit only on
     overflow fallback (random test vs a 128-bit reference); host 1.4-1.6x.
     All SDRAM data is uncached (engine arena/program, samples, delay
     line; see below).
     Stack: the engine recurses on the C stack (one Eval::callLib per
     nested function). callLib's frame was 4 KB (Pat arrays in switch
     cases) + 4.7 KB lambda; moved those cases to noinline helpers ->
     ~1 KB, ~2.5 KB per nesting level. A bench build with only ~35 KB
     heap/stack gap hung in run 1. Keep checking .su files (-fstack-usage)
     and the bench's "deepest stack" line; budget the stack before 4b.
     Bench v2 (2026-11-09, docs/tidal_bench.txt): rationals fix gave only
     1.5-2.4x (stut 8: 215 -> 89 ms/cycle run 1, 372 ms run 2; q4 23 /
     98 ms). Arena in internal RAM: another 1.5-2x (q4 stut 15 / 65 ms).
     Run 2 / run 1 ~4.2x even with the internal arena (CPU share alone
     ~2.1x: flash/code-cache thrash by the ISR?). Per event ~67k cycles on
     the M4 vs ~3k on the host. Host perf: rationals still >50% of the
     time (qgcd32 26%). Heap at runtime ~30 KB: the bench build had only
     10.5 KB heap/stack gap and its stack measurement saturated (used it
     all). Real free internal RAM in a normal build ~74 KB minus stack.
     No SDRAM data cache on the MK66: LMEM has only the code-bus cache
     (8 KB, also caches flash code); the system bus (0x70000000) is never
     cached (NXP forum; the SDK header has only PC* registers). Possible
     route: SDRAM code-bus alias at 0x08000000 (AN4745, K65; write-through,
     PCCRMR R1). Bench v3 probes it safely and times a pass through it.
     Lazy rationals (no GCD while denominators <= 2^16, == cross-
     multiplies, queryPattern returns normalised times): host stut 8
     165 -> 73 us/cycle since the start.
     Bench v3 (2026-11-09): alias NOT readable (bus fault) -> no way to
     cache SDRAM data on the MK66. Lazy rationals: no device gain (stut 8
     97 ms/cycle run 1, 364 ms run 2; q4 25 / 96 ms), so the device cost
     is not arithmetic: suspect memory (uncached SDRAM arena + program,
     212 B events copied per layer) and flash code fetch (37 KB engine vs
     8 KB code cache shared with the audio ISR). Measure before changing:
     device PC-sampling profile.
     Heap: Profiler::init() mallocs all free RAM at boot and frees it, so
     the heap ends 8 KB (teensy STACK_MARGIN) below the boot-time stack;
     freed .bss becomes free heap, not stack. The bench's painted gap is
     therefore always ~10.6 KB; "saturated" = the stack went at least that
     deep, into malloc's free space. Measure engine stack via SP tracking.
     Bench v4: PC-sampling profile (SysTick vector swapped at run time;
     core/tidal/bench_profile.py) + evaluator SP tracking.
     v4 results (2026-11-09, docs/tidal_bench.txt): timings as v3. Profile
     run 1 (~10k samples): audio ISR ~45% (reverb alone 18%), engine code
     29%, memmove/memcpy/memset 26% (nearly all engine: ~45% of the
     engine's time is moving 212 B events in uncached SDRAM; drop()
     memmoves the arena tail, Event() zeroes 20 ctl slots), rationals ~10%
     (~15% of the engine). Run 2: audio ~75% of all samples, engine gets
     the rest. Stack: evaluator deepest 22.9 KB below the top of RAM, 10.6
     KB INTO the heap (heap ends 12.3 KB below the top) -> must fix before
     a real build (smaller frames / iterative evaluation + reserve a stack
     margin when Profiler::init claims the heap).
     Next: slim events (control maps out of line, ~40 B events, no full
     zeroing), no tail memmove in drop(); and cut the audio floor (the
     engine only gets what the ISR leaves).
     Memory rework (2026-11-09): internal events 52 B (Ev; control maps
     out of line in a copy-on-write pool, kCtlPool 8192 x 8 B, reset per
     query; queryPattern still returns the 212 B public Event); withParam
     moves its parameter values to a small stack (kParamStack 256) so
     results land in place (no memmove). Host peaks: arena 384 events =
     19 KB one-cycle, pool 2432 entries (quarter cycle ~5 KB + ~5 KB:
     could live in internal RAM). Stack per nesting level ~1.3 KB (was
     ~2.4 KB): callLib reads args in place (592 B), qApp sizes its arg
     array with alloca, qNode 520 B. Bench builds: Profiler::init leaves a
     32 KB stack reserve (TIDAL_BENCH) instead of 8 KB.
   - Cross-compile with arm-none-eabi-gcc throughout; track flash size.
   - Early on-device timing check: once a few dozen functions work, flash a
     throwaway build that times queries on the MK66 (32-bit rationals at
     180 MHz is the biggest technical risk).
3. Swap the prototype backend to our engine -> OSC -> SuperDirt. Validate by
   recording Tidal's OSC and ours for the same code and comparing
   timestamps, controls sent, cps/cycle position.
4. Port to the Tracker:
   - 4a. Strip firmware: pattern editor, song mode, step sequencer, legacy
     import/export; stub their callers. Independent, can start any time;
     riskiest firmware change (the sequencer has many callers).
   - 4b. Scheduler: queries on the main loop a fixed window ahead into a
     fixed-size event queue; the audio ISR only pops due events.
   - 4c. Control mapping to playerEngine noteOn/mod*() calls, MIDI out for
     MIDI channels, and the per-channel voice allocator (Polyphony below).
   - 4d. Device editor: step 1 editor logic + FT812 drawing layer +
     pad/encoder input.
   - 4e. Project files: .tidal file + sample banks load/save on SD.

## Polyphony (planned)
- Today: 8 sample voices, one per track (voice index == track index).
- Target: 2 voices per sample channel (16 total), alternating, so a cut
  note keeps its release while the next one starts ("release tails"). Covers
  overlapping legato/sustain, long samples, stut/echo.
- Fallback if CPU is short: per channel, 2-3 cheap players (player, amp,
  pan) summed into ONE filter + bit-crusher; cutoff/crush become
  per-channel (latest event wins), like SuperDirt per-orbit FX.
- Full voices x3 (24) only if measurements show lots of headroom.
- Cost: ~1.7 KB internal RAM per voice (`instrumentPlayer[8]` = 13.8 KB) +
  mixer channels; 32 KB SDRAM ring buffer per streaming voice. CPU unknown.
  Streaming many voices from SD at once may underrun on MK66; consider
  limiting extra voices to resident samples.
- Work: our scheduler owns a small voice pool per channel and steals the
  oldest; decouple voice index from track index (mute/level, RMS meters,
  stem export, mixer channel count `MixerAndFxs<kVoices>`).
- Measured 2026-11-09 (Config dev-mode diagnostics, `/.develop`; % of the
  2,902 us audio block, whole callback timed). Test project: 8 tracks,
  every step filled, 170 BPM, reverb + delay sends full, bit-crusher,
  overdrive, master sidechain + saturation.

  | State | avg | max |
  | :--- | ---: | ---: |
  | Empty project, stopped (firmware floor) | 60 | 60 |
  | Test project, stopped | 67 | 68 |
  | 1 track playing | 68 | 73 |
  | 8 tracks playing | 85 | 112 |
  | 8 tracks, reverb/delay sends 0 | 84 | 102 |
  | 8 tracks, also no overdrive | 79 | 97 |

  - Floor 60% with nothing playing: the stock DSP chain (delay, reverb,
    master, mixer, output) runs every block. Sends at 0 don't stop it.
  - Per voice: ~1.5% (player + filter + crusher), +0.6% overdrive, sends
    ~0.1%: ~2.2% fully loaded. 8 more full voices ~ +18%.
  - Note starts spike: max is 18-27 points over avg with 8 noteOns in one
    block (~100 us each). Scheduler (4b) should cap note starts per block
    (spill to the next block, <= 2.9 ms) or precompute noteOn setup.
  - Max > 100%: heavy stock projects already overrun.
  - So 16 full voices don't fit a heavy project; the fallback (cheap extra
    players) costs less but the real lever is the 60% floor. Next: the
    audio profiler build (core/tidal/README.md) splits it per graph node.
  - The main loop (Tidal queries, UI) only gets what the ISR leaves:
    ~40% idle, ~15% with the heavy project.
- Audio profiler (docs/audio_profile.txt, 2026-11-09; % of the block):
  - Empty project stopped 48% (Config read 60% earlier: state differed,
    unexplained). Always-on: Polyverb reverb 18.1, mixers ~8.4, short
    delay 5.4, master limiter 4.2 (5.9 with master FX on), per-voice idle
    chain (MonoToStereo, amps, RMS meters) ~4.5, sequencer.tick 1.8,
    dynamics 0.9, line input 0.6.
  - Per playing voice: AudioPlayMemory 2.7 (antialias ~0.8 of it), filter
    1.5, bit-crusher 0.15. Filters keep running (1.5 each) on silent tracks
    once used: 1 track playing still cost 12% in filters.
  - "Master FX off" / "more headroom off" don't stop the limiter/dynamics
    nodes; reverb and delay run regardless of sends.
  - Reverb buffers are in uncached SDRAM on MK66 (`internalRamReverbBuff`
    is __NOINIT(EXTERNAL_RAM) for MK66 in effect_polyverb.cpp), as are the
    delay line and samples.
  - Levers: skip reverb / delay / filters when input is silent and the
    tail has decayed; reverb buffer (32 KB) to internal RAM; SDRAM cache
    (PSCCR) on. Players are the real per-voice cost: 16 voices = +35%.

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
- CE itself is MIT + "no wholesale commercial clones": don't sell compiled
  .ptf builds of this fork.

## Environment
- WSL2 Ubuntu. Repo lives in ~/tracker-cycles (NOT under /mnt/c).
- Toolchain: GNU Arm Embedded 10.3-2021.10, installed at
  ~/toolchains/gcc-arm-none-eabi-10.3-2021.10 and on PATH
  (alternatives: ./agt/10.3/ or `make TOOLCHAIN=/path/to/arm-none-eabi-`).
- Needs make, python3 (stdlib only).

## Build
- `make original` -> build/PolyendTracker_CE_<ver>.ptf
  (version in version.mk, ORIGINAL_VER_*; currently 1.0.0)
- `make clean`, `make version`
- Makefiles don't track flags: `make clean` after changing CUSTOM_DEFINES.
- Confirm `make original` still builds after firmware-side changes.
- Linker prints region usage (`-print-memory-usage`); for a breakdown run
  `arm-none-eabi-size -A platforms/mk66/build/PolyendTracker.axf`.

## Layout (inherited from Tracker CE)
- core/      shared app, audio engine, UI (+ synth engines, IMX-only)
- core/source/mtSequencer.*      step sequencer (the thing we replace)
- core/AudioEngine/              playerEngine voices, mixer/FX, streaming
- platforms/mk66/  Original Tracker HAL, drivers, linker script
  (platforms/mk66/3rdparty/teensy_core/mk66fx1m0.ld)
- synth/lib/ precompiled libPolySynth.a, libPolyDSP.a (not used on MK66)
- docs/developer_guide.md  architecture, hardware limits, gotchas
- core/tidal/  firmware wrappers that build tidal/core into libTracker.a
  (only with -DTIDAL_BENCH for now), the on-device timing bench, and the
  audio profiler (-DAUDIO_PROFILE, per-stage / per-node audio cost).
- tidal/     our code (host build, `make -C tidal test smoke arm`):
  core/ shared tree/parser/renderer/checker (no heap, portable),
  proto/ editor prototype, tests/, golden/ (cases + events captured from
  real Tidal, `make -C tidal golden`). See tidal/README.md.

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
  the boot (bench v5 did, 2026-11-09: screen stuck at the updater's
  100%). Use raw `unsigned char` storage + placement-new; check objects
  for `.init_array` (core/tidal/README.md).
- Don't format floats (%f) from interrupt context (newlib heap allocation).
- Don't shift `Interface::modules[]` indices (hard-coded by UI navigation).

## Git
- main mirrors upstream (ff-only), develop branches off main (exists on
  origin only; no local branch), feature/tidal is the working branch,
  branched from develop. Commit small and often.
- origin: https://github.com/CyopStudio/tracker-cycles.git (the user's fork,
  not Polyend's). Pushing uses Git Credential Manager from Git for Windows.

## Original Tracker budgets (measured / derived)

### Flash / RAM
Measured 2026-11-08 from the existing `make original` ELF, built from the
source at 1343677 (9860d76 only added CLAUDE.md). The
table in docs/developer_guide.md is older and omits DMA/USB buffers.

| Region | Size | Used | Free |
| :--- | ---: | ---: | ---: |
| FLASH (code+rodata+.data init) | 1024 KB | ~888 KB (.text 902,604 B + .data 6,460 B) | **~136 KB** |
| RAM (internal SRAM @ 0x1FFF0000) | 256 KB | ~152 KB (.bss 126,692 + .dmabuffers 20,712 + .data 6,460 + .usbbuffers 2,160 B) | **~104 KB**, shared by heap and stack (stack at top of RAM) |
| EXTERNAL_RAM (SDRAM @ 0x70000000) | 16 MB | ~14.84 MB (.noinit_RAM4) | **~1.16 MB** |

- SDRAM is mostly `SAMPLE_MEMORY_SIZE` = 11.25 MB (7.5 MB mono sample pool
  + 3.75 MB Sample Editor FX edit buffer at the top), 1.5 MB delay line,
  768 KB `externalRamScratchBuff`, VoiceStreamManager ring buffers, cold UI
  singletons.
- Reclaimable SDRAM: lowering SAMPLE_MEMORY_SIZE to 31*256*1024 frees
  ~3.5 MB more (must stay > 3.75 MB because of the edit buffer). Samples
  >= 256 KB stream from SD (One-Shot / loop modes only on MK66; Slice and
  Beat Slice stay fully resident).
- Flash is the tightest budget for a "many Tidal functions" library.
  Estimated reclaim (symbol sizes by source file, not a measured build):
  pattern editor UI ~48 KB, step sequencer ~33 KB (keep MIDI out/clock,
  metronome, export hooks: expect ~100-110 KB for the planned cuts in
  total), legacy MOD/IT import/export ~16 KB, song mode ~15 KB, pattern
  files/undo ~4 KB. Optional: C++ demangler ~26 KB (override
  `__gnu_cxx::__verbose_terminate_handler` with abort), performance mode
  ~15 KB, random name generator ~15 KB, mtTest ~6.5 KB, image viewer ~2 KB.
  Fonts are 76 KB. So ~250 KB (up to ~325 KB) for the Tidal engine + editor.
- Pattern arenas: put the big pools in SDRAM (zero them yourself); keep
  only hot, small state in internal RAM.

### CPU
- Audio: 44,117.647 Hz (180 MHz * 16 / 255 / 256), 128-sample blocks,
  so the audio interrupt fires every **~2.90 ms**. Priority: MIDI UART 0 >
  SysTick > audio in 4 > audio out 5 > SDHC 6 > USB 7.
- The sequencer runs INSIDE the audio interrupt: TrackerImpl's audio
  callback (core/source/Tracker.cpp) calls `midiUpdate()`, then
  `sequencer.tick(blockSeconds)`, then `engine.update()` and
  `engine.updateAudioBuffers()`. Everything there shares one ~2.9 ms
  window with the DSP for 8 voices + delay/reverb/master FX.
- Main loop (UI, FT812 display over SPI, SD, VoiceStreamManager refills)
  runs at lowest priority; the dev guide says FT812 pattern redraws and
  song-mode reads take ~20-35 ms per step on MK66.
- No audio-load percentage is documented. Measure it with `Profiler`
  (`startMeasuringAudio`/`stopMeasuringAudio` wrap the callback; numbers
  show in Config diagnostics, `AUDIO_PERFORMANCE_INFO_ENABLED` is on by
  default in core/makefile) before setting Tidal's share.
- So: do Tidal queries on the main loop, a cycle (or a fixed window) ahead,
  into a fixed-size event queue; the ISR only pops due events and calls
  the voice API. Worst-case query cost must not depend on the ISR.

### How the sample engine triggers voices
- 8 sample tracks, one `playerEngine` voice per track
  (`instrumentPlayer[8]`, `audioEngine::kVoices = 8`). **Each track is
  monophonic today**: a new noteOn on a track replaces the sounding note.
  See "Polyphony (planned)" for the 2-voices-per-channel plan.
- 48 sample instruments (`kSampleInstrumentsCount`), shared by all tracks.
- Timing grid: `Sequencer::tick()` accumulates block time and calls
  `tick_uStep()` per 1/12 microstep; 1 step (16th) = 48 microsteps, so
  4 steps/beat = 192 microsteps per beat. Notes fire at the block in which
  their microstep falls; sample voices get no sub-block offset (only MIDI
  clock out does), so effective trigger jitter is up to ~2.9 ms.
- Trigger path: `Sequencer::sendNoteOn()` ->
  `instrumentPlayer[track].noteOn(instr, note, volume, fx1, val1, fx2, val2)`
  (core/AudioEngine/mtAudioNoteOnHandle.cpp). It runs inside
  `AudioNoInterrupts()`, inits envelopes, filter, gain, pan, delay/reverb
  sends and bit-crusher from the instrument, then applies up to 2 step FX.
  `noteOff(option)` releases; MIDI tracks/instruments go out via
  `sendMidiNoteOn/Off` instead.
- Per-voice modulation without a noteOn: `playerEngine::mod*()`
  (modVolume, modPanning, modCutoff, modResonance, modOverdrive,
  modBitDepth, modTune, modFineTune, modDelaySend, modReverbSend, modSP,
  modSeqPoints, modGlide, ...), and `seqFx(id, val, slot)`.
- Candidate Tidal control mapping (not yet verified, check value ranges):

  | Tidal | Tracker |
  | :--- | :--- |
  | `s` / `sound` | instrument index (bank name -> instrument slot) |
  | `n` (sliced instrument) | `FX_TYPE_SAMPLE_SLICE` |
  | `note` / `n` / `up` | note (pitch); `FX_TYPE_TUNE`, `FX_TYPE_MICROTUNING` |
  | `gain` / `amp` / `velocity` | step volume / `FX_TYPE_VOLUME` |
  | `pan` | `FX_TYPE_PANNING` / modPanning |
  | `cutoff`/`lpf`, `hpf`, `bandf` | `FX_TYPE_FILTER_LOWPASS/HIGHPASS/BANDPASS`; `resonance` -> modResonance |
  | `delay` | `FX_TYPE_DELAY_SEND` (time/feedback are global engine params) |
  | `room` | `FX_TYPE_REVERB_SEND` (`size` etc. are global) |
  | `crush` | `FX_TYPE_BIT_DEPTH` |
  | `shape` / `distort` | `FX_TYPE_OVERDRIVE` |
  | `begin` | `FX_TYPE_POSITION` / modSP |
  | `speed` < 0 | `FX_TYPE_REVERSE_PLAYBACK` (no arbitrary playback rate; positive speed ~ pitch) |
  | `legato` / `sustain` | scheduled noteOff, or `FX_TYPE_GATE_LENGTH` |
  | `nudge` | sub-step offset (`FX_TYPE_MICROMOVE` semantics) |
  | `cut` | implicit: tracks are monophonic |

  The step noteOn only takes 2 FX; for more controls per event, call the
  mod*() functions right after noteOn instead of going through step FX.
