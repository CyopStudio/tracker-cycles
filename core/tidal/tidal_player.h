// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// Tracker Cycles Tidal file player (plan 4a stage 3): the normal firmware's
// music source until the device editor (4d). Once the project's instruments
// are loaded it reads /project.tidal from the card and evaluates every block
// in it (d1-d8 on the sample tracks, d9-d16 MIDI, `setcps`). The Play key
// re-reads the file and re-evaluates it (edit the file on a computer, put
// the card back, press Play); stop / hush silences everything. MIDI clock
// out (24 ppqn, 4 beats per cycle) follows Tidal's cycles when Config's
// clock out is on. Not built into the -DTIDAL_TICK / -DTIDAL_BENCH test
// images, which own the runtime themselves.
#pragma once
#include <stdint.h>

class Platform;

#if !defined(PLATFORM_IMX) && !defined(TIDAL_TICK) && !defined(TIDAL_BENCH)
#define TC_TIDAL_PLAYER 1

// Main loop: start-up, queries, the clock snapshot for MIDI clock out.
void tidalPlayerUpdate(Platform* platform);
// Audio interrupt, once per block (where sequencer.tick runs).
void tidalPlayerTick();

// For the editor's status view (main loop).
constexpr const char* kTidalPlayerFile = "/project.tidal";
bool tidalPlayerReload();  // read the file and evaluate it; false on a file / parse error
void tidalPlayerHush();
bool tidalPlayerStarted();             // the runtime is up (project loaded)
bool tidalPlayerAnyPlaying();
const char* tidalPlayerMessage();      // last file / parse / check message ("" = fine)
bool tidalPlayerSlotPlaying(int slot);
const char* tidalPlayerSlotError(int slot);  // evaluate / Tracker-mode error, or nullptr
const char* tidalPlayerSlotWarning(int slot, char* buf, int len);  // mapping warnings, or nullptr
float tidalPlayerCps();
int tidalPlayerAudioLoad();            // smoothed audio load, %
float tidalPlayerAheadSeconds();       // music queued beyond now (least over playing slots; large if none)
int tidalPlayerSounding();             // sample players sounding now
int tidalPlayerCap();                  // the load-driven polyphony cap now

// The device editor (-DTIDAL_EDITOR, plan 4d) owns the program; it plays
// it through these (main loop). Loading plays nothing in that build: the
// file is only read at start-up, for the editor.
namespace tc {
class Program;
struct LitRecord;
}
bool tidalPlayerReadFile(const char*& text, int& len, char* msg, int msgLen);  // /project.tidal
bool tidalPlayerReadText(const char* path, const char*& text, int& len, char* msg, int msgLen);  // any file
bool tidalPlayerWriteFile(const char* text, int len, char* msg, int msgLen);
bool tidalPlayerWriteText(const char* path, const char* text, int len, char* msg, int msgLen);  // any file
// checked block; liveRoot: its copy's root in the live program (highlighting)
bool tidalPlayerEvaluate(const tc::Program& p, int slot, char* msg, int msgLen, uint16_t* liveRoot = nullptr);
const tc::Program* tidalPlayerLive();  // the program the scheduler plays (live node ids)
// Steps of played entries since the last call (TC_EVENT_SRC builds; live node
// ids); `nowSample` gets the scheduler's current sample position, the clock
// their onsets are on. Returns how many records were written.
int tidalPlayerTakeLit(tc::LitRecord* out, int cap, int64_t& nowSample);
constexpr double kTidalSampleRate = 750000.0 / 17.0;
void tidalPlayerSilence(int slot);
void tidalPlayerMute(int slot, bool on);
void tidalPlayerUnmuteAll();
void tidalPlayerSetCps(const tc::Program& p);
#else
inline void tidalPlayerUpdate(Platform*) {}
inline void tidalPlayerTick() {}
#endif
