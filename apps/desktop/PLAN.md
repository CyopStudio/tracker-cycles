# Tracker Cycles Desktop: plan

A standalone PC app for writing Tidal with the Tracker Cycles structured editor.
You use the keyboard, it sends real Tidal (GHCi, Tidal 1.10.3), and SuperDirt
makes the sound. Status: plan (2026-11-23), branch `feature/desktop-app`.

## Status (2026-11-23)

- **M0 done:**
  - `make -C tidal lib` builds the shared code, which now lives in
    `tidal/host/`.
  - `apps/desktop` builds `tidal-desktop` (`make`, `make test`, `run.sh`).
  - It finds SuperDirt's address itself: in WSL NAT mode that's the Windows
    host (`wslinfo` + the default route).
  - It writes `boot.ghci` from `tidal/proto/boot.ghci` with that address
    and starts GHCi with it.
  - GHCi boots and gets the evaluated blocks. Not heard yet, because
    SuperDirt wasn't running.
  - Still to check: that WSLg passes Alt and F10–F12 through (the window
    opens).
- **M1 mostly done:**
  - `keymaps/iso-uk.txt` is compiled in as the default, and `--keymap
    FILE` loads another.
  - Working: the F-row, the pads on the four rows, the cluster, Alt held as
    Pattern, Ctrl+letter, Shift passing through, and `--chord-ms`.
  - The panel shows each control's key and each pad's key in its corner.
  - `tests/*.keys` cover the keymap and the chord window.
  - Left: trying the key feel on your keyboard.

## Goals

1. **The same editor as the device.** It uses the `Editor` model from
   `tidal/editor` unchanged. The code is valid at every step, there are no
   holes, and menus are built by trial edit.
2. **Muscle memory that carries over.** Every Tracker control has one home
   key. That key is either in the same place as the control on the Tracker or
   has the same name (Insert, Delete, Home, Backspace, Copy, Paste, Shift).
   Shift means Shift on both devices.
3. **Things only a keyboard can do are extras.** These are typing values,
   filtering menus and typing comments. Each one is a faster way to do an edit
   the Tracker controls can already do. The device never needs one of them.
4. **Real Tidal.** The app is not limited to the engine's subset. Any function
   with a type signature in the library can be used.

Not goals: audio of its own, running without GHC, or file formats other
than `.tidal`.

## Keyboard mapping

### The idea

The Tracker's front panel has three groups, and the keyboard has the same
three.

| Tracker | Keyboard | Why |
| :--- | :--- | :--- |
| 8 screen buttons under the screen | **F1–F8** | a row of 8 soft keys; the soft-key bar labels them F1 … F8 |
| Note / Instr / FX1 / FX2 (right of the screen buttons) | **F9–F12** | the next four keys in the same row, in the same order |
| 48 pads, 12 × 4 | **the 4 character rows**, 12 keys each | pad (row, column) = key (row, column) |
| Arrow cluster with Insert and Delete beside Up | **arrows, Insert, Delete, Home** | the same names; the device prints Home and Backspace as Shift labels, and the PC has those keys |
| Jog wheel | **mouse wheel, PgUp / PgDn** | PgUp / PgDn sit above the arrows, as the jog sits by the arrow cluster |
| Shift | **Shift** | the same modifier, with the same meaning everywhere |
| Pattern (held) | **Alt (held)** | a modifier you hold with one hand while the other presses pads |
| Play | **Space** | the play key in every DAW and tracker; Ctrl+Enter also works (Tidal editor habit) |
| Other named buttons (File, Config, Song, Master, Smp Load, Rec, …) | **Ctrl + the button's letter** | one rule: Ctrl+letter presses the named button, and adding Shift gives its Shift function |

The rule to learn is: **Fn row = buttons, letter rows = pads, cluster = cluster,
Ctrl+letter = named button.** Once you know the device, a key's position or
name tells you which control it is, and the other way round.

### Pads on the letter rows

Keys are read by **scancode** (physical position), so the grid stays the same
on any layout. The on-screen pad grid shows each pad's key cap (from
`SDL_GetKeyFromScancode`), so the screen always shows the mapping.

```
pad  1-12   1 2 3 4 5 6 7 8 9 0 - =          number row
pad 13-24   Q W E R T Y U I O P [ ]          top letter row
pad 25-36   A S D F G H J K L ; ' #          home row (ISO #; ANSI: \ )
pad 37-47   \ Z X C V B N M , . /            bottom row (ISO \ left of Z)
pad 48      `  (the key left of 1)           ~ rest / instrument 48
```

- **Numbers and notes** keep the device's layout: −12 is top left and 0 is at
  the start of the second row. So `Q` is 0, `W` 1, `E` 2 and so on, and for
  `n` / `note` that is a chromatic keyboard from c5 (Tidal 0) on `Q`. On
  both devices, `Q T I` together is a major triad.
- **The rest `~`** is pad 48. The bottom row has only 11 keys, so it moves to
  the backtick key, which carries `~` on US keyboards. On ANSI keyboards
  there's no key left of Z, so `Z` … `/` are pads 37–46 and pad 47 has no key
  (number 34 or instrument 47: use the jog or the Enter field). The ANSI `\`
  above Enter is pad 36. The keymap file decides this, and the default
  follows the layout.
- **Slots**: Shift + pad works as on the device. `Shift+1`…`8` selects d1–d8
  and `Shift+Q`…`I` selects d9–d16.
- **Instruments**: in an `s` pattern, pad N is instrument slot N. On the PC
  the slots are a kit of Dirt-Samples folder names (see Samples).
- **Chords**: hold the keys, as you hold the pads. Fast typists roll over
  (they press the next key before releasing the last), so a chord only forms
  when the keys go down within the **chord window**. The window is set in
  Settings, from 0 to 150 ms in 5 ms steps (default 40 ms; 0 = overlapping
  keys never make a chord). Rolled-over keys outside the window are separate
  steps. Shift+F5 (`,`) still adds a layer by hand. Most
  keyboards register 6 keys at once, enough for any triad or 7th.
- OS key repeat is ignored on pads. Arrows and jog keys repeat.

### Function keys

| Key | Tracker control | | Key | Tracker control |
| :--- | :--- | --- | :--- | :--- |
| F1 … F8 | Screen 1 … 8 (`$ # [] <> {} () \| \|+`) | | Shift+F1 … F8 | Shift+Screen (`* / ! @ , "" ?.. undo`) |
| F9 | Note | | Shift+F9 | chord menu |
| F10 | Instrument | | Shift+F10 | (Shift+Instr) |
| F11 | FX1 (functions) | | Shift+F11 | (Shift+FX1) |
| F12 | FX2 (effects) | | Shift+F12 | (Shift+FX2) |

In a menu, F1 is Cancel and F8 is OK, as on the device. Esc (just left of F1)
and Enter are aliases for them.

Laptops: F-keys need Fn-lock, and the app says so the first time F-keys are
missing. If a keyboard has no usable F-row, the mapping editor can put Screen 1–8 on
other keys.

### Cluster, modifiers and named buttons

| Key | Tracker control | Notes |
| :--- | :--- | :--- |
| ← → ↑ ↓ | Left / Right / Up / Down | Shift+arrows as on the device (block jumps, step selection) |
| Insert | Insert | |
| Home | Shift+Insert (Home) | the device prints "Home" as Insert's Shift label |
| Delete | Delete | |
| Backspace | Shift+Delete (Backspace) | the device's Shift label |
| PgUp / PgDn, wheel | Jog | Shift = the coarse step (×10, octave, sample `:n`) |
| Space | Play (evaluate) | Shift+Space = silence the block; Ctrl+Enter = evaluate too |
| Alt (held) | Pattern (held) | Alt+pad = mute dN, Shift+Alt+pad = solo, Alt+F8 = unmute all, Alt+Delete = cancel |
| Shift+Alt (tapped) | Shift+Pattern (hush) | Ctrl+. = hush too (Tidal editor habit) |
| Ctrl+C / Ctrl+Shift+C | Copy / Shift+Copy (Paste) | Ctrl+V = Paste too |
| Ctrl+Z | Shift+Screen 8 (undo) | |
| Ctrl+S | File (save) | |
| Ctrl+L / Ctrl+Shift+L | Song (launch menu) / Shift+Song (setcps) | |
| Ctrl+R | Rec (cursor ↔ tree mode) | |
| Ctrl+T / Ctrl+Shift+T | Config (Tracker mode) / Shift+Config (comment) | Ctrl+/ = comment too |
| Ctrl+M / Ctrl+Shift+M | Master / Shift+Master (status) | |
| Ctrl+O / Ctrl+Shift+O | Smp Load (sample kit) / Shift+Smp Load (sample folders) | |
| Ctrl+P | Params | reserved, as on the device |
| Ctrl+E / Ctrl+Shift+E | Perform / Shift+Perform | open / close the performance display / move it to the next display (desktop only; Perform is free on the device) |
| Tab / Shift+Tab | next / previous slot | there's no device control for this |
| Ctrl+, | Settings (desktop only) | includes the mapping editor |
| Ctrl+Q | quit | |

Alt and the Windows menu bar: the app consumes Alt, so a lone Alt tap never
opens a system menu. AltGr (right Alt on UK and EU layouts) is not Pattern.

### What changes for the prototype

The prototype's current keys (`n`, `s`, `f`, `c`, `e`, `+`/`-`, digits as
slot pads …) put shortcuts on the letters, which the pad grid needs. **The
prototype stays as it is until the desktop layout is settled**, because the
layout may still change. After that it loads the same default keymap file,
so there's only one PC mapping. (Its panel hints already disagree with its
keys about Space, Play and Pattern; that gets fixed in the same step.) The
headless `--keys` tokens stay as they are, so the smoke sessions keep
working.

### Keyboard-only extras (all additive)

1. **Menu type-ahead.** While a menu is open, letter keys filter it (`fas` →
   `fast`, `fastGap`). Pads do nothing in a menu on the device, so nothing
   conflicts. Arrows, jog, F1 and F8 still work.
2. **Type a value (Enter).** Outside a menu, Enter opens a small text field
   on the token at the cursor, filled with its current text. You type
   `0 3 5 7`, `bd*2 [sn cp]`, `fast 2` or `0.25`, then Enter splices it in. The
   text goes through `parseMini` / `parseExpr` (tidal/core/parse.h) for the
   node's type and is type-checked like any trial edit. If it doesn't fit, it
   stays in the field with the error. That keeps the code valid at every step.
   Numpad digits open the field with that digit already typed.
3. **Comments typed directly.** Ctrl+Shift+T opens the comment as a plain
   text field in place of the 42-pad naming keyboard. The device's pad
   keyboard is still reachable through `hw:` tokens for tests.

## Performance display

This is a second window for the audience. It shows only the code, its
highlights and the cursor. There are no menus, soft-key bar, info box,
status line, panel or error pop-ups.

- **Opening it.** **Ctrl+E (Perform)** opens it fullscreen on the display
  chosen in Settings, and pressing it again closes it. The Tracker's Perform
  button is free, and the name fits. **Ctrl+Shift+E** moves it to the next
  display, for when the projector shows up as a different display than
  expected. Keyboard focus stays in the editor window the whole time, so
  you keep playing while it opens. It uses SDL2's borderless fullscreen
  (`SDL_WINDOW_FULLSCREEN_DESKTOP` on `SDL_WINDOWPOS_CENTERED_DISPLAY(n)`),
  so the resolution and the other screens don't change.
- **What it shows.**
  - All of the file's code, laid out by the same `layout.cpp` as the
    editor, with its own font size.
  - Highlights, the cursor (purple or blue, as in the editor), and the
    selection.
  - Hidden: menu overlays, the cursor's info and error text, and Settings or
    the mapping editor when they are open.
  - Optional: muted blocks dimmed. The Enter field's text shown inline while
    you type, so the audience sees the code being written.
- **Fitting the code.** The font size is fixed (one setting, never scaled
  to fit). Long lines wrap at the window width, and the view scrolls with
  the cursor, keeping the cursor's line in view.
- **Look.** Its own theme (background, text, highlight colours), large
  type, and no screen furniture. The background can be transparent for
  Spout (below).
- **Drawing.** It is redrawn only when its content changes (an edit, the
  cursor, a highlight frame), the way the device's code view redraws only
  the controls whose fingerprint changed. There's no extra work while
  nothing changes.

### Spout

Spout output is possible. Spout2 (Windows only) shares a GPU texture with
VJ software (Resolume, TouchDesigner, OBS with the Spout plugin …).

- The performance view is drawn into its own frame at a set size (default
  1920×1080), independent of any window. A Spout sender publishes that
  frame (`SpoutSender::SendImage` takes CPU pixels in RGBA). So Spout works
  **with or without** the fullscreen window open, and the size isn't tied to
  a monitor.
- Settings: Spout on / off, sender name (default "Tracker Cycles"),
  resolution, transparent background (alpha 0 behind the text, for keying
  over visuals).
- Licence: the Spout2 SDK is BSD-2-Clause, which is compatible with
  GPL-3.0. Check the version we vendor or link when we add it.
- Spout is **Windows build only**: it can't share textures out of WSL. In
  WSL the performance window still works. To check: WSLg's multi-monitor
  support. Linux (v4l2loopback) and macOS (Syphon) are possible later but
  not planned. NDI is left out because its SDK licence doesn't fit a GPL
  app.

## Keymaps are data

The mapping is a text file, not code, so it can change at any time: in the
mapping editor, or by editing the file by hand. The default (`keymaps/iso-uk.txt`)
follows this document. `ansi-us.txt` comes later if anyone needs it.

```
# key (SDL scancode name, + modifiers)   control (ctlId from editor/controls.cpp, or a desktop action)
F1                          s1
Q                           pad13
Grave                       pad48
Insert                      insert
Home                        shift insert      # the device's Shift label
Backspace                   shift delete
Space                       play
ctrl Return                 play
hold Alt                    pattern           # held: a modifier, like the Pattern button
ctrl L                      song
ctrl Comma                  settings          # desktop action
```

- **Keys are physical positions** (SDL scancode names), so a keymap is the
  same whichever OS layout is active. Key caps for display come from the
  OS layout.
- **Shift passes through.** `F3 → s3` also makes `Shift+F3 → Shift+s3`.
  Only exceptions need a line.
- **Controls are the tokens tests already use** (`s1`…`s8`, `pad1`…`pad48`,
  `play`, `insert`, `fx1` …: `ctlId()` in `editor/controls.cpp`). Desktop-only
  actions (`settings`, `type`, `hush`, `quit`, `nextslot` …) have their own
  names.
- **Every hint comes from the active keymap.** That covers the panel's key
  caps, the soft-key bar's `F3` labels, menu help and the status line, so a
  remapped key shows up everywhere straight away.
- A script generates the "Desktop" column of the `tidal/README.md` controls
  table from the default keymap, so the docs can't drift from it.

## Mapping editor

You open it from Settings → Keyboard → Edit mapping. It uses the same
controls as the rest of the app (arrows, jog, F8 / Enter = OK, F1 / Esc =
Cancel), so you don't need the mouse.

**Two views.** F2 switches between them.

1. **Panel view.** The mock Tracker front panel (reusing `proto/panel.cpp`)
   with every control labelled with its key(s). Pick a control with the
   arrows, or with a click, then:
   - **Enter (learn)**: "Press a key for Screen 3 …". The next key, with its
     modifiers, becomes that control's key. Esc cancels learning, and Esc
     can't be learned here.
   - **Delete**: removes that control's key.
   - **Shift+Enter**: learns a key for the control's *Shift* function only
     (an exception to Shift pass-through).
2. **Keyboard view.** A drawn UK keyboard with every key labelled with its
   control. Pick a key, then Enter opens a menu of controls (with
   type-ahead) to put on it.

**Conflicts.** If the key you press is already used, the editor says so ("Q
is Pad 13") and offers *Swap* (the other control takes this control's old
key), *Replace* (the other control is left without a key) or *Cancel*.

**Pad rows.** "Learn row" fills a whole pad row: press its 12 keys from left
to right.

**Test mode.** Press any key to see what it would do right now, without
editing the code. For example, "Q → Pad 13 → enters 0 here". This uses the
same labels as the soft-key bar (`controlCommand()`).

**Safety.** You can't save a keymap that leaves a required control without a
key. The required controls are the arrows, Shift, Play, Insert, Delete,
Screen 1 (Cancel) and Screen 8 (OK). Settings (Ctrl+,) always works.
`--keymap default` on the command line starts with the default keymap if a
mapping goes wrong.

**Files.** The default keymaps are read-only. Your edits are saved as a copy
(e.g. `iso-uk (mine)`) in the config folder. Settings picks which keymap is
active, and you can reset to the default from there too.

## Settings

Ctrl+, opens Settings. This is a desktop-only screen: on the device, Config
stays the Tracker-mode button. It works like the device's menus: ↑ ↓ to
move, the jog to change a value, Enter to edit text, F8 OK, F1 Cancel.
Changes apply immediately. For example, a new chord window takes effect on
the next key.

| Page | Settings |
| :--- | :--- |
| Keyboard | active keymap; Edit mapping …; **chord window (0–150 ms, default 40)**; jog key repeat rate |
| Tidal | GHCi command; boot file; SuperDirt address and port (in WSL, defaults to the Windows host address); Dirt-Samples folder; kit file |
| Editor | start in cursor or tree mode; Tracker mode on at start |
| View | font size; panel on / off |
| Performance | display (a list of the connected displays by name); font size; theme; dim muted blocks; show typing inline; Spout on / off, sender name, resolution, transparent background (Windows) |

Settings are stored in a plain `key = value` file, `desktop.conf`, in
SDL's per-user folder: `~/.local/share/CyopStudio/TrackerCycles/` on Linux,
or `%APPDATA%\CyopStudio\TrackerCycles\` on Windows. Keymaps go next to it
in `keymaps/`, and so does the generated `boot.ghci`.

## Architecture

```
apps/desktop/
  PLAN.md            this file
  Makefile           builds tidal-desktop (SDL2), links the shared library
  main.cpp           arguments, config, main loop
  keymap.{h,cpp}     load / save keymap files; scancode -> Control (+ shift /
                     held); chord window; Alt = Pattern hold
  keymaps/           iso-uk.txt (default); ansi-us.txt later if needed
  settings.{h,cpp}   desktop.conf, the Settings screen
  map_editor.cpp     the mapping editor (panel view, keyboard view, learn)
  text_entry.cpp     Enter field, menu type-ahead, comment field
  view_desktop.cpp   resizable view: layout.cpp + scalable font
  samples.cpp        Dirt-Samples scan -> 48-slot kit + folder menu
  perf_view.cpp      performance display: code-only render, own window / frame
  spout_out.cpp      Spout2 sender (Windows build only)
tidal/host/          (moved out of proto/: shared by proto and desktop)
  backend.*, output.*, engine_backend.*   GHCi / engine backends
```

- **Shared code.** `tidal/core` and `tidal/editor` are built once as a
  static library (`make -C tidal lib`) and linked by the prototype, the tests
  and the desktop app. Nothing in them changes for the desktop app, so the
  device and desktop editors can't drift apart.
- **Input path.** A key is looked up in the keymap and becomes a `Control`
  (+ shift). That goes through `press` / `release` in `editor/controls.cpp`,
  exactly as on the Tracker. Only the three extras call `Editor` actions
  directly.
- **Backend.** The existing GHCi backend (`tidal/host/backend.cpp`) runs GHCi
  with `boot.ghci`, sends statements and reports errors. The engine backend
  (OSC straight to SuperDirt) stays as an option.
- **Limits.** `kMaxNodes` (4096) and `kMaxBlocks` (32) and the other pool
  sizes are constants sized for the MK66. The host build can raise them with
  `-D` overrides (they need `#ifndef` guards in `tree.h`; the device values
  stay the defaults).
- **View.** The first step is the prototype's 800×480 view scaled up, with
  the firmware fonts and an optional panel showing key caps. After that comes
  a resizable view: `layout.cpp` with a TrueType font through
  `stb_truetype` (public domain, one header). It shows more lines and more
  of the code width. To check: how much `layout.cpp` assumes the bitmap
  glyph widths.
- **Samples.** The app scans the SuperDirt `Dirt-Samples` folder. The 48
  instrument slots are a kit file (`--instruments`, as in the prototype),
  editable from Ctrl+O, and Ctrl+Shift+O lists every folder.

## More of Tidal

`LibEntry::device` already has `HostOnly`. To use a Tidal function on the
desktop it only needs a library entry with its signature, checked against
GHCi `:t` as the golden `:t` capture already does. It doesn't need an engine
node. Tracker mode (Ctrl+T) then marks code that won't run on the device, so
you can still write device-compatible files on the PC. Each new entry costs
device flash, so HostOnly entries go behind `#if TC_HOST_LIBRARY`.

## Highlighting

Highlighting is currently engine-backend only. Two options with real Tidal:

- **A.** Run our engine alongside GHCi, silently, only for the highlights.
  The highlights only match for functions the engine supports, and the random
  ones differ (randomness is our own).
- **B.** Use Tidal's editor-highlight OSC (the feed Pulsar uses: character
  ranges per event) and map the ranges to node ids through the renderer's
  positions. This is exact for everything. To do: check the message format
  in Tidal 1.10.3.

B is preferred. A is the fallback if B gets stuck.

## Platform

SuperDirt runs on Windows (where the audio is) and stays there. GHC + Tidal
are in WSL.

- **Development: the app and GHCi in WSL (WSLg window), SuperDirt on
  Windows.** This already works for the prototype (docs/host_setup.md
  sections 4–5). OSC goes to the Windows host at the WSL default gateway
  (or `127.0.0.1` in mirrored networking), through the firewall rule for
  UDP 57120. In WSL, Settings defaults the SuperDirt address to the
  gateway. There's no need for a second SuperDirt in WSL. To check early in
  M0: WSLg passes Alt, F10, F11, F12 and scancodes to SDL unchanged.
- **Release: native Windows (MSYS2 UCRT64 + SDL2), GHCi from ghcup on
  Windows.** This gives the best keyboard handling and latency, and it's the
  app to give other people. It needs a Windows GHC + Tidal install.

The code stays portable: SDL2 + C++17, and no POSIX in app code except the
GHCi process wrapper, which gets a Windows version.

## Milestones

| # | What | Done when |
| :--- | :--- | :--- |
| M0 | `tidal` library target; backends to `tidal/host/` (the prototype links them from there, with its keys unchanged); `apps/desktop` builds a window with the prototype view and the GHCi backend | in WSL, evaluating a block plays on Windows SuperDirt; WSLg key check done |
| M1 | keymap files (`iso-uk.txt`), F-row, cluster, Alt = Pattern, Ctrl+letter buttons, chord window; panel and soft-key bar show the keymap's keys | every control in the README table is reachable; a smoke session runs through key events |
| M2 | Settings screen + `desktop.conf` (chord window, keymap choice, Tidal / SuperDirt paths) | the chord window changes live from Settings and is kept after a restart |
| M3 | mapping editor (panel view, keyboard view, learn, conflicts, learn row, test mode, required-control check) | a remapped key works, shows in every hint, and survives a restart; `--keymap default` restores |
| M4 | extras: menu type-ahead, Enter value field, comment field | typing `0 3 5 7` or `fast 2` gives the same tree as the pad / menu route |
| M5 | resizable view, TrueType font, raised limits | a 30-block file is readable at 1080p |
| M6 | HostOnly library entries (first batch: the common Tidal functions the engine lacks) | `:t` golden check passes for them |
| M7 | highlighting (B, else A) | steps light while real Tidal plays |
| M8 | performance display (Ctrl+E, display setting, fixed font size, wrapping, scrolling with the cursor, theme) | opens fullscreen on the chosen display with code, highlights and cursor only; editing goes on in the main window |
| M9 | Windows build; Spout output; release notes | runs on a clean Windows machine with SuperDirt; Resolume or OBS receives the Spout frame, transparent background included |
| M10 | **when the layout is settled (your call):** the prototype loads the default keymap; generated Desktop column in `tidal/README.md` | `make -C tidal smoke` passes |

The performance display (M8) can come before highlighting (M7) if you want
it for a show sooner. It works without highlights, and they appear in it
once M7 lands. It needs M5's scalable font, because the 800×480 bitmap view
doesn't scale to a projector well.

## Decisions (2026-11-23)

- UK ISO keyboard: `iso-uk.txt` is the default keymap.
- SuperDirt on Windows; development in WSL sends OSC to it.
- The prototype keeps its keys until the desktop layout is settled (M9).
- F-row for Screen 1–8 (no Ctrl+1…8 default; the mapping editor can move them).
- Chord window: a setting (0–150 ms, default 40), changed in Settings.
- The mapping is data and can be edited in the app (mapping editor, M3).
- A performance display for the audience: code, highlights and cursor only, fullscreen on a chosen display (Perform = Ctrl+E), plus Spout output on Windows.

## Open questions

1. The default chord window (40 ms) is a guess. Try it in M1 and M2.
2. Is Alt comfortable as the held Pattern key, or would the key left of
   `Z` be better (it would then stop being pad 37)? Try it in M1. The mapping
   editor can change it either way.
