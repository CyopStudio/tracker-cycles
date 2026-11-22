# Upcoming Changes (Unreleased)

Changes merged since the latest release. They are not in the release download yet; the build workflow publishes test builds for every push (see the Actions tab).

For what is in the current release, see [`CHANGELOG.md`](CHANGELOG.md).

---

### USB storage and USB audio out (from Tracker CE)

* **USB storage** (Config > USB > Storage): mount the SD card on your computer over USB to copy samples, projects and `.tidal` files. While it is mounted the Tracker hushes, writes your code to the card first, and holds autosave and sample-folder loading until you end the mode. The code stays in the editor. The mode can't start while a sample folder is loading.
* **USB audio out** (Config > USB > Audio: `Off` / `Output (2 out)`): the master mix as a 44.1 kHz / 16-bit stereo USB audio device, to record a set into a DAW. Connect the Tracker directly to the computer (no hub). USB audio input isn't offered (the sample recorder it fed was removed). On a busy set, streaming adds audio-interrupt load.
* Ported from Tracker CE (USB storage and audio for the Original Tracker, and its Windows mount fix); stereo samples from the same CE change are not included.
