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

void DeviceVoice::noteOn(int slot, const tc::Entry& e, uint32_t) {
    if (slot < audioEngine::kVoices) {
        instrumentPlayer[slot].tidalNoteOn(e.chan, (int8_t)e.note, e.vel == 255 ? -1 : (int8_t)e.vel, e.modId,
                                           e.modVal, e.nmods);
        return;
    }
    const uint8_t ch = (uint8_t)(e.chan + 1);  // firmware channels are 1-based
    for (int i = 0; i < e.nmods; i++) {
        if (e.modId[i] == (uint8_t)tc::TMod::Cc) sendCC(e.modVal[i] >> 8, e.modVal[i] & 0x7F, ch, false);
        else if (e.modId[i] == (uint8_t)tc::TMod::Bend) sendPitchbend((int16_t)(e.modVal[i] - 8192), ch, false);
    }
    sendMidiNoteOn(e.note, e.vel, ch, false);
}

void DeviceVoice::noteOff(int slot, uint8_t chan, uint8_t note, uint8_t) {
    if (slot < audioEngine::kVoices) instrumentPlayer[slot].noteOff();
    else sendMidiNoteOff(note, 0, (uint8_t)(chan + 1), false);
}

void DeviceVoice::allOff() {
    for (int i = 0; i < audioEngine::kVoices; i++) instrumentPlayer[i].noteOff();
    for (uint8_t ch = 1; ch <= 16; ch++) sendCC(kMidiAllNotesOff, 0, ch, false);
}
#endif
