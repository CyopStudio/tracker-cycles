# Tidal controls on the Tracker (plan step 4c), design v0.2

How a Tidal event's controls (`s`, `n`, `gain`, `pan`, `cutoff`, ...)
become a sound on the Original Tracker: a sample voice on tracks 1-8, or
MIDI out for d9-d16. Semantics follow Tidal / SuperDirt's documented
behaviour (tidalcycles.org reference, Tidal Club answers), not their
source (CLAUDE.md licensing). Tracker facts below are from this repo
(core/AudioEngine, core/source/mtStructs.h).

## 1. Where it runs

```
main loop                                   audio interrupt
---------                                   ---------------
query -> Event (controls)
  -> Mapper (tidal/core/mapping.*, portable)
     resolve s -> instrument, n -> slice/pitch,
     convert every control to Tracker units
  -> Entry (block, dur, instr, note, mods)  -> tick() -> Voice
                                               sample: playerEngine::tidalNoteOn()
                                               MIDI:   sendMidiNoteOn/Off, CC
```

- **All conversion on the main loop.** The mapper is portable C++ in
  tidal/core (host-tested, like the scheduler); it needs only a small
  description of the project's instruments (name, play mode, slice
  count) that the device fills in. The interrupt only copies values.
- **The interrupt calls one new player method**,
  `playerEngine::tidalNoteOn(instr, note, mods)` in a new file
  core/AudioEngine/mtAudioTidal.cpp. The stock sequencer path
  (`noteOn` + `seqFx`) only carries two 8-bit step FX, so it can't take
  `# pan # cutoff # room # crush ...`.
- **Every event starts from the instrument's settings.** SuperDirt
  events don't inherit controls from the previous event, so
  `tidalNoteOn` sets the per-note override (the player's existing
  "sequencer FX lane 0": `trackControlParameter[sequencerMode][param]`
  flag + `currentSeqModValues` field) for each control the event has
  and clears it for every one it doesn't. The player's own
  `update()` refresh then combines it with envelopes / LFOs as for step
  FX. Performance mode keeps priority, as it does over step FX.
- **Per-name instrument cache.** `s` names are looked up once per
  program string (not per event, v3 measured up to 37k cycles per event
  for the scan); the cache resets when the project's instruments change.

## 2. Queue entry

The 32 B Entry has 6 mods. Live code stacks controls
(`# pan sine # cutoff 800 # resonance 0.2 # room 0.3 # crush 6 # legato
0.5 # speed 2` is ordinary), so **entries are 48 B with 10 mods** (uint8 id + uint16 value each):

| | 32 B / 6 mods | 48 B / 10 mods |
| :--- | ---: | ---: |
| Ring memory (16 x 128) | 64 KB | 96 KB |
| Main loop copy / ISR read per entry | same order | +50 % bytes, ISR reads only due entries |

Pitch (note + speed + fine tune) travels in `note` + one fine-tune mod;
volume in `vel`; instrument in `chan`. More controls than mods: the
mapper drops the lowest-priority ones and counts it (a "too many
controls" warning on the slot), never an error mid-performance.

## 3. Sample tracks (d1-d8)

Defaults: SuperDirt plays an event at amp 0.4 x gain^4 (gain capped at
2), full sample, no filter, unless controls say otherwise. Here the
instrument's own settings are the default (its volume = gain 1, amp
0.4), and controls override them per event.

| Tidal | Tracker | Conversion |
| :--- | :--- | :--- |
| `s` / `sound` | instrument (`chan`) | name -> instrument (file name without extension, case-insensitive; V1 = the 48 loaded instruments). `s "bd:3"` = `s "bd" # n 3`. Unknown name: event skipped, slot warning. |
| `n` | slice, or pitch | Slice / Beat Slice instruments: slice `n mod sliceNumber` (SuperDirt picks file `n mod count` in a folder, same idea). Other play modes: semitones, like `note`. |
| `note` / `up` | note | C5 (60) + note; fractional part -> fine tune |
| `speed` | pitch, reverse | semitones += 12 log2 \|speed\| (rate 0.25-4: tune +-24 st); speed < 0 -> reverse playback; 0 -> skip event |
| `gain`, `amp`, `velocity` | volume | ratio = (amp / 0.4) x min(gain, 2)^4 x velocity, applied to the instrument's amplitude (`ampLogValues[instr.volume]`), nearest step of the 0-100 curve |
| `pan` | panning | 0-1 -> 0-100 (Tracker pan law: balance, centre = both sides full; SuperDirt uses equal power) |
| `cutoff` / `lpf` | low-pass | Hz -> cutoff = log2(f / 20) x 50 / 477 (the filter's own table: 20 Hz x 2^(i/50), i < 478, top ~17.6 kHz); filter on, type LP |
| `hcutoff` / `hpf` | high-pass | same curve, type HP |
| `bandf` / `bpf` | band-pass | same curve, type BP |
| `resonance` / `lpq`, `hresonance` / `hpq`, `bandq` / `bpq` | resonance | 0-1 -> 0-4.3 (RESONANCE_MAX); needs a new override (resonance is instrument-only today) |
| `shape` / `distort` | overdrive | 0-1 -> 0-100 |
| `crush` | bit depth | 1-16 bits -> 4-16 (BIT_DEPTH_MIN 4: crush < 4 = 4) |
| `room` | reverb send | 0-1 -> 0-100 |
| `delay` | delay send | 0-1 -> 0-100 |
| `begin`, `end` | start / end point | 0-1 -> 0-65535 of the sample (`modSeqPoints`); not on Slice / Granular / Wavetable instruments |
| `attack`, `release` | amp envelope A / R | seconds -> ms (`ampAttack` / `ampRelease`) |
| `legato` | noteOff | after legato x event length (scheduler `dur`) |
| `sustain` | noteOff | after `sustain` seconds |
| `cut` | (implicit) | each track is one voice: a new note always cuts the last (cut groups across tracks: later) |
| `nudge` | timing | done by the scheduler |
| `orbit` | ignored | one FX bus |

Only one filter per voice: if an event has more than one of
lpf / hpf / bpf, the last one in the event wins (and a warning).

Global engine parameters (one reverb, one delay for all tracks):
`size`, `dry`, `delaytime`, `delayfeedback` set the global value when an
event that has them plays (last one wins), not per event. For now: not
mapped (Tracker-mode error), revisit after the per-event set works.

Not mappable in V1 (Tracker-mode error in the checker, so the editor
menus never offer them): `vowel`, `coarse`, `squiz`, `accelerate`,
`unit "c"` (loopAt), `waveloss`, `phaserrate`, `leslie`, `djf`,
`octave`, synth-only params, ...

## 4. MIDI tracks (d9-d16)

| Tidal | MIDI | Conversion |
| :--- | :--- | :--- |
| slot | channel | d9 = channel 1 ... d16 = 8; `midichan` (0-15) overrides |
| `note` / `n` | note number | 60 + note (Tidal's MIDI convention: note 0 = middle C, 60) |
| `velocity` / `amp` / `gain` | velocity | 100 x the amplitude ratio (amp / 0.4 x min(gain, 2)^4 x velocity), clamped to 0-127; none given = 100 |
| `legato` / `sustain` | noteOff | as above; default: noteOff at the end of the event (legato 1) |
| `ccn` + `ccv` | control change | number, value 0-127 |
| `midibend` | pitch bend | -8192..8191 |

MIDI notes already go out uncapped (scheduler). A chord (`n "c'maj"`)
is 3 entries in the same block: the per-slot noteOff table holds 8.

## 5. Tracker mode in the editor / checker

The control table lives in tidal/core/mapping.* and is the one place
that says what the Tracker can play. The checker uses it in Tracker
mode (CLAUDE.md step 1: "anything else is an error"), the editor's
`#` control menus only list mapped controls, and the prototype's GHCi
backend keeps the same restriction so what plays on SuperDirt in Tracker
mode is what the device can do (minus sound differences).

## 6. Work, in order

1. Done (2026-11-09): tidal/core/mapping.{h,cpp} (TrackerMapper,
   TrackerHost, TMod; 4 KB flash at -Os), Entry 48 B / 10 mods, new
   controls amp, velocity, attack, release, hresonance, distort, midichan,
   ccn, ccv, midibend + aliases lpf, lpq, hpq, bpf, att, rel (golden
   cases ctl_aliases / ctl_tracker / ctl_midi from real Tidal);
   tests/test_mapping.cpp (every conversion, instruments / cache /
   fallback, slices, lengths, priority drop, MIDI, through the
   scheduler; mutations in the gain curve, filter curve and priority
   order caught).
2. Checker: Tracker-mode errors from the table; editor menus filtered.
3. Device: `playerEngine::tidalNoteOn` (core/AudioEngine/mtAudioTidal.cpp)
   + MIDI out; tick test v4 plays a listening set (pan, cutoff sweep,
   crush, room, begin/end, slices, speed, legato, MIDI) and reports
   noteOn cost with mods.
4. Then the per-channel voice allocator (Polyphony plan) is the next
   step on top: the Entry gets a voice field.

## 7. Decisions (2026-11-09, with the user)

1. Entry 48 B / 10 mods (rings 96 KB in SDRAM).
2. Volume reference: the instrument's volume = gain 1 (amp 0.4);
   gain / amp / velocity scale relative to it.
3. Looped instruments without legato / sustain release at the event's
   end (legato 1); one-shots play out.
4. `n` on a non-sliced instrument = pitch in semitones; on Slice / Beat
   Slice it picks the slice.

Still open: global reverb / delay parameters (`size`, `delaytime`, ...)
from patterns: later.
