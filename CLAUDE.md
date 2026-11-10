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
     v5 device results (2026-11-09): q4 stut 8 16.5 ms run 1 / 69 ms run 2
     (v4 23 / 97), jux/chop 8.0 / 33, echo 9.0 / 37, simple 0.3 / 1.3.
     mem* 27% -> 9.5% of samples: the engine is compute-bound (rationals
     ~1/3 of it, qCompress 6% of all). Stack deepest 18.8 KB, 13.9 KB
     clear of the heap with the 32 KB reserve. Everyday patterns fit
     easily; 8 tracks of the heaviest bench pattern would not under the
     heavy project. Next: v6 internal-RAM pass + -O2 variant.
     Bench v6: engineUseMemory() runs queries in one caller block (q4
     working set 256 events / 1024 controls / 128 params = 25.6 KB, fits
     every bench pattern on the host); v6 adds that pass in internal RAM.
     -O2 variant (TC_ENGINE_O2): flash 1,006,428 B (96%) vs 982,292 B.
     v6 results (2026-11-09; docs/tidal_bench_o2.txt, docs/tidal_bench.txt
     is a v5 -Os rerun): -O2 vs -Os 1.2-1.3x (run 2 q4 stut 69 -> 53 ms,
     jux 33 -> 28, echo 38 -> 33). Internal-RAM working set on top: stut
     81% of SDRAM, jux 50%, echo 68%, everyday ~75%. Best (-O2 + internal
     RAM) run 2 q4: stut 43 ms, echo 22, sometimes 19, jux 14, off 11,
     chunk 8, hh*16+pan 7, simple 0.8-3 ms; run 1 ~4x faster. Stack at
     -O2 20.6 KB deepest. Engine -O2 profile: qCompress (inlined rational
     maths) 11% of all samples, operator+ 6%, still rationals-bound.
     Decision input for 4b: real build = engine -O2 + q4 working set in
     internal RAM (~26 KB) + ~24 KB stack reserve.
   - Cross-compile with arm-none-eabi-gcc throughout; track flash size.
   - Early on-device timing check: once a few dozen functions work, flash a
     throwaway build that times queries on the MK66 (32-bit rationals at
     180 MHz is the biggest technical risk).
3. Swap the prototype backend to our engine -> OSC -> SuperDirt. Validate by
   recording Tidal's OSC and ours for the same code and comparing
   timestamps, controls sent, cps/cycle position.
   Done (2026-11-09): `--backend engine` (tidal/proto/engine_backend.cpp,
   scheduler on a main-loop thread + a real-time "audio" thread) and
   tidal/golden/osc_compare.py (real Tidal vs ours on local UDP ports).
   Messages identical on 13 test patterns, jitter 0.02 ms vs Tidal's
   0.2-0.3. Found and fixed: chop ignores begin/end unless both are set
   (golden chop_begin/range/reverse); `end` and `unit` controls added.
   Tidal 1.10.3's default cps is 0.575 (scheduler default). Not yet heard
   through SuperDirt (not installed here).
4. Port to the Tracker:
   - 4a. Strip firmware: pattern editor, song mode, step sequencer, legacy
     import/export; stub their callers. Independent, can start any time;
     riskiest firmware change (the sequencer has many callers).
   - 4b. Scheduler: queries on the main loop a fixed window ahead into a
     fixed-size event queue; the audio ISR only pops due events.
     Design: docs/tidal_scheduler.md (v0.2: per-slot SPSC rings of 32 B
     entries in SDRAM, windows <= 125 ms, lookahead 250 ms, switches by
     truncating the queue at S, sample note-start cap 4 per block (MIDI
     uncapped), noteOff = duration on the entry + per-slot table in the
     ISR, cps change after the furthest horizon, live program via
     copyTree). Host implementation done (tidal/core/scheduler.*, tests in
     tidal/tests/test_scheduler.cpp; 6.1 KB flash at -Os, 73 KB object in
     SDRAM). OSC backend (step 3) done. Device tick test build
     (2026-11-09): `-DTIDAL_TICK`, core/tidal/tidal_tick.cpp (tidalTick()
     after sequencer.tick in the audio callback, update() on the main
     loop, 4 built-in sets on the project's instruments, report in
     /tidal_tick.txt; core/tidal/README.md). Flash 996,180 B (95 %).
     v1 results (2026-11-09, docs/tidal_tick.txt = run 2, -Os; run 1 in
     git 9666bba had a project loading mid-test: 1 s main-loop stalls ->
     drops and horizon skips). Run 2: no drops, no skips in any set,
     main-loop gaps <= 2.6 ms. Interrupt: cap holds (<= 4 starts/block),
     noteOn avg 26-36 us (max 58), worst tick block ~40k cycles = 226 us
     (7.8 % of a block); an idle tick still costs ~5.5k cycles (31 us:
     rings + noteOff tables in uncached SDRAM). update() share of
     main-loop wall time: everyday 8 slots 47 %, heavy set 75 %, 8x bd*16
     49 %; audio load 76-81 % avg. Heavy set: 21 % of notes 3-7 blocks
     late from the cap (3328 holds): stut/chop put several starts of one
     monophonic slot in the same block, each counted by the cap though
     only the last is heard. Stack 18.5 KB deepest, 6 KB spare of 24 KB.
     Test bugs: set 1 ends at once (stale ms).
     v2 (2026-11-09; docs/tidal_tick.txt -Os, docs/tidal_tick_o2.txt
     -O2): scheduler object (3.8 KB) in internal RAM, entries caller
     storage in SDRAM; mono sample slots (setMonoSlots: only the last
     start due in a block plays, not counted by the cap; host test
     testMono); per-slot query timing; waits for the project. Results:
     no drops / skips in any set; idle tick 900-1330 cycles (5-7 us, was
     ~5.5k); heavy set: no note > 1 block late (v1: 21 % 3-7 late), 4142
     superseded; worst tick block ~22k cycles = 122 us (4.2 %), except
     set 2 ~34k (4 starts). Audio load 59 % (set 1) / 77-79 %. Main loop:
     update() takes 36-42 % of wall time even for set 1 (3 trivial
     slots): ~36 us per call x ~10k calls/s, mostly NOT queries (set 1
     queries total 59 ms of 7.2 s) -> fixed in v3 (below). Heavy set queries ~10 s of 14 s (-Os) in update; stut 8
     window 11.2 ms avg -Os / 8.5 ms -O2 (wall, interrupts included).
     -O2 vs -Os: 1.1-1.5x on queries, flash 97.4 % vs 95.1 %, stack 20.2
     KB deepest (4.4 KB spare of 24 KB) vs 18.5 KB. Mapping 3-25k cycles
     per event (wall; s-name lookup scans 48 instruments in mtProject:
     cache it in 4c).
     v3 (2026-11-09, docs/tidal_tick.txt, -Os): update() early exit: after a call that queued
     everything it returns at once until the first block a slot needs a
     query (horizon - lookahead) or a cps anchor retires; evaluate /
     silence / hush / setCps / setSampleRate / setLookahead reset it.
     Host test testIdleUpdates (update() every block: > 90 % idle calls,
     nothing late); mutations (no reset on evaluate, wake one lookahead
     late) caught. Report adds "idle update() calls". Results: update()
     share of main-loop wall time set 1 36 -> 5.4 %, everyday 8 slots
     40 -> 10.2 %, heavy 70 -> 56 % (now nearly all queries), 8x bd*16
     42 -> 11.5 %; 96-99 % of calls return at once; main loop runs 1.7x
     more passes in set 1. Interrupt side unchanged, still no drops.
   - 4c. Control mapping to playerEngine noteOn/mod*() calls, MIDI out for
     MIDI channels, and the per-channel voice allocator (Polyphony below).
     Design: docs/tidal_control_mapping.md (draft v0.1, 2026-11-09):
     portable mapper in tidal/core/mapping.* (all conversion on the main
     loop), new playerEngine::tidalNoteOn (per-note overrides on the
     player's sequencer-FX lane 0, every event from instrument defaults),
     per-name instrument cache, Tracker-mode checker from the same table.
     Decided (v0.2): Entry 48 B / 10 mods, gain 1 = the instrument's
     volume, looped instruments release at the event's end, `n` = pitch
     on non-sliced instruments. Step 1 done: tidal/core/mapping.*
     (TrackerMapper, host-tested in tests/test_mapping.cpp), 10 new
     controls + 6 aliases (golden cases from real Tidal). Step 2 done:
     trackerCheck() (same table) in the prototype's evaluate + recheck,
     menus filtered per slot kind (smoke session 10). Step 3:
     playerEngine::tidalNoteOn (core/AudioEngine/mtAudioTidal.cpp),
     core/tidal/tidal_device.* (DeviceHost / DeviceVoice, MIDI out), tick
     test v4 with listening sets 5-6 (PolyendTrac_TidalTick4.ptf, flash
     1,004,884 B = 95.8 %). v4 results (2026-11-09, docs/tidal_tick.txt):
     no drops in any set; noteOn with overrides avg 5.0-5.8k cycles
     (~31 us, max 7.1k), MIDI noteOn 3.3k; worst tick block ~25k cycles
     (4.7 %); mapping 6k cycles per event (set 4, was 37k: name cache).
     STACK: heap end now 20,480 B below the top of RAM (was 24,576), the
     evaluator's deepest 18.5 KB -> only 1.9 KB spare. Fix before more
     RAM goes in: move the checker's 28 KB type pool to SDRAM (used on
     evaluate only) or raise the reserve. Listening (user, set 5 all
     together): "sounded OK", hard to tell apart; MIDI not yet checked.
     v5 (2026-11-09, docs/tidal_tick.txt, PolyendTrac_TidalTick5.ptf):
     checker in SDRAM (TC_CHECKER_ATTR, internal RAM 219.5 -> 191.7 KB),
     stack reserve 32 KB; set 5 split into eight solo sets with hint
     popups, MIDI set solo; docs/tidal_tick_midi.tidal = MIDI-only card
     file. Results: stack 14.2 KB spare (heap end 32 KB below the top,
     evaluator deepest 18.5 KB); no drops. User: solo sets "sounded
     good"; reverb (room) hard to judge with the project's short global
     reverb, retest with a longer one; MIDI not yet checked. Evaluate
     (main loop): check 0.2-3 ms, heavy 8-slot set 17 ms (SDRAM checker
     fine); parse ~12 ms even for one line, mostly Program::clear() on
     the 70 KB SDRAM program (cheap to fix: reset counts instead of
     clearing all).
     Step 4 (2026-11-10): polyphony, see "Polyphony" below and
     docs/tidal_voices.md; tick test v7 (PolyendTrac_TidalTick7.ptf,
     core/tidal/README.md).
   - 4d. Device editor: step 1 editor logic + FT812 drawing layer +
     pad/encoder input.
   - 4e. Project files: .tidal file + sample banks load/save on SD.

## Polyphony (planned)
- Built (2026-11-10, tick v7, not yet run on the device): design
  docs/tidal_voices.md. Decided with the user: shared track chain (3
  sample players per track summed by AudioSumPlayers into the track's
  one crusher / filter / amp / pan / sends; per note: pitch, points,
  slice, reverse, volume = player gain, amp envelope; per track: filter,
  crush, shape, pan, sends) and at most 16 of the 24 players sounding.
  DeviceVoice picks the player (free < cap, else the track's oldest, else
  steal the oldest anywhere; streamed instruments on player 0); the
  scheduler's setSlotPlayers(n) keeps the last n starts per block and
  hands each noteOff its noteOn's tag. Every build: internal RAM +5.7 KB.
  v7 results (2026-11-10, docs/tidal_tick.txt; project changed since v6:
  11 instruments incl. the long/looped poly samples, longer global
  reverb): polyphony works (chords 3 players, overlapping bass, echoes;
  cap steals 25 and 358 stale noteOffs in the cap set, as designed; heap
  17.7 KB used / 13.5 KB free; audio blocks <= 39 of 70; stack 14.2 KB
  spare). But audio load v6 -> v7: silence 7 -> 7, light 36 -> 61,
  everyday 74 -> 96 (max 112), heavy 65 -> 88, 8x x*16 77 -> 102 (max
  123), cap set 90; solo sets +4..+37 (hats 22 -> 59: hh rings 1.7 s, so
  3 players instead of 1; snare +13 with one player at a time, so part
  is the longer reverb staying active). At >= 90 % the main loop starves:
  update() 56-97 % of wall time, gaps up to 1.3 s, horizon skips, 790
  dropped notes in the heavy set. Cap 16 is too many sounding players for
  this CPU with reverb on (16 sounding ~ 60 points). Over-cap bug: up to
  19 sounding (starts in the same block, or a retriggered player in its
  release-on-noteOn, not yet isPlaying(), count as free). Next: baseline
  v6 on the same project, then a lower / load-driven cap.
  The notes below are the plan before that decision.
- Before: 8 sample voices, one per track (voice index == track index).
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
  - Audio floor work (2026-11-09, all builds, not yet measured; tick test
    v6 has a 10 s silent set): the cause was idle players transmitting a
    zero block every block, so every voice chain, mixer and send ran on
    silence. Now silence = no block (the AudioStream convention):
    idle players send 16 zero blocks (~46 ms, filters settle) then
    nothing (effect_envelope.cpp); mixers treat muted inputs (gain 0,
    not ramping) as absent (mixer.cpp); the delay skips while its input
    is absent and isSilent() (line exactly zero, effect_shortDelay.cpp);
    the reverb runs on silence while IsActive() (tail above ~-76 dBFS)
    then skips (effect_polyverb.cpp); record queues (export stems,
    recorder) record an absent block as zeros (record_queue.cpp).
    Verified by reading every node in the graph: voice chain, mixers,
    limiter, dynamics, mux, RMS, output, USB out handle no-block input.
    To check on the device: export / stem lengths, recording, reverb and
    delay tails, meters.
    v6 results (2026-11-10, docs/tidal_tick.txt): audio load, v5 -> v6:
    silence 7 % (floor was ~60 %), 3 light tracks 59 -> 36 %, solo sets
    59-61 -> 21-40 %, everyday 8 tracks 77 -> 74 %, heavy 80 -> 65 %,
    8x x*16 78 -> 77 % (every voice busy: little to skip). No drops; idle
    tick ~730 cycles. Per-slot window times at high load include a whole
    audio interrupt when one lands mid-query (set 5: d4 / d8 ~2.4 ms vs
    ~150 us for the rest), so mapping averages there are inflated.
    Stock features (export, recording, tails, meters) not yet checked.
    Export and recording are not needed in this firmware (user,
    2026-11-10): no need to test them; strip candidates for 4a.
  - Reverb bench (2026-11-10, `-DREVERB_BENCH`, core/tidal/README.md): the reverb is Polyverb (Clouds' Griesinger/Dattorro
    plate, MIT), ~23 delay-line accesses per sample, both 32 KB buffers in
    uncached SDRAM: the memory, not the algorithm, is the suspect. The
    bench times variants in the ISR (buffers internal, the 4 input
    diffusers internal = 4 KB, half rate with halved lines = half the
    memory) to keep as a CPU-vs-RAM reference table.
    Results (2026-11-10, docs/reverb_bench.txt; % of the block, identical
    stopped / light / heavy playback: SDRAM contention from playback is
    negligible): stock 17.1 (as shipped, 0 internal); buffer 1 internal
    11.3 (32 KB); buffer 2 internal 14.4 (32 KB); both internal 8.7
    (64 KB); diffusers internal 15.6 (4 KB, bench engine; it costs +1.1
    over stock: row 4 18.2 vs row 0); half rate SDRAM 10.4 (0 KB internal,
    SDRAM 64 -> 32 KB); half rate + diffusers 8.9 (2 KB); half rate all
    internal 6.0 (32 KB). Memory ~half the cost (~15 cycles per SDRAM
    access, ~23 per sample); the other half is compute (~356 cycles per
    sample even all internal: signed % per access, int16 <-> float,
    ssat). Cheapest win: half rate in SDRAM, -6.7 points, no internal
    RAM (needs a listening check); then trade internal RAM per the table.

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
  (only with -DTIDAL_BENCH / -DTIDAL_TICK for now), the on-device timing
  bench, the device tick test (tidal_tick.cpp), and the
  audio profiler (-DAUDIO_PROFILE, per-stage / per-node audio cost), the
  reverb bench (-DREVERB_BENCH).
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
