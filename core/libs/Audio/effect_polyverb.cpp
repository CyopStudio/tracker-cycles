// Copyright 2020 Wojciech Jakóbczyk (jakobczyk.woj@gmail.com)

#include "effect_polyverb.h"

#include <string.h>
#include "cr_mtb_buffer.h"

extern uint16_t externalRamReverbBuff[];
#ifdef PLATFORM_IMX
static uint16_t internalRamReverbBuff[16384] __attribute__((section(".noinit")));
#else
static __NOINIT(EXTERNAL_RAM) uint16_t internalRamReverbBuff[16384];
#endif

AudioEffectPolyverb::AudioEffectPolyverb()
  : AudioStream(2, inputQueueArray)
{
    buffer = internalRamReverbBuff;
    buffer2 = externalRamReverbBuff;
    reverb.Init(buffer, buffer2);

    reverb.SetInputGain(0.1f);
    reverb.SetDiffusion(0.625f);
    setTime(0.7f);
    setDamp(0.2f);
    setAmount(0.8f);
    blockUpdate();
}

AudioEffectPolyverb::~AudioEffectPolyverb()
{
}

AUDIO_CORE_RAM void AudioEffectPolyverb::update(void)
{
    if (isBlockedUpdate)
        return;

    audio_block_t* block_left;
    audio_block_t* block_right;

    block_left = receiveWritable(0);
    block_right = receiveWritable(1);
    // Tracker Cycles audio floor: no input -> run on silence only while the
    // tail is audible (reverb.IsActive(): average wet level above ~-76 dBFS),
    // then skip (no output = silence downstream)
    if (block_left == NULL && block_right == NULL && !reverb.IsActive())
        return;
    if (block_left == NULL)
    {
        block_left = allocate();
        if (block_left)
            memset(block_left->data, 0, sizeof block_left->data);
    }
    if (block_right == NULL)
    {
        block_right = allocate();
        if (block_right)
            memset(block_right->data, 0, sizeof block_right->data);
    }
    if (block_left == NULL || block_right == NULL)
    {
        if (block_left)
            release(block_left);
        if (block_right)
            release(block_right);
        return;
    }

    reverb.Process(block_left->data, block_right->data, AUDIO_BLOCK_SAMPLES);

    transmit(block_left, 0);
    transmit(block_right, 1);

    release(block_left);
    release(block_right);
}

AUDIO_CORE_FLASH void AudioEffectPolyverb::reset(void)
{
    isBlockedUpdate = true;
    reverb.Reset();
    isBlockedUpdate = false;
}

AUDIO_CORE_FLASH bool AudioEffectPolyverb::isActive(void)
{
    return reverb.IsActive();
}

AUDIO_CORE_FLASH void AudioEffectPolyverb::setActiveThreshold(float threshold)
{
    reverb.SetActiveThreshold(threshold);
}
