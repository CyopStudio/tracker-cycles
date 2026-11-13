// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// Throwaway on-device editor timing build (plan 4d,
// docs/tidal_device_editor.md section 4). Only with -DTIDAL_EDITOR_BENCH.
#pragma once

class Platform;

#ifdef TIDAL_EDITOR_BENCH
// Main loop. Run 1 at 10 s after boot with the Tidal player hushed, run 2
// at 40 s with /project.tidal playing (reloaded at 30 s); popups before and
// after; report in /tidal_editor_bench.txt.
void tidalEditorBenchUpdate(Platform* platform);
#else
inline void tidalEditorBenchUpdate(Platform*) {}
#endif
