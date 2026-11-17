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

} // namespace userPlayModes

#endif /* SOURCE_USERPLAYMODES_H_ */
