# Tracker Cycles code tree (spec, step 0)

Status: draft v0.2. Shared by the editor (prototype and device) and the engine.
Implementation: `tidal/core/` (see `tidal/README.md`).

The code tree is the source of truth for a project's code. The editor edits
the tree, renders it as Tidal text, and the engine compiles it into its own
runtime pattern graph. Text is only parsed when a `.tidal` file is loaded.

Target: **Tidal 1.10.3** (tidal-core 1.10.x). Everything here is written
from Tidal's documentation and observed behaviour, not its source.

---

## 1. Goals and rules

1. **Round trip.** `parse(render(t)) == t` for every valid tree, and
   `render(parse(s))` equals `s` up to whitespace and redundant parentheses.
   Rendered text must run unchanged in real Tidal 1.10.3.
2. **Typed.** Every node has a resolved type. The editor uses types to offer
   only valid choices; the engine uses them to pick node implementations.
3. **Fixed memory.** Nodes live in a fixed pool, addressed by 16-bit index.
   No allocation while editing, rendering or compiling.
4. **Subset, not dialect.** Anything the tree can't represent is a parse
   error with a location and a clear message. We never invent syntax.

Supported language (settled): function application, `$`, `.`, infix
operators, operator sections, numeric literals, string (mini-notation)
literals, lists. **Not supported:** lambdas, `let`, `where`, `do`, guards,
user-defined functions, type annotations, `^name` variables.

---

## 2. Program and blocks

A program is a fixed set of block slots plus globals.

| Slot | Meaning | Text form |
| :--- | :--- | :--- |
| `d1`..`d8` | Sample channels (Tracker sample tracks) | `d1 $ <expr>` |
| `d9`..`d16` | MIDI channels 1..8 (`d9` = channel 1) | `d9 $ <expr>` |
| `cps` | Global tempo; holds an expression, e.g. `(135/60/4)` | `setcps (<expr>)` |

- Each slot holds one `Block` node whose single child is an expression of
  type `ControlPattern`, or is empty.
- A block renders as `dN $ <expr>`, or `dN <atom>` when the expression is a
  single atom (e.g. `d1 silence`).
- `hush` and similar commands are editor actions, not stored in the tree.
- A `.tidal` file is the blocks in slot order, separated by blank lines.
  Line comments (`-- ...`) directly above a block are kept as the block's
  comment; other comments are dropped with a warning on load.
- Evaluating a block compiles its expression and replaces that channel's
  pattern at the next cycle boundary.

### 2.1 Launch modes (transitions)

A block can instead switch with one of Tidal's jump transitions. The mode
is a property of the block (`Block` flags `kLaunchMask`, cycles in `b`),
not an expression node; it only changes when an evaluation takes effect.

| Mode | Text form | Switches at (now = cycle position when evaluated) |
| :--- | :--- | :--- |
| Now | `dN $ <expr>` | next cycle boundary (Tidal: immediately) |
| JumpMod | `jumpMod N k $ <expr>` | smallest multiple of k strictly after now (k 1..256) |
| JumpIn | `jumpIn N k $ <expr>` | `ceil(now) + k` (k 0..256) |

Timing observed in Tidal 1.10.3 (golden cases `@jump*`): `jumpMod 4` at
2.5, 3.9 → 4, at 4 → 8; `jumpMod 3` at 0 → 3; `jumpIn 2` at 2.5 → 5, at
3 → 5; `jumpIn 0` at 2.5 → 3. N and k are plain whole numbers (Tidal:
`ID -> Int`); only one form per slot is allowed in a file. The prototype's
GHCi boot script defines both as in Tidal's BootTidal.hs. `xfade`,
`jumpMod'` and the other transitions are out of v1.

Note: plain `dN $` replaces immediately in Tidal but at the next cycle in
our engine; step 3's OSC comparison has to allow for that.

---

## 3. Type system

### 3.1 Types

```
B    ::= Time | Double | Int | Note | String | Bool | Control
T    ::= Pat B            -- a pattern of B   (ControlPattern = Pat Control)
       | Pat a            -- type variable, with optional constraint
       | T -> T           -- function (curried)
       | [T]              -- list (only as a function argument)
C    ::= Num | Fractional | Unionable
```

- `Time` is Tidal's `Rational`; `Integer` is folded into `Int`.
- `Note` is a numeric type that also accepts note names and chords.
- Constraints: `Num` = Time, Double, Int, Note; `Fractional` = Time, Double,
  Note; `Unionable` = any B (used by `#`, `|<`, `|>` families).
- Plain non-pattern values (`Int`, `Time`) don't appear: Tidal's functions
  take patterned arguments, and a literal `2` is a `Pat B`.

### 3.2 Inference

- Unification over `T`, with constraints checked on binding. No
  let-polymorphism is needed (no `let`).
- Function signatures come from the library table (section 7).
- Literals are typed by context:
  - A numeric literal is `Pat B` for any `Num B` (`Fractional B` if it has a
    fractional part).
  - A string literal is `Pat B` for whatever `B` is expected; its atoms are
    checked against `B` (section 5.4).
- No defaulting. In GHCi a bare literal stays polymorphic (`1 :: Num a =>
  a`) and a literal argument takes its type from the function's signature
  (`fast 2 ...` makes `2` a `Pat Time`). Since every block is a
  `ControlPattern`, signatures fix every literal in practice. A type
  variable still unresolved at block level is an "ambiguous type" error in
  the editor.
- The editor holds incomplete trees: a `Hole` node has an expected type and
  makes the block non-evaluable until filled.

### 3.3 Type storage

For now a node stores a compact type code: `Pat <Base>`, `Pat a`, function,
list, or 0 = unknown/error (`tidal/core/check.h`). Full types are recomputed
by the checker when needed. A per-program table of interned types (max 255)
is deferred until the device editor needs it.

---

## 4. Expression nodes

| Kind | Children | Payload | Renders as |
| :--- | :--- | :--- | :--- |
| `Num` | none | rational value + format (see 6.3) | `2`, `0.25`, `(-1)`; a ratio like `(1/8)` is an `Op /` of two `Num`s, rendered unspaced |
| `Mini` | one mini `Seq` root | — | `"bd*2 [sd cp]"` |
| `Fn` | none | library id | `fast`, `rev`, `s`, `sine`, `silence` |
| `App` | function expr, then args | flag `dollar` | `fast 2 $ s "bd"` / `fast 2 (s "bd")` |
| `Op` | lhs, rhs | operator id | `s "bd" # gain 0.8` |
| `SectionL` | lhs | operator id | `(0.25 ~>)`, `(1/8)` is an `Op`, not a section |
| `SectionR` | rhs | operator id | `(# speed 2)`, `(+ n 12)` |
| `Compose` | f, g | — | `rev . fast 2` |
| `List` | elements | — | `[s "bd", s "hh*2"]` |
| `Hole` | none | — | editor placeholder, written `??`. Files may contain holes (so drafts survive saving), but a block with a hole can't be evaluated and Tidal can't run it |

Notes:
- `App` is n-ary over a curried function; fewer args than the signature
  gives a function-typed value (partial application, e.g. `every 3 (fast 2)`
  passes `fast 2` as an `App` with one arg).
- `dollar` on an `App` means its last argument is rendered after `$`.
  It is style only and preserved for round trip.
- Parentheses are not nodes. The renderer inserts the minimum parentheses
  from the fixity table; the parser drops them.

### 4.1 Fixity table

Haskell's own: `$` infixr 0, `.` infixr 9, `+`/`-` infixl 6, `*`/`/`
infixl 7, function application binds tightest.

Tidal operators in scope (v1): `#`, the `|op|`, `|op`, `op|` forms for
`+ - * / % < >` (and `++` for strings), `<~`, `~>`.

Fixities from `:info` in GHCi, Tidal 1.10.3 (checked 2026-11-08):

| Operator | Fixity | Checked |
| :--- | :--- | :--- |
| `#` | infixl 9 | yes |
| `\|+\|`, `\|+`, `+\|` | infixl 9 | yes |
| `\|<`, `\|>` | infixl 9 | yes |
| `<~`, `~>` | infixl 9 | yes |
| `\|-\|`.., `\|*\|`.., `\|/\|`.., `\|%\|`.., `\|<\|`, `\|>\|`, `\|++\|`.., and the `\|\|op` forms (`\|\|+` `\|\|-` `\|\|*` `\|\|/` `\|\|<` `\|\|>`) | infixl 9 | yes (2026-11-09) |

Consequences for the parser and renderer:
- All Tidal operators share one level and associate left:
  `1 <~ s "bd" # n 1` is `(1 <~ s "bd") # n 1`, and
  `a # b |+ c` is `(a # b) |+ c`.
- They bind **tighter** than Haskell arithmetic (`+`, `-` infixl 6; `*`, `/`
  infixl 7): `n "1" # n "2" + n "3"` is `(n "1" # n "2") + n "3"`.
- `.` is infixr 9, the same precedence with the opposite associativity.
  GHC rejects an unparenthesised mix (`rev . fast 2 # speed 2`), so the
  parser reports the same error, and the renderer always parenthesises when
  a `Compose` and a Tidal operator meet.
- Function application binds tightest; `$` (infixr 0) loosest.

---

## 5. Mini-notation nodes

A `Mini` literal holds a tree of mini nodes. All of them share the node
pool and layout with expression nodes.

### 5.1 Containers

| Kind | Children | Payload | Renders as |
| :--- | :--- | :--- | :--- |
| `MSeq` | steps | flag `dots` | `a b c`; with `dots`, children are sub-sequences joined by ` . ` |
| `MSub` | `MSeq`s (layers) | — | `[a b, c d]`: subdivide one step, layers stacked |
| `MAlt` | `MSeq`s (layers) | — | `<a b c>`: one child per cycle; layers stacked |
| `MPoly` | `MSeq`s (layers) | optional steps arg (child) | `{a b c, d e}` / `{a b c}%4` |
| `MChoice` | `MSeq`s | — | `[a \| b \| c]`: random choice, re-picked each time the step occurs (deterministic, seeded by time) |

The top level of a string is an `MSeq`, or an `MSub` if it uses `,` at top
level (Tidal requires `[ ]` there; the renderer adds them).

### 5.2 Step modifiers

A step is an atom or container, optionally wrapped in modifier nodes. The
innermost wrapper applies first, matching left-to-right text order
(`bd*2?` = `Degrade(Fast(bd, 2))`). Confirmed in Tidal 1.10.3:
`bd*2!2` is `Replicate(Fast(bd, 2), 2)` (4 events), and `bd(3,8)?` degrades
the euclid's events individually.

| Kind | Args (mini sub-trees, may be patterned) | Renders as |
| :--- | :--- | :--- |
| `MFast` | factor | `x*2`, `x*<2 3>` |
| `MSlow` | factor | `x/2` |
| `MReplicate` | count (optional) | `x!3`, or bare `x !` repeat |
| `MWeight` | weight | `x@3`; flag `underscore` renders as `x _ _` |
| `MDegrade` | probability (optional, default 0.5) | `x?`, `x?0.8` |
| `MEuclid` | k, n, rotation (optional) | `x(3,8)`, `x(<3 5>,8,2)` |
| `MRange` | children: lo atom, hi atom | `0 .. 7`, `c .. e`; numeric and `Note` types. Expands into one step per value in the enclosing sequence (`c .. e` = 5 steps). Descending ranges: verify |

### 5.3 Atoms

| Kind | Payload | Renders as |
| :--- | :--- | :--- |
| `MRest` | — | `~` |
| `MWord` | string id + optional index | `bd`, `bd:3` |
| `MNum` | rational + format | `1`, `0.25`, `-3` |
| `MNote` | `a` = semitone from c5; `b` = original spelling (string id + 1, 0 = canonical) | `c`, `cs5`, `ef3`; `c` = `c5` = 0, `s` sharp, `f` flat |
| `MChord` | root (note) + chord id + modifier list | `c'maj`, `e'min9'i2'o` |
| `MBool` | value + style | `t`, `f`, `1`, `0` |

### 5.4 Atoms allowed per type

| Expected `B` | Allowed atoms |
| :--- | :--- |
| `String` | `MWord`, `MRest` |
| `Note` | `MNum`, `MNote`, `MChord`, `MRest`, `MRange` |
| `Double`, `Time` | `MNum`, `MRest`, `MRange` |
| `Int` | integer `MNum`, `MRest`, `MRange` |
| `Bool` | `MBool`, `MRest` |
| `Control` | not allowed: control patterns are built with `s`, `n`, etc. |

Modifier arguments are always numeric mini sub-trees (`Time` for `*`, `/`,
`@`; `Int` for `!` and euclid; `Double` for `?`).

---

## 6. Storage

### 6.1 Node layout (16 bytes)

```c
struct Node {
    uint8_t  kind;    // section 4/5 kinds
    uint8_t  type;    // type-table index (0 = none/error)
    uint16_t flags;   // per-kind flags (dollar, dots, underscore, format)
    uint16_t first;   // first child (0 = none)
    uint16_t next;    // next sibling (0 = none)
    uint32_t a;       // payload: library id / string id / numerator / ...
    uint32_t b;       // payload: denominator / sample index / chord id / ...
};
```

- Index 0 is the null node. Children are first-child / next-sibling lists,
  so arity is variable without extra storage.
- `Num`/`MNum` store an exact rational: `a` = numerator (int32), `b` =
  denominator (uint32, > 0, GCD-normalised).

### 6.2 Limits (v1, revisit after the step 2 benchmarks)

| Limit | Value |
| :--- | ---: |
| Nodes per program | 4096 (64 KB, lives in SDRAM) |
| Nodes per block | 512 |
| Nesting depth (expression + mini) | 32 |
| Children per node | 64 |
| Steps per mini sequence | 64 |
| Interned strings per program | 256, each <= 15 chars (`[A-Za-z0-9_.-]`) |
| Types per program | 255 |

Exceeding a limit is an edit-time refusal or a load-time error naming the
block and the limit.

### 6.3 Number formatting

A number keeps how it was written so the text round-trips: integer or
decimal with N fractional digits (`0.25`), with sign. Ratios are `Op /`
nodes (section 4). A
decimal with more than 9 fractional digits, or one whose rational doesn't
fit 32 bits, is a load-time error.

### 6.4 String table

Sample names and other words are interned per program; nodes store the
id. Function and control names are not strings: they are library ids.

---

## 7. Function library table

One row per function, control or operator, shared by parser, renderer,
editor and engine:

| Field | Meaning |
| :--- | :--- |
| `id` | Stable 16-bit id (never reused; stored in saved trees) |
| `name` | Tidal name as rendered |
| `sig` | Type signature in section 3 notation |
| `args` | Argument names + default value for insertion (`fast 2`) |
| `fixity` | Operators only |
| `category` | For the editor's menus (time, structure, random, control, ...) |
| `device` | `full`, `approx` (works with documented differences) or `host-only` |

Controls (`s`, `n`, `note`, `gain`, `pan`, `cutoff`, ...) are rows with
signature `Pat B -> ControlPattern` and also carry their mapping to the
Tracker engine (see CLAUDE.md control table).

### 7.1 Initial candidates (v1)

Signatures are as documented on Hackage where shown; verify every row
against Tidal 1.10.3 (`:type` in GHCi) when it gets a golden test.

- Core: `silence`, `fast`, `slow`, `hurry`, `rev`, `stack`, `cat`,
  `fastcat`, `slowcat`, `overlay`, `<~`, `~>`, `every`, `whenmod`,
  `palindrome`, `brak`, `iter`, `ply`, `linger`, `segment`, `range`
- Structure: `struct`, `mask`, `euclid`, `euclidInv`, `euclidFull`
- Random: `degradeBy`, `sometimesBy`, `sometimes`, `often`, `rarely`,
  `almostNever`, `almostAlways`, `someCyclesBy`, `rand`, `irand`
- Continuous: `sine`, `cosine`, `saw`, `tri`, `square`
- Layering: `superimpose`, `off`, `jux`, `stut`/`echo`, `chunk`
- Samples: `chop`, `striate` (host-only until the device supports them),
  `loopAt`, `splice` (approximate on device: playback rate via tune)
- Harmony: `scale`, `arp`, `up`
- Operators: `#`, `|+|`/`|+`/`+|`, `|-|`.., `|*|`.., `|/|`.., `|<`, `|>`
- Controls: `s`, `n`, `note`, `gain`, `pan`, `cutoff`, `resonance`, `hpf`,
  `bandf`, `delay`, `room`, `crush`, `shape`, `begin`, `speed`, `legato`,
  `sustain`, `nudge`, `cut`, `orbit`

---

## 8. Editor operations (informative)

All edits are type-directed: the editor knows the expected type at the
cursor and offers only matching choices.

- Move: parent / first child / next / previous sibling (wheel + pads).
- Replace node with a candidate (function, control, literal, atom).
- Wrap: `x` -> `f ... x` (e.g. wrap a pattern in `every 4 (fast 2) $`), or a
  mini step in `[ ]`, `< >`, or a modifier.
- Unwrap, delete (leaves a `Hole`), insert sibling (sequence step, list
  element, `#` control).
- Change number with the wheel; change word from the sample-bank list.
- Edits go to a working copy; evaluate compiles it (see engine
  constraints in CLAUDE.md).

---

## 9. Open questions

1. **MIDI channel naming: decided.** `d9`..`d16` are MIDI channels 1..8,
   with no MIDI routing written in the code. Real Tidal needs a SuperDirt
   MIDI target for this, so the prototype backend appends
   `# s "midi" # midichan N` (N = 0..7) when sending d9..d16 to SuperDirt;
   it is never stored in the tree or the `.tidal` file.
2. **Operator fixities** (4.1): resolved. Every Tidal operator family is
   infixl 9 (`:info`, 2026-11-09).
3. **Numeric defaulting** (3.2): resolved, no defaulting needed.
4. **Random seeds.** `?`, `|`, `rand` and friends are deterministic in Tidal
   (seeded from cycle position). The tree only records where randomness is;
   matching Tidal's exact values is an engine/golden-test question.
5. **Transitions**: `jumpMod` and `jumpIn` are supported as block launch
   modes (2.1). `xfade` and the rest, `once`, `p "name"`, muting with
   `_d1`: out of v1.
6. **Mini-notation corner cases**: resolved; results in section 10.
   Modifier chaining and `..` with note names (5.2); descending `..`
   counts down; `!` and `_` after a group repeat / stretch the group;
   nested polymeter `%` works; the `%` steps can be a pattern (`%[4]`).
   Our parser accepts all of these. `[bd sd]!` renders as `[bd sd] !`,
   which gives the same events.

---

## 10. Observed Tidal 1.10.3 results (first golden cases)

From `queryArc` in GHCi, 2026-11-08. Times in cycles.

| Pattern (type) | Arc | Events |
| :--- | :--- | :--- |
| `"[bd \| sd \| hh]"` (String) | 0-4 | 0-1 sd, 1-2 sd, 2-3 bd, 3-4 hh |
| `"bd*2!2"` (String) | 0-1 | bd at 0, ¼, ½, ¾, each ¼ long |
| `"bd(3,8)?"` (String) | 0-1 | 0-⅛ bd, ⅜-½ bd |
| `"c .. e"` (Note) | 0-1 | 0, 1, 2, 3, 4 (c5..e5), each ⅕ long |
| `"e .. c"` (Note) | 0-1 | 4, 3, 2, 1, 0 (e5 down to c5), each ⅕ long |
| `"[bd sd]! hh"` (String) | 0-1 | bd 0-⅙, sd ⅙-⅓, bd ⅓-½, sd ½-⅔, hh ⅔-1 (`"[bd sd] ! hh"` the same) |
| `"[bd sd] _ hh"` (String) | 0-1 | bd 0-⅓, sd ⅓-⅔, hh ⅔-1 |
| `"{a {b c}%3}%4"` (String) | 0-1 | a 0-¼, b ¼-⅓, c ⅓-5/12, b 5/12-½, a ½-¾, c ¾-⅚, b ⅚-11/12, c 11/12-1 |
| `"{a b c}%[4]"` (String) | 0-1 | a 0-¼, b ¼-½, c ½-¾, a ¾-1 |

Golden tests: compare event times and values only. Tidal's `Event ==`
also compares the source-position context, so two spellings with the
same events (`"[bd sd]!"` / `"[bd sd] !"`) are not `==`.
