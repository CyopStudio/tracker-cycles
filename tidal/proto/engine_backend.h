// Plan step 3: our own engine + scheduler behind the prototype's Backend
// interface, sending SuperDirt OSC (/dirt/play bundles) like Tidal does.
// Runs the scheduler the way the device will: a "main loop" thread calls
// update(), an "audio" thread calls tick() every 128 samples of real time;
// tick() sends each due entry as an OSC bundle timestamped at its exact
// sample time + latency (Tidal's oLatency).
#pragma once
#include <memory>
#include <string>

#include "backend.h"

namespace proto {

class EngineBackend : public Backend {
public:
    // host / port of SuperDirt; latency in seconds (boot.ghci uses 0.1)
    EngineBackend(const std::string& host, int port, double latency);
    ~EngineBackend() override;
    const char* name() const override { return "engine"; }
    bool alive() const override;
    void send(const std::string& stmt) override;
    void poll() override;

    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

}  // namespace proto
