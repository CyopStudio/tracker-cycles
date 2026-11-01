#ifndef SOURCE_PROFILER_H_
#define SOURCE_PROFILER_H_
#include "config.h"
#include "stddef.h"
#include "stdint.h"

class PlatformProfiler
{
  public:
    struct AudioProcessorUsage
    {
        int minimum;
        int maximum;
        int average;
        int last;
    };

    static constexpr int kK66VectorNumber = 116;
    void getAudioProcessingUsageUs(AudioProcessorUsage& usage);
    void getAudioProcessingUsagePercent(AudioProcessorUsage& usage);
    uint32_t getNvicIrqPriorityRegister(int vector);
    const char* getNvicIrqName(int vector);
    void startMeasuringAudio();
    void stopMeasuringAudio();
    void resetMeasuringAudio();
    void listInterruptsPriorities();

  private:
    static constexpr float kSamplePeriodUs = (1.0f / 44100.0f) * 1000000.0f;
    static constexpr int kMaxAudioUpdateTime = kAudioBlockSamples * kSamplePeriodUs;
    static constexpr int kCoreInterruptsNumber = 16;

    int calculatePercentProcessorUsage(int processorUsageUs);

    uint32_t audioUpdateTimer;
    int lastSampleUs;
    int sumOfSamples;
    int samples;
    int minSampleUs;
    int maxSampleUs;
};

#endif /* SOURCE_PROFILER_H_ */
