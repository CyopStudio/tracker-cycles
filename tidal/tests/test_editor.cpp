// The editor model (tidal/editor) driven directly, without the prototype:
// lazy menus give the same items as eager ones, a small undo arena drops
// the oldest steps and still undoes the rest exactly, the output sees
// evaluate / mute / tempo calls. Behaviour is covered by the prototype's
// smoke sessions (tests/proto); these check what the device relies on.
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "controls.h"
#include "editor.h"

using namespace tc;

#define EXPECT(cond, ...)                          \
    do {                                           \
        if (cond) {                                \
            pass++;                                \
        } else {                                   \
            fail++;                                \
            printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                   \
            printf("\n");                          \
        }                                          \
    } while (0)

namespace {

struct RecOutput : EditorOutput {
    std::vector<std::string> calls;
    bool evaluate(const Program&, int slot, Label&) override {
        calls.push_back("eval " + std::to_string(slot + 1));
        return true;
    }
    void silence(int slot) override { calls.push_back("silence " + std::to_string(slot + 1)); }
    void hush() override { calls.push_back("hush"); }
    void mute(int slot, bool on) override { calls.push_back((on ? "mute " : "unmute ") + std::to_string(slot + 1)); }
    void unmuteAll() override { calls.push_back("unmuteAll"); }
    void setCps(const Program&) override { calls.push_back("cps"); }
    bool save(const Program&, Label& msg) override {
        msg = "saved";
        return true;
    }
};

const char* kFile =
    "setcps (120/60/4)\n\n"
    "-- kick\n"
    "d1 $ every 4 (fast 2) $ s \"bd*2 [~ sd] bd sd\" # gain 1\n\n"
    "d2 $ s \"hh*8\" # gain 0.8 # pan sine\n";

Program gP1, gP2;
Checkpoint gC1, gC2;
uint8_t gUndo1[256 * 1024], gUndo2[6 * 1024];

std::string fileOf(const Editor& ed) {
    static char buf[65536];
    ed.fileText(buf, sizeof buf);
    return buf;
}

std::vector<std::string> labels(const Editor& ed) {
    std::vector<std::string> v;
    for (const MenuItem& it : ed.menu()) v.push_back(it.label.c_str());
    return v;
}

}  // namespace

void testEditorModel(int& pass, int& fail) {
    static Editor eager(gP1, gC1, gUndo1, (int)sizeof gUndo1);
    static Editor lazy(gP2, gC2, gUndo2, (int)sizeof gUndo2);
    RecOutput out1, out2;
    eager.setOutput(&out1);
    lazy.setOutput(&out2);
    Label err;
    EXPECT(eager.loadText(kFile, (int)strlen(kFile), err), "load: %s", err.c_str());
    EXPECT(lazy.loadText(kFile, (int)strlen(kFile), err), "load: %s", err.c_str());
    const std::string original = fileOf(eager);

    // lazy menus: same items, checked a few at a time
    lazy.setLazyMenus(true);
    for (Editor* e : {&eager, &lazy}) {
        e->gotoSlot(0);
        e->act(Action::Home);
        e->act(Action::Right);       // the block's expression
        e->act(Action::Replace);     // Shift+Screen 1
    }
    EXPECT(eager.menuOpen() && lazy.menuOpen(), "menus open");
    const int before = lazy.menu().size();
    int left = lazy.checkMenu(5);
    EXPECT(left > 0 && before > eager.menu().size(), "lazy: %d unchecked of %d (eager %d)", left, before,
           eager.menu().size());
    int passes = 1;
    while (lazy.checkMenu(5) > 0) passes++;
    EXPECT(labels(lazy) == labels(eager), "lazy menu has the eager items (%d vs %d)", lazy.menu().size(),
           eager.menu().size());
    EXPECT(passes > 5, "lazy menu took %d passes", passes);
    for (Editor* e : {&eager, &lazy}) {
        e->selectMenu(1);
        e->act(Action::Confirm);
    }
    EXPECT(fileOf(lazy) == fileOf(eager), "the same item applied lazily");

    // OK on an item before it was checked: applied (and checked) then
    for (Editor* e : {&eager, &lazy}) e->act(Action::MenuModifiers);  // FX1
    lazy.selectMenu(3);  // unchecked: lazy has checked nothing yet
    eager.selectMenu(3);
    const std::string want = eager.menu()[3].label.c_str();
    const std::string got = lazy.menu()[3].label.c_str();
    EXPECT(want == got, "item 3: %s vs %s", want.c_str(), got.c_str());
    for (Editor* e : {&eager, &lazy}) e->act(Action::Confirm);
    EXPECT(fileOf(lazy) == fileOf(eager), "an unchecked item applied");

    // many edits on a small undo arena: the oldest steps go, the rest undo
    // exactly (lazy has 6 KB, eager 256 KB)
    for (int i = 0; i < 40; i++) {
        for (Editor* e : {&eager, &lazy}) {
            e->gotoSlot(1);
            e->act(Action::JumpSamples);  // the s pattern's first step
            e->act(i % 2 ? Action::WheelUp : Action::WheelUpFast);
        }
    }
    EXPECT(fileOf(lazy) == fileOf(eager), "40 jog edits the same");
    int undone = 0;
    while (true) {
        lazy.act(Action::Undo);
        if (lazy.messageIsError()) break;
        undone++;
        eager.act(Action::Undo);
    }
    EXPECT(undone > 3 && undone < 42, "small arena kept %d undo steps", undone);
    EXPECT(fileOf(lazy) == fileOf(eager), "undo with a small arena matches the big one");
    while (true) {  // the big arena undoes everything
        eager.act(Action::Undo);
        if (eager.messageIsError()) break;
    }
    EXPECT(fileOf(eager) == original, "every edit undone");

    // output calls
    out1.calls.clear();
    eager.gotoSlot(0);
    eager.act(Action::Evaluate);
    eager.playDown(false);
    eager.mutePad(1);
    eager.playUp();
    eager.act(Action::Hush);
    std::string calls;
    for (auto& c : out1.calls) calls += c + ";";
    EXPECT(calls == "cps;eval 1;mute 2;hush;", "output calls: %s", calls.c_str());

    // highlights: a slot's new onset ends its earlier steps; steps with the
    // same onset (a chord) stay lit together
    {
        struct LitOut : RecOutput {
            std::vector<Lit> q;
            int litSteps(Lit* out, int cap) override {
                int n = 0;
                while (n < cap && !q.empty()) out[n++] = q.front(), q.erase(q.begin());
                return n;
            }
        } lo;
        eager.setOutput(&lo);
        eager.pollHighlights(0.0);
        lo.q = {{0, 101, 1.0, 9.0}, {1, 201, 1.0, 9.0}};
        eager.pollHighlights(1.1);
        lo.q = {{0, 102, 2.0, 9.0}, {0, 103, 2.0, 9.0}};  // d1's next onset: a two-note chord
        eager.pollHighlights(2.1);
        std::string lit;
        for (const auto& l : eager.litSteps()) lit += std::to_string(l.slot) + ":" + std::to_string(l.node) + " ";
        EXPECT(lit == "1:201 0:102 0:103 ", "lit after d1's next onset: %s", lit.c_str());
        EXPECT(!eager.pollHighlights(2.2), "nothing turned on or off: no redraw");
        lo.q = {{1, 202, 2.15, 9.0}};  // arrives after the poll that passed its onset
        EXPECT(eager.pollHighlights(2.3), "a late record still redraws");
        EXPECT(eager.pollHighlights(9.5) && eager.litSteps().empty(), "all off at 9.5");
        eager.setOutput(&out1);
    }

    // a menu with one item applies it at once, lazily checked or not:
    // | at the gap inside [~ sd] -> [~ | sd]
    {
        static Program p3, p4;
        static Checkpoint c3, c4;
        static uint8_t u3[16 * 1024], u4[16 * 1024];
        static Editor e3(p3, c3, u3, (int)sizeof u3), e4(p4, c4, u4, (int)sizeof u4);
        e4.setLazyMenus(true);
        const char* f = "d1 $ s \"bd [~ sd]\"\n";
        for (Editor* e : {&e3, &e4}) {
            const char* who = e == &e4 ? "lazy" : "eager";
            Label er;
            EXPECT(e->loadText(f, (int)strlen(f), er), "load: %s", er.c_str());
            e->gotoSlot(0);
            e->act(Action::JumpSamples);  // bd
            for (Action a : {Action::Right, Action::Right, Action::Down, Action::Down, Action::Right}) e->act(a);
            e->act(Action::Struct7);
            EXPECT(!e->menuOpen(), "one-item menu applied (%s): %s", who, e->message().c_str());
            EXPECT(fileOf(*e) == "d1 $ s \"bd [~ | sd]\"\n", "%s: %s", who, fileOf(*e).c_str());
        }
    }

    // a block started on an empty slot and then cancelled / undone leaves no
    // block without code (Editor12: "d12" alone, evaluate -> device hang)
    {
        const std::string before = fileOf(eager);
        eager.gotoSlot(11);
        eager.act(Action::Replace);  // ensureBlock + the replace menu
        EXPECT(eager.menuOpen(), "replace menu on an empty slot");
        eager.act(Action::Cancel);
        EXPECT(fileOf(eager) == before && !eager.prog().block[11], "cancel: no empty block left");
        eager.act(Action::JumpNotes);  // starts d12 $ n "0"
        EXPECT(fileOf(eager).find("d12 $ n") != std::string::npos, "Note started d12");
        eager.act(Action::Undo);
        EXPECT(fileOf(eager) == before && !eager.prog().block[11], "undo: no empty block left");
        eager.act(Action::Evaluate);
        EXPECT(eager.messageIsError(), "evaluate on the empty slot: %s", eager.message().c_str());
        eager.act(Action::JumpNotes);  // and it starts again
        EXPECT(fileOf(eager).find("d12 $ n") != std::string::npos, "Note starts d12 again");
        eager.act(Action::Undo);
    }

    // Sample Loader: the output's folders as a menu; OK hands the name to
    // loadFolder (no edit, no type check); one folder loads at once
    {
        struct FolderOut : RecOutput {
            std::vector<std::string> names;
            std::string loaded;
            int folders() override { return (int)names.size(); }
            const char* folderName(int i) override { return names[i].c_str(); }
            bool loadFolder(const char* name, Label& msg) override {
                loaded = name;
                msg = "loading";
                return true;
            }
        } fo;
        fo.names = {"bd", "hi-hat", "jvbass"};
        const std::string before = fileOf(eager);
        eager.setOutput(&fo);
        EXPECT(!press(eager, Control::SmpLoad, false), "Sample Loader alone: the stock loader (not the editor)");
        EXPECT(press(eager, Control::SmpLoad, true), "Shift+Sample Loader pressed");
        EXPECT(eager.menuOpen() && eager.menu().size() == 3, "folder menu: %d items", eager.menu().size());
        eager.act(Action::WheelUp);
        eager.act(Action::Confirm);
        EXPECT(fo.loaded == "hi-hat" && !eager.menuOpen() && fileOf(eager) == before, "loaded '%s'",
               fo.loaded.c_str());
        lazy.setOutput(&fo);
        fo.names = {"bd"};
        fo.loaded.clear();
        lazy.act(Action::MenuFolders);  // one folder: no menu (lazy menus too)
        EXPECT(fo.loaded == "bd" && !lazy.menuOpen(), "single folder loaded at once: '%s'", fo.loaded.c_str());
        fo.names.clear();
        eager.act(Action::MenuFolders);
        EXPECT(eager.messageIsError() && !eager.menuOpen(), "no folders: %s", eager.message().c_str());
        eager.setOutput(&out1);
        lazy.setOutput(&out2);
    }

    // jog(n): a fast spin folded into one edit (device: one per main-loop pass)
    {
        static Program pj;
        static Checkpoint cj;
        static uint8_t uj[16 * 1024];
        static Editor ej(pj, cj, uj, (int)sizeof uj);
        const char* f = "d1 $ s \"bd\" # gain 1\n";
        Label er;
        ej.loadText(f, (int)strlen(f), er);
        ej.gotoSlot(0);
        ej.act(Action::Home);
        ej.act(Action::Right);  // the # expression
        ej.act(Action::Down);   // s "bd"
        ej.act(Action::Right);  // gain 1
        ej.act(Action::Down);   // gain
        ej.act(Action::Right);  // 1
        ej.jog(4, false);  // gain: 0.05 a detent (UI v3 B1)
        EXPECT(fileOf(ej).find("gain 1.2") != std::string::npos, "jog(4): %s", fileOf(ej).c_str());
        ej.act(Action::Undo);
        EXPECT(fileOf(ej).find("gain 1") != std::string::npos, "one undo step: %s", fileOf(ej).c_str());
    }

    // jog steps by meaning (UI v3 B1): the number's owner decides
    {
        static Program pb;
        static Checkpoint cb;
        static uint8_t ub[32 * 1024];
        static Editor eb(pb, cb, ub, (int)sizeof ub);
        struct Case {
            const char* code;
            int detents;
            bool fast;
            const char* want;
        } cases[] = {
            {"gain 1", -4, false, "gain 0.8"},
            {"pan 0.5", 20, false, "pan 1"},  // clamped to 0 .. 1
            {"cutoff 1000", 1, false, "cutoff 1059"},
            {"cutoff 1000", 1, true, "cutoff 2000"},
            {"speed 1", 3, false, "speed 1.3"},
            {"crush 4", -10, false, "crush 1"},
        };
        for (const Case& c : cases) {
            char f[128];
            snprintf(f, sizeof f, "d1 $ s \"bd\" # %s\n", c.code);
            Label er;
            eb.loadText(f, (int)strlen(f), er);
            eb.gotoSlot(0);
            eb.act(Action::Home);
            eb.act(Action::Right);
            eb.act(Action::Down);
            eb.act(Action::Right);
            eb.act(Action::Down);
            eb.act(Action::Right);  // the control's value
            eb.jog(c.detents, c.fast);
            EXPECT(fileOf(eb).find(c.want) != std::string::npos, "%s %+d%s -> %s", c.code, c.detents,
                   c.fast ? " (fast)" : "", fileOf(eb).c_str());
        }
        const char* f2 = "d1 $ fast 2 $ s \"bd\"\n";
        Label er;
        eb.loadText(f2, (int)strlen(f2), er);
        eb.gotoSlot(0);
        eb.act(Action::Home);
        eb.act(Action::Right);
        eb.act(Action::Down);
        eb.act(Action::Right);  // the 2
        eb.jog(1, false);
        EXPECT(fileOf(eb).find("fast 3") != std::string::npos, "fast ladder up: %s", fileOf(eb).c_str());
        eb.jog(-2, false);
        EXPECT(fileOf(eb).find("fast 1.5") != std::string::npos, "fast ladder down: %s", fileOf(eb).c_str());

        // UI v3 B2: after `# gain` from the FX2 menu the cursor is on its value
        eb.act(Action::Home);
        eb.act(Action::Right);  // the whole expression
        eb.act(Action::MenuFx);
        int gi = -1;
        for (int i = 0; i < eb.menu().size(); i++)
            if (!strcmp(eb.menu()[i].label.c_str(), "# gain 1")) gi = i;
        EXPECT(gi >= 0, "FX menu has # gain 1");
        if (gi >= 0) {
            eb.selectMenu(gi);
            eb.act(Action::Confirm);
            eb.jog(-2, false);
            EXPECT(fileOf(eb).find("# gain 0.9") != std::string::npos, "cursor on the new value: %s",
                   fileOf(eb).c_str());
        }
    }

    // UI v3 A3: a fully checked lazy menu, reopened at the same place with no
    // edit in between, needs no checking; after an edit it's checked again
    {
        static Program pm;
        static Checkpoint cm;
        static uint8_t um[32 * 1024];
        static Editor em(pm, cm, um, (int)sizeof um);
        em.setLazyMenus(true);
        Label er;
        em.loadText(kFile, (int)strlen(kFile), er);
        em.gotoSlot(0);
        em.act(Action::Home);
        em.act(Action::Right);
        em.act(Action::Replace);
        while (em.checkMenu(5) > 0) {}
        const std::vector<std::string> first = labels(em);
        em.act(Action::Cancel);
        em.act(Action::Replace);
        EXPECT(em.menuOpen() && em.checkMenu(0) == 0, "reopened: nothing left to check");
        EXPECT(labels(em) == first, "reopened: the same %zu items", first.size());
        em.act(Action::Cancel);
        em.act(Action::Down);
        em.act(Action::WheelUp);  // an edit
        em.act(Action::Up);
        em.act(Action::Replace);
        EXPECT(em.menuOpen() && em.checkMenu(0) > 0, "after an edit: checked again");
        em.act(Action::Cancel);
    }

    // the control mapping (editor/controls.*), shared with the device
    Control c;
    EXPECT(ctlFromId("pad13", c) && c == padCtl(12), "pad13");
    EXPECT(ctlFromId("s1", c) && c == Control::Screen0, "s1");
    EXPECT(!ctlFromId("pad49", c) && !ctlFromId("nope", c), "bad ids refused");
    EXPECT(!strcmp(ctlName(padCtl(47)), "Pad 48") && !strcmp(ctlId(Control::Fx1), "fx1"), "names");
    Command k = controlCommand(eager, Control::Pattern, false);
    EXPECT(k.type == Command::Act && k.action == Action::Evaluate && k.label == "Evaluate", "Pattern: %s",
           k.label.c_str());
    k = controlCommand(eager, padCtl(1), true);  // Shift + pad 2 = d2
    EXPECT(k.type == Command::Slot && k.slot == 1 && k.label == "d2", "Shift+pad2: %s", k.label.c_str());
    EXPECT(press(eager, padCtl(2), true) && eager.slot() == 2, "pressing Shift+pad3 selects d3");
}
