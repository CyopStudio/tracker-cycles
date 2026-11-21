# Upcoming Changes (Unreleased)

Changes merged since the latest release. They are not in the release download yet; the build workflow publishes test builds for every push (see the Actions tab).

For what is in the current release, see [`CHANGELOG.md`](CHANGELOG.md).

---

### Tracker Mini (feature/mini, not device-tested yet)

- The Tidal editor firmware now builds for the Tracker Mini (`make mini`).
  The Mini plays one note at a time per track for now (`# voices` plays as 1).
- Mini controls: Fn1-4 = Pattern / Song / File / Config, Menu = the module
  menu (Shift: Tidal folders), Middle = OK / chord menu (Shift: status),
  +-1 / +-10 = jog. **Shift+Note** turns the screen keys into note entry:
  8 pads of a window, Left / Right move it 1, Up / Down an octave (or 8
  instruments). Hold Fn1 and press a screen key to mute a track.
  Details: docs/tidal_mini.md.
