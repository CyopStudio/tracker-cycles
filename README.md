# Tracker Cycles

**TidalCycles-style live coding on the Original Polyend Tracker.**

Tracker Cycles turns the Original Polyend Tracker into a standalone live-coding instrument. You write patterns in real Tidal syntax (`d1 $ every 4 (fast 2) $ s "bd*2 [~ sd]" # gain 0.9`), edit them with the pads, jog wheel and buttons, and the Tracker's own sample engine and effects play them. You don't need a computer, SuperCollider or GHC.

It is a fork of [Polyend Tracker Community Edition (CE)](https://github.com/polyend/tracker-ce). The step sequencer, pattern editor and song mode have been replaced by a Tidal pattern engine and a structured code editor. The sample engine, instruments, sample loader and editor, mixer, effects and MIDI are the Tracker's own.

* **Hardware:** Original Polyend Tracker (MK66) only. Tracker+ and Tracker Mini are not supported.
* **Version:** 1.0.0. See [`CHANGELOG.md`](CHANGELOG.md).
* Tracker Cycles is inspired by [TidalCycles](https://tidalcycles.org). It is an independent implementation and is not affiliated with the TidalCycles project or with Polyend.

---

## Disclaimer

Tracker Cycles is provided **"AS IS"**, without warranty of any kind (see [`LICENSE`](LICENSE)). Neither Polyend nor the TidalCycles project supports it. Custom firmware is used at your own risk, and hardware problems caused by it are not covered by Polyend's warranty (Polyend offers out-of-warranty repairs). The Tracker's bootloader is separate from the firmware, so you can always flash a known-good firmware again (see [Recovery](#recovery)).

Back up your SD card before flashing. Tracker Cycles has no patterns or songs: it opens a project's instruments, samples and settings, and adds its code (see [Projects](#projects-and-the-sd-card)).

---

## Install

1. Download `PolyendTracker_Cycles_<version>.ptf` from [Releases](../../releases).
2. Copy it to the `/Firmware/` folder on the Tracker's microSD card.
3. On the Tracker: **Config → Firmware → Firmware update**, select the file and confirm.
4. After the reboot, **Config → Firmware** shows `tracker-cycles 1.0.0`.

### Recovery

If a firmware build ever fails to boot:

1. Put a known-good `.ptf` (Tracker Cycles, CE or Polyend's) alone in `/Firmware/` on the card.
2. Insert the card and power on the Tracker.
3. With a paperclip, press the recessed **Reset / bootloader** pinhole on the bottom of the unit. The bootloader flashes the `.ptf` from `/Firmware/`.

`.ptf` file names must start with `PolyendTrac` and be at most 31 characters long, or the updater won't list them.

---

## Quick start

The screen shows your code: a `setcps` line and blocks `d1` to `d16`.

* **d1–d8** play the 8 sample tracks (the project's 48 instruments, chosen by name with `s`).
* **d9–d16** send MIDI: d9 on channel 1 through d16 on channel 8 (`# midichan` overrides).

1. **Shift + pad 1** goes to `d1`. On an empty slot, press **Instrument** to start a block with `s "<first instrument>"`, or **Note** for `n "0"`.
2. Move with the **arrows**. The editor starts in **cursor mode**: **Left / Right** go token by token, **Up / Down** line by line and on to the blocks around. **Rec** switches to tree mode, where Left / Right step through the code tree and Up / Down go to the parent / first child.
3. Turn the **jog wheel** to change the value under the cursor (numbers, notes, sample names, functions). **Shift + jog** takes bigger steps; on a sample it changes the file index (`bd` → `bd:1` …).
4. In a `"..."` sequence the **pads** enter values (notes on the keyboard layout, or the 48 instruments in an `s` pattern). Holding pads together enters a chord.
5. Press **Play** to evaluate the block. It flashes, and its `dN` turns green.
6. Hold **Pattern** and press a slot pad to mute / unmute it. **Shift + Pattern** alone is `hush`.

### Main controls

| Control | Action |
| :--- | :--- |
| Play | evaluate the block (Shift: silence it) |
| Pattern (held) + pad N | mute / unmute dN (Shift: solo); Pattern alone: unmute all, Shift + Pattern alone: `hush` |
| Shift + pad N | go to dN; the same pad twice in a row: a new dN block |
| Shift + ↑ / ↓ | previous / next block |
| Screen 1–8 | structure menus: `$` functions, `#` controls, `[ ]`, `< >`, `{ }`, `( , )` euclid, `\|`, operators |
| Rec | cursor mode / tree mode |
| Shift + Screen 1–4 | `# gain 0.9`, `# room 0.3`, `# delay 0.3`, `# shape 0.3` |
| Shift + Screen 5 | line break before the argument / operator at the cursor |
| Shift + Screen 6 / 7 | inside `"..."`: repeat the step (`bd!2`) / hold it longer (`bd@2`); the jog changes the count |
| Shift + Screen 8 | Undo |
| Note / Instrument | jump to the block's `n` / `s` pattern (Shift + Note: chord names) |
| FX1 / FX2 | pattern modifiers (`$` functions) / sound effects (`#` controls) |
| Song | launch menu (`dN $`, `jumpMod`, `jumpIn`); Shift + Song: tempo (`setcps`) |
| Copy / Shift + Copy | copy / paste (at a block's top: duplicate the block) |
| Delete / Shift + Delete | delete the token (inside `"..."`: a step becomes `~`; on a function, `$` or `#`: unwrap) / the token to the left (tree mode: unwrap) |
| Insert / Shift + Insert | on a function's name: a `$` function in front; elsewhere a `#` control behind; inside `"..."`: a copy of the step / the block's top |
| Shift + Sample Loader | load a Tidal sample folder (below) |
| Shift + Config | edit the block's comment |
| Shift + File | save the code to the project |
| Shift + Master | status (tempo, audio load, voices, errors) |

The full control reference is in [`tidal/README.md`](tidal/README.md).

---

## What it plays

Tracker Cycles evaluates real Tidal syntax: function application, `$`, `.`, operators and sections, mini-notation strings, lists and pairs. It supports about 140 functions and operators. Each one is type-checked like GHC would, and checked against event lists captured from Tidal 1.10.3. The full list is in [`docs/tidal_code_tree.md`](docs/tidal_code_tree.md) (section 7). Highlights:

* **Time:** `fast` `slow` `hurry` `rev` `iter` `iter'` `ply` `linger` `segment` `every` `every'` `foldEvery` `whenmod` `repeatCycles` `qtrigger` `trigger` `inside` `outside` `within` `within'` `zoom` `compress` `trunc` `fastGap` `swingBy` `press` `rot` `<~` `~>`
* **Structure:** `struct` `mask` `euclid` (and `Inv` / `Full` / `Off`) `sew` `stitch` `while` `wedge` `bite` `squeeze` `select` `run` `scan` `range` `rangex` `quantise` `fix` `unfix` `contrast` `append` `fastAppend`
* **Samples:** `slice` `randslice` `splice` `chew` `loopAt`, sample folders with `s "bd:3"`
* **Random:** `degradeBy` `sometimesBy` (and friends) `someCyclesBy` `shuffle` `scramble` `rand` `irand` `brand` `brandBy` `perlin` `choose` `wchoose` `randcat` `wrandcat`
* **Layering:** `superimpose` `off` `jux` `juxBy` `stut` `stutter` `echo` `echoWith` `ghost` `chunk` `chunk'` `plyWith` `layer` `pickF` `selectF`
* **Harmony:** `scale` `toScale` `arp` `rolled` `rolledBy`, chord names (`"c'maj e'min7"`)
* **Signals:** `sine` `cosine` `saw` `isaw` `tri` `square` `envL` `envLR` `envEq` `envEqR`
* **Controls:** `s` `n` `note` `gain` `pan` `cutoff` `resonance` `hpf` `bandf` `delay` `room` `crush` `shape` `begin` `end` `speed` `legato` `attack` `release` `cut` `djf` `size` `delaytime` `delayfeedback` and more, plus the MIDI controls on d9–d16 (`ccn` / `ccv`, `midibend`, `progNum`, `miditouch`)
* **Operators:** `#` and the `|+|` `|-|` `|*|` `|/|` `|%|` `|**|` `|<|` `|>|` families, `+ - * /`

### Differences from TidalCycles

* **Sound:** the Tracker's sample engine replaces SuperDirt.
  * There are no SuperCollider synths (`superpiano` and so on).
  * Playback rate (`speed`, `loopAt`) is approximated through tuning.
  * The reverb and delay are global: `size`, `delaytime` and `delayfeedback` set them for everything (the last note wins). `coarse` isn't available (use `crush`).
  * `chop` and `striate` are not available on the device yet: the editor accepts them, but evaluating refuses them.
* **Voices:** each track plays one note at a time unless you ask for more with `# voices 2` or `# voices 3` (Tracker Cycles' own control). In total at most 16 notes sound, fewer when the CPU is busy.
* **Samples:** `s` picks one of the project's 48 instruments by name. Sample folders (below) bring back Tidal's `s "bd:3"` folder lookup.
* **Language:** there are no lambdas, `let`, user-defined functions or type annotations. Tuples are pairs only. `echoWith` and `plyWith` are limited to 8 layers.
* **Random values:** they follow Tidal's probabilities but differ from Tidal's actual values.

---

## Projects and the SD card

```
/Projects/<Name>/            a Tracker project (instruments, samples, settings)
/Projects/<Name>/<Name>.tidal   the project's code
/Workspace/project.tidal     the working copy (saved ~2 s after every edit)
/Tidal/<folder>/*.wav        Tidal-style sample folders (optional)
/tidal_folders.txt           the sample-folder loader's log
/Firmware/                   firmware files (.ptf)
```

* **File → Save / Save As** saves the instruments, samples and code. **Shift + File** saves only the code.
* Opening a project loads its code. A new project starts with an empty editor.
* Stock projects open with their instruments and samples. Their patterns and songs are not loaded or changed: the files stay in the project folder, so the stock firmware can still open the project.

### Sample folders

Copy Tidal-style sample folders to `/Tidal/` on the card, for example `/Tidal/bd/`, `/Tidal/hh/` with WAV files inside. **Shift + Sample Loader** lists them, and OK loads one into a free instrument slot:

* The files are sorted by name and joined into one sliced instrument, so `s "bd:3"` (or `s "bd" # n 3`) plays the fourth file, as in SuperDirt. At most 48 files per folder are used.
* WAV files of any common format (8–32 bit PCM or 32-bit float, mono or stereo, 8 kHz and up) are converted to the Tracker's mono 16-bit 44.1 kHz. Refused files and the reasons are listed in `/tidal_folders.txt`.
* Loading takes about a quarter of a second for a drum folder and a few seconds for large ones. The screen may pause while a folder loads, so load folders before a performance rather than during one.
* Sample memory is about 7.5 MB (around 89 seconds of mono audio), shared with the project's other instruments.
* Sample names come from file names (lower case, other characters become `_`, at most 15 characters). A name needs at least one letter: `808 5.wav` won't work in a pattern, `808 kick 5.wav` does.

The [Dirt-Samples](https://github.com/tidalcycles/Dirt-Samples) set used by SuperDirt has no clear licence. You can copy folders from it onto your own card, but they are not distributed with Tracker Cycles. For shareable sounds, see [Clean-Samples](https://github.com/tidalcycles/Clean-Samples) and its sound packs, which give licence information for each pack.

---

## Building from source

Requirements: GNU Arm Embedded Toolchain 10.3-2021.10 (`arm-none-eabi-gcc`), `make` and `python3`. See [`CLAUDE.md`](CLAUDE.md) for the developer notes.

```bash
make original              # -> build/PolyendTracker_Cycles_1.0.0.ptf
make -C tidal test         # host tests: engine, editor, golden cases from Tidal
make -C tidal smoke        # editor sessions in the desktop prototype
tidal/run_proto.sh         # the desktop prototype (needs GHC + Tidal + SuperDirt for sound)
```

* `tidal/`: the portable engine, editor and tests (host build).
* `core/tidal/`: the firmware glue.
* `docs/tidal_*.md`: the design documents.

---

## Credits & License

* **Firmware base:** [Polyend Tracker Community Edition](https://github.com/polyend/tracker-ce) by Polyend and contributors, **MIT License with a Wholesale Commercial Clone Restriction** (see [`LICENSE`](LICENSE)). Tracker Cycles is under the same terms. Please don't sell compiled builds.
* **Inspiration:** [TidalCycles](https://tidalcycles.org) by Alex McLean and contributors. Tracker Cycles implements Tidal's documented behaviour independently and checks it against Tidal's output. No TidalCycles (GPL) or Strudel (AGPL) source code is used.
* **Third-party components:** see [`NOTICE.md`](NOTICE.md).
