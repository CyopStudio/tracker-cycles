# Tidal sample folders on the Tracker (design draft v0.1, 2026-11-11)

Status: draft. Decided with the user: a separate `/Tidal/` root on the SD
card; write this up, then a timing bench (section 7) before any loader.

## 1. The difference from normal Tidal

In Tidal + SuperDirt, `s "bd"` names a folder of samples, `bd:3` (or
`# n 3`) picks a file in it (index modulo the file count, files sorted by
name), and every folder SuperDirt loaded at startup is available at once.

Here (V1), `s` names one of the 48 Tracker instruments by its file name
(`tc::sampleToken`: lower case, other characters as `_`, 15 characters),
and `n` is pitch, unless the instrument is sliced: then `n` picks slice
`n mod count`. Folder lookup is the main remaining difference in everyday
use. (Others, unchanged by this: no SuperCollider synths, playback rate
approximated through tune, voice limits, global reverb / delay settings.)

## 2. Idea: one folder = one sliced instrument

The mapper already implements SuperDirt's rule inside one instrument
(`TrackerMapper::map`: sliced instrument -> `TMod::Slice = n mod slices`),
and a slice plays to the next slice by default (`slicePlayThru = 0`), so a
folder's files joined end to end with a slice at each file start behaves
like the folder:

- `s "bd:3"` / `s "bd" # n 3`: instrument `bd`, slice 3 mod file count.
- Files sorted by name (SuperDirt's order; to confirm against SuperDirt's
  documentation: plain byte order or case-insensitive).
- Instrument name = folder name (`file_name`, so `sampleToken` and the
  pads give `bd`).

Nothing new is needed in the audio engine for plain playback; the work is
a loader, plus `begin` / `end` on slices (section 5).

## 3. Card layout

```
/Tidal/
  bd/        BT0A0A7.wav  BT0A0D0.wav ...
  sn/        ...
  jvbass/    ...
```

- The same tree as Dirt-Samples (or the user's own folders), so folders
  can be copied from a laptop as they are.
- Folder names become `s` names through `sampleToken` (so `808bd` and
  `hi-hat` work; spaces become `_`).
- WAV only at first (the stock import's formats: PCM 8/16/24/32-bit,
  float, mono / stereo, any rate; converted to mono 16-bit 44.1 kHz).
  Other formats (Dirt-Samples is mostly WAV; some packs use FLAC / AIFF)
  are skipped with a warning.
- Optional later: a host tool (python3, stdlib) that pre-converts a tree to
  mono 16-bit 44.1 kHz WAV, so the device only copies (section 7 tells
  whether it is needed).

## 4. Which folders load, and when

SuperDirt loads everything at boot; the Tracker can't (memory, section 6).

Proposal: load on demand from the code.

- The editor knows every `s` / `sound` name in the program (mini atoms of
  String patterns under an `s` control). At evaluate, each name that
  matches no instrument but has a folder under `/Tidal/` is queued.
- A background job (file manager operation, main loop) builds the
  instrument; until it is ready the events of that name are silent and
  the status line says `loading jvbass (3/12)`. When it is ready the name
  cache is dropped (`TrackerMapper::instrumentsChanged`) and the next
  events play.
- Built folder instruments are ordinary project instruments: saved with
  the project (instrument file + its joined sample), so reopening the
  project is the stock load, with no folder scan.
- A project can pin folders (a comment line in the .tidal file, e.g.
  `-- tracker: load bd sn hh`) so they load at project open even before
  the code that uses them is evaluated. Optional.
- Slots: folder instruments use the first free of the 48 slots. When
  memory or slots run out: evict folder instruments that no block uses
  (stock instrument delete compacts the sample memory), else refuse with
  `no room for jvbass (needs 410 KB, 120 KB free)`.

Open: should the user's own (non-folder) instruments win over a folder
with the same name? Proposal: yes, an existing instrument always wins.

## 5. Building a folder instrument

1. List `/Tidal/<name>/` (files, `.wav`, hidden / macOS `._` files
   skipped), sort by name, keep the first 48 (`MAX_SLICE_NUMBER`; more:
   a warning, `n` wraps over 48).
2. For each file: read the header (`readHeader`), stream the data through
   `convertAudioData` (the stock converter: to mono 16-bit at 44117.647
   Hz) into the sample pool after the last instrument.
3. Slice positions are `uint16_t` fractions of the instrument length: pad
   each file with silence so its start lands exactly on a slice step
   (at most length / 65536 samples, ~3 ms for a 10 s instrument), and set
   `slices[i]`, `sliceNumber`, `playMode = Slice`, `slicePlayThru = 0`.
4. Write the instrument file and its sample into the workspace (stock
   `saveInstrumentStructToWorkspace` / sample save steps), so a power-off
   keeps it and project save copies it.

Mapper / engine changes:

- `begin` / `end` relative to the chosen slice (SuperDirt: relative to the
  file). Today they are ignored on sliced instruments
  (`mapping.cpp`: `points` only for one-shot / loop): map them to absolute
  points inside the slice; `chop` / `striate` need this too.
- `loopAt` / `splice` / `unit "c"`: rates from the slice length, not the
  whole instrument (later).

## 6. Limits on the Original Tracker

- 48 instrument slots in total, shared by folders and normal instruments.
- 48 files per folder (slice limit).
- Sample memory ~7.5 MB mono 16-bit (~89 s at 44.1 kHz; ~125 s with the
  `SAMPLE_MEMORY_SIZE` reclaim noted in CLAUDE.md). The full Dirt-Samples
  tree does not fit; a project uses a subset.
- Sliced instruments are fully resident: Slice mode can't stream on MK66
  (`isPlayModeStreamable()`). Folders of long files (pads, loops, breaks)
  would need one streamed instrument per file instead (later, if wanted).
- Loading time: unknown, measured by the bench (section 7).

## 7. Timing bench (next step)

`-DFOLDER_BENCH` (core/tidal/folder_bench.*): 15 s after boot, and again
at 60 s with a project playing, it walks `/Tidal/` the way the loader
would and writes `/tidal_folder_bench.txt`:

- per folder: listing time, file count, sort time, per-file open + header,
  read, convert, bytes read / produced, source formats;
- totals: wall time, MB/s read, ms per file, convert vs read share;
- run 2 vs run 1: the cost of loading while audio plays.

The converted data goes to a scratch SDRAM buffer (nothing is written to
the instruments or the card except the report). The main loop is blocked
while a run is going (screen and SD streaming pause; audio keeps running).
The answer decides whether on-demand loading at evaluate feels instant,
needs a "loading" state of a second or more, or wants pre-converted files
(section 3).

### 7.1 Results v1 (2026-11-11, docs/tidal_folder_bench.txt)

5 folders, 63 .wav files (53 read, 10 rejected), 9.9 MB read -> 7.7 MB
mono (89 s of audio).

| | run 1 (stopped) | run 2 (playing) |
| :--- | ---: | ---: |
| total | 2.16 s | 2.68 s |
| read (SD) | 1055 ms, 4.5 MB/s | 1334 ms, 3.6 MB/s |
| convert | 920 ms | 1143 ms |
| open + header | 141 ms (2.7 ms / file) | 150 ms |
| audio load during the run | 8 % | 26 % (max 35) |

Per folder (run 1 / run 2): bd (24 short files, 578 KB, some at another
rate) 0.24 / 0.31 s; hh (13 stereo, 3.3 MB) 0.49 / 0.64 s; sitar (8 mono,
4.6 MB) 0.90 / 1.05 s; slowest single file 146 / 166 ms.

Reading:
- Speed is fine for on-demand loading: a typical drum folder takes a
  quarter of a second, a big one about a second. A short silent gap
  before a new name plays is acceptable; it must not block the main
  loop, so the loader works in steps (a chunk per main-loop pass, as the
  stock file manager operations do) instead of one 2 s run.
- Loading barely touches the audio interrupt (run 2's audio load stays
  where the playing project puts it).
- Converting files that are already mono 16-bit 44.1 kHz cost as much
  as reading them (sitar: 385 ms read, 492 ms "convert", a copy into
  uncached SDRAM): the loader reads those straight into the sample pool.
- Memory is the real limit: these five folders alone are 7.7 MB, more
  than the whole ~7.5 MB pool (sitar 4.6 MB). Eviction, a clear "no room"
  message, and reclaiming SAMPLE_MEMORY_SIZE (~125 s) matter more than
  speed. Stereo folders halve when converted; a host pre-convert tool
  would also halve their read time.
- 10 of 63 files were rejected by readHeader / checkSampleValid (5 in
  808, 5 in juno); the bench did not record why (likely
  WAVE_FORMAT_EXTENSIBLE or a depth the stock import refuses). The loader
  reports each refused file and its header.

## 8. Steps after the bench

1. Loader as a file manager operation (folder -> sliced instrument in a
   given slot), triggered by hand first (a status-view action).
2. Names from the program at evaluate -> queue -> loader; mapper cache
   reset; silent until ready; status line.
3. `begin` / `end` per slice in the mapper / tidalNoteOn.
4. Eviction when memory or slots run out; pinned folders.
5. Optional host pre-convert tool.
