# Tracker Cycles retrospective (2026-11-12, at RC13)

A look back over the code base after the 1.0.0 work (UI v3 stages A–D): what's solid, what to improve, how much room is left, and how the next features could fit. Measurements are from the RC13 build (`make original`, commit after db835ea) unless noted.

---

## 1. Where things stand

| Area | Size | State |
| :--- | ---: | :--- |
| Engine (`tidal/core/engine.cpp` + library, parser, checker) | 72 KB flash (engine, -O2), 12 KB parser | 215 exact golden cases; rationals still the main CPU cost |
| Scheduler | 15 KB | stable since tick v9; no drops in any test set |
| Editor model + layout + controls + cursor mode (`tc_editor.o`) | **109 KB** | the biggest object; 3,567 lines in `editor.cpp` alone |
| Device glue (`core/tidal/*`, `interface/tidalEditor`) | ~25 KB | player, editor glue, folders loader, view |

### Memory left

| Region | Used | Free |
| :--- | ---: | ---: |
| Flash | 953,220 B (90.9 %) | **95,356 B (~93 KB)** |
| Internal RAM (static) | 188,952 B | 73,192 B, shared by heap and stack (the Profiler keeps a 32 KB stack reserve; the editor's deepest stack is still to be measured on the device) |
| SDRAM | 16,107,008 B | ~655 KB (plus ~3.5 MB if `SAMPLE_MEMORY_SIZE` is lowered, see CLAUDE.md) |

### Flash that could be won back

| What | Saves | Cost |
| :--- | ---: | :--- |
| Editor at `-Os` (a pragma in `core/tidal/tc_editor.cpp`) | **44 KB** (measured: 953,220 → 909,260 B) | menu checks and edits run maybe 1.3–1.5x slower; the engine and scheduler stay at -O2. Worth it once the menu-check work below is done. |
| Random name generator (`mtRandomNameGenerator`) | 15 KB | kept so far by your choice; project / sample names would need typing |
| Factory test (`mtTest`, Config button combinations) | 9 KB | only Polyend's production test uses it |
| Test-image code (`tidal_tick`, benches) | 0 | already left out of the normal build |

So about **93 KB free now, ~140 KB with `-Os` on the editor, ~165 KB if the name generator and factory test go too.**

---

## 2. What went well

* **One edit model, many front ends.** Every edit is an `EditSpec` on the tree with checkpoints and undo. The menus, both navigation modes, pads and the device all reuse it, so cursor mode came out at about 600 lines and the code stays valid in both modes.
* **Shared layout, byte-identical prototype.** `layout.cpp` drives both the prototype and the FT812 view, so most UI bugs reproduce on the host.
* **Measure first.** Benches (engine, tick, editor, reverb, folders) caught the real costs before they shipped: uncached SDRAM, rationals, display steps.
* **Golden cases from real Tidal.** These kept the engine honest through two function batches.
* **Audio safety.** Since tick v9 no test set drops notes. The loader and the jog both yield to the scheduler (`tidalPlayerAheadSeconds`).

## 3. What to improve in the code

1. **Split `editor.cpp`** (3,567 lines) the way `cursor.cpp` was split out: navigation (tree), menus (build / check / cache), edits (delete / unwrap / insert / wrap), pads and chords, keyboard and comments. It's mechanical, but it would make the next features cheaper to add and review.
2. **Cheaper menu checks** (the remaining cause of the highlight stall). Each item is a full trial edit: copy, apply, type-check the block, Tracker check, roll back.
   * Filter by the result type first: an item whose type can't fit the cursor's type code is dropped without a trial (planned as A3, not built).
   * Check only the edited subtree where the change can't affect the outer types.
   * Cache checked menus per context across edits elsewhere. Today any edit clears the cache.
3. **Device glue state machines.** The menu pacing, jog folding and autosave in `core/tidal/tidal_editor.cpp` live in function-static variables. Moving them into a small struct with a clock passed in would let host tests cover them; RC2–RC7 were all bugs there.
4. **Token cache.** Cursor mode renders the block once per key (cheap, about a millisecond). It could be cached by `changes()` plus the slot if blocks grow large.
5. **Smoke sessions count key presses.** Session 32 had to be recounted when the gap stops came in. A `find:TEXT` token (cursor onto the first token matching the text) would make sessions survive navigation changes.
6. **Destructive single keys.** Delete on the `dN` top clears the block in one press, in both modes. Undo brings it back, but on stage a confirm (press twice) or Shift+Delete would be safer.
7. **Small known bugs and limits:**
   * pads on a range (`"0 .. 7"`) replace the ends one after another rather than the step;
   * jog on a string's quotes reports an error rather than doing something useful (e.g. stepping the first sample);
   * sample names without letters don't parse;
   * list elements don't keep line breaks.
8. **CI.** `build.yml` builds the normal image and runs host tests. Building one test image (e.g. `-DTIDAL_TICK`) as well would stop the bench code from rotting unnoticed.
9. **Stock leftovers.** Tracker+ / Mini (IMX) code, the slim `Sequencer` shim, placeholder modules and the CE-only docs remain in the tree. Fine for now; trim them when touching those areas.

## 4. Documentation

* **A user manual, separate from the developer notes.** `tidal/README.md` mixes the prototype, the control table (tree mode first) and developer detail. Suggested:
  * `docs/manual.md`: install, the two modes, every control by context, menus, pads, projects, sample folders, troubleshooting;
  * `tidal/README.md`: the host build and the prototype only.
* **A one-page cheat sheet** of the panel (the prototype's `--panel` screenshot with labels) to print or keep on a phone.
* **A function reference with one example per function**, generated from the library table plus the golden cases, so it never drifts from what the engine does.
* **Split `CLAUDE.md`.** It's now mostly a history log. Keep the rules, design decisions and budgets there, and move the RC / bench history to `docs/history.md`. It's loaded into every session, so a shorter file is cheaper and clearer.
* **CE leftovers in `docs/`** (`custom_synth_sdk.md`, `wavetable_envelopes.md`, `modular_effects_system.md`, `image_viewer.md`, `custom_themes_and_fonts.md`, `developer_guide.md`) describe CE or IMX features. Mark them "inherited from CE" or move them to `docs/ce/`.
* **The wiki pages** (`docs/Home.md`, `_Sidebar.md`) should link the manual once it exists.

## 5. Config menu settings (ideas)

Stored in `mtConfig` (append fields only, so stock firmware still reads it):

| Setting | Values | Why |
| :--- | :--- | :--- |
| Start in | cursor / tree / last used | your answer was cursor at boot; some will prefer tree |
| Tracker mode | on / off | moves off the Config button, which could then do something else |
| Highlights | on / off / dim | saves display time on busy sets |
| Menu checking | fast / gentle | trades menu speed against highlights while playing |
| Info box / scroll bar | on / off | the parked "screen elements" idea: more lines of code |
| Default tempo | BPM | for new projects (Tidal's 0.575 cps = 138 BPM) |
| Voices governor | max sounding 8–16, load threshold | the measured trade-off from tick v9 / v10 |
| Autosave delay | 1–10 s / off | SD writes while performing |
| Delete on `dN` top | one press / press twice | see 3.6 |
| MIDI clock in | off / follow | the "wanted later" item from 4a |

## 6. Your feature ideas

### 6.1 Pages: several songs (`.tidal` files) over one sample bank, as tabs

The pieces are mostly there:

* **Files.** A project already has `<Name>.tidal`. Pages could be `<Name>.tidal`, `<Name>-2.tidal` … (or a `pages/` folder), each a normal Tidal file you can open on a computer. Names are shown on the tabs.
* **Switching while playing.** The scheduler plays its own copy of each evaluated block (the live program, `liveRaw`), so the editor can load another page **without stopping the music**. You'd prepare the next song while the current one plays, then evaluate blocks to bring it in, or use a "launch page" action (evaluate all blocks of the page on the next cycle, with `jumpIn`-style timing).
* **Memory.** The editor holds only the current page (one Program, 74 KB SDRAM). The others stay on the card; a switch is a save plus a parse (about 3–11 ms measured for real sets). Undo would be per page (cleared on a switch) unless each page gets its own undo arena (128 KB SDRAM each).
* **UI.** A thin tab strip at the top or above the soft keys. Keys need deciding:
  * **Perform** is free (but see 6.2);
  * **Shift+Song** is `setcps` today;
  * one option is a page menu on **Shift+File** (new, switch, rename, delete) plus **Shift+Left / Right at a block's top** to step through pages.
* **Flash.** About 4–8 KB.
* **Open questions:**
  * Does a page have its own `setcps`?
  * Do mutes carry over between pages?
  * Should "launch page" silence slots the new page doesn't use?

### 6.2 Perform mode: punch-in glitch FX per track

**Perform** is free, and the stock performance mode was removed in 4a. Two ways to build it:

* **(a) Pattern-level punch-ins (recommended first).** While a pad is held, dN plays a transformed copy of its live pattern; on release it goes back. No code changes, and nothing is saved.
  * Layout: hold Perform, then columns = tracks d1–d8 and rows = effects:
    * stutter / retrigger: `ply 2 / 4 / 8`, or repeat the current step;
    * reverse: `rev`;
    * half / double time: `slow 2` / `fast 2`;
    * crush: `# crush 4`;
    * filter sweep: `# cutoff (range 200 4000 saw)`;
    * gate: `mask "1 0 1 1 0 1 0 1"`;
    * "tape stop": `|* speed` falling.
  * It reuses the engine and scheduler (the punch-in is a wrapper node around the live block, so no parse is needed).
  * **The catch is latency.** The scheduler is 250 ms ahead. For a punch-in to feel immediate it needs a fast switch: truncate the queue at "now plus one block" (the switch-by-truncation in `docs/tidal_scheduler.md` already does this at a chosen time S) and query the new pattern straight away.
  * Flash about 5–10 KB; nearly no RAM.
* **(b) Audio-level FX per track** (beat-repeat buffer, tape stop, gate in the voice chain).
  * True audio glitches, independent of the pattern, but it costs audio-interrupt CPU per track (the governor's headroom) and buffers (about 0.5 s × 8 tracks ≈ 350 KB SDRAM).
  * The stock `performanceMod` path that did per-track FX was removed (`mtAudioPerformanceMode.cpp`); parts could come back from git.

I'd start with (a): it fits the Tidal model, is cheap and testable on the host, and the fast switch also helps 6.1's "launch page". Add (b) later for effects that patterns can't do (e.g. a tape stop on held notes).

## 7. Suggested order from here

1. Your RC11–RC13 device notes (cursor-mode feel, gaps, colours, the highlight stall).
2. Cheaper menu checks (3.2). This finishes the parked stall and makes `-Os` affordable.
3. Editor at `-Os` (+44 KB flash) once the checks are cheaper.
4. Release 1.0.0 (Stage E).
5. 1.1: split `editor.cpp`, the manual, the Config settings, then pages (6.1) and perform punch-ins (6.2a) with the fast scheduler switch.
