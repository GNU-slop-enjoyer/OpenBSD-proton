# Dependency inventory

Status legend: **pkg-8.0** = binary package exists in
`cdn.openbsd.org/pub/OpenBSD/8.0/packages/amd64` (checked 2026-10-20 against
index.txt); **port** = in `upstream/ports` (-current); **n/a** = not applicable to
OpenBSD; **blocked** = needs a missing interface.

## emulators/wine (baseline, wine-11.19)

From `upstream/ports/emulators/wine/Makefile`.

| Dependency | Kind | Status |
|---|---|---|
| lang/clang (MODCLANG 22) + wine-clang-pe-headers-22.1.8 | build | pkg-8.0 `llvm-22.1.8p10`; headers fetched from thfr.info |
| devel/bison, devel/nasm | build | pkg-8.0 |
| autoconf-2.73, automake-1.18 | build | pkg-8.0 |
| gmake | build | pkg-8.0 |
| audio/pulseaudio | lib | pkg-8.0 `pulseaudio-17.0p4` |
| devel/sdl2 | lib | pkg-8.0 |
| graphics/ffmpeg | lib | pkg-8.0 `ffmpeg-9.0.2v1` |
| graphics/vulkan-loader | lib | pkg-8.0 `1.4.357.0` |
| multimedia/gstreamer1 core/base/good/bad/ugly/libav | lib/run | pkg-8.0 `gstreamer1-1.28.7` |
| net/samba,-main (netapi) | lib | pkg-8.0 `samba-4.25.0v0` |
| security/gnutls, pcsc-lite; print/cups,-libs; graphics/sane-backends, libgphoto2; net/libpcap; devel/libinotify; x11/dbus, xkbcommon | lib | pkg-8.0 |
| x11/gtk+4 | run | pkg-8.0 |

There is **no wine package in 8.0**: the port was re-imported to -current on
2026-10-19 (`upstream/ports` commit `e441aff3de7`).

## Proton components (proton-11.0-2e)

| Component | Submodule commit | OpenBSD status |
|---|---|---|
| wine (Valve fork) | `e60892cc` | not fetched; needs OpenBSD patch rebase |
| dxvk | `a6764047` | not fetched; PE-only, needs port |
| vkd3d-proton | `212991fc` | not fetched; PE-only, needs port |
| vkd3d | `1128125f` | upstream vkd3d already bundled in wine |
| Vulkan-Headers, SPIRV-Headers, glslang | — | pkg-8.0 (vulkan-headers 1.4.357.0); glslang to check |
| gstreamer, ffmpeg, orc, dav1d, graphene, libsoup | — | use OpenBSD ports |
| gst-plugins-rs | — | to check (rust in ports) |
| dxvk-nvapi | `f9fdf268` | n/a (no NVIDIA driver) |
| lsteamclient, steam_helper, vrclient, openvr, OpenXR, wineopenxr | — | n/a (no Steam/VR on OpenBSD) |
| FEX | `1cc4b93e` | n/a (arm64 only) |
| piper, kaldi, openfst, vosk | — | optional (speech); not started |
| BattlEye/EAC bridges | — | n/a (Linux binaries) |
| wine-mono, wine-gecko | — | prebuilt PE; to check versions |

## Kernel interfaces

| Interface | Needed by | Status |
|---|---|---|
| 32-bit user code segment (WoW64) | 32-bit PE | **blocked** — see FEASIBILITY.md §2 |
| user-settable `%gs` base | Windows TEB | absent; emulated by fault-and-rewrite patch |
| W\|X mappings | wine loader/JIT | allowed via `USE_WXNEEDED` (wxallowed mount) |
