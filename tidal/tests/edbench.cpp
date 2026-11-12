// Host run of the editor timing bench (tidal/core/edit_bench.*): `make edbench`.
#include <stdio.h>
#include <time.h>

#include "edit_bench.h"

static uint32_t clockUs() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000000ull + ts.tv_nsec / 1000);
}

static tc::Program gA, gB, gC;
static tc::Span gSpans[tc::kMaxNodes];
static char gText[8192];
static char gOut[16384];

int main() {
    tc::runEditBench(&gA, &gB, &gC, gSpans, gText, sizeof gText, clockUs, 20, gOut, sizeof gOut);
    fputs(gOut, stdout);
    return 0;
}
