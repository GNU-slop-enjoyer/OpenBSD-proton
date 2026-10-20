# Roadmap

Ordered by what unblocks the next stage. See PROGRESS.md for current status.

## M0 — Infrastructure (Stage A)
- [x] Verified OpenBSD 8.0 set fetch (`scripts/fetch-openbsd-sets.sh`, signify check in Python)
- [ ] Unattended OpenBSD VM install (`scripts/vm/install.py`) and control (`scripts/vm/run.sh`)
- [ ] Guest provisioning: packages, ports tree, mk.conf (`scripts/vm/provision.sh`)

## M1 — Native wine baseline (Stage B)
- [ ] Build `emulators/wine` (ports -current) on OpenBSD 8.0 in the VM; record failures
- [ ] Package + install; `wine --version`, `wineboot -i` (prefix creation)
- [ ] Tier-6 test: minimal x86_64 console PE (cross-built on host with clang/mingw target)
- [ ] Tier-7 test: graphical PE under Xvfb in the VM

## M2 — PE graphics layers as ports (Stage E, independent of Valve wine)
- [ ] `emulators/dxvk` port at Proton's pinned `a6764047`: PE-only meson cross-build with clang-pe
- [ ] `emulators/vkd3d-proton` port at `212991fc`
- [ ] Vulkan availability on real hardware (radv/anv on OpenBSD 8.0); lavapipe as fallback in VM

## M3 — Valve wine fork (Stage C)
- [ ] Fetch `ValveSoftware/wine` at `e60892cc`; diff against wine-11.0 / the OpenBSD tree
- [ ] Rebase OpenBSD patches (`emulators/wine/patches`, NethIafin/wine `openbsd-master`) onto Valve's fork
- [ ] `emulators/proton-wine` port; x86_64-only archs

## M4 — Launcher (Stage F/G)
- [ ] `proton`-equivalent launcher: prefix setup, DXVK/vkd3d-proton installation, env vars
- [ ] Application test matrix (TESTING.md)

## Out of scope unless something changes
- 32-bit PE (kernel lacks a 32-bit user code segment; FEASIBILITY.md §2)
- Steam client integration, VR, NVIDIA nvapi, anticheat bridges
