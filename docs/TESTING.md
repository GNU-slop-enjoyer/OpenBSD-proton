# Testing

Tiers follow CLAUDE.md §8 Stage G. Every result names the machine, revision,
command and log. "Not tested" means exactly that.

## Machines

| Name | What | Details |
|---|---|---|
| host | Gentoo Linux x86_64, kernel 6.18.48, 8 CPUs, 7.6 GB RAM | QEMU 10.2.3 + KVM, LLVM 22 |
| obsd-vm | OpenBSD 8.0 GENERIC.MP#84 amd64 under QEMU/KVM on host | 6 vCPU, 4 GB, virtio disk/net, no GPU; created by `scripts/vm/install.py` |

## Results

| Date | Tier | Test | Machine | Result | Evidence |
|---|---|---|---|---|---|
| 2026-10-20 | infra | Release set signature + checksums (`scripts/fetch-openbsd-sets.sh`) | host | **pass** (12 files OK) | `logs/fetch-sets.log` |
| 2026-10-20 | infra | Pre-existing `base80.tgz` | host | **fail** — truncated (162 MB of 556 MB), moved to `openbsd-sets/8.0/base80.tgz.bad-checksum` | sha256 mismatch |
| 2026-10-20 | infra | Unattended install, attempt 1 | obsd-vm | **fail** — installer requires `BUILDINFO`; fixed in fetch script | `logs/vm-install.log` |
| 2026-10-20 | infra | Unattended install, attempt 2 | obsd-vm | **pass** — "CONGRATULATIONS", boots, ssh as builder/root works | `logs/vm-install.log` |
| 2026-10-20 | 6 | `tests/pe/basic.exe` compiles/links on host | host | **pass** (PE32+ x86-64, kernel32 imports only) | `make -C tests/pe` |
| 2026-10-20 | 2 | `emulators/wine` (wine-11.19, ports -current) compile on OpenBSD 8.0 | obsd-vm | **pass** (full build + fake install, ~1h45 at -j6) | `logs/wine-11.19-build-1.log` |
| 2026-10-20 | 3 | `make package` against -current tree | obsd-vm | **fail** — WANTLIB mismatch: -current tree says glib-2.0.4201.17, 8.0 package provides .16 | `logs/wine-11.19-build-1.log` tail |
| 2026-10-20 | 3 | `make package` with 8.0 release ports tree (`ports.tar.gz`, signify-verified) + emulators/wine overlay | obsd-vm | **pass** → `wine-11.19p0.tgz` | `~builder/build-logs/emulators_wine.log` |
| 2026-10-20 | 4 | `pkg_add wine` (TRUSTED_PKG_PATH), `pkg_delete wine` (no files left), reinstall | obsd-vm | **pass** | session transcript |
| 2026-10-20 | 5 | `wine --version` → `wine-11.19` (build tree and installed package) | obsd-vm | **pass** | |
| 2026-10-20 | 5 | `wineboot -i` headless (no DISPLAY, mscoree/mshtml disabled) | obsd-vm | **pass** (rc 0, ~2 min, prefix created; expected errors: no X driver, no syswow64) | `~builder/wineboot.log` |
| 2026-10-20 | 6 | `wine basic.exe` (sha256 6e7e4722…) — %gs TEB access, threads, exec memory, file I/O | obsd-vm | **pass 6/6, exit 0** (build tree and installed package) | session transcript |
| 2026-10-20 | 2 | `emulators/proton-wine` (Valve wine e60892cc + 42 rebased patches) extract/patch/generate/configure | obsd-vm | **pass** | `~builder/build-logs/proton-wine-configure.log` |
| 2026-10-20 | 2 | `emulators/proton-wine` compile | obsd-vm | **pass** after 13 portability fixes (COMPATIBILITY.md) | `~builder/build-logs/emulators_proton-wine.log` |
| 2026-10-20 | 3/4 | `make package` → `proton-wine-11.0.20261008.tgz`; `pkg_add`/`pkg_delete`; installs in /usr/local/lib/proton-wine next to wine-11.19 | obsd-vm | **pass** | `~builder/build-logs/proton-wine-package.log` |
| 2026-10-20 | 5 | first run: `ntdll.so` failed to load (undefined `strchrnul`, `fill_performance_core_info`, `sysfs_parse_bitmap`) | obsd-vm | **fail → fixed** | `scripts/vm/check-undefined.sh` (control wine-11.19: clean) |
| 2026-10-20 | 5 | `wineboot -i`: secondary-thread stack overflow, boot event timeout (threads not registered for the signal relay) | obsd-vm | **fail → fixed** | `logs/proton-wine-runtime-2.log` (before) |
| 2026-10-20 | 5 | Proton wine `wine --version` → `wine-11.0`; `wineboot -i` | obsd-vm | **pass** (rc 0, 44 s; expected errors: no Vulkan device, no X driver for some services) | `logs/proton-wine-runtime-2.log` |
| 2026-10-20 | 6 | Proton wine `basic.exe` | obsd-vm | **pass 6/6** | `logs/proton-wine-runtime-2.log` |
| 2026-10-20 | 7 | Proton wine `gui.exe` under Xvfb | obsd-vm | **pass 5/5** | `logs/proton-wine-runtime-2.log` |
| 2026-10-20 | 1 | Port patch consistency: clean `make patch` in a separate WRKOBJDIR vs the tested tree (excluding build artifacts) | obsd-vm | **pass** (no differences) | session transcript |
| 2026-10-20 | — | `scripts/vm/check-undefined.sh` on proton-wine and wine-11.19 | obsd-vm | **pass** (no unresolved strong symbols) | |
| 2026-10-20 | 7 | `wine gui.exe` under Xvfb :1 (winex11.drv): RegisterClass, CreateWindowEx, WM_PAINT, GetPixel read-back, WM_TIMER loop | obsd-vm, wine-11.19p0 package | **pass 5/5, exit 0** | session transcript |
| 2026-10-21 | 2 | mingw-w64 toolchain prototype: headers, UCRT CRT, compiler-rt builtins, winpthreads, libunwind/libc++abi/libc++ | obsd-vm | **pass** after 2 fixes (missing clang `float.h` etc.; meson unused-argument probes) | COMPATIBILITY.md |
| 2026-10-21 | 2/3/4 | `devel/mingw-w64` port: build, fake, `make test`, package, `pkg_add` | obsd-vm | **pass** → `mingw-w64-13.0.0.tgz` (5237 PLIST entries) | `~builder/build-logs/devel_mingw-w64.log` |
| 2026-10-21 | 6 | C++17 test (`ports/devel/mingw-w64/files/hello.cpp`: libc++ containers, `std::thread`, exception through libunwind/SEH) built with the installed toolchain, `-static` and with libc++.dll; run under proton-wine | obsd-vm | **pass** (both exit 0, "caught exc", n=42) | session transcript |
| 2026-10-21 | 2 | DXVK 2.7.1 (`a6764047`) meson cross-build, first attempt | obsd-vm | **fail** — dxbc-spirv nested submodule missing; then `-lpthread` missing (winpthreads added) | `~builder/build-logs/dxvk.log` |
| 2026-10-21 | 2/3 | `graphics/dxvk` port: build, package (d3d8, d3d9, d3d10core, d3d11, dxgi; imports only system DLLs) | obsd-vm | **pass** → `dxvk-2.7.1.20260720.tgz`; extracted sources identical to recursive git checkout | `~builder/build-logs/graphics_dxvk*.log` |
| 2026-10-21 | 8 | `d3d11probe.exe` with wine's builtin d3d11/dxgi (wined3d) | obsd-vm, proton-wine | **expected failure**: CreateDXGIFactory1 and D3D11CreateDevice → 0x887a0004 (DXGI_ERROR_UNSUPPORTED), 0 adapters; no GPU | session transcript |
| 2026-10-21 | 8 | `d3d11probe.exe` with DXVK (`WINEDLLOVERRIDES=d3d11,dxgi=n`), first runs | obsd-vm, proton-wine p0 | **fail → fixed**: (1) page fault in winex11 `x11drv_init_wgl_extensions` (GL_EXTENSIONS use-after-free); (2) SIGABRT, "backwards memcpy" (overlapping memcpy in `strip_driver_extra`); (3) DXVK null instance access violation (singleton) | COMPATIBILITY.md |
| 2026-10-21 | 8 | `d3d11probe.exe` with packaged DXVK after fixes | obsd-vm, proton-wine-11.0.20261008p0, dxvk-2.7.1.20260720 | **pass (clean failure as expected without GPU)**: DXVK 2.7.1 loads, finds winevulkan, "Failed to create Vulkan instance"; both calls return E_FAIL, exit 1 | session transcript |
| 2026-10-21 | 2/3/4 | `devel/mingw-w64` p0 adds `x86_64-w64-mingw32-widl` (mingw-w64-tools); package/install | obsd-vm | **pass** (first p0 package lacked widl because the PLIST was stale; regenerated) | `~builder/build-logs/devel_mingw-w64.log` |
| 2026-10-21 | 2/3/4 | `graphics/vkd3d-proton` port (`212991fc`): build with mingw widl and glslang, package, `pkg_add` → d3d12.dll, d3d12core.dll (system imports only); build id = pinned commit | obsd-vm | **pass**; extracted sources identical to recursive git checkout (minus unused SPIRV-Cross/Tools) | `~builder/build-logs/graphics_vkd3d-proton.log` |
| 2026-10-21 | 1 | proton-wine patch consistency with the 2 new patches: clean `make WRKOBJDIR=… patch` (54 patches) vs tested tree | obsd-vm | **pass** (only build-generated `configure`, `config.*`, `po/*` differ) | session transcript |
| 2026-10-21 | 8 | `d3d12probe.exe`: wine builtin d3d12 → 0x80004005; vkd3d-proton + DXVK dxgi (`d3d12,d3d12core,dxgi=n`) → vkd3d-proton loads, "Failed to create DXGI factory", 0x80004005, exit 1 | obsd-vm | **pass (clean failure as expected without GPU)** | session transcript |
| — | 8 | DXVK / vkd3d-proton actually rendering (needs anv/radv on real hardware) | — | not tested | |
| — | 9–11 | audio/input, Proton game, Steam | — | not tested | |

## Test programs

- `tests/pe/basic.c`: freestanding x86_64 console PE, TAP output, exit code =
  number of failures. Covers `%gs` TEB access (OpenBSD gs rewrite path),
  threads, VirtualAlloc/VirtualProtect to executable, temp-file I/O.
  Run: `wine basic.exe; echo $?` (expect six `ok` lines and 0).
- `tests/pe/gui.c`: freestanding GUI PE (user32/gdi32): window creation,
  painting and pixel read-back, timer-driven message loop. Needs an X server:
  `Xvfb :1 -screen 0 1024x768x24 & DISPLAY=:1 wine gui.exe` (expect 5 `ok`).
- `tests/pe/d3d11probe.cpp` (`make -C tests/pe d3d`, needs
  `devel/mingw-w64`): enumerates DXGI adapters and creates a D3D11 hardware
  device, printing each HRESULT and which dxgi/d3d11 DLL was loaded. Exit 0
  only if a device was created.
- `tests/pe/d3d12probe.cpp`: D3D12CreateDevice at feature level 11_0;
  prints the HRESULT and the d3d12/d3d12core/dxgi DLLs in use.
- Wine conformance tests: `scripts/vm/winetest-build.sh` builds the
  `*_test.exe` files from a port's build tree (copy reconfigured without
  `--disable-tests`); `scripts/vm/winetest-run.sh` runs each test unit
  separately with a timeout and writes `summary.txt`.
