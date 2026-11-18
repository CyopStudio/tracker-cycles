# User-Defined Play Modes (`playmodes.txt`)

Performance Mode's **Play Mode** FX offers `Fwd`, `Back`, `Rnd` and twenty custom step orders, `C1`–`C20`. A project can replace any of these twenty with its own step orders, and give each step an FX to play, by including a plain-text file named **`playmodes.txt`** in its project folder.

* Modes the file does not define keep their built-in behaviour. A project without the file plays exactly as before.
* A mode that contains an error keeps its built-in behaviour. The other modes in the file still load, and a short message says which modes fell back.
* Works on all three devices (Original Tracker, Tracker+ and Tracker Mini).

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

# FX presets: fx <id 1-16> <fx> <value> [chance %]
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
  * `<step>:<fx>` also plays FX preset `<fx>` (1–16) on that step.
  * Steps beyond the track's length wrap around, as with the built-in modes.
* **Length**: a mode has up to 128 steps. A shorter mode repeats to fill 128, so `1 2 3 4` is a 4-step loop.
* **Text is not case-sensitive**, and `#` starts a comment anywhere on a line. Maximum line length: 255 characters. Maximum file size: 64 KB.

### FX Presets

`fx <id> <fx> <value> [chance]` defines preset `<id>` (1–16). The optional `chance` is 0–100, with or without `%`, and defaults to 100.

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

Play modes can also be edited on the Tracker itself, in a grid with one lane per custom mode (`C1`–`C20`) and 128 rows. Edits are heard straight away, the next time the edited step plays.

### Opening and Leaving the Editor

* **Open it from Performance mode only**: hold **`[Shift]`** and press **`[Pattern]`** (Tracker Mini: **`[Shift]`**+**`[Menu]`**). The same buttons do nothing special anywhere else. Custom play modes only play in Performance mode, which keeps running while the editor is open.
* **`[Performance]`** goes back to Performance mode, still running. On the Tracker Mini, a short press of **`[Menu]`** does the same: the module menu opens with Perform selected.
* Any other module (a module button, a Mini function key, or another choice in the Mini's module menu) leaves the editor, and Performance mode stops as it would if you left it directly (at the end of the pattern while playing).

### The Grid

Four lanes are visible at a time. Each lane uses three of the Pattern editor's four columns (the instrument column stays empty):

| Column | Shows | Example |
| :--- | :--- | :--- |
| **Step** (Note column) | The pattern step played at this position (1–128) | ` 12` |
| **FX** (Fx1 column) | The FX letter and value, as in the Pattern editor (`----` = none) | `Rd 4` |
| **Chance** (Fx2 column) | How often the FX plays | ` 60%` |

* With no `playmodes.txt`, every lane shows its built-in mode.
* A lane you have changed is marked with `*` (`C3 *`). The first change to a lane copies its built-in steps, so you always start from the default.
* The title bar spells out the cell under the cursor, for example `C3 * 12 > step 4 R - Roll 4 60%`.
* **Edit mode**: as in the Pattern editor, **`[Rec]`** turns edit mode on and off. In edit mode the cursor is red and the jog wheel, pads, Delete, Insert, Copy/Paste, Fill, Reset lane and Undo/Redo change the modes; outside it the cursor is green and nothing is changed.
* **Playhead**: while the sequencer plays, a playhead line across the lanes shows the row the cursor's lane is at: that of the first track set to its mode, or, if no track uses it, where it would be (track 1's position). Outside edit mode the cursor follows it (Up/Down and the jog wheel only move the cursor while stopped), as in the Pattern editor. A mode only plays its first *pattern length* rows: it starts again from row 1 at the end of the pattern.

### Controls

The controls follow the Pattern editor's. A lane is like a track, and the selection stays within one lane.

| Control | Action |
| :--- | :--- |
| **`[Rec]`** | Edit mode on / off. |
| **Up/Down** | Move the cursor (wrapping round). |
| **Left/Right** | Change lane. |
| **`[Shift]`+Up/Down** | Select rows. On the first row, **`[Shift]`+Up** selects the whole lane. |
| **Jog wheel** | Edit mode: change the column on the selection, or the cursor's step. In the FX column it changes the value. Otherwise (stopped): move the cursor up to 10 rows. |
| **`[Note]` / `[Fx1]` / `[Fx2]`** | The Step / FX / Chance column (`[Instr]` does nothing). On the Original, `[Fx1]` and `[Fx2]` are the buttons the Pattern editor uses for its two FX columns (`[Vol]` / `[Fx]` on older labels). |
| **`[Fx1]`** (hold) | The Pattern editor's FX list: choose with the jog wheel, arrows or pads, and release. In edit mode, with no selection of several rows, it sets that FX on the cursor's step. FX that play modes can't use (section 2) show a message instead. |
| **Pads** | Edit mode, on the selection or the cursor's step: Step column = the pad's number (1–48); FX column = the value, spread over its range (with **`[Fx1]`** held: the FX on that pad of the FX list, as in the Pattern editor); Chance = spread over 0–100%. With no selection the cursor then moves by the **Step jump**. |
| **`[Delete]`** | Edit mode: clear the column on the selection or the cursor's step (Step: back to its own number; FX: no FX; Chance: 100%), then move by the Step jump. |
| **`[Shift]`+`[Delete]`** | Edit mode: remove the row above the cursor; the rows below move up and row 128 becomes a plain row. |
| **`[Insert]`** | Edit mode: insert a plain row (step = its own number, no FX) at the cursor; the rows below move down and row 128 drops off. |
| **`[Shift]`+`[Insert]`** | Go to row 1. |
| **`[Copy]`** | Edit mode: copy the selection, or the cursor's step (up to 128 rows). |
| **`[Shift]`+`[Copy]`** | Edit mode: paste at the cursor (past row 128 it carries on at row 1), then move below the pasted rows. Hold **`[Note]`**, **`[Fx1]`** or **`[Fx2]`** too to paste only that column. |
| **`[Play]`** | Start / stop. |
| Screen button 1, **Lane** (hold) | Choose the lane with the jog wheel, the arrows or pads 1–20. |
| Screen button 2, **Revert** | Throw away unsaved edits and reload the saved file. |
| Screen button 3, **Step jump** (hold) | How far the cursor moves after an edit (shared with the Pattern editor). |
| Screen button 4, **Fill** | Edit mode: the fill popup; see below. |
| Screen button 5, **Reset lane** | Edit mode: put the lane back to its built-in mode. |
| Screen button 6 / 7, **Undo** / **Redo** | Edit mode: undo the last edit, or redo it (one level). |
| Screen button 8, **Save** | Write `playmodes.txt` to the project folder and the Workspace. |

### Fill

**Fill** opens the Pattern editor's fill popup, for the selection (or the cursor's step) in the cursor's column: **Fill Steps**, **Fill Fx** or **Fill Chance**. **`[Note]`** / **`[Fx1]`** / **`[Fx2]`** switch between them. Screen buttons 1–6 (or Left/Right) pick a setting, and the jog wheel, Up/Down or the pads change it; pressing a list's button again steps through it. Screen button 7 is **Cancel** and 8 is **Fill**.

| Setting | Choices |
| :--- | :--- |
| **Where?** | **Fx** / **No Fx**: rows with / without an FX. **Random** (with a density), **Each** (every *n*th row), **Euclidean** (*n* events spread over the rows). |
| **Fx Type** | Fill Fx only: the FX written, from the FX allowed in play modes. |
| **Fill Type** | **Constant**: one value. **From-To**: from one value to the other across the rows. **Random**: any value between the two. |
| **From / To** | Steps 1–128, the FX value, or the chance (0–100%). |

Fill Steps has six more Fill Types, sequences that count up or down by one, wrapping from 128 to 1 and from 1 to 128. For these, the third column of the popup is **Count**: **Filled rows** moves the sequence on at each row it fills (with **Where?** set to Each 2: 5 · 6 · 7), **All rows** at every row, filled or not (5 · 7 · 9).

| Fill Type | Steps written | Example (4 rows) |
| :--- | :--- | :--- |
| **Up from** | From *n* up | From 5: 5 6 7 8 |
| **Up to** | Up, ending at To *n* | To 5: 2 3 4 5 |
| **Down from** | From *n* down | From 5: 5 4 3 2 |
| **Down to** | Down, ending at To *n* | To 5: 8 7 6 5 |
| **Loop up** | From *n* up, starting again after **Loop** *m* steps | From 5, Loop 2: 5 6 5 6 |
| **Loop down** | From *n* down, starting again after **Loop** *m* steps | From 5, Loop 2: 5 4 5 4 |

Fill Fx keeps a row's chance if it had an FX (otherwise 100%). Fill Chance leaves rows without an FX alone.

### Notes

* Unsaved edits are lost when another project is opened. They also stay in effect until then, even after leaving the editor.
* Undo keeps one edit, on one lane. Revert can't be undone.
* On the Tracker Mini the controls are the same; its **`[+1]`/`[-1]`/`[+10]`/`[-10]`** buttons act as the jog wheel.
* **Save** only writes the lanes you changed, each as its shortest repeating loop.
* Up to **64 different FX settings** (FX + value + chance) can be used across all 20 modes. Identical settings are shared, and settings no step uses any more are freed automatically. A message appears when the limit is reached.

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
* **Memory**: one `__NOINIT(EXTERNAL_RAM)` block of about 5.1 KB. Room for it is reserved in `USER_PLAY_MODES_MEMORY_SIZE` (6 KB) in [`core/source/mtStructs.h`](../core/source/mtStructs.h). See [Memory Map](memory_map.md).
