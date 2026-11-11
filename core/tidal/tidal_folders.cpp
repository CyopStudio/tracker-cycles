// Tidal sample folders -> sliced instruments (see tidal_folders.h).
#include "tidal_folders.h"

#ifdef TIDAL_EDITOR
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "../FileManager/AudioDataUtilities.h"
#include "../FileManager/fileManagerDefs.h"
#include "Platform.h"
#include "cr_section_macros.h"
#include "fileManager.h"
#include "mtAudioEngine.h"
#include "mtStructs.h"
#include "tidal_player.h"
#include "wavHeaderReader.h"
#include "wav_parse.h"
#include "../../tidal/core/mapping.h"

namespace {

constexpr const char* kRoot = "/Tidal";
constexpr const char* kLogPath = "/tidal_folders.txt";
constexpr int kMaxFolders = 128;
constexpr int kMaxListed = 128;  // files listed per folder (sorted), the first kMaxFiles usable kept
constexpr int kMaxFiles = MAX_SLICE_NUMBER;
constexpr int kMaxName = 63;
constexpr uint32_t kChunk = 8 * 1024;                                    // source bytes per step
constexpr uint32_t kMinRate = 8000;                                      // so one chunk converts into gOut
constexpr uint32_t kOutCap = kChunk * targetSampleRate / kMinRate + 64;  // samples
// Pacing (Editor18: loading while 6-8 patterns played starved the
// scheduler, which runs on the main loop): work only while the scheduler is
// this far ahead, and at most this long per main-loop pass.
constexpr float kMinAheadSec = 0.15f;
constexpr uint32_t kBusyBudgetUs = 3000, kIdleBudgetUs = 12000;

struct FileInfo {
    uint32_t dataOffset, dataBytes, outFrames, rate;
    uint16_t channels, bits, format, align;
    bool ok;
};

// SDRAM, plain data (nothing runs at static init)
__NOINIT(EXTERNAL_RAM) char gFolderNames[kMaxFolders * (kMaxName + 2)];
__NOINIT(EXTERNAL_RAM) char* gFolderList[kMaxFolders];
__NOINIT(EXTERNAL_RAM) char gFileNames[kMaxListed * (kMaxName + 2)];
__NOINIT(EXTERNAL_RAM) char* gFileList[kMaxListed];
__NOINIT(EXTERNAL_RAM) FileInfo gInfo[kMaxListed];
__NOINIT(EXTERNAL_RAM) uint8_t gIn[kChunk] __attribute__((aligned(4)));
__NOINIT(EXTERNAL_RAM) int16_t gOut[kOutCap] __attribute__((aligned(4)));
__NOINIT(EXTERNAL_RAM) char gLog[6 * 1024];

enum class St : uint8_t { Idle, Scan, Plan, Write, Save, SaveData };
St gSt = St::Idle;
int gFolderCount = 0;
char gFolder[kMaxName + 1];  // folder name as on the card
char gToken[16];             // its s name (sampleToken): the instrument's name
int gListed = 0, gUsed = 0, gFiles = 0, gRefused = 0, gIdx = 0, gSlot = -1;
uint32_t gGap = 0, gTotal = 0, gPos[kMaxFiles];
FileSystem::FileHandle gSrc = nullptr, gDst = nullptr;
uint32_t gLeft = 0, gWritten = 0, gStartMs = 0, gSaved = 0, gWaitedPasses = 0;
bool gBlocked = false;
int gLogLen = 0;
char gMsg[96];
bool gMsgNew = false, gMsgErr = false;

void say(bool err, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
void say(bool err, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(gMsg, sizeof gMsg, fmt, ap);
    va_end(ap);
    gMsgNew = true;
    gMsgErr = err;
}

void logf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void logf(const char* fmt, ...) {
    if (gLogLen >= (int)sizeof gLog - 1) return;
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(gLog + gLogLen, sizeof gLog - gLogLen, fmt, ap);
    va_end(ap);
    if (n > 0) gLogLen += n;
    if (gLogLen > (int)sizeof gLog - 1) gLogLen = (int)sizeof gLog - 1;
}

void flushLog(Platform* platform) {
    FileSystem* fs = platform->getFileSystem();
    auto [ok, fh] = fs->openFile(kLogPath, FileSystem::kWriteAppend);
    if (ok && fh) {
        fs->writeFile(gLog, (size_t)gLogLen, fh);
        fs->closeFile(fh);
    }
    gLogLen = 0;
}

void closeFiles(Platform* platform) {
    FileSystem* fs = platform->getFileSystem();
    if (gSrc) fs->closeFile(gSrc);
    if (gDst) fs->closeFile(gDst);
    gSrc = gDst = nullptr;
}

// The instrument whose memory we were filling is useless: free its slot.
void dropInstrument() {
    if (!gBlocked) return;
    newFileManager.completeLoadedInstrumentStruct(gSlot, false);
    mtProject.used_memory = newFileManager.getSampleMemorySize(0, kSampleInstrumentsCount - 1);
    engine.clearBlockedInstrument(gSlot);
    gBlocked = false;
}

void fail(Platform* platform, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
void fail(Platform* platform, const char* fmt, ...) {
    closeFiles(platform);
    dropInstrument();
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(gMsg, sizeof gMsg, fmt, ap);
    va_end(ap);
    gMsgNew = gMsgErr = true;
    logf("FAILED: %s\n\n", gMsg);
    flushLog(platform);
    gSt = St::Idle;
}

bool isWav(const char* n) {
    if (n[0] == '.') return false;
    const char* dot = strrchr(n, '.');
    return dot && (!strcasecmp(dot, ".wav") || !strcasecmp(dot, ".wave"));
}

// The forgiving WAV walk (wav_parse.h, host-tested) on an open file.
bool parseWav(FileSystem* fs, FileSystem::FileHandle fh, FileInfo& f, char* why, int whyCap) {
    const uint32_t size = (uint32_t)fs->getFileSize(fh);
    wavp::Info w;
    const bool ok = wavp::parse(
      size,
      [&](uint32_t at, uint8_t* buf, uint32_t n) { return fs->seekFile(at, fh) && fs->readFile(buf, n, fh) == (int32_t)n; },
      w, why, whyCap);
    if (!ok) return false;
    f.dataOffset = w.dataOffset;
    f.dataBytes = w.dataBytes;
    f.rate = w.rate;
    f.channels = w.channels;
    f.bits = w.bits;
    f.format = w.format;
    f.align = w.align;
    return true;
}

// Frames convertAudioData produces for a file read in kChunk pieces (its
// per-chunk rounding included).
uint32_t framesOut(const FileInfo& f) {
    const uint32_t bps = (uint32_t)f.bits / 8 * f.channels;
    if (!bps) return 0;
    const uint32_t chunk = kChunk - kChunk % (f.align ? f.align : 1);
    uint32_t left = f.dataBytes - f.dataBytes % (f.align ? f.align : 1), total = 0;
    while (left > 0) {
        const uint32_t take = left < chunk ? left : chunk;
        total += (uint32_t)((uint64_t)(take / bps) * targetSampleRate / f.rate);
        left -= take;
    }
    return total;
}

void pathOf(char* buf, int cap, int i) { snprintf(buf, cap, "%s/%s/%s", kRoot, gFolder, gFileList[i]); }

// scan: one file's header per call
void stepScan(Platform* platform) {
    FileSystem* fs = platform->getFileSystem();
    if (gIdx >= gListed) {
        gSt = St::Plan;
        return;
    }
    FileInfo& f = gInfo[gIdx];
    memset(&f, 0, sizeof f);
    char path[200];
    pathOf(path, sizeof path, gIdx);
    const char* name = gFileList[gIdx];
    gIdx++;
    if (gUsed >= kMaxFiles) {
        logf("  skipped %s: more than %d files (n wraps over the first %d)\n", name, kMaxFiles, kMaxFiles);
        return;
    }
    auto [ok, fh] = fs->openFile(path, FileSystem::kRead);
    if (!ok || !fh) {
        logf("  refused %s: can't open\n", name);
        gRefused++;
        return;
    }
    char why[64];
    const bool parsed = parseWav(fs, fh, f, why, sizeof why);
    fs->closeFile(fh);
    if (!parsed) {
        logf("  refused %s: %s\n", name, why);
        gRefused++;
        return;
    }
    strWavFileHeader h{};  // the stock converter's formats
    h.AudioFormat = f.format;
    h.numChannels = f.channels;
    h.sampleRate = f.rate;
    h.bitsPerSample = f.bits;
    h.blockAlign = f.align;
    h.byteRate = f.rate * f.align;
    if (!checkSampleValid(h) || f.channels < 1 || f.channels > 2 || f.rate < kMinRate) {
        logf("  refused %s: format %u, %u ch, %u bit, %lu Hz (supported: PCM / float, 1-2 ch, >= %lu Hz)\n", name,
             f.format, f.channels, f.bits, (unsigned long)f.rate, (unsigned long)kMinRate);
        gRefused++;
        return;
    }
    f.outFrames = framesOut(f);
    if (f.outFrames == 0) {
        logf("  refused %s: no audio\n", name);
        gRefused++;
        return;
    }
    f.ok = true;
    logf("  %2d %-30s %u ch %2u bit %6lu Hz %8lu frames\n", gUsed, name, f.channels, f.bits, (unsigned long)f.rate,
         (unsigned long)f.outFrames);
    gUsed++;
}

// plan: layout, slot, room; then the slot's memory (as the stock sample
// import places it) with the instrument blocked while we fill it
void stepPlan(Platform* platform) {
    if (gUsed == 0) return fail(platform, "%s: no usable WAV files (see %s)", gFolder, kLogPath);
    uint64_t sum = 0;
    for (int i = 0; i < gListed; i++)
        if (gInfo[i].ok) sum += gInfo[i].outFrames;
    // silence before each file, longer than a slice step (length / 65535),
    // so a slice point never lands inside the file before
    gGap = 2 * (uint32_t)((sum + 65534) / 65535) + 4;
    uint64_t total = sum + (uint64_t)(gUsed - 1) * gGap;
    while (gGap < (uint32_t)((total + 65534) / 65535) + 2) {
        gGap += 2;
        total = sum + (uint64_t)(gUsed - 1) * gGap;
    }
    // the slot: an instrument of this name is reloaded, else the first free one
    gSlot = -1;
    for (int i = 0; i < kSampleInstrumentsCount && gSlot < 0; i++) {
        char tok[16];
        tc::sampleToken(mtProject.instrument[i].sample.file_name, tok, sizeof tok);
        if (mtProject.instrument[i].isActive == 1 && !strcmp(tok, gToken)) gSlot = i;
    }
    const bool reload = gSlot >= 0;
    for (int i = 0; i < kSampleInstrumentsCount && gSlot < 0; i++)
        if (mtProject.instrument[i].isActive != 1) gSlot = i;
    if (gSlot < 0) return fail(platform, "%s: no free instrument slot (48 in use)", gFolder);
    const uint32_t oldSize = reload ? newFileManager.getSampleMemorySize(gSlot, gSlot) : 0;
    const uint32_t used = mtProject.used_memory - oldSize;
    const uint32_t room = mtProject.max_memory > used ? mtProject.max_memory - used : 0;
    if (total * 2 > room)
        return fail(platform, "%s: no room: needs %lu KB, %lu KB free", gFolder, (unsigned long)(total * 2 / 1024),
                    (unsigned long)(room / 1024));
    gTotal = (uint32_t)total;
    logf("  -> %d files (%d refused), %lu frames (%lu.%lu s), gap %lu, slot %d%s\n", gUsed, gRefused,
         (unsigned long)gTotal, (unsigned long)(gTotal / targetSampleRate),
         (unsigned long)(gTotal * 10 / targetSampleRate % 10), (unsigned long)gGap, gSlot + 1,
         reload ? " (reload)" : "");

    // the stock import's placement (SampleImport: init, load data struct, move memory)
    engine.setBlockedInstrument(gSlot);
    gBlocked = true;
    mtProject.used_memory -= oldSize;
    strInstrument& ins = mtProject.instrument[gSlot];
    if (ins.isActive != 1) newFileManager.setDefaultActiveInstrument(&ins);
    snprintf(ins.sample.file_name, SAMPLE_NAME_SIZE, "%s", gToken);
    newFileManager.completeLoadedInstrumentStruct(gSlot, true, gTotal);
    newFileManager.setInstrumentSampleAddress(gSlot, 1, gTotal);
    ins.setStreamPreferred(false);
    const uint32_t newSize = newFileManager.getSampleMemorySize(gSlot, gSlot);
    newFileManager.moveSampleMemory(newFileManager.calcFirstSlotToMoveInMemory(gSlot + 1),
                                    (int32_t)newSize - (int32_t)oldSize);
    mtProject.used_memory = newFileManager.getSampleMemorySize(0, kSampleInstrumentsCount - 1);

    gIdx = 0;
    gFiles = gUsed;
    gUsed = 0;  // counts again while writing: the slice index of each file
    gWritten = 0;
    gSrc = nullptr;
    gSt = St::Write;
}

// write: a chunk of a file straight into the instrument's memory
void stepWrite(Platform* platform) {
    FileSystem* fs = platform->getFileSystem();
    int16_t* const mem = mtProject.instrument[gSlot].sample.address;
    if (!gSrc) {  // the next usable file
        while (gIdx < gListed && !gInfo[gIdx].ok) gIdx++;
        if (gIdx >= gListed) {
            gSt = St::Save;
            return;
        }
        char path[200];
        pathOf(path, sizeof path, gIdx);
        auto [ok, fh] = fs->openFile(path, FileSystem::kRead);
        if (!ok || !fh || !fs->seekFile(gInfo[gIdx].dataOffset, fh)) {
            if (ok && fh) fs->closeFile(fh);
            return fail(platform, "%s: %s went away", gFolder, gFileList[gIdx]);
        }
        gSrc = fh;
        if (gUsed > 0) {  // the silence before it
            memset(mem + gWritten, 0, gGap * 2);
            gWritten += gGap;
        }
        gPos[gUsed] = gWritten;
        gLeft = gInfo[gIdx].dataBytes - gInfo[gIdx].dataBytes % (gInfo[gIdx].align ? gInfo[gIdx].align : 1);
        if ((gUsed & 3) == 0) say(false, "loading %s: file %d of %d", gFolder, gUsed + 1, gFiles);
        return;
    }
    const FileInfo& f = gInfo[gIdx];
    const uint32_t align = f.align ? f.align : 1, chunk = kChunk - kChunk % align;
    const uint32_t start = gPos[gUsed], want = f.outFrames;
    const uint32_t take = gLeft < chunk ? gLeft : chunk;
    const int32_t got = take ? fs->readFile(gIn, take, gSrc) : 0;
    if (got > 0) {
        gLeft -= (uint32_t)got < gLeft ? (uint32_t)got : gLeft;
        const uint32_t usable = (uint32_t)got - (uint32_t)got % align;
        const uint32_t done = gWritten - start;
        uint32_t produced;
        if (f.format == wavFormatPCM && f.bits == 16 && f.channels == 1 && f.rate == targetSampleRate) {
            produced = usable / 2;  // already the Tracker's format: copied as it is
            if (done + produced > want) produced = want - done;
            memcpy(mem + gWritten, gIn, produced * 2);
        } else {
            ConversionConfig cc{};
            cc.inputData = gIn;
            cc.inputSize = usable;
            cc.outPtrL = gOut;
            cc.outPtrR = gOut;  // mono
            cc.sourceChannelCount = f.channels;
            cc.sourceBitsPerSample = f.bits;
            cc.sourceSampleRate = f.rate;
            cc.sourceFormat = f.format;
            cc.destSampleRate = targetSampleRate;
            produced = convertAudioData(cc);
            if (done + produced > want) produced = want - done;  // exactly the planned length
            memcpy(mem + gWritten, gOut, produced * 2);
        }
        gWritten += produced;
    }
    if (got <= 0 || gLeft == 0) {  // file done: a short read is padded to the planned length
        const uint32_t done = gWritten - start;
        if (done < want) memset(mem + gWritten, 0, (want - done) * 2);
        gWritten = start + want;
        fs->closeFile(gSrc);
        gSrc = nullptr;
        gIdx++;
        gUsed++;
    }
}

// save: the slices, the instrument playable again, its struct to the workspace
void stepSave(Platform* platform) {
    strInstrument& ins = mtProject.instrument[gSlot];
    const uint32_t len = ins.sample.length;
    ins.playMode = playModeSlice;
    ins.sliceNumber = (uint8_t)gUsed;
    ins.selectedSlice = 0;
    ins.slicePlayThru = 0;
    for (int i = 0; i < gUsed; i++)  // pos(v) = v * len / 65535 lands in the silence before file i
        ins.slices[i] = i == 0 ? 0 : (uint16_t)((uint64_t)gPos[i] * 65535 / len);
    engine.clearBlockedInstrument(gSlot);
    gBlocked = false;
    // the workspace copy: a new file (an old, longer one would leave a tail)
    char path[PATCH_SIZE];
    snprintf(path, sizeof path, cWorkspaceInstrumentFileFormatNew, gSlot + 1);
    FileSystem* fs = platform->getFileSystem();
    if (fs->exists(path)) fs->remove(path);
    newFileManager.setInstrumentStructChanged((uint8_t)gSlot);
    newFileManager.saveInstrumentStructToWorkspace(gSlot);
    auto [ok, fh] = fs->openFile(path, FileSystem::kWriteOpen);
    if (!ok || !fh || !fs->seekFile(sizeof(strInstrumentFile), fh)) {
        if (ok && fh) fs->closeFile(fh);
        return fail(platform, "%s: can't write the workspace copy", gFolder);
    }
    gDst = fh;
    gSaved = 0;
    gSt = St::SaveData;
    say(false, "%s: playable; saving it to the workspace", gToken);
}

void stepSaveData(Platform* platform) {
    FileSystem* fs = platform->getFileSystem();
    const uint32_t bytes = gTotal * 2;
    if (gSaved < bytes) {
        const uint32_t n = bytes - gSaved < kChunk ? bytes - gSaved : kChunk;
        const uint8_t* src = reinterpret_cast<const uint8_t*>(mtProject.instrument[gSlot].sample.address) + gSaved;
        if (fs->writeFile(src, n, gDst) != (int32_t)n) return fail(platform, "%s: SD write failed", gFolder);
        gSaved += n;
        return;
    }
    fs->closeFile(gDst);
    gDst = nullptr;
    const uint32_t ms = platform->getTimer()->millis() - gStartMs;
    logf("  done: instrument %d, %lu KB, %lu.%lu s (%lu passes waited for the scheduler)\n\n", gSlot + 1,
         (unsigned long)(bytes / 1024), (unsigned long)(ms / 1000), (unsigned long)(ms / 100 % 10),
         (unsigned long)gWaitedPasses);
    flushLog(platform);
    if (gRefused)
        say(true, "%s: %d files in instrument %d, %d refused (see %s)", gToken, gUsed, gSlot + 1, gRefused, kLogPath);
    else
        say(false, "%s: %d files in instrument %d (%lu KB, %lu.%lu s)", gToken, gUsed, gSlot + 1,
            (unsigned long)(bytes / 1024), (unsigned long)(ms / 1000), (unsigned long)(ms / 100 % 10));
    gSt = St::Idle;
}

}  // namespace

int tidalFolderCount(Platform* platform) {
    if (!platform) return 0;
    int n = kMaxFolders;
    platform->getFileSystem()->listDirContents(
      kRoot, FileSystem::kFolders, [](const char* name) { return name[0] != '.'; }, gFolderNames,
      sizeof gFolderNames, kMaxName, n, gFolderList);
    gFolderCount = n;
    return n;
}

const char* tidalFolderName(int i) {
    if (i < 0 || i >= gFolderCount) return "";
    const char* s = gFolderList[i];
    return s[0] == '/' ? s + 1 : s;
}

bool tidalFoldersBusy() { return gSt != St::Idle; }

bool tidalFolderLoad(Platform* platform, const char* name, char* msg, int cap) {
    if (!platform) return snprintf(msg, cap, "not ready"), false;
    if (gSt != St::Idle) return snprintf(msg, cap, "still loading %s", gFolder), false;
    snprintf(gFolder, sizeof gFolder, "%s", name);
    tc::sampleToken(gFolder, gToken, sizeof gToken);
    if (!gToken[0]) return snprintf(msg, cap, "%s: no usable name", name), false;
    char path[96];
    snprintf(path, sizeof path, "%s/%s", kRoot, gFolder);
    int n = kMaxListed;
    platform->getFileSystem()->listDirContents(path, FileSystem::kFiles, [](const char* f) { return isWav(f); },
                                               gFileNames, sizeof gFileNames, kMaxName, n, gFileList);
    if (n <= 0) return snprintf(msg, cap, "%s: no .wav files", name), false;
    gListed = n;
    gIdx = gUsed = gRefused = 0;
    gWritten = gWaitedPasses = 0;
    gStartMs = platform->getTimer()->millis();
    gLogLen = 0;
    logf("== %s (s \"%s\"), %d .wav files listed\n", gFolder, gToken, n);
    gSt = St::Scan;
    snprintf(msg, cap, "loading %s (%d files)...", gFolder, n);
    return true;
}

void tidalFoldersUpdate(Platform* platform) {
    if (gSt == St::Idle || !platform) return;
    // only while the file manager is idle and the scheduler is comfortably
    // ahead (it queries on this main loop: loading must never cost notes)
    if (newFileManager.getCurrentOperation() != fmNoOperation) return;
    const float ahead = tidalPlayerAheadSeconds();
    if (ahead < kMinAheadSec) {
        gWaitedPasses++;
        return;
    }
    Timer* tm = platform->getTimer();
    const uint32_t t0 = tm->micros(), budget = ahead > 100.0f ? kIdleBudgetUs : kBusyBudgetUs;
    do {
        switch (gSt) {
            case St::Scan: stepScan(platform); break;
            case St::Plan: stepPlan(platform); break;
            case St::Write: stepWrite(platform); break;
            case St::Save: stepSave(platform); break;
            case St::SaveData: stepSaveData(platform); break;
            default: return;
        }
    } while (gSt != St::Idle && gSt != St::Plan && gSt != St::Save && tm->micros() - t0 < budget);
}

bool tidalFoldersMessage(char* msg, int cap, bool& isError) {
    if (!gMsgNew) return false;
    gMsgNew = false;
    snprintf(msg, cap, "%s", gMsg);
    isError = gMsgErr;
    return true;
}
#endif
