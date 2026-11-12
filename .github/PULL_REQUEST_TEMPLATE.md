## Summary & Goal
<!-- WHY this change: the musical or editing problem it solves. One feature or fix per PR. -->


## Behavior & Changes
<!-- WHAT changed for the player: controls, messages, screen; and the key technical details. -->


## Hardware Testing (Original Tracker)
<!-- Tested on an Original Polyend Tracker? Testers: the "tracker-cycles-firmware" artifact on the Checks tab has a ready-to-flash .ptf. -->
- [ ] Tested on hardware by the author
- [ ] Needs testing by someone with an Original Tracker

## How to Test
<!-- Step-by-step instructions to verify this on the Tracker (or in the prototype): -->
1. 
2. 
3. 

## Checklist
- [ ] I have read `CONTRIBUTING.md`.
- [ ] One feature or fix (no unrelated refactors or formatting sweeps).
- [ ] `make -C tidal test` and `make -C tidal smoke` pass (changed `.expected` files explained above).
- [ ] `make original` builds; flash / RAM / SDRAM use checked.
- [ ] New or changed Tidal functions have golden cases captured from Tidal (`make -C tidal golden`).
- [ ] Nothing allocates, touches the SD card or blocks in the audio interrupt; long main-loop work is done in small steps.
- [ ] Existing `.tidal` files, projects and instruments still load.
