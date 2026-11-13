// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// Throwaway on-device folder bench (see folder_bench.h).
//
// Question: how long does the MK66 take to turn a Tidal sample folder
// (Dirt-Samples layout: /Tidal/<name>/*.wav) into instrument data? That
// decides whether a folder can load on demand when a block is evaluated,
// needs a visible "loading" state, or wants files pre-converted on a
// computer (docs/tidal_sample_folders.md).
//
// Two runs: kRun1Ms after boot (once the file manager is idle; leave the
// Tracker stopped) and kRun2Ms (start a project playing in between: run 2
// measures with the audio interrupt busy). Each run, per folder (at most
// kMaxFolders, at most kMaxFiles .wav files each, as the loader would):
//   list  listDirContents of the folder (includes its sort)
//   open  openFile + readHeader, per file
//   read  readFile of the sample data in kChunk pieces
//   conv  convertAudioData to mono 16-bit 44.1 kHz into the scratch buffer
// The output lands in the shared SDRAM scratch buffer (wrapping; it is
// only timed, never used). The main loop is blocked while a run goes:
// screen and SD streaming pause, audio keeps running.
#ifdef FOLDER_BENCH
#include "folder_bench.h"

#include <stdio.h>
#include <string.h>

#include "../FileManager/AudioDataUtilities.h"
#include "../FileManager/fileManager.h"
#include "Platform.h"
#include "Profiler.h"
#include "core/interfacePopups.h"
#include "cr_section_macros.h"
#include "mtStructs.h"
#include "wavHeaderReader.h"

extern uint8_t externalRamScratchBuff[];

namespace {

constexpr const char* kRoot = "/Tidal";
constexpr const char* kPath = "/tidal_folder_bench.txt";
constexpr uint32_t kRun1Ms = 15000, kRun2Ms = 75000, kPopupMs = 8000;
constexpr int kRuns = 2;
constexpr int kMaxFolders = 64, kMaxFiles = 48, kMaxName = 63;
constexpr uint32_t kChunk = 16 * 1024;

struct FolderStat {
    char name[24];
    uint16_t files, failed;
    uint32_t listUs, openUs, readUs, convUs, maxFileUs;
    uint32_t bytesIn, samplesOut;
    uint8_t formats;  // bit 0: already mono 16-bit 44.1 kHz, 1: stereo, 2: other rate, 3: other depth
};

// bench buffers in SDRAM (plain arrays: nothing runs at static init, when
// SDRAM isn't up yet); internal RAM has no room for ~40 KB
__NOINIT(EXTERNAL_RAM) char gFolderNames[kMaxFolders * (kMaxName + 2)];
__NOINIT(EXTERNAL_RAM) char* gFolders[kMaxFolders];
__NOINIT(EXTERNAL_RAM) char gFileNames[kMaxFiles * (kMaxName + 2)];
__NOINIT(EXTERNAL_RAM) char* gFiles[kMaxFiles];
__NOINIT(EXTERNAL_RAM) uint8_t gChunk[kChunk] __attribute__((aligned(4)));
__NOINIT(EXTERNAL_RAM) FolderStat gStat[kMaxFolders];
__NOINIT(EXTERNAL_RAM) char gReport[12 * 1024];
char gLine1[64], gLine2[64];

int gRun = 0;
bool gPopup = false;
uint32_t gShownAt = 0;

bool isWav(const char* n) {
    const char* dot = strrchr(n, '.');
    return dot && (!strcasecmp(dot, ".wav") || !strcasecmp(dot, ".wave"));
}

// one folder, as the loader would read it
void benchFolder(Platform* platform, const char* folder, FolderStat& st, uint32_t& outPos) {
    FileSystem* fs = platform->getFileSystem();
    Timer* tm = platform->getTimer();
    char path[160];
    snprintf(path, sizeof path, "%s%s", kRoot, folder);  // folder starts with '/'
    snprintf(st.name, sizeof st.name, "%s", folder + 1);

    int n = kMaxFiles;
    uint32_t t0 = tm->micros();
    fs->listDirContents(path, FileSystem::kFiles, [](const char* name) { return isWav(name); }, gFileNames,
                        sizeof gFileNames, kMaxName, n, gFiles);
    st.listUs = tm->micros() - t0;
    int16_t* const out = reinterpret_cast<int16_t*>(externalRamScratchBuff);
    const uint32_t outCap = SHARED_SCRATCH_MEMORY_SIZE / 2;

    for (int i = 0; i < n; i++) {
        char file[240];
        snprintf(file, sizeof file, "%s/%s", path, gFiles[i]);
        const uint32_t f0 = tm->micros();
        auto [ok, fh] = fs->openFile(file, FileSystem::kRead);
        strWavFileHeader h{};
        if (!ok || !fh || !readHeader(&h, fs, fh) || !checkSampleValid(h)) {
            if (ok && fh) fs->closeFile(fh);
            st.failed++;
            continue;
        }
        const uint32_t f1 = tm->micros();
        st.openUs += f1 - f0;
        if (h.numChannels == 1 && h.bitsPerSample == 16 && h.sampleRate == targetSampleRate &&
            h.AudioFormat == wavFormatPCM)
            st.formats |= 1;
        if (h.numChannels > 1) st.formats |= 2;
        if (h.sampleRate != targetSampleRate) st.formats |= 4;
        if (h.bitsPerSample != 16 || h.AudioFormat != wavFormatPCM) st.formats |= 8;

        const uint32_t align = h.blockAlign ? h.blockAlign : 1;
        const uint32_t chunk = kChunk - kChunk % align;
        uint32_t left = h.subchunk2Size;
        while (left > 0) {
            const uint32_t want = left < chunk ? left : chunk;
            const uint32_t r0 = tm->micros();
            const int32_t got = fs->readFile(gChunk, want, fh);
            const uint32_t r1 = tm->micros();
            st.readUs += r1 - r0;
            if (got <= 0) break;
            st.bytesIn += (uint32_t)got;
            left -= (uint32_t)got < left ? (uint32_t)got : left;
            // room for the converted chunk (wraps: the data is never used)
            const uint32_t maxOut = (uint32_t)got / align * 2 + 16;  // upsampling 22 kHz -> 44 kHz at most x2
            if (outPos + maxOut > outCap) outPos = 0;
            ConversionConfig cc{};
            cc.inputData = gChunk;
            cc.inputSize = (uint32_t)got - (uint32_t)got % align;
            cc.outPtrL = out + outPos;
            cc.outPtrR = out + outPos;  // mono (as the stock import's mono mode)
            cc.sourceChannelCount = h.numChannels;
            cc.sourceBitsPerSample = h.bitsPerSample;
            cc.sourceSampleRate = h.sampleRate;
            cc.sourceFormat = h.AudioFormat;
            cc.destSampleRate = targetSampleRate;
            const uint32_t produced = convertAudioData(cc);
            st.convUs += tm->micros() - r1;
            st.samplesOut += produced;
            outPos += produced;
        }
        fs->closeFile(fh);
        st.files++;
        const uint32_t fileUs = tm->micros() - f0;
        if (fileUs > st.maxFileUs) st.maxFileUs = fileUs;
    }
}

void runBench(Platform* platform, uint32_t now) {
    FileSystem* fs = platform->getFileSystem();
    Timer* tm = platform->getTimer();
    Profiler::AudioProcessorUsage u{};
    Profiler::getInstance()->getAudioProcessingUsagePercent(u, true);

    memset(gStat, 0, sizeof gStat);
    int nf = kMaxFolders;
    const uint32_t t0 = tm->micros();
    fs->listDirContents(kRoot, FileSystem::kFolders, nullptr, gFolderNames, sizeof gFolderNames, kMaxName, nf,
                        gFolders);
    const uint32_t rootUs = tm->micros() - t0;
    uint32_t outPos = 0;
    for (int i = 0; i < nf; i++) benchFolder(platform, gFolders[i], gStat[i], outPos);
    const uint32_t totalUs = tm->micros() - t0;
    Profiler::getInstance()->getAudioProcessingUsagePercent(u, true);

    uint32_t files = 0, failed = 0, bytes = 0, samples = 0, openUs = 0, readUs = 0, convUs = 0, listUs = 0,
             maxFileUs = 0;
    for (int i = 0; i < nf; i++) {
        const FolderStat& s = gStat[i];
        files += s.files;
        failed += s.failed;
        bytes += s.bytesIn;
        samples += s.samplesOut;
        openUs += s.openUs;
        readUs += s.readUs;
        convUs += s.convUs;
        listUs += s.listUs;
        if (s.maxFileUs > maxFileUs) maxFileUs = s.maxFileUs;
    }

    int n = 0;
    if (gRun == 0)
        n += snprintf(gReport + n, sizeof gReport - n,
                      "Tracker Cycles folder bench v1 (docs/tidal_sample_folders.md 7)\n"
                      "per folder: list = listDirContents (+ sort), open = open + WAV header, read = SD reads "
                      "(%lu KB chunks), conv = convertAudioData to mono 16-bit 44.1 kHz; times in ms\n"
                      "fmt: m = mono 16-bit 44.1 kHz already, s = stereo, r = other rate, d = other depth / "
                      "float\n\n",
                      (unsigned long)(kChunk / 1024));
    const uint32_t ms = totalUs / 1000 ? totalUs / 1000 : 1;
    n += snprintf(gReport + n, sizeof gReport - n,
                  "=== run %d of %d, %lu s after boot, %s\n"
                  "audio load during the run (Profiler): avg %d %%, max %d %%\n"
                  "%d folders (root list %lu ms), %lu files (%lu failed), %lu KB read -> %lu KB out (%lu s of "
                  "audio)\n"
                  "total %lu ms: list %lu, open %lu, read %lu, conv %lu; %lu.%02lu MB/s read overall; "
                  "%lu.%lu ms per file avg, %lu.%lu ms slowest file\n"
                  "%-22s %5s %4s %7s %7s %8s %8s %8s %7s %4s\n",
                  gRun + 1, kRuns, (unsigned long)(now / 1000),
                  gRun == 0 ? "should be stopped" : "should be playing a project", u.average, u.tempMaximum, nf,
                  (unsigned long)(rootUs / 1000), (unsigned long)files, (unsigned long)failed,
                  (unsigned long)(bytes / 1024), (unsigned long)(samples * 2 / 1024),
                  (unsigned long)(samples / targetSampleRate), (unsigned long)ms, (unsigned long)(listUs / 1000),
                  (unsigned long)(openUs / 1000), (unsigned long)(readUs / 1000), (unsigned long)(convUs / 1000),
                  (unsigned long)(bytes / 1024 / 1024 * 1000 / ms),
                  (unsigned long)((uint64_t)bytes * 100 / 1048576 * 1000 / ms % 100),
                  (unsigned long)(files ? totalUs / files / 1000 : 0),
                  (unsigned long)(files ? totalUs / files / 100 % 10 : 0), (unsigned long)(maxFileUs / 1000),
                  (unsigned long)(maxFileUs / 100 % 10), "folder", "files", "fail", "KB", "list", "open", "read",
                  "conv", "slowest", "fmt");
    for (int i = 0; i < nf && n < (int)sizeof gReport - 160; i++) {
        const FolderStat& s = gStat[i];
        char fmt[5] = "----";
        if (s.formats & 1) fmt[0] = 'm';
        if (s.formats & 2) fmt[1] = 's';
        if (s.formats & 4) fmt[2] = 'r';
        if (s.formats & 8) fmt[3] = 'd';
        n += snprintf(gReport + n, sizeof gReport - n, "%-22s %5u %4u %7lu %4lu.%lu %4lu.%lu %8lu %8lu %7lu %4s\n",
                      s.name, s.files, s.failed, (unsigned long)(s.bytesIn / 1024), (unsigned long)(s.listUs / 1000),
                      (unsigned long)(s.listUs / 100 % 10), (unsigned long)(s.openUs / 1000),
                      (unsigned long)(s.openUs / 100 % 10), (unsigned long)(s.readUs / 1000),
                      (unsigned long)(s.convUs / 1000), (unsigned long)(s.maxFileUs / 1000), fmt);
    }
    if (nf == 0) n += snprintf(gReport + n, sizeof gReport - n, "no folders under %s\n", kRoot);
    n += snprintf(gReport + n, sizeof gReport - n, "\n");

    auto [ok, fh] = fs->openFile(kPath, gRun == 0 ? FileSystem::kWriteCreateNew : FileSystem::kWriteAppend);
    if (ok && fh) {
        fs->writeFile(gReport, (size_t)n, fh);
        fs->closeFile(fh);
    }
    snprintf(gLine1, sizeof gLine1, "Folder bench %d/%d: %lu files, %lu ms%s", gRun + 1, kRuns,
             (unsigned long)files, (unsigned long)ms, ok ? "" : " - SD write failed");
    snprintf(gLine2, sizeof gLine2, "%s",
             gRun == 0 ? "now start a project playing for run 2 (at 75 s)" : "done: /tidal_folder_bench.txt");
}

}  // namespace

void folderBenchUpdate(Platform* platform) {
    const uint32_t now = platform->getTimer()->millis();
    if (gPopup && now - gShownAt >= kPopupMs) {
        mtPopups.hideInfoPopup();
        gPopup = false;
    }
    if (gRun >= kRuns) return;
    if (now < (gRun == 0 ? kRun1Ms : kRun2Ms)) return;
    if (newFileManager.getCurrentOperation() != fmNoOperation) return;  // the scratch buffer may be in use
    runBench(platform, now);
    mtPopups.showInfoPopup(gLine1, gLine2);
    gShownAt = platform->getTimer()->millis();
    gPopup = true;
    gRun++;
}
#endif
