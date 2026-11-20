// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// Menus: building the items, lazy checking and the per-context cache, opening /
// applying, and the value / function / operator / launch menus.
#include "editor_impl.h"

namespace tc {

MenuList& Editor::menuBuild() {
    menuOpen_ = false;
    menuVoices_ = false;
    menuFolders_ = false;
    menu_.clear();
    menuNext_ = 0;
    buildOnly_ = true;
    return menu_;
}

static uint32_t fnvStr(uint32_t h, const char* s) {
    for (; *s; s++) h = (h ^ (uint8_t)*s) * 16777619u;
    return (h ^ 0xFF) * 16777619u;
}
static uint32_t fnvNum(uint32_t h, uint32_t v) {
    for (int i = 0; i < 4; i++) h = (h ^ ((v >> (8 * i)) & 0xFF)) * 16777619u;
    return h;
}

int Editor::checkMenu(int n) {
    if (menuFolders_) {  // names, not edits: nothing to check
        for (MenuItem& it : menu_) it.checked = true;
        menuNext_ = menu_.size();
    }
    while (n-- > 0 && menuNext_ < menu_.size()) {
        MenuItem& it = menu_[menuNext_];
        uint16_t nn = 0;
        if (trySpec(it.spec, nn)) {
            it.cursor = nn;
            it.checked = true;
            menuNext_++;
        } else {
            menu_.erase_at(menuNext_);
            if (menuSel_ > menuNext_) menuSel_--;
        }
    }
    if (menuSel_ >= menu_.size()) menuSel_ = menu_.size() ? menu_.size() - 1 : 0;
    if (menuOpen_ && menu_.empty()) {
        menuOpen_ = false;
        error("nothing fits here");
    }
    if (menuOpen_ && !menuTouched_ && menu_.size() == 1 && menuNext_ == 1) {
        applyMenuItem(0, onOperator());  // one item left: as if it had been the only one
        // (outside act(): settle the cursor as an act() would, cursor mode
        // otherwise sent the next Left / Right to the block's start)
        dropEmptyBlocks();
        cursorSettle();
        return 0;
    }
    if (menuOpen_ && menuKey_ && menuNext_ >= menu_.size()) {  // remember the survivors
        int at = -1, oldest = 0;
        for (int i = 0; i < kMenuCache && at < 0; i++)
            if (menuCache_[i].key == menuKey_) at = i;
        if (at < 0) {
            for (int i = 1; i < kMenuCache; i++)
                if (menuCache_[i].used < menuCache_[oldest].used) oldest = i;
            at = oldest;
            MenuCache& c = menuCache_[at];
            c.key = menuKey_;
            c.count = menu_.size();
            for (int i = 0; i < c.count; i++) c.label[i] = fnvStr(2166136261u, menu_[i].label.c_str());
        }
        menuCache_[at].used = ++menuUse_;
        menuKey_ = 0;  // stored
    }
    if (menuOpen_ && menuChecking_ && menuNext_ >= menu_.size()) {  // lazily checked: done
        menuChecking_ = false;
        info(menuTitle_ + ": wheel to choose, ok to apply, cancel to close");
    } else if (menuOpen_ && menuChecking_) {
        Label m;
        info(m.printf("%s: checking %d of %d ...", menuTitle_.c_str(), menuNext_, menu_.size()));
    }
    return menu_.size() - menuNext_;
}

// A menu with one item that fits applies it at once (user, after
// Editor14): no menu to confirm. Lazily checked menus (the device) do the
// same when checking leaves one item before the user touched the menu.
// What decides a menu's checked contents (UI v3 A3, by type context since
// RC24): the menu and its items, and the types where they would go: the
// target's kind / type, its parent (kind, function / operator, which child,
// every child's type: polymorphic operands tie them), its grandparent's
// kind / type, the slot kind and Tracker mode. Not the block or the code
// version: the same situation anywhere reuses the result.
uint32_t Editor::menuKeyNow() const {
    const Program& p = *prog_;
    uint32_t h = fnvStr(2166136261u, menuTitle_.c_str());
    h = fnvNum(h, (uint32_t)onOp_ | (uint32_t)cps_ << 1 | (uint32_t)(slot_ >= kSampleSlots) << 2 |
                      (uint32_t)trackerMode_ << 3 | (uint32_t)(gapSeq_ != 0) << 4);
    const uint16_t t = gapSeq_ ? gapSeq_ : cursor_;
    if (t) {
        const Node& x = p.at(t);
        h = fnvNum(h, (uint32_t)x.kind | (uint32_t)x.type << 8);
        if (x.kind == Kind::Fn || x.kind == Kind::Op) h = fnvNum(h, x.a);
        const uint16_t par = p.parentOf(t);
        if (par) {
            const Node& pn = p.at(par);
            h = fnvNum(h, (uint32_t)pn.kind | (uint32_t)pn.type << 8 | 0x10000u);
            if (pn.kind == Kind::Op) h = fnvNum(h, pn.a);
            if (pn.kind == Kind::App && pn.first && p.at(pn.first).kind == Kind::Fn) h = fnvNum(h, p.at(pn.first).a);
            int idx = 0;
            for (uint16_t c = pn.first; c; c = p.at(c).next, idx++) {
                if (c == t) h = fnvNum(h, 0x20000u | (uint32_t)idx);
                h = fnvNum(h, p.at(c).type);
            }
            const uint16_t gp = p.parentOf(par);
            if (gp) h = fnvNum(h, (uint32_t)p.at(gp).kind | (uint32_t)p.at(gp).type << 8 | 0x40000u);
        }
    }
    h = fnvNum(h, (uint32_t)menu_.size());
    for (const MenuItem& it : menu_) h = fnvStr(h, it.label.c_str());
    return h ? h : 1;
}

bool Editor::openMenu(const Label& title) {
    buildOnly_ = false;
    menuNext_ = 0;
    menuTitle_ = title;
    menuKey_ = lazyMenus_ && !menuFolders_ ? menuKeyNow() : 0;
    const MenuCache* hit = nullptr;
    for (int i = 0; i < kMenuCache && menuKey_ && !hit; i++)
        if (menuCache_[i].key == menuKey_) hit = &menuCache_[i];
    if (hit) {  // checked before in the same type context: its survivors
        menuCache_[hit - menuCache_].used = ++menuUse_;
        menuKey_ = 0;
        int w = 0;
        for (int i = 0; i < menu_.size(); i++) {
            const uint32_t lh = fnvStr(2166136261u, menu_[i].label.c_str());
            bool keep = false;
            for (int k = 0; k < hit->count && !keep; k++) keep = hit->label[k] == lh;
            if (!keep) continue;
            if (w != i) menu_[w] = menu_[i];
            menu_[w].checked = true;  // its cursor comes from applying it (cursor 0)
            menu_[w].cursor = 0;
            w++;
        }
        while (menu_.size() > w) menu_.pop_back();
        menuNext_ = menu_.size();
    }
    if (!lazyMenus_ || menu_.size() == 1) checkMenu(menu_.size());
    if (menu_.empty()) {
        error("nothing fits here");
        return false;
    }
    menuTitle_ = title;
    menuSel_ = 0;
    menuOpen_ = true;
    menuTouched_ = false;
    if (menu_.size() == 1 && menuNext_ == 1) {
        applyMenuItem(0, onOperator());
        return true;
    }
    // lazily checked (the device): say so until every item is checked (RC4:
    // with patterns playing it takes a moment)
    menuChecking_ = menuNext_ < menu_.size();
    if (menuChecking_) {
        Label m;
        info(m.printf("%s: checking %d items ...", title.c_str(), menu_.size()));
    } else {
        info(title + ": wheel to choose, ok to apply, cancel to close");
    }
    return true;
}

void Editor::applyMenuItem(int i, bool wasOnOp) {
    MenuItem& it = menu_[i];
    if (menuFolders_) {
        menuOpen_ = menuFolders_ = false;
        Label msg;
        if (!out_) return error("no output");
        if (out_->loadFolder(it.label.c_str(), msg)) info(msg);
        else error(msg);
        return;
    }
    if (it.label.startsWith("# ")) lastControl_ = it.label.substr(2);
    menuOpen_ = false;
    uint16_t nn = 0;
    if (!doSpec(it.spec, nn)) return error("can't apply " + it.label);
    cursor_ = it.checked && it.cursor ? it.cursor : nn;
    // a wrap (# gain 1, fast 2 $ ...): onto the first value of what was
    // added, so the jog edits it at once (UI v3 B2)
    if (const uint16_t v = addedValue(it.spec, nn)) cursor_ = v;
    gapSeq_ = 0;
    onOp_ = wasOnOp && it.spec.kind == EditKind::SetOp;
    state_[slot_] = SlotState::Edited;
    if (menuVoices_ && !it.label.startsWith("single")) ensureVoices();
    info(menuTitle_ + ": " + it.label);
}

// Sample Loader: the Tidal folders on the card; OK loads one (the output
// builds a sliced instrument in the background).
void Editor::openFolders() {
    MenuList& items = menuBuild();
    if (!out_) return error("no output");
    const int n = out_->folders();
    if (n <= 0) return error("no sample folders (put them in /Tidal/<name>/ on the card)");
    for (int i = 0; i < n && items.push_back(MenuItem{}); i++) items.back().label = out_->folderName(i);
    menuFolders_ = true;
    openMenu("load a Tidal folder");
}

void Editor::addReplaceItems(MenuList& items, const Label& label, const Text& code,
                             bool mini) {
    uint16_t nn;
    EditSpec sp;
    const bool ok = listGap() ? tryInsertListAt(mini ? "\"" + code + "\"" : code, nn, &sp)
                              : tryReplace(code, mini, nn, &sp);
    if (ok) items.push_back(MenuItem{label, sp, nn});
}

// Note button: pitches. Inside "..." single steps, otherwise note patterns.
void Editor::openNotes() {
    ensureBlock();
    if (!cursor_) return;
    MenuList& items = menuBuild();
    auto add = [&](const Text& l, const Text& code, bool mini) { addReplaceItems(items, l, code, mini); };
    if (isMiniKind(prog_->at(cursor_).kind)) {
        for (const char* v : {"c", "d", "e", "f", "g", "a", "b", "cs", "ef", "fs", "af", "bf", "c4", "c6",
                              "c'maj", "c'min", "c'maj7", "c'min7", "c'dom7", "c'sus4", "e'min", "g'maj"})
            add(v, v, true);
        for (int i = 0; i <= 12; i++) add(tnum(i), tnum(i), true);
        add("rest ~", "~", true);
    } else {
        for (const char* v : {"note \"c e g\"", "n \"0 .. 7\"", "n \"c'maj e'min\"",
                              "n (scale \"major\" \"0 .. 7\")", "n (arp \"up\" \"<c'maj e'min>\")",
                              "\"c e g\"", "\"0 .. 7\"", "\"<c'maj e'min>\"", "(scale \"major\" \"0 .. 7\")",
                              "(arp \"up\" \"<c'maj e'min>\")"})
            add(v, v, false);
    }
    openMenu("notes");
}

// Instrument button: samples. Inside "..." names (and name:n), otherwise s "name".
void Editor::openSamples() {
    ensureBlock();
    if (!cursor_) return;
    MenuList& items = menuBuild();
    bool mini = isMiniKind(prog_->at(cursor_).kind);
    for (int si = 0; si < kInstrumentSlots; si++) {
        const Text s = samples_[si];
        if (s.empty()) continue;
        if (mini) {
            addReplaceItems(items, s, s, true);
            for (int i = 1; i <= 3; i++) addReplaceItems(items, s + ":" + tnum(i), s + ":" + tnum(i), true);
        } else {
            addReplaceItems(items, "s \"" + s + "\"", "s \"" + s + "\"", false);
        }
    }
    openMenu("samples");
}

void Editor::openOps() {
    MenuList& items = menuBuild();
    const int cur = (int)prog_->at(cursor_).a;
    for (int id = 0; id < libCount(); id++) {
        if (!libIsOp(id) || id == cur || !items.push_back(MenuItem{})) continue;
        MenuItem& it = items.back();
        it.spec.kind = EditKind::SetOp;
        it.spec.target = cursor_;
        it.spec.launch = id;
        it.label = lib(id).name;
    }
    (void)openMenu("operators");
}

void Editor::openReplace() {
    if (cursor_ && prog_->at(cursor_).kind == Kind::Op && onOp_) return openOps();
    MenuList& items = menuBuild();
    auto add = [&](const Text& label, const Text& code, bool mini) {
        addReplaceItems(items, label, code, mini);
    };
    if (cps_) {  // setcps: numbers only (silence would type-check but set no tempo)
        if (cursor_ == prog_->cpsExpr && prog_->at(cursor_).kind != Kind::Num) {  // the line: whole tempos
            for (const char* v : {"90", "100", "110", "120", "128", "130", "138", "140", "150", "160", "170", "174"})
                add(Text(v) + " BPM", Text(v) + "/60/4", false);
            add("0.5 cycles / s", "0.5", false);
            return (void)openMenu("tempo");
        }
        for (const char* v : {"90", "100", "110", "120", "128", "130", "138", "140", "150", "160", "170", "174",
                              "60", "4", "3", "0.5", "1"})
            add(v, v, false);
        return (void)openMenu("tempo");
    }
    ensureBlock();
    if (!cursor_) return;
    const Node& n = prog_->at(cursor_);
    if (n.kind == Kind::Block || cursor_ == exprRoot()) {
        bool midi = slot_ >= kSampleSlots;
        if (midi) {
            add("note pattern", "n \"c e g\"", false);
            add("arpeggio", "n (arp \"up\" \"<c'maj e'min>\")", false);
            add("scale run", "n (scale \"major\" \"0 .. 7\")", false);
        } else {
            add("drum pattern", "s \"bd*2 [~ sd] bd sd\"", false);
            add("hi-hats", "s \"hh*8\" # gain 0.8", false);
            add("euclid", "s \"bd(3,8) sd(<3 5>,8,2)\"", false);
            add("melody", "n \"0 .. 7\" # s \"arpy\"", false);
            add("notes", "note \"c e g\" # s \"superpiano\"", false);
        }
        add("silence", "silence", false);
    }
    if (isMiniKind(n.kind)) {
        Text t = renderNode(*prog_, cursor_);
        add("rest ~", "~", true);
        for (int si = 0; si < kInstrumentSlots; si++)
            if (samples_[si][0]) add(samples_[si], samples_[si], true);
        for (const char* v : {"0", "1", "2", "3", "0.5", "c", "e", "g", "t", "f"}) add(v, v, true);
        add("group [x x]", "[" + t + " " + t + "]", true);
        add("alternate <x x>", "<" + t + " " + t + ">", true);
        add("stack [x, x]", "[" + t + ", " + t + "]", true);
    } else if (n.kind != Kind::Block) {
        for (const char* v : {"silence", "1", "0.5", "2", "(1/8)", "\"1 2\"", "\"<0 2>\"", "\"bd sd\"",
                              "\"c e g\"", "\"t f t t\"", "sine", "saw", "rand", "rev", "(fast 2)",
                              "(# speed 2)", "(0.25 ~>)"})
            add(v, v, false);
        for (int id = 0; id < libCount(); id++) {
            const LibEntry& e = lib(id);
            if (libIsOp(id)) continue;
            Text code = e.name;
            if (e.cat == Cat::Control) code += Text(" ") + controlDefault(e.name);
            else if (e.defaults) code += Text(" ") + e.defaults;
            add(code, code, false);
        }
    }
    openMenu(listGap() ? "insert" : "replace");
}

// Function / control menus in alphabetical order (user, after RC18), case
// ignored and a leading "# " / symbol skipped; ties keep the library order.
// Sorted through an index and moved once each (items are ~700 B).
static const char* sortKey(const char* l) {
    while (*l && !((*l >= 'a' && *l <= 'z') || (*l >= 'A' && *l <= 'Z') || (*l >= '0' && *l <= '9'))) l++;
    return l;
}
static int keyCmp(const char* a, const char* b) {
    a = sortKey(a);
    b = sortKey(b);
    for (;; a++, b++) {
        int ca = (unsigned char)*a, cb = (unsigned char)*b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb || !ca) return ca - cb;
    }
}
void Editor::sortMenu(MenuList& items) {
    const int n = (int)items.size();
    uint8_t idx[kMenuMax];
    for (int i = 0; i < n; i++) idx[i] = (uint8_t)i;
    for (int i = 1; i < n; i++)
        for (int k = i; k > 0 && keyCmp(items[idx[k - 1]].label.c_str(), items[idx[k]].label.c_str()) > 0; k--) {
            const uint8_t t = idx[k];
            idx[k] = idx[k - 1];
            idx[k - 1] = t;
        }
    bool done[kMenuMax] = {};
    for (int start = 0; start < n; start++) {  // apply the permutation: item k := old item idx[k]
        if (done[start] || idx[start] == start) continue;
        menuTmp_ = items[start];
        int k = start;
        for (;;) {
            done[k] = true;
            const int j = idx[k];
            if (j == start) {
                items[k] = menuTmp_;
                break;
            }
            items[k] = items[j];
            k = j;
        }
    }
}

// "d3", "jumpMod 3 4" or "jumpIn 3 2" for the slot's launch mode.

// Pattern button: when this block's next evaluation takes effect.
void Editor::openLaunch() {
    uint16_t b = prog_->block[slot_];
    if (!b || !prog_->at(b).first) return error("empty slot: write a pattern first");
    static const struct {
        Launch l;
        uint32_t k;
        const char* fmt;  // %d = the slot
    } opts[] = {
        {Launch::Now, 0, "d%d $  (now)"},
        {Launch::JumpMod, 2, "jumpMod %d 2  (every 2 cycles)"},
        {Launch::JumpMod, 4, "jumpMod %d 4  (every 4 cycles)"},
        {Launch::JumpMod, 8, "jumpMod %d 8  (every 8 cycles)"},
        {Launch::JumpMod, 16, "jumpMod %d 16  (every 16 cycles)"},
        {Launch::JumpIn, 1, "jumpIn %d 1  (after 1 cycle)"},
        {Launch::JumpIn, 2, "jumpIn %d 2  (after 2 cycles)"},
        {Launch::JumpIn, 4, "jumpIn %d 4  (after 4 cycles)"},
    };
    MenuList& items = menuBuild();
    int sel = 0;
    const Node& cur = prog_->at(b);
    for (auto& o : opts) {
        if (!items.push_back(MenuItem{})) break;
        MenuItem& it = items.back();
        it.spec.kind = EditKind::Launch;
        it.spec.target = b;
        it.spec.launch = (int)o.l;
        it.spec.cycles = o.k;
        it.cursor = cursor_;
        it.label.printf(o.fmt, slot_ + 1);
        if (o.l == blockLaunch(cur) && (o.l == Launch::Now || o.k == cur.b)) sel = items.size() - 1;
    }
    openMenu("launch");
    menuSel_ = sel;
}

}  // namespace tc
