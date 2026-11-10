# Polyphony on the Original Tracker (plan 4c), design v0.1

How Tidal notes on d1-d8 overlap on the Original Tracker: release tails,
chords, `legato` > 1, `stut` / `echo` echoes. Before this, each track was
one sample voice and every new note cut the last.

## Decisions (2026-11-10, with the user)

1. **Shared track chain.** Each track has 3 sample players that sum into
   the track's one bit-crusher / filter / amp / pan / sends chain (the
   "fallback" in CLAUDE.md's Polyphony plan, like SuperDirt's per-orbit
   effects). Full voices (a chain each) cost ~3 KB internal RAM apiece
   (~1.7 KB chain + ~5 audio blocks in flight) and did not fit (~14 KB
   spare); an extra player costs ~0.7 KB.
2. **3 players per track, at most 16 sounding** at once (24 exist).
   One sounding player costs ~2.7 % of the audio block (the player alone,
   v6 audio profile); all 24 with reverb and delay busy would overrun.

## What is per note and what per track

| Per note (the player) | Per track (the latest note's) |
| :--- | :--- |
| instrument, pitch (note / speed / fine tune), begin / end, slice, reverse | cutoff / hcutoff / bandf + resonance |
| volume (gain / amp / velocity) | crush, shape |
| amp envelope (attack / release, legato / sustain noteOff) | pan |
| | room, delay sends |

So a ringing tail follows the next note's filter, pan and sends; volume
does not jump (it is the player's gain). Pitch modulation that the
player engine refreshes every block (LFOs, glide, performance mode)
follows the newest note only.

## Audio graph (per track, MK66)

```
player 0 ─┐
player 1 ─┼─ AudioSumPlayers<3> ── bit-crusher ── filter ── amp ── MonoToStereo ── track mixer
player 2 ─┘   (gain per player)                         └─ sends (post filter)
```

- `AudioSumPlayers` (core/libs/Audio/sum_players.h): no input -> no
  output; one input at unity gain -> passed through; else sum of input x
  gain (Q16, `smulwb` / `smulwt`), saturated.
- Players are declared before the sum and the sum before the crusher in
  `AudioVoice` (construction order = AudioStream update order), so a
  player's block is consumed in the same pass: at most 2 extra audio
  blocks in flight at a time.
- The stock sequencer (and pads / previews) only use player 0 at unity
  gain (`tidalRestoreStock()` in every stock noteOn).

## Who does what

- **Scheduler** (tidal/core/scheduler.*, portable, host-tested):
  `setSlotPlayers(n)` replaces mono slots: of the starts of one slot due
  in a block only the last n play (`superseded`); with n >= 2 a new note
  no longer cancels the slot's pending noteOffs, and every noteOff gets
  back the tag its noteOn returned (`Voice::noteOn` returns uint16_t,
  `Voice::noteOff(..., tag)`). Tests: testPlayers (last 3 per block,
  every noteOff once with its own note's tag; players 1 still cancels),
  4 mutations caught.
- **DeviceVoice** (core/tidal/tidal_device.*, audio interrupt) picks the
  player:
  1. streamed instrument (VoiceStreamManager has one stream per track):
     player 0;
  2. a free player of the track while fewer than 16 sound (free: idle
     longest);
  3. else the track's oldest sounding player (retrigger: the player's own
     short release-on-noteOn, as one voice per track did);
  4. else (track silent, cap reached) the oldest sounding player of any
     track is stopped.
  Tag = player | note serial << 2; a noteOff whose player was taken since
  is ignored (counted as stale).
- **playerEngine** (core/AudioEngine/mtAudioTidal.cpp):
  `tidalNoteOn(player, ...)` points `playMemPtr[0]` at that player, so the
  stock setters and update() refresh act on it; the note's volume
  (`ampLogValues[step]`) is its gain in the sum, and the track amp and
  sends run at unity (`tidalVolume`: getMostSignificantVolume() = 50 =
  1.0). Player-level overrides are ended per player (`tidalModsP`),
  track-level ones per track (`tidalMods`). `tidalNoteOff(player)`:
  the newest note -> the track's noteOff (envelopes / LFOs too), an older
  one -> only its amp release. `tidalStop`, `tidalPlaying`,
  `tidalAllOff`.

## Costs

- Internal RAM +5.7 KB in every build (`instrumentPlayer` 13.9 -> 19.6 KB:
  2 x AudioPlayMemory 328 B + sum node per track); stock `make original`
  RAM 156.6 -> 162.3 KB, flash +1 KB. Tick build RAM 198.2 KB (heap ~26
  KB above the 32 KB stack reserve, was ~32 KB: the v7 report prints the
  heap in use).
- CPU: ~2.7 % per extra sounding player; the sum node only does work when
  2+ players sound or a gain is not 1.

## Known approximations

- Note volume now sits before the crusher / filter drive instead of after
  the filter (amp): louder notes drive `shape` a little harder.
- A stolen player stops at once (a click is possible on the cap path);
  a retriggered one uses the player's release-on-noteOn.
- The track's filter / pan envelopes and LFOs restart with each note
  (as before), also under older notes' tails.

## Test (tick v7)

Set 6 overfills the cap (8 tracks of 3-note chords, legato 3); sets 15-17
are polyphony listening sets (chords on d4, overlapping bass on d5,
echoes on d3). The report adds a `players:` line (most sounding, average,
retriggers, cap steals, stale noteOffs, audio blocks in use) and the heap
in use.
