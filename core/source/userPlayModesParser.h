#ifndef SOURCE_USERPLAYMODESPARSER_H_
#define SOURCE_USERPLAYMODESPARSER_H_

// Parser for a project's playmodes.txt, which replaces any of the 20 built-in
// custom play modes (C1-C20). Format: docs/user_play_modes.md.
//
// Kept free of firmware headers so it can be built and tested on a desktop
// compiler. userPlayModes.cpp supplies the FX table and the SD card reading.

#include <cstdint>

namespace userPlayModes
{

constexpr int kModeCount = 20;
constexpr int kStepCount = 128;
constexpr int kMaxPresets = 16;
constexpr int kMaxLineLength = 255;
constexpr int kFormatVersion = 1;

// One entry of a play order. Same layout as strPlayOrderWithEffects.
// stepNumberToPlay: 1-128. fxOption: 0 = none, 1-16 = preset.
struct Step
{
    uint8_t stepNumberToPlay;
    uint8_t fxOption;
};

// FX that a step of a play mode injects into the first free FX slot.
// chance: 0-100 (%). value: raw FX value as stored in a pattern.
struct FxPreset
{
    uint8_t type;
    uint8_t chance;
    int16_t value;
};

// Everything a project's file can change. userModeMask bit n set means mode
// n (C(n+1)) plays from steps[n]; otherwise the built-in mode plays.
struct Block
{
    uint32_t userModeMask;
    FxPreset presets[kMaxPresets]; // preset id p is presets[p - 1]
    Step steps[kModeCount][kStepCount];
};

// An FX that a play mode may use. min/max: allowed raw values.
struct FxSpec
{
    const char* name;
    uint8_t type;
    int16_t min;
    int16_t max;
    bool isRoll;
};

struct ParserConfig
{
    const FxSpec* fx;
    int fxCount;
    // Presets used when the file defines no "fx" lines (the 15 built-in roll
    // variants), so that built-in modes can be copied over unchanged.
    const FxPreset* builtinPresets;
    int builtinPresetCount;
    // Roll value = period + type * rollStride (ROLL_PERIOD_MAX + 1).
    int rollStride;
};

struct LoadReport
{
    uint32_t loadedModes;   // modes now played from the file
    uint32_t rejectedModes; // modes in the file that failed and play built-in
    uint16_t rejectedPresets;
    bool fileRejected; // missing/unknown version: nothing loaded
    int errorCount;
    int firstErrorLine; // 0 if none
};

// Popup telling the user what went wrong: kWarningTitle above up to
// kWarningLineCount lines of kWarningLineSize characters (with the NUL).
constexpr char kWarningTitle[] = "Custom Play Modes (playmodes.txt)";
constexpr int kWarningLineCount = 3;
constexpr int kWarningLineSize = 48;

// Writes the lines for the report and returns how many, or 0 when there is
// nothing to warn about.
int formatLoadWarning(const LoadReport& report, char (&lines)[kWarningLineCount][kWarningLineSize]);

// Feed the file one line at a time (without the line ending) between begin()
// and finish(). Modes are written into the block while its mask is cleared,
// and the mask is written last, so playback never sees a half-written mode.
class Parser
{
  public:
    Parser(Block& block, const ParserConfig& config)
      : block(block)
      , config(config)
    {
    }

    void begin();
    void feedLine(const char* line, int lineNumber);
    // Marks the current line as broken (for example too long to read).
    void lineError(int lineNumber);
    LoadReport finish();

  private:
    void parseVersion(const char* args, int lineNumber);
    void parseFx(const char* args, int lineNumber);
    void parseMode(const char* args, int lineNumber);
    void parseSteps(const char* text, int lineNumber);
    void closeMode();
    void error(int lineNumber);

    Block& block;
    const ParserConfig& config;

    bool versionSeen = false;
    bool fileRejected = false;
    int currentMode = -1;
    int stepCount = 0;
    bool modeBroken = false;
    uint32_t modeUsedPresets = 0;

    uint32_t seenModes = 0;
    uint32_t pendingModes = 0;
    uint32_t brokenModes = 0;
    uint32_t usedPresets[kModeCount] = {};
    uint32_t definedPresets = 0;
    uint32_t brokenPresets = 0;
    bool anyFxLine = false;

    int errorCount = 0;
    int firstErrorLine = 0;
};

} // namespace userPlayModes

#endif /* SOURCE_USERPLAYMODESPARSER_H_ */
