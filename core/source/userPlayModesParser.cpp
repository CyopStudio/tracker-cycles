#include "userPlayModesParser.h"

#include <cstdio>
#include <cstring>

namespace userPlayModes
{

namespace
{

constexpr int kMaxTokenLength = 31;

// Roll rates in ROLL_PERIOD order: index + 1 is the period value.
const char* const kRollRates[] = { "1/1", "1/2", "1/3", "1/4", "1/6", "1/8", "1/12", "1/16" };
constexpr int kRollRateCount = sizeof(kRollRates) / sizeof(kRollRates[0]);

// Roll types in rollType order (rollType_const ... rollType_noteRandom).
const char* const kRollTypes[] = { "flat", "down", "up", "notedown", "noteup", "noterandom" };
constexpr int kRollTypeCount = sizeof(kRollTypes) / sizeof(kRollTypes[0]);

// Preset id p (1-64) is bit p - 1 of a uint64_t. Bit 64 does not exist, so
// ids are kept 1-based in the API and shifted here.
uint64_t presetBit(int id)
{
    return uint64_t(1) << (id - 1);
}

char toLower(char c)
{
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

bool isSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\r';
}

bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

bool equalsIgnoreCase(const char* a, const char* b)
{
    while (*a && *b)
    {
        if (toLower(*a++) != toLower(*b++))
        {
            return false;
        }
    }
    return *a == *b;
}

// Copies the next whitespace-separated token into out and advances text.
// Returns false at the end of the text. Tokens longer than out are reported
// as a single over-long token so they fail to parse.
bool nextToken(const char*& text, char (&out)[kMaxTokenLength + 1])
{
    while (isSpace(*text))
    {
        text++;
    }
    if (*text == 0)
    {
        return false;
    }

    int length = 0;
    bool overflow = false;
    while (*text && !isSpace(*text))
    {
        if (length < kMaxTokenLength)
        {
            out[length++] = *text;
        }
        else
        {
            overflow = true;
        }
        text++;
    }
    out[length] = 0;
    if (overflow)
    {
        out[0] = '?';
    }
    return true;
}

// Parses a whole token as a decimal integer with an optional sign and an
// optional single trailing character (allowedSuffix, 0 for none).
bool parseInt(const char* token, int32_t& out, char allowedSuffix = 0)
{
    bool negative = false;
    if (*token == '-' || *token == '+')
    {
        negative = (*token == '-');
        token++;
    }
    if (!isDigit(*token))
    {
        return false;
    }

    int32_t value = 0;
    while (isDigit(*token))
    {
        value = value * 10 + (*token - '0');
        if (value > 100000)
        {
            return false;
        }
        token++;
    }
    if (*token != 0 && !(allowedSuffix != 0 && *token == allowedSuffix && token[1] == 0))
    {
        return false;
    }

    out = negative ? -value : value;
    return true;
}

int findIndex(const char* const* list, int count, const char* token)
{
    for (int i = 0; i < count; i++)
    {
        if (equalsIgnoreCase(list[i], token))
        {
            return i;
        }
    }
    return -1;
}

} // namespace

void Parser::begin()
{
    // Built-in modes play while the file is read.
    __atomic_store_n(&block.userModeMask, 0u, __ATOMIC_RELEASE);

    versionSeen = false;
    fileRejected = false;
    currentMode = -1;
    stepCount = 0;
    modeBroken = false;
    modeUsedPresets = 0;
    seenModes = 0;
    pendingModes = 0;
    brokenModes = 0;
    memset(usedPresets, 0, sizeof(usedPresets));
    definedPresets = 0;
    brokenPresets = 0;
    anyFxLine = false;
    errorCount = 0;
    firstErrorLine = 0;
}

void Parser::error(int lineNumber)
{
    errorCount++;
    if (firstErrorLine == 0)
    {
        firstErrorLine = lineNumber;
    }
}

void Parser::lineError(int lineNumber)
{
    error(lineNumber);
    if (currentMode >= 0)
    {
        modeBroken = true;
    }
}

void Parser::feedLine(const char* line, int lineNumber)
{
    if (fileRejected)
    {
        return;
    }

    char text[kMaxLineLength + 1];
    size_t length = strlen(line);
    if (length > static_cast<size_t>(kMaxLineLength))
    {
        lineError(lineNumber);
        return;
    }
    memcpy(text, line, length + 1);

    char* comment = strchr(text, '#');
    if (comment != nullptr)
    {
        *comment = 0;
    }

    const char* cursor = text;
    while (isSpace(*cursor))
    {
        cursor++;
    }
    if (*cursor == 0)
    {
        return;
    }

    if (isDigit(*cursor))
    {
        if (!versionSeen)
        {
            fileRejected = true;
            error(lineNumber);
            return;
        }
        parseSteps(cursor, lineNumber);
        return;
    }

    char keyword[kMaxTokenLength + 1];
    nextToken(cursor, keyword);

    if (!versionSeen)
    {
        if (equalsIgnoreCase(keyword, "version"))
        {
            parseVersion(cursor, lineNumber);
        }
        else
        {
            fileRejected = true;
            error(lineNumber);
        }
        return;
    }

    if (equalsIgnoreCase(keyword, "fx"))
    {
        parseFx(cursor, lineNumber);
    }
    else if (equalsIgnoreCase(keyword, "mode"))
    {
        parseMode(cursor, lineNumber);
    }
    else
    {
        lineError(lineNumber);
    }
}

void Parser::parseVersion(const char* args, int lineNumber)
{
    char token[kMaxTokenLength + 1];
    int32_t version = 0;
    if (!nextToken(args, token) || !parseInt(token, version) || version != kFormatVersion ||
        nextToken(args, token))
    {
        fileRejected = true;
        error(lineNumber);
        return;
    }
    versionSeen = true;
}

void Parser::parseFx(const char* args, int lineNumber)
{
    closeMode();
    anyFxLine = true;

    char token[kMaxTokenLength + 1];
    int32_t id = 0;
    if (!nextToken(args, token) || !parseInt(token, id) || id < 1 || id > kMaxPresets)
    {
        error(lineNumber);
        return;
    }

    const uint64_t bit = presetBit(id);
    if ((definedPresets | brokenPresets) & bit)
    {
        // Defined twice: neither definition is trusted.
        definedPresets &= ~bit;
        brokenPresets |= bit;
        error(lineNumber);
        return;
    }

    bool ok = nextToken(args, token);
    const FxSpec* spec = nullptr;
    for (int i = 0; ok && i < config.fxCount; i++)
    {
        if (equalsIgnoreCase(config.fx[i].name, token))
        {
            spec = &config.fx[i];
        }
    }
    ok = ok && (spec != nullptr);

    int32_t value = 0;
    if (ok && spec->isRoll)
    {
        // roll <rate> [type]
        ok = nextToken(args, token);
        const int rate = ok ? findIndex(kRollRates, kRollRateCount, token) : -1;
        ok = ok && rate >= 0;
        int type = 0;
        const char* afterRate = args;
        if (ok && nextToken(args, token) && !isDigit(token[0]))
        {
            type = findIndex(kRollTypes, kRollTypeCount, token);
            ok = type >= 0;
        }
        else
        {
            args = afterRate;
        }
        value = (rate + 1) + type * config.rollStride;
    }
    else if (ok)
    {
        ok = nextToken(args, token) && parseInt(token, value);
    }
    ok = ok && value >= spec->min && value <= spec->max;

    int32_t chance = 100;
    if (ok && nextToken(args, token))
    {
        ok = parseInt(token, chance, '%') && chance >= 0 && chance <= 100;
    }
    ok = ok && !nextToken(args, token);

    if (!ok)
    {
        brokenPresets |= bit;
        error(lineNumber);
        return;
    }

    FxPreset& preset = block.presets[id - 1];
    preset.type = spec->type;
    preset.chance = static_cast<uint8_t>(chance);
    preset.value = static_cast<int16_t>(value);
    definedPresets |= bit;
}

void Parser::parseMode(const char* args, int lineNumber)
{
    closeMode();

    char token[kMaxTokenLength + 1];
    int32_t mode = 0;
    if (!nextToken(args, token) || !parseInt(token, mode) || mode < 1 || mode > kModeCount)
    {
        // Ignore the steps of a mode we cannot identify.
        currentMode = -2;
        error(lineNumber);
        return;
    }

    const int index = mode - 1;
    const uint32_t bit = 1u << index;
    if (seenModes & bit)
    {
        // Defined twice: keep the built-in mode and ignore these steps.
        brokenModes |= bit;
        currentMode = -2;
        error(lineNumber);
        return;
    }
    seenModes |= bit;

    currentMode = index;
    stepCount = 0;
    modeBroken = false;
    modeUsedPresets = 0;

    if (nextToken(args, token))
    {
        lineError(lineNumber);
    }
}

void Parser::parseSteps(const char* text, int lineNumber)
{
    if (currentMode == -2)
    {
        return;
    }
    if (currentMode < 0)
    {
        error(lineNumber);
        return;
    }

    char token[kMaxTokenLength + 1];
    bool lineOk = true;
    while (nextToken(text, token))
    {
        int32_t step = 0;
        int32_t preset = 0;
        char* colon = strchr(token, ':');
        if (colon != nullptr)
        {
            *colon = 0;
        }

        bool ok = parseInt(token, step) && step >= 1 && step <= kStepCount;
        if (ok && colon != nullptr)
        {
            ok = parseInt(colon + 1, preset) && preset >= 1 && preset <= kMaxPresets;
        }
        if (ok && stepCount >= kStepCount)
        {
            ok = false;
        }

        if (!ok)
        {
            lineOk = false;
            continue;
        }

        Step& entry = block.steps[currentMode][stepCount++];
        entry.stepNumberToPlay = static_cast<uint8_t>(step);
        entry.fxOption = static_cast<uint8_t>(preset);
        if (preset != 0)
        {
            modeUsedPresets |= presetBit(preset);
        }
    }

    if (!lineOk)
    {
        lineError(lineNumber);
    }
}

void Parser::closeMode()
{
    if (currentMode < 0)
    {
        currentMode = -1;
        return;
    }

    const uint32_t bit = 1u << currentMode;
    if (modeBroken || stepCount == 0)
    {
        if (stepCount == 0 && !modeBroken)
        {
            error(0);
        }
        brokenModes |= bit;
    }
    else
    {
        // A short mode repeats to fill all 128 steps.
        Step* steps = block.steps[currentMode];
        for (int i = stepCount; i < kStepCount; i++)
        {
            steps[i] = steps[i % stepCount];
        }
        pendingModes |= bit;
        usedPresets[currentMode] = modeUsedPresets;
    }

    currentMode = -1;
}

LoadReport Parser::finish()
{
    closeMode();

    LoadReport report = {};

    if (!versionSeen)
    {
        fileRejected = true;
    }

    if (!fileRejected && !anyFxLine)
    {
        const int count =
          config.builtinPresetCount < kMaxPresets ? config.builtinPresetCount : kMaxPresets;
        for (int i = 0; i < count; i++)
        {
            block.presets[i] = config.builtinPresets[i];
            definedPresets |= presetBit(i + 1);
        }
    }

    uint32_t loaded = 0;
    if (!fileRejected)
    {
        const uint64_t validPresets = definedPresets & ~brokenPresets;
        for (int mode = 0; mode < kModeCount; mode++)
        {
            const uint32_t bit = 1u << mode;
            if ((pendingModes & bit) && !(brokenModes & bit) &&
                (usedPresets[mode] & ~validPresets) == 0)
            {
                loaded |= bit;
            }
        }
    }

    // Publish last: the sequencer reads the mask before any step.
    __atomic_store_n(&block.userModeMask, loaded, __ATOMIC_RELEASE);

    report.loadedModes = loaded;
    report.rejectedModes = fileRejected ? 0 : ((seenModes | brokenModes) & ~loaded);
    report.rejectedPresets = brokenPresets;
    report.fileRejected = fileRejected;
    report.errorCount = errorCount;
    report.firstErrorLine = firstErrorLine;
    return report;
}

namespace
{

// "C3", "C3 and C4", "C3, C4 and C7", "C1, C2, C3 and 17 more".
void formatModeList(uint32_t modes, char* out, int outSize)
{
    constexpr int kMaxListed = 3;
    int total = 0;
    for (int mode = 0; mode < kModeCount; mode++)
    {
        total += (modes >> mode) & 1u;
    }

    const int listed = total > kMaxListed ? kMaxListed : total;
    int length = 0;
    int written = 0;
    out[0] = 0;
    for (int mode = 0; mode < kModeCount && written < listed; mode++)
    {
        if (!((modes >> mode) & 1u))
        {
            continue;
        }
        const char* separator = "";
        if (written > 0)
        {
            separator = (written == listed - 1 && total == listed) ? " and " : ", ";
        }
        if (length < outSize)
        {
            length += snprintf(out + length, outSize - length, "%sC%d", separator, mode + 1);
        }
        written++;
    }
    if (total > listed && length < outSize)
    {
        snprintf(out + length, outSize - length, " and %d more", total - listed);
    }
}

} // namespace

int formatLoadWarning(const LoadReport& report, char (&lines)[kWarningLineCount][kWarningLineSize])
{
    if (report.fileRejected)
    {
        snprintf(lines[0], kWarningLineSize, "The file was not loaded:");
        if (report.firstErrorLine > 0)
        {
            // Only a missing or wrong version line rejects a readable file.
            snprintf(lines[1], kWarningLineSize, "its first line must be: version 1");
        }
        else
        {
            snprintf(lines[1], kWarningLineSize, "it is empty, unreadable or over 64 KB.");
        }
        snprintf(lines[2], kWarningLineSize, "All modes play their built-in version.");
        return 3;
    }

    if (report.errorCount == 0 && report.rejectedModes == 0)
    {
        return 0;
    }

    int count = 0;
    if (report.rejectedModes != 0)
    {
        // Longest list: "C10, C11, C12 and 17 more" (25 characters).
        char modes[28];
        formatModeList(report.rejectedModes, modes, sizeof(modes));
        const bool one = (report.rejectedModes & (report.rejectedModes - 1)) == 0;
        snprintf(lines[count++],
                 kWarningLineSize,
                 "%s %s %s errors.",
                 one ? "Mode" : "Modes",
                 modes,
                 one ? "has" : "have");
        snprintf(lines[count++],
                 kWarningLineSize,
                 one ? "It plays its built-in version." : "They play their built-in versions.");
    }
    else
    {
        snprintf(lines[count++], kWarningLineSize, "The file has errors, but all its");
        snprintf(lines[count++], kWarningLineSize, "modes loaded.");
    }

    if (report.firstErrorLine > 0)
    {
        snprintf(lines[count++], kWarningLineSize, "First error: line %d.", report.firstErrorLine);
    }
    else
    {
        snprintf(lines[count++], kWarningLineSize, "A mode in the file has no steps.");
    }
    return count;
}

// ---- Writing a file ----

namespace
{

const FxSpec* findSpec(const ParserConfig& config, uint8_t type)
{
    for (int i = 0; i < config.fxCount; i++)
    {
        if (config.fx[i].type == type)
        {
            return &config.fx[i];
        }
    }
    return nullptr;
}

// "roll 1/4 down 60" / "volume 20": the part of an fx line after the id.
// Returns false for an FX type the config does not know.
bool formatPreset(const ParserConfig& config, const FxPreset& preset, char* out, int outSize)
{
    const FxSpec* spec = findSpec(config, preset.type);
    if (spec == nullptr)
    {
        return false;
    }

    int length = 0;
    if (spec->isRoll)
    {
        const int rate = preset.value % config.rollStride;
        const int type = preset.value / config.rollStride;
        if (rate < 1 || rate > kRollRateCount || type >= kRollTypeCount)
        {
            return false;
        }
        length = snprintf(out, outSize, "%s %s", spec->name, kRollRates[rate - 1]);
        if (type != 0 && length < outSize)
        {
            length += snprintf(out + length, outSize - length, " %s", kRollTypes[type]);
        }
    }
    else
    {
        length = snprintf(out, outSize, "%s %d", spec->name, preset.value);
    }

    if (preset.chance != 100 && length < outSize)
    {
        snprintf(out + length, outSize - length, " %d", preset.chance);
    }
    return true;
}

bool sameStep(const Step& a, const Step& b)
{
    return a.stepNumberToPlay == b.stepNumberToPlay && a.fxOption == b.fxOption;
}

// The shortest loop that, repeated, gives all 128 steps.
int loopLength(const Step* steps)
{
    for (int length = 1; length < kStepCount; length++)
    {
        bool repeats = true;
        for (int i = length; i < kStepCount && repeats; i++)
        {
            repeats = sameStep(steps[i], steps[i - length]);
        }
        if (repeats)
        {
            return length;
        }
    }
    return kStepCount;
}

bool samePreset(const FxPreset& a, const FxPreset& b)
{
    return a.type == b.type && a.value == b.value && a.chance == b.chance;
}

} // namespace

int writeFile(const Block& block, const ParserConfig& config, LineSink sink, void* context)
{
    const uint32_t modes = block.userModeMask;
    char line[kMaxLineLength + 1];

    sink(context, "# Custom play modes (C1-C20) for Polyend Tracker CE");
    sink(context, "# Format: docs/user_play_modes.md");
    snprintf(line, sizeof(line), "version %d", kFormatVersion);
    sink(context, line);

    // Renumber the presets the modes use: newId[old id] = 1, 2, ... in order of first use.
    uint8_t newId[kMaxPresets + 1] = {};
    int presetCount = 0;
    for (int mode = 0; mode < kModeCount; mode++)
    {
        if (!((modes >> mode) & 1u))
        {
            continue;
        }
        for (int i = 0; i < kStepCount; i++)
        {
            const int id = block.steps[mode][i].fxOption;
            if (id >= 1 && id <= kMaxPresets && newId[id] == 0)
            {
                newId[id] = static_cast<uint8_t>(++presetCount);
            }
        }
    }

    if (presetCount > 0)
    {
        sink(context, "");
    }
    for (int written = 1; written <= presetCount; written++)
    {
        for (int id = 1; id <= kMaxPresets; id++)
        {
            if (newId[id] != written)
            {
                continue;
            }
            char preset[64];
            if (!formatPreset(config, block.presets[id - 1], preset, sizeof(preset)))
            {
                snprintf(preset, sizeof(preset), "unknown");
            }
            snprintf(line, sizeof(line), "fx %d %s", written, preset);
            sink(context, line);
        }
    }

    int modeCount = 0;
    for (int mode = 0; mode < kModeCount; mode++)
    {
        if (!((modes >> mode) & 1u))
        {
            continue;
        }
        modeCount++;

        sink(context, "");
        snprintf(line, sizeof(line), "mode %d", mode + 1);
        sink(context, line);

        const Step* steps = block.steps[mode];
        const int length = loopLength(steps);
        int lineLength = 0;
        for (int i = 0; i < length; i++)
        {
            const int id = steps[i].fxOption;
            const int fx = (id >= 1 && id <= kMaxPresets) ? newId[id] : 0;
            lineLength += snprintf(line + lineLength,
                                   sizeof(line) - lineLength,
                                   fx ? "%s%d:%d" : "%s%d",
                                   lineLength ? " " : "",
                                   steps[i].stepNumberToPlay,
                                   fx);
            if ((i % 16) == 15 || i == length - 1)
            {
                sink(context, line);
                lineLength = 0;
            }
        }
    }
    return modeCount;
}

// ---- Editing the block ----

uint64_t presetsInUse(const Block& block, uint32_t modeMask)
{
    uint64_t used = 0;
    for (int mode = 0; mode < kModeCount; mode++)
    {
        if (!((modeMask >> mode) & 1u))
        {
            continue;
        }
        for (int i = 0; i < kStepCount; i++)
        {
            const int id = block.steps[mode][i].fxOption;
            if (id >= 1 && id <= kMaxPresets)
            {
                used |= presetBit(id);
            }
        }
    }
    return used;
}

int findOrAddPreset(Block& block, const FxPreset& preset, uint32_t modeMask)
{
    const uint64_t used = presetsInUse(block, modeMask);
    for (int id = 1; id <= kMaxPresets; id++)
    {
        if ((used & presetBit(id)) && samePreset(block.presets[id - 1], preset))
        {
            return id;
        }
    }
    for (int id = 1; id <= kMaxPresets; id++)
    {
        if (!(used & presetBit(id)))
        {
            block.presets[id - 1] = preset;
            __atomic_thread_fence(__ATOMIC_RELEASE);
            return id;
        }
    }
    return 0;
}

bool copyBuiltinMode(Block& block,
                     int mode,
                     const Step* builtinSteps,
                     const FxPreset* builtinPresets,
                     int builtinPresetCount)
{
    if (mode < 0 || mode >= kModeCount)
    {
        return false;
    }

    // The mode is not user-defined yet, so the sequencer does not read these
    // steps: they can be written freely. Presets are added as if the mode
    // already counted, so the ones it needs are not handed out twice.
    const uint32_t bit = 1u << mode;
    if (block.userModeMask & bit)
    {
        return true; // already user-defined
    }
    const uint32_t withMode = block.userModeMask | bit;

    // Built-in fxOption n maps to the user preset in map[n].
    uint8_t map[kMaxPresets + 1] = {};
    for (int i = 0; i < kStepCount; i++)
    {
        block.steps[mode][i].stepNumberToPlay = builtinSteps[i].stepNumberToPlay;
        block.steps[mode][i].fxOption = 0;
    }
    for (int i = 0; i < kStepCount; i++)
    {
        const int option = builtinSteps[i].fxOption;
        if (option < 1 || option > builtinPresetCount || option > kMaxPresets)
        {
            continue;
        }
        if (map[option] == 0)
        {
            const int id = findOrAddPreset(block, builtinPresets[option - 1], withMode);
            if (id == 0)
            {
                return false;
            }
            map[option] = static_cast<uint8_t>(id);
        }
        block.steps[mode][i].fxOption = map[option];
    }

    __atomic_store_n(&block.userModeMask, withMode, __ATOMIC_RELEASE);
    return true;
}

} // namespace userPlayModes
