# Tracker Cycles Desktop: plan

A standalone PC app for writing Tidal with the Tracker Cycles structured editor.
You use the keyboard, it sends real Tidal (GHCi, Tidal 1.10.3), and SuperDirt
makes the sound. Status: plan (2026-11-23), branch `feature/desktop-app`.

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
  when the keys go down within a **chord window** of about 40 ms (tune it on
  the device later and make it configurable). Rolled-over keys outside the
  window are separate steps. Shift+F5 (`,`) still adds a layer by hand. Most
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
missing. There's an optional alternative of **Ctrl+1…8** for Screen 1–8 and
Ctrl+Shift+1…8 for their Shift functions. It's off by default, because the
F-row placement is what gives the muscle memory.

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
| Ctrl+P, Ctrl+E | Params, Perform | reserved, as on the device |
| Tab / Shift+Tab | next / previous slot | there's no device control for this |
| Ctrl+Q | quit | |

Alt and the Windows menu bar: the app consumes Alt, so a lone Alt tap never
opens a system menu. AltGr (right Alt on UK and EU layouts) is not Pattern.

### What changes for the prototype

The prototype's current keys (`n`, `s`, `f`, `c`, `e`, `+`/`-`, digits as
slot pads …) put shortcuts on the letters, which the pad grid needs. Once the
desktop keymap works, the prototype switches to it as well, so there's only
one PC mapping. (Its panel hints already disagree with its keys about Space,
Play and Pattern.) The headless `--keys` tokens stay as they are, so the
smoke sessions keep working.

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

## Architecture

```
apps/desktop/
  PLAN.md            this file
  Makefile           builds tidal-desktop (SDL2), links the shared library
  main.cpp           arguments, config, main loop
  keymap.{h,cpp}     scancode -> Control (+ shift / held) table, chord window,
                     Alt = Pattern hold; loads keymaps/*.txt
  keymaps/           iso-uk.txt, ansi-us.txt (default chosen by layout)
  text_entry.cpp     Enter field, menu type-ahead, comment field
  view_desktop.cpp   resizable view: layout.cpp + scalable font
  samples.cpp        Dirt-Samples scan -> 48-slot kit + folder menu
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
- **Backend.** The existing GHCi backend (`proto/backend.cpp`) runs GHCi
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

GHC + Tidal are installed in WSL. SuperCollider and SuperDirt most likely
run on Windows, where the audio is. Options:

1. **App and GHCi in WSL (WSLg window).** This builds with today's Makefile.
   OSC goes to the Windows host IP (`boot.ghci` `oAddress`). The cost is
   WSLg's keyboard handling: check Alt, F11 and F12, and scancodes.
2. **Native Windows (MSYS2 UCRT64 + SDL2), GHCi from ghcup on Windows.**
   This is the best keyboard and latency, and the app to give other people.
   It needs a Windows GHC + Tidal install.

Start with 1 for development and make 2 the release target. Keep the code
portable (SDL2 + C++17, no POSIX in app code except the GHCi process
wrapper, which gets a Windows version).

## Milestones

| # | What | Done when |
| :--- | :--- | :--- |
| M0 | `tidal` library target; backends to `tidal/host/`; `apps/desktop` builds a window with the prototype view and the GHCi backend | evaluates a block and SuperDirt plays it |
| M1 | keymap (scancodes, ISO / ANSI files), F-row, cluster, Alt = Pattern, Ctrl+letter buttons, chord window; panel shows key caps | every control in the README table is reachable; a smoke session runs through key events |
| M2 | prototype on the same keymap; `tidal/README.md` controls table gets a Desktop column | `make -C tidal smoke` passes |
| M3 | extras: menu type-ahead, Enter value field, comment field | typing `0 3 5 7` or `fast 2` gives the same tree as the pad / menu route |
| M4 | resizable view, TrueType font, raised limits | a 30-block file is readable at 1080p |
| M5 | HostOnly library entries (first batch: the common Tidal functions the engine lacks) | `:t` golden check passes for them |
| M6 | highlighting (B, else A) | steps light while real Tidal plays |
| M7 | Windows build, config file (SuperDirt address, Dirt-Samples path, keymap, chord window), release notes | runs on a clean Windows machine with SuperDirt |

## Open questions (for the user)

1. Keyboard: UK ISO? This sets the default keymap and pad 36 / 37.
2. Where does SuperDirt run: Windows or WSL? And should GHCi be on Windows in
   the end (Platform option 2)?
3. Should the prototype move to the new keymap (M2), or keep its letter
   shortcuts?
4. Is the F-row OK for Screen 1–8 on your keyboard, or do you want the
   Ctrl+1…8 alternative as the default?
5. Chord window: should rollover make chords (≈40 ms window) or separate
   steps? This is a feel question, best tried in M1.
