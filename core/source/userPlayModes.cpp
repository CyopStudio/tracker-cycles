#include "userPlayModes.h"

#include <cstdio>
#include <cstring>

#include "Platform.h"
#include "SdTextFile.h"
#include "config.h"
#include "cr_section_macros.h"
#include "fileTransfer.h"
#include "mtSequencer.h"
#include "mtStructs.h"
#include "playModeDefs.h"

namespace userPlayModes
{

namespace
{

using Fx = Sequencer::strFxConsts;

static_assert(kModeCount == playOrdersTotal, "one user slot per built-in mode");
static_assert(kStepCount == playOrderWithFxLength, "same length as the built-in modes");
static_assert(sizeof(Step) == sizeof(strPlayOrderWithEffects), "same step layout");
static_assert(Fx::ROLL_PERIOD_1_1 == 1 && Fx::ROLL_PERIOD_1_16 == 8,
              "parser's roll rate list follows enRollPeriod");
static_assert(Fx::rollType_const == 0 && Fx::rollType_noteRandom == 5,
              "parser's roll type list follows enRollDir");
static_assert(sizeof(Block) <= USER_PLAY_MODES_MEMORY_SIZE,
              "USER_PLAY_MODES_MEMORY_SIZE (mtStructs.h) must hold the block");

// SDRAM. On Tracker+ / Mini this space is taken out of SAMPLE_MEMORY_SIZE, so
// EXTERNAL_RAM headroom does not change (mtStructs.h).
__NOINIT(EXTERNAL_RAM) Block userBlock;

__NOINIT(EXTERNAL_RAM) LoadReport lastReport;

// Popup text, kept here because the popup only stores the pointers.
__NOINIT(EXTERNAL_RAM) char warningLines[kWarningLineCount][kWarningLineSize];
__NOINIT(EXTERNAL_RAM) const char* warningLinePointers[kWarningLineCount];
__NOINIT(EXTERNAL_RAM) int warningLineCount;

constexpr int kRollStride = Fx::ROLL_PERIOD_MAX + 1;

constexpr int16_t rollValue(int period, int type)
{
    return static_cast<int16_t>(period + type * kRollStride);
}

// The 15 built-in roll variants, in playOrderFxOptions order (fxOption 1-15).
const FxPreset kBuiltinPresets[] = {
    { Fx::FX_TYPE_ROLL, 30, rollValue(Fx::ROLL_PERIOD_1_4, Fx::rollType_const) },
    { Fx::FX_TYPE_ROLL, 60, rollValue(Fx::ROLL_PERIOD_1_4, Fx::rollType_const) },
    { Fx::FX_TYPE_ROLL, 100, rollValue(Fx::ROLL_PERIOD_1_4, Fx::rollType_const) },

    { Fx::FX_TYPE_ROLL, 30, rollValue(Fx::ROLL_PERIOD_1_4, Fx::rollType_volDown) },
    { Fx::FX_TYPE_ROLL, 60, rollValue(Fx::ROLL_PERIOD_1_4, Fx::rollType_volDown) },
    { Fx::FX_TYPE_ROLL, 100, rollValue(Fx::ROLL_PERIOD_1_4, Fx::rollType_volDown) },

    { Fx::FX_TYPE_ROLL, 30, rollValue(Fx::ROLL_PERIOD_1_2, Fx::rollType_volDown) },
    { Fx::FX_TYPE_ROLL, 60, rollValue(Fx::ROLL_PERIOD_1_2, Fx::rollType_volDown) },
    { Fx::FX_TYPE_ROLL, 100, rollValue(Fx::ROLL_PERIOD_1_2, Fx::rollType_volDown) },

    { Fx::FX_TYPE_ROLL, 30, rollValue(Fx::ROLL_PERIOD_1_1, Fx::rollType_const) },
    { Fx::FX_TYPE_ROLL, 60, rollValue(Fx::ROLL_PERIOD_1_1, Fx::rollType_const) },
    { Fx::FX_TYPE_ROLL, 100, rollValue(Fx::ROLL_PERIOD_1_1, Fx::rollType_const) },

    { Fx::FX_TYPE_ROLL, 30, rollValue(Fx::ROLL_PERIOD_1_1, Fx::rollType_volDown) },
    { Fx::FX_TYPE_ROLL, 60, rollValue(Fx::ROLL_PERIOD_1_1, Fx::rollType_volDown) },
    { Fx::FX_TYPE_ROLL, 100, rollValue(Fx::ROLL_PERIOD_1_1, Fx::rollType_volDown) },
};
constexpr int kBuiltinPresetCount = sizeof(kBuiltinPresets) / sizeof(kBuiltinPresets[0]);
static_assert(kBuiltinPresetCount == playModeFx_rollDecVelo_1_1_P100,
              "one preset per playOrderFxOptions value after playModeFx_none");

struct AllowedFx
{
    const char* name;
    uint8_t type;
};

// FX a play mode may inject: those that act on their own track or note.
// Not allowed: Tempo and Swing (whole project), Break pattern (would upset the
// play mode's step count), Off, Random FX value and MIDI Chord.
const AllowedFx kAllowedFx[] = {
    { "volume", Fx::FX_TYPE_VOLUME },
    { "panning", Fx::FX_TYPE_PANNING },
    { "microtune", Fx::FX_TYPE_MICROTUNING },
    { "glide", Fx::FX_TYPE_GLIDE },
    { "micromove", Fx::FX_TYPE_MICROMOVE },
    { "gate", Fx::FX_TYPE_GATE_LENGTH },
    { "chance", Fx::FX_TYPE_CHANCE },
    { "roll", Fx::FX_TYPE_ROLL },
    { "arp", Fx::FX_TYPE_ARP },
    { "randomnote", Fx::FX_TYPE_RANDOM_NOTE },
    { "randominstrument", Fx::FX_TYPE_RANDOM_INSTRUMENT },
    { "randomvolume", Fx::FX_TYPE_RANDOM_VOLUME },
    { "reverse", Fx::FX_TYPE_REVERSE_PLAYBACK },
    { "position", Fx::FX_TYPE_POSITION },
    { "slice", Fx::FX_TYPE_SAMPLE_SLICE },
    { "volumelfo", Fx::FX_TYPE_VOLUME_LFO },
    { "panninglfo", Fx::FX_TYPE_PANNING_LFO },
    { "filterlfo", Fx::FX_TYPE_FILTER_LFO },
    { "positionlfo", Fx::FX_TYPE_POSITION_LFO },
    { "finetunelfo", Fx::FX_TYPE_FINETUNE_LFO },
    { "overdrive", Fx::FX_TYPE_OVERDRIVE },
    { "lowpass", Fx::FX_TYPE_FILTER_LOWPASS },
    { "bandpass", Fx::FX_TYPE_FILTER_BANDPASS },
    { "highpass", Fx::FX_TYPE_FILTER_HIGHPASS },
    { "delay", Fx::FX_TYPE_DELAY_SEND },
    { "reverb", Fx::FX_TYPE_REVERB_SEND },
    { "bitdepth", Fx::FX_TYPE_BIT_DEPTH },
    { "tune", Fx::FX_TYPE_TUNE },
    { "slideup", Fx::FX_TYPE_SLIDE_UP },
    { "slidedown", Fx::FX_TYPE_SLIDE_DOWN },
    { "ampenv", Fx::FX_TYPE_AMP_ENV },
    { "filterenv", Fx::FX_TYPE_FILTER_ENV },
    { "granularlength", Fx::FX_TYPE_GRANULAR_LENGTH },
    { "cca", Fx::FX_TYPE_SEND_CC_A },
    { "ccb", Fx::FX_TYPE_SEND_CC_B },
    { "ccc", Fx::FX_TYPE_SEND_CC_C },
    { "ccd", Fx::FX_TYPE_SEND_CC_D },
    { "cce", Fx::FX_TYPE_SEND_CC_E },
    { "ccf", Fx::FX_TYPE_SEND_CC_F },
};
constexpr int kAllowedFxCount = sizeof(kAllowedFx) / sizeof(kAllowedFx[0]);

constexpr size_t kMaxFileSize = 64 * 1024;
constexpr int kReadChunkSize = 128;

bool isModeUserDefined(uint8_t mode)
{
    return (__atomic_load_n(&userBlock.userModeMask, __ATOMIC_ACQUIRE) >> mode) & 1u;
}

void projectFilePath(char* out, size_t outSize, const char* projectDir)
{
    snprintf(out, outSize, "%s/%s", projectDir, kFileName);
}

// Reads the open file line by line into the parser.
void parseFile(FileSystem* fileSystem, FileSystem::FileHandle file, Parser& parser)
{
    char chunk[kReadChunkSize];
    char line[kMaxLineLength + 1];
    int lineLength = 0;
    bool lineTooLong = false;
    int lineNumber = 1;

    auto endLine = [&]()
    {
        line[lineLength] = 0;
        if (lineTooLong)
        {
            parser.lineError(lineNumber);
        }
        else
        {
            parser.feedLine(line, lineNumber);
        }
        lineLength = 0;
        lineTooLong = false;
        lineNumber++;
    };

    while (true)
    {
        const int32_t count = fileSystem->readFile(chunk, sizeof(chunk), file);
        if (count <= 0)
        {
            break;
        }

        for (int32_t i = 0; i < count; i++)
        {
            const char c = chunk[i];
            if (c == '\n')
            {
                endLine();
            }
            else if (lineLength < kMaxLineLength)
            {
                // A NUL would cut the line short: treat it as a broken line.
                line[lineLength++] = (c == 0) ? '\x01' : c;
            }
            else
            {
                lineTooLong = true;
            }
        }
    }

    if (lineLength > 0 || lineTooLong)
    {
        endLine();
    }
}

// The parser's view of the allowed FX, with today's value ranges.
ParserConfig makeConfig(FxSpec (&fxSpecs)[kAllowedFxCount])
{
    for (int i = 0; i < kAllowedFxCount; i++)
    {
        const uint8_t type = kAllowedFx[i].type;
        fxSpecs[i].name = kAllowedFx[i].name;
        fxSpecs[i].type = type;
        fxSpecs[i].min = sequencer.getFxMin(type);
        fxSpecs[i].max = sequencer.getFxMax(type);
        fxSpecs[i].isRoll = (type == Fx::FX_TYPE_ROLL);
        if (fxSpecs[i].isRoll)
        {
            // Roll's lowest value means "no roll"; a preset always rolls.
            fxSpecs[i].min = Fx::ROLL_PERIOD_1_1;
        }
    }
    return ParserConfig{
        fxSpecs, kAllowedFxCount, kBuiltinPresets, kBuiltinPresetCount, kRollStride
    };
}

struct FileSinkContext
{
    FileSystem* fileSystem;
    FileSystem::FileHandle file;
    bool ok;
};

void writeLineToFile(void* context, const char* line)
{
    FileSinkContext* sink = static_cast<FileSinkContext*>(context);
    const size_t length = strlen(line);
    if (sink->fileSystem->writeFile(line, length, sink->file) != static_cast<int32_t>(length) ||
        sink->fileSystem->writeFile("\n", 1, sink->file) != 1)
    {
        sink->ok = false;
    }
}

// Makes `mode` user-defined (a copy of its built-in steps) if it is not yet.
bool ensureUserMode(uint8_t mode)
{
    if (isModeUserDefined(mode))
    {
        return true;
    }
    Step steps[kStepCount];
    for (int i = 0; i < kStepCount; i++)
    {
        steps[i] =
          Step{ playOrderWithFx[mode][i].stepNumberToPlay, playOrderWithFx[mode][i].fxOption };
    }
    return copyBuiltinMode(userBlock, mode, steps, kBuiltinPresets, kBuiltinPresetCount);
}

} // namespace

void init()
{
    __atomic_store_n(&userBlock.userModeMask, 0u, __ATOMIC_RELEASE);
    lastReport = {};
    warningLineCount = 0;
}

AUDIO_CORE_FLASH Step getStep(uint8_t mode, uint8_t index)
{
    if (isModeUserDefined(mode))
    {
        return userBlock.steps[mode][index];
    }
    const strPlayOrderWithEffects& entry = playOrderWithFx[mode][index];
    return Step{ entry.stepNumberToPlay, entry.fxOption };
}

AUDIO_CORE_FLASH const FxPreset* getFxPreset(uint8_t mode, uint8_t fxOption)
{
    if (fxOption == 0)
    {
        return nullptr;
    }

    if (isModeUserDefined(mode))
    {
        return (fxOption <= kMaxPresets) ? &userBlock.presets[fxOption - 1] : nullptr;
    }
    return (fxOption <= kBuiltinPresetCount) ? &kBuiltinPresets[fxOption - 1] : nullptr;
}

LoadReport loadFromWorkspace(FileSystem* fileSystem)
{
    FxSpec fxSpecs[kAllowedFxCount];
    const ParserConfig config = makeConfig(fxSpecs);
    Parser parser(userBlock, config);
    parser.begin();

    if (!fileSystem->exists(kWorkspaceFilePath))
    {
        lastReport = parser.finish();
        lastReport.fileRejected = false; // no file is not an error
        warningLineCount = 0;
        return lastReport;
    }

    // Streamed in small chunks (SdTextFile::readAll() would need a buffer for
    // the whole file).
    SdScopedFile file(fileSystem, kWorkspaceFilePath, FileSystem::kRead);
    if (file && fileSystem->getFileSize(file.handle()) <= kMaxFileSize)
    {
        parseFile(fileSystem, file.handle(), parser);
    }

    // A file that could not be read has no version line and is rejected.
    lastReport = parser.finish();
    warningLineCount = formatLoadWarning(lastReport, warningLines);
    return lastReport;
}

void copyProjectFileToWorkspace(FileSystem* fileSystem, const char* projectDir)
{
    char path[PATCH_SIZE + sizeof(kFileName) + 1];
    projectFilePath(path, sizeof(path), projectDir);

    if (fileSystem->exists(path))
    {
        cFileTransfer::copyFile(fileSystem, path, kWorkspaceFilePath);
    }
    else if (fileSystem->exists(kWorkspaceFilePath))
    {
        fileSystem->remove(kWorkspaceFilePath);
    }
}

void copyWorkspaceFileToProject(FileSystem* fileSystem,
                                const char* projectDir,
                                bool incrementalSave)
{
    if (incrementalSave)
    {
        return;
    }

    char path[PATCH_SIZE + sizeof(kFileName) + 1];
    projectFilePath(path, sizeof(path), projectDir);

    if (fileSystem->exists(kWorkspaceFilePath))
    {
        cFileTransfer::copyFile(fileSystem, kWorkspaceFilePath, path);
    }
    else if (fileSystem->exists(path))
    {
        fileSystem->remove(path);
    }
}

const LoadReport& getLastLoadReport()
{
    return lastReport;
}

int takeLoadWarning(const char*& title, const char* const*& lines)
{
    const int count = warningLineCount;
    warningLineCount = 0;
    if (count == 0)
    {
        return 0;
    }

    for (int i = 0; i < count; i++)
    {
        warningLinePointers[i] = warningLines[i];
    }
    title = kWarningTitle;
    lines = warningLinePointers;
    return count;
}

// ---- Play mode editor ----

bool isUserMode(uint8_t mode)
{
    return mode < kModeCount && isModeUserDefined(mode);
}

StepInfo getStepInfo(uint8_t mode, uint8_t index)
{
    const Step step = getStep(mode, index);
    const FxPreset* preset = getFxPreset(mode, step.fxOption);
    StepInfo info = { step.stepNumberToPlay, 0, 0, 100 };
    if (preset != nullptr)
    {
        info.fxType = preset->type;
        info.value = preset->value;
        info.chance = preset->chance;
    }
    return info;
}

bool setStepNumber(uint8_t mode, uint8_t index, uint8_t stepNumber)
{
    if (mode >= kModeCount || index >= kStepCount || stepNumber < 1 || stepNumber > kStepCount ||
        !ensureUserMode(mode))
    {
        return false;
    }
    userBlock.steps[mode][index].stepNumberToPlay = stepNumber;
    return true;
}

bool setStepFx(uint8_t mode, uint8_t index, uint8_t fxType, int16_t value, uint8_t chance)
{
    if (mode >= kModeCount || index >= kStepCount || !ensureUserMode(mode))
    {
        return false;
    }

    uint8_t id = 0;
    if (fxType != 0)
    {
        const FxPreset preset = { fxType, chance > 100 ? uint8_t(100) : chance, value };
        const int found = findOrAddPreset(userBlock, preset, userBlock.userModeMask);
        if (found == 0)
        {
            return false;
        }
        id = static_cast<uint8_t>(found);
    }
    userBlock.steps[mode][index].fxOption = id;
    return true;
}

void resetMode(uint8_t mode)
{
    if (mode < kModeCount)
    {
        __atomic_and_fetch(&userBlock.userModeMask, ~(1u << mode), __ATOMIC_RELEASE);
    }
}

int allowedFxCount()
{
    return kAllowedFxCount;
}

uint8_t allowedFxType(int index)
{
    return (index >= 0 && index < kAllowedFxCount) ? kAllowedFx[index].type : 0;
}

int allowedFxIndex(uint8_t fxType)
{
    for (int i = 0; i < kAllowedFxCount; i++)
    {
        if (kAllowedFx[i].type == fxType)
        {
            return i;
        }
    }
    return -1;
}

int16_t fxMin(uint8_t fxType)
{
    return fxType == Fx::FX_TYPE_ROLL ? int16_t(Fx::ROLL_PERIOD_1_1) : sequencer.getFxMin(fxType);
}

int16_t fxMax(uint8_t fxType)
{
    return sequencer.getFxMax(fxType);
}

int16_t fxDefault(uint8_t fxType)
{
    const int16_t value = sequencer.getFxDefault(fxType);
    const int16_t low = fxMin(fxType);
    const int16_t high = fxMax(fxType);
    return value < low ? low : (value > high ? high : value);
}

bool save(FileSystem* fileSystem, const char* projectDir)
{
    bool ok = true;
    if (userBlock.userModeMask == 0)
    {
        if (fileSystem->exists(kWorkspaceFilePath))
        {
            ok = fileSystem->remove(kWorkspaceFilePath);
        }
    }
    else
    {
        SdScopedFile file(fileSystem, kWorkspaceFilePath, FileSystem::kWriteCreateNew);
        if (!file)
        {
            return false;
        }
        FxSpec fxSpecs[kAllowedFxCount];
        const ParserConfig config = makeConfig(fxSpecs);
        FileSinkContext context = { fileSystem, file.handle(), true };
        writeFile(userBlock, config, writeLineToFile, &context);
        ok = context.ok;
    }

    if (ok && projectDir != nullptr)
    {
        // Mirror the Workspace file into the project, or remove the project's.
        copyWorkspaceFileToProject(fileSystem, projectDir, false);
    }
    return ok;
}

} // namespace userPlayModes
