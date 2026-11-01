#pragma once

#include "PolySynth/Synth.h"
#include "PolySynth/sdk/InstrumentType.h"

#include <map>
#include <unordered_map>

namespace polysynth
{

/**
 * Registry of InstrumentType instances accessed by id.
 *
 * The registry is configured in Tracker/synth/registry.cpp.
 */
class InstrumentTypeRegistry
{
  public:
    [[nodiscard]] std::shared_ptr<const InstrumentType> getInstrumentType(int typeId) const;
    void addInstrumentType(int typeId, std::unique_ptr<InstrumentType> instrumentType);

    static void registerInstrumentTypes(InstrumentTypeRegistry& registry,
                                        const Synth::Allocator& largeBuffersAlloc);

    const std::unordered_map<int, std::shared_ptr<InstrumentType>>& getInstrumentTypes() const
    {
        return instrumentTypes;
    }

  private:
    std::unordered_map<int, std::shared_ptr<InstrumentType>> instrumentTypes;
};

} // namespace polysynth
