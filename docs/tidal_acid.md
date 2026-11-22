# Acid lines on MIDI tracks (design v0.3, 2026-11-20)

Status: steps 1 and most of 2 built on `feature/acid` (engine controls,
golden cases, mapper, interrupt ordering, Tracker mode, `#` menu defaults,
boot.ghci). Not yet on a device. Step 3 (editor helpers) to do.

Sequencing 303-style synths (target: **Behringer TD-3**) from d9-d16 with
slide, accent, octave and note / tie / rest timing. Each one is its own
mini-notation pattern, in real Tidal syntax. The patterns are
**independent**: each can have its own length, so `{...}%16` on every
pattern gives polymeters (user, 2026-11-20).

```haskell
d9 $ time  "{1 1 0 ~ 1 1 ~}%16"     -- 1 = note, 0 = tie, ~ = rest
   # note  "{c2 c3 ef2 g2 bf2}%16"
   # oct   "{1 1 2 1 0 1}%16"       -- 0 / 1 / 2 = -12 / 0 / +12
   # acc   "{0 0 1 0 0 1 0 1 0}%16" -- 1 = accent (t / f work too)
   # slid  "{0 0 0 1 0 0 0 0 1 0}%16" -- 1 = slide into the next note
```

## 1. How the patterns line up

- `{...}%16` gives every pattern 16 steps per cycle, and each pattern
  cycles through its own list at that rate: 7 gates, 5 pitches, 6 octaves,
  9 accents and 10 slides all wrap at different points.
- Plain Tidal semantics, nothing special: the leftmost pattern gives the
  structure (the onsets), and every `#` pattern after it is read at those
  onsets. **`time` goes first**, so the rhythm is the structure and the
  pitches don't have to match its length. Checked in GHCi (Tidal 1.10.3):
  `pB "time" "{1 0 ~}%4" # note "{0 3 7 5 2}%4"` gives notes 0 / 3 / 5 at
  steps 0 / 1 / 3 (step 2 is a rest), i.e. each pattern's own step.
- With different grids (`%16` against `%8`), a pattern is read at
  whichever of its steps is sounding at the onset, as with any `#`.
- Without `time`, a line is an ordinary one (`note "c2 c3 _ ~"`: `_`
  lengthens, `~` rests) and `acc` / `oct` / `slid` still apply.
- `time` placed after `note` also works, but then `note` is the structure
  and `time` only tags its steps.

## 2. Controls (all ours: defined in boot.ghci, none in Tidal)

| Control | Type | Values | GHCi | Device (MIDI slots) |
| :--- | :--- | :--- | :--- | :--- |
| `time` | Bool | `1` / `0` / `~` (or `t` / `f`) | `pB "time"` | 1 = gate (note-on); 0 = tie (no note-on, the held note lasts to the end of this step); `~` = rest (no event, plain Tidal) |
| `oct` | Int | 0 / 1 / 2, default 1 | `pI "oct"` | note + (oct - 1) x 12; values outside 0-2 work too (3 = +24) |
| `acc` | Bool | `1` / `0` (or `t` / `f`), default 0 | `pB "acc"` | velocity 127 when 1 (**overrides** `velocity` / `amp` / `gain`), else the event's velocity, 80 when none is given |
| `slid` | Bool | `1` / `0` (or `t` / `f`), default 0 | `pB "slid"` | hold this note until the next note-on has gone out (TD-3 slide) |

- Flats are `f`, sharps `s`, as in Tidal (`ef2`, `bf2`, `fs3`). Tidal
  reads `eb2` as two notes (`e`, `b2`); our checker refuses it and says
  "flats are f: 'ef2'".
- Bool patterns need a string: `acc "1"`, not `acc 1` (Tidal has no
  `Num Bool` either; the checker says to write "1" or "t").
- `slid`, not Tidal's `slide` / `sld`: those are a SuperDirt synth
  parameter, so ours stays separate (user).
- Tidal's Bool patterns read `t f 1 0` alike (checked in GHCi), so
  `acc "t f 1 0"` is valid Tidal. On the device the pads / jog enter
  0 / 1 (there's no way to type t / f there); t / f come from files.
- Accent velocity 127, normal 80 (no velocity given): the TD-3 reads
  velocity > 100 as accent (community reports; the threshold can be set in
  Behringer's Synth Tool), so both are well clear of it. Settings for
  them can come later if needed.
- On a tie step `note`, `oct` and `acc` are ignored (a tie only holds);
  `slid` on a tie step slides out of the held note at the tie's end.
- On sample tracks (d1-d8) all four are refused in Tracker mode for now
  ("only works on MIDI slots (d9-d16)"). Possible later: `slid` ->
  the instrument's glide, `acc` -> volume boost, `oct` -> pitch, `time`
  -> ties as longer notes.

## 3. What the TD-3 needs on the wire

- **Slide** = the next note-on arrives while the previous note is still
  held; then the previous note-off.
- **Tie** = one note held across steps: no new note-on.
- **Same-pitch slide** = a tie. Note-on 36 then note-off 36 ends the held
  note (forum reports of hanging / cut notes on repeated tied notes; RV0
  lists a fixed TD-3 firmware bug for slide + same note), so the device
  never sends a second note-on for a pitch it is holding for a slide.

Problems in today's scheduler (`tidal/core/scheduler.cpp`, `tick()`):

1. Due note-offs run **before** the block's note-ons, so a note that ends
   as the next starts (or overlaps by < 2.9 ms) goes out off-then-on: a
   retrigger, never a slide.
2. Overlapping same-pitch notes send on, on, off, off: the first off ends
   the note that should still be held.

## 4. Device design

### 4.1 Engine / checker

- A Bool control kind (`'b'`, value 0 / 1) in the engine's control table
  for `time`, `acc`, `slid`; `oct` as `'i'`. Library entries
  `CTL("time", "Bool")` etc. Nothing in Tidal is shadowed (checked:
  `time`, `oct`, `acc`, `slid` are not in Sound.Tidal.Context).
- Golden cases from GHCi with the boot.ghci definitions: the example
  above, `t f` spellings, `time` before / after `note`, mixed grids.

### 4.2 Mapper (main loop, `tidal/core/mapping.cpp`)

- `oct` -> note offset; `acc` 1 -> velocity 127 (`kAccentVelocity`,
  wins over the velocity maths); no velocity controls and no accent -> 80
  (`kAcidVelocity`; only for events with an `oct` / `acc` / `slid` /
  `time` control, so other MIDI lines keep RC25's 127).
- Flags in `Entry.voices` (sample-only today; MIDI entries don't use it):
  bit 0 **Slide** (`slid` 1), bit 1 **Tie** (`time` 0). Entry stays 48 B.
- `dur` = the event's length as usual (legato 1). The overlap comes from
  the interrupt's ordering, not a longer note, so a slide into a rest
  ends on time.

### 4.3 Interrupt (MIDI slots only; sample slots unchanged)

The `Off` table gets a `slide` bit (copied from the entry; `Off` has
padding).

Per block, per MIDI slot:

1. Due offs go out first, **except** those with the slide bit and those
   on a channel that has a Tie entry due in this block (a peek at the
   ring's due entries).
2. Due entries:
   - **Tie**: no note-on. The channel's held note (latest active off)
     gets its off moved to the tie's end and takes the tie's slide bit.
     Nothing held (a tie after a rest, or at the start) = play it as a
     note.
   - **Note** whose pitch is held on that channel by a held-back slide
     off: no note-on; that off moves to this note's end and takes this
     note's slide bit (same-pitch slide = tie).
   - Otherwise the note-on goes out as now.
3. The held-back offs go out after the note-ons (0-2.9 ms overlap: the
   TD-3 only looks at the order).

As built (scheduler.cpp `tick()`, `tieChannels`, `releaseHeld`):

- A note's end and the next onset are timed separately and can round to
  neighbouring blocks (~1 in 150 notes in the tests). So a slide off
  waits for a note-on on its channel or at most one block, and ties are
  looked for up to the next block. Both cases have chain tests over 512
  notes.
- Any MIDI note of a pitch still sounding on its channel (no slide)
  first releases it: on, off, on instead of on, on, off, off (problem 2,
  for every MIDI line, not only acid ones).
- Stats: `tied` counts ties / same-pitch slides.

Cost: a few compares per due MIDI entry; no new memory.

### 4.4 Host side

- `tidal/proto/boot.ghci`: `time = pB "time"`, `oct = pI "oct"`,
  `acc = pB "acc"`, `slid = pB "slid"`. SuperDirt ignores them, so the GHCi
  backend plays the notes without ties / slides / accents; the engine
  backend (`--backend engine`) runs the same mapper + scheduler and plays
  them.
- The editor's `#` controls menu lists them on MIDI blocks.

## 5. Editor (step 3)

- On a MIDI block: an "acid" item that adds the four rows in the order
  above, each on its own line (`<CR>`), all `{...}%16`, with the cursor
  on the first step of `time`.
- Pads on `time` / `acc` / `slid` steps: the parked step-grid pad mode
  (toggle 1 / 0; `~` for `time`); `oct` steps: pads 0 / 1 / 2. Jog
  already steps the numbers.

## 6. Tests

- Scheduler host tests (`tidal/tests/test_scheduler.cpp`) with a
  recording Voice: slide inside one block and across a block boundary
  (on before off), same-pitch slide (one note-on, one off at the end),
  tie (no note-on, off moved), tie after a rest (plays), tie then slide,
  chords untouched, hush with a held slide; mutation checks (drop the
  reorder -> a test fails).
- Mapper tests: oct, acc over a velocity, defaults, flags.
- Checker / Tracker mode: refused on d1-d8, accepted on d9-d16.
- Device (RC): the example line on a TD-3; a slide chain `slid "1 1 1 0"`;
  same-pitch slides; ties across a cycle boundary; polymeters by ear.

## 7. Steps

1. Engine Bool control kind + controls + golden cases; mapper; interrupt
   ordering; host tests.
2. boot.ghci, `#` menu entries, Tracker-mode table. RC build.
3. Editor helpers (acid rows, pads). RC build.
