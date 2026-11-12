// Tracker Cycles: sums a track's sample players (polyphony, docs/tidal_voices.md)
// into the track's one crusher / filter / amp chain, with a gain per player
// (the note's volume). Cheaper than AudioMixer for the usual cases: no input
// -> no output (silence, the AudioStream convention); one input at unity gain
// -> passed through untouched; otherwise sum of input x gain, saturated.
#pragma once

#include "AudioStream.h"
#include "utility/dspinst.h"

template<int N>
class AudioSumPlayers : public AudioStream
{
  public:
    static constexpr int32_t kUnity = 65536;  // Q16

    AudioSumPlayers()
      : AudioStream(N, inputQueueArray)
    {
        for (int i = 0; i < N; i++)
            gain_[i] = kUnity;
    }

    // gain of input i (0..~16); takes effect from the next block
    void gain(int i, float g)
    {
        if (i < 0 || i >= N)
            return;
        if (g < 0.0f)
            g = 0.0f;
        if (g > 16.0f)  // the volume curve's top is ~15.8
            g = 16.0f;
        gain_[i] = (int32_t)(g * kUnity);
    }

    virtual void update(void)
    {
        audio_block_t* in[N];
        int present = 0, last = -1;
        for (int i = 0; i < N; i++)
        {
            in[i] = receiveReadOnly(i);
            if (in[i])
            {
                present++;
                last = i;
            }
        }
        if (!present)
            return;
        if (present == 1 && gain_[last] == kUnity)
        {
            transmit(in[last]);
            release(in[last]);
            return;
        }
        audio_block_t* out = allocate();
        if (out)
        {
            int32_t acc[AUDIO_BLOCK_SAMPLES];
            bool first = true;
            for (int i = 0; i < N; i++)
            {
                if (!in[i])
                    continue;
                const uint32_t* s = (const uint32_t*)in[i]->data;
                const int32_t g = gain_[i];
                if (first)
                {
                    for (int k = 0; k < AUDIO_BLOCK_SAMPLES / 2; k++)
                    {
                        const uint32_t v = s[k];
                        acc[2 * k] = signed_multiply_32x16b(g, v);
                        acc[2 * k + 1] = signed_multiply_32x16t(g, v);
                    }
                    first = false;
                }
                else
                {
                    for (int k = 0; k < AUDIO_BLOCK_SAMPLES / 2; k++)
                    {
                        const uint32_t v = s[k];
                        acc[2 * k] += signed_multiply_32x16b(g, v);
                        acc[2 * k + 1] += signed_multiply_32x16t(g, v);
                    }
                }
            }
            int16_t* d = out->data;
            for (int k = 0; k < AUDIO_BLOCK_SAMPLES; k++)
                d[k] = (int16_t)signed_saturate_rshift(acc[k], 16, 0);
            transmit(out);
            release(out);
        }
        for (int i = 0; i < N; i++)
            if (in[i])
                release(in[i]);
    }

  private:
    int32_t gain_[N];
    audio_block_t* inputQueueArray[N];
};
