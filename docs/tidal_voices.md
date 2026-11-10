# Polyphony on the Original Tracker (plan 4c), design v0.2

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

3. **`# voices N` (v0.2, after tick v7).** Polyphony is opt-in per note:
   `voices` (Tracker Cycles' own control, 1-3, default 1) is how many
   notes the track lets ring, the new one included. Without it each note
   cuts the last (as before polyphony), so everyday drum patterns cost
   what they did in v6; v7 (always up to 3) let long hats ring under each
   other and pushed 8-track sets to 88-102 % audio load.

## `voices`

- A per-note control like any other: `# voices 3`,
  `# voices "<1 3>"`, `sometimes (# voices 3)`. Sample tracks only
  (Tracker mode refuses it on d9-d16). Clamped to 1-3.
- Not in Tidal: plain Tidal needs `let voices = pI "voices"` (the
  prototype's boot.ghci and golden/capture.py define it; golden case
  ctl_voices). The name doesn't clash with Tidal / SuperDirt (`voice`,
  singular, exists).
- SuperDirt ignores it and lets every note ring, so the prototype routes
  it: boot.ghci's `d1`-`d8` apply `fix (|< cut i) (voices 1) (p |< voices
  1)` (a note with 1 voice gets its track's own cut group unless it has a
  `cut`); the engine backend adds the same `voices` / `cut` to its OSC.
- Device: a note with `voices N` keeps at most N - 1 of its track's other
  notes: the oldest extra one is retriggered with the new note, any
  further extras fade out in 5 ms (`tidalCut`).
- Entry: `voices` uses one of the two spare bytes (Entry stays 48 B).

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
     player 0 (other notes beyond `voices` fade);
  2. more than voices - 1 others sounding: retrigger the oldest, fade the
     rest;
  3. a free player of the track while fewer than the cap sound (free:
     idle longest);
  4. else the track's oldest sounding player (retrigger: the player's own
     short release-on-noteOn, as one voice per track did);
  5. else (track silent, cap reached) the oldest sounding player of any
     track fades out (5 ms; v7 stopped it at once).
  A player started in the last 2 blocks counts as sounding (v7 counted
  only isPlaying(), so starts in the same block, or during a
  retrigger's release-on-noteOn, looked free: up to 19 sounding with cap
  16). A faded player no longer counts; one still playing 8 blocks after
  its fade (an envelope mode without a timed release) is stopped. The cap
  is set at run time (`setCap`, default 16).
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

## Played-out one-shots go idle (v8, all builds)

A one-shot that reached its end (`playing` 0) with no noteOff kept its
envelope in sustain and sent a zero block every block, so its track's
chain, sends and reverb ran on silence (v7: extra players never got a
noteOff, so every used track stayed busy; +9..+16 points on solo sets vs
v6 on the same project). AudioPlayMemory::update now switches such a
player's envelope to idle (16 zero blocks, then nothing), and
`tidalAllOff` releases every player.

## v10: load-driven cap

The user's choice (2026-11-10): the cap follows the audio load, max 16.
DeviceVoice::blockLoad(us) runs once per block in the interrupt with the
last audio callback's time (Profiler::lastAudioUs()) and keeps a smoothed
load (% of the 2902 us block): fast up (~4 blocks), slow down (~32 blocks,
~93 ms). Effective cap = 16 at <= 70 %, falling in a line to 8 at >= 85 %
(kLoadLo / kLoadHi / kMinCap in tidal_device.h). At a noteOn, players
sounding beyond the effective cap fade out oldest first (5 ms, up to 4 per
note: "governor cuts"), then the usual rules apply with that cap. The
report's `governor:` line gives the highest smoothed load, the lowest and
average cap at noteOns and the governor cuts.

## v9 fixes

- Scheduler: a start due in a block is skipped if the k-th later start
  due in the same block has `voices` <= k (it would cut it at once);
  v8 used 3 for every note, so voices-1 tracks passed up to 3 starts per
  block, each counted by the 4-starts-per-block cap (heavy set: 1759
  capped, 942 notes 2+ blocks late vs 434 / 0 in v6).
- DeviceVoice: a streamed instrument's new note at the cap fades the
  oldest sounding player out (v8 reached 14 sounding at cap 12).
- Default cap 14. Tick v9: chords at caps 12 / 14 / 16, everyday with
  voices 3 at caps 12 / 14 (15 sets).

## Test (tick v8)

18 -> 14 sets: 1 silence; 2-5 the timing sets with default voices 1
(compare with v6 on the same project); 6-8 eight tracks of ringing
chords with `voices 3` at caps 8 / 12 / 16 (load vs cap); 9 the everyday
set with `voices 3` on every track at cap 12; 10-12 polyphony listening
(voices 3); 13 `voices "<1 3>"` (one note, then a chord); 14 MIDI. The
`players:` line adds notes faded by `voices` and for the cap.

## Test (tick v7)

Set 6 overfills the cap (8 tracks of 3-note chords, legato 3); sets 15-17
are polyphony listening sets (chords on d4, overlapping bass on d5,
echoes on d3). The report adds a `players:` line (most sounding, average,
retriggers, cap steals, stale noteOffs, audio blocks in use) and the heap
in use.
