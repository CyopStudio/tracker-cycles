# Changelog

Notable changes to **Tracker Cycles**, TidalCycles-style live coding for the Original Polyend Tracker (MK66). Tracker Cycles is a fork of [Polyend Tracker Community Edition (CE)](https://github.com/polyend/tracker-ce); for the changes in CE itself, see its own changelog.

---

## [1.0.0] - First release

### Added

* **Tidal pattern engine.** Patterns are written in real Tidal syntax and evaluated on the Tracker. The engine is type-checked like GHC would, and checked against event lists captured from Tidal 1.10.3 (215 exact cases; random functions are checked by their behaviour).
  * More than 100 functions and operators: time, structure, randomness, layering, harmony, signals, pairs (`within`, `zoom` …) and the operator families (`#`, `|+|` … `|%|`, `|**|`, `|<|`, `|>|`). The list is in `docs/tidal_code_tree.md`.
  * `d1`–`d8` play the 8 sample tracks. `d9`–`d16` send MIDI notes and CCs (`midichan`, `ccn`, `ccv`, `midibend`).
  * `setcps` sets the tempo; MIDI clock out follows it.
  * `mute` / `unmute` / solo / `hush`; launch modes `jumpMod` / `jumpIn`.
* **Scheduler.** Patterns are queried on the main loop ahead of time, so the audio interrupt only plays the notes that are due.
* **Sound mapping.** Tidal controls map onto the Tracker's engine: gain, pan, filters, resonance, delay / reverb sends, bit crush, overdrive, sample start / end, envelopes, pitch and slices.
  * `# voices 1-3` (Tracker Cycles' own control) gives a track overlapping notes.
  * A load-driven governor keeps at most 16 notes sounding.
* **Code editor on the Tracker.** A structured editor works on the code tree, so the code is always valid.
  * Two editing modes, switched with Rec. Cursor mode (the default) moves token by token and line by line, like a text editor that only allows valid code. Tree mode moves through the code tree.
  * The jog wheel changes values in steps that suit what they mean. The pads enter notes, instruments and chords.
  * The Screen buttons open the structure menus (`$`, `#`, `[ ]`, `< >`, `{ }`, euclid, `|`, operators), which only offer choices that type-check.
  * Delete unwraps functions and operators and never leaves a hole. Insert adds a `$` function or a `#` control depending on where the cursor is. Shift + Screen 1–3 add `# gain`, `# room` and `# delay`. Undo, copy, paste and block duplication.
  * Evaluating flashes the block green (accepted) or red (refused).
  * Several blocks per `dN`, line breaks, and block comments (edited on the Tracker's naming keyboard).
  * Each event's steps light up as they sound.
  * The status view (Shift + Master) shows tempo, audio load, voices and errors.
* **Projects.** The code is saved with the project as `<Name>.tidal`, and the working copy is saved about 2 s after each edit. Instruments, samples and settings load and save as before.
* **Tidal sample folders.** Shift + Sample Loader loads a `/Tidal/<folder>/` of WAV files as one sliced instrument, so `s "bd:3"` works as in SuperDirt.
  * It converts most WAV formats and reports refused files in `/tidal_folders.txt`.
  * Loading waits whenever the scheduler needs the main loop, so it doesn't cost notes.

### Changed

* **Audio CPU load.** Idle voices, delay and reverb no longer process silence; the reverb runs at half rate. Together these cut the base audio load from about 60 % to about 7 %.
* **Reduced firmware.** The pattern editor, song mode, step sequencer, performance mode, sample recorder, audio export and MOD / IT import / export are removed. The Tracker is a Tidal instrument now.
* **Version label.** The firmware is named `PolyendTracker_Cycles_<version>.ptf` and shows `tracker-cycles <version>`.

### Known limitations

* Original Tracker (MK66) only.
* No SuperCollider synths. Playback rate is approximated through tuning. `chop` / `striate` are not available on the device yet.
* No lambdas, `let` or user-defined functions. Tuples are pairs only. `echoWith` / `plyWith` are limited to 8 layers.
* Random functions follow Tidal's probabilities, not Tidal's values.
* At most 48 instruments and 48 files per sample folder, and about 7.5 MB of sample memory.
* Loading a sample folder can pause the screen briefly; load folders before a performance.
* While several patterns play, a menu with many choices (e.g. Replace on a whole block) opens at once but takes a moment to finish checking which choices fit ("checking k of N"). The highlights pause until it's done.
* A sample whose name has no letters (`808 5.wav` → `808_5`) can't be used in a pattern; rename the file to include a letter.
