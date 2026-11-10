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

// For the status screen (main loop).
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
int tidalPlayerSounding();             // sample players sounding now
int tidalPlayerCap();                  // the load-driven polyphony cap now

// The device editor (-DTIDAL_EDITOR, plan 4d) owns the program; it plays
// it through these (main loop). Loading plays nothing in that build: the
// file is only read at start-up, for the editor.
namespace tc {
class Program;
}
bool tidalPlayerReadFile(const char*& text, int& len, char* msg, int msgLen);  // /project.tidal
bool tidalPlayerWriteFile(const char* text, int len, char* msg, int msgLen);
bool tidalPlayerEvaluate(const tc::Program& p, int slot, char* msg, int msgLen);  // checked block
void tidalPlayerSilence(int slot);
void tidalPlayerMute(int slot, bool on);
void tidalPlayerUnmuteAll();
void tidalPlayerSetCps(const tc::Program& p);
#else
inline void tidalPlayerUpdate(Platform*) {}
inline void tidalPlayerTick() {}
#endif
