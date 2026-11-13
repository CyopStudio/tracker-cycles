#!/usr/bin/env python3
# Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
# GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
"""Plan step 3 check: real Tidal vs our engine (tidal_proto --backend engine)
on the same statements, compared on the OSC they send to SuperDirt.

usage: golden/osc_compare.py [--seconds S] [--stmts FILE] [--keep DIR]
       (run from tidal/, after `make`; needs GHC + Tidal like `make golden`)

Both send to UDP listeners here (no SuperDirt needed). For each slot
(_id_) and event start (cycle) present in both captures, the messages must
carry the same keys, types and values (floats within 1e-4 relative;
cycle/cps/delta within 1e-5 s). Timing: per stream, bundle time - nudge -
cycle/cps must be constant (the stream's clock offset); the spread is the
timing jitter, reported for both. The cycle range covered differs (Tidal
starts patterns mid-cycle, ours at the next cycle), so only cycles both
captured are compared.
"""
import os
import queue
import socket
import struct
import subprocess
import sys
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
TIDAL_PORT, OURS_PORT = 57197, 57198
STOP = threading.Event()

DEFAULT_STMTS = [
    'd1 $ s "bd*2 [sd cp]" # n "<0 2>" # pan 0.3 # legato 0.5',
    'd2 $ note "0 7 <12 5>" # s "superpiano" # cut 1 # nudge 0.01',
    'd3 $ every 2 (fast 2) $ s "hh*4" # gain 0.9 # speed "1 2"',
    'd4 $ off 0.125 (|+ n 12) $ n "0 .. 3" # s "arpy" # room 0.3',
    'd5 $ jux rev $ s "bd sn" # cutoff 2000 # resonance 0.2',
    'd6 $ chop 2 $ s "break" # begin 0.25 # orbit 1',
]


def ostr(b, i):
    j = b.index(b"\0", i)
    return b[i:j].decode(), (j + 4) & ~3


def parse_msg(b):
    addr, i = ostr(b, 0)
    tags, i = ostr(b, i)
    args = []
    for t in tags[1:]:
        if t == "s":
            v, i = ostr(b, i)
        elif t == "f":
            v = struct.unpack(">f", b[i:i + 4])[0]
            i += 4
        elif t == "i":
            v = struct.unpack(">i", b[i:i + 4])[0]
            i += 4
        elif t == "d":
            v = struct.unpack(">d", b[i:i + 8])[0]
            i += 8
        else:
            raise ValueError("tag " + t)
        args.append(v)
    return addr, tags, args


def parse_packet(b, out, tt=None):
    if b.startswith(b"#bundle"):
        sec, frac = struct.unpack(">II", b[8:16])
        t = sec - 2208988800 + frac / 2 ** 32
        i = 16
        while i < len(b):
            n = struct.unpack(">i", b[i:i + 4])[0]
            parse_packet(b[i + 4:i + 4 + n], out, t)
            i += 4 + n
    else:
        addr, tags, args = parse_msg(b)
        if addr != "/dirt/play":
            return
        kv = {}
        types = {}
        for k in range(0, len(args) - 1, 2):
            kv[args[k]] = args[k + 1]
            types[args[k]] = tags[1 + k + 1]
        out.append({"t": tt, "kv": kv, "types": types, "keys": [args[k] for k in range(0, len(args) - 1, 2)]})


def listen(port, seconds, out, ready):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("127.0.0.1", port))
    s.settimeout(0.1)
    ready.set()
    end = time.time() + seconds
    while time.time() < end and not STOP.is_set():
        try:
            b, _ = s.recvfrom(65536)
        except socket.timeout:
            continue
        parse_packet(b, out)
    s.close()


def run_tidal(stmts, seconds, boot):
    env = dict(os.environ)
    ghcup = os.path.expanduser("~/.ghcup/bin")
    env["PATH"] = ghcup + os.pathsep + env.get("PATH", "")
    p = subprocess.Popen(["ghci", "-ghci-script", boot], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT, text=True, env=env)
    lines = queue.Queue()
    threading.Thread(target=lambda: [lines.put(l) for l in p.stdout], daemon=True).start()
    t0 = time.time()
    while time.time() - t0 < 60:
        try:
            l = lines.get(timeout=1)
        except queue.Empty:
            continue
        if "Loaded GHCi configuration" in l:
            break
    p.stdin.write("streamResetCycles tidal\n")
    for st in stmts:
        p.stdin.write(st + "\n")
    p.stdin.flush()
    time.sleep(seconds)
    p.stdin.write("hush\n:quit\n")
    p.stdin.flush()
    try:
        p.wait(timeout=10)
    except subprocess.TimeoutExpired:
        p.kill()
    log = []
    while not lines.empty():
        log.append(lines.get())
    return log


def run_ours(stmts, seconds):
    args = [os.path.join(ROOT, "build", "tidal_proto"), "--backend", "engine", "--osc",
            f"127.0.0.1:{OURS_PORT}", "--keys", f"wait:{int(seconds * 1000)} log"]
    for st in stmts:
        args += ["--send", st]
    r = subprocess.run(args, capture_output=True, text=True, cwd=ROOT)
    return [l[2:] for l in r.stdout.splitlines() if l.startswith("| ")]


def offsets(msgs):
    o = []
    for m in msgs:
        kv = m["kv"]
        o.append(m["t"] - kv.get("nudge", 0.0) - kv["cycle"] / kv["cps"])
    return o


def close(a, b, key):
    if isinstance(a, str) or isinstance(b, str):
        return a == b
    tol = 1e-5 if key in ("cycle", "cps", "delta") else 1e-4
    return abs(a - b) <= tol * max(1.0, abs(a), abs(b))


def main():
    seconds = 8.0
    stmts = DEFAULT_STMTS
    keep = None
    a = sys.argv[1:]
    while a:
        o = a.pop(0)
        if o == "--seconds":
            seconds = float(a.pop(0))
        elif o == "--stmts":
            stmts = [l.rstrip("\n") for l in open(a.pop(0)) if l.strip() and not l.startswith("--")]
        elif o == "--keep":
            keep = a.pop(0)
        else:
            sys.exit(__doc__)

    boot_src = open(os.path.join(ROOT, "proto", "boot.ghci")).read()
    boot_src = boot_src.replace("oPort = 57120", f"oPort = {TIDAL_PORT}")
    boot = os.path.join(ROOT, "build", "osc_compare_boot.ghci")
    os.makedirs(os.path.dirname(boot), exist_ok=True)
    open(boot, "w").write(boot_src)

    tidal, ours = [], []
    r1, r2 = threading.Event(), threading.Event()
    l1 = threading.Thread(target=listen, args=(TIDAL_PORT, seconds + 70, tidal, r1))
    l2 = threading.Thread(target=listen, args=(OURS_PORT, seconds + 70, ours, r2))
    l1.start()
    l2.start()
    r1.wait()
    r2.wait()
    print(f"running {len(stmts)} statements for {seconds} s in each ...", flush=True)
    ours_log = []
    th = threading.Thread(target=lambda: ours_log.extend(run_ours(stmts, seconds)))
    th.start()
    tidal_log = run_tidal(stmts, seconds, boot)
    th.join()
    time.sleep(0.5)
    STOP.set()
    l1.join()
    l2.join()
    errs = [l for l in tidal_log if "error" in l.lower() or "<interactive>" in l]
    if errs:
        print("Tidal reported:\n  " + "\n  ".join(errs[:10]))
    errs = [l for l in ours_log if not l.startswith(">") and not l.startswith("engine:")]
    if errs:
        print("ours reported:\n  " + "\n  ".join(errs[:10]))
    if keep:
        os.makedirs(keep, exist_ok=True)
        for name, msgs in (("tidal", tidal), ("ours", ours)):
            with open(os.path.join(keep, name + ".txt"), "w") as f:
                for m in msgs:
                    f.write(f"{m['t']:.6f} {m['kv']}\n")

    ok = True
    print(f"messages: tidal {len(tidal)}, ours {len(ours)}")
    ids = sorted({m["kv"]["_id_"] for m in tidal + ours}, key=lambda s: int(s))
    for i in ids:
        T = [m for m in tidal if m["kv"]["_id_"] == i]
        O = [m for m in ours if m["kv"]["_id_"] == i]
        if not T or not O:
            print(f"d{i}: tidal {len(T)} messages, ours {len(O)}")
            ok = False
            continue
        lo = max(min(m["kv"]["cycle"] for m in T), min(m["kv"]["cycle"] for m in O))
        hi = min(max(m["kv"]["cycle"] for m in T), max(m["kv"]["cycle"] for m in O))
        lo = float(int(lo) + 1)  # whole cycles both covered
        hi = float(int(hi))
        tc = [m for m in T if lo <= m["kv"]["cycle"] < hi]
        oc = [m for m in O if lo <= m["kv"]["cycle"] < hi]
        key = lambda m: (round(m["kv"]["cycle"], 6), sorted((k, str(v)) for k, v in m["kv"].items()))
        tc.sort(key=key)
        oc.sort(key=key)
        bad = []
        if len(tc) != len(oc):
            bad.append(f"{len(tc)} vs {len(oc)} messages in cycles [{lo:g}, {hi:g})")
        for a, b in zip(tc, oc):
            if a["keys"] != b["keys"]:
                bad.append(f"cycle {a['kv']['cycle']:g}: keys {a['keys']} vs {b['keys']}")
                continue
            for k in a["keys"]:
                if a["types"][k] != b["types"][k] or not close(a["kv"][k], b["kv"][k], k):
                    bad.append(f"cycle {a['kv']['cycle']:g}: {k} = {a['kv'][k]!r} ({a['types'][k]}) vs "
                               f"{b['kv'][k]!r} ({b['types'][k]})")
        jt = offsets(T)
        jo = offsets(O)
        js = lambda o: (max(o) - min(o)) * 1000 if o else 0
        status = "ok" if not bad else "DIFF"
        print(f"d{i}: {status:4} cycles [{lo:g}, {hi:g}) {len(tc)} messages; jitter tidal {js(jt):.3f} ms, "
              f"ours {js(jo):.3f} ms")
        for b in bad[:6]:
            print("    " + b)
        ok &= not bad
    print("all match" if ok else "differences found")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
