# OpenBSD Proton

Valve's Proton stack — Proton's Wine fork, DXVK, vkd3d-proton, Proton's
fonts, Wine Mono/Gecko and a `proton`-style launcher — ported to OpenBSD
as ports-tree packages, for running 64-bit Windows programs and games on
OpenBSD/amd64.

This is a work in progress developed and tested in an OpenBSD 8.0 amd64
virtual machine without a GPU. Read [docs/TESTING.md](docs/TESTING.md) for
exactly what has been run and with which result; nothing below claims more.

## What works (tested in the VM)

- Proton's wine 11.0 (`ValveSoftware/wine` e60892cc) built natively on
  OpenBSD with the OpenBSD port's patches plus fixes from this project
  (`ports/emulators/proton-wine`, 70+ patches).
- 64-bit console and GUI programs, .NET programs (Wine Mono), the HTML
  engine (Wine Gecko); 7-Zip, Notepad++; the games OpenTTD and SuperTuxKart
  (OpenGL on Mesa llvmpipe, keyboard/mouse, PulseAudio sound) and GZDoom
  with its Vulkan renderer (on lavapipe).
- Direct3D 9/10/11 through wine's wined3d (OpenGL): the D3D11 conformance
  tests pass. DXVK renders Direct3D 11 and vkd3d-proton renders Direct3D 12 on
  Mesa's software Vulkan (lavapipe, built for testing), in windows with
  swap chains too; on real
  radv/anv hardware they are **not tested yet**.
- fsync — Proton's in-process synchronization — reimplemented on
  OpenBSD's futex(2) (waits on Windows objects: 0.35 µs instead of 17 µs).
- 226 of 278 Wine conformance test units pass with fsync (p16); the remaining failures
  are explained in [docs/TESTING.md](docs/TESTING.md) and
  [docs/COMPATIBILITY.md](docs/COMPATIBILITY.md).

## What does not work

- 32-bit Windows programs (OpenBSD gives user processes no 32-bit code
  segment). Many installers of 64-bit programs are 32-bit; use an `.msi` or
  a portable archive.
- Steam, Steam's runtime, anti-cheat, VR, NVIDIA-specific libraries.
- Dual-stack (IPv4-mapped IPv6) sockets, DOS hidden/system file attributes
  (no extended attributes on OpenBSD), some socket options.

## Packages

| Port | Package |
|---|---|
| `ports/emulators/proton-wine` | Proton's wine in `/usr/local/lib/proton-wine` |
| `ports/emulators/proton-wine-addons` | Wine Mono 11.2.0, Wine Gecko 2.47.4 |
| `ports/fonts/proton-fonts` | Proton's replacement Windows fonts (incl. CJK) |
| `ports/graphics/dxvk` | DXVK 2.7.1 (Proton's revision) |
| `ports/graphics/vkd3d-proton` | vkd3d-proton 3.0 (Proton's revision) |
| `ports/devel/mingw-w64` | mingw-w64 toolchain (builds DXVK/vkd3d-proton) |
| `ports/emulators/proton-openbsd` | the `proton-openbsd` launcher |

Building and installing: [docs/BUILDING.md](docs/BUILDING.md).
Using: [docs/USING.md](docs/USING.md) and `proton-openbsd(1)`.

`upstream-submissions/emulators-wine` holds the fixes that also apply to
OpenBSD's own `emulators/wine` (11.19), prepared for submission.

## Project documents

[PROGRESS](docs/PROGRESS.md) (current state and next steps) ·
[FEASIBILITY](docs/FEASIBILITY.md) · [ARCHITECTURE](docs/ARCHITECTURE.md) ·
[DEPENDENCIES](docs/DEPENDENCIES.md) · [COMPATIBILITY](docs/COMPATIBILITY.md) ·
[TESTING](docs/TESTING.md) · [ROADMAP](docs/ROADMAP.md) ·
[DECISIONS](docs/DECISIONS.md)

## Licenses

Each port records the licenses of what it packages (`PERMIT_*` and the
comments in its Makefile; installed license files under
`share/doc/<package>`): Wine and the wine patches LGPL-2.1-or-later, DXVK
zlib, vkd3d-proton LGPL-2.1, Wine Mono/Gecko and the fonts as listed in
their ports. No license has been chosen yet for this repository's own
scripts and documents.
