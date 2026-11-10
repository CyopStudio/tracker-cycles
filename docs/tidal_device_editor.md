# Device editor (plan 4d), design draft v0.1

The prototype editor (tidal/proto) on the Original Tracker: same tree, same
controls (docs/tidal_ui_v2.md, tidal/README.md), drawn on the FT812 and
driven by the Tracker's buttons. This draft fixes what can be decided from
the code and lists what the editor timing build (below) has to measure
before the port.

## 1. Starting point

- Portable and already in every MK66 build: tree / parser / renderer /
  checker / engine / mapper / scheduler (tidal/core), the device voice
  layer and the file player (core/tidal), the Tidal status screen
  (interface/tidalStatus, module slot 2 = Pattern key).
- Prototype only: the editor model (tidal/proto/editor.cpp, ~2000 lines),
  the control mapping (hardware.cpp, meant to be reused), the view
  (view.cpp, an 800 x 480 software framebuffer), backends (text to GHCi or
  to our engine).
- The prototype is written for the host: `std::string` / `std::vector` /
  `shared_ptr`, every edit and every menu candidate copies the whole
  `Program` (sizeof ~70 KB: 4096 nodes x 16 B + strings + blocks), 64 undo
  copies, menus built by trial (copy + edit + type-check per item).

## 2. Budgets (from CLAUDE.md, measured)

| Resource | Free now | Notes |
| :--- | ---: | :--- |
| Flash | ~235 KB (789 KB of 1 MB) | editor logic + drawing must fit with room for more Tidal functions |
| Internal RAM | heap ~28 KB, stack 13 KB spare (tick v13) | editor state goes to SDRAM; hot UI state only inside |
| SDRAM | ~1.16 MB (+3.5 MB if SAMPLE_MEMORY_SIZE shrinks) | uncached: every access ~15 cycles (reverb bench) |
| Main loop CPU | ~40 % of the time with light playback, ~15 % heavy | shared with the scheduler's queries, SD streaming, display |
| Display list | 8 KB per frame (all shown controls appended) | CMD_TEXT = 4 B per glyph; each control has 8 KB of RAM_G and up to 5 stages |

Fonts: Roboto Mono 14 / 17 (Light, Regular, Bold) are already in flash
(the theme's mono set): monospaced code without new font data.

## 3. Decisions (can be made now)

1. **One Program, edited in place.** The editor's Program lives in SDRAM
   (raw storage + placement new). Edits change it in place (alloc /
   freeTree / replace, as the prototype's helpers already do); no copy per
   edit.
2. **Undo = block text.** Before an edit, the edited block (or setcps /
   a comment) is rendered to text and pushed on a ring in SDRAM (e.g. 32
   entries x 2 KB = 64 KB); undo re-parses that block in place. A block
   renders to well under 2 KB (kMaxBlockNodes 512). Cost: one render per
   edit, one parse per undo (both measured below).
3. **No heap in the editor.** Fixed buffers: menu items (label + code text
   in a static table or a small arena), message line, render buffer
   (8 KB) and spans (kMaxNodes x 4 B = 16 KB), both in SDRAM. Labels in
   the control mapping become `const char*`.
4. **Evaluate goes straight to the scheduler.** No text backend on the
   device: Pattern runs checkBlock + trackerCheck, then
   `Scheduler::evaluate(prog, slot)` on the editor's Program (it copies the
   block into the live program, as the player does). Statements like
   `mute` / `hush` become direct calls (setMuted, hush).
5. **The editor replaces the player's file loading, not the runtime.** The
   player's runtime (scheduler, DeviceVoice, MIDI clock) stays; the editor
   owns the Program the player used to parse from /project.tidal. Loading
   plays nothing (decided); Reload / Hush become editor actions. The
   status screen stays as a sub-view (e.g. Shift+Pattern) for load /
   voices / errors.
6. **Module slot 2.** The editor is a module in the Pattern key's slot
   (where tidalStatus sits now), so the stock module switching keeps
   working. Buttons are bound per context with `setButtonObj`, from the
   same mapping table as the prototype.
7. **Keyboard screen = mtKeyboardManager** (the stock one): Params opens it
   for comments, as in the prototype.
8. **Highlighting** (per-event steps): the step ids go in a per-slot side
   table indexed by ring position (Entry stays 48 B); TC_EVENT_SRC on for
   the editor build (+244 B flash, +2 KB internal working set).

## 4. Open questions (the editor timing build answers them)

| Question | Measured as | Decision rule |
| :--- | :--- | :--- |
| Cost of a whole-Program copy (what the prototype does per edit / item) | memcpy of sizeof(Program) in SDRAM | expected ~ms: confirms "edit in place" |
| Parse / check / render of a block (light, everyday, heavy) | per operation, us | undo by text if parse of a heavy block < ~20 ms |
| A jog edit (number + check) | in place, and with a whole copy | must stay < ~10 ms for a responsive jog |
| A Replace menu (~110 candidates) built by trial | full copy per item / block-only scratch copy / in place with rollback | < ~100 ms: build at once; else fill the menu over several main-loop passes |
| A `$` (wrap) menu (~50 functions) | the same three ways | same |
| A mini-notation menu on a step (~60 items) | same | same |
| Redraw of a full code screen | refresh latency until the frame is swapped, DL bytes, main-loop time in display.update() | < ~33 ms: highlights at 30 fps; else redraw highlights only (a separate small control) |
| DL bytes of a code screen with per-token colours | bytes appended | must leave room for soft keys / header within 8 KB |
| All of the above with a heavy project playing | run 2 | the main loop gets ~15 % then |

## 5. Port order (after the timing build)

1. Editor model in tidal/core-style C++ (no heap): tree edit helpers,
   cursor / gap / menus, undo ring; host tests reuse the prototype's smoke
   sessions (same key tokens) against the new model.
2. Control mapping table without std::string (shared by prototype and
   device).
3. FT812 code-view control (lines, colour runs, cursor, gap bar,
   highlights), soft-key labels, menu overlay; keyboard via
   mtKeyboardManager.
4. Module in slot 2, player hand-over, File = save /project.tidal
   (4e: per-project files later).
5. Device build, then the parked UI TODO (pad modes) if it fits.

## 6. Editor timing build

`-DTIDAL_EDITOR_BENCH` (core/tidal/editor_bench.*, portable part in
tidal/core/edit_bench.*; host: `make -C tidal edbench`). Runs at 10 s and
60 s after boot like the engine bench (start a heavy project playing
between the runs), blocks the main loop for a few seconds per run, writes
`/tidal_editor_bench.txt`. See core/tidal/README.md for the build.
