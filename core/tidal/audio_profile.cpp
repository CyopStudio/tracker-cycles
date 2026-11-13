// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// Throwaway on-device audio profiler (see audio_profile.h). Captures
// kBlocks audio blocks (~3 s) every kEveryMs starting at kFirstMs, so the
// user can change project / play state / FX between captures. The main
// loop is not blocked; only the report formatting and SD write run there.
#ifdef AUDIO_PROFILE
#include "audio_profile.h"

#include <stdio.h>
#include <string.h>

#include "Platform.h"
#include "Profiler.h"
#include "core/interfacePopups.h"
#include "cr_section_macros.h"
#include "mtSequencer.h"

namespace {

constexpr int kMaxNodes = 256;
constexpr uint32_t kBlocks = 1024;  // ~2.97 s
constexpr int kCaptures = 6;
constexpr uint32_t kFirstMs = 15000, kEveryMs = 30000;
constexpr uint32_t kPopupMs = 4000;
const char* const kPath = "/audio_profile.txt";

struct Node {
    const void* obj;
    const void* vtbl;
    uint32_t total, max, calls;
};
enum { kMidi, kSeq, kPlayers, kGraph, kWhole, kStages };
const char* const kStageNames[kStages] = {"midiUpdate", "sequencer.tick", "engine.update (players)",
                                          "update_all (graph)", "whole callback"};

Node nodes[kMaxNodes];
int nodeCount = 0;
uint32_t stageTotal[kStages], stageMax[kStages];
volatile uint32_t blocks = 0;
volatile int capturing = 0;  // 1 while the ISR accumulates

__NOINIT(EXTERNAL_RAM) char report[20480];

int gCapture = 0;  // captures done
bool gWaiting = false;
uint32_t gStartedAt = 0, gShownAt = 0;
bool gPopup = false;
char gLine1[64], gLine2[64];

const char* seqStateName() {
    switch (sequencer.getSeqState()) {
        case Sequencer::SEQ_STATE_STOP: return "stopped";
        default: return "playing";
    }
}

void startCapture() {
    capturing = 0;
    memset(nodes, 0, sizeof nodes);
    memset(stageTotal, 0, sizeof stageTotal);
    memset(stageMax, 0, sizeof stageMax);
    nodeCount = 0;
    blocks = 0;
    Profiler::AudioProcessorUsage u{};
    Profiler::getInstance()->getAudioProcessingUsagePercent(u, true);  // reset the temp max
    capturing = 1;
}

void writeReport(Platform* platform, uint32_t now) {
    Profiler::AudioProcessorUsage u{};
    Profiler::getInstance()->getAudioProcessingUsagePercent(u, true);
    const uint32_t perUs = platform->getTimer()->clockCyclesPerUs();
    const uint32_t blockCycles = perUs * 2902;  // 128 / 44117.6 Hz
    const uint32_t nb = blocks ? blocks : 1;
    int n = 0;
    n += snprintf(report + n, sizeof report - n,
                  "=== capture %d of %d, %lu s after boot, sequencer %s, %lu blocks\n"
                  "Profiler: max %d %% during capture (avg since last reset %d %%)\n"
                  "cycles per us %lu; block = %lu cycles = 2902 us\n"
                  "%-26s %8s %8s %6s\n",
                  gCapture + 1, kCaptures, (unsigned long)(now / 1000), seqStateName(), (unsigned long)blocks,
                  u.tempMaximum, u.average, (unsigned long)perUs, (unsigned long)blockCycles, "stage", "avg cyc",
                  "max cyc", "avg %");
    for (int s = 0; s < kStages; s++) {
        uint32_t avg = stageTotal[s] / nb;
        n += snprintf(report + n, sizeof report - n, "%-26s %8lu %8lu %3lu.%lu\n", kStageNames[s],
                      (unsigned long)avg, (unsigned long)stageMax[s], (unsigned long)(avg * 100 / blockCycles),
                      (unsigned long)(avg * 1000 / blockCycles % 10));
    }
    n += snprintf(report + n, sizeof report - n, "nodes %d (avg/max cycles per block; calls = blocks active)\n",
                  nodeCount);
    n += snprintf(report + n, sizeof report - n, "node idx  object   vtable   avg-cyc  max-cyc  calls\n");
    for (int i = 0; i < nodeCount && n < (int)sizeof report - 80; i++) {
        const Node& d = nodes[i];
        if (!d.calls) continue;
        n += snprintf(report + n, sizeof report - n, "node %3d %08lx %08lx %8lu %8lu %6lu\n", i,
                      (unsigned long)(uintptr_t)d.obj, (unsigned long)(uintptr_t)d.vtbl,
                      (unsigned long)(d.total / nb), (unsigned long)d.max, (unsigned long)d.calls);
    }
    n += snprintf(report + n, sizeof report - n, "\n");

    FileSystem* fs = platform->getFileSystem();
    auto [ok, fh] = fs->openFile(kPath, gCapture == 0 ? FileSystem::kWriteCreateNew : FileSystem::kWriteAppend);
    if (ok && fh) {
        fs->writeFile(report, (size_t)n, fh);
        fs->closeFile(fh);
    }
    uint32_t whole = stageTotal[kWhole] / nb;
    snprintf(gLine1, sizeof gLine1, "Audio profile %d/%d saved (%s)%s", gCapture + 1, kCaptures, seqStateName(),
             ok ? "" : " - SD write failed");
    if (gCapture + 1 < kCaptures)
        snprintf(gLine2, sizeof gLine2, "avg %lu%%, next at %lu s", (unsigned long)(whole * 100 / blockCycles),
                 (unsigned long)((kFirstMs + (gCapture + 1) * kEveryMs) / 1000));
    else
        snprintf(gLine2, sizeof gLine2, "avg %lu%%, all done: %s", (unsigned long)(whole * 100 / blockCycles),
                 kPath);
}

}  // namespace

void audioProfileNode(int index, const void* node, uint32_t cycles) {
    if (!capturing || index >= kMaxNodes) return;
    Node& d = nodes[index];
    d.obj = node;
    d.vtbl = *(const void* const*)node;
    d.total += cycles;
    if (cycles > d.max) d.max = cycles;
    d.calls++;
    if (index >= nodeCount) nodeCount = index + 1;
}

void audioProfileBlock(uint32_t midi, uint32_t seq, uint32_t players, uint32_t graph) {
    if (!capturing) return;
    const uint32_t v[kStages] = {midi, seq, players, graph, midi + seq + players + graph};
    for (int s = 0; s < kStages; s++) {
        stageTotal[s] += v[s];
        if (v[s] > stageMax[s]) stageMax[s] = v[s];
    }
    const uint32_t b = blocks + 1;
    blocks = b;
    if (b >= kBlocks) capturing = 0;
}

void audioProfileUpdate(Platform* platform) {
    uint32_t now = platform->getTimer()->millis();
    if (gPopup && now - gShownAt >= kPopupMs) {
        mtPopups.hideInfoPopup();
        gPopup = false;
    }
    if (gCapture >= kCaptures) return;
    if (!gWaiting) {
        if (now < kFirstMs + (uint32_t)gCapture * kEveryMs) return;
        startCapture();
        gStartedAt = now;
        gWaiting = true;
        return;
    }
    if (capturing) {
        if (now - gStartedAt > 10000) capturing = 0;  // audio stopped? write what we have
        else return;
    }
    writeReport(platform, now);
    mtPopups.showInfoPopup(gLine1, gLine2);
    gShownAt = platform->getTimer()->millis();
    gPopup = true;
    gWaiting = false;
    gCapture++;
}
#endif
