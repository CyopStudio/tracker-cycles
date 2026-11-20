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

### Other changes

* MIDI: a note of a pitch that is still sounding (overlapping notes, e.g. `legato 2`) now releases the old note before it starts again.
* Flats are written with `f` (`ef2`), as in Tidal; `eb2` now says so in its error.
