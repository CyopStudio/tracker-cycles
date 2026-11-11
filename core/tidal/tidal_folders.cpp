// Tidal sample folders -> sliced instruments (see tidal_folders.h).
#include "tidal_folders.h"

#ifdef TIDAL_EDITOR
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "../FileManager/AudioDataUtilities.h"
#include "Platform.h"
#include "cr_section_macros.h"
#include "fileManager.h"
#include "mtStructs.h"
#include "wavHeaderReader.h"
#include "../../tidal/core/mapping.h"

namespace {

constexpr const char* kRoot = "/Tidal";
constexpr const char* kCacheDir = "/Tidal/.cache";
constexpr const char* kLogPath = "/tidal_folders.txt";
constexpr int kMaxFolders = 128;
constexpr int kMaxListed = 128;  // files listed per folder (sorted), the first kMaxFiles usable kept
constexpr int kMaxFiles = MAX_SLICE_NUMBER;
constexpr int kMaxName = 63;
constexpr uint32_t kChunk = 8 * 1024;                     // source bytes per step
constexpr uint32_t kMinRate = 8000;                       // so one chunk converts into gOut
constexpr uint32_t kOutCap = kChunk * targetSampleRate / kMinRate + 64;  // samples

struct FileInfo {
    uint32_t dataBytes, outFrames, rate;
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

enum class St : uint8_t { Idle, Scan, Plan, Write, Import, ImportWait, Finish };
St gSt = St::Idle;
int gFolderCount = 0;
char gFolder[kMaxName + 1];  // folder name as on the card
char gToken[16];             // its s name (sampleToken): the cache file's and the instrument's name
char gCacheDir[24];
int gListed = 0, gUsed = 0, gFiles = 0, gRefused = 0, gIdx = 0, gSlot = -1;
uint32_t gGap = 0, gTotal = 0, gPos[kMaxFiles];
FileSystem::FileHandle gSrc = nullptr, gDst = nullptr;
uint32_t gLeft = 0, gWritten = 0, gStartMs = 0, gImportTries = 0;
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

void fail(Platform* platform, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
void fail(Platform* platform, const char* fmt, ...) {
    closeFiles(platform);
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

// Frames convertAudioData produces for a file read in kChunk pieces (its
// per-chunk rounding included).
uint32_t framesOut(const FileInfo& f) {
    const uint32_t bps = (uint32_t)f.bits / 8 * f.channels;
    if (!bps) return 0;
    const uint32_t chunk = kChunk - kChunk % (f.align ? f.align : 1);
    uint32_t left = f.dataBytes, total = 0;
    while (left > 0) {
        const uint32_t take = left < chunk ? left : chunk;
        const uint32_t in = take / bps;
        total += (uint32_t)((uint64_t)in * targetSampleRate / f.rate);
        left -= take;
    }
    return total;
}

void pathOf(char* buf, int cap, int i) { snprintf(buf, cap, "%s/%s/%s", kRoot, gFolder, gFileList[i]); }

// scan: one file's header per pass
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
    strWavFileHeader h{};
    if (!readHeader(&h, fs, fh)) {
        fs->closeFile(fh);
        logf("  refused %s: not a plain WAV (no fmt / data chunk where expected)\n", name);
        gRefused++;
        return;
    }
    fs->closeFile(fh);
    f.dataBytes = h.subchunk2Size;
    f.rate = h.sampleRate;
    f.channels = h.numChannels;
    f.bits = h.bitsPerSample;
    f.format = h.AudioFormat;
    f.align = h.blockAlign;
    if (!checkSampleValid(h) || f.channels < 1 || f.channels > 2 || f.rate < kMinRate) {
        logf("  refused %s: format %u, %u ch, %u bit, %lu Hz (the stock import's formats: PCM / float, 1-2 ch, "
             ">= %lu Hz)\n",
             name, f.format, f.channels, f.bits, (unsigned long)f.rate, (unsigned long)kMinRate);
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

// plan: layout, slot, room
void stepPlan(Platform* platform) {
    if (gUsed == 0) return fail(platform, "%s: no usable WAV files (see %s)", gFolder, kLogPath);
    uint64_t sum = 0;
    for (int i = 0; i < gListed; i++)
        if (gInfo[i].ok) sum += gInfo[i].outFrames;
    // a gap of silence before each file, longer than a slice step
    // (length / 65535), so a slice point never lands inside the file before
    gGap = 2 * (uint32_t)((sum + 65534) / 65535) + 4;
    uint64_t total = sum + (uint64_t)(gUsed - 1) * gGap;
    while (gGap < (uint32_t)((total + 65534) / 65535) + 2) {
        gGap += 2;
        total = sum + (uint64_t)(gUsed - 1) * gGap;
    }
    const uint64_t bytes = total * 2;
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
    const uint32_t used = mtProject.used_memory - (reload ? newFileManager.getSampleMemorySize(gSlot, gSlot) : 0);
    const uint32_t room = mtProject.max_memory > used ? mtProject.max_memory - used : 0;
    if (bytes > room)
        return fail(platform, "%s: no room: needs %lu KB, %lu KB free", gFolder, (unsigned long)(bytes / 1024),
                    (unsigned long)(room / 1024));
    gTotal = (uint32_t)total;
    logf("  -> %d files (%d refused), %lu frames (%lu.%lu s), gap %lu, slot %d%s\n", gUsed, gRefused,
         (unsigned long)gTotal, (unsigned long)(gTotal / targetSampleRate),
         (unsigned long)(gTotal * 10 / targetSampleRate % 10), (unsigned long)gGap, gSlot + 1,
         reload ? " (reload)" : "");

    // the cache WAV: header now (the size is known), data in stepWrite
    FileSystem* fs = platform->getFileSystem();
    if (!fs->exists(kCacheDir)) fs->mkdir(1, kCacheDir);
    char path[64];
    snprintf(path, sizeof path, "%s/%s.wav", kCacheDir, gToken);
    auto [ok, fh] = fs->openFile(path, FileSystem::kWriteCreateNew);
    if (!ok || !fh) return fail(platform, "%s: can't write %s", gFolder, path);
    gDst = fh;
    uint8_t hd[44];
    auto le32 = [&](int at, uint32_t v) {
        for (int k = 0; k < 4; k++) hd[at + k] = (uint8_t)(v >> (8 * k));
    };
    auto le16 = [&](int at, uint16_t v) {
        hd[at] = (uint8_t)v;
        hd[at + 1] = (uint8_t)(v >> 8);
    };
    memcpy(hd, "RIFF", 4);
    le32(4, 36 + (uint32_t)bytes);
    memcpy(hd + 8, "WAVEfmt ", 8);
    le32(16, 16);
    le16(20, 1);  // PCM
    le16(22, 1);  // mono
    le32(24, targetSampleRate);
    le32(28, targetSampleRate * 2);
    le16(32, 2);
    le16(34, 16);
    memcpy(hd + 36, "data", 4);
    le32(40, (uint32_t)bytes);
    if (fs->writeFile(hd, sizeof hd, gDst) != (int32_t)sizeof hd) return fail(platform, "%s: SD write failed", gFolder);
    gIdx = 0;
    gFiles = gUsed;
    gUsed = 0;  // counts again while writing: the slice index of each file
    gSrc = nullptr;
    gSt = St::Write;
}

bool writeZeros(FileSystem* fs, uint32_t frames) {
    memset(gOut, 0, (frames < kOutCap ? frames : kOutCap) * 2);
    while (frames > 0) {
        const uint32_t k = frames < kOutCap ? frames : kOutCap;
        if (fs->writeFile(gOut, k * 2, gDst) != (int32_t)(k * 2)) return false;
        frames -= k;
    }
    return true;
}

// write: one chunk of one file per pass
void stepWrite(Platform* platform) {
    FileSystem* fs = platform->getFileSystem();
    if (!gSrc) {  // the next usable file
        while (gIdx < gListed && !gInfo[gIdx].ok) gIdx++;
        if (gIdx >= gListed) {
            fs->closeFile(gDst);
            gDst = nullptr;
            gSt = St::Import;
            gImportTries = 0;
            return;
        }
        char path[200];
        pathOf(path, sizeof path, gIdx);
        auto [ok, fh] = fs->openFile(path, FileSystem::kRead);
        strWavFileHeader h{};
        if (!ok || !fh || !readHeader(&h, fs, fh)) {
            if (ok && fh) fs->closeFile(fh);
            return fail(platform, "%s: %s went away", gFolder, gFileList[gIdx]);
        }
        gSrc = fh;
        if (gUsed > 0 && !writeZeros(fs, gGap)) return fail(platform, "%s: SD write failed", gFolder);
        gPos[gUsed] = gWritten + (gUsed > 0 ? gGap : 0);
        gWritten = gPos[gUsed];
        gLeft = gInfo[gIdx].dataBytes;
        if ((gUsed & 3) == 0) say(false, "loading %s: file %d of %d", gFolder, gUsed + 1, gFiles);
        return;
    }
    const FileInfo& f = gInfo[gIdx];
    const uint32_t align = f.align ? f.align : 1, chunk = kChunk - kChunk % align;
    const uint32_t want = gLeft < chunk ? gLeft : chunk;
    const uint32_t start = gPos[gUsed], wantTotal = f.outFrames;
    uint32_t produced = 0;
    int32_t got = want ? fs->readFile(gIn, want, gSrc) : 0;
    if (got > 0) {
        gLeft -= (uint32_t)got < gLeft ? (uint32_t)got : gLeft;
        const uint32_t usable = (uint32_t)got - (uint32_t)got % align;
        const int16_t* data = gOut;
        if (f.format == wavFormatPCM && f.bits == 16 && f.channels == 1 && f.rate == targetSampleRate) {
            data = reinterpret_cast<const int16_t*>(gIn);  // already the Tracker's format: as it is
            produced = usable / 2;
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
        }
        const uint32_t done = gWritten - start;
        if (done + produced > wantTotal) produced = wantTotal - done;  // exactly the planned length
        if (produced && fs->writeFile(data, produced * 2, gDst) != (int32_t)(produced * 2))
            return fail(platform, "%s: SD write failed", gFolder);
        gWritten += produced;
    }
    if (got <= 0 || gLeft == 0) {  // file done: pad a short read to the planned length
        const uint32_t done = gWritten - start;
        if (done < wantTotal && !writeZeros(fs, wantTotal - done)) return fail(platform, "%s: SD write failed", gFolder);
        gWritten = start + wantTotal;
        fs->closeFile(gSrc);
        gSrc = nullptr;
        gIdx++;
        gUsed++;
    }
}

void stepImport(Platform* platform) {
    if (newFileManager.getStatus() != fmIdle) {
        if (++gImportTries > 2000) fail(platform, "%s: the file manager stayed busy", gFolder);
        return;
    }
    snprintf(gCacheDir, sizeof gCacheDir, "%s", kCacheDir);
    char name[sizeof gToken];
    snprintf(name, sizeof name, "%s", gToken);
    if (!newFileManager.importSampleToProject(gCacheDir, name, (uint8_t)gSlot)) {
        if (++gImportTries > 2000) fail(platform, "%s: the sample import didn't start", gFolder);
        return;
    }
    say(false, "loading %s: into instrument %d", gFolder, gSlot + 1);
    gSt = St::ImportWait;
}

void stepFinish(Platform* platform) {
    strInstrument& ins = mtProject.instrument[gSlot];
    if (ins.isActive != 1 || ins.sample.length == 0) return fail(platform, "%s: the sample import failed", gFolder);
    const uint32_t len = ins.sample.length;
    ins.playMode = playModeSlice;
    ins.sliceNumber = (uint8_t)gUsed;
    ins.selectedSlice = 0;
    ins.slicePlayThru = 0;
    for (int i = 0; i < gUsed; i++)  // pos(v) = v * len / 65535 lands in the gap before file i
        ins.slices[i] = i == 0 ? 0 : (uint16_t)((uint64_t)gPos[i] * 65535 / len);
    newFileManager.setInstrumentStructChanged((uint8_t)gSlot);
    newFileManager.saveInstrumentStructToWorkspace(gSlot);
    const uint32_t ms = platform->getTimer()->millis() - gStartMs;
    logf("  done: instrument %d, %lu KB, %lu.%lu s\n\n", gSlot + 1, (unsigned long)(len * 2 / 1024),
         (unsigned long)(ms / 1000), (unsigned long)(ms / 100 % 10));
    flushLog(platform);
    if (gRefused)
        say(true, "%s: %d files in instrument %d, %d refused (see %s)", gToken, gUsed, gSlot + 1, gRefused, kLogPath);
    else
        say(false, "%s: %d files in instrument %d (%lu KB, %lu.%lu s): s \"%s:0\" ..", gToken, gUsed, gSlot + 1,
            (unsigned long)(len * 2 / 1024), (unsigned long)(ms / 1000), (unsigned long)(ms / 100 % 10), gToken);
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
    gWritten = 0;
    gStartMs = platform->getTimer()->millis();
    gLogLen = 0;
    logf("== %s (s \"%s\"), %d .wav files listed\n", gFolder, gToken, n);
    gSt = St::Scan;
    snprintf(msg, cap, "loading %s (%d files)...", gFolder, n);
    return true;
}

void tidalFoldersUpdate(Platform* platform) {
    if (gSt == St::Idle || !platform) return;
    if (gSt == St::ImportWait) {  // the stock import runs as a file manager operation
        if (newFileManager.getCurrentOperation() != fmNoOperation) return;
        newFileManager.clearStatus();  // fmImportSamplesEnd: no sample import screen to clear it
        gSt = St::Finish;
        return;
    }
    // our own SD work only while the file manager is idle
    if (newFileManager.getCurrentOperation() != fmNoOperation) return;
    switch (gSt) {
        case St::Scan: stepScan(platform); break;
        case St::Plan: stepPlan(platform); break;
        case St::Write: stepWrite(platform); break;
        case St::Import: stepImport(platform); break;
        case St::Finish: stepFinish(platform); break;
        default: break;
    }
}

bool tidalFoldersMessage(char* msg, int cap, bool& isError) {
    if (!gMsgNew) return false;
    gMsgNew = false;
    snprintf(msg, cap, "%s", gMsg);
    isError = gMsgErr;
    return true;
}
#endif
