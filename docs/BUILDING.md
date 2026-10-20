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

## 4. Build the wine baseline (in the guest)

    cd /usr/ports/emulators/wine && make package

## 5. PE test programs (on the host)

    make -C tests/pe          # → build/tests/pe/basic.exe
