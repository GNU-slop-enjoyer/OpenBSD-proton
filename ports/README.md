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

Status: see docs/PROGRESS.md. Not yet compiled.
