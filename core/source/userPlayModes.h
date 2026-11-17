#ifndef SOURCE_USERPLAYMODES_H_
#define SOURCE_USERPLAYMODES_H_

// User-defined custom play modes: a project's playmodes.txt can replace any of
// the 20 built-in custom play modes (C1-C20) and give their steps any
// track-level FX. Format and behaviour: docs/user_play_modes.md.
//
// The project file is copied to Workspace/playmodes.txt when a project is
// opened and parsed when the project loads from the Workspace. Modes not in
// the file, or that fail to parse, keep their built-in definition.

#include <cstdint>

#include "userPlayModesParser.h"

class FileSystem;

namespace userPlayModes
{

constexpr char kFileName[] = "playmodes.txt";
constexpr char kWorkspaceFilePath[] = "Workspace/playmodes.txt";

// Called once at boot, before the sequencer runs: all modes built-in.
void init();

// Sequencer side (audio timer context): no file access, no allocation.
// mode: 0-19 (C1-C20). index: 0-127.
Step getStep(uint8_t mode, uint8_t index);
// The FX a step's fxOption injects, or nullptr for none.
const FxPreset* getFxPreset(uint8_t mode, uint8_t fxOption);

// File side (main loop, during project open / load / save).
// Parses Workspace/playmodes.txt, or reverts to the built-in modes if there is none.
LoadReport loadFromWorkspace(FileSystem* fileSystem);
// projectDir: "<path>/<name>" of the project.
void copyProjectFileToWorkspace(FileSystem* fileSystem, const char* projectDir);
// incrementalSave: saving over the project that is open. Its file is then
// left as it is, so edits made to it on a computer are not overwritten.
// Otherwise the destination gets the Workspace file, or loses its own.
void copyWorkspaceFileToProject(FileSystem* fileSystem,
                                const char* projectDir,
                                bool incrementalSave);

const LoadReport& getLastLoadReport();

// After a load whose file had errors: returns the number of popup lines
// (once), with a title and the lines. They stay valid until the next load.
// 0 when there is nothing to show.
int takeLoadWarning(const char*& title, const char* const*& lines);

// ---- Play mode editor ----
// Edits go straight into the block the sequencer plays from, so they are heard
// the next time the step plays. Editing a built-in mode first copies it into
// the block. save() makes the edits permanent.

struct StepInfo
{
    uint8_t stepNumber; // 1-128
    uint8_t fxType;     // 0 = none
    int16_t value;
    uint8_t chance; // 0-100
};

bool isUserMode(uint8_t mode);
StepInfo getStepInfo(uint8_t mode, uint8_t index);
// False if the mode could not be made user-defined (the FX preset list is full).
bool setStepNumber(uint8_t mode, uint8_t index, uint8_t stepNumber);
// fxType 0 removes the step's FX. False if the FX preset list is full.
bool setStepFx(uint8_t mode, uint8_t index, uint8_t fxType, int16_t value, uint8_t chance);
// Back to the built-in mode (the user copy is dropped on the next save).
void resetMode(uint8_t mode);

// The FX a play mode may use, in menu order.
int allowedFxCount();
uint8_t allowedFxType(int index);
int allowedFxIndex(uint8_t fxType); // -1 if not allowed
int16_t fxMin(uint8_t fxType);
int16_t fxMax(uint8_t fxType);
int16_t fxDefault(uint8_t fxType);

// Writes Workspace/playmodes.txt (or removes it when no mode is user-defined)
// and mirrors it into projectDir ("<path>/<name>"), if not nullptr.
bool save(FileSystem* fileSystem, const char* projectDir);

} // namespace userPlayModes

#endif /* SOURCE_USERPLAYMODES_H_ */
