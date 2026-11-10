// See tidal_device.h.
#ifdef TIDAL_TICK
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

int DeviceVoice::pickPlayer(int slot, uint8_t instr) {
    if (mtProject.sampleMemory[instr].isStreamed) {  // one stream per track: player 0
        if (instrumentPlayer[slot].tidalPlaying(0)) vstats.trackSteals++;
        return 0;
    }
    int sounding = 0;
    int oldT = -1, oldK = -1;  // oldest sounding player of any track
    for (int t = 0; t < kTracks; t++)
        for (int k = 0; k < kPlayers; k++)
            if (instrumentPlayer[t].tidalPlaying(k)) {
                sounding++;
                if (oldT < 0 || (int32_t)(started_[t][k] - started_[oldT][oldK]) < 0) oldT = t, oldK = k;
            }
    if ((uint32_t)sounding > vstats.maxSounding) vstats.maxSounding = (uint32_t)sounding;
    vstats.soundingSum += (uint32_t)sounding;
    vstats.picks++;
    int freeK = -1, busyK = -1;  // free: idle longest; busy: oldest sounding
    for (int k = 0; k < kPlayers; k++) {
        if (instrumentPlayer[slot].tidalPlaying(k)) {
            if (busyK < 0 || (int32_t)(started_[slot][k] - started_[slot][busyK]) < 0) busyK = k;
        } else if (freeK < 0 || (int32_t)(started_[slot][k] - started_[slot][freeK]) < 0) {
            freeK = k;
        }
    }
    if (freeK >= 0 && sounding < kCap) return freeK;
    if (busyK >= 0) {
        vstats.trackSteals++;
        return busyK;
    }
    // this track is silent and the cap is reached: free the oldest anywhere
    if (oldT >= 0) {
        instrumentPlayer[oldT].tidalStop((uint8_t)oldK);
        serial_[oldT][oldK]++;  // its pending noteOff is now stale
        vstats.globalSteals++;
    }
    return freeK;
}

uint16_t DeviceVoice::noteOn(int slot, const tc::Entry& e, uint32_t) {
    if (slot < audioEngine::kVoices) {
        const int k = pickPlayer(slot, e.chan);
        const uint16_t serial = (uint16_t)((serial_[slot][k] + 1) & 0x3FFF);
        serial_[slot][k] = serial;
        started_[slot][k] = ++clock_;
        if (!instrumentPlayer[slot].tidalNoteOn((uint8_t)k, e.chan, (int8_t)e.note, e.vel == 255 ? -1 : (int8_t)e.vel,
                                                e.modId, e.modVal, e.nmods))
            serial_[slot][k] = (uint16_t)((serial + 1) & 0x3FFF);  // nothing started: its noteOff is stale
        return (uint16_t)(k | (serial << 2));
    }
    const uint8_t ch = (uint8_t)(e.chan + 1);  // firmware channels are 1-based
    for (int i = 0; i < e.nmods; i++) {
        if (e.modId[i] == (uint8_t)tc::TMod::Cc) sendCC(e.modVal[i] >> 8, e.modVal[i] & 0x7F, ch, false);
        else if (e.modId[i] == (uint8_t)tc::TMod::Bend) sendPitchbend((int16_t)(e.modVal[i] - 8192), ch, false);
    }
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
