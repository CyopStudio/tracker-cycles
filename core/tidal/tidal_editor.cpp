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
