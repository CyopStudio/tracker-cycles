// Where evaluated code goes. Step 1: real Tidal in GHCi. Step 3 swaps in
// our own engine behind the same interface.
#pragma once
#include <deque>
#include <string>

namespace proto {

class Backend {
public:
    virtual ~Backend() = default;
    virtual const char* name() const = 0;
    virtual bool alive() const = 0;
    // One Tidal statement, e.g. `d1 $ s "bd"`, `hush`, `setcps 0.5`.
    virtual void send(const std::string& stmt) = 0;
    // Pull any pending output into the log.
    virtual void poll() {}
    std::deque<std::string> log;  // most recent last
protected:
    void addLog(const std::string& line);
};

// Prints statements (headless runs, tests).
class PrintBackend : public Backend {
public:
    explicit PrintBackend(bool toStdout) : stdout_(toStdout) {}
    const char* name() const override { return "print"; }
    bool alive() const override { return true; }
    void send(const std::string& stmt) override;
private:
    bool stdout_;
};

// Runs `ghci -ghci-script <boot>` and writes statements to its stdin.
class GhciBackend : public Backend {
public:
    GhciBackend(const std::string& ghci, const std::string& boot);
    ~GhciBackend() override;
    const char* name() const override { return "ghci"; }
    bool alive() const override;
    void send(const std::string& stmt) override;
    void poll() override;
private:
    int pid_ = -1;
    int in_ = -1, out_ = -1;
    std::string partial_;
};

}  // namespace proto
