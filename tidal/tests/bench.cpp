// Engine benchmark: heavy patterns, queried the way the scheduler will
// (one cycle, and quarter-cycle windows). Prints events, arena high water
// and time per query. `make bench`. The same patterns are meant for the
// on-device timing build (plan step 2).
#include <stdio.h>
#include <string.h>

#include <chrono>

#include "check.h"
#include "engine.h"
#include "parse.h"

using namespace tc;

static Program gProg;
static Event gOut[4096];

static const char* kCases[] = {
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

int main() {
    printf("%-6s %-6s %-6s %-8s %-8s  pattern\n", "events", "peak", "peak/4", "us/cyc", "us/q4");
    int worst = 0;
    for (const char* code : kCases) {
        char text[512];
        snprintf(text, sizeof text, "d1 $ %s\n", code);
        Error e, ce;
        if (!parseFile(gProg, text, (int)strlen(text), e) || !checkProgram(gProg, ce)) {
            printf("bad: %s (%s%s)\n", code, e.msg, ce.msg);
            continue;
        }
        uint16_t expr = gProg.at(gProg.block[0]).first;
        const int reps = 200;
        // one cycle at a time
        engineResetStats();
        int events = 0;
        auto t0 = std::chrono::steady_clock::now();
        for (int r = 0; r < reps; r++) {
            Error qe;
            int c = r % 16;
            int n = queryPattern(gProg, expr, Arc{Q{c, 1}, Q{c + 1, 1}}, gOut, 4096, qe);
            if (n < 0) {
                printf("error: %s\n", qe.msg);
                break;
            }
            events = n;
        }
        auto t1 = std::chrono::steady_clock::now();
        int peak = engineHighWater();
        // quarter-cycle windows
        engineResetStats();
        for (int r = 0; r < reps * 4; r++) {
            Error qe;
            int c = r % 64;
            queryPattern(gProg, expr, Arc{Q{c, 4}, Q{c + 1, 4}}, gOut, 4096, qe);
        }
        auto t2 = std::chrono::steady_clock::now();
        int peak4 = engineHighWater();
        double us1 = std::chrono::duration<double, std::micro>(t1 - t0).count() / reps;
        double us4 = std::chrono::duration<double, std::micro>(t2 - t1).count() / (reps * 4);
        if (peak > worst) worst = peak;
        printf("%-6d %-6d %-6d %-8.1f %-8.1f  %.70s\n", events, peak, peak4, us1, us4, code);
    }
    printf("worst arena peak %d events x %zu B = %zu KB (arena %d)\n", worst, sizeof(Event),
           worst * sizeof(Event) / 1024, kEventArena);
    return 0;
}
