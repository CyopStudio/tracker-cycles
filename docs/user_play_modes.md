# User-Defined Play Modes (`playmodes.txt`)

Performance Mode's **Play Mode** FX offers `Fwd`, `Back`, `Rnd` and twenty custom step orders, `C1`–`C20`. A project can replace any of these twenty with its own step orders, and give each step an FX to play, by including a plain-text file named **`playmodes.txt`** in its project folder.

* Modes the file does not define keep their built-in behaviour. A project without the file plays exactly as before.
* A mode that contains an error keeps its built-in behaviour. The other modes in the file still load, and a short message says which modes fell back.
* Works on all three devices (Original Tracker, Tracker+ and Tracker Mini).
* The modes can also be edited on the Tracker, in the [Play Mode Editor](play_mode_editor.md), which writes this file for you.

---

## 1. Adding the File to a Project

1. Write a `playmodes.txt` file (see the format below).
2. Copy it into the project's folder on the SD card, next to `project.mt`: `/Projects/<Your Project>/playmodes.txt`.
3. Open the project on the Tracker (**Project → Open**). The file is read every time the project is opened, and again on the next power-up.
4. In Performance Mode, choose `C1`–`C20` with the **Play Mode** FX as usual.

**Editing an open project's file on a computer** (for example via USB Storage mode): open the project again on the Tracker to apply the changes. **Save** leaves the project's `playmodes.txt` as it is, so edits made on a computer are never overwritten. **Save As** copies the file to the new project.

> [!NOTE]
> Firmware without this feature ignores `playmodes.txt` and plays the built-in modes, so a project that relies on its own modes will sound different there.

---

## 2. File Format (version 1)

```text
# Lines starting with # are comments. Blank lines are ignored.
version 1

# FX presets: fx <id 1-64> <fx> <value> [chance %]
fx 1 roll 1/4 60          # roll 1/4, 60% chance
fx 2 volume 20            # volume 20, always
fx 3 reverse 1 30%        # reverse playback, 30% chance
fx 4 roll 1/2 down        # roll 1/2 with falling volume, always

# Modes: mode <1-20>, then the steps to play, over one or more lines
mode 3
1 2 3:2 4 1 2:3 3 4 9 10 11 12 1 1 15 16

mode 7
1 1:1 3 4:4               # a 4-step loop
```

* **`version 1`** must be the first line that is not a comment or blank. A file without it, or with another version, is ignored entirely.
* **`mode <n>`** starts the definition of `C<n>` (1–20). The steps that follow belong to it until the next `mode` or `fx` line.
* **Step tokens** are separated by spaces and may span several lines:
  * `<step>` plays that step (1–128) of the track.
  * `<step>:<fx>` also plays FX preset `<fx>` (1–64) on that step.
  * Steps beyond the track's length wrap around, as with the built-in modes.
* **Length**: a mode has up to 128 steps. A shorter mode repeats to fill 128, so `1 2 3 4` is a 4-step loop.
* **Text is not case-sensitive**, and `#` starts a comment anywhere on a line. Maximum line length: 255 characters. Maximum file size: 64 KB.

### FX Presets

`fx <id> <fx> <value> [chance]` defines preset `<id>` (1–64). The optional `chance` is 0–100, with or without `%`, and defaults to 100.

> [!NOTE]
> Firmware with this feature but without the Play Mode Editor's changes accepts preset ids 1–16 only. A mode that uses a higher id plays its built-in version there.

When a step with a preset plays, the FX is placed in the step's **first empty FX slot**. If both FX slots of the step are already used in the pattern, the step's own FX win and the preset is skipped. FX usually need a note on the step to have an audible effect.

**Without any `fx` lines**, presets 1–15 are the 15 roll variants the built-in modes use, so built-in modes can be copied into a file and edited:

| Preset | Roll | Preset | Roll | Preset | Roll |
| :---: | :--- | :---: | :--- | :---: | :--- |
| 1 | 1/4 flat, 30% | 6 | 1/4 down, 100% | 11 | 1/1 flat, 60% |
| 2 | 1/4 flat, 60% | 7 | 1/2 down, 30% | 12 | 1/1 flat, 100% |
| 3 | 1/4 flat, 100% | 8 | 1/2 down, 60% | 13 | 1/1 down, 30% |
| 4 | 1/4 down, 30% | 9 | 1/2 down, 100% | 14 | 1/1 down, 60% |
| 5 | 1/4 down, 60% | 10 | 1/1 flat, 30% | 15 | 1/1 down, 100% |

As soon as the file has an `fx` line, only the presets it defines exist.

### FX Names and Values

Values are the **raw values stored in a pattern**, which for some FX differ from what the screen shows (for example, `panning` 50 is centre).

| Name | FX | Values |
| :--- | :--- | :--- |
| `roll` | R – Roll | `<rate> [type]`: rate `1/1`, `1/2`, `1/3`, `1/4`, `1/6`, `1/8`, `1/12`, `1/16`; type `flat` (default), `down`, `up`, `notedown`, `noteup`, `noterandom` |
| `volume` | V – Volume/Velocity | 0–100 |
| `panning` | P – Panning | 0–100 (50 = centre) |
| `microtune` | M – Micro-tune | 0–198 (99 = no change) |
| `glide` | G – Glide | 0–100 |
| `micromove` | m – Micro-move | 0–100 |
| `gate` | q – Gate Length | 0–100 |
| `chance` | C – Chance | as in the pattern editor |
| `arp` | A – Arp | 0–33 |
| `randomnote` | n – Random Note | 0–100 |
| `randominstrument` | i – Random Instrument | 0–47 |
| `randomvolume` | v – Random Volume | 0–100 |
| `reverse` | r – Reverse Playback | 0–1 |
| `position` | p – Position | 0–100 |
| `slice` | S – Slice | 0–47 |
| `volumelfo`, `panninglfo`, `filterlfo`, `positionlfo`, `finetunelfo` | g, h, j, k, l – LFOs | as in the pattern editor |
| `overdrive` | D – Overdrive | 0–100 |
| `lowpass`, `bandpass`, `highpass` | L, B, H – Filters | 0–100 |
| `delay` | s – Delay Send | 0–100 |
| `reverb` | t – Reverb Send | 0–100 |
| `bitdepth` | E – Bit Depth | as in the pattern editor |
| `tune` | U – Tune | 0–48 |
| `slideup`, `slidedown` | F, J – Slides | 0–255 |
| `ampenv`, `filterenv` | w, y – WT Envelopes | 0–255 |
| `granularlength` | z – Granular Length | 0–100 |
| `cca` … `ccf` | a–f – CC / Macro | 0–127 |

"As in the pattern editor" means the same range the pattern editor allows for that FX (`Sequencer::getFxMin()` / `getFxMax()`).

**Not allowed**: `Tempo` and `Swing` (they change the whole project), `Break pattern` (it would interfere with how the play mode counts steps), `Off`, `Random FX value` and `MIDI Chord`.

---

## 3. Play Mode Editor (Prototype)

The modes can also be edited on the Tracker itself, in a grid with one lane per custom mode (`C1`–`C20`) and 128 rows, while Performance Mode keeps playing so every change is heard. Its controls follow the Pattern editor's: edit mode on `[Rec]`, pads and jog wheel, selection, copy and paste, the fill popup, the FX list, and Undo. **Save** writes this file.

* Open it in Performance Mode with **`[Shift]`**+**`[Pattern]`** (Tracker Mini: **`[Shift]`**+**`[Menu]`**).
* The editor is a build option (`PLAY_MODE_EDITOR`, on by default).

See the **[Play Mode Editor manual](play_mode_editor.md)**.

---

## 4. Errors

Errors never stop a project from opening. When the file has errors, a message titled **Custom Play Modes (playmodes.txt)** appears for 6 seconds once the project has opened:

| Situation | Message |
| :--- | :--- |
| Some modes have errors | `Modes C3 and C4 have errors.` / `They play their built-in versions.` / `First error: line 19.` |
| No `version 1` line | `The file was not loaded:` / `its first line must be: version 1` / `All modes play their built-in version.` |
| File empty, unreadable or too big | `The file was not loaded:` / `it is empty, unreadable or over 64 KB.` / `All modes play their built-in version.` |
| Errors that did not affect any mode | `The file has errors, but all its` / `modes loaded.` / `First error: line 2.` |

Up to three modes are named (`C1, C2, C3 and 17 more`). The line number is the first error in the file, which may belong to a different mode than the ones named.


| Problem | Result |
| :--- | :--- |
| No `version 1` line, or a different version | The whole file is ignored. |
| A step outside 1–128, an unreadable token, more than 128 steps, an empty mode, or a mode defined twice | That mode plays its built-in version. |
| An FX that is not allowed, a value out of range, a bad chance, or a preset defined twice | That preset is unusable, and every mode that uses it plays its built-in version. |
| A mode using a preset the file does not define | That mode plays its built-in version. |
| An unknown line | Ignored. If it is inside a mode, that mode plays its built-in version. |

---

## 5. Developer Notes

* **Code**: [`core/source/userPlayModes.cpp`](../core/source/userPlayModes.cpp) (memory block, FX allow-list, SD reading, file copying) and [`core/source/userPlayModesParser.cpp`](../core/source/userPlayModesParser.cpp) (the parser, free of firmware headers so it can be built and tested on a desktop compiler). The built-in modes stay in flash in [`core/source/playModeDefs.h`](../core/source/playModeDefs.h).
* **Sequencer**: [`Sequencer`](../core/source/mtSequencer.cpp) reads steps through `userPlayModes::getStep()` and FX through `userPlayModes::getFxPreset()`. A 32-bit mask selects, per mode, the built-in table or the user block. Nothing in the audio path touches the SD card.
* **Project lifecycle**: like `patternsMetadata` and synth patches. `ProjectOpen::openProjectFinish()` copies `<project>/playmodes.txt` to `Workspace/playmodes.txt`, or deletes a stale Workspace copy. `WorkspaceProjectLoad::loadProjectFromWorkspaceFinish()` parses it before the audio engine restarts. `ProjectSave::saveProjectFinish()` copies it to the destination on **Save As**, and leaves it alone on **Save**. A new project starts with an empty Workspace.
* **Error message**: `formatLoadWarning()` (parser) builds up to three lines after each load, and keeps them in `EXTERNAL_RAM`. `Interface::commonThingsUpdate()` shows them once with `mtPopups.showTitledPopup()` (a bold title above the lines, 6 seconds), when the interface is in `mtOperatingModeRun` and the FileManager is idle. This covers start-up, **Project → Open** and imports.
* **Load safety**: parsing clears the mask first (built-in modes play), writes the modes into the block, then publishes the mask last.
* **Memory**: one `__NOINIT(EXTERNAL_RAM)` block of 5,380 B (mask, 64 FX presets, 20 × 128 steps). Room for it is reserved in `USER_PLAY_MODES_MEMORY_SIZE` in [`core/source/mtStructs.h`](../core/source/mtStructs.h): 6 KB, or 7 KB with the Play Mode Editor, whose clipboard and undo buffer share the reservation. See [Memory Map](memory_map.md).
* **Editor**: [`core/interface/playModeEditor/`](../core/interface/playModeEditor/) edits the block through `userPlayModes::setStepNumber()` / `setStepFx()` / `resetMode()`, and `userPlayModes::save()` writes the file with the parser's `writeFile()`. See the [Play Mode Editor manual](play_mode_editor.md#13-building-with-or-without-the-editor).
