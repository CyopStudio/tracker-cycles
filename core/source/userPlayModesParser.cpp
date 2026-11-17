#include "userPlayModesParser.h"

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

    const uint32_t bit = 1u << id;
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
            modeUsedPresets |= 1u << preset;
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
            definedPresets |= 1u << (i + 1);
        }
    }

    uint32_t loaded = 0;
    if (!fileRejected)
    {
        const uint32_t validPresets = definedPresets & ~brokenPresets;
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
    report.rejectedPresets = static_cast<uint16_t>((brokenPresets >> 1) & 0xFFFF);
    report.fileRejected = fileRejected;
    report.errorCount = errorCount;
    report.firstErrorLine = firstErrorLine;
    return report;
}

} // namespace userPlayModes
