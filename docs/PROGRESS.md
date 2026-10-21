# Progress log

Newest first. Each entry: what changed, files, commands, results, failures,
blockers, next action.

## 2026-10-21 — Session 2: Windows toolchain, DXVK, vkd3d-proton, launcher, winetest

### What changed
- **`ports/devel/mingw-w64`** (new): x86_64-w64-mingw32 C/C++ toolchain on
  the installed llvm-22 — mingw-w64 v13.0.0 headers, UCRT CRT,
  winpthreads, widl; LLVM 22.1.8 compiler-rt builtins, libunwind,
  libc++abi, libc++. Packaged (`mingw-w64-13.0.0p0`), `make test` passes,
  static and shared C++ programs run under proton-wine.
- **`ports/graphics/dxvk`** (new): DXVK 2.7.1 at Proton's `a6764047`, one
  patch (singleton null instance). Packaged.
- **`ports/graphics/vkd3d-proton`** (new): vkd3d-proton at Proton's
  `212991fc`. Packaged.
- **`ports/emulators/proton-openbsd`** (new): `proton-openbsd run prog.exe`
  launcher + man page; installs DXVK/vkd3d-proton into the prefix like
  Proton's `setup_prefix()`, `PROTON_*` switches. Packaged.
- **`emulators/proton-wine` p0**: 2 backported upstream wine fixes
  (GL_EXTENSIONS use-after-free; overlapping memcpy that OpenBSD libc
  aborts on). 54 patches; clean `make patch` reproduces the tested tree.
- Tests: `tests/pe/d3d11probe.cpp`, `d3d11render.cpp`, `d3d12probe.cpp`;
  `scripts/vm/winetest-build.sh` / `winetest-run.sh` (Wine conformance
  tests from the port build tree); `scripts/debug/memcpy-overlap.c`.
- Docs: FEASIBILITY §4/4a, COMPATIBILITY (runtime bugs, toolchain issues),
  DECISIONS D8/D9, DEPENDENCIES, BUILDING §7–9, TESTING, ROADMAP.
- Commits on `stage-a/openbsd-vm-baseline`: `7db04ac`, `39febb5`, `c487e72`,
  `4f15498` (pushed).

### Key results (OpenBSD 8.0 VM, no GPU)
- **Direct3D 11 renders** through proton-wine with wined3d (OpenGL on Mesa
  llvmpipe, Xvfb): `d3d11render.exe` 8/8.
- DXVK and vkd3d-proton load and fail cleanly at Vulkan instance/device
  creation (no Vulkan device in the VM) — rendering with them needs real
  Intel/AMD hardware.
- Winetest (20 DLL test suites, 252 units, test binaries from the 11.0
  tree, partly run concurrently with port builds): proton-wine p0 187 pass /
  47 fail / 12 crash / 6 timeout; OpenBSD's wine-11.19 on the same
  binaries 183 / 52 / 13 / 4. 57 units fail with both (OpenBSD/wine-port
  limitations or 11.0-vs-11.19 test differences), 8 only with proton-wine:
  `kernel32 drive` (bug, fixed in p1), `kernel32 path` (Proton's Steam
  search path, expected), and ntdll om, user32 scroll, comctl32
  button/treeview, advapi32 crypt, crypt32 protectdata (being checked
  against the p1 run).
- **ABI bug found and fixed (p1):** narrow (BOOLEAN/WORD) syscall arguments
  reach clang-compiled Unix code with garbage upper bits; 44 syscalls /
  51 arguments normalised (COMPATIBILITY.md). OpenBSD's emulators/wine has
  the same latent bug: a 46-syscall / 53-argument patch set for wine-11.19
  is in `upstream-submissions/emulators-wine/` (not yet built/tested).
  (Commit `4f15498`'s message says "57 narrow arguments"; the correct count
  is 51.)

### Mistakes caught this session
- Wait loops using `pgrep -f`/`pkill -f` matched their own ssh command
  line (never finished / killed the session); use bracketed patterns and
  keep kill and launch commands in separate ssh calls.
- OpenBSD `grep` has no `\|` in basic regexps: several searches silently
  found nothing until switched to `grep -E`.
- `/etc/mk.conf` assigns `WRKOBJDIR`, so the environment variable is
  ignored; pass `WRKOBJDIR=` on the make command line.
- The first mingw-w64 p0 package lacked widl (stale PLIST; pkg_create
  ignores unlisted files). Always `make update-plist` after changing what
  a port installs.

### VM state (end of session 2)
- Installed: proton-wine-11.0.20261008p1, mingw-w64-13.0.0p0,
  dxvk-2.7.1.20260720, vkd3d-proton-3.0.20260729, proton-openbsd-0.1p0,
  wine-11.19p0, plus cmake/ninja/meson/glslang. Local unsigned packages in
  /home/packages (install with `TRUSTED_PKG_PATH`).
- Winetest: binaries in `~builder/winetest/bin`, results in
  `~builder/winetest/out-proton` (p0), `out-1119` (wine-11.19), `out-p1`.
  `~builder/winetest-run.sh`, `winetest-build.sh`.
- Test programs in `~builder/tests-pe/out`; launcher prefix
  `~builder/pfx-launch`.
- Xvfb on :1 must be restarted after a reboot.

### Late session 2 (after the handoff above)
- proton-wine p2 (socket option fixes), fonts/proton-fonts, launcher
  0.1p2 (fonts linked before first boot): committed up to `d7b5e9f`.
- Proton-only winetest failures now explained: kernel32 path, ntdll om
  (Proton's GlobalRoot prefix stripping in ntdll file.c), advapi32 crypt
  / crypt32 protectdata (SteamGameId hacks); comctl32/user32 message
  sequences unverified.
- **In progress when the session ended:** emulators/wine-11.19p1 build in
  the VM with `upstream-submissions/emulators-wine/patches` (log
  `~builder/build-logs/emulators_wine-narrow.log`; original patches kept in
  `/usr/ports/emulators/wine/patches.orig`, REVISION bumped locally only).
  Next: install it, run `kernel32_test drive`/winetest, update the
  submission README.
- Open: wineserver `IOCTL_AFD_WINE_CONNECT` (server/sock.c) should
  recreate the Unix socket when OpenBSD refuses connect() after a failed
  attempt (EINVAL); afdko port for Proton's CJK fonts.

### Next concrete actions
1. Finish the p1 winetest comparison; explain or fix the remaining
   proton-only units (ntdll om, user32 scroll, comctl32 button/treeview,
   advapi32 crypt, crypt32 protectdata); re-run timing-sensitive units on
   an idle VM.
2. Build and test emulators/wine-11.19 with
   `upstream-submissions/emulators-wine/patches` (≈2 h build), then
   consider sending it (ports@openbsd.org / NethIafin/wine) — needs the
   user's OK, as it is outward-facing.
3. Investigate crashes common to both wines (`ntdll exception`,
   `kernel32 file`, `msvcrt misc`, `ole32 marshal`, `ws2_32 sock`, …) —
   they are OpenBSD-specific candidates (gs emulation, signals, sockets).
4. Real-hardware Vulkan test of DXVK/vkd3d-proton (needs an OpenBSD
   machine with Intel/AMD GPU) — not possible in this environment.
5. Real 64-bit Windows applications (needs the user's OK to download
   third-party binaries).
6. wine-mono / wine-gecko packaging (prebuilt PE binaries; same OK).

## 2026-10-20 — Session 1: environment, OpenBSD VM, baseline build start

### Environment (verified)
- Host: Gentoo Linux x86_64, kernel 6.18.48, 8 CPUs, 7.6 GB RAM (≈3.6 GB
  free), 319 GB free on /home. QEMU 10.2.3, `/dev/kvm` usable (user in
  `kvm` group). LLVM/clang 22 with lld-link and llvm-dlltool. No mingw, no
  wine, no signify, no gh on the host.
- Git: remote `origin` = `git@github.com:GNU-slop-enjoyer/OpenBSD-proton.git`;
  SSH auth works and `git push --dry-run` succeeds (write access).
  Work happens on branch `stage-a/openbsd-vm-baseline`.
- Sources: `upstream/Proton` @ `bc896217` (`proton-11.0-2e`, submodules NOT
  initialised); `upstream/ports` @ `d12f48bb` (master / -current,
  2026-10-20). No upstream/src or xenocara checkouts; `src.tar.gz` and
  `sys.tar.gz` (CVS checkouts) at top level, untracked and git-ignored.
- OpenBSD 8.0 amd64 sets: all fetched and signify-verified. The pre-existing
  `base80.tgz` was truncated → kept as `openbsd-sets/8.0/base80.tgz.bad-checksum`.

### Implemented
- `scripts/signify-verify.py`: Python signify verifier (Ed25519 + SHA256 list).
- `scripts/fetch-openbsd-sets.sh`: resumable fetch + verification.
- `scripts/vm/install.py` + `install.conf.in`: fully unattended OpenBSD
  install into qcow2 as an unprivileged user (boot loader driven via QEMU
  monitor sendkey → serial console → autoinstall from loopback HTTP).
- `scripts/vm/run.sh`: start/stop/status/ssh for the VM.
- `scripts/vm/provision.sh`: wine build deps from 8.0 packages, ports tree
  copy to /usr/ports, mk.conf, doas for builder, login class limits.
- `tests/pe/`: freestanding x86_64 PE test (no mingw needed).
- Docs: FEASIBILITY, DEPENDENCIES, DECISIONS, ROADMAP, BUILDING, TESTING.

### Key findings
- **32-bit Windows binaries are blocked by the OpenBSD amd64 kernel**: no
  32-bit user code segment (`segments.h`: only GUCODE_SEL, `VALID_USER_CSEL`).
  Matches the port's README ("Only 64-bit Windows applications").
- OpenBSD wine port (wine-11.19, re-imported to -current 2026-10-19, source
  `github.com/NethIafin/wine` `openbsd-master`) emulates `%gs` TEB access by
  fault-and-rewrite: every Windows binary depends on it.
- No wine package in 8.0; all its deps are in 8.0 packages.
- Proton's build requires the Linux Steam Runtime container → must be
  re-expressed as OpenBSD ports (D2).

### Test results
See TESTING.md. VM install: pass (2nd attempt). basic.exe: compiled, not run.

### Later in session 1
- VM provisioned (`scripts/vm/provision.sh`, 165 packages from 8.0). First
  `make package` of emulators/wine failed: `Fatal: /home/pobj must be on a
  wxallowed filesystem` (USE_WXNEEDED). Fixed by mounting /home `wxallowed`
  in the guest (now part of provision.sh). Rebuild running; fetch, the 42
  OpenBSD patches, autoconf and configure all succeeded.
- No mingw-w64 in ports; Mesa 8.0 ships anv/hasvk/radv Vulkan ICDs but no
  lavapipe (FEASIBILITY.md §4/§4a).
- dxvk `a6764047` fetched as Proton submodule (shallow).
- Valve wine `e60892cc` fetched (shallow submodule). Patch check against it:
  12 clean / 18 fuzz / 12 failed (30 of 80 hunks). All rejects resolved in
  `work/proton-port/wine` branch `openbsd` (local clone) — details in
  COMPATIBILITY.md. NethIafin/wine history in `upstream/nethiafin-wine`
  (blobless clone) used to understand each change.
- New port `ports/emulators/proton-wine` (GH distfile + Vulkan registry,
  patches, generation in post-patch) — see results below.
- Commits on `stage-a/openbsd-vm-baseline`: `ca18f33`, `df3aab9` (pushed).

### Results at end of session 1 (all on OpenBSD 8.0 amd64, QEMU/KVM VM)
- emulators/wine-11.19 (ports -current): built, packaged (8.0 ports tree),
  installed/removed; `wineboot -i`, basic.exe 6/6, gui.exe 5/5 under Xvfb.
- **emulators/proton-wine (Valve wine `e60892cc`, Proton 11.0-2e): built,
  packaged, installed; `wineboot -i` OK, basic.exe 6/6, gui.exe 5/5.**
  52 ports patches reproduce the tested tree exactly (clean `make patch`
  diff). Fixes beyond the rebase are listed in COMPATIBILITY.md.
- Mistake caught by testing: the rebase dropped `add_signal_tid()` for
  secondary threads, so their `%gs` faults went unhandled (stack overflow
  in wineboot). Fixed; OpenBSD clang only *warns* on implicit declarations,
  which also hides undeclared calls in emulators/wine (8 warnings,
  ABI-harmless today: void/BOOL returns).

### VM state (for the next session)
- `scripts/vm/run.sh start` boots it; `/usr/ports` = 8.0 tree + overlays,
  `/usr/ports-current` = -current copy. Installed: wine-11.19p0,
  proton-wine-11.0.20261008 (unsigned local packages in /home/packages).
- Prefixes: `~builder/pfx-pkg` (wine), `~builder/pfx-proton` (proton-wine).
  Test binaries `~builder/basic.exe`, `~builder/gui.exe`; Xvfb on :1 must be
  restarted after a reboot.

### Next concrete actions
1. Application tests beyond the synthetic PEs with proton-wine: real
   64-bit Windows programs (e.g. 7-Zip x64 installer/CLI, Notepad++ x64
   portable), wine's own `programs/` (cmd, reg, regedit under Xvfb) and a
   subset of winetest (`--disable-tests` must be dropped for that, or use
   upstream winetest.exe x64). Record results per program.
2. Windows-target C++ toolchain port (llvm-mingw style) — prerequisite for
   building DXVK/vkd3d-proton from source (FEASIBILITY.md §4a).
3. Real-hardware Vulkan check (anv/radv) — needs a physical OpenBSD
   machine with an Intel or AMD GPU; not available in this environment.
4. Upstreaming candidates: the implicit-declaration fixes and the
   add_signal_tid declaration issue apply to emulators/wine as well.
5. `proton`-style launcher script (prefix management, env vars) once a
   graphics layer exists.
6. Minor: VM console shows `reorder_kernel: failed` at boot (KARL relink;
   see /usr/share/relink/kernel/GENERIC.MP/relink.log in the guest).
   Possibly from the one hard power-off (run.sh stop bug, fixed).
