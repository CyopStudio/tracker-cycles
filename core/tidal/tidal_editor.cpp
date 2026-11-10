// See tidal_editor.h.
#include "tidal_editor.h"

#ifdef TIDAL_EDITOR
#include <stdio.h>
#include <string.h>

#include <new>

#include "Platform.h"
#include "cr_section_macros.h"
#include "mtStructs.h"
#include "tidal_config.h"
#include "tidal_player.h"
#include "../../tidal/editor/editor.h"
#include "../../tidal/editor/layout.h"

namespace {

// SDRAM isn't up for static constructors: raw storage, built in update()
__NOINIT(EXTERNAL_RAM) unsigned char progRaw[sizeof(tc::Program)] __attribute__((aligned(8)));
__NOINIT(EXTERNAL_RAM) unsigned char cpRaw[sizeof(tc::Checkpoint)] __attribute__((aligned(8)));
__NOINIT(EXTERNAL_RAM) unsigned char edRaw[sizeof(tc::Editor)] __attribute__((aligned(8)));
__NOINIT(EXTERNAL_RAM) unsigned char scrRaw[sizeof(tc::CodeScreen)] __attribute__((aligned(8)));
__NOINIT(EXTERNAL_RAM) unsigned char scratchRaw[sizeof(tc::LayoutScratch)] __attribute__((aligned(8)));
__NOINIT(EXTERNAL_RAM) uint8_t undoArena[128 * 1024];
__NOINIT(EXTERNAL_RAM) char saveText[16384];

double nowSeconds();

// ---- stack painting -------------------------------------------------------
extern "C" char* __brkval;    // heap end (teensy core)
extern "C" unsigned long _estack;  // top of RAM
constexpr uint32_t kPaint = 0xC5A3C5A3u;
uint32_t* gPaintLo = nullptr;  // painted words: [gPaintLo, gPaintHi)
uint32_t* gPaintHi = nullptr;

__attribute__((noinline)) void paintStack() {
    volatile uint32_t here = 0;
    uintptr_t lo = ((uintptr_t)__brkval + 1024 + 3) & ~(uintptr_t)3;  // leave malloc some room
    uintptr_t hi = ((uintptr_t)&here - 512) & ~(uintptr_t)3;           // below this frame
    if (hi <= lo) return;
    gPaintLo = (uint32_t*)lo;
    gPaintHi = (uint32_t*)hi;
    for (uint32_t* w = gPaintLo; w < gPaintHi; w++) *w = kPaint;
}

uintptr_t deepestAddr() {
    if (!gPaintLo) return 0;
    uint32_t* w = gPaintLo;
    if ((uintptr_t)__brkval > (uintptr_t)w) w = (uint32_t*)(((uintptr_t)__brkval + 3) & ~(uintptr_t)3);
    while (w < gPaintHi && *w == kPaint) w++;
    return (uintptr_t)w;
}

// The editor's results go to the Tidal player.
class DeviceOutput : public tc::EditorOutput {
public:
    bool evaluate(const tc::Program& p, int slot, tc::Label& msg) override {
        char m[96];
        if (tidalPlayerEvaluate(p, slot, m, sizeof m)) return true;
        msg = m;
        return false;
    }
    void silence(int slot) override { tidalPlayerSilence(slot); }
    void hush() override { tidalPlayerHush(); }
    void mute(int slot, bool on) override { tidalPlayerMute(slot, on); }
    void unmuteAll() override { tidalPlayerUnmuteAll(); }
    void setCps(const tc::Program& p) override { tidalPlayerSetCps(p); }
    bool save(const tc::Program& p, tc::Label& msg) override {
        tc::Renderer r(saveText, sizeof saveText);
        saveText[0] = 0;
        const int n = r.file(p);
        char m[96];
        bool ok;
        if (r.overflow()) {
            snprintf(m, sizeof m, "file over %u bytes: not saved", (unsigned)sizeof saveText);
            ok = false;
        } else {
            ok = tidalPlayerWriteFile(saveText, n, m, sizeof m);
        }
        msg = m;
        return ok;
    }
};

DeviceOutput* gOut = nullptr;  // built with the rest (it has a vtable)
__NOINIT(EXTERNAL_RAM) unsigned char outRaw[sizeof(DeviceOutput)] __attribute__((aligned(8)));
tc::Editor* gEd = nullptr;
Platform* gPlatform = nullptr;
bool gLoaded = false, gDirty = true;
char gStatus[96] = "waiting for the project";

double nowSeconds() { return gPlatform ? gPlatform->getTimer()->micros() * 1e-6 : 0; }

// The project's instruments on the pads: the sample file name without its
// extension, the name `s` uses (the player maps names the same way).
void setInstrumentNames() {
    for (int i = 0; i < tc::kInstrumentSlots && i < kSampleInstrumentsCount; i++) {
        const strInstrument& ins = mtProject.instrument[i];
        char name[16] = "";
        if (ins.isActive == 1) {
            snprintf(name, sizeof name, "%s", ins.sample.file_name);
            if (char* dot = strrchr(name, '.')) *dot = 0;
            for (char* c = name; *c; c++)
                if (*c >= 'A' && *c <= 'Z') *c = (char)(*c - 'A' + 'a');
        }
        gEd->setInstrument(i, name);
    }
}

}  // namespace

tc::Editor* tidalEditorModel() { return gEd; }
bool tidalEditorTakeDirty() {
    const bool d = gDirty;
    gDirty = false;
    return d;
}
void tidalEditorSetDirty() { gDirty = true; }
const char* tidalEditorStatus() { return gStatus; }

int tidalStackDeepest() {
    const uintptr_t a = deepestAddr();
    return a ? (int)((uintptr_t)&_estack - a) : 0;
}
int tidalHeapGap() {
    const uintptr_t a = deepestAddr();
    return a ? (int)(a - (uintptr_t)__brkval) : 0;
}

namespace {
bool gStatusOpen = false;
__NOINIT(EXTERNAL_RAM) char gStatusText[1536];
}  // namespace

bool tidalEditorStatusOpen() { return gStatusOpen; }

const char* tidalEditorStatusLines() {
    tidalEditorStatusText(gStatusText, sizeof gStatusText);
    return gStatusText;
}

int tidalEditorStatusText(char* buf, int cap) {
    int n = 0;
    auto add = [&](const char* fmt, auto... a) {
        if (n < cap - 1) n += snprintf(buf + n, cap - n, fmt, a...);
        if (n > cap - 1) n = cap - 1;
    };
    if (!tidalPlayerStarted()) {
        add("waiting for the project\n");
    } else {
        const float cps = tidalPlayerCps();
        add("cps %d.%03d (%d BPM)   audio %d %%   voices %d / %d\n", (int)cps, (int)(cps * 1000.0f + 0.5f) % 1000,
            (int)(cps * 240.0f + 0.5f), tidalPlayerAudioLoad(), tidalPlayerSounding(), tidalPlayerCap());
    }
    const int deep = tidalStackDeepest(), gap = tidalHeapGap();
    if (deep) add("stack: deepest %d.%d KB below the top of RAM, %d.%d KB to spare above the heap\n", deep / 1024,
                  deep % 1024 * 10 / 1024, gap / 1024, gap % 1024 * 10 / 1024);
    else add("stack: not measured\n");
    add("heap end %d B below the top of RAM\n", (int)((uintptr_t)&_estack - (uintptr_t)__brkval));
    add("%s\n", gStatus);
    int quiet = 0;
    for (int s = 0; s < tc::kSlots; s++) {
        char warn[64];
        const char* err = tidalPlayerSlotError(s);
        const char* w = tidalPlayerSlotWarning(s, warn, sizeof warn);
        if (err) add("d%d  error: %s\n", s + 1, err);
        else if (tidalPlayerSlotPlaying(s)) add("d%d  playing%s%s\n", s + 1, w ? "  - " : "", w ? w : "");
        else quiet++;
    }
    if (quiet == tc::kSlots) add("nothing playing\n");
    return n;
}

void tidalEditorToggleStatus() {
    gStatusOpen = !gStatusOpen;
    gDirty = true;
    if (gStatusOpen && tidalPlayerStarted()) {  // a copy on the card, for the record
        const int n = tidalEditorStatusText(gStatusText, sizeof gStatusText);
        char msg[64];
        tidalPlayerWriteText("/tidal_status.txt", gStatusText, n, msg, sizeof msg);
    }
}

const tc::CodeScreen& tidalEditorLayout(const tc::CodeGeometry& g) {
    tc::CodeScreen& scr = *reinterpret_cast<tc::CodeScreen*>(scrRaw);
    if (gEd) tc::layoutCode(*gEd, nowSeconds(), g, scr, *reinterpret_cast<tc::LayoutScratch*>(scratchRaw));
    else scr.n = 0, scr.box.on = false;
    return scr;
}

void tidalEditorUpdate(Platform* platform) {
    gPlatform = platform;
    if (!gEd) {
        tc::Program* p = new (progRaw) tc::Program();
        tc::Checkpoint* cp = new (cpRaw) tc::Checkpoint();
        gOut = new (outRaw) DeviceOutput();
        gEd = new (edRaw) tc::Editor(*p, *cp, undoArena, (int)sizeof undoArena);
        gEd->setOutput(gOut);
        gEd->setLazyMenus(true);  // menu items checked a few per pass (editor bench v1)
        gDirty = true;
        paintStack();
    }
    if (!gLoaded && tidalPlayerStarted()) {
        gLoaded = true;
        setInstrumentNames();
        const char* text = nullptr;
        int len = 0;
        char msg[96];
        if (tidalPlayerReadFile(text, len, msg, sizeof msg)) {
            tc::Label err;
            if (gEd->loadText(text, len, err)) snprintf(gStatus, sizeof gStatus, "%s loaded", kTidalPlayerFile);
            else snprintf(gStatus, sizeof gStatus, "%s: %s", kTidalPlayerFile, err.c_str());
        } else {
            snprintf(gStatus, sizeof gStatus, "%s (a new file is made on Shift+File)", msg);
        }
        gDirty = true;
    }
    if (gEd->menuOpen() && gEd->checkMenu(3) >= 0) gDirty = true;  // lazily checked menus fill in
    if (gEd->pollHighlights(nowSeconds())) gDirty = true;
}
#endif
