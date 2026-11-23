// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// Where evaluated code goes. Step 1: real Tidal in GHCi. Step 3 swaps in
// our own engine behind the same interface.
#pragma once
#include <stdint.h>

#include <deque>
#include <string>
#include <vector>

#include "engine.h"
#include "tree.h"

namespace proto {

// Per-event highlighting (like Pulsar's): a sounding event names the
// mini-notation steps that made it, as their positions in the preorder
// list of steps of the statement it came from (atomOrder()). The editor
// renders the statements from its tree, so it maps them back to its own
// nodes by the statement's hash.
struct Highlight {
    int slot;
    uint32_t stmt;                // stmtHash() of the evaluated statement
    uint8_t n;
    uint16_t atom[tc::kMaxSrc];   // indices into atomOrder() of its expression
    double on, off;               // steadySeconds() when it sounds / ends
};
uint32_t stmtHash(const std::string& stmt);  // FNV-1a of the trimmed text
double steadySeconds();
// Step nodes (the kinds the engine tags events with) of the tree at
// `root`, in preorder.
using tc::isStepAtom;
void atomOrder(const tc::Program& p, uint16_t root, std::vector<uint16_t>& out);

class Backend {
public:
    virtual ~Backend() = default;
    virtual const char* name() const = 0;
    virtual bool alive() const = 0;
    // One Tidal statement, e.g. `d1 $ s "bd"`, `hush`, `setcps 0.5`.
    virtual void send(const std::string& stmt) = 0;
    // Pull any pending output into the log.
    virtual void poll() {}
    // Events that sounded since the last call (backends that know; the
    // engine backend). Appends to `out`.
    virtual void highlights(std::vector<Highlight>& out) { (void)out; }
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
