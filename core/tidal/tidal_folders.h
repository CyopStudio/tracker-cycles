// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// Tidal sample folders (docs/tidal_sample_folders.md, loader step 1): a
// folder /Tidal/<name>/ of WAV files becomes ONE sliced instrument (files
// sorted by name, a slice per file), so `s "<name>:3"` plays its fourth
// file through the mapper's slice rule (n mod slices).
//
// Step 1 is by hand: the editor's Shift+Sample Loader menu lists the
// folders and starts a load. The job runs on the main loop in small steps,
// only while the file manager is idle and the Tidal scheduler is at least
// 150 ms ahead (it queries on the main loop: loading must not cost notes):
//   scan   each file's header (a forgiving WAV parser; refused files and
//          why go to the log)
//   plan   layout (a short silence before each file so each slice point
//          lands before its first sample), slot, room; the slot's memory
//          placed as the stock import does, the instrument blocked
//   write  each file converted to mono 16-bit 44.1 kHz straight into that
//          memory (files already in that format copied)
//   save   slices + Slice play mode, unblocked (playable), then its
//          workspace file written in 8 KB pieces
// The log of every load: /tidal_folders.txt.
#pragma once
#include <stdint.h>

class Platform;

#ifdef TIDAL_EDITOR
// List /Tidal/ again; the folder names after it (without the '/').
int tidalFolderCount(Platform* platform);
const char* tidalFolderName(int i);
// Start loading a folder (false + why when it can't start).
bool tidalFolderLoad(Platform* platform, const char* name, char* msg, int cap);
bool tidalFoldersBusy();
// Main loop: one step of the job.
void tidalFoldersUpdate(Platform* platform);
// A new line for the editor's message line (progress, done, error): true
// once per new message.
bool tidalFoldersMessage(char* msg, int cap, bool& isError);
#endif
