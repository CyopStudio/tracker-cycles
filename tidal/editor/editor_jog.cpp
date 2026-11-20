// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// The jog wheel: numbers step by meaning, functions / controls / operators
// cycle through what fits (jogFits).
#include "editor_impl.h"

namespace tc {

void Editor::jog(int n, bool fast) {
    if (!n) return;
    const int dir = n > 0 ? 1 : -1, count = n > 0 ? n : -n;
    const Action a = dir > 0 ? (fast ? Action::WheelUpFast : Action::WheelUp)
                             : (fast ? Action::WheelDownFast : Action::WheelDown);
    const Kind k = cursor_ && !menuOpen_ && !kbdOpen_ ? prog_->at(cursor_).kind : Kind::None;
    if (k == Kind::Num || k == Kind::MNum || k == Kind::MNote || k == Kind::Fn || (k == Kind::Op && onOperator())) {
        jogMul_ = count;  // one edit of count steps
        act(a);
        jogMul_ = 1;
        return;
    }
    for (int i = 0; i < count && i < 8; i++) act(a);
}


// ---- jog steps by meaning (UI v3 B1) -----------------------------------------------
// The step for a number comes from what it is: `# gain 1` moves in 0.05,
// `fast 2` along a ladder of tempos, a cutoff in semitones ... (the user:
// "no way to edit # gain 1 to be 0.8"). Values stay exact decimals (the
// file writes numbers as decimals: no thirds).
bool ed::nameIn(const char* n, std::initializer_list<const char*> names) {
    for (const char* x : names)
        if (!strcmp(n, x)) return true;
    return false;
}

namespace {
enum class Step : uint8_t { Default, Gain, Unit, Secs, Freq, Speed, Ladder, Time, Count, Crush, Note };

// The function / control / operator a number belongs to, and which argument
// it is (1-based).
const char* numberOwner(const Program& p, uint16_t n, int& arg) {
    arg = 0;
    uint16_t x = n;
    if (isMiniKind(p.at(x).kind)) {  // a step of a "...": its string
        while (x && p.at(x).kind != Kind::Mini) x = p.parentOf(x);
        if (!x) return nullptr;
    }
    uint16_t par = p.parentOf(x);
    if (par && (p.at(par).kind == Kind::Tuple || p.at(par).kind == Kind::List)) {  // within (0, 0.5), toScale [..]
        x = par;
        par = p.parentOf(x);
    }
    if (!par) return nullptr;
    const Node& pn = p.at(par);
    if (pn.kind == Kind::App) {
        const uint16_t head = pn.first;
        if (head == x || p.at(head).kind != Kind::Fn) return nullptr;
        int i = 0;
        for (uint16_t c = pn.first; c && c != x; c = p.at(c).next) i++;
        arg = i;
        return lib((int)p.at(head).a).name;
    }
    if (pn.kind == Kind::Op || pn.kind == Kind::SectionL || pn.kind == Kind::SectionR) {
        arg = pn.kind == Kind::Op && pn.first != x ? 2 : 1;
        return lib((int)pn.a).name;
    }
    return nullptr;
}

Step stepFor(const char* f, int arg) {
    if (!f) return Step::Default;
    if (nameIn(f, {"gain", "amp", "velocity"})) return Step::Gain;
    if (nameIn(f, {"pan", "shape", "room", "size", "delay", "delayfeedback", "delayfb", "begin", "end", "legato",
                   "resonance", "hresonance", "lpq", "hpq", "distort", "ccv"}))
        return Step::Unit;
    if (nameIn(f, {"attack", "release", "att", "rel", "sustain", "hold"})) return Step::Secs;
    if (nameIn(f, {"cutoff", "hcutoff", "bandf", "lpf", "hpf", "bpf"})) return Step::Freq;
    if (nameIn(f, {"speed"})) return Step::Speed;
    if (nameIn(f, {"n", "note", "up"})) return Step::Note;
    if (nameIn(f, {"crush"})) return Step::Crush;
    if (arg == 1 && nameIn(f, {"degradeBy", "unDegradeBy", "sometimesBy", "sometimesBy'", "someCyclesBy", "juxBy"}))
        return Step::Unit;
    if (arg == 1 && nameIn(f, {"fast", "slow", "ply", "hurry", "segment", "discretise", "linger", "loopAt",
                               "fastGap", "inside", "outside", "swing"}))
        return Step::Ladder;
    if (nameIn(f, {"<~", "~>", "within", "within'", "zoom", "compress"})) return Step::Time;
    if (arg == 1 && nameIn(f, {"off", "swingBy", "trunc", "rolledBy", "pressBy"})) return Step::Time;
    if (nameIn(f, {"stut"})) return arg == 1 ? Step::Count : arg == 2 ? Step::Unit : Step::Time;
    if (nameIn(f, {"echo"})) return arg == 1 ? Step::Count : arg == 2 ? Step::Time : Step::Unit;
    if (nameIn(f, {"echoWith", "stutWith"})) return arg == 1 ? Step::Count : Step::Time;
    if (nameIn(f, {"every", "every'", "iter", "chunk", "chunk'", "euclid", "euclidInv", "euclidFull", "euclidOff",
                   "eoff", "chop", "striate", "plyWith", "shuffle", "scramble", "bite", "run", "scan", "whenmod",
                   "splice", "irand", "rot"}))
        return Step::Count;
    return Step::Default;
}

Rational qadd(Rational a, int64_t num, int64_t den) {  // a + num/den
    return makeRational((int64_t)a.num * den + num * (int64_t)a.den, (int64_t)a.den * den);
}
int qcmpR(Rational a, int64_t num, int64_t den) {  // sign of a - num/den
    const int64_t l = (int64_t)a.num * den, r = num * (int64_t)a.den;
    return l < r ? -1 : l > r ? 1 : 0;
}
Rational qclamp(Rational v, int64_t lo, int64_t hi, int64_t den) {  // lo/den .. hi/den
    if (qcmpR(v, lo, den) < 0) return makeRational(lo, den);
    if (qcmpR(v, hi, den) > 0) return makeRational(hi, den);
    return v;
}
}  // namespace

// The new value for `r` after `steps` jog detents (signed), by context.
static bool steppedValue(Step st, Rational r, int steps, bool fast, Rational& out) {
    const int dir = steps > 0 ? 1 : -1, count = steps > 0 ? steps : -steps;
    switch (st) {
        case Step::Gain:
            out = qclamp(qadd(r, (int64_t)steps * (fast ? 5 : 1), 20), 0, 2, 1);  // 0.05 / 0.25, 0 .. 2
            return true;
        case Step::Unit:
            out = qclamp(qadd(r, (int64_t)steps * (fast ? 5 : 1), 20), 0, 1, 1);  // 0.05 / 0.25, 0 .. 1
            return true;
        case Step::Secs:
            out = qclamp(qadd(r, (int64_t)steps * (fast ? 10 : 1), 100), 0, 10, 1);  // 0.01 / 0.1 s
            return true;
        case Step::Time:
            out = qadd(r, (int64_t)steps * (fast ? 4 : 1), 16);  // 1/16 / 1/4 cycle
            return true;
        case Step::Count:
            out = qclamp(qadd(r, (int64_t)steps * (fast ? 4 : 1), 1), 0, 1024, 1);
            return true;
        case Step::Crush:
            out = qclamp(qadd(r, (int64_t)steps * (fast ? 4 : 1), 1), 1, 16, 1);
            return true;
        case Step::Note:
            out = qadd(r, (int64_t)steps * (fast ? 12 : 1), 1);
            return true;
        case Step::Speed:
            if (fast) {  // x2 / /2 per detent
                int64_t num = r.num, den = r.den;
                for (int i = 0; i < count && i < 8; i++) (dir > 0 ? num : den) *= 2;
                out = makeRational(num, den);
            } else {
                out = qadd(r, steps, 10);  // 0.1
            }
            return true;
        case Step::Freq: {  // a semitone / an octave per detent, whole Hz, 20 .. 20000
            const double hz = (double)r.num / r.den;
            double v = hz * pow(2.0, (double)steps * (fast ? 12 : 1) / 12.0);
            int64_t h = (int64_t)(v + 0.5);
            if (h == (int64_t)(hz + 0.5)) h += dir;
            if (h < 20) h = 20;
            if (h > 20000) h = 20000;
            out = makeRational(h, 1);
            return true;
        }
        case Step::Ladder: {  // decimal-friendly tempo ladder
            static const int64_t kN[] = {1, 1, 1, 3, 1, 3, 2, 3, 4, 6, 8, 16};
            static const int64_t kD[] = {8, 4, 2, 4, 1, 2, 1, 1, 1, 1, 1, 1};
            const int n = (int)(sizeof kN / sizeof kN[0]);
            int at = -1;  // the ladder rung at or just past r in the jog's direction
            if (dir > 0) {
                for (int i = 0; i < n && at < 0; i++)
                    if (qcmpR(r, kN[i], kD[i]) < 0) at = i;
                if (at < 0) at = n - 1;
                at += (count - 1) * (fast ? 2 : 1) + (fast ? 1 : 0);
            } else {
                for (int i = n - 1; i >= 0 && at < 0; i--)
                    if (qcmpR(r, kN[i], kD[i]) > 0) at = i;
                if (at < 0) at = 0;
                at -= (count - 1) * (fast ? 2 : 1) + (fast ? 1 : 0);
            }
            if (at < 0) at = 0;
            if (at >= n) at = n - 1;
            out = makeRational(kN[at], kD[at]);
            return true;
        }
        default: return false;
    }
}


void Editor::wheel(int dir, bool fast) {
    if (!cursor_) return;
    if (cps_ && cursor_ == prog_->cpsExpr && prog_->at(cursor_).kind != Kind::Num) {
        const uint16_t top = cursor_, num = firstNumber(*prog_, top);
        if (!num) return;
        cursor_ = num;
        wheel(dir, fast);
        cursor_ = prog_->cpsExpr;  // stays on the line
        (void)top;
        return info(cursorInfo());
    }
    if (prog_->at(cursor_).kind == Kind::Fn) return wheelFn(dir * jogMul_);
    if (prog_->at(cursor_).kind == Kind::Op) return wheelOp(dir * jogMul_);
    if (onOperator()) return info("$: Delete unwraps it, Shift+Screen 7 puts a line break before it");
    if (prog_->at(cursor_).kind == Kind::MReplicate && !(prog_->at(cursor_).flags & kBare)) {
        // x!n: the count (RC14, Shift+Screen 6 makes these)
        const int v = (int)prog_->at(cursor_).a + dir * jogMul_ * (fast ? 4 : 1);
        if (v < 1 || v > kMaxSeqSteps) return error("! counts go from 1 to 64");
        if (!beginEdit()) return;
        prog_->at(cursor_).a = (uint32_t)v;
        Label m;
        if (endEdit(cursor_)) info(m.printf("!%d", v));
        return;
    }
    if (!beginEdit()) return;
    Program* p = prog_;
    Node& n = prog_->at(cursor_);
    Node& m = n;  // in place
    switch (n.kind) {
        case Kind::Num:
        case Kind::MNum: {
            int dec = nodeDecimals(n);
            Rational r = nodeRational(n);
            {  // by meaning when the number's owner says (gain, cutoff, fast ...)
                int arg = 0;
                const char* owner = numberOwner(*p, cursor_, arg);  // sets arg: not in the same call
                const Step st = stepFor(owner, arg);
                Rational nr;
                if (steppedValue(st, r, dir * jogMul_, fast, nr)) {
                    setNodeRational(m, nr, 0);  // as many decimals as the value needs
                    if ((int32_t)m.a < 0) m.flags |= kNegParen;
                    else m.flags &= ~kNegParen;
                    break;
                }
            }
            int64_t scale = 1;
            for (int k = 0; k < dec; k++) scale *= 10;
            int64_t step = (fast ? 10 : 1) * (int64_t)jogMul_;
            // value + dir*step/scale
            int64_t num = (int64_t)r.num * scale + dir * step * (int64_t)r.den;
            Rational nr = makeRational(num, (int64_t)r.den * scale);
            setNodeRational(m, nr, dec);
            if ((int32_t)m.a < 0) m.flags |= kNegParen;
            else m.flags &= ~kNegParen;
            break;
        }
        case Kind::MNote:
            m.a = (uint32_t)((int32_t)m.a + dir * (fast ? 12 : 1) * jogMul_);
            m.b = 0;
            break;
        case Kind::MBool: m.a = !m.a; break;
        case Kind::MWord: {
            if (fast) {
                int idx = (int)m.b - 1 + dir;  // -1 = no index
                m.b = idx < 0 ? 0 : (uint32_t)idx + 1;
            } else {
                const char* cur = p->str((int)m.a);
                int i = 0;
                for (; i < kInstrumentSlots; i++)
                    if (!strcmp(samples_[i], cur)) break;
                const int ns = kInstrumentSlots;
                int k = i < ns ? i : 0;
                for (int t = 0; t < ns; t++) {  // next non-empty slot
                    k = ((k + dir) % ns + ns) % ns;
                    if (samples_[k][0]) break;
                }
                if (!samples_[k][0]) return abortEdit(), error("no instruments");
                int id = p->intern(samples_[k], (int)strlen(samples_[k]));
                if (id < 0) return abortEdit(), error("too many words");
                m.a = (uint32_t)id;
            }
            break;
        }
        default: abortEdit(); error("the wheel changes numbers, notes, samples and functions"); return;
    }
    if (!endEdit(cursor_)) return;
    state_[slot_] = SlotState::Edited;
    info(renderNode(*prog_, cursor_));
}

// Wheel on a function name: step through the functions that type-check here.
// Jog on a function's name / an operator's symbol (user, after RC21:
// "scrolling on rev caused audio dropouts"): each detent used to type-check
// the whole block once per library entry (~120 trials, up to 8 detents a
// pass). Now the functions that fit at this spot are worked out once and
// kept while the jog goes on (only this spot's own swaps happen meanwhile):
// an entry with the very same type needs no trial (only the Tracker-mode
// check), one with the same number of arguments one trial, others can't
// fit. The list is alphabetical (like the menus); n detents = one edit.
static int gFitTrials = 0;  // trial checks made by the last jogFits (tests)
int jogFitTrials() { return gFitTrials; }
static int sigArity(const char* sig) {
    int depth = 0, n = 0;
    for (const char* c = sig; *c; c++) {
        if (*c == '(' || *c == '[') depth++;
        else if (*c == ')' || *c == ']') depth--;
        else if (depth == 0 && c[0] == '-' && c[1] == '>') n++;
    }
    return n;
}

void Editor::jogFits(bool op) {
    if (fitValid_ && fitNode_ == cursor_ && fitChanges_ == changes_ && fitOp_ == op) return;
    fits_.clear();
    gFitTrials = 0;
    const int cur = (int)prog_->at(cursor_).a;
    const char* sig = lib(cur).sig;
    const int arity = sigArity(sig);
    for (int id = 0; id < libCount(); id++) {
        if (op != libIsOp(id) || libIsAlias(id)) continue;
        // a control only swaps with a control, a function with a function
        if ((lib(id).cat == Cat::Control) != (lib(cur).cat == Cat::Control)) continue;
        bool fit = id == cur;
        if (!fit && !strcmp(lib(id).sig, sig)) {
            fit = !trackerMode_ || !trackerLibIssue(id, slot_);
        } else if (!fit && sigArity(lib(id).sig) == arity) {
            gFitTrials++;
            if (op) {
                EditSpec sp;
                sp.kind = EditKind::SetOp;
                sp.target = cursor_;
                sp.launch = id;
                uint16_t nn;
                fit = trySpec(sp, nn);
            } else {
                if (!beginEdit()) return;
                prog_->at(cursor_).a = (uint32_t)id;
                Text err;
                fit = recheck(*prog_, err);
                abortEdit();
            }
        }
        if (fit && !fits_.push_back((int16_t)id)) break;
    }
    for (int a = 1; a < (int)fits_.size() && !op; a++)  // functions alphabetical; operators by family (as the menu)
        for (int k = a; k > 0 && strcmp(lib(fits_[k - 1]).name, lib(fits_[k]).name) > 0; k--) {
            const int16_t t = fits_[k];
            fits_[k] = fits_[k - 1];
            fits_[k - 1] = t;
        }
    fitValid_ = true;
    fitNode_ = cursor_;
    fitOp_ = op;
    fitChanges_ = changes_;
}

void Editor::wheelFn(int dir) {
    jogFits(false);
    const int n = (int)fits_.size(), cur = (int)prog_->at(cursor_).a;
    int at = 0;
    for (int i = 0; i < n; i++)
        if (fits_[i] == cur) at = i;
    if (n < 2) return info("no other function fits here");
    const int next = fits_[((at + dir) % n + n) % n];
    if (!beginEdit()) return;
    prog_->at(cursor_).a = (uint32_t)next;
    if (endEdit(cursor_)) {      // one check (it also adds # voices for jux ...), not one per function
        fitChanges_ = changes_;  // still this spot's list
        info(lib(next).name);
    }
}

void Editor::wheelOp(int dir) {
    jogFits(true);
    const int n = (int)fits_.size(), cur = (int)prog_->at(cursor_).a;
    int at = 0;
    for (int i = 0; i < n; i++)
        if (fits_[i] == cur) at = i;
    if (n < 2) return info("no other operator fits here");
    const int next = fits_[((at + dir) % n + n) % n];
    EditSpec s;
    s.kind = EditKind::SetOp;
    s.target = cursor_;
    s.launch = next;
    uint16_t nn;
    if (doSpec(s, nn)) {
        onOp_ = true;
        fitChanges_ = changes_;
        info(lib(next).name);
    }
}

}  // namespace tc
