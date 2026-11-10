// See engine_backend.h. Message format checked against real Tidal 1.10.3
// output (captured with a UDP listener in place of SuperDirt): one
// /dirt/play message per bundle, arguments as key / value pairs sorted by
// key: _id_ (slot number as a string), cps, cycle (event start), delta
// (event length in seconds), then the controls; strings 's', ints 'i'
// (cut, orbit, ...), everything else 'f'. The bundle time is the event
// time + latency (+ nudge, which is also sent as a control).
#include "engine_backend.h"

#include <arpa/inet.h>
#include <math.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

#include "check.h"
#include "engine.h"
#include "parse.h"
#include "scheduler.h"

namespace proto {

using namespace tc;

namespace {

constexpr int kMsgMax = 512;
constexpr int kSide = 256;  // per slot; > kRingSize so ids outlive their entries
constexpr int64_t kFsNum = 750000, kFsDen = 17;  // the Tracker's 44117.647 Hz

std::mutex gIsrMu;  // the "interrupts off" of the host
void isrLock() { gIsrMu.lock(); }
void isrUnlock() { gIsrMu.unlock(); }

struct Msg {
    int len;
    unsigned char b[kMsgMax];
};

// OSC encoding
struct OscWriter {
    unsigned char* b;
    int cap, n = 0;
    bool ok = true;
    void raw(const void* p, int len) {
        if (n + len > cap) {
            ok = false;
            return;
        }
        memcpy(b + n, p, len);
        n += len;
    }
    void str(const char* s) {
        int len = (int)strlen(s) + 1;
        raw(s, len);
        static const char zero[4] = {0, 0, 0, 0};
        if (len & 3) raw(zero, 4 - (len & 3));
    }
    void i32(int32_t v) {
        uint32_t u = htonl((uint32_t)v);
        raw(&u, 4);
    }
    void f32(float f) {
        uint32_t u;
        memcpy(&u, &f, 4);
        i32((int32_t)u);
    }
};

struct Arg {
    const char* key;
    char tag;  // s i f
    char s[64];
    int32_t i;
    float f;
};

}  // namespace

struct EngineBackend::Impl : Mapper, Voice {
    // engine side (main loop thread)
    Program edit, live;
    Event buf[1024];
    Entry storage[Scheduler::kStorageEntries];
    Scheduler sched;
    Msg staging[kSlots][kRingSize];
    int nStaged[kSlots] = {0};
    Msg side[kSlots][kSide];
    uint32_t seq[kSlots] = {0};

    // output
    int sock = -1;
    sockaddr_storage addr{};
    socklen_t addrLen = 0;
    double latency = 0.1;
    double wall0 = 0;  // unix time of block 0
    std::chrono::steady_clock::time_point t0;

    // threads
    std::mutex mainMu, logMu;
    std::vector<std::string> pending;
    std::atomic<bool> quit{false};
    std::thread audio, mainLoop;
    bool slotFailed[kSlots] = {false};

    void say(const std::string& s) {
        std::lock_guard<std::mutex> g(logMu);
        pending.push_back(s);
    }

    // ---- Mapper (main loop) ------------------------------------------------------
    void begin(int slot) override { nStaged[slot] = 0; }

    bool map(const Program& p, int slot, const Event& e, float samplesPerCycle, float lenSamples,
             Entry& out) override {
        if (!e.isMap) return false;
        const float fs = (float)kFsNum / (float)kFsDen;
        const float cps = fs / samplesPerCycle;
        Arg args[kMaxCtl + 4];
        int na = 0;
        auto add = [&](const char* key, char tag) -> Arg& {
            Arg& a = args[na++];
            a.key = key;
            a.tag = tag;
            return a;
        };
        snprintf(add("_id_", 's').s, sizeof args[0].s, "%d", slot + 1);
        add("cps", 'f').f = cps;
        add("cycle", 'f').f = (float)e.whole.s.n / (float)e.whole.s.d;
        add("delta", 'f').f = lenSamples / fs;
        for (int i = 0; i < e.nctl; i++) {
            const Ctl& c = e.ctl[i];
            const char* key = controlDef(c.key).name;
            bool dup = false;
            for (int j = 0; j < na; j++) dup |= !strcmp(args[j].key, key);
            if (dup) continue;
            if (c.tag == 's') formatCtl(p, c, add(key, 's').s, sizeof args[0].s);
            else if (c.tag == 'i') add(key, 'i').i = (int32_t)lroundf(c.f);
            else add(key, 'f').f = c.f;
        }
        std::sort(args, args + na, [](const Arg& a, const Arg& b) { return strcmp(a.key, b.key) < 0; });
        int k = nStaged[slot]++;
        Msg& m = staging[slot][k];
        OscWriter w{m.b, kMsgMax};
        w.str("/dirt/play");
        char tags[2 * (kMaxCtl + 4) + 2] = ",";
        for (int j = 0; j < na; j++) {
            size_t l = strlen(tags);
            tags[l] = 's';
            tags[l + 1] = args[j].tag;
            tags[l + 2] = 0;
        }
        w.str(tags);
        for (int j = 0; j < na; j++) {
            w.str(args[j].key);
            if (args[j].tag == 's') w.str(args[j].s);
            else if (args[j].tag == 'i') w.i32(args[j].i);
            else w.f32(args[j].f);
        }
        m.len = w.ok ? w.n : 0;
        out.kind = EntryKind::Note;
        out.chan = (uint8_t)slot;
        out.modVal[0] = (uint16_t)k;
        return w.ok;
    }

    void commit(int slot, Entry* e, int n) override {
        for (int j = 0; j < n; j++) {
            uint32_t id = seq[slot]++ % kSide;
            side[slot][id] = staging[slot][e[j].modVal[0]];
            e[j].modVal[0] = (uint16_t)id;
        }
    }

    // ---- Voice (audio thread) ------------------------------------------------------
    uint16_t noteOn(int slot, const Entry& e, uint32_t) override {
        const Msg& m = side[slot][e.modVal[0] % kSide];
        if (!m.len || sock < 0) return 0;
        double t = wall0 + ((double)e.block * kBlockSamples + e.offset) * kFsDen / kFsNum + latency;
        double sec = floor(t);
        uint32_t ntpSec = (uint32_t)((uint64_t)sec + 2208988800ULL);
        uint32_t ntpFrac = (uint32_t)((t - sec) * 4294967296.0);
        unsigned char b[kMsgMax + 32];
        OscWriter w{b, (int)sizeof b};
        w.str("#bundle");
        w.i32((int32_t)ntpSec);
        w.i32((int32_t)ntpFrac);
        w.i32(m.len);
        w.raw(m.b, m.len);
        if (w.ok) sendto(sock, b, w.n, 0, (const sockaddr*)&addr, addrLen);
        return 0;
    }
    void noteOff(int, uint8_t, uint8_t, uint8_t, uint16_t) override {}  // SuperDirt plays events out by delta / legato
    void allOff() override {}

    // ---- threads ---------------------------------------------------------------------
    void audioThread() {
        using namespace std::chrono;
        for (uint64_t b = 0; !quit; b++) {
            auto due = t0 + nanoseconds((int64_t)(b * kBlockSamples * 1e9 * kFsDen / kFsNum));
            std::this_thread::sleep_until(due);
            std::lock_guard<std::mutex> g(gIsrMu);
            sched.tick(*this);
        }
    }
    void mainThread() {
        while (!quit) {
            {
                std::lock_guard<std::mutex> g(mainMu);
                sched.update();
                for (int i = 0; i < kSlots; i++) {
                    const Error* err = sched.error(i);
                    if (err && !slotFailed[i]) say("d" + std::to_string(i + 1) + ": " + err->msg);
                    slotFailed[i] = err != nullptr;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    bool setCpsFrom(const Program& p) {
        Error err;
        int n = queryPattern(p, p.cpsExpr, Arc{Q{0, 1}, Q{1, 1}}, buf, 1024, err);
        if (n <= 0 || buf[0].v.t != VT::Num) {
            say(std::string("setcps: ") + (n < 0 ? err.msg : "needs a number"));
            return false;
        }
        const Val& v = buf[0].v;
        Q c = v.exact ? v.q : qmake(llroundf(v.fl * 100000.0f), 100000);
        if (c.n <= 0) {
            say("setcps: needs a positive number");
            return false;
        }
        sched.setCps(c);
        return true;
    }
};

EngineBackend::EngineBackend(const std::string& host, int port, double latency) : impl_(new Impl) {
    Impl& m = *impl_;
    m.latency = latency;
    m.live.clear();
    m.sched.init(&m.live, m.buf, 1024, &m, m.storage, Scheduler::Hooks{isrLock, isrUnlock});
    m.sched.setSampleRate(kFsNum, kFsDen);

    addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    std::string ps = std::to_string(port);
    if (getaddrinfo(host.c_str(), ps.c_str(), &hints, &res) == 0 && res) {
        m.sock = socket(res->ai_family, SOCK_DGRAM, 0);
        memcpy(&m.addr, res->ai_addr, res->ai_addrlen);
        m.addrLen = res->ai_addrlen;
        freeaddrinfo(res);
    }
    if (m.sock < 0) addLog("engine: can't open a UDP socket to " + host + ":" + ps);
    else addLog("engine: OSC to " + host + ":" + ps + ", latency " + std::to_string(latency) + " s");

    m.t0 = std::chrono::steady_clock::now();
    m.wall0 = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
    m.audio = std::thread([&m] { m.audioThread(); });
    m.mainLoop = std::thread([&m] { m.mainThread(); });
}

EngineBackend::~EngineBackend() {
    impl_->quit = true;
    impl_->audio.join();
    impl_->mainLoop.join();
    if (impl_->sock >= 0) close(impl_->sock);
}

bool EngineBackend::alive() const { return impl_->sock >= 0; }

void EngineBackend::send(const std::string& stmt) {
    Impl& m = *impl_;
    addLog("> " + stmt);
    std::string s = stmt;
    s.erase(0, s.find_first_not_of(" \t\n"));
    s.erase(s.find_last_not_of(" \t\n") + 1);
    std::lock_guard<std::mutex> g(m.mainMu);
    if (s == "hush") {
        m.sched.hush();
        return;
    }
    Error e, ce;
    m.edit.clear();
    std::string text = s + "\n";
    if (!parseFile(m.edit, text.c_str(), (int)text.size(), e)) {
        addLog(std::string("parse error: ") + e.msg);
        return;
    }
    if (!checkProgram(m.edit, ce)) {
        addLog(std::string("type error: ") + ce.msg);
        return;
    }
    if (m.edit.cpsExpr) m.setCpsFrom(m.edit);
    for (int i = 0; i < kSlots; i++) {
        if (!m.edit.block[i]) continue;
        Error err;
        if (!m.sched.evaluate(m.edit, i, err)) addLog("d" + std::to_string(i + 1) + ": " + err.msg);
    }
}

void EngineBackend::poll() {
    std::vector<std::string> p;
    {
        std::lock_guard<std::mutex> g(impl_->logMu);
        p.swap(impl_->pending);
    }
    for (auto& l : p) addLog(l);
}

}  // namespace proto
