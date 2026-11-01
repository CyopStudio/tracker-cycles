#ifndef EFFECTOR_MTSAMPLEEDITORENGINE_H_
#define EFFECTOR_MTSAMPLEEDITORENGINE_H_
#include "mtEffect.h"

enum enSampleEditorEffect
{
    editorEffectNormalizer,
    editorEffectCut,
    editorEffectReverse,
    editorEffectAmplifier,
    editorEffectOverdrive,
    editorEffectDelay,
    editorEffectBitcrusher,
    editorEffectChorus,
    editorEffectFlanger,
    editorEffectEQ,
    editorEffectLimiter,
    editorEffectCompresor,
    editorEffectWavetableSmoother,
    editorEffectTimestretchBeat,
    editorEffectTimestretchNote,
    editorEffectFadeIn,
    editorEffectFadeOut,
    editorEffectModular,

    //**********************//
    editorEffectMax
};

extern mtEffect* sampleEditorEffect[editorEffectMax];

namespace delayParams
{
extern const char* const labelText[2];
extern float upConstrain[2];
extern float downConstrain[2];
extern float changeStep[2];
extern float displayMult[2];
extern const char* const afterValueText[2];
extern const char displayType[2];
}

namespace bitcrusherParams
{
extern const char* const labelText[2];
extern float upConstrain[2];
extern float downConstrain[2];
extern float changeStep[2];
extern float displayMult[2];
extern const char* const afterValueText[2];
extern const char displayType[2];
}
namespace amplifierParams
{
extern const char* const labelText[1];
extern float upConstrain[1];
extern float downConstrain[1];
extern float changeStep[1];
extern float displayMult[1];
extern const char* const afterValueText[1];
extern const char displayType[1];
}

namespace overdriveParams
{
extern const char* const labelText[2];
extern float upConstrain[2];
extern float downConstrain[2];
extern float changeStep[2];
extern float displayMult[2];
extern const char* const afterValueText[2];
extern const char displayType[2];
}

namespace timestretchBeatParams
{
extern const char* const labelText[4];
extern float upConstrain[4];
extern float downConstrain[4];
extern float changeStep[4];
extern float displayMult[4];
extern const char* const afterValueText[4];
extern const char displayType[4];
}

namespace timestretchNoteParams
{
extern const char* const labelText[2];
extern float upConstrain[2];
extern float downConstrain[2];
extern float changeStep[2];
extern float displayMult[2];
extern const char* const afterValueText[2];
extern const char displayType[2];
}

namespace chorusParams
{
extern const char* const labelText[2];
extern float upConstrain[2];
extern float downConstrain[2];
extern float changeStep[2];
extern float displayMult[2];
extern const char* const afterValueText[2];
extern const char displayType[2];
}

namespace flangerParams
{
extern const char* const labelText[4];
extern float upConstrain[4];
extern float downConstrain[4];
extern float changeStep[4];
extern float displayMult[4];
extern const char* const afterValueText[4];
extern const char displayType[4];
}

namespace eqParams
{
extern const char* const labelText[9];
extern float upConstrain[9];
extern float downConstrain[9];
extern float changeStep[9];
extern float displayMult[9];
extern const char* const afterValueText[9];
extern const char displayType[9];
}

namespace limiterParams
{
extern const char* const labelText[3];
extern float upConstrain[3];
extern float downConstrain[3];
extern float changeStep[3];
extern float displayMult[3];
extern const char* const afterValueText[3];
extern const char displayType[3];
}

namespace compressorParams
{
extern const char* const labelText[4];
extern float upConstrain[4];
extern float downConstrain[4];
extern float changeStep[4];
extern float displayMult[4];
extern const char* const afterValueText[4];
extern const char displayType[4];
}

namespace wavetableSmootherParams
{
extern const char* const labelText[2];
extern float upConstrain[2];
extern float downConstrain[2];
extern float changeStep[2];
extern float displayMult[2];
extern const char* const afterValueText[2];
extern const char displayType[2];
}

namespace normalizerParams
{
extern const char* const labelText[1];
extern float upConstrain[1];
extern float downConstrain[1];
extern float changeStep[1];
extern float displayMult[1];
extern const char* const afterValueText[1];
extern const char displayType[1];
}

#endif /* EFFECTOR_MTSAMPLEEDITORENGINE_H_ */
