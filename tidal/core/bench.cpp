#include "bench.h"

#include <stdio.h>
#include <string.h>

#include "check.h"
#include "parse.h"

namespace tc {

static const char* const kCases[] = {
    "s \"bd*2 [~ sd] bd sd\" # gain 1",
    "every 4 (fast 2) $ s \"bd*2 [~ sd] bd sd\" # n \"<0 2>\" # speed \"1 2\"",
    "s \"hh*16\" # n \"0 .. 15\" # pan sine # gain (range 0.8 1 saw)",
    "stut 8 0.5 0.0625 $ s \"hh*16\" # n \"0 .. 15\"",
    "jux rev $ chop 8 $ s \"bd*8 sd*8\" # speed \"1 2\"",
    "echo 8 0.125 0.7 $ s \"bd(5,8) sd(3,8,2)\" # n (irand 8)",
    "off 0.125 (|+ n 12) $ off 0.25 (|+ n 7) $ n (scale \"major\" \"0 .. 7\") # s \"superpiano\"",
    "sometimes (# speed 2) $ often (# crush 4) $ s \"[bd*4, hh*8, ~ sd]\" # n \"<0 1 2 3>\"",
    "n (arp \"up\" \"<c'maj e'min7 g'dom7 a'min>*2\") # s \"arpy\" # legato 0.5",
    "chunk 4 (hurry 2) $ iter 4 $ s \"bd sd hh cp hh sd bd cp\" # n \"0 .. 7\"",
};

int benchCaseCount() { return (int)(sizeof kCases / sizeof kCases[0]); }
const char* benchCase(int i) { return kCases[i]; }

bool runBenchCase(int i, Program& prog, Event* out, int cap, BenchClock clock, int reps, BenchResult& r, bool oneCycle) {
    memset(&r, 0, sizeof r);
    r.code = kCases[i];
    char text[512];
    snprintf(text, sizeof text, "d1 $ %s\n", kCases[i]);
    Error e, ce;
    if (!parseFile(prog, text, (int)strlen(text), e) || !checkProgram(prog, ce)) {
        snprintf(r.err, sizeof r.err, "%s%s", e.msg, ce.msg);
        return false;
    }
    uint16_t expr = prog.at(prog.block[0]).first;
    auto timed = [&](int n, int den, int& peak, int& ctl, uint32_t& avg, uint32_t& mx) {
        engineResetStats();
        uint64_t total = 0;
        for (int k = 0; k < n; k++) {
            int c = k % (16 * den);
            Error qe;
            uint32_t t0 = clock();
            int got = queryPattern(prog, expr, Arc{qmake(c, den), qmake(c + 1, den)}, out, cap, qe);
            uint32_t dt = clock() - t0;
            if (got < 0) {
                snprintf(r.err, sizeof r.err, "%s", qe.msg);
                return false;
            }
            if (den == 1) r.events = got;
            total += dt;
            if (dt > mx) mx = dt;
        }
        avg = (uint32_t)(total / (uint64_t)(n > 0 ? n : 1));
        peak = engineHighWater();
        ctl = engineCtlHighWater();
        return true;
    };
    if (oneCycle && !timed(reps, 1, r.peak, r.ctl, r.avgUs, r.maxUs)) return false;
    if (!timed(reps * 4, 4, r.peak4, r.ctl4, r.avgUs4, r.maxUs4)) return false;
    r.ok = true;
    return true;
}

}  // namespace tc
