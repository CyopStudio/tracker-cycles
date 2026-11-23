// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// The prototype's EditorOutput (tidal/editor/editor.h): turns the editor's
// results into Tidal statements for a Backend (GHCi, our engine, or print),
// as the prototype always did, writes the file on Save, and maps the engine
// backend's highlights (statement + step index) back to the editor's nodes.
#pragma once
#include <string>
#include <vector>

#include "backend.h"
#include "editor.h"

namespace proto {

class BackendOutput : public tc::EditorOutput {
public:
    explicit BackendOutput(Backend* b) : backend_(b) {}
    std::string path;  // where Save writes ("" = nowhere yet)

    bool evaluate(const tc::Program& p, int slot, tc::Label& msg) override;
    void silence(int slot) override;
    void hush() override;
    void mute(int slot, bool on) override;
    void unmuteAll() override;
    void setCps(const tc::Program& p) override;
    bool save(const tc::Program& p, tc::Label& msg) override;
    int litSteps(Lit* out, int cap) override;

    // The statement for slot's selected block ("d1 $ ..."; MIDI slots are
    // routed to SuperDirt's MIDI target); "" + err if it doesn't fit.
    static std::string evalText(const tc::Program& p, int slot, std::string* err = nullptr);

private:
    Backend* backend_;
    struct Evaluated {
        uint32_t stmt;                // stmtHash of what was sent
        std::vector<uint16_t> atoms;  // its steps, atomOrder() of the block's expression
    };
    std::vector<Evaluated> evaluated_[tc::kSlots];  // newest last, a few kept
    std::vector<Lit> pending_;
    void send(const std::string& s) {
        if (backend_) backend_->send(s);
    }
};

// The backend the prototype runs with (view / window / headless keys use it).
Backend* appBackend();
void setAppBackend(Backend* b);

// Load / save files for the editor (the model itself does no file I/O).
bool loadFile(tc::Editor& ed, const std::string& path, std::string& err);
// Headless `lit`: each playing block with the steps lit since the last call
// marked [[step]] (clears the editor's list).
std::string litReport(tc::Editor& ed);

}  // namespace proto
