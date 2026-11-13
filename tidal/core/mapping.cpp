// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// See mapping.h and docs/tidal_control_mapping.md. Control semantics
// follow Tidal / SuperDirt's documented behaviour: amplitude = amp x
// min(gain, 2)^4 (x velocity), amp 0.4 by default; legato = note length as
// a fraction of the event, sustain in seconds; speed = playback rate,
// negative plays backwards; crush 1..16 bits; shape / pan / room / delay /
// resonance 0..1; cutoff / hcutoff / bandf in Hz; attack / release in
// seconds. MIDI: note 0 = MIDI 60.
#include "mapping.h"

#include "library.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace tc {

namespace {

constexpr int kSampleNote0 = 60;   // Tracker C5 = the sample's own pitch
constexpr int kMaxSampleNote = 120;
constexpr float kDefaultAmp = 0.4f;
constexpr int kMinBits = 4;        // Tracker BIT_DEPTH_MIN
constexpr int kMaxBits = 16;
constexpr int kMaxEnvMs = 10000;   // Tracker ATTACK_MAX / RELEASE_MAX
constexpr int kMaxDelayMs = 3500;  // Tracker DELAY_TIME_MAX / MAX_SHORT_DELAY_TIME
// The Tracker filter: cutoff c in 0..1 picks entry i = c x 477 of a table
// of 20 Hz x 2^(i / 50).
constexpr float kFilterSteps = 477.0f, kFilterOctaveSteps = 50.0f, kFilterLowHz = 20.0f;

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
inline int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
inline uint16_t unit16(float v) { return (uint16_t)lroundf(clampf(v, 0.0f, 1.0f) * 65535.0f); }
inline uint16_t percent(float v) { return (uint16_t)lroundf(clampf(v, 0.0f, 1.0f) * 100.0f); }

int lowerc(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

// `name` equals the file name without its extension, ignoring case.
bool stemMatches(const char* name, const char* file) {
    if (!file) return false;
    char tok[16];
    sampleToken(file, tok, sizeof tok);
    int i = 0;
    for (; name[i]; i++)
        if (lowerc((unsigned char)name[i]) != tok[i]) return false;
    return tok[i] == 0 && i > 0;
}

}  // namespace

int sampleToken(const char* file, char* out, int cap) {
    if (cap <= 0) return 0;
    int n = 0;
    if (file) {
        const char* dot = strrchr(file, '.');  // only the last dot starts the extension
        const char* end = dot && dot != file ? dot : file + strlen(file);
        const int max = cap - 1 < 15 ? cap - 1 : 15;
        for (const char* c = file; c < end && n < max; c++) {
            const int ch = lowerc((unsigned char)*c);
            out[n++] = (char)(isalnum(ch) || ch == '-' || ch == '_' || ch == '.' ? ch : '_');
        }
    }
    out[n] = 0;
    return n;
}

namespace {

uint32_t hashName(const char* s) {
    uint32_t h = 2166136261u;
    for (; *s; s++) h = (h ^ (uint8_t)*s) * 16777619u;
    return h;
}

struct ModList {
    uint16_t val[(int)TMod::Count];
    bool has[(int)TMod::Count];
    void set(TMod m, uint16_t v) {
        has[(int)m] = true;
        val[(int)m] = v;
    }
};

// Where each control does something on the Tracker (design sections 3-4).
enum Where : uint8_t { kNowhere = 0, kSample = 1, kMidi = 2, kBoth = 3 };
struct CtlRule {
    const char* name;
    Where where;
    const char* why;  // kNowhere: the reason
};
const CtlRule kCtlRules[] = {
    {"s", kSample, nullptr},        {"sound", kSample, nullptr},    {"n", kBoth, nullptr},
    {"note", kBoth, nullptr},       {"up", kBoth, nullptr},         {"gain", kBoth, nullptr},
    {"amp", kBoth, nullptr},        {"velocity", kBoth, nullptr},   {"legato", kBoth, nullptr},
    {"sustain", kBoth, nullptr},    {"nudge", kBoth, nullptr},      {"pan", kSample, nullptr},
    {"cutoff", kSample, nullptr},   {"lpf", kSample, nullptr},      {"resonance", kSample, nullptr},
    {"lpq", kSample, nullptr},      {"hpf", kSample, nullptr},      {"hresonance", kSample, nullptr},
    {"hpq", kSample, nullptr},      {"bandf", kSample, nullptr},    {"bpf", kSample, nullptr},
    {"delay", kSample, nullptr},    {"room", kSample, nullptr},     {"crush", kSample, nullptr},
    {"shape", kSample, nullptr},    {"distort", kSample, nullptr},  {"begin", kSample, nullptr},
    {"end", kSample, nullptr},      {"speed", kSample, nullptr},    {"attack", kSample, nullptr},
    {"att", kSample, nullptr},      {"release", kSample, nullptr},  {"rel", kSample, nullptr},
    {"cut", kSample, nullptr},      {"midichan", kMidi, nullptr},   {"ccn", kMidi, nullptr},
    {"ccv", kMidi, nullptr},        {"midibend", kMidi, nullptr},   {"voices", kSample, nullptr},
    {"size", kSample, nullptr},     {"sz", kSample, nullptr},       {"delaytime", kSample, nullptr},
    {"delayt", kSample, nullptr},   {"delayfeedback", kSample, nullptr}, {"delayfb", kSample, nullptr},
    {"djf", kSample, nullptr},      {"progNum", kMidi, nullptr},    {"miditouch", kMidi, nullptr},
    {"coarse", kNowhere, "isn't available on the Tracker (no sample-rate reduction: use crush)"},
    {"orbit", kNowhere, "isn't available on the Tracker (one effects bus)"},
    {"unit", kNowhere, "isn't available on the Tracker (no cycle-relative speed)"},
};

}  // namespace

const char* trackerLibIssue(int libId, int slot) {
    const LibEntry& e = lib(libId);
    if (e.device == Device::HostOnly) return "isn't available on the Tracker";
    if (e.cat != Cat::Control) return nullptr;
    const bool midi = slot >= kSampleSlots;
    for (const CtlRule& r : kCtlRules) {
        if (strcmp(r.name, e.name) != 0) continue;
        if (r.where == kNowhere) return r.why;
        if (midi && !(r.where & kMidi)) return "has no effect on MIDI slots (d9-d16)";
        if (!midi && !(r.where & kSample)) return "only works on MIDI slots (d9-d16)";
        return nullptr;
    }
    return "isn't mapped on the Tracker";
}

namespace {
// Depth-first; depth is bounded by the tree's nesting (the parser and the
// editor cap it far below this), width by nothing.
bool checkNode(const Program& p, uint16_t x, int slot, int depth, Error& err) {
    if (depth > 200) {
        err = Error();
        err.set(-1, "tracker mode: expression too deep to check");
        return false;
    }
    const Node& n = p.at(x);
    if (n.kind == Kind::Fn) {
        if (const char* why = trackerLibIssue((int)n.a, slot)) {
            err = Error();
            err.set(-1, "tracker mode: '%s' %s", lib((int)n.a).name, why);
            err.node = x;
            return false;
        }
    }
    for (uint16_t c = n.first; c; c = p.at(c).next)
        if (!checkNode(p, c, slot, depth + 1, err)) return false;
    return true;
}
}  // namespace

bool trackerCheck(const Program& p, int slot, Error& err) {
    const uint16_t b = p.block[slot];
    return !b || checkNode(p, b, slot, 0, err);
}

void TrackerMapper::init(const TrackerHost* host, float sampleRate) {
    host_ = host;
    fs_ = sampleRate;
    kS_ = controlKey("s");
    kN_ = controlKey("n");
    kNote_ = controlKey("note");
    kSpeed_ = controlKey("speed");
    kGain_ = controlKey("gain");
    kAmp_ = controlKey("amp");
    kVelocity_ = controlKey("velocity");
    kPan_ = controlKey("pan");
    kCutoff_ = controlKey("cutoff");
    kHcutoff_ = controlKey("hcutoff");
    kBandf_ = controlKey("bandf");
    kRes_ = controlKey("resonance");
    kHres_ = controlKey("hresonance");
    kShape_ = controlKey("shape");
    kDistort_ = controlKey("distort");
    kCrush_ = controlKey("crush");
    kRoom_ = controlKey("room");
    kDelay_ = controlKey("delay");
    kBegin_ = controlKey("begin");
    kEnd_ = controlKey("end");
    kAttack_ = controlKey("attack");
    kRelease_ = controlKey("release");
    kLegato_ = controlKey("legato");
    kSustain_ = controlKey("sustain");
    kMidichan_ = controlKey("midichan");
    kVoices_ = controlKey("voices");
    kCcn_ = controlKey("ccn");
    kCcv_ = controlKey("ccv");
    kBend_ = controlKey("midibend");
    kSize_ = controlKey("size");
    kDelayTime_ = controlKey("delaytime");
    kDelayFb_ = controlKey("delayfeedback");
    kDjf_ = controlKey("djf");
    kProg_ = controlKey("progNum");
    kTouch_ = controlKey("miditouch");
    instrumentsChanged();
    resetWarnings();
}

void TrackerMapper::instrumentsChanged() {
    for (CacheSlot& c : cache_) c.instr = -2;
}

void TrackerMapper::resetWarnings() { memset(warn_, 0, sizeof warn_); }

uint16_t TrackerMapper::cutoffValue(float hz) {
    if (!(hz > kFilterLowHz)) return 0;
    float c = log2f(hz / kFilterLowHz) * kFilterOctaveSteps / kFilterSteps;
    return unit16(c);
}

int TrackerMapper::findInstrument(const char* name) {
    const size_t len = strlen(name);
    CacheSlot* slot = nullptr;
    if (len < sizeof cache_[0].name) {
        uint32_t h = hashName(name);
        for (int k = 0; k < kCache; k++) {
            CacheSlot& c = cache_[(h + k) % kCache];
            if (c.instr == -2) {
                slot = &c;
                break;
            }
            if (!strcmp(c.name, name)) return c.instr;
        }
    }
    int found = -1;
    const int n = host_->instrumentCount();
    for (int i = 0; i < n && found < 0; i++) {
        InstrumentInfo info = host_->instrument(i);
        if (info.active && stemMatches(name, info.name)) found = i;
    }
    if (found < 0) found = host_->fallbackInstrument(name);
    if (slot) {  // a full cache just stops caching new names
        memcpy(slot->name, name, len + 1);
        slot->instr = (int8_t)found;
    }
    return found;
}

bool TrackerMapper::map(const Program& p, int slot, const Event& e, float, float lenSamples, Entry& out) {
    if (!e.isMap) return false;
    MapWarnings& w = warn_[slot];

    // gather the controls the mapping uses (the last value of a key wins)
    char name[32] = {0};
    bool hasName = false;
    float n = 0, note = 0, speed = 1, gain = 1, amp = kDefaultAmp, velocity = 1;
    bool hasVolume = false;
    int filter = -1;  // key of the last of cutoff / hcutoff / bandf
    float filterHz = 0;
    int filters = 0;
    bool hasRes = false, hasHres = false, hasPan = false, hasDrive = false, hasCrush = false, hasRoom = false,
         hasDelay = false, hasBegin = false, hasEnd = false, hasAttack = false, hasRelease = false,
         hasLegato = false, hasSustain = false, hasChan = false, hasCcn = false, hasCcv = false, hasBend = false,
         hasSize = false, hasDelayTime = false, hasDelayFb = false, hasDjf = false, hasProg = false, hasTouch = false,
         hasN = false, hasNote = false;
    float size = 0, delayTime = 0, delayFb = 0, djf = 0.5f, prog = 0, touch = 0;
    float voices = 1;
    float res = 0, hres = 0, pan = 0.5f, drive = 0, crush = 16, room = 0, delay = 0, begin = 0, end = 1, attack = 0,
          release = 0, legato = 1, sustain = 0, chan = 0, ccn = 0, ccv = 0, bend = 0;
    for (int i = 0; i < e.nctl; i++) {
        const Ctl& c = e.ctl[i];
        const int k = c.key;
        const float v = c.f;
        if (k == kS_) {
            formatCtl(p, c, name, sizeof name);
            hasName = true;
        } else if (k == kN_) {
            n = v, hasN = true;
        } else if (k == kNote_) {
            note = v, hasNote = true;
        } else if (k == kSpeed_) {
            speed = v;
        } else if (k == kGain_) {
            gain = v;
            hasVolume = true;
        } else if (k == kAmp_) {
            amp = v;
            hasVolume = true;
        } else if (k == kVelocity_) {
            velocity = v;
            hasVolume = true;
        } else if (k == kPan_) {
            pan = v, hasPan = true;
        } else if (k == kCutoff_ || k == kHcutoff_ || k == kBandf_) {
            filter = k, filterHz = v, filters++;
        } else if (k == kRes_) {
            res = v, hasRes = true;
        } else if (k == kHres_) {
            hres = v, hasHres = true;
        } else if (k == kShape_ || k == kDistort_) {
            drive = hasDrive && drive > v ? drive : v, hasDrive = true;
        } else if (k == kCrush_) {
            crush = v, hasCrush = true;
        } else if (k == kRoom_) {
            room = v, hasRoom = true;
        } else if (k == kDelay_) {
            delay = v, hasDelay = true;
        } else if (k == kBegin_) {
            begin = v, hasBegin = true;
        } else if (k == kEnd_) {
            end = v, hasEnd = true;
        } else if (k == kAttack_) {
            attack = v, hasAttack = true;
        } else if (k == kRelease_) {
            release = v, hasRelease = true;
        } else if (k == kLegato_) {
            legato = v, hasLegato = true;
        } else if (k == kSustain_) {
            sustain = v, hasSustain = true;
        } else if (k == kMidichan_) {
            chan = v, hasChan = true;
        } else if (k == kCcn_) {
            ccn = v, hasCcn = true;
        } else if (k == kCcv_) {
            ccv = v, hasCcv = true;
        } else if (k == kBend_) {
            bend = v, hasBend = true;
        } else if (k == kVoices_) {
            voices = v;
        } else if (k == kSize_) {
            size = v, hasSize = true;
        } else if (k == kDelayTime_) {
            delayTime = v, hasDelayTime = true;
        } else if (k == kDelayFb_) {
            delayFb = v, hasDelayFb = true;
        } else if (k == kDjf_) {
            djf = v, hasDjf = true;
        } else if (k == kProg_) {
            prog = v, hasProg = true;
        } else if (k == kTouch_) {
            touch = v, hasTouch = true;
        }
    }
    const float ratio = (amp / kDefaultAmp) * powf(clampf(gain, 0.0f, 2.0f), 4.0f) * velocity;

    ModList m;
    memset(&m, 0, sizeof m);
    out.kind = EntryKind::Note;
    out.voices = (uint8_t)clampi((int)lroundf(voices), 1, kMaxVoices);

    if (slot >= kSampleSlots) {  // ---- MIDI
        out.chan = (uint8_t)(hasChan ? clampi((int)lroundf(chan), 0, 15) : (slot - kSampleSlots) % 16);
        out.note = (uint8_t)clampi(60 + (int)lroundf(note + n), 0, 127);
        // velocity 1 (the default) = MIDI velocity 127 (user, before 1.0.0)
        out.vel = (uint8_t)(hasVolume ? clampi((int)lroundf(127.0f * ratio), 0, 127) : 127);
        float len = hasLegato ? lenSamples * legato : hasSustain ? sustain * fs_ : lenSamples;
        out.dur = len > 1.0f ? (uint32_t)len : 1;
        // no n / note but MIDI controls (`ccv "0 64" # ccn 74`): just those
        // messages, as Tidal users send them; no note (an event with neither
        // plays note 60, n 0, as in Tidal)
        if (!hasN && !hasNote && ((hasCcn && hasCcv) || hasBend || hasProg || hasTouch)) {
            out.kind = EntryKind::Control;
            out.dur = 0;  // no noteOff
        }
        if (hasCcn && hasCcv)
            m.set(TMod::Cc, (uint16_t)(clampi((int)lroundf(ccn), 0, 127) << 8 | clampi((int)lroundf(ccv), 0, 127)));
        if (hasBend) m.set(TMod::Bend, (uint16_t)clampi(8192 + (int)lroundf(bend), 0, 16383));
        if (hasProg) m.set(TMod::Program, (uint16_t)clampi((int)lroundf(prog), 0, 127));
        if (hasTouch) m.set(TMod::Touch, (uint16_t)clampi((int)lroundf(touch), 0, 127));
    } else {  // ---- sample track
        if (!hasName) {
            w.noSound++;
            return false;
        }
        // `s "bd:3"`: the engine already split it into s "bd" + n 3
        const int instr = findInstrument(name);
        if (instr < 0) {
            w.unknownName++;
            snprintf(w.lastUnknown, sizeof w.lastUnknown, "%s", name);
            return false;
        }
        if (speed == 0.0f) return false;  // rate 0: nothing plays
        const InstrumentInfo info = host_->instrument(instr);
        const bool sliced = info.kind == InstrKind::Slice && info.slices > 0;
        out.chan = (uint8_t)instr;

        // pitch: note + n (unless n picks a slice) + speed as semitones
        float semis = note + (sliced ? 0.0f : n);
        if (speed < 0) m.set(TMod::Reverse, 1);
        if (fabsf(speed) != 1.0f) semis += 12.0f * log2f(fabsf(speed));
        const int whole = (int)floorf(semis + 0.5f);
        const int cents = (int)lroundf((semis - (float)whole) * 100.0f);
        out.note = (uint8_t)clampi(kSampleNote0 + whole, 0, kMaxSampleNote);
        if (cents) m.set(TMod::FineTune, (uint16_t)(int16_t)cents);
        if (sliced) {
            int s = (int)floorf(n) % info.slices;
            m.set(TMod::Slice, (uint16_t)(s < 0 ? s + info.slices : s));
        }

        out.vel = hasVolume ? host_->volumeStep(instr, ratio) : 255;

        if (filter >= 0) {
            if (filters > 1) w.filterClash++;
            const TMod t = filter == kCutoff_ ? TMod::CutoffLP : filter == kHcutoff_ ? TMod::CutoffHP : TMod::CutoffBP;
            m.set(t, cutoffValue(filterHz));
            const bool hp = t == TMod::CutoffHP;
            if (hp ? hasHres : hasRes) m.set(TMod::Resonance, unit16(hp ? hres : res));
        }
        // djf (DJ filter): below 0.5 a low-pass closing towards 0, above a
        // high-pass opening towards 1, 0.5 none; an explicit filter wins
        if (hasDjf && fabsf(djf - 0.5f) > 0.001f) {
            if (filter >= 0) {
                w.filterClash++;
            } else if (djf < 0.5f) {
                m.set(TMod::CutoffLP, unit16(clampf(djf * 2.0f, 0.0f, 1.0f)));
            } else {
                m.set(TMod::CutoffHP, unit16(clampf((djf - 0.5f) * 2.0f, 0.0f, 1.0f)));
            }
        }
        if (hasSize) m.set(TMod::RevSize, unit16(clampf(size, 0.0f, 1.0f)));
        if (hasDelayTime) m.set(TMod::DelayTime, (uint16_t)clampi((int)lroundf(delayTime * 1000.0f), 1, kMaxDelayMs));
        if (hasDelayFb) m.set(TMod::DelayFb, (uint16_t)clampi((int)lroundf(delayFb * 100.0f), 0, 99));
        if (hasPan) m.set(TMod::Pan, percent(pan));
        if (hasDrive) m.set(TMod::Overdrive, percent(drive));
        if (hasCrush) m.set(TMod::BitDepth, (uint16_t)clampi((int)lroundf(crush), kMinBits, kMaxBits));
        if (hasRoom) m.set(TMod::Reverb, percent(room));
        if (hasDelay) m.set(TMod::Delay, percent(delay));
        const bool points = info.kind == InstrKind::OneShot || info.kind == InstrKind::Loop;
        if (points && hasBegin) m.set(TMod::Begin, unit16(begin));
        if (points && hasEnd) m.set(TMod::End, unit16(end));
        if (hasAttack) m.set(TMod::Attack, (uint16_t)clampi((int)lroundf(attack * 1000.0f), 0, kMaxEnvMs));
        if (hasRelease) m.set(TMod::Release, (uint16_t)clampi((int)lroundf(release * 1000.0f), 0, kMaxEnvMs));

        // note length: legato / sustain; looping kinds release at the event's
        // end; one-shots and slices play out
        float len = 0;
        if (hasLegato) len = lenSamples * legato;
        else if (hasSustain) len = sustain * fs_;
        else if (info.kind != InstrKind::OneShot && info.kind != InstrKind::Slice) len = lenSamples;
        out.dur = len > 0 ? (len > 1.0f ? (uint32_t)len : 1) : 0;
    }

    // overrides in priority order (TMod order); past kMaxMods they're dropped
    int cnt = 0, total = 0;
    for (int t = 1; t < (int)TMod::Count; t++) {
        if (!m.has[t]) continue;
        total++;
        if (cnt == kMaxMods) continue;
        out.modId[cnt] = (uint8_t)t;
        out.modVal[cnt] = m.val[t];
        cnt++;
    }
    if (total > cnt) w.droppedMods++;
    out.nmods = (uint8_t)cnt;
    return true;
}

}  // namespace tc
