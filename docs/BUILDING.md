# Building

All OpenBSD-side builds run inside a real OpenBSD VM (docs/DECISIONS.md D1).
Nothing in this procedure needs root on the host. Root is used only inside the guest.

## Host requirements

- x86_64 Linux (tested: Gentoo, kernel 6.18) with `/dev/kvm` writable by the user
- `qemu-system-x86_64` (tested 10.2.3), `qemu-img`, `curl`, `ssh`, `tar`
- Python 3 with `cryptography` (signify verification)
- For PE tests: `clang`, `lld-link`, `llvm-dlltool` (tested LLVM 22)
- About 100 GB free for the VM disk (sparse qcow2, 80 GB virtual) and sources

## 1. Fetch and verify OpenBSD release sets

    scripts/fetch-openbsd-sets.sh 8.0 amd64

Downloads to `openbsd-sets/8.0/amd64/` (resumable), then checks
`SHA256.sig` with the 8.0 base key and every file's SHA256. The key is
extracted from `src.tar.gz` (`etc/signify/openbsd-80-base.pub`), or supply it
with `OPENBSD_PUBKEY=`. `scripts/signify-verify.py` is a minimal signify
`-C` replacement for hosts without signify.

## 2. Install the VM (unattended, ~10 minutes)

    python3 -I scripts/vm/install.py

- Creates `vm/openbsd.qcow2` (refuses to overwrite) and, if missing, a
  VM-only SSH key `vm/id_ed25519`.
- Serves `install.conf` (rendered from `scripts/vm/install.conf.in`) and the
  sets on `127.0.0.1:8080`; the guest reaches it as `10.0.2.2:8080`.
- Boot loader → serial console via QEMU monitor `sendkey`; installer driven
  by autoinstall(8). The installer verifies SHA256.sig again with signify.
- Root and `builder` passwords: `vm/root-password` (random). Root SSH is
  key-only.
- Full serial transcript: `logs/vm-install.log`.

## 3. Run and provision

    scripts/vm/run.sh start         # headless; ssh on 127.0.0.1:2222
    scripts/vm/provision.sh         # packages, /usr/ports, mk.conf, limits
    scripts/vm/run.sh ssh           # shell as builder (VM_USER=root for root)
    scripts/vm/run.sh stop

`VM_MEM` (MB, default 4096) and `VM_CPUS` (default 6) size the guest.
Provisioning copies `upstream/ports` (minus `.git`) to `/usr/ports` and puts
`WRKOBJDIR`, packages and distfiles under `/home` (the largest partition).

Provisioning installs the release's `ports.tar.gz` (signify-verified) as
`/usr/ports` and overlays `emulators/wine` (from ports -current) and this
project's `ports/emulators/*`. /home is remounted `wxallowed`.

## 4. Build the ports (in the guest, detached from ssh)

    scripts/vm/port-build.sh start emulators/wine package
    scripts/vm/port-build.sh start emulators/proton-wine package
    scripts/vm/port-build.sh status emulators/proton-wine

Each takes ~1.5–2 h at -j6 in the VM. Install locally built (unsigned)
packages with:

    doas env TRUSTED_PKG_PATH=/home/packages/amd64/all pkg_add proton-wine

proton-wine lives in `/usr/local/lib/proton-wine` (run
`/usr/local/lib/proton-wine/bin/wine`).

## 5. Iterating on proton-wine

1. Edit `work/proton-port/wine` (git branch `openbsd`, a local clone of
   Valve's wine at `e60892cc` with the OpenBSD commits), commit.
2. For a quick rebuild copy changed files into the guest's WRKSRC:
   `scripts/vm/sync-wrksrc.sh path/in/tree ...`, then
   `scripts/vm/port-build.sh start emulators/proton-wine build`.
3. Re-export the patches:

       rm -rf build/src/git-orig build/src/git-new
       mkdir -p build/src/git-orig build/src/git-new
       git -C work/proton-port/wine archive e60892c | tar -C build/src/git-orig -xf -
       git -C work/proton-port/wine archive HEAD | tar -C build/src/git-new -xf -
       scripts/make-port-patches.sh build/src/git-orig build/src/git-new \
           ports/emulators/proton-wine/patches upstream/ports/emulators/wine/patches

4. Check unresolved symbols after installing:
   `scripts/vm/check-undefined.sh /usr/local/lib/proton-wine` (copy it to the guest).

## 6. PE test programs (on the host)

    make -C tests/pe          # → build/tests/pe/{basic,gui}.exe

## 7. Windows-target toolchain and DXVK (in the guest)

    cd /usr/ports/devel/mingw-w64 && make package    # ~20 min at -j6
    doas env TRUSTED_PKG_PATH=/home/packages/amd64/all pkg_add mingw-w64
    cd /usr/ports/graphics/dxvk && make package      # ~5 min
    doas env TRUSTED_PKG_PATH=/home/packages/amd64/all pkg_add dxvk
    cd /usr/ports/graphics/vkd3d-proton && make package
    doas env TRUSTED_PKG_PATH=/home/packages/amd64/all pkg_add vkd3d-proton

Build dependencies from 8.0 packages: cmake, ninja, meson, glslang (part of
`scripts/vm/provision.sh` from session 2 on; install them with `pkg_add` on
an older VM). Copy `ports/devel/mingw-w64`, `ports/graphics/dxvk` and
`ports/graphics/vkd3d-proton` into
`/usr/ports` first (same as the other overlays).

Using DXVK in a prefix:

    cp /usr/local/lib/dxvk/x64/*.dll "$WINEPREFIX/drive_c/windows/system32/"
    WINEDLLOVERRIDES="d3d8,d3d9,d3d10core,d3d11,dxgi=n" wine program.exe
    # Direct3D 12: also copy /usr/local/lib/vkd3d-proton/x64/*.dll and add
    # d3d12,d3d12core=n

## 8. Wine conformance tests (in the guest)

    ./winetest-build.sh /home/pobj/proton-wine-11.0.20261008 \
        dlls/ntdll/tests/x86_64-windows/ntdll_test.exe ...
    DISPLAY=:1 UNIT_TIMEOUT=180 ./winetest-run.sh \
        /usr/local/lib/proton-wine/bin/wine out bin/*_test.exe
