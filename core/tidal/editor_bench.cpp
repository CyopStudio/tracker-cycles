// Editor timing build (see editor_bench.h, docs/tidal_device_editor.md).
// Blocks the main loop while it runs (a few seconds): the screen and SD
// streaming pause, audio keeps running in its interrupt.
//
// Part 1: tidal/core/edit_bench.cpp (edits, undo by text, menus built by
// trial three ways). Part 2: an FT812 test control that draws a screen of
// colour-coded Tidal code the way the device editor would (one CMD_TEXT
// per colour run, cursor / highlight rectangles), timed from
// refreshControl() until the new frame is swapped, with its display-list
// bytes.
#ifdef TIDAL_EDITOR_BENCH
#include "editor_bench.h"

#include <stdio.h>
#include <string.h>

#include <new>

#include "FT812.h"
#include "Platform.h"
#include "Profiler.h"
#include "core/interfacePopups.h"
#include "cr_section_macros.h"
#include "display.h"
#include "tidal_config.h"
#include "tidal_player.h"
#include "../../tidal/core/check.h"
#include "../../tidal/core/edit_bench.h"
#include "../../tidal/core/parse.h"

namespace {

const char* const kPath = "/tidal_editor_bench.txt";
constexpr int kReps = 4;
constexpr uint32_t kRun1Ms = 10000, kReloadMs = 30000, kRun2Ms = 40000;

__NOINIT(EXTERNAL_RAM) unsigned char progA[sizeof(tc::Program)] __attribute__((aligned(8)));
__NOINIT(EXTERNAL_RAM) unsigned char progB[sizeof(tc::Program)] __attribute__((aligned(8)));
__NOINIT(EXTERNAL_RAM) unsigned char progC[sizeof(tc::Program)] __attribute__((aligned(8)));
__NOINIT(EXTERNAL_RAM) tc::Span spans[tc::kMaxNodes];
__NOINIT(EXTERNAL_RAM) char text[8192];
__NOINIT(EXTERNAL_RAM) char report[16384];

Platform* gPlatform = nullptr;
uint32_t clockUs() { return gPlatform->getTimer()->micros(); }

// ---- code screen ----------------------------------------------------------------
constexpr int kLines = 16, kCols = 64;
constexpr int kX = 12, kY = 36, kLineH = 20;
// colour classes (the prototype view's palette)
enum : uint8_t { cText, cFn, cNum, cWord, cQuote };
const uint32_t kPalette[5] = {0xD8DCE2, 0x79B8FF, 0xFFB86B, 0x9FE08F, 0x6FA868};
__NOINIT(EXTERNAL_RAM) char lineText[kLines][kCols + 1];
__NOINIT(EXTERNAL_RAM) uint8_t lineCol[kLines][kCols];
int gLineCount = 0;
int gMode = 0;          // 0 colour runs, 1 one colour per line, 2 colour + highlights
int gDrawLines = kLines;
constexpr int kHighlights = 12;
struct Rect {
    int16_t x, y, w, h;
};
Rect gHl[kHighlights];
int gGlyphs = 0, gRuns = 0, gLinesDrawn = 0;
// A control owns 8 KB of RAM_G for its display list (all stages): stop
// before a conservative estimate gets near it (glyph 4 B, a run's colour +
// CMD_TEXT header ~32 B, a rectangle ~20 B). The report gives the real size.
constexpr int kBudget = 6500;
int gEst = 0;
bool gTrunc = false;

const strFont* codeFont() { return &fonts[0]; }
int advance() {
    const strFont* f = codeFont();
    return f->data[(uint8_t)'m'] ? f->data[(uint8_t)'m'] : 9;
}

void rect(int x, int y, int w, int h, uint32_t c) {
    gEst += 20;
    API_COLOR(c);
    API_BEGIN(RECTS);
    API_VERTEX2F(x, y);
    API_VERTEX2F(x + w, y + h);
    API_END();
}

void drawLines(int from, int to) {
    const int adv = advance();
    const int16_t fh = codeFont()->handle;
    char run[kCols + 1];
    for (int l = from; l < to && l < gLineCount && l < gDrawLines && !gTrunc; l++) {
        const int y = kY + l * kLineH;
        if (l == 2) rect(kX + 5 * adv, y - 1, 12 * adv, kLineH, 0x2F5FA8);  // cursor
        const char* s = lineText[l];
        const int n = (int)strlen(s);
        int i = 0;
        while (i < n) {
            int j = i + 1;
            if (gMode != 1)
                while (j < n && lineCol[l][j] == lineCol[l][i]) j++;
            else
                j = n;
            if (gEst + 32 + 4 * (j - i) > kBudget) {
                gTrunc = true;
                return;
            }
            gEst += 32 + 4 * (j - i);
            memcpy(run, s + i, j - i);
            run[j - i] = 0;
            API_COLOR(kPalette[gMode == 1 ? cText : lineCol[l][i]]);
            API_CMD_TEXT(kX + i * adv, y, fh, 0, run);
            gRuns++;
            gGlyphs += j - i;
            i = j;
        }
        gLinesDrawn = l + 1;
    }
}

class TcCodeView;
class TcHighlights;
TcCodeView* gCodeView = nullptr;
TcHighlights* gHlView = nullptr;

class TcCodeView : public cDisplayControl {
  public:
    explicit TcCodeView(strControlProperties* p) : cDisplayControl(controlIdSimpleText, 2) {
        style = p ? p->style : 0;
        gCodeView = this;
    }
    ~TcCodeView() override { gCodeView = nullptr; }
    void update() override {
        gRuns = gGlyphs = gLinesDrawn = gEst = 0;
        gTrunc = false;
        rect(kX - 6, kY - 4, 780, kLines * kLineH + 8, 0x16181C);  // code area background
        if (gMode == 2)
            for (const Rect& r : gHl) rect(r.x, r.y, r.w, r.h, 0x8A5A12);
        drawLines(0, kLines / 2);
    }
    void update2() override { drawLines(kLines / 2, kLines); }
    void setStyle(uint32_t s) override { style = s; }
    void setText(const char*) override {}
    void setValue(int) override {}
    void setColors(const uint32_t*) override {}
    void setData(void*) override {}
    int bytes() const { return ramSize; }
};

// highlights alone, as a separate control (redrawn without the code)
class TcHighlights : public cDisplayControl {
  public:
    explicit TcHighlights(strControlProperties* p) : cDisplayControl(controlIdSimpleText, 1) {
        style = p ? p->style : 0;
        gHlView = this;
    }
    ~TcHighlights() override { gHlView = nullptr; }
    void update() override {
        API_COLOR_A(128);
        for (const Rect& r : gHl) rect(r.x, r.y, r.w, r.h, 0xFF9F1C);
        API_COLOR_A(255);
    }
    void setStyle(uint32_t s) override { style = s; }
    void setText(const char*) override {}
    void setValue(int) override {}
    void setColors(const uint32_t*) override {}
    void setData(void*) override {}
    int bytes() const { return ramSize; }
};

uint8_t classOf(tc::Kind k) {
    switch (k) {
        case tc::Kind::Fn: return cFn;
        case tc::Kind::Num:
        case tc::Kind::MNum: return cNum;
        case tc::Kind::MWord:
        case tc::Kind::MNote:
        case tc::Kind::MChord:
        case tc::Kind::MBool: return cWord;
        default: return cText;
    }
}

void paint(const tc::Program& p, uint16_t idx, uint8_t* cls, int n) {
    const tc::Node& nd = p.at(idx);
    const tc::Span& s = spans[idx];
    if (s.end > s.start) {
        if (nd.kind == tc::Kind::Mini) {
            if (s.start < n) cls[s.start] = cQuote;
            if (s.end - 1 < n) cls[s.end - 1] = cQuote;
        } else if (uint8_t c = classOf(nd.kind)) {
            for (int i = s.start; i < s.end && i < n; i++) cls[i] = c;
        }
    }
    for (uint16_t ch = nd.first; ch; ch = p.at(ch).next) paint(p, ch, cls, n);
}

// Lay the bench blocks out as the editor would: rendered with spans,
// coloured by node kind, hard-wrapped at kCols, until the screen is full.
void buildScreen(tc::Program& p) {
    __NOINIT(EXTERNAL_RAM) static char file[2048];
    int n = 0;
    for (int i = 0; i < 6; i++)
        n += snprintf(file + n, sizeof file - n, "d%d $ %s\n\n", i + 1,
                      tc::editBenchCase(i % tc::editBenchCaseCount()));
    tc::Error e;
    gLineCount = 0;
    if (!tc::parseFile(p, file, n, e) || !tc::checkProgram(p, e)) return;
    __NOINIT(EXTERNAL_RAM) static uint8_t cls[8192];
    for (int b = 0; b < p.blockCount() && gLineCount < kLines; b++) {
        memset(spans, 0, sizeof spans);
        tc::Renderer r(text, sizeof text, spans);
        const int len = r.blockNode(p, p.blockAt(b));
        memset(cls, cText, (size_t)len);
        paint(p, p.blockAt(b), cls, len);
        for (int at = 0; at < len && gLineCount < kLines; at += kCols) {
            const int k = len - at < kCols ? len - at : kCols;
            memcpy(lineText[gLineCount], text + at, (size_t)k);
            lineText[gLineCount][k] = 0;
            memcpy(lineCol[gLineCount], cls + at, (size_t)k);
            gLineCount++;
        }
    }
    // highlights: on the first 12 word / number runs of the screen
    const int adv = advance();
    int h = 0;
    for (int l = 0; l < gLineCount && h < kHighlights; l++)
        for (int i = 0; lineText[l][i] && h < kHighlights; i++)
            if ((lineCol[l][i] == cWord || lineCol[l][i] == cNum) && (i == 0 || lineCol[l][i - 1] != lineCol[l][i])) {
                int j = i;
                while (lineText[l][j] && lineCol[l][j] == lineCol[l][i]) j++;
                gHl[h++] = Rect{(int16_t)(kX + i * adv), (int16_t)(kY + l * kLineH - 1), (int16_t)((j - i) * adv),
                                (int16_t)kLineH};
            }
    for (; h < kHighlights; h++) gHl[h] = Rect{0, 0, 0, 0};
}

bool displayIdle() { return display.isIdle() && display.getDisplayDriver()->isCoProEmpty(); }

void drain() {
    const uint32_t t0 = clockUs();
    while (!displayIdle() && clockUs() - t0 < 500000) display.update();
}

struct DrawStat {
    uint32_t avgUs = 0, maxUs = 0, workUs = 0;
};

// refreshControl -> loop display.update() until the frame is swapped
DrawStat timeRefresh(hControl h, int reps) {
    DrawStat st;
    uint32_t sum = 0, work = 0;
    for (int r = 0; r < reps; r++) {
        drain();
        const uint32_t t0 = clockUs();
        display.refreshControl(h);
        while (clockUs() - t0 < 500000) {
            const uint32_t a = clockUs();
            display.update();
            const uint32_t d = clockUs() - a;
            if (d > 15) work += d;  // calls that did something (not just "coprocessor busy")
            if (displayIdle()) break;
        }
        const uint32_t us = clockUs() - t0;
        sum += us;
        if (us > st.maxUs) st.maxUs = us;
    }
    st.avgUs = sum / reps;
    st.workUs = work / reps;
    return st;
}

int displayTest(char* out, int cap) {
    tc::Program* p = reinterpret_cast<tc::Program*>(progC);
    buildScreen(*p);
    int n = 0;
    const strFont* f = codeFont();
    n += snprintf(out + n, cap - n,
                  "display: font 0 handle %d, 'm' %d px, 'i' %d px (%s); %d lines of up to %d columns\n",
                  (int)f->handle, (int)f->data[(uint8_t)'m'], (int)f->data[(uint8_t)'i'],
                  f->data[(uint8_t)'m'] == f->data[(uint8_t)'i'] ? "monospaced" : "proportional: Config font = mono "
                                                                                 "for the real editor",
                  gLineCount, kCols);
    if (!gLineCount) return n + snprintf(out + n, cap - n, "display: screen text failed to build\n");

    strControlProperties prop;
    prop.style = controlStyleShow;
    const hControl code = display.createControl<TcCodeView>(&prop);
    const hControl hl = display.createControl<TcHighlights>(&prop);
    if (code < 0 || hl < 0) {
        if (code >= 0) display.destroyControl(code);
        if (hl >= 0) display.destroyControl(hl);
        return n + snprintf(out + n, cap - n, "display: no free control slot\n");
    }
    display.setControlHide(hl);
    struct V {
        const char* name;
        int mode, lines;
    } vars[] = {
        {"code, colour runs, 16 lines", 0, 16},
        {"code, one colour per line, 16 lines", 1, 16},
        {"code, colour runs + 12 highlight rects", 2, 16},
        {"code, colour runs, 8 lines", 0, 8},
    };
    n += snprintf(out + n, cap - n, "%-42s %8s %8s %8s %6s %6s %6s %s\n", "frame", "avg us", "max us", "work us",
                  "bytes", "glyphs", "runs", "lines");
    for (const V& v : vars) {
        gMode = v.mode;
        gDrawLines = v.lines;
        const DrawStat st = timeRefresh(code, 10);
        // the control's display list (both stages), after the refresh
        const int bytes = gCodeView ? gCodeView->bytes() : -1;
        n += snprintf(out + n, cap - n, "%-42s %8lu %8lu %8lu %6d %6d %6d %d%s\n", v.name, (unsigned long)st.avgUs,
                      (unsigned long)st.maxUs, (unsigned long)st.workUs, bytes, gGlyphs, gRuns, gLinesDrawn,
                      gTrunc ? " (budget)" : "");
    }
    // highlights only: the code stays, the small control is redrawn
    gMode = 0;
    gDrawLines = kLines;
    display.setControlShow(hl);
    const DrawStat st = timeRefresh(hl, 10);
    n += snprintf(out + n, cap - n, "%-42s %8lu %8lu %8lu %6d\n", "highlights only (separate control)",
                  (unsigned long)st.avgUs, (unsigned long)st.maxUs, (unsigned long)st.workUs,
                  gHlView ? gHlView->bytes() : -1);
    display.destroyControl(hl);
    display.destroyControl(code);
    display.forceAppedStage();
    drain();
    return n;
}

bool save(bool create, int n) {
    FileSystem* fs = gPlatform->getFileSystem();
    auto [o, fh] = fs->openFile(kPath, create ? FileSystem::kWriteCreateNew : FileSystem::kWriteAppend);
    bool ok = o && fh;
    if (ok) {
        fs->writeFile(report, (size_t)n, fh);
        fs->closeFile(fh);
    }
    return ok;
}

bool runOnce(int run) {
    static bool built = false;
    if (!built) {
        new (progA) tc::Program();
        new (progB) tc::Program();
        new (progC) tc::Program();
        built = true;
    }
    Profiler::AudioProcessorUsage u{};
    Profiler::getInstance()->getAudioProcessingUsagePercent(u, true);
    int n = snprintf(report, sizeof report,
                     "Tracker Cycles editor bench v1, run %d (%lu s after boot), Tidal player %s\n"
                     "times in microseconds (wall, audio interrupt included); Programs in SDRAM\n\n",
                     run, (unsigned long)(clockUs() / 1000000), tidalPlayerAnyPlaying() ? "playing" : "silent");
    n += tc::runEditBench(reinterpret_cast<tc::Program*>(progA), reinterpret_cast<tc::Program*>(progB),
                          reinterpret_cast<tc::Program*>(progC), spans, text, sizeof text, clockUs, kReps,
                          report + n, (int)sizeof report - n);
    Profiler::getInstance()->getAudioProcessingUsagePercent(u, true);
    n += snprintf(report + n, sizeof report - n, "audio during part 1: max %d %%, avg %d %%\n\n", u.tempMaximum,
                  u.average);
    bool ok = save(run == 1, n);
    mtPopups.hideInfoPopup();
    drain();
    n = displayTest(report, sizeof report);
    Profiler::getInstance()->getAudioProcessingUsagePercent(u, true);
    n += snprintf(report + n, sizeof report - n, "audio during the display test: max %d %%, avg %d %%\n\n",
                  u.tempMaximum, u.average);
    return save(false, n) && ok;
}

int gStage = 0;
uint32_t gShownAt = 0;
bool gResultShown = false;

}  // namespace

void tidalEditorBenchUpdate(Platform* platform) {
    gPlatform = platform;
    const uint32_t now = platform->getTimer()->millis();
    if (gResultShown && now - gShownAt >= 4000) {
        mtPopups.hideInfoPopup();
        gResultShown = false;
    }
    switch (gStage) {
        case 0:  // run 1: silent
            if (now < kRun1Ms) return;
            tidalPlayerHush();
            mtPopups.showInfoPopup("Tidal editor bench 1 running...", "screen freezes for a few s");
            gShownAt = now;
            gStage = 1;
            return;
        case 1:
            if (now - gShownAt < 500) return;
            {
                const bool ok = runOnce(1);
                mtPopups.showInfoPopup("Editor bench 1 done.", ok ? "/project.tidal plays at 30 s, bench 2 at 40 s"
                                                                  : "SD write failed");
            }
            gShownAt = platform->getTimer()->millis();
            gResultShown = true;
            gStage = 2;
            return;
        case 2:  // start the card's /project.tidal for run 2
            if (now < kReloadMs) return;
            tidalPlayerReload();
            gStage = 3;
            return;
        case 3:
            if (now < kRun2Ms) return;
            mtPopups.showInfoPopup("Tidal editor bench 2 running...", "screen freezes for a few s");
            gShownAt = now;
            gStage = 4;
            return;
        case 4:
            if (now - gShownAt < 500) return;
            {
                const bool ok = runOnce(2);
                mtPopups.showInfoPopup("Editor bench 2 done:", ok ? "/tidal_editor_bench.txt" : "SD write failed");
            }
            gShownAt = platform->getTimer()->millis();
            gResultShown = true;
            gStage = 5;
            return;
        default: return;
    }
}
#endif
