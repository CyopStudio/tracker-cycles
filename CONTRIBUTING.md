# Contributing to Tracker Cycles

Thanks for helping with Tracker Cycles: TidalCycles-style live coding on the Original Polyend Tracker. Bug reports, tested fixes, new Tidal functions and editor improvements are all welcome. These notes keep changes reliable on a small machine (MK66: 180 MHz Cortex-M4, about 1 MB of flash, 256 KB of internal RAM) that people play live.

---

## 1. What Tracker Cycles is for

* **A live-coding instrument.** Changes should make it better to play live on the Tracker itself: musical results, fast and predictable editing with the pads, jog wheel and buttons, and nothing that can stop the music.
* **Real Tidal.** The syntax is a subset of Tidal 1.10.3. A supported function behaves like Tidal's: same types, same events. Anything unsupported gives a clear error rather than something that only looks like Tidal. Tracker-specific additions (such as `# voices`) are named as such.
* **Never at the cost of the music.** No change may make the audio drop out or the scheduler fall behind while patterns play. The scheduler runs on the main loop, so long main-loop work (SD access, large memory moves) must be done in small steps.
* **Original Tracker only.** This fork targets the MK66 Original Tracker. Tracker+ and Tracker Mini code from CE is still in the tree but isn't built or supported.

---

## 2. Pull requests

1. **One change per PR.** One feature or fix, no unrelated refactors or formatting sweeps.
2. **Explain the goal, behaviour and test plan.**
   * **Goal:** what musical or editing problem it solves.
   * **Behaviour:** what the player sees and presses (controls, messages, screen).
   * **How to test:** steps someone can follow on the Tracker.
3. **Say what you tested on hardware.** Mark it in the PR if you have tested on an Original Tracker; otherwise say it needs someone with one. Every PR's checks build a `PolyendTracker_Cyc_<sha>.ptf` (the `tracker-cycles-firmware` artifact) that testers can flash (**Config → Firmware → Firmware update**).
4. **Credit everyone who did the work.** `Co-authored-by:` trailers for people and for AI coding tools (`Claude`, `Copilot` ...) are both welcome; much of Tracker Cycles was written with one.

---

## 3. Before you open a PR

* **Host tests and smoke sessions pass.**
  * `make -C tidal test`: the engine, editor and golden cases.
  * `make -C tidal smoke`: scripted editor sessions in the desktop prototype. A session whose output changes on purpose gets its `.expected` file updated in the same PR, with the reason given.
* **The firmware builds.** `make original` builds with no errors. Run `make clean` after changing `CUSTOM_DEFINES`, because the makefiles don't track flags. Check the linker's `FLASH` / `RAM` / `EXTERNAL_RAM` lines: flash is the tightest budget (about 89 % used at 1.0.0). Watch internal RAM too, because the heap and stack share what's left.
* **New or changed Tidal functions are checked against Tidal.**
  * Get the type from `:t` in GHCi.
  * Add cases to `tidal/golden/cases.txt` and capture Tidal's events with `make -C tidal golden` (this needs GHC and Tidal on the host; see `docs/host_setup.md`).
  * Add the case to `tidal/golden/engine_pass.txt` once the engine matches.
  * Random functions get property checks in `tidal/golden/random.txt`.
* **The device's limits are respected.**
  * No `new` / `malloc`, SD I/O or blocking in the audio interrupt or the engine's real-time paths. The engine and editor use fixed pools and caller-provided storage.
  * Keep stack frames small on the evaluator's recursive path (check `-fstack-usage`). Deep nesting costs about 1.3 KB per level.
  * Data in SDRAM (`__NOINIT(EXTERNAL_RAM)`) must have no constructors: static constructors run before SDRAM is initialised. Use raw storage with placement-new.
  * Use `#ifndef PLATFORM_IMX` for MK66-only code.
* **File formats stay compatible.** Existing `.tidal` files keep loading. Projects, instruments (`.pti`) and `mtConfig` stay readable by the stock firmware: don't reorder serialised enums or struct fields.
* **Line endings.** Many inherited files are CRLF. Keep each file's line endings (`git -c core.autocrlf=false add` for those).

`CLAUDE.md` holds the developer notes: design decisions, measured budgets, hardware gotchas and the history behind them.

---

## 4. Licences

* Tracker Cycles is distributed under the repository's [`LICENSE`](LICENSE) (MIT License with a Wholesale Commercial Clone Restriction, inherited from Polyend Tracker CE). By submitting a PR you agree that your contribution is licensed under those terms.
* **Don't copy or translate GPL / AGPL code.** Tidal's behaviour is implemented from its documentation and checked against its output. TidalCycles (GPL-3.0) and Strudel (AGPL-3.0) source code must not be copied or translated.
* **Don't add sample sets** without a clear licence that allows redistribution (Dirt-Samples has none).
* **Credit permissively licensed third-party code** you include in [`NOTICE.md`](NOTICE.md).
* **Leave the precompiled `synth/lib/*.a` archives alone.** They are Polyend-licensed; don't edit or redistribute them beyond their terms.
