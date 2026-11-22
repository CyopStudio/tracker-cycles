# Upcoming Changes (Unreleased)

Changes merged since the latest release. They are not in the release download yet; the build workflow publishes test builds for every push (see the Actions tab).

For what is in the current release, see [`CHANGELOG.md`](CHANGELOG.md).

---

### Acid lines on MIDI tracks (d9-d16)

For 303-style synths such as the Behringer TD-3: slide, accent, octave and note / tie / rest timing, each as its own mini-notation pattern next to the notes. Each pattern can have its own length, so `{...}%16` on every one gives polymeters. The details are in [`docs/tidal_acid.md`](docs/tidal_acid.md).

```haskell
d9 $ time  "{1 1 0 ~ 1 1 ~}%16"       -- 1 = note, 0 = tie, ~ = rest
   # note  "{c2 c3 ef2 g2 bf2}%16"
   # oct   "{1 1 2 1 0 1}%16"         -- 0 / 1 / 2 = an octave down / as written / up
   # acc   "{0 0 1 0 0 1 0 1 0}%16"   -- accent (velocity 127); t / f work too
   # slid  "{0 0 0 1 0 0 0 0 1 0}%16" -- slide into the next note
```

* Put `time` first, so it gives the rhythm and the other patterns are read on its steps.
* A slide holds the note until the next one has started. A slide or tie to the same pitch holds one note instead of starting a new one.
* Accent sends velocity 127. Other notes on such a line are sent at 80 unless you give a velocity, which keeps them below the TD-3's accent threshold.
* The four controls are in the `#` controls menu on MIDI blocks; the jog wheel toggles 1 / 0.

### USB storage and USB audio out (from Tracker CE)

* **USB storage** (Config > USB > Storage): mount the SD card on your computer over USB to copy samples, projects and `.tidal` files. While it is mounted the Tracker hushes, writes your code to the card first, and holds autosave and sample-folder loading until you end the mode. The code stays in the editor. The mode can't start while a sample folder is loading.
* **USB audio out** (Config > USB > Audio: `Off` / `Output (2 out)`): the master mix as a 44.1 kHz / 16-bit stereo USB audio device, to record a set into a DAW. Connect the Tracker directly to the computer (no hub). USB audio input isn't offered (the sample recorder it fed was removed). On a busy set, streaming adds audio-interrupt load.
* Ported from Tracker CE (USB storage and audio for the Original Tracker, and its Windows mount fix); stereo samples from the same CE change are not included.

### Editor polish (Original Tracker and Mini)

* **Scrolling up** through blocks that are more than a page: the cursor moves up through the blocks on screen, and the page scrolls only when the current block reaches the top (it used to scroll on every step).
* **Screen 2 `#` / FX2 at the end of a block** append the new control (on the last value, e.g. the `0.3` of `# room 0.3`, they used to say "nothing fits here"). From inside any control the new one goes straight after it, without brackets (`# gain 0.9 # crush 4 # room 0.3`, not `# (gain 0.9 # crush 4)`).
* **Copy on a `#`** (the `#` itself, or the stop just before it) copies the whole control, `# gain 0.9`. **Paste** of it on a `#` replaces that control; anywhere else it is added at the end of the block. Other operators (`|+ n 12` ...) work the same way.
* **New blocks** (Insert at a block's top, then a slot pad) start as the empty placeholder above the current block, like the one Delete leaves, instead of `dN $ silence`. Note / Instrument / Screen 1-8 start it, Up / Down leave it.
* **Pads in Bool patterns** (the acid `acc` / `slid` / `time`, `struct`, `mask` ...): pad 1 enters `0`, pad 2 `1` (`f` / `t` in a pattern written with t / f), pad 48 a rest `~`. On the Mini's note entry these are Screen 1, 2 and 8.
* **Highlights on every pattern:** each note now lights the steps of up to 6 patterns of its block (was 2 on the Tracker), so an acid line's `time`, `note`, `oct`, `acc` and `slid` all light as they play, like `n` / `s`. Costs 1 KB internal RAM and ~31 KB SDRAM.
* Copy no longer moves the cursor back to the start of the block when it is on an operator, and a slot pad pressed while on a `#` no longer leaves the cursor on the new block's operator.

### Other changes

* MIDI: a note of a pitch that is still sounding (overlapping notes, e.g. `legato 2`) now releases the old note before it starts again.
* Flats are written with `f` (`ef2`), as in Tidal; `eb2` now says so in its error.

### Tracker Mini (feature/mini, device testing: RC1-RC5)

- The Tidal editor firmware now builds for the Tracker Mini (`make mini`).
  The Mini plays one note at a time per track for now (`# voices` plays as 1).
- Mini controls: Fn1-4 = Pattern / Song / File / Config, Menu = the module
  menu (Shift: Tidal folders), Middle = OK / chord menu (Shift: status),
  +-1 / +-10 = jog. **Shift+Note** turns the screen keys into note entry:
  8 pads of a window, Left / Right move it 1, Up / Down an octave (or 8
  instruments). Hold Fn1 and press a screen key to mute a track.
  Details: docs/tidal_mini.md.
- Numbers showed as "1d" on the Mini (its printf has no 64-bit integers):
  the code view writes digits itself now.
- Insert on a dN: the screen keys become d1-d8 for a new block (Shift+Insert
  d9-d16). Note entry in an `s` pattern lists only loaded instruments.
  Instrument on a sample name opens the sample menu (both models).
- **Open bug (parked):** `d1 $ splice 4 "0 1 2 3" $ s "amen_brother"`
  as a project's code stops the Mini during the boot's code load (RC2
  hung at 100%, RC3+ power off). The Mini now sets such code aside (Workspace/crash.tidal)
  and boots empty; the cause isn't found yet (docs/tidal_mini.md, "Boot
  diagnostics").
