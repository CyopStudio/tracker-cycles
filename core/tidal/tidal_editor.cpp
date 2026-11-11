// See tidal_editor.h.
#include "tidal_editor.h"

#ifdef TIDAL_EDITOR
#include <stdio.h>
#include <string.h>

#include <new>

#include "Platform.h"
#include "cr_section_macros.h"
#include "fileManager.h"
#include "mtStructs.h"
#include "tidal_config.h"
#include "tidal_folders.h"
#include "tidal_player.h"
#include "../../tidal/core/engine.h"
#include "../../tidal/core/mapping.h"
#include "../../tidal/core/scheduler.h"
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
// highlighting: live program node -> the editor's node (set at evaluate:
// a block and its live copy list their steps in the same order)
__NOINIT(EXTERNAL_RAM) uint16_t gLiveToEd[tc::kMaxNodes];
constexpr int kMaxSteps = 512;
__NOINIT(EXTERNAL_RAM) uint16_t gLiveSteps[kMaxSteps];
__NOINIT(EXTERNAL_RAM) uint16_t gEdSteps[kMaxSteps];
bool gLitDirty = false;

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

Platform* gPlatform = nullptr;

// The editor's results go to the Tidal player.
class DeviceOutput : public tc::EditorOutput {
public:
    bool evaluate(const tc::Program& p, int slot, tc::Label& msg) override {
        char m[96];
        uint16_t liveRoot = 0;
        if (!tidalPlayerEvaluate(p, slot, m, sizeof m, &liveRoot)) {
            msg = m;
            return false;
        }
        const tc::Program* live = tidalPlayerLive();
        if (live && liveRoot && p.block[slot]) {
            const int nl = tc::atomOrder(*live, liveRoot, gLiveSteps, kMaxSteps);
            const int ne = tc::atomOrder(p, p.at(p.block[slot]).first, gEdSteps, kMaxSteps);
            for (int k = 0; k < nl && k < ne; k++)
                if (gLiveSteps[k] < tc::kMaxNodes) gLiveToEd[gLiveSteps[k]] = gEdSteps[k];
        }
        return true;
    }
    // Tidal sample folders (/Tidal/<name>/ -> a sliced instrument, tidal_folders.h)
    int folders() override { return tidalFolderCount(gPlatform); }
    const char* folderName(int i) override { return tidalFolderName(i); }
    bool loadFolder(const char* name, tc::Label& msg) override {
        char m[96];
        const bool ok = tidalFolderLoad(gPlatform, name, m, sizeof m);
        msg = m;
        return ok;
    }
    // Steps of the entries the scheduler played, on nowSeconds()' clock.
    int litSteps(Lit* out, int cap) override {
#if TC_EVENT_SRC
        tc::LitRecord recs[16];
        int want = cap / tc::kLitSrc;
        if (want > 16) want = 16;
        if (want < 1) return 0;
        int64_t nowSample = 0;
        const int n = tidalPlayerTakeLit(recs, want, nowSample);
        const double now = nowSeconds();
        int w = 0;
        for (int i = 0; i < n; i++) {
            const tc::LitRecord& r = recs[i];
            const int64_t at = (int64_t)r.block * tc::kBlockSamples + r.offset;
            const double on = now + (double)(at - nowSample) / kTidalSampleRate;
            const double off = on + (r.dur ? r.dur / kTidalSampleRate : 0.25);  // plays out: a short flash
            for (int k = 0; k < r.n && w < cap; k++) {
                const uint16_t live = r.src[k];
                const uint16_t ed = live < tc::kMaxNodes ? gLiveToEd[live] : 0;
                if (ed) out[w++] = Lit{(int)r.slot, ed, on, off};
            }
        }
        return w;
#else
        (void)out, (void)cap;
        return 0;
#endif
    }
    void silence(int slot) override { tidalPlayerSilence(slot); }
    void hush() override { tidalPlayerHush(); }
    void mute(int slot, bool on) override { tidalPlayerMute(slot, on); }
    void unmuteAll() override { tidalPlayerUnmuteAll(); }
    void setCps(const tc::Program& p) override { tidalPlayerSetCps(p); }
    // Shift+File: the code into the project folder and the working copy
    // (instruments and settings save from the File screen, which writes the
    // code too: tidalProjectSaved)
    bool save(const tc::Program& p, tc::Label& msg) override;
};

// ---- project files (plan 4e) -----------------------------------------------------
constexpr const char* kWorkspaceTidal = "Workspace/project.tidal";
uint32_t gSavedChanges = 0;      // the editor's changes() when the working copy was written
uint32_t gChangedMs = 0;         // when changes() last moved
uint32_t gSeenChanges = 0;
bool gOpenPending = false, gNewPending = false;

// <project folder>/<project name>.tidal (UI notes, Editor14: named after the
// project), or with `legacy` the earlier <project folder>/project.tidal;
// false for a project never saved to a folder
bool projectTidalPath(char* buf, int cap, bool legacy = false) {
    if ((mtProject.values.projectNotSavedFlag & 2) || !newFileManager.getCurrentProjectName()[0]) return false;
    char dir[PATCH_SIZE];
    newFileManager.getCurrentProjectPathWithName(dir, sizeof dir);
    if (!newFileManager.projectExist(newFileManager.getCurrentProjectPath(), newFileManager.getCurrentProjectName()))
        return false;
    if (legacy) return snprintf(buf, cap, "%s/project.tidal", dir) < cap;
    return snprintf(buf, cap, "%s/%s.tidal", dir, newFileManager.getCurrentProjectName()) < cap;
}

// The code into the project folder under the project's name; an earlier
// project.tidal there goes (a Save As also copies the workspace's one in).
bool writeProjectTidal(const char* text, int n, char* m, int mCap) {
    char path[PATCH_SIZE + 48];
    if (!projectTidalPath(path, sizeof path)) return false;
    if (!tidalPlayerWriteText(path, text, n, m, mCap)) return false;
    char old[PATCH_SIZE + 48];
    if (gPlatform && projectTidalPath(old, sizeof old, true) && strcmp(old, path) != 0 &&
        gPlatform->getFileSystem()->exists(old))
        gPlatform->getFileSystem()->remove(old);
    return true;
}

// the program as .tidal text in saveText; -1 if it doesn't fit
int renderProgram(const tc::Program& p) {
    tc::Renderer r(saveText, sizeof saveText);
    saveText[0] = 0;
    const int n = r.file(p);
    return r.overflow() ? -1 : n;
}

DeviceOutput* gOut = nullptr;  // built with the rest (it has a vtable)
__NOINIT(EXTERNAL_RAM) unsigned char outRaw[sizeof(DeviceOutput)] __attribute__((aligned(8)));
tc::Editor* gEd = nullptr;
bool gLoaded = false, gDirty = true;
char gStatus[96] = "waiting for the project";

// micros() wraps every ~71 min: keep a 64-bit running count (main loop only)
double nowSeconds() {
    static uint32_t last = 0;
    static uint64_t us = 0;
    if (!gPlatform) return 0;
    const uint32_t m = gPlatform->getTimer()->micros();
    us += (uint32_t)(m - last);
    last = m;
    return (double)us * 1e-6;
}

// The project's instruments on the pads: tc::sampleToken of the sample file
// name, the name `s` uses (the player matches names the same way).
// Returns true if a name changed (a sample loaded / removed since).
bool setInstrumentNames() {
    bool changed = false;
    for (int i = 0; i < tc::kInstrumentSlots && i < kSampleInstrumentsCount; i++) {
        const strInstrument& ins = mtProject.instrument[i];
        char name[16] = "";
        if (ins.isActive == 1) tc::sampleToken(ins.sample.file_name, name, sizeof name);
        if (strcmp(gEd->instrument(i), name)) {
            gEd->setInstrument(i, name);
            changed = true;
        }
    }
    return changed;
}
uint32_t gNamesCheckMs = 0;

// Load the editor from a .tidal file; false if it isn't there (or unreadable).
bool loadFile(const char* path) {
    const char* text = nullptr;
    int len = 0;
    char msg[96];
    if (!tidalPlayerReadText(path, text, len, msg, sizeof msg)) return false;
    tc::Label err;
    // the status line (right of the cursor info) stays short; a load error
    // goes on the message line below it, which has the whole width
    const char* base = strrchr(path, '/');
    base = base ? base + 1 : path;
    if (gEd->loadText(text, len, err)) {
        snprintf(gStatus, sizeof gStatus, "%s loaded", base);
    } else {
        snprintf(gStatus, sizeof gStatus, "%s: not loaded", base);
        tc::Label m;
        m.printf("%s: %s", path, err.c_str());
        gEd->showMessage(m, true);
    }
    return true;
}

}  // namespace

bool DeviceOutput::save(const tc::Program& p, tc::Label& msg) {
    const int n = renderProgram(p);
    char m[128];
    if (n < 0) {
        snprintf(m, sizeof m, "file over %u bytes: not saved", (unsigned)sizeof saveText);
        msg = m;
        return false;
    }
    if (!tidalPlayerWriteText(kWorkspaceTidal, saveText, n, m, sizeof m)) {
        msg = m;
        return false;
    }
    gSavedChanges = gEd ? gEd->changes() : 0;
    char path[PATCH_SIZE + 48];
    if (!projectTidalPath(path, sizeof path)) {
        msg = "kept in the workspace: name the project once with File > Save As";
        return true;
    }
    const bool ok = writeProjectTidal(saveText, n, m, sizeof m);
    msg = m;
    return ok;
}

tc::Editor* tidalEditorModel() { return gEd; }
bool tidalEditorTakeDirty() {
    const bool d = gDirty;
    gDirty = false;
    return d;
}
void tidalEditorSetDirty() { gDirty = true; }
bool tidalEditorTakeLitDirty() {
    const bool d = gLitDirty;
    gLitDirty = false;
    return d;
}
const char* tidalEditorStatus() { return gStatus; }

void tidalProjectOpened() {
    // <project>/<name>.tidal (or the earlier project.tidal) -> the working
    // copy (none: an empty project)
    char path[PATCH_SIZE + 48];
    const char* text = nullptr;
    int len = 0;
    char m[96];
    if ((projectTidalPath(path, sizeof path) && tidalPlayerReadText(path, text, len, m, sizeof m)) ||
        (projectTidalPath(path, sizeof path, true) && tidalPlayerReadText(path, text, len, m, sizeof m)))
        tidalPlayerWriteText(kWorkspaceTidal, text, len, m, sizeof m);
    else if (gPlatform)
        gPlatform->getFileSystem()->remove(kWorkspaceTidal);
    gOpenPending = true;  // the editor reloads once the file manager is idle
}

void tidalProjectSaved() {
    // the code into the project folder (and the working copy)
    if (!gEd) return;
    const int n = renderProgram(*reinterpret_cast<tc::Program*>(progRaw));
    if (n < 0) return;
    char m[96];
    tidalPlayerWriteText(kWorkspaceTidal, saveText, n, m, sizeof m);
    writeProjectTidal(saveText, n, m, sizeof m);
    gSavedChanges = gEd->changes();
}

void tidalProjectNew() { gNewPending = true; }

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

int tidalEditorStatusSummary(char* buf, int cap) {
    int n = 0;
    auto add = [&](const char* fmt, auto... a) {
        if (n < cap - 1) n += snprintf(buf + n, cap - n, fmt, a...);
        if (n > cap - 1) n = cap - 1;
    };
    if (!tidalPlayerStarted()) {
        add("waiting for the project");
        return n;
    }
    const float cps = tidalPlayerCps();
    add("cps %d.%03d (%d BPM)   audio %d %%   voices %d/%d", (int)cps, (int)(cps * 1000.0f + 0.5f) % 1000,
        (int)(cps * 240.0f + 0.5f), tidalPlayerAudioLoad(), tidalPlayerSounding(), tidalPlayerCap());
    const int gap = tidalHeapGap();
    if (tidalStackDeepest()) add("   stack %d.%d KB spare", gap / 1024, gap % 1024 * 10 / 1024);
    if (gEd)  // the editor's program pools (a full one refuses edits)
        add("   nodes %d/%d words %d/%d", gEd->prog().liveNodes(), tc::kMaxNodes - 1, gEd->prog().stringCount(),
            tc::kMaxStrings);
    for (int s = 0; s < tc::kSlots; s++)  // the first problem, if any
        if (const char* err = tidalPlayerSlotError(s)) {
            add("   d%d: %s", s + 1, err);
            break;
        }
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
        memset(gLiveToEd, 0, sizeof gLiveToEd);
        gDirty = true;
        paintStack();
    }
    if (!gLoaded && tidalPlayerStarted()) {
        gLoaded = true;
        setInstrumentNames();
        // the working copy; before plan 4e the code lived in /project.tidal
        if (!loadFile(kWorkspaceTidal) && loadFile(kTidalPlayerFile)) gSavedChanges = gEd->changes() - 1;
        gDirty = true;
    }
    if (gLoaded && gOpenPending && newFileManager.getCurrentOperation() == fmNoOperation) {
        gOpenPending = false;  // a project was opened: its code (copied to the workspace) replaces ours
        tidalPlayerHush();
        if (!loadFile(kWorkspaceTidal)) {
            tc::Label err;
            gEd->loadText("", 0, err);
            snprintf(gStatus, sizeof gStatus, "%s: no code yet", newFileManager.getCurrentProjectName());
        }
        gSavedChanges = gEd->changes();
        gDirty = true;
    }
    if (gLoaded && gNewPending) {
        gNewPending = false;  // a new project: empty
        tidalPlayerHush();
        tc::Label err;
        gEd->loadText("", 0, err);
        gSavedChanges = gEd->changes();
        snprintf(gStatus, sizeof gStatus, "new project");
        gDirty = true;
    }
    // the working copy follows the edits (~2 s after the last one)
    {
        const uint32_t now = platform->getTimer()->millis();
        if (gEd->changes() != gSeenChanges) {
            gSeenChanges = gEd->changes();
            gChangedMs = now;
        }
        if (gLoaded && gEd->changes() != gSavedChanges && now - gChangedMs >= 2000 &&
            newFileManager.getCurrentOperation() == fmNoOperation) {
            const int n = renderProgram(*reinterpret_cast<tc::Program*>(progRaw));
            char m[96];
            if (n >= 0 && tidalPlayerWriteText(kWorkspaceTidal, saveText, n, m, sizeof m)) gSavedChanges = gEd->changes();
            else gSavedChanges = gEd->changes();  // don't retry every pass; Shift+File reports problems
        }
    }
    // samples loaded on the Tracker's sample screens since: their names
    // (Editor7: a new sample didn't show up in the editor)
    const uint32_t ms = platform->getTimer()->millis();
    if (gLoaded && ms - gNamesCheckMs >= 1000) {
        gNamesCheckMs = ms;
        if (setInstrumentNames()) gDirty = true;
    }
    // a sample folder loading in the background: a step, and its messages
    tidalFoldersUpdate(platform);
    {
        char m[96];
        bool err = false;
        if (tidalFoldersMessage(m, sizeof m, err)) {
            gEd->showMessage(m, err);
            gDirty = true;
        }
    }
    if (gEd->menuOpen() && gEd->checkMenu(3) >= 0) gDirty = true;  // lazily checked menus fill in
    if (gEd->pollHighlights(nowSeconds())) gLitDirty = true;  // the code view alone, at a lower rate
}
#endif
