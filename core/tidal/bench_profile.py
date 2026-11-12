#!/usr/bin/env python3
"""Name the PC-sampling profile in /tidal_bench.txt (bench v4).

usage: bench_profile.py tidal_bench.txt [firmware.axf] [--top N]

The axf must come from the same build as the .ptf that wrote the file.
Default: build/PolyendTrac_TidalBench.axf.
"""
import bisect
import os
import re
import subprocess
import sys
from collections import defaultdict

NM = "arm-none-eabi-nm"


def load_functions(axf):
    out = subprocess.run([NM, "-C", "-S", "--defined-only", axf], capture_output=True, text=True, check=True).stdout
    syms = []
    for line in out.splitlines():
        m = re.match(r"([0-9a-f]+) ([0-9a-f]+) ([tTwW]) (.*)", line)
        if m:
            addr = int(m[1], 16) & ~1
            syms.append((addr, int(m[2], 16), m[4]))
    syms.sort()
    return syms


def short(name):
    # drop argument lists and template noise for grouping
    name = re.sub(r"\(.*$", "", name)
    return name


def category(name):
    if name.startswith("tc::"):
        return "tidal engine"
    if re.match(r"(mem(cpy|set|move|cmp)|str(cmp|len|chr)|__aeabi_|__(div|mod|udiv|mul)|_?_?(sin|floor|lround)f?)", name):
        return "libc / runtime"
    if re.match(r"(Audio|playerEngine|AudioStream|MixerAndFx|dsp|filter|mtAudio|instrumentPlayer|Sequencer|"
                r"audioEngine|TrackerImpl::updateAudio|arm_|I2S|i2s|dma|DMA|Profiler)", name):
        return "audio / sequencer (interrupt)"
    if name.startswith("tidalBench") or "runBenchCase" in name or "runAll" in name:
        return "bench harness"
    return "other"


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    top = 40
    if "--top" in sys.argv:
        top = int(sys.argv[sys.argv.index("--top") + 1])
        args = [a for a in args if a != str(top)]
    if not args:
        sys.exit(__doc__)
    axf = args[1] if len(args) > 1 else os.path.join(os.path.dirname(__file__), "..", "..", "build",
                                                     "PolyendTrac_TidalBench.axf")
    syms = load_functions(axf)
    starts = [s[0] for s in syms]

    runs = defaultdict(lambda: defaultdict(int))
    totals = {}
    for line in open(args[0], encoding="utf-8", errors="replace"):
        m = re.match(r"pcprofile run (\d+) samples (\d+) other (\d+)", line)
        if m:
            totals[int(m[1])] = (int(m[2]), int(m[3]))
            continue
        m = re.match(r"pc (\d+) ([0-9a-f]+) (\d+)", line)
        if not m:
            continue
        run, addr, cnt = int(m[1]), int(m[2], 16), int(m[3])
        # a 32-byte bin can straddle two functions: credit the one at its middle
        probe = addr + 16
        i = bisect.bisect_right(starts, probe) - 1
        name = "?"
        if i >= 0 and syms[i][0] <= probe < syms[i][0] + max(syms[i][1], 1):
            name = short(syms[i][2])
        elif i >= 0:
            name = short(syms[i][2]) + " (near)"
        runs[run][name] += cnt

    for run in sorted(runs):
        total, other = totals.get(run, (sum(runs[run].values()), 0))
        print("=" * 78)
        print(f"run {run}: {total} samples ({other} outside flash)")
        cats = defaultdict(int)
        for name, c in runs[run].items():
            cats[category(name)] += c
        for cat, c in sorted(cats.items(), key=lambda kv: -kv[1]):
            print(f"  {100.0*c/total:5.1f}%  {cat}")
        print()
        for name, c in sorted(runs[run].items(), key=lambda kv: -kv[1])[:top]:
            print(f"  {100.0*c/total:5.1f}%  {name}")
        print()


if __name__ == "__main__":
    main()
