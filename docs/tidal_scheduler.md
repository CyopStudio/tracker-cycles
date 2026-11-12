# Tracker Cycles scheduler (plan step 4b) - design v0.2

How patterns turn into sounds on the Original Tracker: the engine is
queried on the main loop a little ahead of time into fixed-size queues,
and the audio interrupt only pops due entries and calls the voice API.
Measurements this is based on: CLAUDE.md "Original Tracker budgets" and
plan step 2 (bench v1-v6), docs/tidal_bench*.txt, docs/audio_profile.txt.

Status: implemented host-side in tidal/core/scheduler.{h,cpp}, tested by
tidal/tests/test_scheduler.cpp (section 9). v0.2 changes from v0.1:
noteOffs are a duration on the noteOn entry plus a small per-slot table
in the interrupt (section 5), and a cps change waits for the furthest
queued horizon instead of truncating (section 3).

## 1. Constraints (measured)

- Audio interrupt every 128 samples at 44,117.647 Hz = **2.9013 ms**
  (a "block"). It already runs MIDI, the sequencer tick and all DSP;
  empty-project load ~48%, heavy project 75-85% avg, peaks > 100%.
- Sample voices trigger at block granularity (no sub-block offset), so
  trigger jitter is up to 2.9 ms today; MIDI clock out has a sub-block
  offset (`midiSetClockSubBlockOffsetUs`).
- A noteOn costs ~100 us in the interrupt (8 in one block: +27% load).
- Engine (real-build config: -O2, quarter-cycle working set in internal
  RAM), quarter-cycle query under a heavy project: simple patterns
  0.8-3 ms, everyday 7-19 ms, heaviest bench pattern (`stut 8` on 16
  steps) 43 ms. Without load ~4x faster. Cost ~ a + b * events.
- Main loop also draws the screen (FT812 redraws 20-35 ms), refills SD
  streams and runs file operations (project load: seconds).
- No double (soft-float on the M4), no heap in real-time paths, internal
  RAM is scarce (engine working set ~26 KB + ~24 KB stack reserve).

## 2. Overview

```
 main loop (lowest priority)                      audio interrupt (2.9 ms)
 ---------------------------                      ------------------------
 editor: evaluate dN ----> live program (copy)
                               |
 scheduler.update():           v
   for the slot furthest behind:                  tidalTick(block):
     query [h, h + W) ---> events                   for each slot queue:
     onsets -> trigs (time, voice params)             while head.block <= block
     push to the slot's queue  ===== SPSC ring =====>   (note-start cap)
     h += W                                            pop, noteOn / mod* /
   until every slot is queued to now + L               noteOff / MIDI
```

- One queue per slot (d1-d8 sample tracks, d9-d16 MIDI), written only
  by the main loop (producer) and read only by the interrupt (consumer).
- Each slot has a **horizon** h: everything before h is already queued.
- The interrupt never queries, allocates, converts or formats.

## 3. Time base

- **Block clock**: a 32-bit block counter incremented by the interrupt
  (wraps after ~144 days). Sample time = block * 128 (+ offset).
- **Cycle clock**: an anchor (cycle C0 as an exact rational, block B0)
  plus `cps`. cycle(b) = C0 + (b - B0) * 128 * cps / fs.
- **Conversion** (main loop only): onset cycle q (exact) -> d = q - C0,
  split into whole cycles k and a fraction f. Samples per cycle are an
  exact fraction (fs = 750000/17 Hz, cps limited to denominators <=
  65536), so k cycles convert exactly in 64-bit integers and only f goes
  through float (error < 0.03 samples). No drift however long it runs
  (tested to 100k cycles against a 128-bit reference). Result: block
  index + sample offset 0..127 (the offset is kept for MIDI and for a
  future sub-block voice start).
- **cps changes** (`setcps`, tempo control) take effect at the first
  window boundary after the furthest queued horizon (<= lookahead + one
  window from now, ~375 ms), so nothing already queued is re-timed: a new
  anchor starts there, the old one stays valid before it until the audio
  passes it. A second change meanwhile waits for the first.
- The cycle clock runs while Tidal mode is active (Tidal's clock never
  stops; `hush` silences slots). `resetCycles` re-anchors to cycle 0.

## 4. Main loop side

- **Window W** (cycles): the largest 2^-k cycle with W <= 125 ms at the
  current cps (e.g. cps 0.5: 1/16 cycle). Small enough that one query is
  short (UI stays responsive), large enough that per-query overhead stays
  low (bench: one-cycle vs 4 quarter-cycle queries differ ~20%).
- **Lookahead L**: queue each slot until h >= now + L, L = 250 ms by
  default: covers screen redraws and a few queries back to back. Not a
  latency cost for evaluation (section 6 switches by truncating).
- **Order**: each update() call queries the slot with the smallest
  horizon first and stops after a time box (~4 ms) or when all slots
  reach now + L, so 16 slots share the main loop fairly and the UI keeps
  running between queries.
- **Falling behind**: if a slot's horizon is already in the past (long
  stall, e.g. project load), skip it forward to now (drop the missed
  span, count an underrun shown in the UI) instead of playing a burst of
  late notes.
- **Query errors** (too many events, time overflow, runtime type error):
  hush that slot from h, show the error on its block in the editor.

## 5. Events to triggers

- Keep only events with an onset (whole present and part.start ==
  whole.start), as Tidal does; continuous values reach events through
  `#`/`|+` etc. and are sampled at the onset (already engine semantics).
- `nudge` (seconds) shifts the onset.
- Sort the window's onsets by time (stable: pattern order for ties) and
  append them to the slot queue.
- Durations: noteOff at onset + `sustain` seconds, or onset + `legato` *
  event length; otherwise no noteOff (SuperDirt plays the sample out).
  Looped / sustained Tracker instruments need a default (open question).
  The mapper sets the duration (samples) on the noteOn entry; when the
  interrupt plays it, it notes the noteOff in a per-slot table of 8
  (MIDI chords; a full table releases its oldest note early). That keeps
  each ring in time order. A sample slot's new note clears the table (one
  voice per slot cuts the previous note).
- Control mapping (step 4c) happens here, on the main loop: `s` ->
  instrument slot, `n`/`note` -> note or slice, gain/pan/cutoff/... ->
  values in the Tracker's units. The interrupt applies them as given.

### 5.1 Queue entry (32 bytes)

| Field | Size | |
| :--- | ---: | :--- |
| block | 4 | absolute block index |
| dur | 4 | samples until the noteOff (0 = none) |
| offset | 1 | sample within the block (0-127) |
| kind | 1 | note (the slot says sample voice or MIDI) |
| instrument / MIDI channel | 1 | |
| note | 1 | |
| velocity | 1 | |
| nmods | 1 | |
| mod ids | 6 | for mod*() calls after the noteOn |
| mod values | 6 x 2 | |

Per slot a power-of-two ring of 128 entries (16 slots x 128 x 32 B = 64 KB,
in SDRAM: the interrupt reads only a few entries per block). 128 covers
the heaviest bench pattern (37 onsets per quarter cycle) at L = 250 ms
with room to spare; a full ring stops the slot's querying until the
interrupt drains it (the horizon simply waits). A window whose onsets
fill more than half a ring, or whose query overflows the engine, is
halved and retried (down to 1/64 window; past that the slot fails).

## 6. Evaluate, launch modes, hush

- The scheduler queries a **live program**, separate from the editor's:
  evaluating dN frees the live block's tree and `copyTree`s the edited
  block into it (Program::copyTree), so unevaluated edits in other blocks
  never play. Done on the main loop between queries, so the engine never
  sees a half-copied tree.
- Switch time S (spec 2.1): `dN $` next cycle boundary, `jumpMod N k`
  smallest multiple of k after now, `jumpIn N k` ceil(now) + k.
- Switching: truncate the slot's queue at S, set h = S, install the new
  expression from S. Truncation takes a few microseconds with interrupts
  off: walk back from the ring tail while entry.block >= block(S) and
  move the tail. Entries before S keep playing (that is the "launch at
  the next cycle" behaviour).
- If S is so close that the first window can't be queried in time, its
  first notes play late (the interrupt fires overdue entries at once, up
  to ~20 ms late, older ones are dropped).
- `hush`: truncate every queue at now, clear the live blocks, noteOff
  (fade) all voices. `dN silence` = evaluate silence (switch at S).

## 7. Interrupt side

`tidalTick()` runs in the audio callback where `sequencer.tick()` runs
today (CLAUDE.md CPU section), only in Tidal mode:

1. block++ (the clock).
2. For each slot, pop entries with entry.block <= block: noteOn on the
   slot's voice (`instrumentPlayer[n].noteOn(instr, note, vel)` then the
   mod*() calls), noteOff, or MIDI `sendMidiNoteOn/Off`.
3. **Note-start cap** (sample voices only): at most 4 sample noteOns per
   block across the sample slots; the rest wait for the next block
   (<= 2.9 ms each, measured spikes come from 8 noteOns in one block, so
   8 simultaneous hits spread over 2 blocks at most). Round-robin the
   starting slot so no track is always last. MIDI slots are not capped:
   a MIDI note-on is a few UART bytes, so MIDI always goes out on its
   block.
4. Entries more than ~20 ms overdue are dropped (counted).
5. Due noteOffs from the per-slot tables run first in each block.

Synchronisation: single producer / single consumer per ring; head and
tail are 32-bit and written by one side each; the producer fills an
entry before publishing the tail (`dmb` between), the consumer reads the
entry before publishing the head. Truncation is the only producer write
to the tail that can move it backwards, so it runs with interrupts off.

## 8. Budgets

Code: scheduler.o 6.1 KB flash at -Os (13.9 KB at -O2; only tick() is
interrupt code). Since device tick v2: the Scheduler object (~3.8 KB:
ring heads / tails, noteOff tables, slot state) sits in internal RAM, as
the interrupt reads it every block (v1 had it all in uncached SDRAM: an
idle tick cost ~31 us); the ring entries + a 4 KB sort buffer (68 KB,
`Scheduler::kStorageEntries`) are caller storage in SDRAM. Placement-new
the object at run time, then init().

update() early exit (device tick v3): after a call that queued every
slot up to the lookahead, update() returns at once until the first block
at which a slot needs a query (its horizon - lookahead) or a cps anchor
retires; evaluate / silence / hush / setCps reset it. v2 measured ~36 us
per call on every main-loop pass (~10k/s) without it.

Mono slots (`setMonoSlots`, on for the device): a sample slot has one
voice, so when several of its starts are due in the same block only the
last is audible; the earlier ones are skipped (`superseded`) and don't
count toward the note-start cap (device tick v1: the heavy set had 21 %
of notes 3-7 blocks late from the cap). MIDI slots play every note.


| Item | Where | Size |
| :--- | :--- | ---: |
| Engine working set (q4) | internal RAM | ~26 KB |
| Stack reserve (evaluator 20.6 KB at -O2) | internal RAM | 24 KB |
| Queues 16 x 128 x 32 B | SDRAM | 64 KB |
| Live program (+ editor program) | SDRAM | 70 KB each |
| Interrupt work per block | | ~16 head checks + <= 4 sample noteOns + MIDI |

Main loop CPU per second at 120 BPM (cps 0.5), heavy project playing, 8
sample slots: everyday patterns ~5-15% of real time, each `stut 8`-class
slot ~9%. Everything else in the main loop gets the rest.

## 9. Testing

- Host first (portable, `tidal/core/scheduler.*`, done): a fake interrupt
  advances the block clock, a fake main loop calls update() every 1-40
  blocks at random, a fake voice records (block, offset, note, lateness).
  Expected onsets come from querying the engine directly over the whole
  span, timed with exact 128-bit arithmetic. Tests (all pass; deliberate
  bugs in the cap, the switch truncation and the cps anchor are caught):
  - onset times vs the golden events (`golden/expected.txt`) mapped
    through cps: every onset at the right block/offset;
  - evaluate at random times, jumpMod / jumpIn switch exactly at S,
    no lost or doubled notes across the switch;
  - cps change mid-pattern: no gap, no duplicate;
  - stalls: block the "main loop" for 100 ms / 2 s: late notes inside
    20 ms play, older ones drop, the horizon skips forward;
  - ring full (`bd*1024` at cps 1): windows split, the slot waits,
    nothing is lost or overwritten;
  - note-start cap: 8 sample slots together -> <= 4 per block, 1 block
    late at most, round robin; MIDI never held back;
  - hush, `silence`, and a slot failing without affecting others.
- Then the prototype (step 3): the same scheduler driving OSC to
  SuperDirt, compared with real Tidal's timing.
- Then the device (4b/4c): replace `sequencer.tick` in Tidal mode,
  measure interrupt cost of tidalTick and the note-start cap.

## 10. Open questions

1. Default noteOff for looped / sustained Tracker instruments (one-shot
   samples just play out).
2. Sub-block start offsets for sample voices (needs the player to start
   mid-block): worth it if 2.9 ms jitter is audible on dense patterns.
3. MIDI clock out from the cycle clock (24 ppqn per cycle / 4 beats?)
   and external clock in (follow MIDI clock by adjusting cps).
4. Two voices per sample slot (Polyphony plan): the queue entry gets a
   voice field, the allocator lives on the main loop side.
5. W and L defaults: confirm on the device with the real UI running.
