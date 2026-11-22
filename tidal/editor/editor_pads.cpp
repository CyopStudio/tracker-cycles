// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// Pads: entering numbers / notes / instruments, chords, steps, and the
// Note / Instrument jumps to the block's n / s pattern.
#include "editor_impl.h"
#include "cold.h"

TC_COLD_BEGIN
namespace tc {

bool Editor::padNumeric() const {
    if (menuOpen_ || !cursor_) return false;
    const Node& n = prog_->at(cursor_);
    // a number argument, the quotes of a numeric pattern, or anything inside
    // them (a step, or a group like <1 2> / 0 .. 7, which gets replaced)
    if (!(n.kind == Kind::Num || n.kind == Kind::Mini || isMiniKind(n.kind))) return false;
    return n.type == kTyPatBase + (int)Base::Time || n.type == kTyPatBase + (int)Base::Double ||
           n.type == kTyPatBase + (int)Base::Int || n.type == kTyPatBase + (int)Base::Note;
}

// Bool patterns (UI notes 2026-11-22): `acc "0 1"`, `time "1 0 ~"`,
// `struct "t f"`: the cursor on the quotes or anything inside them.
bool Editor::padBool() const {
    if (menuOpen_ || !cursor_) return false;
    const Node& n = prog_->at(cursor_);
    return (n.kind == Kind::Mini || isMiniKind(n.kind)) && n.type == kTyPatBase + (int)Base::Bool;
}

// A Bool step in the pattern's own style: 1 / 0, or t / f when the pattern
// already writes them so.
static const char* boolText(const Program& p, uint16_t at, bool on) {
    uint16_t m = at;
    while (m && p.at(m).kind != Kind::Mini) m = p.parentOf(m);
    struct Find {
        static int style(const Program& p, uint16_t n) {  // 1 digits, 0 letters, -1 none
            if (!n) return -1;
            if (p.at(n).kind == Kind::MBool) return (p.at(n).flags & kDigitStyle) ? 1 : 0;
            for (uint16_t c = p.at(n).first; c; c = p.at(c).next)
                if (int s = style(p, c); s >= 0) return s;
            return -1;
        }
    };
    const bool letters = m && Find::style(p, m) == 0;
    return letters ? (on ? "t" : "f") : (on ? "1" : "0");
}

bool Editor::cursorInt(int& v) const {
    if (!cursor_) return false;
    const Node& n = prog_->at(cursor_);
    if (n.kind == Kind::MNote || n.kind == Kind::MBool) {
        v = (int32_t)n.a;
        return true;
    }
    if ((n.kind == Kind::Num || n.kind == Kind::MNum) && n.b == 1) {
        v = (int32_t)n.a;
        return true;
    }
    return false;
}

bool Editor::cursorRest() const { return cursor_ && prog_->at(cursor_).kind == Kind::MRest; }

// The step a mini node belongs to: climb out of modifier targets (bd in
// bd*2). A range's ends (lo .. hi) are values of their own, so stop there.
uint16_t stepOf(const Program& p, uint16_t n) {
    for (;;) {
        uint16_t par = p.parentOf(n);
        if (!par || !isMiniModifier(p.at(par).kind) || p.at(par).kind == Kind::MRange || p.at(par).first != n)
            return n;
        n = par;
    }
}

// The value to select in a step: descend into modifier targets.
uint16_t ed::valueOf(const Program& p, uint16_t step) {
    while (step && isMiniModifier(p.at(step).kind) && p.at(step).first) step = p.at(step).first;
    return step;
}

void Editor::enterNumber(int v, bool rest) {
    headGap_ = preSym_ = false;
    padAgain_ = -1;
    if (padBool()) {  // pads 1 / 2 = 0 / 1 (or f / t), pad 48 = a rest
        if (rest) return enterStep("~", "", "~");
        if (v != 0 && v != 1) return error("in a Bool pattern pad 1 = 0, pad 2 = 1, pad 48 = ~");
        const char* b = boolText(*prog_, cursor_, v == 1);
        return enterStep(b, "", b);
    }
    if (!padNumeric()) return error("pads enter numbers inside a numeric pattern");
    Text n = tnum(v);
    if (rest) return enterStep("~", "", "~");
    if (heldPads_ && chordHere()) return chordAdd(n, n);
    const uint16_t s0 = gapSeq_;
    const int p0 = gapPos_;
    enterStep(n, v < 0 ? "(" + n + ")" : n, n);
    if (entryDone(s0, p0)) chord_.push_back(n.c_str());
}

// Instrument mode: pads are the 48 instrument slots (samples_).
bool Editor::padInstruments() const {
    if (menuOpen_ || !cursor_) return false;
    const Node& n = prog_->at(cursor_);
    if (n.type != kTyPatBase + (int)Base::String) return false;
    if (!(n.kind == Kind::Mini || isMiniKind(n.kind))) return false;
    // only the pattern of s / sound (not arp "up", scale "major", ...)
    uint16_t m = cursor_;
    while (m && prog_->at(m).kind != Kind::Mini) m = prog_->parentOf(m);
    uint16_t app = m ? prog_->parentOf(m) : 0;
    if (!app || prog_->at(app).kind != Kind::App) return false;
    const Node& f = prog_->at(prog_->at(app).first);
    if (f.kind != Kind::Fn) return false;
    const char* name = lib((int)f.a).name;
    return strcmp(name, "s") == 0 || strcmp(name, "sound") == 0;
}

bool Editor::cursorOnSample() const {
    return cursor_ && prog_->at(cursor_).kind == Kind::MWord && padInstruments();
}

// as cursorInsert / doInsert decide
bool Editor::insertAddsBlock() const {
    if (menuOpen_ || kbdOpen_ || gapSeq_ || !cursor_ || cps_) return false;
    if (cursorMode_ && (headGap_ || preSym_)) return false;
    return prog_->at(cursor_).kind == Kind::Block;
}

int Editor::cursorInstrument() const {
    if (!cursor_ || prog_->at(cursor_).kind != Kind::MWord) return -1;
    const char* w = prog_->str((int)prog_->at(cursor_).a);
    for (int i = 0; i < kInstrumentSlots; i++)
        if (!strcmp(samples_[i], w)) return i;
    return -1;
}

void Editor::enterInstrument(int i) {
    headGap_ = preSym_ = false;
    padAgain_ = -1;
    if (!padInstruments()) return error("pads pick instruments inside an s pattern");
    if (i < 0 || i >= kInstrumentSlots || !samples_[i][0])
        return error("instrument slot " + tnum(i + 1) + " is empty");
    if (heldPads_ && chordHere()) return chordAdd(samples_[i], samples_[i]);
    const uint16_t s0 = gapSeq_;
    const int p0 = gapPos_;
    enterStep(samples_[i], "", samples_[i]);
    if (entryDone(s0, p0)) chord_.push_back(samples_[i]);
}

// ---- chords (held pads, Shift+Note) ----------------------------------------

void Editor::entryPadDown(int pad) {
    if (pad >= 0 && pad < 64) heldPads_ |= (uint64_t)1 << pad;
}

void Editor::entryPadUp(int pad) {
    if (pad >= 0 && pad < 64) heldPads_ &= ~((uint64_t)1 << pad);
    if (!heldPads_) chord_.clear();  // all up: the chord is done
}

// The cursor is still in the gap after the step the held pads entered.
bool Editor::chordHere() const {
    return !chord_.empty() && gapSeq_ && gapSeq_ == chordSeq_ && gapPos_ == chordPos_ && gapPos_ > 0 &&
           !menuOpen_;
}

// A pad entry ended in a gap it moved (a step entered): the start of a chord
// if more pads come while this one is held.
bool Editor::entryDone(uint16_t seq0, int pos0) {
    chord_.clear();
    if (!gapSeq_ || (gapSeq_ == seq0 && gapPos_ == pos0) || listGap() || gapPos_ < 1) return false;
    // only a plain value (not the range or group the pads just ended)
    const Kind k = prog_->at(prog_->child(gapSeq_, gapPos_ - 1)).kind;
    if (k != Kind::MNum && k != Kind::MNote && k != Kind::MWord) return false;
    chordSeq_ = gapSeq_;
    chordPos_ = gapPos_;
    return true;
}

// A pad while another is held: the step before the gap becomes the stack of
// every held pad's value, "[0,4,7]"; the cursor stays in the gap.
void Editor::chordAdd(const Text& code, const Label& label) {
    if (chord_.size() >= chord_.capacity()) return error("a chord takes at most 8 notes");
    for (const auto& c : chord_)
        if (code == c.c_str()) return info("already in the chord: " + label);
    chord_.push_back(code.c_str());
    Text st = "[";
    for (int i = 0; i < chord_.size(); i++) st = st + (i ? "," : "") + chord_[i].c_str();
    st = st + "]";
    const uint16_t seq = gapSeq_;
    const int pos = gapPos_;
    EditSpec sp = spec(EditKind::Replace, prog_->child(seq, pos - 1), st);
    sp.mini = true;
    uint16_t nn;
    if (!doSpec(sp, nn)) {
        chord_.pop_back();
        gapSeq_ = seq;
        gapPos_ = pos;
        cursor_ = seq;
        return error("chord " + st + " doesn't fit here");
    }
    gapSeq_ = seq;
    gapPos_ = pos;
    cursor_ = seq;
    if (chord_.size() == 2) ensureVoices();
    info("= chord " + st + "  (hold pads to add)");
}

uint16_t ed::findControlApp(const Program& p, uint16_t n, const char* name) {
    if (!n) return 0;
    const Node& x = p.at(n);
    if (x.kind == Kind::App && x.first && p.at(x.first).kind == Kind::Fn && !strcmp(lib((int)p.at(x.first).a).name, name))
        return n;
    for (uint16_t c = x.first; c; c = p.at(c).next)
        if (uint16_t f = findControlApp(p, c, name)) return f;
    return 0;
}

// Chords ring only with more than one voice: a sample block without a
// `voices` control gets `# voices 3` on its whole pattern (MIDI blocks
// play chords anyway; an existing voices is left as the user set it).
void Editor::ensureVoices() {
    if (slot_ >= kSampleSlots) return;
    const uint16_t b = prog_->block[slot_];
    if (!b || !exprRoot() || findControlApp(*prog_, b, "voices")) return;
    const uint16_t old = cursor_, seq = gapSeq_;
    const int pos = gapPos_;
    cursor_ = exprRoot();
    gapSeq_ = 0;
    uint16_t nn;
    EditSpec sp;
    const bool ok = tryWrapOp("#", "voices 3", true, nn, &sp) && doSpec(sp, nn);
    cursor_ = old;
    gapSeq_ = seq;
    gapPos_ = pos;
    if (!ok) error("chord: can't add # voices 3 (it would cut its own notes)");
}

// Shift+Note: chord names on the note at the cursor (in a gap: the step
// before it, e.g. right after pad entry). The root is written as a note name
// (Tidal: c5 = 0), since the chord syntax takes one: 4 -> e'maj, 12 -> c6'maj.
void Editor::openChords() {
    if (gapSeq_) {
        if (gapPos_ == 0 || listGap()) return error("chords: on a note, or in the gap after one");
        cursor_ = prog_->child(gapSeq_, gapPos_ - 1);
        gapSeq_ = 0;
    }
    if (!cursor_) return error("chords: on a note in an n / note pattern");
    const uint16_t v = valueOf(*prog_, cursor_);
    const Node& x = prog_->at(v);
    if (x.type != kTyPatBase + (int)Base::Note) return error("chords: on a note in an n / note pattern");
    int root = 0;
    bool chord = false;
    if (x.kind == Kind::MNote || ((x.kind == Kind::MNum) && x.b == 1)) {
        root = (int32_t)x.a;
    } else if (x.kind == Kind::MChord) {
        const char* w = prog_->str((int)x.a);
        const char* q = strchr(w, '\'');
        if (!q || !parseNoteName(w, (int)(q - w), root)) return error("chords: can't read this chord's root");
        chord = true;
    } else {
        return error("chords: on a whole-number note (0, e, c6 ...)");
    }
    if (const uint16_t par = prog_->parentOf(v))
        if (prog_->at(par).kind == Kind::MRange) return error("chords: not on a range's end (0 .. 7)");
    cursor_ = v;
    static const char* const kPc[12] = {"c", "cs", "d", "ef", "e", "f", "fs", "g", "af", "a", "bf", "b"};
    const int pc = ((root % 12) + 12) % 12, oct = 5 + (root - pc) / 12;
    Text name = kPc[pc];
    if (oct != 5) name = name + tnum(oct);
    MenuList& items = menuBuild();
    for (const char* c : {"maj", "min", "maj7", "dom7", "min7", "dim", "dim7", "aug", "sus2", "sus4", "six", "five"}) {
        const Text code = name + "'" + c;
        addReplaceItems(items, code, code, true);
    }
    if (chord) addReplaceItems(items, "single note " + tnum(root), tnum(root), true);
    menuVoices_ = true;
    openMenu("chord");
}

// Pad entry: replace the value at the cursor (mini code inside "...",
// exprCode for a number argument; empty = not allowed there), then select
// the next step's value or append a copy to type over (Delete removes it).
void Editor::enterStep(const Text& miniCode, const Text& exprCode, const Label& label) {
    if (gapSeq_) return insertAtGap(miniCode, label);
    const uint16_t oldCursor = cursor_;
    if (prog_->at(cursor_).kind == Kind::Mini) cursor_ = prog_->at(cursor_).first;  // whole "..."
    bool mini = isMiniKind(prog_->at(cursor_).kind);
    Text code = mini ? miniCode : exprCode;
    if (code.empty()) {
        cursor_ = oldCursor;
        return error(label + " only goes inside \"...\"");
    }
    uint16_t nn;
    EditSpec sp = spec(EditKind::Replace, cursor_, code);
    sp.mini = mini;
    if (!openSpec(sp, nn)) {
        cursor_ = oldCursor;
        return error(label + " doesn't fit here");
    }
    Program* r = prog_;  // the edit is open: the follow-up below is part of it
    uint16_t cur = nn, appended = 0;
    if (mini) {
        // replacing a whole sequence leaves a one-step sequence: use its step
        while (r->at(cur).kind == Kind::MSeq && r->at(cur).first && !r->at(r->at(cur).first).next)
            cur = r->at(cur).first;
        uint16_t step = stepOf(*r, cur), par = r->parentOf(step);
        if (par && r->at(par).kind == Kind::MRange) {
            if (r->at(step).next) {
                par = 0;  // lo: go on to hi
                cur = r->at(step).next;
            } else {
                step = stepOf(*r, par);  // hi: continue after the range
                par = r->parentOf(step);
            }
        }
        if (par && r->at(par).kind == Kind::MSeq) {
            // then the gap after the step: the next pads insert there (Delete
            // there removes the step before, the arrows move on)
            int idx = 0;
            for (uint16_t c = r->at(par).first; c && c != step; c = r->at(c).next) idx++;
            if (!endEdit(par)) {
                cursor_ = oldCursor;
                return;
            }
            gapSeq_ = par;
            gapPos_ = idx + 1;
            cursor_ = par;
            return info("= " + label + "  (gap: more pads insert more)");
        }
    }
    if (!endEdit(cur)) {
        cursor_ = oldCursor;
        return;
    }
    pending_ = appended;
    info("= " + label);
}

// The quoted pattern of the first `name "..."` (or `alt "..."`) in the tree
// under `n`, preorder; 0 if none.
// The "..." of a control's argument; through operators (`"0,7,10" +| "0 1 2"`)
// it follows the side the structure comes from: +| the right operand, |+
// the left, + / |+| the left.
static uint16_t structMini(const Program& p, uint16_t n) {
    for (int depth = 0; n && depth < 16; depth++) {
        const Node& x = p.at(n);
        if (x.kind == Kind::Mini) return n;
        if (x.kind != Kind::Op || !x.first) return 0;
        const char* op = lib((int)x.a).name;
        const int len = (int)strlen(op);
        const bool right = len >= 2 && op[len - 1] == '|' && op[0] != '|';
        n = right ? p.at(x.first).next : x.first;
    }
    return 0;
}

static uint16_t findControlMini(const Program& p, uint16_t n, const char* name, const char* alt) {
    if (!n) return 0;
    const Node& x = p.at(n);
    if (x.kind == Kind::App && x.first && p.at(x.first).kind == Kind::Fn) {
        const char* fn = lib((int)p.at(x.first).a).name;
        uint16_t arg = p.at(x.first).next;
        if ((!strcmp(fn, name) || !strcmp(fn, alt)) && arg)
            if (const uint16_t m = structMini(p, arg)) return m;
    }
    for (uint16_t c = x.first; c; c = p.at(c).next)
        if (uint16_t f = findControlMini(p, c, name, alt)) return f;
    return 0;
}

// Note / Instrument (v2): the cursor goes to the first step of the block's
// n / note (or s / sound) pattern; a block without one gets `# n "0"` /
// `# s "bd"` on its whole pattern first.
void Editor::jumpControl(bool notes) {
    const char* what = notes ? "n" : "s";
    if (!notes && slot_ >= kSampleSlots) return error("MIDI blocks (d9-d16) have no s pattern");
    if (emptyRowIndex() >= 0) {
        // the empty placeholder: start the block with the pattern this key jumps to
        const char* firstIns = "bd";
        for (int i = 0; i < kInstrumentSlots; i++)
            if (samples_[i][0]) {
                firstIns = samples_[i];
                break;
            }
        Text code;
        if (!notes) code.printf("s \"%s\"", firstIns);
        else if (slot_ >= kSampleSlots) code = "n \"0\"";
        else code.printf("n \"0\" # s \"%s\"", firstIns);
        ensureBlock();
        uint16_t nn;
        EditSpec sp = spec(EditKind::Replace, cursor_, code);
        if (!cursor_ || !doSpec(sp, nn)) {  // the edit's own reason too (1.0.0 device pass: hidden)
            const Label why = messageError_ ? message_ : Label("");
            return error(Label("can't start a block with ") + code.c_str() + (why.empty() ? "" : ": ") + why.c_str());
        }
    }
    uint16_t b = prog_->block[slot_];
    if (!b || !exprRoot()) return error("empty slot: replace to start a pattern");
    uint16_t mini = notes ? findControlMini(*prog_, b, "n", "note") : findControlMini(*prog_, b, "s", "sound");
    if (!mini) {
        const uint16_t old = cursor_;
        cursor_ = exprRoot();
        uint16_t nn;
        EditSpec sp;
        if (!tryWrapOp("#", notes ? "n \"0\"" : "s \"bd\"", true, nn, &sp)) {
            cursor_ = old;
            return error(Text("can't add # ") + what + " to this block");
        }
        if (!doSpec(sp, nn)) return;
        mini = notes ? findControlMini(*prog_, prog_->block[slot_], "n", "note")
                     : findControlMini(*prog_, prog_->block[slot_], "s", "sound");
        if (!mini) return;
    }
    uint16_t step = prog_->at(mini).first;
    while (step && prog_->at(step).kind == Kind::MSeq && prog_->at(step).first) step = prog_->at(step).first;
    cursor_ = step ? valueOf(*prog_, step) : mini;
    info(Text(what) + " pattern: " + (notes ? "pads enter notes" : "pads pick instruments"));
}

}  // namespace tc
TC_COLD_END
