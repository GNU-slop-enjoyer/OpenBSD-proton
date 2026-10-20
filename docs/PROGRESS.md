# Progress log

Newest first. Each entry: what changed, files, commands, results, failures,
blockers, next action.

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
