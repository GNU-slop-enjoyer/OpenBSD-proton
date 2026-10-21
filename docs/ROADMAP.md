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
- [x] Prerequisite: Windows-target C++ toolchain port `devel/mingw-w64` (mingw-w64 headers/CRT/winpthreads + compiler-rt, libunwind, libc++ for x86_64-w64-mingw32, on the installed llvm-22)
- [x] `graphics/dxvk` port at Proton's pinned `a6764047` (meson cross-build); packaged; runs up to Vulkan instance creation in the VM
- [ ] DXVK D3D11 rendering test on real hardware
- [x] `graphics/vkd3d-proton` port at `212991fc`; packaged; loads and fails cleanly without a Vulkan device
- [ ] Vulkan on real hardware (radv/anv in 8.0 xenocara); no lavapipe in 8.0, no GPU in the VM

## M3 — Valve wine fork (Stage C)
- [x] Fetch `ValveSoftware/wine` at `e60892cc`; patch-check the OpenBSD tree against it
- [x] Rebase OpenBSD patches onto Valve's fork (52 patches)
- [x] `emulators/proton-wine` port; builds, packages, passes tiers 5–7
- [x] Broader application tests with Proton wine: full winetest (p8: 230/278 units), real Windows programs (7-Zip, Notepad++, OpenTTD, SuperTuxKart)
- [x] OpenBSD-specific fixes found against a Linux reference build (debugging without ptrace, process/CPU info, gcrypt, getaddrinfo, %fs, cwm focus loops)

## M4 — Launcher (Stage F/G)
- [x] `proton`-equivalent launcher `emulators/proton-openbsd` (`proton-openbsd run …`): prefix setup, DXVK/vkd3d-proton installation, `PROTON_USE_WINED3D`/`PROTON_DXVK_D3D8`/`PROTON_NO_D3D10/11`/`PROTON_LOG`
- [x] wine-mono / wine-gecko for .NET and mshtml apps: `emulators/proton-wine-addons` (Proton's versions)
- [ ] Application test matrix (TESTING.md) — tiers 5–10 done in the VM; real-hardware Vulkan pending

## M5 — Performance and remaining platform gaps
- [ ] In-process synchronization for kernel objects. Measured (`tests/pe/syncbench.c`):
      a wait on a signaled event costs 17 µs on OpenBSD (every wait/set is a wineserver
      round trip), 4.8 µs with server-side sync on Linux and 0.12 µs with Proton's fsync.
      The raw pipe round trip is 2.5× slower in the OpenBSD VM than on the Linux host, so
      the server path cannot get close; fsync needs Linux futex_waitv, esync needs eventfd.
      Candidate: an esync-style backend on OpenBSD primitives (shared memory + futex(2)
      for single-object waits, kqueue/pipes for multi-object waits), behind an opt-in
      environment variable until it passes the sync winetests. In-process locks
      (SRW/condition variables/WaitOnAddress, kqueue EVFILT_USER) are already 6 µs vs 4 µs.
- [ ] Directory reparse points (symlinks/junctions to directories) without xattrs
- [ ] Rare silent SIGABRT (likely pinsyscalls) in the %gs fault path
- [ ] Sending the emulators/wine-11.19 bundle upstream (needs the owner's OK)

## Out of scope unless something changes
- 32-bit PE (kernel lacks a 32-bit user code segment; FEASIBILITY.md §2)
- Steam client integration, VR, NVIDIA nvapi, anticheat bridges
