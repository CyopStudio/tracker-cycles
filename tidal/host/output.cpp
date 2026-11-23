// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// See output.h.
#include "output.h"

#include <stdio.h>
#include <string.h>

#include <fstream>
#include <sstream>

#include "render.h"

using namespace tc;

namespace proto {

namespace {
Backend* gBackend = nullptr;

std::string launchHead(const Program& p, int slot) {
    const uint16_t b = p.block[slot];
    const std::string n = std::to_string(slot + 1);
    if (!b) return "d" + n;
    const Node& bn = p.at(b);
    switch (blockLaunch(bn)) {
        case Launch::JumpMod: return "jumpMod " + n + " " + std::to_string(bn.b);
        case Launch::JumpIn: return "jumpIn " + n + " " + std::to_string(bn.b);
        default: return "d" + n;
    }
}
}  // namespace

Backend* appBackend() { return gBackend; }
void setAppBackend(Backend* b) { gBackend = b; }

std::string BackendOutput::evalText(const Program& p, int slot, std::string* errOut) {
    static char buf[8192];
    Renderer r(buf, sizeof buf);
    buf[0] = 0;
    const uint16_t b = p.block[slot];
    if (!b || !p.at(b).first) return "d" + std::to_string(slot + 1) + " silence";
    if (slot < kSampleSlots) {
        r.block(p, slot);
        return buf;
    }
    // MIDI channel: route to SuperDirt's MIDI target (spec 9.1); this never
    // goes into the tree or the .tidal file.
    r.expr(p, p.at(b).first);
    if (r.overflow() && errOut) *errOut = "block too long";
    // |< : the block's own values win (a `# midichan` in the code), the
    // slot's channel only fills in (same rule as the device mapper)
    return launchHead(p, slot) + " $ (" + buf + ") |< s \"midi\" |< midichan " +
           std::to_string(slot - kSampleSlots);
}

bool BackendOutput::evaluate(const Program& p, int slot, Label& msg) {
    std::string err;
    const std::string text = evalText(p, slot, &err);
    if (!err.empty()) {
        msg = err.c_str();
        return false;
    }
    send(text);
    // remember its steps for highlighting (the old statement's events keep
    // sounding until the switch, so a few are kept)
    Evaluated ev{stmtHash(text), {}};
    atomOrder(p, p.at(p.block[slot]).first, ev.atoms);
    auto& list = evaluated_[slot];
    list.push_back(std::move(ev));
    if (list.size() > 4) list.erase(list.begin());
    return true;
}

void BackendOutput::silence(int slot) { send("d" + std::to_string(slot + 1) + " silence"); }
void BackendOutput::hush() { send("hush"); }
void BackendOutput::mute(int slot, bool on) { send(std::string(on ? "mute " : "unmute ") + std::to_string(slot + 1)); }
void BackendOutput::unmuteAll() { send("unmuteAll"); }

void BackendOutput::setCps(const Program& p) {
    char buf[256];
    Renderer r(buf, sizeof buf);
    buf[0] = 0;
    r.expr(p, p.cpsExpr);
    send(std::string("setcps (") + buf + ")");
}

bool BackendOutput::save(const Program& p, Label& msg) {
    if (path.empty()) {
        msg = "no file path (use --save)";
        return false;
    }
    std::ofstream out(path);
    if (!out) {
        msg = ("can't write " + path).c_str();
        return false;
    }
    static char buf[65536];
    Renderer r(buf, sizeof buf);
    buf[0] = 0;
    r.file(p);
    out << buf;
    msg = ("saved " + path).c_str();
    return true;
}

int BackendOutput::litSteps(Lit* out, int cap) {
    if (backend_) {
        std::vector<Highlight> hs;
        backend_->highlights(hs);
        for (const Highlight& h : hs) {
            if (h.slot < 0 || h.slot >= kSlots) continue;
            const auto& list = evaluated_[h.slot];
            for (auto it = list.rbegin(); it != list.rend(); ++it) {
                if (it->stmt != h.stmt) continue;
                for (int k = 0; k < h.n; k++)
                    if (h.atom[k] < it->atoms.size()) pending_.push_back(Lit{h.slot, it->atoms[h.atom[k]], h.on, h.off});
                break;
            }
        }
    }
    const int n = (int)pending_.size() < cap ? (int)pending_.size() : cap;
    for (int i = 0; i < n; i++) out[i] = pending_[i];
    pending_.erase(pending_.begin(), pending_.begin() + n);
    return n;
}

bool loadFile(Editor& ed, const std::string& path, std::string& err) {
    std::ifstream in(path);
    if (!in) {
        err = "can't open " + path;
        return false;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string text = ss.str();
    Label e;
    if (!ed.loadText(text.c_str(), (int)text.size(), e)) {
        err = e.c_str();
        return false;
    }
    return true;
}

std::string litReport(Editor& ed) {
    std::string out;
    const Program& p = ed.prog();
    static Span spans[kMaxNodes];
    for (int s = 0; s < kSlots; s++) {
        const uint16_t b = ed.playingBlock(s);
        if (!b || !p.at(b).first) continue;
        std::vector<bool> on(kMaxNodes, false);
        bool any = false;
        for (const auto& l : ed.litSeen())
            if (l.slot == s && l.node < kMaxNodes) on[l.node] = any = true;
        if (!any) continue;
        memset(spans, 0, sizeof spans);
        char buf[8192];
        Renderer r(buf, sizeof buf, spans);
        buf[0] = 0;
        r.blockNode(p, b);
        std::string text = buf;
        std::vector<int> open(text.size() + 1, 0), close(text.size() + 1, 0);
        for (int n = 1; n < kMaxNodes; n++)
            if (on[n] && spans[n].end > spans[n].start && spans[n].end <= text.size()) {
                open[spans[n].start]++;
                close[spans[n].end]++;
            }
        std::string line;
        for (size_t i = 0; i <= text.size(); i++) {
            for (int k = 0; k < close[i]; k++) line += "]]";
            for (int k = 0; k < open[i]; k++) line += "[[";
            if (i < text.size()) line += text[i];
        }
        out += "lit: " + line + "\n";
    }
    ed.clearLitSeen();
    return out;
}

}  // namespace proto
