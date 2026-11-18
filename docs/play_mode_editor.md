# Play Mode Editor (Prototype)

The Play Mode Editor edits Performance Mode's twenty custom play modes, `C1`–`C20`, on the Tracker itself. Each mode is a lane of 128 rows: the step each row plays, and an optional FX with its chance. Performance Mode keeps running while you edit, so you hear every change the next time that step plays. **Save** writes the modes to the project's [`playmodes.txt`](user_play_modes.md), the same file you can also write by hand.

The editor works like the Pattern editor: the same edit mode, columns, selection, copy and paste, fill popup and FX list.

> [!NOTE]
> The editor is a **prototype**. It is a build option of this firmware (see [Building With or Without the Editor](#13-building-with-or-without-the-editor)), and it has been tested on the **Original Tracker**. Tracker+ and Tracker Mini builds include it but have not been tested on hardware yet.

---

## Contents

1. [Quick Start](#1-quick-start)
2. [Opening and Leaving the Editor](#2-opening-and-leaving-the-editor)
3. [The Screen](#3-the-screen)
4. [Edit Mode, Moving and Playback](#4-edit-mode-moving-and-playback)
5. [Editing Steps, FX and Chance](#5-editing-steps-fx-and-chance)
6. [Selecting, Copying and Moving Rows](#6-selecting-copying-and-moving-rows)
7. [Fill](#7-fill)
8. [Undo, Revert, Reset Lane and Save](#8-undo-revert-reset-lane-and-save)
9. [Tracker Mini](#9-tracker-mini)
10. [Limits and Tips](#10-limits-and-tips)
11. [Messages](#11-messages)
12. [Control Reference](#12-control-reference)
13. [Building With or Without the Editor](#13-building-with-or-without-the-editor)

---

## 1. Quick Start

1. Open a project, go to **Performance Mode**, and set a track's **Play Mode** FX to a custom mode, for example `C1`. Press **`[Play]`**.
2. Hold **`[Shift]`** and press **`[Pattern]`** (Tracker Mini: **`[Shift]`**+**`[Menu]`**). The editor opens on lane `C1`, with the cursor following the playhead.
3. Press **`[Rec]`**: the cursor turns red. You are in edit mode.
4. Press a pad: the row under the cursor now plays that step (pad 1 = step 1, pad 48 = step 48), and the cursor moves on by the **Step jump**.
5. Press **`[Fx1]`** to go to the FX column, hold it, pick an FX from the list and release it. Turn the jog wheel to change its value. **`[Fx2]`** goes to its chance.
6. Press screen button 8, **Save**. **`[Performance]`** takes you back to Performance Mode.

---

## 2. Opening and Leaving the Editor

* **Open**: in Performance Mode, hold **`[Shift]`** and press **`[Pattern]`** (Tracker Mini: **`[Shift]`**+**`[Menu]`**). These buttons do nothing special anywhere else. The editor opens from Performance Mode only, because custom play modes only play there.
* **Back to Performance Mode**: press **`[Performance]`**. Performance Mode is still running as you left it. On the Tracker Mini, a short press of **`[Menu]`** does the same: the module menu opens with Perform selected.
* **Another module**: any other module button (or a Mini function key, or another choice in the Mini's module menu) leaves the editor. Performance Mode then stops as it would if you left it directly: at the end of the pattern if the sequencer is playing.

Edits stay in effect after you leave the editor, saved or not, until another project is opened. See [Save](#save).

---

## 3. The Screen

The screen looks like the Pattern editor in its four-track view: the title bar at the top, four lanes in the middle, and the eight screen-button labels at the bottom.

### Lanes and Columns

Each lane is one custom mode. Four lanes are visible at a time; moving to another lane scrolls the view. A lane uses three of the Pattern editor's four columns (the instrument column stays empty):

| Column | Button | Shows | Example |
| :--- | :--- | :--- | :--- |
| **Step** | **`[Note]`** | The pattern step this row plays (1–128) | ` 12` |
| **FX** | **`[Fx1]`** | The FX letter and value, as in the Pattern editor; `----` = none | `Rd 4` |
| **Chance** | **`[Fx2]`** | How often the FX plays; `---` when the row has no FX | ` 60%` |

* A lane you haven't changed shows its **built-in** mode (or the mode from the project's `playmodes.txt`).
* A lane you have changed is marked with **`*`** (`C3 *`). The first change to a lane copies its built-in steps, so you always start from what you heard.
* The **title bar** spells out the cell under the cursor: the lane, the row, the step it plays, and the FX with its value and chance, for example `C3 * 12 > step 4 R - Roll 4 60%`.
* Rows wrap around: below row 128 comes row 1, as the mode loops.

### Screen Buttons

Like the Pattern editor's first page. The edit actions (Fill, Reset lane, Undo, Redo) are greyed out outside edit mode, and Undo and Redo also when there is nothing to undo or redo.

| Button | Label | Action |
| :---: | :--- | :--- |
| 1 | **Lane** `C1` | Hold: choose the lane (jog wheel, arrows, or pads 1–20). |
| 2 | **Revert** | Throw away unsaved edits and reload the saved file. |
| 3 | **Step jump** `1` | Hold: how far the cursor moves after an edit (jog wheel, arrows, or pads). |
| 4 | **Fill** | Edit mode: the [fill popup](#7-fill). |
| 5 | **Reset lane** | Edit mode: the lane back to its built-in mode. |
| 6 | **Undo** | Edit mode: undo the last edit. |
| 7 | **Redo** | Edit mode: redo it. |
| 8 | **Save** | Write `playmodes.txt`. |

While **Lane** or **Step jump** is held, its label is framed and the cursor is hidden.

---

## 4. Edit Mode, Moving and Playback

### Edit Mode

As in the Pattern editor, **`[Rec]`** turns edit mode on and off.

* **Edit mode on** (red cursor): the jog wheel, pads, `[Delete]`, `[Insert]`, copy and paste, Fill, Reset lane and Undo/Redo change the modes.
* **Edit mode off** (green cursor): nothing is changed. While the sequencer plays, the cursor follows the playhead.

### Moving

| Control | Action |
| :--- | :--- |
| **Up / Down** | Move one row (wrapping round). While playing, only in edit mode. |
| **Left / Right** | Previous / next lane. |
| **Jog wheel** (edit mode off, stopped) | Move up to 10 rows a turn. |
| **`[Shift]`+`[Insert]`** | Go to row 1. |
| **Lane** (hold) + pad 1–20 | Jump to lane `C1`–`C20`. |
| **`[Note]` / `[Fx1]` / `[Fx2]`** | Go to the Step / FX / Chance column. `[Instr]` does nothing. |

### The Playhead

While the sequencer plays, a playhead line across the lanes shows the row the cursor's lane is playing: that of the first track set to its mode or, if no track uses it, the row it would be at (track 1's position). Outside edit mode the cursor follows it, as in the Pattern editor.

A mode only reaches its first *pattern length* rows: at the end of the pattern it starts again from row 1. With a 32-step pattern, rows 33–128 are never heard.

**`[Play]`** starts and stops the sequencer, as in Performance Mode.

---

## 5. Editing Steps, FX and Chance

All edits need edit mode. Each one applies to the [selection](#6-selecting-copying-and-moving-rows) if there is one, otherwise to the row under the cursor. After a pad or `[Delete]` on a single row, the cursor moves down by the **Step jump** (screen button 3, shared with the Pattern editor; 0 keeps it in place).

### Step Column (`[Note]`)

The pattern step the row plays, 1–128.

* **Pads**: pad 1–48 sets step 1–48.
* **Jog wheel**: up or down by one (Shift: faster, as everywhere on the Tracker).
* **`[Delete]`**: back to the row's own number (row 5 plays step 5).

Steps beyond the track's length wrap around, as with the built-in modes: on a 16-step pattern, step 20 plays step 4.

### FX Column (`[Fx1]`)

The FX played with the step, and its value.

* **Choose the FX**: hold **`[Fx1]`**. After a moment the Pattern editor's FX list opens; choose with the jog wheel, the arrows or the pads, and release `[Fx1]`. The FX is set on the cursor's row, with its default value. Nothing is set while several rows are selected: use `[Fx1]`+pad or Fill for those.
* **Or with a pad**: hold **`[Fx1]`** and press a pad: the FX on that pad of the FX list, without opening the list. This works on a selection too.
* **Value**: the jog wheel changes it (the FX's own range, as in the Pattern editor). The pads spread it over its range: pad 1 = lowest, pad 48 = highest. Rows without an FX are left alone.
* **`[Delete]`**: removes the FX (and its chance).

Some FX can't be used in a play mode: **Tempo**, **Swing**, **Break pattern**, **Off**, **Random FX value** and **MIDI Chord**. Choosing one shows a message instead.

### Chance Column (`[Fx2]`)

How often the row's FX plays, 0–100%. Only rows with an FX have a chance.

* **Jog wheel**: up or down by 1%.
* **Pads**: spread over 0–100% (pad 1 = 0%, pad 48 = 100%).
* **`[Delete]`**: back to 100%.

### What an FX Does When It Plays

When a row with an FX plays, the FX goes into the step's **first empty FX slot** in the pattern. If both of the step's FX slots are already used, the pattern's own FX win and the row's FX is skipped. Most FX need a note on the step to be heard.

---

## 6. Selecting, Copying and Moving Rows

### Selecting

* **`[Shift]`+Up/Down**: select rows, from the cursor. The selection stays within one lane.
* **`[Shift]`+Up on row 1**: select the whole lane.
* Moving without `[Shift]` clears the selection.

### Copy and Paste (Edit Mode)

* **`[Copy]`**: copy the selection, or the cursor's row (up to 128 rows, from one lane). A message says how many.
* **`[Shift]`+`[Copy]`**: paste at the cursor; past row 128 it carries on at row 1. The cursor then moves below the pasted rows.
* **`[Shift]`+`[Copy]` while holding `[Note]`, `[Fx1]` or `[Fx2]`**: paste only that column (Step; FX with its value and chance; or Chance).

The clipboard is kept while the Tracker is on, so you can copy from one lane and paste into another.

### Insert and Remove Rows (Edit Mode)

* **`[Insert]`**: insert a plain row at the cursor (it plays its own step number, with no FX). The rows below move down and row 128 drops off the end.
* **`[Shift]`+`[Delete]`**: remove the row above the cursor. The rows below move up and row 128 becomes a plain row.

---

## 7. Fill

**Fill** (screen button 4, edit mode) opens the Pattern editor's fill popup. It fills the selection, or the cursor's row, in the column the cursor is in:

| Column | Popup |
| :--- | :--- |
| Step (`[Note]`) | **Fill Steps** |
| FX (`[Fx1]`) | **Fill Fx**: the FX and its value |
| Chance (`[Fx2]`) | **Fill Chance** |

**`[Note]`**, **`[Fx1]`** and **`[Fx2]`** switch between the three while the popup is open. Each keeps its own settings, also after the popup closes.

### Using the Popup

* Screen buttons 1–6 (or Left/Right) pick a setting; settings that don't apply are skipped.
* The jog wheel, Up/Down or the pads change it. Pressing a list's button again steps through the list.
* Screen button 7 is **Cancel**, screen button 8 is **Fill**.

### Settings

| Button | Setting | Choices |
| :---: | :--- | :--- |
| 1 | **Where?** | Which rows are filled: **Fx** / **No Fx** (rows with / without an FX), **Random** (with a density), **Each** (every *n*th row), **Euclidean** (*n* events spread over the rows). |
| 2 | **Density** / **Step** / **Events** | The amount for Random (%), Each (*n*) or Euclidean (*n*). |
| 3 | **Fx Type** (Fill Fx) | The FX written, from the FX allowed in play modes. |
| 3 | **Count** (Fill Steps sequences) | See [Sequences](#sequences-fill-steps). |
| 4 | **Fill Type** | How the values are chosen; see below. |
| 5 | **From** | The first value (Constant: the value). |
| 6 | **To** | The last value (**Loop** for the loop fills). |

**Fill Types** for all three popups:

| Fill Type | Values written |
| :--- | :--- |
| **Constant** | One value, *From*, on every filled row. |
| **From-To** | From *From* to *To*, evenly across the rows. |
| **Random** | Any value between *From* and *To* on each row. |

* **Fill Steps** writes steps 1–128. **Fill Fx** writes the FX type and a value in its range, and keeps a row's chance if it had an FX (otherwise 100%). **Fill Chance** writes 0–100% and leaves rows without an FX alone.

### Sequences (Fill Steps)

Fill Steps has six more Fill Types: sequences that count up or down by one, wrapping from 128 to 1 and from 1 to 128.

| Fill Type | Steps written | Example on 4 rows |
| :--- | :--- | :--- |
| **Up from** | From *From* up | From 5: `5 6 7 8` |
| **Up to** | Up, ending at *To* on the last row | To 5: `2 3 4 5` |
| **Down from** | From *From* down | From 5: `5 4 3 2` |
| **Down to** | Down, ending at *To* on the last row | To 5: `8 7 6 5` |
| **Loop up** | From *From* up, starting again after **Loop** steps | From 5, Loop 2: `5 6 5 6` |
| **Loop down** | From *From* down, starting again after **Loop** steps | From 5, Loop 2: `5 4 5 4` |

**Count** (screen button 3) decides how the sequence moves on when **Where?** skips rows:

| Count | With Where? = Each 2, Up from 5 | |
| :--- | :--- | :--- |
| **Filled rows** (default) | `5 · 6 · 7 · 8` | The sequence moves on only at the rows it fills. |
| **All rows** | `5 · 7 · 9 · 11` | It moves on at every row, so each step matches its row's position. |

---

## 8. Undo, Revert, Reset Lane and Save

### Undo and Redo

**Undo** (screen button 6) takes back the last edit; **Redo** (screen button 7) puts it back. There is one level: a new edit replaces what Undo would restore. One edit is one pad press, `[Delete]`, paste, fill, insert, reset and so on; turning the jog wheel on the same cells counts as one edit until you edit something else. Undo moves the cursor to the lane it changes.

### Revert

**Revert** (screen button 2) throws away every unsaved edit and reloads the last saved `playmodes.txt`. It can't be undone.

### Reset Lane

**Reset lane** (screen button 5, edit mode) puts the cursor's lane back to its built-in mode. Undo brings the lane back; Save makes the reset permanent.

### Save

**Save** (screen button 8) writes `playmodes.txt` into the project folder and the Workspace:

* Only the lanes you changed are written, each as its shortest repeating loop.
* A lane back to its built-in mode is left out. If no lane is changed, the file is removed.
* If the project has never been saved, the file goes to the Workspace only (`Play modes saved to the Workspace.`): save the project (**Save As**) to keep it.

The project's own **Save** doesn't write `playmodes.txt`, so **save in the editor** before you leave the project. Unsaved edits stay in effect until another project is opened, then they are lost.

You can still edit `playmodes.txt` on a computer: see [User-Defined Play Modes](user_play_modes.md). Open the project again on the Tracker to load your changes.

---

## 9. Tracker Mini

The controls are the same, with these differences:

* **Open** with **`[Shift]`**+**`[Menu]`** in Performance Mode (`[Shift]`+`[Pattern]` isn't used, because a Mini function key can stand for `[Pattern]`).
* **Leave**: a short press of **`[Menu]`** goes back to Performance Mode; the module menu takes you anywhere else.
* The **`[+1]` / `[-1]` / `[+10]` / `[-10]`** buttons act as the jog wheel.

---

## 10. Limits and Tips

* **64 FX settings**: an FX setting is an FX type, value and chance. Up to 64 different settings can be used across all 20 modes. Rows with the same setting share it, and settings no row uses any more are freed automatically. A message appears when the limit is reached; the edit isn't made.
* **Pattern length**: a mode only plays its first *pattern length* rows (see [The Playhead](#the-playhead)). To make a 16-step pattern use a 32-step order, use steps above 16 on rows 1–16; they wrap around.
* **Other firmware**: this firmware reads up to 64 FX settings, with or without the editor built in. Firmware that has [User-Defined Play Modes](user_play_modes.md) without the editor's changes reads up to **16**: if the editor saved more than 16, that firmware plays the modes using settings above 16 as built-in. Firmware without play modes at all ignores the file.
* **Sound check while editing**: keep edit mode off to have the cursor follow the playhead, and turn it on to change the row you hear.
* **Start from a built-in mode**: edit a lane you like; the first change copies its built-in steps. Reset lane gets the original back.

---

## 11. Messages

| Message | Meaning |
| :--- | :--- |
| `Copied 4 steps.` | The selection is on the clipboard. |
| `Nothing copied yet.` | Paste with an empty clipboard. |
| `<FX> can't be used in play modes.` | That FX isn't allowed (see [FX Column](#fx-column-fx1)). |
| `No FX on this pad.` | `[Fx1]`+pad on a pad with no FX in the FX list. |
| `The FX list is full.` / `At most 64 different FX settings.` | See [Limits](#10-limits-and-tips). |
| `Play modes saved.` | Saved to the project and the Workspace. |
| `Play modes saved to the Workspace.` / `Save the project to keep them.` | The project has never been saved. |
| `Could not save playmodes.txt.` | The SD card couldn't be written. |
| `Back to the saved play modes.` | Revert done. |
| `Lane back to its built-in mode.` / `Save to keep the change.` | Reset lane done. |

If the saved file has errors when it is loaded (for example after editing it on a computer), the usual **Custom Play Modes (playmodes.txt)** message appears; see [User-Defined Play Modes, Errors](user_play_modes.md#4-errors).

---

## 12. Control Reference

| Control | Edit mode off | Edit mode on |
| :--- | :--- | :--- |
| **`[Rec]`** | Edit mode on | Edit mode off |
| **`[Play]`** | Start / stop | Start / stop |
| **Up / Down** | Move (stopped only) | Move |
| **`[Shift]`+Up / Down** | Select rows (stopped only); Up on row 1: whole lane | Select rows; Up on row 1: whole lane |
| **Left / Right** | Lane | Lane |
| **Jog wheel** | Move up to 10 rows (stopped only) | Change the column's value |
| **Pads** | — | Set the column (Step: pad number; FX: value; Chance: %) |
| **`[Note]` / `[Fx1]` / `[Fx2]`** | Column: Step / FX / Chance | Column: Step / FX / Chance |
| **`[Fx1]`** (hold) | FX list (nothing set) | FX list: sets the FX on release |
| **`[Fx1]`** (hold) + pad | — | Set that pad's FX |
| **`[Delete]`** | — | Clear the column, then Step jump |
| **`[Shift]`+`[Delete]`** | — | Remove the row above the cursor |
| **`[Insert]`** | — | Insert a plain row |
| **`[Shift]`+`[Insert]`** | Go to row 1 | Go to row 1 |
| **`[Copy]`** | — | Copy |
| **`[Shift]`+`[Copy]`** | — | Paste (hold a column button: that column only) |
| **Screen button 1** (hold) | Lane: jog wheel, arrows, pads 1–20 | Same |
| **Screen button 2** | Revert | Revert |
| **Screen button 3** (hold) | Step jump: jog wheel, arrows, pads | Same |
| **Screen button 4** | — | Fill |
| **Screen button 5** | — | Reset lane |
| **Screen buttons 6 / 7** | — | Undo / Redo |
| **Screen button 8** | Save | Save |
| **`[Performance]`** | Back to Performance Mode | Back to Performance Mode |

---

## 13. Building With or Without the Editor

The editor is built by default. To leave it out:

```bash
make original PLAY_MODE_EDITOR=0    # or: make plus / make mini / make all
```

Without the editor, play modes still load from `playmodes.txt`. On the Original, the editor costs about **15 KB of flash** and **2 KB of external RAM** (its clipboard and undo buffer live in the play modes' reservation, which grows from 6 KB to 7 KB; on Tracker+ and Tracker Mini that 1 KB comes out of sample memory). Changing the option rebuilds everything once.

**Code**: [`core/interface/playModeEditor/`](../core/interface/playModeEditor/). The editing functions it uses (`userPlayModes::setStepNumber()`, `setStepFx()`, `save()` …) are in [`core/source/userPlayModes.cpp`](../core/source/userPlayModes.cpp), and the file writer (`writeFile()`) in [`core/source/userPlayModesParser.cpp`](../core/source/userPlayModesParser.cpp). See also [Memory Map](memory_map.md).
