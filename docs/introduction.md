# Introducing Tracker Cycles

**Tracker Cycles turns the Original Polyend Tracker into a standalone live-coding instrument for TidalCycles-style patterns.** You write real Tidal code, such as `d1 $ every 4 (fast 2) $ s "bd*2 [~ sd]" # room 0.3`, edit it with the pads, jog wheel and buttons, and the Tracker's own sample engine plays it. You don't need a computer, SuperCollider or GHC.

It's a free firmware for the Original Tracker (MK66), built on Polyend's Tracker Community Edition. The step sequencer, pattern editor and song mode are replaced by a Tidal pattern engine and a code editor. Everything else that makes the Tracker a Tracker stays: instruments, the sample loader and editor, the mixer, effects and MIDI.

---

## What you get

### Real Tidal, on the device

* **About 140 functions, controls and operators**, from `fast`, `every` and `jux` to `within`, `fix`, `ghost`, `chew` and `qtrigger`, with mini-notation in full: `[ ]` `< >` `{ }%n` euclid `(3,8)` `*` `/` `!` `@` `?` `..` `|` chords such as `c'maj`.
* **Checked against real Tidal.** Every function's type comes from GHCi. The engine's output is compared with event lists captured from Tidal 1.10.3: 284 cases, all matching, with random functions checked by their behaviour.
* **Unsupported means an error, never a guess.** Code that works here works in Tidal.

### An editor made for pads and a jog wheel

* **The code is always valid.** You edit a code tree, not raw text, and every menu only offers choices that type-check.
* **Two ways to move.** Cursor mode (the default) goes token by token and line by line like a text editor, with stops in the gaps between steps and before every `$` and `#`. Tree mode moves through the code's structure. Rec switches between them.
* **Edits by context.**
  * The jog wheel changes values in steps that suit them: `gain` in 0.05, `cutoff` in semitones, `fast` along a ladder of tempos.
  * Insert adds a function in front or a control behind, depending on where you are.
  * Delete unwraps functions and operators without ever leaving a hole.
* **Pads enter music.** In a number pattern the pads are a keyboard; in an `s` pattern they are your 48 instruments. Hold several to enter a chord.
* **One-press structure.**
  * Screen keys put brackets, alternations, polymeters or euclid rhythms around a step.
  * Shift + Screen adds step marks (`*2`, `/2`, `!2`, `@2`), a new layer `,` or a `?`.
  * The operator key turns `# crush 4` into `|* crush 4`.
* **Live feedback.**
  * Each step lights up as it sounds.
  * A block flashes green when it's accepted or red when it's refused.
  * Each block's `dN` shows whether it's playing, edited or in error.

### Performance

* **Play evaluates** the block under the cursor. **Hold Pattern and press pads** to mute and unmute, with Shift to solo. Shift + Pattern alone is `hush`.
* **Launch modes** bring a block in on the next cycle or at a chosen point (`jumpIn`, `jumpMod`).
* **Several blocks per track**, to prepare the next idea while the current one plays.
* **Tempo** with `setcps`. The MIDI clock out follows it.

### Sound

* **d1–d8 play the Tracker's 8 sample tracks.** Tidal controls map onto the Tracker's engine:
  * `gain` and `pan`;
  * filters (`cutoff`, `hpf`, `bandf`, `djf`);
  * reverb and delay sends and sizes;
  * `crush` and `shape`;
  * sample start and end, envelopes, pitch and slices.
* **Polyphony when you want it.** `# voices 2` or `# voices 3` lets a track's notes overlap. The editor adds it for you when you bring in `jux`, `off`, echoes, stacks or chords. A load-aware limit keeps the audio safe.
* **Tidal-style sample folders.** Copy folders of WAVs to `/Tidal/` on the card and load one as a sliced instrument, so `s "bd:3"` works as it does with SuperDirt.
* **d9–d16 send MIDI** on channels 1–8: notes, chords, CCs, pitch bend, program changes and aftertouch, plus clock out.

### Projects

* Your code is saved with the project as `<Name>.tidal`, a plain Tidal file you can open on a computer.
* A working copy is saved a couple of seconds after every edit, so a power cut doesn't lose your set.
* Stock projects open with their instruments and samples, and the stock firmware can still open them afterwards.

---

## How it works

The Tracker runs the whole of Tidal's pattern model itself. Patterns are queried on the main loop a quarter of a second ahead, and the audio interrupt only starts the notes that are due, so editing and drawing never compete with the sound. Work that could hold up the main loop waits until enough notes are queued, so it can't cost notes. That covers type-checking menu choices, loading sample folders and saving to the card.

---

## What's different from TidalCycles

* The Tracker's sample engine replaces SuperDirt, so there are no SuperCollider synths. Playback speed is approximated with tuning, and the reverb and delay are shared by all tracks.
* There are no lambdas, `let` or user-defined functions. Tuples are pairs only.
* Random functions follow Tidal's probabilities but produce different values.
* At most 48 instruments and about 7.5 MB of sample memory.

---

## Getting started

1. Copy `PolyendTracker_Cycles_1.0.0.ptf` to `/Firmware/` on the card and update from **Config → Firmware**.
2. Press **Shift + pad 1** to go to `d1`, then **Instrument** to start a block with your first sample.
3. Turn the jog wheel to change things, tap pads to enter steps, and press **Play**.

The README has the full controls, the SD card layout and recovery instructions. Tracker Cycles is MIT-licensed (with CE's no-commercial-clones condition) and is not affiliated with Polyend or the TidalCycles project.
