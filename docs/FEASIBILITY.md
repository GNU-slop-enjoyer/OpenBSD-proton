# Feasibility — OpenBSD Proton

Evidence-based assessment. Every claim cites a source path or a test. Status
words: **verified** (read in source or tested), **inferred** (reasoned from
source, not yet tested), **untested**.

Source revisions examined (2026-10-20):

| Tree | Revision |
|---|---|
| upstream/Proton | `bc896217ff` (tag `proton-11.0-2e`), submodules not checked out |
| upstream/ports | `d12f48bbdb` (branch `master` = -current, 2026-10-20) |
| OpenBSD kernel source | `sys.tar.gz` (CVS checkout, segments.h v1.18 2025/06/27) |
| OpenBSD release | 8.0 amd64 sets, verified against `SHA256.sig` with `openbsd-80-base.pub` |

## 1. Native Wine on OpenBSD — feasible, already ported

`upstream/ports/emulators/wine` packages **wine-11.19** for amd64 with 42 OpenBSD
patches (verified, `emulators/wine/patches/`). Key OpenBSD adaptations:

- **`%gs` TEB access** (`patch-dlls_ntdll_unix_gs_openbsd_c`, +975 lines):
  OpenBSD gives userland no way to set the `%gs` base. Windows x86_64 code
  reaches the TEB through `%gs:`, so the port catches the resulting fault and
  rewrites the instruction (in place to `%fs:`, through a jmp stub, or by a
  per-hit SIGSEGV round-trip for instructions under 5 bytes). Proton inherits
  this unchanged. **Performance impact on games is untested.**
- Signal/context handling (`sig_openbsd.[ch]` in ntdll and wineserver),
  `virtual.c` (+416, W^X/mmap semantics), ptrace, sockets, nsiproxy.
- The port sets `USE_WXNEEDED` and `USE_NOBTCFI`: wine needs W|X mappings and
  can't follow IBT/BTI landing-pad rules.
- PE side is built by clang (`files/clang-pe`, `--with-mingw`); no mingw-gcc.
- Only `--enable-archs=x86_64`: no i386 PE code is built (see §2).

## 2. 32-bit Windows applications — BLOCKED by the kernel (verified)

Proton's wine builds with `--enable-archs=x86_64,i386` (new WoW64;
`upstream/Proton/Makefile.in:618-621`). WoW64 runs 32-bit PE code by
far-jumping to a 32-bit compatibility code segment.

OpenBSD amd64 GDT (`sys/arch/amd64/include/segments.h`): only
`GUDATA_SEL` (3) and `GUCODE_SEL` (4, 64-bit). `VALID_USER_CSEL(s)` accepts
only `GSEL(GUCODE_SEL, SEL_UPL)`. There is no 32-bit user code descriptor and
no `i386_set_ldt`/LDT interface on amd64. With no 32-bit code segment there is
nowhere to execute 32-bit x86 instructions natively.

Consequence: every 32-bit Windows game/app (much of the older catalogue, many
launchers) is blocked on a stock kernel. Options, none started:
1. Kernel patch adding a `GUCODE32_SEL` descriptor and accepting it in
   `VALID_USER_CSEL`/sigreturn (cf. FreeBSD `GUCODE32_SEL`). Not upstreamable
   without OpenBSD developers' agreement; also requires testing far-call/iretq
   paths.
2. A 32-bit x86 CPU emulator as wow64 backend (the role FEX plays for
   x86-on-arm64 in Proton). No x86-on-x86_64 wow64 emulator exists today.
3. Accept x86_64-only scope (current OpenBSD port behaviour).

## 3. Proton's build system — not portable as-is (verified)

`upstream/Proton/configure.sh` and `Makefile.in` build everything inside the
Steam Runtime "sniper" SDK container via Docker/Podman. OpenBSD has neither,
and the container is Linux userland. The pieces must be rebuilt as OpenBSD
ports instead. Components (`Makefile.in` `## ` sections) and their OpenBSD
relevance:

| Component | Type | Assessment |
|---|---|---|
| wine (Valve fork) | Unix + PE | Core. Rebase OpenBSD patches onto Valve's tree |
| dxvk | PE only | Builds with clang-pe/mingw; needs winevulkan + OpenBSD Vulkan driver |
| vkd3d-proton | PE only | As dxvk; needs D3D12-capable Vulkan features |
| vkd3d (upstream) | PE | Already built inside the wine port |
| dxvk-nvapi | PE | NVIDIA only; no NVIDIA driver on OpenBSD → drop |
| lsteamclient, steam.exe, steam_helper | Unix + PE | Bridge to the Linux Steam client. No native Steam client for OpenBSD → drop/replace |
| vrclient, openvr, wineopenxr, OpenXR | VR | No VR runtime on OpenBSD → drop |
| FEX | arm64 only | Not applicable to amd64 |
| gstreamer, ffmpeg, orc, dav1d, gst-plugins-rs | Unix | Use OpenBSD ports (already deps of wine port) |
| piper, kaldi, openfst, vosk | Unix | Speech (TTS/recognition); optional |
| BattlEye/EAC bridges | Unix | Linux-native anticheat runtimes → not available |
| proton (python script), default_pfx, fonts | data/script | Portable; script assumes Steam env vars |
| wine-mono, wine-gecko | PE | Prebuilt PE; portable |

## 4. Graphics — open question, needs hardware test

Wine port links `vulkan` (graphics/vulkan-loader) and builds with
`--with-vulkan`. DXVK requires Vulkan 1.3-class features. OpenBSD's Mesa
(xenocara) driver support for radv/anv on the actual test hardware is not yet
verified. The QEMU VM has no GPU passthrough → D3D/Vulkan tests need real
hardware or a software rasterizer (lavapipe) if Mesa on OpenBSD ships it.

## 5. Steam integration — not available

There is no OpenBSD Steam client. Proton's `proton` script is a Steam
compatibility tool launched by Steam with `STEAM_COMPAT_*` variables.
Realistic target: a standalone `proton`-style launcher that manages prefixes
and DXVK/vkd3d-proton installs for non-Steam Windows games (games from
DRM-free stores, or Steam games copied from another machine, subject to DRM).

## Overall

A "Proton-equivalent for OpenBSD" means: OpenBSD's native wine (or Valve's fork
with the OpenBSD patches) + DXVK + vkd3d-proton + a launcher. It is feasible
for 64-bit Windows applications if the Vulkan driver path works. Steam,
VR, NVIDIA-specific, anticheat and 32-bit applications are out of reach without
kernel or vendor changes.
