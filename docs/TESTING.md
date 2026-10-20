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
| 2026-10-20 | 2 | `emulators/wine` (wine-11.19, ports -current) compile on OpenBSD 8.0 | obsd-vm | **pass** (full build + fake install, ~1h45 at -j6) | `logs/wine-11.19-build-1.log` |
| 2026-10-20 | 3 | `make package` against -current tree | obsd-vm | **fail** — WANTLIB mismatch: -current tree says glib-2.0.4201.17, 8.0 package provides .16 | `logs/wine-11.19-build-1.log` tail |
| 2026-10-20 | 3 | `make package` with 8.0 release ports tree (`ports.tar.gz`, signify-verified) + emulators/wine overlay | obsd-vm | **pass** → `wine-11.19p0.tgz` | `~builder/build-logs/emulators_wine.log` |
| 2026-10-20 | 4 | `pkg_add wine` (TRUSTED_PKG_PATH), `pkg_delete wine` (no files left), reinstall | obsd-vm | **pass** | session transcript |
| 2026-10-20 | 5 | `wine --version` → `wine-11.19` (build tree and installed package) | obsd-vm | **pass** | |
| 2026-10-20 | 5 | `wineboot -i` headless (no DISPLAY, mscoree/mshtml disabled) | obsd-vm | **pass** (rc 0, ~2 min, prefix created; expected errors: no X driver, no syswow64) | `~builder/wineboot.log` |
| 2026-10-20 | 6 | `wine basic.exe` (sha256 6e7e4722…) — %gs TEB access, threads, exec memory, file I/O | obsd-vm | **pass 6/6, exit 0** (build tree and installed package) | session transcript |
| 2026-10-20 | 2 | `emulators/proton-wine` (Valve wine e60892cc + 42 rebased patches) extract/patch/generate/configure | obsd-vm | **pass** | `~builder/build-logs/proton-wine-configure.log` |
| 2026-10-20 | 2 | `emulators/proton-wine` compile | obsd-vm | **pass** after 13 portability fixes (COMPATIBILITY.md) | `~builder/build-logs/emulators_proton-wine.log` |
| 2026-10-20 | 3/4 | `make package` → `proton-wine-11.0.20261008.tgz`; `pkg_add`/`pkg_delete`; installs in /usr/local/lib/proton-wine next to wine-11.19 | obsd-vm | **pass** | `~builder/build-logs/proton-wine-package.log` |
| 2026-10-20 | 5 | first run: `ntdll.so` failed to load (undefined `strchrnul`, `fill_performance_core_info`, `sysfs_parse_bitmap`) | obsd-vm | **fail → fixed** | `scripts/vm/check-undefined.sh` (control wine-11.19: clean) |
| 2026-10-20 | 5 | `wineboot -i`: secondary-thread stack overflow, boot event timeout (threads not registered for the signal relay) | obsd-vm | **fail → fixed** | `logs/proton-wine-runtime-2.log` (before) |
| 2026-10-20 | 5 | Proton wine `wine --version` → `wine-11.0`; `wineboot -i` | obsd-vm | **pass** (rc 0, 44 s; expected errors: no Vulkan device, no X driver for some services) | `logs/proton-wine-runtime-2.log` |
| 2026-10-20 | 6 | Proton wine `basic.exe` | obsd-vm | **pass 6/6** | `logs/proton-wine-runtime-2.log` |
| 2026-10-20 | 7 | Proton wine `gui.exe` under Xvfb | obsd-vm | **pass 5/5** | `logs/proton-wine-runtime-2.log` |
| 2026-10-20 | 1 | Port patch consistency: clean `make patch` in a separate WRKOBJDIR vs the tested tree (excluding build artifacts) | obsd-vm | **pass** (no differences) | session transcript |
| 2026-10-20 | — | `scripts/vm/check-undefined.sh` on proton-wine and wine-11.19 | obsd-vm | **pass** (no unresolved strong symbols) | |
| 2026-10-20 | 7 | `wine gui.exe` under Xvfb :1 (winex11.drv): RegisterClass, CreateWindowEx, WM_PAINT, GetPixel read-back, WM_TIMER loop | obsd-vm, wine-11.19p0 package | **pass 5/5, exit 0** | session transcript |
| — | 8–11 | D3D (needs GPU), audio/input, Proton game, Steam | — | not tested | |

## Test programs

- `tests/pe/basic.c`: freestanding x86_64 console PE, TAP output, exit code =
  number of failures. Covers `%gs` TEB access (OpenBSD gs rewrite path),
  threads, VirtualAlloc/VirtualProtect to executable, temp-file I/O.
  Run: `wine basic.exe; echo $?` (expect six `ok` lines and 0).
- `tests/pe/gui.c`: freestanding GUI PE (user32/gdi32): window creation,
  painting and pixel read-back, timer-driven message loop. Needs an X server:
  `Xvfb :1 -screen 0 1024x768x24 & DISPLAY=:1 wine gui.exe` (expect 5 `ok`).
