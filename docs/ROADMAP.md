# Roadmap

Ordered by what unblocks the next stage. See PROGRESS.md for current status.

## M0 — Infrastructure (Stage A)
- [x] Verified OpenBSD 8.0 set fetch (`scripts/fetch-openbsd-sets.sh`, signify check in Python)
- [x] Unattended OpenBSD VM install (`scripts/vm/install.py`) and control (`scripts/vm/run.sh`)
- [x] Guest provisioning: packages, ports tree, mk.conf (`scripts/vm/provision.sh`)

## M1 — Native wine baseline (Stage B)
- [x] Build `emulators/wine` (ports -current) on OpenBSD 8.0 in the VM (packaging needs the 8.0 ports tree)
- [x] Package + install; `wine --version`, `wineboot -i` (prefix creation)
- [x] Tier-6 test: minimal x86_64 console PE (`tests/pe/basic.c`)
- [x] Tier-7 test: graphical PE under Xvfb in the VM (`tests/pe/gui.c`)

## M2 — PE graphics layers as ports (Stage E, independent of Valve wine)
- [ ] Prerequisite: Windows-target C++ toolchain port (mingw-w64 headers/CRT + compiler-rt, libunwind, libc++ for x86_64-w64-mingw32, built with clang/lld) — none in ports (FEASIBILITY.md §4a)
- [ ] `emulators/dxvk` port at Proton's pinned `a6764047` (meson cross-build)
- [ ] `emulators/vkd3d-proton` port at `212991fc`
- [ ] Vulkan on real hardware (radv/anv in 8.0 xenocara); no lavapipe in 8.0, no GPU in the VM

## M3 — Valve wine fork (Stage C)
- [x] Fetch `ValveSoftware/wine` at `e60892cc`; patch-check the OpenBSD tree against it
- [x] Rebase OpenBSD patches onto Valve's fork (52 patches)
- [x] `emulators/proton-wine` port; builds, packages, passes tiers 5–7
- [ ] Broader application tests with Proton wine (real Windows programs, winetest subsets)

## M4 — Launcher (Stage F/G)
- [ ] `proton`-equivalent launcher: prefix setup, DXVK/vkd3d-proton installation, env vars
- [ ] Application test matrix (TESTING.md)

## Out of scope unless something changes
- 32-bit PE (kernel lacks a 32-bit user code segment; FEASIBILITY.md §2)
- Steam client integration, VR, NVIDIA nvapi, anticheat bridges
