# Engineering decisions

## D1 — Test target: OpenBSD 8.0/amd64 in a user-mode QEMU/KVM VM (2026-10-20)

The development host is Gentoo Linux x86_64 with `/dev/kvm` accessible to the
user (group `kvm`). A headless VM gives a real OpenBSD kernel and userland
without root on the host. The VM is installed unattended by
`scripts/vm/install.py` from locally served, signify-verified sets.

8.0 rather than a -current snapshot: release sets and packages are stable and
reproducible, and the 8.0 package repository carries every build/library
dependency of `emulators/wine` (llvm-22.1.8, autoconf-2.73, automake-1.18,
nasm, samba, gtk+4, …). If the -current wine port turns out to need kernel or
libc features newer than 8.0, switch the VM to a snapshot and record it here.

Limitation: no GPU in the VM. Vulkan/D3D tests need real hardware.

## D2 — Base the port on OpenBSD's `emulators/wine`, not on Proton's build system (2026-10-20)

Proton's build (`upstream/Proton/configure.sh`, `Makefile.in`) runs inside the
Linux Steam Runtime SDK container through Docker/Podman, which OpenBSD lacks.
OpenBSD's `emulators/wine` (re-imported to ports-current 2026-10-19, wine 11.19,
sourced from `github.com/NethIafin/wine` branch `openbsd-master`) already solves
the hard platform work. The plan is:

1. Reproduce `emulators/wine` natively (baseline).
2. Port the PE-only Proton components (DXVK, vkd3d-proton) as separate ports;
   they don't depend on Valve's wine fork.
3. Evaluate rebasing the OpenBSD wine patches onto Valve's wine fork
   (`proton-11.0-2e`, submodule commit `e60892cc`) as a separate
   `emulators/proton-wine` port.

## D3 — Scope excludes 32-bit Windows binaries on stock kernels (2026-10-20)

See FEASIBILITY.md §2. Revisit only with a kernel patch or an x86 emulator.

## D4 — Separate VM SSH key (2026-10-20)

`vm/id_ed25519` is generated per checkout for the VM only. The user's personal
key is never copied into the guest. `vm/` is git-ignored.
