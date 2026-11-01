#include "mtSampleEditorEngine.h"
#include "mtEffect.h"
#include "mtEffectAmplifier.h"
#include "mtEffectBitcrusher.h"
#include "mtEffectChorus.h"
#include "mtEffectCompresor.h"
#include "mtEffectCut.h"
#include "mtEffectDelay.h"
#include "mtEffectEQ.h"
#include "mtEffectFadeIn.h"
#include "mtEffectFadeOut.h"
#include "mtEffectFlanger.h"
#include "mtEffectLimiter.h"
#include "mtEffectNormalizer.h"
#include "mtEffectOverdrive.h"
#include "mtEffectReverse.h"
#include "mtEffectTimestretch.h"
#include "mtEffectWavetableSmoother.h"
#include "mtEffectModular.h"

mtEffect* sampleEditorEffect[editorEffectMax] = { &sampleEditorNormalizer,
                                                  &sampleEditorCut,
                                                  &sampleEditorReverse,
                                                  &sampleEditorAmplifier,
                                                  &sampleEditorOverdrive,
                                                  &sampleEditorDelay,
                                                  &sampleEditorBitcrusher,
                                                  &sampleEditorChorus,
                                                  &sampleEditorFlanger,
                                                  &sampleEditorEQ,
                                                  &sampleEditorLimiter,
                                                  &sampleEditorCompresor,
                                                  &sampleEditorWavetableSmoother,
                                                  &sampleEditorTimestretchBeat,
                                                  &sampleEditorTimestretchNote,
                                                  &sampleEditorFadeIn,
                                                  &sampleEditorFadeOut,
                                                  &sampleEditorModular };

//******************************DELAY

namespace delayParams
{
const char* const labelText[2] = { (const char* const)"Feedback", (const char* const)"Time" };
float upConstrain[2] = { 0.95f, 5000.0f };
float downConstrain[2] = { 0.01f, 1.0f };
float changeStep[2] = { 0.01f, 5.0f };
float displayMult[2] = { 100.0f, 1.0f };
const char* const afterValueText[2] = { (const char* const)" %", (const char* const)" ms" };
const char displayType[2] = { 'd', 'd' };
}

namespace bitcrusherParams
{
const char* const labelText[2] = { (const char* const)"Bits", (const char* const)"Rate" };
float upConstrain[2] = { 16.0f, 44100.0f };
float downConstrain[2] = { 1.0f, 500.0f };
float changeStep[2] = { 1.0f, 100.0f };
float displayMult[2] = { 1.0f, 1.0f };
const char* const afterValueText[2] = { nullptr, (const char* const)" Hz" };
const char displayType[2] = { 'd', 'd' };
}

namespace amplifierParams
{
const char* const labelText[1] = { (const char* const)"Amplifier" };
float upConstrain[1] = { 5.0f };
float downConstrain[1] = { 0.01f };
float changeStep[1] = { 0.01f };
float displayMult[1] = { 1.0f };
const char* const afterValueText[1] = { nullptr };
const char displayType[1] = { 'f' };
}

namespace overdriveParams
{
const char* const labelText[2] = { (const char* const)"Gain", (const char* const)"Type" };
float upConstrain[2] = { 5.0f, 2.0f };
float downConstrain[2] = { 0.01f, 0.0f };
float changeStep[2] = { 0.01f, 1.0f };
float displayMult[2] = { 1.0f, 1.0f };
const char* const afterValueText[2] = { nullptr, nullptr };
const char displayType[2] = { 'f', 'd' };
}

namespace timestretchBeatParams
{
const char* const labelText[4] = { (const char* const)"Tempo",
                                   (const char* const)"Steps",
                                   (const char* const)"Micro-step",
                                   (const char* const)"Granule" };
float upConstrain[4] = { 800.0f, 64.0f, 1.0f, 10.0f };
float downConstrain[4] = { 40.0f, 1.0f, 0.0f, 1.0f };
float changeStep[4] = { 1.0f, 1.0f, 0.01f, 1.0f };
float displayMult[4] = { 1.0f, 1.0f, 1.0f, 0.25f };
const char* const afterValueText[4] = { " BPM", nullptr, nullptr, nullptr };
const char displayType[4] = { 'd', 'd', 'f', 'f' };
}

namespace timestretchNoteParams
{
const char* const labelText[2] = { (const char* const)"Length", (const char* const)"Granule" };
float upConstrain[2] = { 8.0f, 1000.0f };
float downConstrain[2] = { 0.1f, 10.0f };
float changeStep[2] = { 0.01f, 1.0f };
float displayMult[2] = { 1.0f, 1.0f };
const char* const afterValueText[2] = { " s", " ms" };
const char displayType[2] = { 'f', 'd' };
}

namespace chorusParams
{
const char* const labelText[2] = { (const char* const)"Length", (const char* const)"Voices" };
float upConstrain[2] = { 4096.0f, 8.0f };
float downConstrain[2] = { 128.0f, 1.0f };
float changeStep[2] = { 128.0f, 1.0f };
float displayMult[2] = { 1.0f / 44.1f, 1.0f };
const char* const afterValueText[2] = { " ms", nullptr };
const char displayType[2] = { 'd', 'd' };
}

namespace flangerParams
{
const char* const labelText[4] = { (const char* const)"Length",
                                   (const char* const)"Offset",
                                   (const char* const)"Depth",
                                   (const char* const)"Rate" };
float upConstrain[4] = { 4096.0f, 100.0f, 8.0f, 5.0f };
float downConstrain[4] = { 128.0f, 1.0f, 1.0f, 0.1f };
float changeStep[4] = { 128.0f, 1.0f, 1.0f, 0.1f };
float displayMult[4] = { 1.0f / 44.1f, 1.0f, 1.0f, 1.0f };

const char* const afterValueText[4] = { (const char* const)" ms",
                                        (const char* const)" %",
                                        nullptr,
                                        (const char* const)" Hz" };
const char displayType[4] = { 'd', 'd', 'd', 'f' };
}

namespace eqParams
{
const char* const labelText[9] = { (const char* const)"Low",       (const char* const)"Low freq",
                                   (const char* const)"Low Width", (const char* const)"Mid",
                                   (const char* const)"Mid freq",  (const char* const)"Mid Width",
                                   (const char* const)"High",      (const char* const)"High freq",
                                   (const char* const)"High Width" };
float upConstrain[9] = { 12.0f, 200.0f, 5.0f, 12.0f, 8000.0f, 5.0f, 12.0f, 20000.0f, 5.0f };
float downConstrain[9] = { -12.0f, 10.0f, 0.5f, -12.0f, 100.0f, 0.5f, -12.0f, 8000.0f, 0.5f };
float changeStep[9] = { 0.05f, 1.0f, 0.05f, 0.05f, 10.0f, 0.05f, 0.05f, 100.0f, 0.05f };
float displayMult[9] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };

const char* const afterValueText[9] = { " dB", " Hz", "", " dB", " Hz", "", " dB", " Hz", "" };
const char displayType[9] = { 'f', 'd', 'f', 'f', 'd', 'f', 'f', 'd', 'f' };
}

namespace limiterParams
{
const char* const labelText[3] = { (const char* const)"Threshold",
                                   (const char* const)"Attack",
                                   (const char* const)"Release" };

float upConstrain[3] = { 0.0f, 10.0f, 2000.0f };
float downConstrain[3] = { -60.0f, 0.3f, 1.0f };

float changeStep[3] = { 1, 0.1f, 1 };
float displayMult[3] = { 1, 1, 1 };

const char* const afterValueText[3] = { (const char* const)" dB",
                                        (const char* const)" ms",
                                        (const char* const)" ms" };

const char displayType[3] = { 'd', 'f', 'd' };
}

namespace compressorParams
{
const char* const labelText[4] = {
    (const char* const)"Threshold",
    (const char* const)"Ratio",
    (const char* const)"Attack",
    (const char* const)"Release",
};
float upConstrain[4] = { 0.0f, 50.0f, 100.0f, 2000.0f };
float downConstrain[4] = { -60.0f, 1.0f, 0.3f, 1.0f };

float changeStep[4] = { 1, 0.5f, 0.1f, 1 };
float displayMult[4] = { 1, 1, 1, 1 };

const char* const afterValueText[4] = { (const char* const)" dB",
                                        nullptr,
                                        (const char* const)" ms",
                                        (const char* const)" ms" };

const char displayType[4] = { 'd', 'f', 'f', 'd' };
}

namespace wavetableSmootherParams
{
const char* const labelText[2] = {
    (const char* const)"Window Num",
    (const char* const)"Window Len",
};
float upConstrain[2] = { 256.0f, 2048.0f };
float downConstrain[2] = { 1.0f, 1.0f };
float changeStep[2] = { 1.0f, 1.0f };
float displayMult[2] = { 1.0f, 1.0f };
const char* const afterValueText[2] = { nullptr, nullptr };
const char displayType[2] = { 'd', 'd' };
}

namespace normalizerParams
{
const char* const labelText[1] = { (const char* const)"Level" };
float upConstrain[1] = { 100.0f };
float downConstrain[1] = { 0.0f };
float changeStep[1] = { 1.0f };
float displayMult[1] = { 1.0f };

const char* const afterValueText[1] = {
    "%",
};
const char displayType[1] = { 'd' };
}
