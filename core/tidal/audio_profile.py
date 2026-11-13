#!/usr/bin/env python3
# Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
# GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
"""Name the nodes in /audio_profile.txt (from a -DAUDIO_PROFILE build).

usage: audio_profile.py audio_profile.txt [firmware.axf]

The axf must be the one built with the .ptf that wrote the file (the
audio graph objects are statics, so their addresses come from the link).
Default: build/PolyendTrac_AudioProf.axf.
"""
import bisect
import os
import re
import subprocess
import sys
from collections import defaultdict

NM = "arm-none-eabi-nm"


def load_symbols(axf):
    out = subprocess.run([NM, "-C", "-S", "--defined-only", axf], capture_output=True, text=True, check=True).stdout
    vt, data = [], []
    for line in out.splitlines():
        m = re.match(r"([0-9a-f]+) ([0-9a-f]+) (\w) (.*)", line)
        if not m:
            continue
        addr, size, kind, name = int(m[1], 16), int(m[2], 16), m[3], m[4]
        if name.startswith("vtable for "):
            vt.append((addr, size, name[len("vtable for "):]))
        elif kind in "bBdD":
            data.append((addr, size, name))
    vt.sort()
    data.sort()
    return vt, data


def find(table, addr):
    i = bisect.bisect_right([t[0] for t in table], addr) - 1
    if i >= 0 and table[i][0] <= addr < table[i][0] + max(table[i][1], 1):
        return table[i]
    return None


def parse(path):
    captures, cur = [], None
    for line in open(path, encoding="utf-8", errors="replace"):
        if line.startswith("=== capture"):
            cur = {"title": line[4:].strip(), "head": [], "stages": [], "nodes": [], "block": 522360}
            captures.append(cur)
        elif cur is None:
            continue
        elif line.startswith("node ") and not line.startswith("node idx"):
            f = line.split()
            cur["nodes"].append((int(f[1]), int(f[2], 16), int(f[3], 16), int(f[4]), int(f[5]), int(f[6])))
        elif re.match(r"cycles per us", line):
            cur["block"] = int(re.search(r"block = (\d+)", line)[1])
            cur["head"].append(line.rstrip())
        elif line.startswith("Profiler"):
            cur["head"].append(line.rstrip())
        elif re.match(r"\S.*\s\d+\s+\d+\s+\d+\.\d$", line.rstrip()):
            cur["stages"].append(line.rstrip())
    return captures


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    axf = sys.argv[2] if len(sys.argv) > 2 else os.path.join(
        os.path.dirname(__file__), "..", "..", "build", "PolyendTrac_AudioProf.axf")
    vt, data = load_symbols(axf)
    captures = parse(sys.argv[1])

    def name_of(obj, vtbl):
        v = find(vt, vtbl)
        cls = v[2] if v else f"? vtbl {vtbl:08x}"
        d = find(data, obj)
        owner = f"{d[2]}+{obj - d[0]}" if d else f"{obj:08x}"
        return cls, owner

    summary = defaultdict(lambda: [0.0] * len(captures))
    for ci, c in enumerate(captures):
        blk = c["block"]
        print("=" * 78)
        print(c["title"])
        for h in c["head"]:
            print(h)
        print()
        for s in c["stages"]:
            print(s)
        print(f"\n{'avg %':>6} {'max %':>6} {'calls':>6}  class @ owner+offset")
        rows = []
        for idx, obj, vtbl, avg, mx, calls in c["nodes"]:
            cls, owner = name_of(obj, vtbl)
            rows.append((avg, mx, calls, cls, owner))
            summary[cls][ci] += 100.0 * avg / blk
        for avg, mx, calls, cls, owner in sorted(rows, reverse=True):
            if 100.0 * avg / blk < 0.05:
                continue
            print(f"{100.0*avg/blk:6.2f} {100.0*mx/blk:6.2f} {calls:6d}  {cls} @ {owner}")
        print()

    print("=" * 78)
    print("avg % of the block per class, by capture")
    print(f"{'':40}" + "".join(f"{i+1:>7}" for i in range(len(captures))))
    for cls, v in sorted(summary.items(), key=lambda kv: -max(kv[1])):
        print(f"{cls[:40]:40}" + "".join(f"{x:7.2f}" for x in v))


if __name__ == "__main__":
    main()
