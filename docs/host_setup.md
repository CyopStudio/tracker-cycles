# Host setup: Tidal + SuperDirt for WSL2

Needed for: checking Tidal behaviour in GHCi (spec open questions), golden
tests (step 2), and listening to the prototype (steps 1 and 3).

Layout:

```text
WSL2 (Ubuntu 26.04)                         Windows
  GHC 9.8.2 + Tidal 1.10.3  --OSC/UDP 57120-->  SuperCollider + SuperDirt -> speakers
  golden-test capture (no audio needed)
  C++ prototype (window via WSLg)
```

Why SuperDirt runs on Windows: SuperCollider on Linux needs JACK, and WSL2
only provides a PulseAudio socket (WSLg). Running SuperCollider natively on
Windows avoids an audio bridge and gives normal latency. Golden tests and
GHCi checks only need GHC and Tidal in WSL; no audio at all.

Versions:
- **Tidal 1.10.3** (depends on tidal-core 1.10.2, pinned). Hackage lists it
  as tested with GHC 8.6-9.8.2, so we use **GHC 9.8.2** via ghcup rather than
  Ubuntu's own ghc package.
- **SuperDirt v1.7.4** (check for a newer tag before installing, see 3.3).

---

## 1. GHC and cabal (WSL)

```bash
sudo apt update
sudo apt install -y build-essential curl git libffi-dev libgmp-dev \
    libncurses-dev pkg-config zlib1g-dev

# ghcup, non-interactive, with GHC 9.8.2 and the recommended cabal
curl --proto '=https' --tlsv1.2 -sSf https://get-ghcup.haskell.org | \
    BOOTSTRAP_HASKELL_NONINTERACTIVE=1 \
    BOOTSTRAP_HASKELL_GHC_VERSION=9.8.2 \
    BOOTSTRAP_HASKELL_CABAL_VERSION=recommended \
    BOOTSTRAP_HASKELL_ADJUST_BASHRC=1 \
    sh

source ~/.ghcup/env          # or open a new shell
ghc --version                # expect 9.8.2
cabal --version
```

## 2. Tidal (WSL)

```bash
cabal update
cabal install tidal-1.10.3 --lib
```

Installed this way on 2026-11-09 (ghcup 0.2.6.2, cabal 3.16.1.0). The
Tidal 1.10.3 startup banner says `[TidalCycles version 1.10.1]`; that is
the version string inside the package, not a wrong install.
`tidal/run_proto.sh` sources `~/.ghcup/env` itself, so the Windows
shortcut finds `ghci` too.

`--lib` registers Tidal in GHC's default package environment
(`~/.ghc/x86_64-linux-9.8.2/environments/default`), so plain `ghci` can
import it. The first build takes a while.

If it fails with dependency conflicts, move `~/.ghc` and `~/.cabal` (or
`~/.config/cabal` / `~/.local/state/cabal`) aside and rerun, as the Tidal
docs suggest.

### Check it

```bash
ghci
```

```haskell
import Sound.Tidal.Context
:set -XOverloadedStrings
queryArc (s "bd*2 sd") (Arc 0 1)   -- should list 3 events
:info (#)                          -- shows the fixity (spec 4.1)
:quit
```

This is all that's needed for the spec's open questions and for golden-test
capture. Sections 3-5 are only for hearing sound.

---

## 3. SuperCollider and SuperDirt (Windows)

### 3.1 SuperCollider

Install the current Windows release from
https://supercollider.github.io/downloads.

### 3.2 sc3-plugins

Download the Windows sc3-plugins release from
https://supercollider.github.io/sc3-plugins and extract the
`SC3plugins` folder into SuperCollider's user extension directory. To
find that directory, run this in the SuperCollider IDE (Ctrl+Enter):

```supercollider
Platform.userExtensionDir
```

Restart SuperCollider afterwards.

### 3.3 SuperDirt

Check the latest tag (from WSL):

```bash
git ls-remote https://codeberg.org/musikinformatik/SuperDirt.git | grep tags | tail -n1
```

In the SuperCollider IDE, run (Ctrl+Enter), using that tag:

```supercollider
Quarks.checkForUpdates({Quarks.install("SuperDirt", "v1.7.4"); thisProcess.recompile()})
```

This also installs the Vowel and Dirt-Samples quarks. It's done when the
post window shows the SuperCollider welcome message again.

### 3.4 Start SuperDirt

Each session, in the IDE:

```supercollider
SuperDirt.start;
```

It listens on UDP port 57120. The first time, Windows Firewall asks about
`sclang.exe`: allow it on **Public** networks too, because Windows treats
the WSL virtual network as public.

---

## 4. Networking WSL -> Windows

This machine uses WSL's default **NAT** networking. Windows is reachable at
the WSL default gateway:

```bash
ip route show default | awk '{print $3}'     # e.g. 172.28.32.1
```

If the firewall prompt was missed or answered wrongly, add a rule in an
admin PowerShell:

```powershell
New-NetFirewallRule -DisplayName "SuperDirt from WSL" -Direction Inbound `
    -Protocol UDP -LocalPort 57120 -Action Allow
```

Optional alternative: setting `networkingMode=mirrored` under `[wsl2]` in
`%UserProfile%\.wslconfig` (then `wsl --shutdown`) makes Windows reachable
at `127.0.0.1` from WSL. It changes networking for all WSL distros, so it's
your call.

---

## 5. Boot Tidal against Windows SuperDirt (WSL)

Our own minimal boot script; not stored in the repo yet. Save it as
`~/tidal/boot.ghci`, replacing the address with the gateway from section 4
(or `127.0.0.1` in mirrored mode):

```haskell
:set -XOverloadedStrings
:set prompt "tidal> "
import Sound.Tidal.Context

tidal <- startTidal (superdirtTarget {oAddress = "172.28.32.1", oPort = 57120, oLatency = 0.1}) defaultConfig

:{
let p = streamReplace tidal
    hush = streamHush tidal
    once = streamOnce tidal
    setcps = once . cps
    d1 = p 1
    d2 = p 2
    d3 = p 3
    d4 = p 4
    d5 = p 5
    d6 = p 6
    d7 = p 7
    d8 = p 8
:}
```

Run it and play something:

```bash
ghci -ghci-script ~/tidal/boot.ghci
```

```haskell
d1 $ s "bd*2 [sd cp]"
hush
```

Sound out of the Windows speakers means the whole chain works.

---

## 6. Troubleshooting

| Symptom | Check |
| :--- | :--- |
| `ghci` can't find `Sound.Tidal.Context` | `ghc --version` is 9.8.2 and `cabal install ... --lib` used the same GHC (`ghcup list`). |
| No sound, no errors | SuperDirt running? Firewall rule (section 4)? Right address in `boot.ghci`? Watch the SuperDirt post window: if nothing at all appears when you play a pattern, the OSC isn't arriving. |
| `late` messages in SuperDirt | Raise `oLatency` (0.2), or switch to mirrored networking. |
| cabal dependency errors | Section 2 reset; make sure you didn't also install Ubuntu's `ghc`/`cabal-install` packages. |

Sources: [Tidal Linux install](https://tidalcycles.org/docs/getting-started/linux_install),
[Tidal Windows install](https://tidalcycles.org/docs/getting-started/windows_install),
[tidal 1.10.3 on Hackage](https://hackage.haskell.org/package/tidal-1.10.3).
