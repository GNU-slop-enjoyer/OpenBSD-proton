# Ports overlay

Port definitions in OpenBSD ports-tree layout (`category/name`). Copy a port
directory into `/usr/ports/<category>/` on an OpenBSD system (or the VM, see
docs/BUILDING.md) and build it with the usual `make package`.

## emulators/proton-wine

- Source: `github.com/ValveSoftware/wine` commit `e60892cc23`, the `wine`
  submodule of Proton `proton-11.0-2e` (`ValveSoftware/Proton` `bc896217ff`).
- Patches: OpenBSD support from the OpenBSD ports tree's `emulators/wine`
  (ports `master` `d12f48bbdb`, wine-11.19; maintainers Thomas Frohwein and
  Andrii Yukhymchak), whose code originates from
  `github.com/NethIafin/wine` branch `openbsd-master` (`33de78813f`).
  Rebased onto Valve's tree in `work/proton-port/wine` (local branch
  `openbsd`) and exported with `scripts/make-port-patches.sh`. Patches
  without a header come unchanged from emulators/wine; rebased patches
  carry a header explaining the 11.0-specific changes.
- License: Wine and these patches are LGPL 2.1 or later.
- Generated sources (`server_protocol.h`, syscall tables, winevulkan
  thunks) are created in `post-patch`, as Proton's own build does.

Status: builds, packages and passes tests tiers 5–7 on OpenBSD 8.0 amd64
(docs/TESTING.md). Build against the 8.0 release ports tree (docs/DECISIONS.md D5).
Two patches are backports of later upstream wine fixes (headers name the
commits).

## emulators/proton-openbsd

- Project-owned launcher (`files/proton-openbsd.sh`, man page
  `files/proton-openbsd.1`); no distfiles. Mirrors the DLL setup and
  `PROTON_*` switches of upstream Proton's `proton` script
  (proton-11.0-2e) without Steam. RUN_DEPENDS on proton-wine, dxvk and
  vkd3d-proton.
- License: LGPL 2.1 or later.

## emulators/proton-wine-addons

- Prebuilt upstream Windows binaries: wine-mono 11.2.0
  (github.com/madewokherd/wine-mono release asset) and wine-gecko 2.47.4
  x86_64 (dl.winehq.org), the versions Proton proton-11.0-2e bundles,
  unpacked into proton-wine's data dir as Proton does. Neither project
  publishes hashes for these tarballs; distinfo records what was fetched
  over HTTPS.
- License: Wine Mono MIT/X11 and others; Wine Gecko MPL 2.0.

## fonts/proton-fonts

- Source: `ValveSoftware/Proton` `bc896217ff` (proton-11.0-2e) `fonts/`
  (Noto fonts, patches, merge script) and its `fonts/liberation-fonts`
  submodule `4b01920461`. Build steps mirror Proton's Makefile.in
  (`files/build-fonts.sh`).
- Not included: the CJK fonts (need afdko).
- License: SIL OFL 1.1 (fonts); merge.py Apache 2.0/MIT.

## devel/mingw-w64

- Sources: mingw-w64 v13.0.0 release tarball (SourceForge; its detached
  signature verifies against key CAF5641F74F7DFBA88AE205693BDB53CD4EBC740,
  "JonY"), and `llvm-project-22.1.8.src.tar.xz` (same distfile and checksum
  as `devel/llvm/22`).
- Builds target libraries only; the compiler is the installed llvm-22
  (docs/DECISIONS.md D8). Build logic: `files/mingw-w64-build.sh`.
- License: mingw-w64 ZPL 2.1 / public domain, winpthreads MIT, LLVM
  runtimes Apache 2.0 with LLVM exceptions.

## graphics/dxvk

- Source: `github.com/doitsujin/dxvk` commit `a6764047e5` (2.7.1+), the
  `dxvk` submodule of Proton `proton-11.0-2e`, plus its submodules at their
  pinned commits (see Makefile).
- Patch: `src/util/util_singleton.h` — null instance after a failed first
  construction (not fixed upstream as of 2026-10-21; candidate to send).
- License: zlib (dxvk), MIT and LGPL 2.1+ for bundled headers.

## graphics/vkd3d-proton

- Source: `github.com/HansKristian-Work/vkd3d-proton` commit `212991fc2c`
  (3.0+), the `vkd3d-proton` submodule of Proton `proton-11.0-2e`, plus the
  submodules its meson build uses (dxil-spirv's SPIRV-Cross/SPIRV-Tools are
  CMake-only and left out).
- No patches; `post-patch` sets meson's vcs_tag fallback to the commit so
  the build id (shader cache key) is right without `.git`.
- License: LGPL 2.1+ (vkd3d-proton), MIT/zlib/Apache for bundled code.
