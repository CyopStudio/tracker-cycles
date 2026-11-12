// Throwaway on-device bench for Tidal sample folders (only with
// -DFOLDER_BENCH, docs/tidal_sample_folders.md section 7): walks /Tidal/
// the way a folder loader would (list folders, list and sort each folder's
// .wav files, read headers and data, convert to mono 16-bit 44.1 kHz into
// a scratch SDRAM buffer) and writes /tidal_folder_bench.txt. Nothing is
// loaded into instruments. See README.md.
#pragma once
#include <stdint.h>

class Platform;

#ifdef FOLDER_BENCH
// Main loop: schedules the runs (the main loop is blocked while one runs).
void folderBenchUpdate(Platform* platform);
#else
inline void folderBenchUpdate(Platform*) {}
#endif
