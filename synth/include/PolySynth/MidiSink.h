#pragma once

#include "MidiEvent.h"

#include <functional>

namespace polysynth
{
/**
 * Abstraction of a component that consumes Midi events.
 */
class MidiSink
{
  public:
    virtual void onMidiEvent(const MidiEvent& event) = 0;

    virtual ~MidiSink() = default;
};
}
