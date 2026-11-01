#pragma once

#include "config.h"

struct AudioCS
{
  public:
    AudioCS()
    {
        if (enterCount == 0)
        {
            // NOLINTNEXTLINE(hicpp-no-assembler)
            AUDIO_CS_BEGIN();
        }
        enterCount++;
        active = true;
    }
    ~AudioCS() { exit(); }
    AudioCS(const AudioCS&) = delete;
    AudioCS& operator=(const AudioCS&) = delete;
    AudioCS(AudioCS&&) = delete;
    AudioCS& operator=(AudioCS&&) = delete;

    void exit()
    {
        if (active)
        {
            enterCount--;
            if (enterCount == 0)
            {
                // NOLINTNEXTLINE(hicpp-no-assembler)
                AUDIO_CS_END();
            }
            active = false;
        }
    }

  private:
    // https://bugs.llvm.org/show_bug.cgi?id=48040
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    static int enterCount;

    bool active;
};
