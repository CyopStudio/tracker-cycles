# Device editor (plan 4d), design draft v0.2

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
  the control mapping (hardware.cpp, meant to be reused; now
  tidal/editor/controls.cpp), the view
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
2. **Undo = packed checkpoints** (changed in the port, 2026-11-10; the
   first draft said block text). Every edit runs inside a
   `Program::beginCheckpoint` (tidal/core/tree.h): the block's and setcps's
   nodes and the header are saved, alloc() journals what it overwrites,
   and the comment table is saved if the block list changes. Committed
   checkpoints, packed to their used part (header + 18 B per node), are the
   undo steps: rolling them back newest first is exact, keeps node ids
   (playing block, highlights) and covers new / deleted blocks and setcps.
   Comment edits are their own small undo step. A typical step is ~1-3 KB.
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

## 4b. Results (editor bench v1, 2026-11-10, docs/tidal_editor_bench.txt)

Run 1 silent, run 2 with /project.tidal playing (audio ~32 %; run 2 is
1.2-1.4x slower). Light / everyday / heavy block (19 / 37 / 98 nodes) in
an 8-block file.

| Operation | Light | Everyday | Heavy |
| :--- | ---: | ---: | ---: |
| whole-Program copy (73,844 B, SDRAM) | 8.5-8.8 ms | | |
| parse the 8-block file | 4.5-5.1 ms | 4.0-5.0 ms | 5.2-5.7 ms |
| check the block | 0.16 ms | 0.8-1.3 ms | 2.3-2.6 ms |
| render (with spans) | 0.05 ms | 0.13 ms | 0.4-0.7 ms |
| undo by text (render + parse + check) | 0.8 ms | 1.9-2.5 ms | 5.3-5.6 ms |
| jog edit, in place | 0.5 ms | 0.8-1.3 ms | 1.9-2.3 ms |
| jog edit, whole copy | 8.7-9.5 ms | 6.6-8.7 ms | 7.7-8.1 ms |
| Replace menu (109 items), in place | 23-30 ms | 24-29 ms | 22-26 ms |
| `$` menu (51), in place | 22-29 ms | 54-65 ms | 110-129 ms |
| mini step menu (62), in place | 20-27 ms | 59-71 ms | 134-156 ms |
| the same menus, whole copy per item | 0.32-0.96 s | 0.35-0.81 s | 0.36-0.66 s |
| block copy per item | ~1.1-1.5x in place | | |

Display (test control, Roboto Mono 8 px advance): any refresh takes one
frame, 16-19 ms from refreshControl() to the swap, whatever its size (the
248-byte highlights-only control too); CPU spent in display.update() 3.5-5.7
ms per refresh. Display-list bytes: one CMD_TEXT per colour run costs ~24-29
B per run + 4 B per glyph: colour-coded code ran into the 6.5 KB guard after
6 lines (157 runs, 353 glyphs, 5.2 KB); 16 lines in one colour per line =
3.5 KB (770 glyphs, 16 runs).

Decisions:
1. Edit in place (whole copies are 8.5 ms each). Block copies buy nothing.
2. Undo by block text: confirmed (<= 5.6 ms).
3. Menus: in place. Replace menus are fine (< 30 ms). `$` / mini-step menus
   on heavy blocks take 110-160 ms, all of it the per-item block check:
   show the menu at once and type-check its items over the next main-loop
   passes (a few ms each, greyed until checked), or pre-filter by the
   candidate's result type before the full check. Mini-step menus passed
   62 / 62: atom-for-atom swaps can skip the check (check on apply).
4. Drawing: no CMD_TEXT per colour run. Draw glyphs as bitmaps (BEGIN
   BITMAPS + BITMAP_HANDLE once, VERTEX2II per glyph with
   VERTEX_TRANSLATE_X past x 511, COLOR only when it changes): ~4 B per
   glyph + 4 B per colour change, 16 coloured lines ~5 KB. Keep the code
   view's display list under ~6 KB so the header / soft keys fit in 8 KB.
5. Highlights: their own small control (248 B for 12 rectangles), refreshed
   at most ~15-20 times / s (3.5 ms of main-loop CPU each, ~7-10 %); the
   frame latency (one display frame) is fine for following the music.

## 5. Port order (after the timing build)

1. Editor model in tidal/core-style C++ (no heap): tree edit helpers,
   cursor / gap / menus, undo ring; host tests reuse the prototype's smoke
   sessions (same key tokens) against the new model.
   - 1a done (2026-11-10): the prototype edits in place. Program
     checkpoints (host test testCheckpoint: 300 random edit sequences
     rolled back exactly, an undo stack of 12 packed steps, journal-full
     alloc failure; mutations caught); menu items are `EditSpec`s (Replace,
     WrapApp, WrapList, WrapOp, InsertGap, InsertList, Launch) tried in
     place and re-applied when chosen; direct edits use beginEdit /
     endEdit; undo pops packed checkpoints. No whole-Program copies left.
     Smoke sessions 1-20 unchanged; session 21 makes ~15 kinds of edits
     and undoes them all back to the original file.
   - 1b + 1c done (2026-11-10): tidal/editor/ (namespace tc), built by
     `make arm` (35.6 KB -Os, 51 KB -O2; no heap symbols). Fixed strings /
     vectors (editor/fixed.h; overflowing codes are flagged and dropped,
     never parsed truncated), caller storage (Program, Checkpoint, undo
     arena: steps packed back to back, the oldest dropped when full),
     EditorOutput replaces the text backend (the prototype's BackendOutput
     in proto/output.* renders the same statements, so the smoke sessions
     are unchanged; File I/O, `--save` and the headless `lit` report moved
     there too). Menus: builders only collect EditSpecs; openMenu checks
     them all (prototype) or, with setLazyMenus(true), checkMenu(n) checks
     n per main-loop pass and drops misfits (an unchecked item can still
     be applied: doSpec checks it). Host test tests/test_editor.cpp.
     Stack (-Os, -fstack-usage): openWrap 5 KB, openSamples / quick ~3.8
     KB, openReplace / cursorInfo ~3 KB (openMini was 14 KB): measure the
     deepest point (menu build + check) on the device.
2. Control mapping table without std::string (shared by prototype and
   device). Done (2026-11-10): tidal/editor/controls.* (was
   proto/hardware.*; the button enum is `Control`, since tc::Ctl is the
   engine's control entry): labels are `Str<24>`, pad names formatted on
   demand (36 B of RAM instead of 769), `ctlFromId(const char*)`; 7.3 KB
   at -O2 on ARM. `ctlFwName()` names each control's firmware button
   (interfaceDefs.h); the device module binds those buttons to `press()`.
   Host checks in tests/test_editor.cpp.
3. FT812 code-view control (lines, colour runs, cursor, gap bar,
   highlights), soft-key labels, menu overlay; keyboard via
   mtKeyboardManager.
   Built (2026-11-10, -DTIDAL_EDITOR, PolyendTrac_TidalEditor1.ptf, not
   yet run): tidal/editor/layout.* lays the code area out as styled lines
   (the prototype view now draws from it: 10 reference screenshots
   byte-identical before / after); interface/tidalEditor draws it as
   bitmap glyphs (cell = char - the font's first char, VERTEX_TRANSLATE_X
   past x 511, a colour change only when it changes, rectangles first,
   scissored to the code area), plus the slot list, status lines, a menu /
   keyboard overlay (our own keyboard screen from the model, not
   mtKeyboardManager: the model already has the stock layout and
   behaviour) and the stock soft-key labels. Input: every button 0-32 (the
   Control enum is the firmware's button order), pads and jog through
   press() / release(). The player gained tidalPlayerEvaluate / Silence /
   Mute / UnmuteAll / SetCps / ReadFile / WriteFile; in the editor build it
   doesn't evaluate the file at boot. Also done here: the module in slot 2
   (step 4's first part). Open: pad LEDs, highlights (TC_EVENT_SRC on the
   device), the status screen as a sub-view, display-list totals (the
   status line shows ours).
   Editor1 on the device (2026-11-10, user): evaluate, mute and hush
   work, Shift+pad works, menus feel responsive; but the text was garbled
   (lowercase shown as capitals, digits / punctuation wrong: each glyph 32
   codes early, so SETFONT2 already maps the cell to the character code:
   VERTEX2II takes the code itself) and menus drew under the code (the
   display draws lower-numbered controls last: the overlay is now created
   first). Both fixed in Editor2.
   Editor2 (user): pad note / sound entry works, menus draw on top, the
   `dl N B` budget line stays present with menus open; but an empty slot
   picked with Shift+pad had no visible cursor and only Replace started a
   block. Editor3 (2026-11-10, not yet run): Note / Instrument (start
   `n "0"` / `s "<first instrument>"`), Screen 1-8, FX1 / FX2 and the menu
   keys start a block on an empty slot, whose "dN" is drawn selected
   (layout kCellSel on the Hint line); pad entry moves to the gap after
   the new step so the next pads insert (no Delete to end entry; smoke
   session 22).
   Editor3 (user): Params must stay free (it opened the comment: now
   Shift+Config; on the device Params leaves for the instrument
   parameters); at the lowest level Left / Right stopped at the end of a
   sibling list. Editor4: on a leaf (or with no sibling on that side) they
   go on to the previous / next leaf of the text, "..." gaps included,
   across the block (smoke session 23; 10 older sessions now edit other
   nodes, their expectations regenerated).
   Editor5 (2026-11-10, user's choice: both, voices automatic): pads held
   together make a stack `"[0,4,7]"` (functPads forwards releases;
   Editor::entryPadDown / Up); Shift+Note opens chord names on the note
   (`e'maj` ...); either adds `# voices 3` to a sample block that has no
   `voices` (smoke session 24).
4. Module in slot 2, player hand-over, File = save /project.tidal
   (4e: per-project files later).
5. Device build, then the parked UI TODO (pad modes) if it fits.

## 6. Editor timing build

`-DTIDAL_EDITOR_BENCH` (core/tidal/editor_bench.*, portable part in
tidal/core/edit_bench.*; host: `make -C tidal edbench`). Runs at 10 s and
60 s after boot like the engine bench (start a heavy project playing
between the runs), blocks the main loop for a few seconds per run, writes
`/tidal_editor_bench.txt`. See core/tidal/README.md for the build.
