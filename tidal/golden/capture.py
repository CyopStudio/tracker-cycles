#!/usr/bin/env python3
# Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
# GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
"""Capture golden event lists from real Tidal (GHCi).

Reads golden/cases.txt, queries every case in GHCi with queryArc over
cycles 0..N, and writes golden/expected.txt. Also records `:t` of every
function / control / operator in core/library.cpp to golden/types.txt.

Needs GHC + Tidal 1.10.3 (docs/host_setup.md). Run from tidal/:
    python3 golden/capture.py            (or: make golden)

expected.txt format, one block per case:
    == name cycles code
    whole-start whole-stop part-start part-stop key=tag:value ...
Times are exact rationals "num/den" ("~ ~" = no whole: continuous signal).
Value tags: s string, f double, n note, r rational, i int, b bool.
Events are sorted by part start, part stop, then text.
Only times and values are recorded (Tidal's Event == also compares source
positions, which we don't model).
"""
import datetime
import glob
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TIDAL = os.path.dirname(HERE)

PRELUDE = r'''
:set -package containers
:set -XOverloadedStrings
import Sound.Tidal.Context
import qualified Data.Map.Strict as GMap
import Data.List (sortOn)
import Data.Ratio (numerator, denominator)
-- Tracker Cycles' own control (not in Tidal; core/library.cpp, docs/tidal_voices.md)
let voices = pI "voices"
:{
gR :: Rational -> String
gR r = show (numerator r) ++ "/" ++ show (denominator r)
gV :: Value -> String
gV (VS x) = "s:" ++ x
gV (VF x) = "f:" ++ show x
gV (VN x) = "n:" ++ show (unNote x)
gV (VR x) = "r:" ++ gR x
gV (VI x) = "i:" ++ show x
gV (VB x) = "b:" ++ show x
gV v = "?:" ++ show v
gE :: Event ValueMap -> String
gE e = w ++ " " ++ gR (start (part e)) ++ " " ++ gR (stop (part e))
         ++ concatMap (\(k, v) -> " " ++ k ++ "=" ++ gV v) (GMap.toList (value e))
  where w = maybe "~ ~" (\a -> gR (start a) ++ " " ++ gR (stop a)) (whole e)
gQ :: Rational -> ControlPattern -> IO ()
gQ cycles p = mapM_ (putStrLn . gE)
  $ sortOn (\e -> (start (part e), stop (part e), gE e)) $ queryArc p (Arc 0 cycles)
:}
'''


def read_cases():
    cases = []
    with open(os.path.join(HERE, "cases.txt")) as f:
        for ln, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            m = re.match(r"\s*(\S+)\s+(\d+)\s+(.+?)\s*$", line)
            if not m:
                sys.exit(f"cases.txt:{ln}: expected 'name cycles code'")
            cases.append((m.group(1), int(m.group(2)), m.group(3)))
    names = [c[0] for c in cases]
    dup = {n for n in names if names.count(n) > 1}
    if dup:
        sys.exit("duplicate case names: " + ", ".join(sorted(dup)))
    return cases


def library_names():
    src = open(os.path.join(TIDAL, "core", "library.cpp")).read()
    return re.findall(r'\b(?:FN|CTL|OP)\("([^"]+)"', src)


def versions():
    ghc = subprocess.run(["ghc", "--numeric-version"], capture_output=True, text=True).stdout.strip()
    pkgs = []
    for env in glob.glob(os.path.expanduser(f"~/.ghc/*-{ghc}/environments/default")):
        for line in open(env):
            m = re.match(r"package-id (tidal(?:-core|-link)?-[\d.]+)-", line)
            if m:
                pkgs.append(m.group(1))
    return ghc, sorted(set(pkgs))


def main():
    cases = read_cases()
    names = library_names()
    script = [PRELUDE]
    for name, cycles, code in cases:
        script.append(f'putStrLn "##BEGIN {name}"')
        script.append(f"gQ {cycles} ({code})")
        script.append(f'putStrLn "##END {name}"')
    for n in names:
        op = not re.match(r"[A-Za-z]", n)
        script.append(f'putStrLn "##TYPE {n}"')
        script.append(f":t {'(' + n + ')' if op else n}")
    script.append('putStrLn "##DONE"')
    try:
        out = subprocess.run(["ghci", "-v0"], input="\n".join(script) + "\n", stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                             text=True, timeout=600, cwd=HERE)
    except FileNotFoundError:
        sys.exit("ghci not found: install GHC + Tidal (docs/host_setup.md) or `source ~/.ghcup/env`")
    lines = out.stdout.splitlines()
    if "##DONE" not in lines:
        sys.exit("GHCi did not finish:\n" + "\n".join(lines[-30:]))

    # split GHCi output into sections
    events, types, errors = {}, {}, {}
    cur, kind = None, None
    for line in lines:
        if line.startswith("##BEGIN "):
            cur, kind = line[8:], "case"
            events[cur] = []
        elif line.startswith("##END "):
            cur, kind = None, None
        elif line.startswith("##TYPE "):
            cur, kind = line[7:], "type"
            types[cur] = []
        elif line == "##DONE":
            cur = None
        elif cur and kind == "case":
            if re.match(r"(~ ~|-?\d+/\d+ -?\d+/\d+) -?\d+/\d+ -?\d+/\d+( |$)", line):
                events[cur].append(line)
            else:
                errors.setdefault(cur, []).append(line)
        elif cur and kind == "type":
            types[cur].append(line.strip())

    missing = [c[0] for c in cases if c[0] not in events]
    if errors or missing:
        for n, err in errors.items():
            print(f"case {n} failed in Tidal:\n  " + "\n  ".join(err[:8]), file=sys.stderr)
        for n in missing:
            print(f"case {n}: no output", file=sys.stderr)
        sys.exit(1)

    ghc, pkgs = versions()
    stamp = f"# {', '.join(pkgs)}, GHC {ghc}, captured {datetime.date.today().isoformat()}"
    with open(os.path.join(HERE, "expected.txt"), "w") as f:
        f.write("# Golden events captured from real Tidal by golden/capture.py. Don't edit.\n")
        f.write(stamp + "\n")
        f.write("# whole-start whole-stop part-start part-stop key=tag:value ...\n")
        for name, cycles, code in cases:
            f.write(f"\n== {name} {cycles} {code}\n")
            for e in events[name]:
                f.write(e + "\n")
    with open(os.path.join(HERE, "types.txt"), "w") as f:
        f.write("# :t of every core/library.cpp entry in real Tidal (golden/capture.py).\n")
        f.write(stamp + "\n\n")
        for n in names:
            t = " ".join(types.get(n, ["?"]))
            t = re.sub(r"\s+", " ", t)
            f.write(f"{n} :: {t.split(' :: ', 1)[-1]}\n")
    total = sum(len(v) for v in events.values())
    print(f"captured {len(cases)} cases ({total} events), {len(names)} types")


if __name__ == "__main__":
    main()
