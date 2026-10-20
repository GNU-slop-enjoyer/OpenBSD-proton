# Testing

Tiers follow CLAUDE.md §8 Stage G. Every result names the machine, revision,
command and log. "Not tested" means exactly that.

## Machines

| Name | What | Details |
|---|---|---|
| host | Gentoo Linux x86_64, kernel 6.18.48, 8 CPUs, 7.6 GB RAM | QEMU 10.2.3 + KVM, LLVM 22 |
| obsd-vm | OpenBSD 8.0 GENERIC.MP#84 amd64 under QEMU/KVM on host | 6 vCPU, 4 GB, virtio disk/net, no GPU; created by `scripts/vm/install.py` |

## Results

| Date | Tier | Test | Machine | Result | Evidence |
|---|---|---|---|---|---|
| 2026-10-20 | infra | Release set signature + checksums (`scripts/fetch-openbsd-sets.sh`) | host | **pass** (12 files OK) | `logs/fetch-sets.log` |
| 2026-10-20 | infra | Pre-existing `base80.tgz` | host | **fail** — truncated (162 MB of 556 MB), moved to `openbsd-sets/8.0/base80.tgz.bad-checksum` | sha256 mismatch |
| 2026-10-20 | infra | Unattended install, attempt 1 | obsd-vm | **fail** — installer requires `BUILDINFO`; fixed in fetch script | `logs/vm-install.log` |
| 2026-10-20 | infra | Unattended install, attempt 2 | obsd-vm | **pass** — "CONGRATULATIONS", boots, ssh as builder/root works | `logs/vm-install.log` |
| 2026-10-20 | 6 | `tests/pe/basic.exe` compiles/links on host | host | **pass** (PE32+ x86-64, kernel32 imports only) | `make -C tests/pe` |
| — | 2 | `emulators/wine` build | obsd-vm | not tested yet | |
| — | 5 | `wineboot -i` | obsd-vm | not tested | |
| — | 6 | `wine basic.exe` | obsd-vm | not tested | |
| — | 7–11 | graphical, D3D, audio/input, Proton game, Steam | — | not tested | |

## Test programs

- `tests/pe/basic.c`: freestanding x86_64 console PE, TAP output, exit code =
  number of failures. Covers `%gs` TEB access (OpenBSD gs rewrite path),
  threads, VirtualAlloc/VirtualProtect to executable, temp-file I/O.
  Run: `wine basic.exe; echo $?` (expect six `ok` lines and 0).
