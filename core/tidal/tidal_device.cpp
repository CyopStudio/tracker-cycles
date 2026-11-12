// See tidal_device.h.
#ifndef PLATFORM_IMX
#include "tidal_device.h"

#include "ampLogarythmicValues.h"
#include "mtAudioEngine.h"
#include "mtMidi.h"
#include "mtStructs.h"

namespace {

constexpr uint8_t kMidiAllNotesOff = 123;

tc::InstrKind kindOf(const strInstrument& ins) {
    if (ins.sample.type == mtSampleTypeWavetable) return tc::InstrKind::Wavetable;
    switch (ins.playMode) {
        case playModeSingleShot: return tc::InstrKind::OneShot;
        case playModeSlice:
        case playModeBeatSlice: return tc::InstrKind::Slice;
        case playModeGranular: return tc::InstrKind::Granular;
        case playModeWavetable: return tc::InstrKind::Wavetable;
        default: return tc::InstrKind::Loop;
    }
}

}  // namespace

int DeviceHost::instrumentCount() const { return kSampleInstrumentsCount; }

tc::InstrumentInfo DeviceHost::instrument(int i) const {
    const strInstrument& ins = mtProject.instrument[i];
    return tc::InstrumentInfo{ins.isActive == 1, kindOf(ins), ins.sliceNumber, ins.sample.file_name};
}

uint8_t DeviceHost::volumeStep(int i, float ratio) const {
    const float want = ampLogValues[mtProject.instrument[i].volume] * ratio;
    // ampLogValues rises with the index: first step >= want, then the nearer of it and the one below
    int lo = 0, hi = AMP_LOG_VALUES - 1;
    if (want >= ampLogValues[hi]) return (uint8_t)hi;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (ampLogValues[mid] < want) lo = mid + 1;
        else hi = mid;
    }
    if (lo > 0 && want - ampLogValues[lo - 1] < ampLogValues[lo] - want) lo--;
    return (uint8_t)lo;
}

static_assert(DeviceVoice::kPlayers == playerEngine::kTidalPlayers, "players per track");
static_assert(DeviceVoice::kTracks == audioEngine::kVoices, "tracks");

bool DeviceVoice::sounding(int t, int k, uint32_t now) const {
    if (cutting_[t][k]) return false;  // fading out (5 ms): already given up
    if (instrumentPlayer[t].tidalPlaying((uint8_t)k)) return true;
    return started_[t][k] && now - startBlock_[t][k] < 2;  // just started, may not report it yet
}

void DeviceVoice::cut(int t, int k) {
    instrumentPlayer[t].tidalCut((uint8_t)k);
    serial_[t][k] = (uint16_t)((serial_[t][k] + 1) & 0x3FFF);  // its pending noteOff is now stale
    cutting_[t][k] = true;
    cutBlock_[t][k] = now_;
}

void DeviceVoice::blockLoad(int us) {
    // % of the 2902 us block; smoothing: fast up (~4 blocks), slow down
    // (~32 blocks, ~93 ms), so a burst lowers the cap at once
    const uint32_t pct = us <= 0 ? 0 : (uint32_t)us * 100u / 2902u;
    const uint32_t x = (pct > 200 ? 200 : pct) << 8;
    if (x > loadQ8_) loadQ8_ += (x - loadQ8_) >> 2;
    else loadQ8_ -= (loadQ8_ - x) >> 5;
    if ((loadQ8_ >> 8) > vstats.loadMax) vstats.loadMax = loadQ8_ >> 8;
}

int DeviceVoice::effectiveCap() const {
    const int load = loadPercent();
    if (load <= kLoadLo) return cap_;
    const int lo = kMinCap < cap_ ? kMinCap : cap_;
    if (load >= kLoadHi) return lo;
    return cap_ - (cap_ - lo) * (load - kLoadLo) / (kLoadHi - kLoadLo);
}

bool DeviceVoice::oldestSounding(uint32_t now, int& t, int& k) const {
    t = k = -1;
    for (int i = 0; i < kTracks; i++)
        for (int j = 0; j < kPlayers; j++)
            if (sounding(i, j, now) && (t < 0 || (int32_t)(started_[i][j] - started_[t][k]) < 0)) t = i, k = j;
    return t >= 0;
}

int DeviceVoice::pickPlayer(int slot, uint8_t instr, int voices, uint32_t now) {
    now_ = now;
    const int cap = effectiveCap();
    if (!vstats.capMin || (uint32_t)cap < vstats.capMin) vstats.capMin = (uint32_t)cap;
    vstats.capSum += (uint32_t)cap;
    // a fade that didn't end (an envelope mode without a timed release): stop it
    for (int t = 0; t < kTracks; t++)
        for (int k = 0; k < kPlayers; k++)
            if (cutting_[t][k] && now - cutBlock_[t][k] > 8 && instrumentPlayer[t].tidalPlaying((uint8_t)k))
                instrumentPlayer[t].tidalStop((uint8_t)k);
    int sounding = 0;
    int oldT = -1, oldK = -1;  // oldest sounding player of any track
    for (int t = 0; t < kTracks; t++)
        for (int k = 0; k < kPlayers; k++)
            if (this->sounding(t, k, now)) {
                sounding++;
                if (oldT < 0 || (int32_t)(started_[t][k] - started_[oldT][oldK]) < 0) oldT = t, oldK = k;
            }
    if ((uint32_t)sounding > vstats.maxSounding) vstats.maxSounding = (uint32_t)sounding;
    vstats.soundingSum += (uint32_t)sounding;
    vstats.picks++;
    // the load lowered the cap below what sounds: fade the oldest (a few per note)
    for (int n = 0; sounding > cap && n < 4; n++) {
        int t, k;
        if (!oldestSounding(now, t, k)) break;
        cut(t, k);
        sounding--;
        vstats.governorCuts++;
    }
    if (!oldestSounding(now, oldT, oldK)) oldT = oldK = -1;

    // this track's sounding players, oldest first; its free one idle longest
    int busy[kPlayers], nb = 0, freeK = -1;
    for (int k = 0; k < kPlayers; k++) {
        if (this->sounding(slot, k, now)) {
            int j = nb++;
            while (j > 0 && (int32_t)(started_[slot][k] - started_[slot][busy[j - 1]]) < 0) {
                busy[j] = busy[j - 1];
                j--;
            }
            busy[j] = k;
        } else if (freeK < 0 || (int32_t)(started_[slot][k] - started_[slot][freeK]) < 0) {
            freeK = k;
        }
    }
    if (voices < 1) voices = 1;
    if (voices > kPlayers) voices = kPlayers;

    if (mtProject.sampleMemory[instr].isStreamed) {  // one stream per track: player 0
        int others = 0, cuts = 0;
        for (int j = nb - 1; j >= 0; j--) {  // newest first: keep voices - 1 others
            if (busy[j] == 0) continue;
            if (others < voices - 1) others++;
            else {
                cut(slot, busy[j]);
                vstats.voicesCuts++;
                cuts++;
            }
        }
        if (this->sounding(slot, 0, now)) {
            vstats.trackSteals++;  // retrigger: no new voice
        } else if (sounding - cuts >= cap && oldT >= 0 && !cutting_[oldT][oldK]) {
            cut(oldT, oldK);  // a new voice at the cap: free the oldest anywhere (v8 skipped this)
            vstats.globalSteals++;
        }
        return 0;
    }
    // `voices`: at most voices - 1 others keep ringing
    if (nb > voices - 1) {
        const int evict = nb - (voices - 1);
        for (int j = 1; j < evict; j++) {
            cut(slot, busy[j]);
            vstats.voicesCuts++;
        }
        vstats.trackSteals++;
        return busy[0];  // retrigger the oldest with this note
    }
    if (sounding < cap) return freeK;
    if (nb > 0) {
        vstats.trackSteals++;
        return busy[0];
    }
    // this track is silent and the cap is reached: free the oldest anywhere
    if (oldT >= 0) {
        cut(oldT, oldK);
        vstats.globalSteals++;
    }
    return freeK;
}

// The Tracker's one reverb and delay (batch 3: size / delaytime /
// delayfeedback): set when a note asks for a value other than the last one.
// Interrupt-safe setters (a value under AudioNoInterrupts); the project's
// own settings (Master screen) aren't changed.
static void globalFx(const tc::Entry& e) {
    static int32_t lastSize = -1, lastTime = -1, lastFb = -1;
    for (int i = 0; i < e.nmods; i++) {
        const int32_t v = e.modVal[i];
        switch ((tc::TMod)e.modId[i]) {
            case tc::TMod::RevSize:
                if (v != lastSize) engine.setReverbSize((float)v / 65535.0f), lastSize = v;
                break;
            case tc::TMod::DelayTime:
                if (v != lastTime) engine.setDelayTime((uint16_t)v), lastTime = v;
                break;
            case tc::TMod::DelayFb:
                if (v != lastFb) engine.setDelayFeedback((uint8_t)v), lastFb = v;
                break;
            default: break;
        }
    }
}

uint16_t DeviceVoice::noteOn(int slot, const tc::Entry& e, uint32_t late) {
    if (slot < audioEngine::kVoices) {
        globalFx(e);
        const uint32_t now = e.block + late;
        const int k = pickPlayer(slot, e.chan, e.voices ? e.voices : 1, now);
        const uint16_t serial = (uint16_t)((serial_[slot][k] + 1) & 0x3FFF);
        serial_[slot][k] = serial;
        started_[slot][k] = ++clock_;
        startBlock_[slot][k] = now;
        cutting_[slot][k] = false;
        if (!instrumentPlayer[slot].tidalNoteOn((uint8_t)k, e.chan, (int8_t)e.note, e.vel == 255 ? -1 : (int8_t)e.vel,
                                                e.modId, e.modVal, e.nmods))
            serial_[slot][k] = (uint16_t)((serial + 1) & 0x3FFF);  // nothing started: its noteOff is stale
        return (uint16_t)(k | (serial << 2));
    }
    const uint8_t ch = (uint8_t)(e.chan + 1);  // firmware channels are 1-based
    for (int i = 0; i < e.nmods; i++) {
        if (e.modId[i] == (uint8_t)tc::TMod::Cc) sendCC(e.modVal[i] >> 8, e.modVal[i] & 0x7F, ch, false);
        else if (e.modId[i] == (uint8_t)tc::TMod::Bend) sendPitchbend((int16_t)(e.modVal[i] - 8192), ch, false);
        else if (e.modId[i] == (uint8_t)tc::TMod::Program) sendProgramChange((uint8_t)e.modVal[i], ch, false);
        else if (e.modId[i] == (uint8_t)tc::TMod::Touch) sendAfterTouch((uint8_t)e.modVal[i], ch, false);
    }
    if (e.kind == tc::EntryKind::Control) return 0;  // `ccv ... # ccn ...` with no n / note: just the messages
    sendMidiNoteOn(e.note, e.vel, ch, false);
    return 0;
}

void DeviceVoice::noteOff(int slot, uint8_t chan, uint8_t note, uint8_t, uint16_t tag) {
    if (slot < audioEngine::kVoices) {
        const int k = tag & 3;
        if (k < kPlayers && (tag >> 2) == serial_[slot][k]) instrumentPlayer[slot].tidalNoteOff((uint8_t)k);
        else vstats.staleOffs++;
        return;
    }
    sendMidiNoteOff(note, 0, (uint8_t)(chan + 1), false);
}

void DeviceVoice::allOff() {
    for (int i = 0; i < audioEngine::kVoices; i++) instrumentPlayer[i].tidalAllOff();
    for (uint8_t ch = 1; ch <= 16; ch++) sendCC(kMidiAllNotesOff, 0, ch, false);
}
#endif
