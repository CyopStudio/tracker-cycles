/* Elapsed time types - for easy-to-use measurements of elapsed time
 * http://www.pjrc.com/teensy/
 * Copyright (c) 2017 PJRC.COM, LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef elapsedMillis_h
#define elapsedMillis_h
#ifdef __cplusplus

#include "Platform.h"
#include <vector>

class elapsedMillis
{
  private:
    unsigned long ms;
    static Timer* timer;
    static std::vector<elapsedMillis*> uninitializedObjects;
    static std::vector<unsigned long> startValue;

  public:
    static void init(Timer* timer)
    {
        elapsedMillis::timer = timer;
        for (uint32_t idx = 0; idx < uninitializedObjects.size(); idx++)
        {
            uninitializedObjects[idx]->ms = timer->millis() - startValue[idx];
        }
        uninitializedObjects.clear();
        startValue.clear();
    }

    elapsedMillis(void)
      : elapsedMillis(0)
    {
    }
    elapsedMillis(unsigned long val)
    {
        if (timer != nullptr)
        {
            ms = timer->millis() - val;
        }
        else
        {
            ms = 0;
            uninitializedObjects.push_back(this);
            startValue.push_back(val);
        }
    }
    elapsedMillis(const elapsedMillis& orig) { ms = orig.ms; }
    operator unsigned long() const { return timer->millis() - ms; }
    elapsedMillis& operator=(const elapsedMillis& rhs)
    {
        ms = rhs.ms;
        return *this;
    }
    elapsedMillis& operator=(unsigned long val)
    {
        ms = timer->millis() - val;
        return *this;
    }
    elapsedMillis& operator-=(unsigned long val)
    {
        ms += val;
        return *this;
    }
    elapsedMillis& operator+=(unsigned long val)
    {
        ms -= val;
        return *this;
    }
    elapsedMillis operator-(int val) const
    {
        elapsedMillis r(*this);
        r.ms += val;
        return r;
    }
    elapsedMillis operator-(unsigned int val) const
    {
        elapsedMillis r(*this);
        r.ms += val;
        return r;
    }
    elapsedMillis operator-(long val) const
    {
        elapsedMillis r(*this);
        r.ms += val;
        return r;
    }
    elapsedMillis operator-(unsigned long val) const
    {
        elapsedMillis r(*this);
        r.ms += val;
        return r;
    }
    elapsedMillis operator+(int val) const
    {
        elapsedMillis r(*this);
        r.ms -= val;
        return r;
    }
    elapsedMillis operator+(unsigned int val) const
    {
        elapsedMillis r(*this);
        r.ms -= val;
        return r;
    }
    elapsedMillis operator+(long val) const
    {
        elapsedMillis r(*this);
        r.ms -= val;
        return r;
    }
    elapsedMillis operator+(unsigned long val) const
    {
        elapsedMillis r(*this);
        r.ms -= val;
        return r;
    }
};

class elapsedMicros
{
  private:
    unsigned long us;
    static Timer* timer;
    static std::vector<elapsedMicros*> uninitializedObjects;
    static std::vector<unsigned long> startValue;

  public:
    static void init(Timer* timer)
    {
        elapsedMicros::timer = timer;
        for (uint32_t idx = 0; idx < uninitializedObjects.size(); idx++)
        {
            uninitializedObjects[idx]->us = timer->micros() - startValue[idx];
        }

        uninitializedObjects.clear();
        startValue.clear();
    }

    elapsedMicros(void)
      : elapsedMicros(0)
    {
    }
    elapsedMicros(unsigned long val)
    {
        if (timer != nullptr)
        {
            us = timer->micros() - val;
        }
        else
        {
            us = 0;
            uninitializedObjects.push_back(this);
            startValue.push_back(val);
        }
    }
    elapsedMicros(const elapsedMicros& orig) { us = orig.us; }
    operator unsigned long() const { return timer->micros() - us; }
    elapsedMicros& operator=(const elapsedMicros& rhs)
    {
        us = rhs.us;
        return *this;
    }
    elapsedMicros& operator=(unsigned long val)
    {
        us = timer->micros() - val;
        return *this;
    }
    elapsedMicros& operator-=(unsigned long val)
    {
        us += val;
        return *this;
    }
    elapsedMicros& operator+=(unsigned long val)
    {
        us -= val;
        return *this;
    }
    elapsedMicros operator-(int val) const
    {
        elapsedMicros r(*this);
        r.us += val;
        return r;
    }
    elapsedMicros operator-(unsigned int val) const
    {
        elapsedMicros r(*this);
        r.us += val;
        return r;
    }
    elapsedMicros operator-(long val) const
    {
        elapsedMicros r(*this);
        r.us += val;
        return r;
    }
    elapsedMicros operator-(unsigned long val) const
    {
        elapsedMicros r(*this);
        r.us += val;
        return r;
    }
    elapsedMicros operator+(int val) const
    {
        elapsedMicros r(*this);
        r.us -= val;
        return r;
    }
    elapsedMicros operator+(unsigned int val) const
    {
        elapsedMicros r(*this);
        r.us -= val;
        return r;
    }
    elapsedMicros operator+(long val) const
    {
        elapsedMicros r(*this);
        r.us -= val;
        return r;
    }
    elapsedMicros operator+(unsigned long val) const
    {
        elapsedMicros r(*this);
        r.us -= val;
        return r;
    }
};

// for audioExporter purposes
class elapsedMillisAudio
{
  private:
    static unsigned long getVirtualMillis() { return virtualMicros / 1000; }
    unsigned long ms;
    static bool virtualTime;
    static Timer* timer;
    static std::vector<elapsedMillisAudio*> uninitializedObjects;
    static std::vector<unsigned long> startValue;

  public:
    static void init(Timer* timer)
    {
        elapsedMillisAudio::timer = timer;
        for (uint32_t idx = 0; idx < uninitializedObjects.size(); idx++)
        {
            uninitializedObjects[idx]->ms = timer->millis() - startValue[idx];
        }

        uninitializedObjects.clear();
        startValue.clear();
    }

    static unsigned long virtualMicros;
    static void enterVirtualTimeMode()
    {
        virtualTime = 1;
        virtualMicros = 0;
    }
    static void exitVirtualTimeMode() { virtualTime = 0; }
    elapsedMillisAudio(void)
      : elapsedMillisAudio(0)
    {
    }

    elapsedMillisAudio(unsigned long val)
    {
        if (virtualTime)
        {
            ms = getVirtualMillis();
            return;
        }

        if (timer != nullptr)
        {
            ms = timer->millis();
        }
        else
        {
            ms = 0;
            uninitializedObjects.push_back(this);
            startValue.push_back(val);
        }
    }
    elapsedMillisAudio(const elapsedMillisAudio& orig) { ms = orig.ms; }
    operator unsigned long() const
    {
        return (virtualTime ? getVirtualMillis() : timer->millis()) - ms;
    }
    elapsedMillisAudio& operator=(const elapsedMillisAudio& rhs)
    {
        ms = rhs.ms;
        return *this;
    }
    elapsedMillisAudio& operator=(unsigned long val)
    {
        ms = (virtualTime ? getVirtualMillis() : timer->millis()) - val;
        return *this;
    }
    elapsedMillisAudio& operator-=(unsigned long val)
    {
        ms += val;
        return *this;
    }
    elapsedMillisAudio& operator+=(unsigned long val)
    {
        ms -= val;
        return *this;
    }
    elapsedMillisAudio operator-(int val) const
    {
        elapsedMillisAudio r(*this);
        r.ms += val;
        return r;
    }
    elapsedMillisAudio operator-(unsigned int val) const
    {
        elapsedMillisAudio r(*this);
        r.ms += val;
        return r;
    }
    elapsedMillisAudio operator-(long val) const
    {
        elapsedMillisAudio r(*this);
        r.ms += val;
        return r;
    }
    elapsedMillisAudio operator-(unsigned long val) const
    {
        elapsedMillisAudio r(*this);
        r.ms += val;
        return r;
    }
    elapsedMillisAudio operator+(int val) const
    {
        elapsedMillisAudio r(*this);
        r.ms -= val;
        return r;
    }
    elapsedMillisAudio operator+(unsigned int val) const
    {
        elapsedMillisAudio r(*this);
        r.ms -= val;
        return r;
    }
    elapsedMillisAudio operator+(long val) const
    {
        elapsedMillisAudio r(*this);
        r.ms -= val;
        return r;
    }
    elapsedMillisAudio operator+(unsigned long val) const
    {
        elapsedMillisAudio r(*this);
        r.ms -= val;
        return r;
    }
};

// for audioExporter purposes
class elapsedMicrosAudio
{
  private:
    unsigned long us;
    static bool virtualTime;
    static Timer* timer;
    static std::vector<elapsedMicrosAudio*> uninitializedObjects;
    static std::vector<unsigned long> startValue;

  public:
    static void init(Timer* timer)
    {
        elapsedMicrosAudio::timer = timer;
        for (uint32_t idx = 0; idx < uninitializedObjects.size(); idx++)
        {
            uninitializedObjects[idx]->us = timer->micros() - startValue[idx];
        }

        uninitializedObjects.clear();
        startValue.clear();
    }

    static unsigned long virtualMicros;
    static void enterVirtualTimeMode()
    {
        virtualTime = 1;
        virtualMicros = 0;
    }
    static bool isVirtualTimeMode() { return virtualTime; }
    static void exitVirtualTimeMode() { virtualTime = 0; }
    elapsedMicrosAudio(void)
      : elapsedMicrosAudio(0)
    {
    }
    elapsedMicrosAudio(unsigned long val)
    {
        if (virtualTime)
        {
            us = virtualMicros;
            return;
        }

        if (timer != nullptr)
        {
            us = timer->micros();
        }
        else
        {
            us = 0;
            uninitializedObjects.push_back(this);
            startValue.push_back(val);
        }
    }
    elapsedMicrosAudio(const elapsedMicrosAudio& orig) { us = orig.us; }
    operator unsigned long() const { return (virtualTime ? virtualMicros : timer->micros()) - us; }
    elapsedMicrosAudio& operator=(const elapsedMicrosAudio& rhs)
    {
        us = rhs.us;
        return *this;
    }
    elapsedMicrosAudio& operator=(unsigned long val)
    {
        us = (virtualTime ? virtualMicros : timer->micros()) - val;
        return *this;
    }
    elapsedMicrosAudio& operator-=(unsigned long val)
    {
        us += val;
        return *this;
    }
    elapsedMicrosAudio& operator+=(unsigned long val)
    {
        us -= val;
        return *this;
    }
    elapsedMicrosAudio operator-(int val) const
    {
        elapsedMicrosAudio r(*this);
        r.us += val;
        return r;
    }
    elapsedMicrosAudio operator-(unsigned int val) const
    {
        elapsedMicrosAudio r(*this);
        r.us += val;
        return r;
    }
    elapsedMicrosAudio operator-(long val) const
    {
        elapsedMicrosAudio r(*this);
        r.us += val;
        return r;
    }
    elapsedMicrosAudio operator-(unsigned long val) const
    {
        elapsedMicrosAudio r(*this);
        r.us += val;
        return r;
    }
    elapsedMicrosAudio operator+(int val) const
    {
        elapsedMicrosAudio r(*this);
        r.us -= val;
        return r;
    }
    elapsedMicrosAudio operator+(unsigned int val) const
    {
        elapsedMicrosAudio r(*this);
        r.us -= val;
        return r;
    }
    elapsedMicrosAudio operator+(long val) const
    {
        elapsedMicrosAudio r(*this);
        r.us -= val;
        return r;
    }
    elapsedMicrosAudio operator+(unsigned long val) const
    {
        elapsedMicrosAudio r(*this);
        r.us -= val;
        return r;
    }
};

#endif // __cplusplus
#endif // elapsedMillis_h
