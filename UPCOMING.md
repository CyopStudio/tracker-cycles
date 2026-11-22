# Upcoming Changes (Unreleased)

Changes merged since the latest release. They are not in the release download yet; the build workflow publishes test builds for every push (see the Actions tab).

For what is in the current release, see [`CHANGELOG.md`](CHANGELOG.md).

---

### Tracker Mini (feature/mini, device testing: RC1-RC5)

- The Tidal editor firmware now builds for the Tracker Mini (`make mini`).
  The Mini plays one note at a time per track for now (`# voices` plays as 1).
- Mini controls: Fn1-4 = Pattern / Song / File / Config, Menu = the module
  menu (Shift: Tidal folders), Middle = OK / chord menu (Shift: status),
  +-1 / +-10 = jog. **Shift+Note** turns the screen keys into note entry:
  8 pads of a window, Left / Right move it 1, Up / Down an octave (or 8
  instruments). Hold Fn1 and press a screen key to mute a track.
  Details: docs/tidal_mini.md.
- Numbers showed as "1d" on the Mini (its printf has no 64-bit integers):
  the code view writes digits itself now.
- Insert on a dN: the screen keys become d1-d8 for a new block (Shift+Insert
  d9-d16). Note entry in an `s` pattern lists only loaded instruments.
  Instrument on a sample name opens the sample menu (both models).
- **Open bug:** docs/crash_project.tidal (`splice ... $ s "amen_brother"`)
  stops the Mini during the boot's code load (RC2 hung at 100%, RC3+
  power off). The Mini now sets such code aside (Workspace/crash.tidal)
  and boots empty; the cause isn't found yet (docs/tidal_mini.md, "Boot
  diagnostics").
