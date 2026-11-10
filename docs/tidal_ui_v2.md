# Editor controls v2 (from docs/UI-changes.md), draft

The user's change requests for the editor (prototype now, the device editor
in plan 4d), with the decisions from the follow-up questions (2026-11-10).
Everything is implemented in the prototype first (tidal/proto), where the
mapping lives in `controlCommand()` (hardware.cpp) and is reused on the
device.

## 1. Navigation

- **Gap cursor.** Inside a sequence `"..."` / list, Left / Right step
  item, gap, item, gap... (a gap before the first and after the last item
  too), drawn as a thin bar. On a gap, pads / menus / F-keys insert a new
  item there; on an item they replace / wrap it as today.
- **Shift+Insert (Home)** goes to the very top of the block: the block node
  itself (`dN $` / `jumpMod N k $`), not the expression below it.
- **Up / Down at the block's top** move to the previous / next block.

## 2. Structure: several blocks per dN

- A file can hold several blocks for the same dN (alternatives); you switch
  by evaluating one of them.
- **Loading plays nothing** (prototype and device): every block waits until
  you evaluate it.
- **Insert, then pad N** at a block's top adds a new empty block for dN
  above the current block.
- Pads 1-16 jump to dN's block that is playing, else its first block;
  pressing the same pad again cycles through dN's blocks.

## 3. Screen keys (F1-F8)

Structure at the cursor ("where"); FX1 / FX2 pick functions / controls
("what") from the same lists, filtered.

| Key | Menu |
| :--- | :--- |
| F1 `$` | functions applied with `$` to the node (`fast 2 $ ...`) |
| F2 `#` | `# control` (all mapped controls) |
| F3 `[]` | mini-notation group `[ ]` around the step / at the gap |
| F4 `<>` | alternation `< >` |
| F5 `{}` | polymeter `{ }%4` |
| F6 `()` | Euclid `(3,8)` on the step |
| F7 `\|` | random choice `bd \| sd` around the step / group |
| F8 `\|+` | the `\|+ \|- \|* \|/ \|< \|>` operators with a control |

**Shift + F1-F8** = the old screen keys: Replace, Wrap, Unwrap, Undo, and
the four context quick-buttons (`# <last control>`, `|+ n 12`, ...).

## 4. Other keys

| Key | Does |
| :--- | :--- |
| Note | jump to the block's `n` / `note` pattern (created as `# n "0"` if missing), cursor on its first step |
| Instrument | jump to the block's `s` pattern (created as `# s "bd"` if missing) |
| FX1 | menu: all pattern modifiers (fast, slow, every, rev, jux, off, chop...) |
| FX2 | menu: all sound FX controls (cutoff, room, crush, pan...) |
| Pattern | evaluate the block at the cursor |
| Song | menu: transitions for the block (`dN $`, `jumpMod N k $`, `jumpIn N k $`) |
| Play + pad N | mute / unmute dN (its pattern keeps running silently, Tidal's `mute`) |
| Shift+Play + pad N | solo dN (Tidal's `solo`) |

## 5. Settled (2026-11-10)

- Play is a modifier, like Shift: pressing it does nothing yet; pads
  pressed while it is held mute / unmute their dN (Shift+Play: solo), and
  releasing it does nothing more. Released with no pad pressed: unmute
  all (Shift+Play: hush). Delete while Play is held cancels: releasing
  Play then does nothing.
- The device player (plan 4a stage 3) keeps playing /project.tidal at
  boot until the device editor replaces it; "nothing plays at load"
  applies to the editor.
- At a block's top, Right enters the expression (Up / Down move between
  blocks there).

## 6. Work, in order (each testable in the prototype)

1. Done (2026-11-10): Shift + Screen 1-8 = the old functions (unshifted
   the same until step 2), Home to the block top, Up / Down between
   blocks there, Right into it, Pattern = evaluate (key `e`), Song =
   transitions (key `l`); smoke session 11, sessions 4-6 updated to the
   new keys (same results).
2. Done (2026-11-10): Screen 1-8 menus (Action::Struct1-8: `$`
   functions, `#` controls, `[ ] < > { } ( , ) |` around the step(s) inside
   "..." with the old one-touch wrap as each menu's first item, `|+ |- |*
   |/ |< |>` with common controls), FX1 = pattern modifiers (Time /
   Structure / Random / Layer / Sample functions), FX2 = sound FX controls;
   smoke session 12; sessions use Shift+Screen and the `functions` /
   `controls` tokens for the old menus (same results).
3. Done (2026-11-10): Note / Instrument = Action::JumpNotes /
   JumpSamples (first `n` / `note` or `s` / `sound` "..." in the block,
   cursor on its first step; missing: `# n "0"` / `# s "bd"` added to the
   block's whole pattern; no `s` on MIDI blocks); the old notes / samples
   menus stay as the `notes` / `samples` actions (no key); smoke session 13.
4. Mute / solo: scheduler drops a muted slot's notes but keeps its timing;
   both backends (GHCi: `mute` / `solo` / `unmute`), Play + pad.
5. Gap cursor (editor model + rendering + insert at a gap).
6. Several blocks per dN: Program keeps a block list instead of one root
   per slot (core tree, parser, renderer, file format, scheduler
   evaluate, editor, device player); the biggest change, touches
   tidal/core and its tests.
