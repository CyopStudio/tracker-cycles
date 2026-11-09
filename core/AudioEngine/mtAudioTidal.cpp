// Tracker Cycles: playerEngine::tidalNoteOn (plan 4c step 3, design
// docs/tidal_control_mapping.md). Same steps as the sequencer's noteOn
// (mtAudioNoteOnHandle.cpp), but the per-note overrides come from a Tidal
// event (already converted to Tracker units on the main loop by
// tidal/core/mapping.*) instead of two 8-bit step FX. They use the
// player's own step-FX state (lane 0: trackControlParameter[sequencerMode]
// flags + currentSeqModValues) and setters, so update()'s refresh,
// envelopes, LFOs and performance mode treat them like step FX. Every note
// starts from the instrument: overrides the previous Tidal note set and
// this one doesn't have are ended with the stock endFx*() calls.
#ifdef TIDAL_TICK
#include "mtAudioEngine.h"
#include "mtSequencer.h"
#include "mtStructs.h"
#include "../../tidal/core/mapping.h"

using namespace mtAudioEngineConstans;

namespace {

using tc::TMod;
constexpr uint32_t bit(TMod m) { return 1u << (uint32_t)m; }
constexpr uint32_t kFilterBits = bit(TMod::CutoffLP) | bit(TMod::CutoffHP) | bit(TMod::CutoffBP);
constexpr uint32_t kPointBits = bit(TMod::Begin) | bit(TMod::End);
constexpr uint32_t kEnvBits = bit(TMod::Attack) | bit(TMod::Release);
static_assert((int)TMod::Count <= 32, "TMod bits must fit 32 bits");

}  // namespace

AUDIO_CORE_RAM uint8_t playerEngine::tidalNoteOn(uint8_t instr_idx,
                                                 int8_t note,
                                                 int8_t velocity,
                                                 const uint8_t* modId,
                                                 const uint16_t* modVal,
                                                 uint8_t nmods)
{
    if (mtProject.instrument[instr_idx].isActive != 1)
        return 0;
    if (isBlockedNoteOn[instr_idx])
        return 0;

    uint32_t has = 0;
    uint16_t v[(int)TMod::Count] = { 0 };
    for (uint8_t i = 0; i < nmods; i++)
    {
        if (modId[i] > 0 && modId[i] < (uint8_t)TMod::Count)
        {
            has |= 1u << modId[i];
            v[modId[i]] = modVal[i];
        }
    }
    auto val = [&](TMod m) { return v[(int)m]; };

    AudioNoInterrupts();

    currentInstrument_idx = instr_idx;
    currentNote = note;
    stepVelocity = velocity;
    for (int channel = 0; channel < (int)playMemPtr.size(); channel++)
    {
        playMemPtr[channel]->setCurrentInstrIdx(instr_idx);
    }

    // step FX the stock sequencer left on this voice
    for (uint8_t lane = 0; lane < 2; lane++)
    {
        if (lastSeqFx[lane])
        {
            endFx(lastSeqFx[lane], lane);
            lastSeqFx[lane] = 0;
            lastSeqVal[lane] = 0;
        }
    }

    uint8_t* seq = trackControlParameter[(int)controlType::sequencerMode];
    const strInstrument& ins = mtProject.instrument[instr_idx];

    // amp envelope: read by the envelope init below
    if (has & kEnvBits)
    {
        currentSeqModValues.ampAttack =
          (has & bit(TMod::Attack)) ? val(TMod::Attack) : ins.envelope[envAmp].attack;
        currentSeqModValues.ampRelease =
          (has & bit(TMod::Release)) ? val(TMod::Release) : ins.envelope[envAmp].release;
        seq[(int)parameterList::ampAttack] = 1;
        seq[(int)parameterList::ampRelease] = 1;
    }
    else
    {
        seq[(int)parameterList::ampAttack] = 0;
        seq[(int)parameterList::ampRelease] = 0;
    }

    // as the sequencer's noteOn
    handleInitFxNoteOnAllEnvelopes();
    handleFxNoteOnFilter();
    handleFxNoteOnGain();
    handleFxNoteOnPanning();
    handleFxNoteOnBitDepth();
    handleFxNoteOnDelaySend();
    handleFxNoteOnReverbSend();

    if (trackControlParameter[(int)controlType::performanceMode][(int)parameterList::endPoint])
    {
        changeEndPointPerformanceMode(std::roundf(mapClamped<float>(
          (float)performanceMod.endPoint, (float)-MAX_16BIT, (float)MAX_16BIT, -100.0f, 100.0f)));
    }
    if (trackControlParameter[(int)controlType::performanceMode][(int)parameterList::startPoint])
    {
        changeStartPointPerformanceMode(performanceMod.startPoint);
    }
    if (trackControlParameter[(int)controlType::performanceMode][(int)parameterList::wavetablePosition])
    {
        changeWavetableWindowPerformanceMode(performanceMod.wavetablePosition);
    }
    if (trackControlParameter[(int)controlType::performanceMode][(int)parameterList::tune])
    {
        changeTunePerformanceMode(performanceMod.tune);
    }

    // end what the previous Tidal note set and this one doesn't
    auto ends = [&](uint32_t mask) { return (tidalMods & mask) && !(has & mask); };
    if (ends(kFilterBits)) endFxFilter(0);
    if (ends(bit(TMod::Pan))) endFxPanning(0);
    if (ends(bit(TMod::BitDepth))) endFxBitDepth(0);
    if (ends(bit(TMod::Delay))) endFxDelaySend(0);
    if (ends(bit(TMod::Reverb))) endFxReverbSend(0);
    if (ends(bit(TMod::Overdrive))) endFxOverdrive(0);
    if (ends(bit(TMod::Reverse))) endFxReversePlayback(0);
    if (ends(bit(TMod::FineTune))) endFxFinetune(0);
    if (ends(bit(TMod::Slice))) endFxSlice(0);
    if (ends(kPointBits)) endFxPositionStartPoint(0);

    // this note's overrides
    if (has & kFilterBits)
    {
        const TMod f = (has & bit(TMod::CutoffHP)) ? TMod::CutoffHP
                       : (has & bit(TMod::CutoffBP)) ? TMod::CutoffBP
                                                     : TMod::CutoffLP;
        seq[(int)parameterList::filterCutoff] = 1;
        seq[(int)parameterList::filterEnable] = 1;
        seq[(int)parameterList::filterType] = 1;
        currentSeqModValues.filterCutoff = val(f) / 65535.0f;
        currentSeqModValues.filterType =
          f == TMod::CutoffHP ? highPass : f == TMod::CutoffBP ? bandPass : lowPass;
        currentSeqModValues.filterEnable = 1;
        setFxCutoff();
        setFxFilterType();
        // resonance has no step FX: set it on the filter (the instrument's
        // when the event has none, as an earlier note may have changed it)
        const float res = (has & bit(TMod::Resonance))
                            ? val(TMod::Resonance) / 65535.0f * RESONANCE_MAX
                            : ins.resonance;
        for (int channel = 0; channel < (int)filterPtr.size(); channel++)
        {
            filterPtr[channel]->resonance(res + RESONANCE_OFFSET);
        }
    }
    if (has & bit(TMod::Pan))
    {
        seq[(int)parameterList::panning] = 1;
        currentSeqModValues.panning = (int16_t)clamp<int>(val(TMod::Pan), PANNING_MIN, PANNING_MAX);
        setRefreshBit(PANNING_MASK);
    }
    if (has & bit(TMod::BitDepth))
    {
        seq[(int)parameterList::bitDepth] = 1;
        currentSeqModValues.bitDepth = (uint8_t)clamp<int>(val(TMod::BitDepth), BIT_DEPTH_MIN, BIT_DEPTH_MAX);
        setFxBitDepth();
    }
    if (has & bit(TMod::Delay))
    {
        seq[(int)parameterList::delaySend] = 1;
        currentSeqModValues.delaySend = (uint8_t)clamp<int>(val(TMod::Delay), SEND_MIN, SEND_MAX);
        setRefreshBit(DELAY_SEND_MASK);
    }
    if (has & bit(TMod::Reverb))
    {
        seq[(int)parameterList::reverbSend] = 1;
        currentSeqModValues.reverbSend = (uint8_t)clamp<int>(val(TMod::Reverb), SEND_MIN, SEND_MAX);
        setRefreshBit(REVERB_SEND_MASK);
    }
    if (has & bit(TMod::Overdrive))
    {
        seq[(int)parameterList::overdrive] = 1;
        currentSeqModValues.overdrive = (uint8_t)clamp<int>(val(TMod::Overdrive), OVERDRIVE_MIN, OVERDRIVE_MAX);
        setFxOverdrive();
    }
    if (has & bit(TMod::Reverse))
    {
        seq[(int)parameterList::samplePlaybeckDirection] = 1;
        currentSeqModValues.reversePlayback = 1;
        setFxReverse();
    }
    if (has & bit(TMod::FineTune))
    {
        seq[(int)parameterList::fineTune] = 1;
        currentSeqModValues.fineTune = (int8_t)clamp<int>((int16_t)val(TMod::FineTune),
                                                          MIN_INSTRUMENT_FINETUNE,
                                                          MAX_INSTRUMENT_FINETUNE);
        setFxFinetune();
    }
    if ((has & bit(TMod::Slice)) && ins.sliceNumber > 0)
    {
        seq[(int)parameterList::slice] = 1;
        currentSeqModValues.slice = (uint8_t)(val(TMod::Slice) % ins.sliceNumber);
        setFxSlice();
    }
    if (has & kPointBits)
    {
        seq[(int)parameterList::startPoint] = 1;
        modSeqPoints((has & bit(TMod::Begin)) ? val(TMod::Begin) : SKIP_MODIFICATION_THIS_VALUE,
                     (has & bit(TMod::End)) ? val(TMod::End) : SKIP_MODIFICATION_THIS_VALUE,
                     0);
    }
    tidalMods = has;

    // start, as the sequencer's noteOn
    bool isAmpRandom =
      (ins.envelope[envAmp].loop == 1) && (ins.lfo[envAmp].shape == lfoShapeRandom);
    for (int channel = 0; channel < (int)playMemPtr.size(); channel++)
    {
        playMemPtr[channel]->envelopeSetIsRandom(isAmpRandom);
        playMemPtr[channel]->envelopeNoteOn(instr_idx, note);
    }
    for (uint8_t i = envPan; i < ACTIVE_ENVELOPES; i++)
    {
        if (isActiveEnvelope(i))
        {
            bool isRandom = (ins.envelope[i].loop == 1) && (ins.lfo[i].shape == lfoShapeRandom);
            envelopePtr[i]->setIsRandom(isRandom);
            if (i != envCutoff ||
                (!trackControlParameter[(int)controlType::sequencerMode][(int)parameterList::filterAttack] &&
                 !trackControlParameter[(int)controlType::sequencerMode2][(int)parameterList::filterAttack]))
            {
                envelopePtr[i]->setWavetableParams(ins.lfo[i].shape, ins.lfo[i].wtBend, ins.lfo[i].wtSust);
            }
            envelopePtr[i]->start();
            setSyncParamsLFO(i);
        }
    }

    if (isTrackDisplayed)
        onEndDisplay = true;
    AudioInterrupts();
    return 1;
}
#endif
