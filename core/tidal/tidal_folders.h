// Tidal sample folders (docs/tidal_sample_folders.md, loader step 1): a
// folder /Tidal/<name>/ of WAV files becomes ONE sliced instrument (files
// sorted by name, a slice per file), so `s "<name>:3"` plays its fourth
// file through the mapper's slice rule (n mod slices).
//
// Step 1 is by hand: the editor's Sample Loader menu lists the folders and
// starts a load. The job runs on the main loop in small steps (one chunk
// per pass, only while the file manager is idle):
//   scan   each file's header (refused files and why go to the log)
//   write  the files converted to mono 16-bit 44.1 kHz into one WAV,
//          /Tidal/.cache/<name>.wav, a short silence between files so each
//          slice point lands before its file's first sample
//   import the stock sample import of that WAV into the instrument slot
//          (memory placement, workspace save, instrument named <name>)
//   finish slices, Slice play mode, instrument struct saved
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
