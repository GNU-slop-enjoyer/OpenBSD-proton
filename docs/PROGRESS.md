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

### Next concrete actions
1. Finish `scripts/vm/provision.sh` (log: `logs/vm-provision.log`); check
   `pkg_add` output for missing packages.
2. In the VM: `cd /usr/ports/emulators/wine && make package 2>&1 | tee
   /home/wine-build.log`. Record the first failure, fix in a project patch
   directory (not in upstream/ports).
3. Install package, `wineboot -i`, copy `build/tests/pe/basic.exe` into the
   VM and run it (tier 5/6).
