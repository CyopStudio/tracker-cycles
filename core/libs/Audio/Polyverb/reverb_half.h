// Tracker Cycles: Polyverb's reverb (reverb.h, MIT: Emilie Gillet, adapted
// by Wojciech Jakobczyk) run at half the sample rate, for the Original
// Tracker (CLAUDE.md, reverb bench 2026-11-10: stock 17.1 % of the audio
// block, half rate 10.4 %; its delay memory is uncached SDRAM there).
//
// Same topology and API as Reverb. Per block: the summed input is
// decimated by 2 (pair average), every delay line is half as long (same
// reverb times, half the memory: 8192 samples per buffer), the damping
// one-poles get the coefficient that decays the same in real time, the
// LFO runs twice as fast per sample; the wet signal is upsampled
// linearly and mixed with the dry signal at the full rate, as Reverb does.

#ifndef POLYVERB_REVERB_HALF_H_
#define POLYVERB_REVERB_HALF_H_

#include <math.h>

#include "fx_engine.h"
#include "util.h"

class ReverbHalf
{
  public:
    ReverbHalf() {}
    ~ReverbHalf() {}

    // buffer, buffer2: at least kBufferSamples samples each
    static constexpr size_t kBufferSamples = 8192;

    void Init(uint16_t* buffer, uint16_t* buffer2)
    {
        engine_.Init(buffer, buffer2);
        engine_.SetLFOFrequency(LFO_1, 0.5f * 2.0f / 32000.0f);
        engine_.SetLFOFrequency(LFO_2, 0.3f * 2.0f / 32000.0f);
        SetLP(0.7f);
        diffusion_ = 0.625f;
        predelay_length_ = 0;
        lp_decay_0_ = lp_decay_1_ = lp_decay_2_ = 0;
        prevL_ = prevR_ = 0;
    }

    inline void Process(int16_t* left, int16_t* right, size_t size)
    {
        typedef E::Reserve<
          75,
          E::Reserve<
            107,
            E::Reserve<
              160,
              E::Reserve<264,
                         E::Reserve<1091, E::Reserve<1345, E::Reserve<2251, E::Reserve<kPredelayMax>>>>>>>>
          Memory;
        typedef E::Reserve<1263, E::Reserve<1099, E::Reserve<3156>>> Memory2;
        E::DelayLine<Memory, 0> ap1;
        E::DelayLine<Memory, 1> ap2;
        E::DelayLine<Memory, 2> ap3;
        E::DelayLine<Memory, 3> ap4;
        E::DelayLine<Memory, 4> dap1a;
        E::DelayLine<Memory, 5> dap1b;
        E::DelayLine<Memory, 6> del1;
        E::DelayLine<Memory, 7> del0;
        E::DelayLine<Memory2, 0> dap2a;
        E::DelayLine<Memory2, 1> dap2b;
        E::DelayLine<Memory2, 2> del2;
        E::Context c;

        const float kap = diffusion_;
        const float klp = lp_;
        const float krt = reverb_time_;
        const float amount = amount_;
        const float gain = input_gain_ / (32768.0f * 2.0f);
        float lp_0 = lp_decay_0_, lp_1 = lp_decay_1_, lp_2 = lp_decay_2_;

        const int m = (int)(size / 2);
        float wl[kMaxHalf], wr[kMaxHalf];
        for (int j = 0; j < m && j < kMaxHalf; j++)
        {
            float wet, apout = 0.0f;
            engine_.Start(&c);
            c.Load(gain * ((float)left[2 * j] + (float)right[2 * j] + (float)left[2 * j + 1] +
                           (float)right[2 * j + 1]));
            c.Write(del0, 0.0f);
            c.Read(del0, predelay_length_, 1.0f);
            c.Lp(lp_0, klp);
            c.Read(ap1 TAIL, kap);
            c.WriteAllPass(ap1, -kap);
            c.Read(ap2 TAIL, kap);
            c.WriteAllPass(ap2, -kap);
            c.Read(ap3 TAIL, kap);
            c.WriteAllPass(ap3, -kap);
            c.Read(ap4 TAIL, kap);
            c.WriteAllPass(ap4, -kap);
            c.Write(apout);

            c.Load(apout);
            c.Interpolate2(del2, 3105.5f, LFO_2, 50.0f, krt);
            c.Lp(lp_1, klp);
            c.Read(dap1a TAIL, -kap);
            c.WriteAllPass(dap1a, kap);
            c.Read(dap1b TAIL, kap);
            c.WriteAllPass(dap1b, -kap);
            c.Write(del1, 2.0f);
            c.Write(wet, 0.0f);
            wl[j] = wet * 32768.0f;

            c.Load(apout);
            c.Read(del1 TAIL, krt);
            c.Lp(lp_2, klp);
            c.Read2(dap2a TAIL, kap);
            c.WriteAllPass2(dap2a, -kap);
            c.Read2(dap2b TAIL, -kap);
            c.WriteAllPass2(dap2b, kap);
            c.Write2(del2, 2.0f);
            c.Write(wet, 0.0f);
            wr[j] = wet * 32768.0f;
        }
        lp_decay_0_ = lp_0;
        lp_decay_1_ = lp_1;
        lp_decay_2_ = lp_2;

        // linear upsampling (even samples between the last value and this
        // one, odd on it) and the dry / wet mix
        float delta_sum = 0.0f, pl = prevL_, pr = prevR_;
        for (int j = 0; j < m && j < kMaxHalf; j++)
        {
            for (int k = 0; k < 2; k++)
            {
                const float a = k ? wl[j] : 0.5f * (pl + wl[j]);
                const float b = k ? wr[j] : 0.5f * (pr + wr[j]);
                int16_t& l = left[2 * j + k];
                int16_t& r = right[2 * j + k];
                float d = (a - l) * amount;
                delta_sum += fabsf(d);
                l = (int16_t)Clip16((int32_t)(l + d));
                d = (b - r) * amount;
                delta_sum += fabsf(d);
                r = (int16_t)Clip16((int32_t)(r + d));
            }
            pl = wl[j];
            pr = wr[j];
        }
        prevL_ = pl;
        prevR_ = pr;
        last_update_delta_ = size ? delta_sum / size : 0.0f;
    }

    inline void SetAmount(float amount) { amount_ = amount; }
    inline void SetInputGain(float input_gain) { input_gain_ = input_gain; }
    inline void SetTime(float reverb_time) { reverb_time_ = reverb_time; }
    inline void SetDiffusion(float diffusion) { diffusion_ = diffusion; }
    // one-pole coefficient given for the full rate; at half rate the pole
    // is squared (same decay per second): k' = 1 - (1 - k)^2
    inline void SetLP(float lp)
    {
        const float p = 1.0f - lp;
        lp_ = 1.0f - p * p;
    }
    inline void SetPredelayLength(float length) { predelay_length_ = (int)(length * kPredelayMax); }

    inline void Reset()
    {
        engine_.Reset();
        lp_decay_0_ = lp_decay_1_ = lp_decay_2_ = 0;
        prevL_ = prevR_ = 0;
        last_update_delta_ = sound_delta_threshold_;
    }

    inline bool IsActive() { return last_update_delta_ > sound_delta_threshold_; }
    inline void SetActiveThreshold(float threshold) { sound_delta_threshold_ = threshold; }

  private:
    typedef FxEngine<kBufferSamples, FORMAT_16_BIT> E;
    static constexpr int kPredelayMax = 2500;  // Reverb's 5000 at the full rate
    static constexpr int kMaxHalf = 64;        // AUDIO_BLOCK_SAMPLES / 2
    E engine_;

    float amount_ = 0;
    float input_gain_ = 0;
    float reverb_time_ = 0;
    float diffusion_ = 0;
    float lp_ = 0;
    float lp_decay_0_ = 0, lp_decay_1_ = 0, lp_decay_2_ = 0;
    float prevL_ = 0, prevR_ = 0;
    int predelay_length_ = 0;
    float last_update_delta_ = 0;
    float sound_delta_threshold_ = 5.0f;

    DISALLOW_COPY_AND_ASSIGN(ReverbHalf);
};

#endif  // POLYVERB_REVERB_HALF_H_
