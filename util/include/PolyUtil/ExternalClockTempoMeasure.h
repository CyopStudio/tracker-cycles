#pragma once

#include <cmath>
#include <ranges>

#include "PolyUtil/misc.h"

template<int MaxMeasureCount, int BufferSize, int SampleRate, int kMidiPpqn>
struct ExternalClockTempoMeasure
{
    ExternalClockTempoMeasure() { std::ranges::fill(updateCounters, 0); }
    void onUpdate()
    {
        if (updatesPerPulse >= kMaxUpdatesPerPulse)
        {
            tempoUnknown = true;
            return;
        }
        updatesPerPulse++;
    }
    [[nodiscard]] tiny_optional<float> getIndicatedTempo() const
    {
        if (tempoUnknown)
        {
            return {};
        }
        if (updatesSum == 0)
        {
            return {};
        }
        auto tempo = 60.0f / ((kUpdatePeriod * static_cast<float>(updatesSum * kMidiPpqn)) /
                              static_cast<float>(MaxMeasureCount));
        return std::roundf(tempo);
    }
    void onExternalPulse()
    {
        if (tempoUnknown)
        {
            std::ranges::fill(updateCounters, 0);
            updatesSum = 0;
            updatesPerPulse = 0;
            tempoUnknown = false;
        }

        updatesSum -= updateCounters[lastPulseIndex];
        updatesSum += updatesPerPulse;
        updateCounters[lastPulseIndex] = updatesPerPulse;

        lastPulseIndex++;
        lastPulseIndex %= MaxMeasureCount;

        updatesPerPulse = 0;
    }

  private:
    static constexpr float kUpdatePeriod =
      static_cast<float>(BufferSize) / static_cast<float>(SampleRate);
    static constexpr int kMaxUpdatesPerPulse = 180;
    bool tempoUnknown = false;
    std::array<uint8_t, MaxMeasureCount> updateCounters{};
    int lastPulseIndex = 0;
    int updatesSum = 0;
    int updatesPerPulse = 0;
};
