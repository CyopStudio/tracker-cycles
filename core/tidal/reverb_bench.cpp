// Throwaway on-device reverb bench (see reverb_bench.h).
//
// Question: how much of the reverb's cost (~18 % of the audio block in the
// audio profiler) is its delay memory in uncached SDRAM, and what does each
// way of trading internal RAM (or quality) for CPU buy? Each audio block
// while capturing, ONE variant processes a 128-sample stereo test block
// (round robin), timed with the cycle counter; the output is discarded and
// the live reverb is untouched. Captures at kFirstMs + k * kEveryMs; change
// the play state between them (popups say what to do).
//
// Variants (rows of the report):
//   0-3  the stock Reverb class (Polyverb/reverb.h, as shipped) with its two
//        32 KB buffers in SDRAM (as shipped) / buffer 1 internal / buffer 2
//        internal / both internal. "Both internal" points both buffers at
//        the SAME internal 32 KB (the sound is garbage, the memory access
//        pattern and cost are the same; 64 KB internal is not available).
//   4-8  BenchReverb below: the same topology and line lengths, with
//        - the 4 input diffusers (1210 samples) optionally in a separate
//          small internal buffer ("split"),
//        - optionally at half the sample rate: the input is decimated by 2
//          (pair average), all line lengths halved (same reverb times, so
//          half the memory), output upsampled (linear), dry/wet mixed at
//          the full rate. Row 4 (full rate, SDRAM) checks BenchReverb's own
//          overhead against row 0.
// BenchReverb's Process follows Polyverb's reverb.h (MIT, Emilie Gillet /
// Wojciech Jakobczyk), reorganised so the wet signal comes out separately.
#ifdef REVERB_BENCH
#include "reverb_bench.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../libs/Audio/Polyverb/reverb.h"
#include "Platform.h"
#include "Profiler.h"
#include "core/interfacePopups.h"
#include "cr_section_macros.h"
#include "mtSequencer.h"

namespace {

#define RB_CYCLES() (*(volatile uint32_t*)0xE0001004u)

constexpr int kN = 128;  // AUDIO_BLOCK_SAMPLES

// ---- the variant engine -------------------------------------------------------
template<int kDiv, bool kSplit>
class BenchReverb {
    static constexpr int32_t L(int32_t n) { return (n + kDiv - 1) / kDiv; }
    typedef FxEngine<16384 / kDiv, FORMAT_16_BIT> E;  // buffer 1: left loop + predelay (+ diffusers)
    typedef FxEngine<2048 / kDiv, FORMAT_16_BIT> D;   // diffusers when kSplit (buffer 1 of D only)

public:
    // buf1 / buf2: (16384 / kDiv) samples each; dbuf: (2048 / kDiv) if kSplit
    void init(uint16_t* buf1, uint16_t* buf2, uint16_t* dbuf) {
        e_.Init(buf1, buf2);
        e_.SetLFOFrequency(LFO_1, 0.5f * kDiv / 32000.0f);
        e_.SetLFOFrequency(LFO_2, 0.3f * kDiv / 32000.0f);
        if (kSplit) d_.Init(dbuf, dbuf);
        lp0_ = lp1_ = lp2_ = 0;
    }
    static constexpr uint32_t buf1Bytes() { return 16384 / kDiv * 2; }
    static constexpr uint32_t dBytes() { return kSplit ? 2048 / kDiv * 2 : 0; }

    // in: mono input (already gain-scaled, -1..1); outputs the wet signals
    void process(const float* in, float* wetL, float* wetR, int n) {
        typedef typename E::template Reserve<
          L(150),
          typename E::template Reserve<
            L(214),
            typename E::template Reserve<
              L(319),
              typename E::template Reserve<
                L(527),
                typename E::template Reserve<
                  L(2182),
                  typename E::template Reserve<
                    L(2690),
                    typename E::template Reserve<L(4501), typename E::template Reserve<L(5000)>>>>>>>>
          Memory;
        typedef typename E::template Reserve<
          L(2525), typename E::template Reserve<L(2197), typename E::template Reserve<L(6312)>>>
          Memory2;
        typedef typename D::template Reserve<
          L(150),
          typename D::template Reserve<L(214),
                                       typename D::template Reserve<L(319), typename D::template Reserve<L(527)>>>>
          DMemory;
        typename E::template DelayLine<Memory, 0> ap1;
        typename E::template DelayLine<Memory, 1> ap2;
        typename E::template DelayLine<Memory, 2> ap3;
        typename E::template DelayLine<Memory, 3> ap4;
        typename E::template DelayLine<Memory, 4> dap1a;
        typename E::template DelayLine<Memory, 5> dap1b;
        typename E::template DelayLine<Memory, 6> del1;
        typename E::template DelayLine<Memory, 7> del0;
        typename E::template DelayLine<Memory2, 0> dap2a;
        typename E::template DelayLine<Memory2, 1> dap2b;
        typename E::template DelayLine<Memory2, 2> del2;
        typename D::template DelayLine<DMemory, 0> dp1;
        typename D::template DelayLine<DMemory, 1> dp2;
        typename D::template DelayLine<DMemory, 2> dp3;
        typename D::template DelayLine<DMemory, 3> dp4;
        typename E::Context c;
        typename D::Context cd;

        const float kap = diffusion_, klp = lp_, krt = time_;
        const int32_t pre = predelay_ / kDiv;
        float lp0 = lp0_, lp1 = lp1_, lp2 = lp2_;
        for (int i = 0; i < n; i++) {
            float wet, apout = 0.0f;
            e_.Start(&c);
            c.Load(in[i]);
            c.Write(del0, 0.0f);
            c.Read(del0, pre, 1.0f);
            c.Lp(lp0, klp);
            if (kSplit) {
                d_.StartNoLFO(&cd);
                cd.Load(c.accumulator_);
                cd.Read(dp1 TAIL, kap);
                cd.WriteAllPass(dp1, -kap);
                cd.Read(dp2 TAIL, kap);
                cd.WriteAllPass(dp2, -kap);
                cd.Read(dp3 TAIL, kap);
                cd.WriteAllPass(dp3, -kap);
                cd.Read(dp4 TAIL, kap);
                cd.WriteAllPass(dp4, -kap);
                cd.Write(apout);
            } else {
                c.Read(ap1 TAIL, kap);
                c.WriteAllPass(ap1, -kap);
                c.Read(ap2 TAIL, kap);
                c.WriteAllPass(ap2, -kap);
                c.Read(ap3 TAIL, kap);
                c.WriteAllPass(ap3, -kap);
                c.Read(ap4 TAIL, kap);
                c.WriteAllPass(ap4, -kap);
                c.Write(apout);
            }
            c.Load(apout);
            c.Interpolate2(del2, 6211.0f / kDiv, LFO_2, 100.0f / kDiv, krt);
            c.Lp(lp1, klp);
            c.Read(dap1a TAIL, -kap);
            c.WriteAllPass(dap1a, kap);
            c.Read(dap1b TAIL, kap);
            c.WriteAllPass(dap1b, -kap);
            c.Write(del1, 2.0f);
            c.Write(wet, 0.0f);
            wetL[i] = wet;

            c.Load(apout);
            c.Read(del1 TAIL, krt);
            c.Lp(lp2, klp);
            c.Read2(dap2a TAIL, kap);
            c.WriteAllPass2(dap2a, -kap);
            c.Read2(dap2b TAIL, -kap);
            c.WriteAllPass2(dap2b, kap);
            c.Write2(del2, 2.0f);
            c.Write(wet, 0.0f);
            wetR[i] = wet;
        }
        lp0_ = lp0;
        lp1_ = lp1;
        lp2_ = lp2;
    }

    // One audio block in place, dry/wet mixed like Reverb::Process.
    void block(int16_t* left, int16_t* right) {
        constexpr int m = kN / kDiv;
        float in[m], wl[m], wr[m];
        for (int j = 0; j < m; j++) {
            float s = 0;
            for (int k = 0; k < kDiv; k++) s += (float)left[j * kDiv + k] + (float)right[j * kDiv + k];
            in[j] = gain_ * s / (32768.0f * kDiv);
        }
        process(in, wl, wr, m);
        float sum = 0;
        for (int i = 0; i < kN; i++) {
            float a, b;
            if (kDiv == 1) {
                a = wl[i];
                b = wr[i];
            } else {  // linear: odd samples on the new value, even between
                const int j = i >> 1;
                if (i & 1) {
                    a = wl[j];
                    b = wr[j];
                } else {
                    a = 0.5f * (prevL_ + wl[j]);
                    b = 0.5f * (prevR_ + wr[j]);
                    prevL_ = wl[j];
                    prevR_ = wr[j];
                }
            }
            float d = (a * 32768 - left[i]) * amount_;
            left[i] = (int16_t)Clip16((int32_t)(left[i] + d));
            sum += fabsf(d);
            d = (b * 32768 - right[i]) * amount_;
            right[i] = (int16_t)Clip16((int32_t)(right[i] + d));
            sum += fabsf(d);
        }
        lastDelta_ = sum / kN;
    }

private:
    E e_;
    D d_;
    float lp0_ = 0, lp1_ = 0, lp2_ = 0, prevL_ = 0, prevR_ = 0, lastDelta_ = 0;
    // the stock node's settings (effect_polyverb.cpp), predelay 0.3 of 5000
    float gain_ = 0.1f, diffusion_ = 0.625f, time_ = 0.7f, lp_ = 0.2f, amount_ = 0.8f;
    int32_t predelay_ = 1500;
};

// ---- memory -----------------------------------------------------------------------
uint16_t intPool[16384];  // 32 KB internal (.bss)
uint16_t intDiff[2048];   // 4 KB internal (.bss)
__NOINIT(EXTERNAL_RAM) uint16_t sdPool1[16384];
__NOINIT(EXTERNAL_RAM) uint16_t sdPool2[16384];

// stock Reverb objects (internal RAM; their constructors only set nothing)
Reverb stock[4];
BenchReverb<1, false> fullSd;
BenchReverb<1, true> fullSplit;
BenchReverb<2, false> halfSd;
BenchReverb<2, true> halfSplit;
BenchReverb<2, false> halfInt;

struct Row {
    const char* name;
    uint32_t intBytes, sdBytes;
};
constexpr int kRows = 9;
const Row kRow[kRows] = {
  {"stock, both buffers SDRAM (as shipped)", 0, 65536},
  {"stock, buffer 1 internal", 32768, 32768},
  {"stock, buffer 2 internal", 32768, 32768},
  {"stock, both internal (aliased)", 65536, 0},
  {"bench, full rate, SDRAM (= row 0?)", 0, 65536},
  {"full rate, diffusers internal", 4096, 65536},
  {"half rate, SDRAM", 0, 32768},
  {"half rate, diffusers internal", 2048, 32768},
  {"half rate, all internal", 32768, 0},
};

struct Stat {
    uint32_t n;
    uint64_t total;
    uint32_t max;
};
Stat stat[kRows];

int16_t srcL[kN], srcR[kN], bufL[kN], bufR[kN];
volatile bool capturing = false;
volatile uint32_t blocks = 0;
int nextRow = 0;
bool ready = false;

constexpr uint32_t kBlocksPerCapture = kRows * 400;  // ~10.4 s
constexpr int kCaptures = 3;
constexpr uint32_t kFirstMs = 20000, kEveryMs = 40000, kPopupMs = 6000;
const char* const kPhase[kCaptures] = {"stopped (nothing playing)", "light playback", "heavy playback"};
const char* const kNextHint[kCaptures] = {"next: play a light pattern (1-3 tracks)",
                                          "next: play your heaviest pattern", "done"};
const char* const kPath = "/reverb_bench.txt";
__NOINIT(EXTERNAL_RAM) char report[8192];

int gCapture = 0;
bool gWaiting = false, gPopup = false;
uint32_t gStartedAt = 0, gShownAt = 0;
char gLine1[64], gLine2[64];

void setup() {
    memset(intPool, 0, sizeof intPool);
    memset(intDiff, 0, sizeof intDiff);
    memset(sdPool1, 0, sizeof sdPool1);
    memset(sdPool2, 0, sizeof sdPool2);
    uint16_t* const b1[4] = {sdPool1, intPool, sdPool1, intPool};
    uint16_t* const b2[4] = {sdPool2, sdPool2, intPool, intPool};
    for (int i = 0; i < 4; i++) {
        stock[i].Init(b1[i], b2[i]);
        stock[i].SetInputGain(0.1f);
        stock[i].SetDiffusion(0.625f);
        stock[i].SetTime(0.7f);
        stock[i].SetLP(0.2f);
        stock[i].SetAmount(0.8f);
        stock[i].SetPredelayLength(0.3f);
    }
    fullSd.init(sdPool1, sdPool2, nullptr);
    fullSplit.init(sdPool1, sdPool2, intDiff);
    halfSd.init(sdPool1, sdPool2, nullptr);
    halfSplit.init(sdPool1, sdPool2, intDiff);
    halfInt.init(intPool, intPool + 8192, nullptr);
    // test signal: decaying noise bursts (the cost does not depend on it)
    uint32_t r = 12345;
    for (int i = 0; i < kN; i++) {
        r = r * 1664525u + 1013904223u;
        srcL[i] = (int16_t)((int32_t)(r >> 16) - 32768) / 4;
        r = r * 1664525u + 1013904223u;
        srcR[i] = (int16_t)((int32_t)(r >> 16) - 32768) / 4;
    }
}

void runRow(int row) {
    switch (row) {
        case 0:
        case 1:
        case 2:
        case 3: stock[row].Process(bufL, bufR, kN); break;
        case 4: fullSd.block(bufL, bufR); break;
        case 5: fullSplit.block(bufL, bufR); break;
        case 6: halfSd.block(bufL, bufR); break;
        case 7: halfSplit.block(bufL, bufR); break;
        default: halfInt.block(bufL, bufR); break;
    }
}

void writeReport(Platform* platform, uint32_t now) {
    Profiler::AudioProcessorUsage u{};
    Profiler::getInstance()->getAudioProcessingUsagePercent(u, true);
    const uint32_t perUs = platform->getTimer()->clockCyclesPerUs();
    const uint32_t blockCycles = perUs * 2902;
    int n = 0;
    if (gCapture == 0)
        n += snprintf(report + n, sizeof report - n,
                      "Tracker Cycles reverb bench v1: one variant per audio block, test signal, output discarded\n"
                      "(cycles per 128-sample block incl. dry/wet mix; %% of the 2902 us block = %lu cycles)\n\n",
                      (unsigned long)blockCycles);
    n += snprintf(report + n, sizeof report - n,
                  "=== capture %d of %d (%s), %lu s after boot, sequencer %s\n"
                  "audio load without the bench (Profiler): avg %d %%, max %d %%\n"
                  "row %-40s %7s %7s %8s %8s %6s %6s\n",
                  gCapture + 1, kCaptures, kPhase[gCapture], (unsigned long)(now / 1000),
                  sequencer.getSeqState() == Sequencer::SEQ_STATE_STOP ? "stopped" : "playing", u.average,
                  u.tempMaximum, "variant", "int KB", "SD KB", "avg cyc", "max cyc", "avg %", "runs");
    for (int i = 0; i < kRows; i++) {
        const Stat& s = stat[i];
        const uint32_t avg = s.n ? (uint32_t)(s.total / s.n) : 0;
        n += snprintf(report + n, sizeof report - n, "%3d %-40s %7lu %7lu %8lu %8lu %3lu.%lu %6lu\n", i,
                      kRow[i].name, (unsigned long)(kRow[i].intBytes / 1024), (unsigned long)(kRow[i].sdBytes / 1024),
                      (unsigned long)avg, (unsigned long)s.max, (unsigned long)(avg * 100 / blockCycles),
                      (unsigned long)(avg * 1000 / blockCycles % 10), (unsigned long)s.n);
    }
    n += snprintf(report + n, sizeof report - n, "\n");

    FileSystem* fs = platform->getFileSystem();
    auto [ok, fh] = fs->openFile(kPath, gCapture == 0 ? FileSystem::kWriteCreateNew : FileSystem::kWriteAppend);
    if (ok && fh) {
        fs->writeFile(report, (size_t)n, fh);
        fs->closeFile(fh);
    }
    const uint32_t a0 = stat[0].n ? (uint32_t)(stat[0].total / stat[0].n) : 0;
    snprintf(gLine1, sizeof gLine1, "Reverb bench %d/%d saved%s", gCapture + 1, kCaptures,
             ok ? "" : " - SD write failed");
    snprintf(gLine2, sizeof gLine2, "stock %lu%%; %s", (unsigned long)(a0 * 100 / blockCycles),
             kNextHint[gCapture]);
}

}  // namespace

void reverbBenchBlock() {
    if (!capturing) return;
    const int row = nextRow;
    nextRow = row + 1 < kRows ? row + 1 : 0;
    memcpy(bufL, srcL, sizeof bufL);
    memcpy(bufR, srcR, sizeof bufR);
    const uint32_t t0 = RB_CYCLES();
    runRow(row);
    const uint32_t c = RB_CYCLES() - t0;
    Stat& s = stat[row];
    s.n++;
    s.total += c;
    if (c > s.max) s.max = c;
    const uint32_t b = blocks + 1;
    blocks = b;
    if (b >= kBlocksPerCapture) capturing = false;
}

void reverbBenchUpdate(Platform* platform) {
    const uint32_t now = platform->getTimer()->millis();
    if (!ready) {
        setup();
        ready = true;
    }
    if (gPopup && now - gShownAt >= kPopupMs) {
        mtPopups.hideInfoPopup();
        gPopup = false;
    }
    if (gCapture >= kCaptures) return;
    if (!gWaiting) {
        if (now < kFirstMs + (uint32_t)gCapture * kEveryMs) return;
        memset(stat, 0, sizeof stat);
        blocks = 0;
        nextRow = 0;
        Profiler::AudioProcessorUsage u{};
        Profiler::getInstance()->getAudioProcessingUsagePercent(u, true);  // reset the max
        capturing = true;
        gStartedAt = now;
        gWaiting = true;
        return;
    }
    if (capturing) {
        if (now - gStartedAt > 30000) capturing = false;  // audio stopped? write what we have
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
