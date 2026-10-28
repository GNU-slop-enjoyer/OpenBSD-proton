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

## D5 — Build against the 8.0 release ports tree, overlaying new ports (2026-10-20)

Packaging emulators/wine from ports -current against 8.0 binary packages fails
the framework's library check (`glib-2.0.4201.17` in the tree vs `.16`
installed). That check is correct and is not bypassed. The VM uses the
signify-verified `8.0/ports.tar.gz` as `/usr/ports` (-current kept as
`/usr/ports-current`) with `emulators/wine` and `emulators/proton-wine`
overlaid. Both port Makefiles work unchanged with the 8.0 framework.

## D6 — proton-wine installs under ${LOCALBASE}/lib/proton-wine (2026-10-20)

`PREFIX` is moved (gnu.port.mk always passes `--prefix=${PREFIX}`), giving a
self-contained tree like Proton's `files/` directory that coexists with
emulators/wine. Its man pages are therefore outside the default man path.

## D7 — Port distfiles are upstream artefacts only (2026-10-20)

emulators/proton-wine fetches Valve's tree by commit (GH_*), the Khronos
registry files, and the clang PE headers; the OpenBSD changes are ports
patches exported from `work/proton-port/wine` (`scripts/make-port-patches.sh`);
generated sources are produced in `post-patch` like Proton's own build.
Nothing locally generated is used as a distfile.

## D8 — Windows-target toolchain on the installed LLVM (2026-10-21)

`devel/mingw-w64` does not build its own clang/lld (llvm-mingw does): the
8.0 `llvm-22` package already has the X86 target, COFF lld, llvm-dlltool,
llvm-windres and llvm-rc. The port only builds target libraries (mingw-w64
headers/CRT/winpthreads; compiler-rt builtins, libunwind, libc++abi,
libc++ from the same `llvm-project-22.1.8.src.tar.xz` distfile as
`devel/llvm/22`) and installs `x86_64-w64-mingw32-*` wrappers. CRT default
is UCRT (what DXVK/vkd3d-proton and current llvm-mingw use). Cost: the port
must track `devel/llvm`'s major version (`LLVM_MAJOR`).

## D9 — DXVK as `graphics/dxvk`, DLLs in lib/dxvk/x64 (2026-10-21)

Distfiles are the pinned dxvk commit plus one GitHub archive per submodule
commit (including dxbc-spirv's nested SPIRV-Headers), placed in
`post-extract`. The extracted tree was verified identical to a recursive git
checkout of the same commits. Only the DLLs are installed (no import
libraries); installing them into a prefix is left to the user or a future
launcher (ROADMAP M4), as Proton does per prefix. `graphics/vkd3d-proton`
follows the same layout (`lib/vkd3d-proton/x64`) and pins widl through a
port-local cross file, so an installed wine's `widl` is never picked up.

## D10 — Steam without Valve's client: proton-launcher (2026-10-27)

The user asked for Steam login, downloads and a launcher, and to avoid
proprietary software. Valve's Windows client (32/64-bit mix, CEF
`steamwebhelper`) was therefore not downloaded or tried. Instead
`launcher/` is a native C client for Steam's servers: the CM protocol over
WebSocket and the Authentication, PICS and ContentServerDirectory messages
as documented by SteamKit2 and SteamDatabase/Protobufs (no code copied;
messages are encoded by hand with `launcher/pb.c`). It logs in with the
user's own account and Steam Guard, downloads only depots the account
owns, and starts games through proton-openbsd. Consequence, documented in
USING.md and proton-launcher(1): games that need a running Steam client
(Steamworks `SteamAPI_Init`) do not work. No Steam emulator (Goldberg etc.)
and nothing that bypasses DRM or ownership is used. *Superseded in part by
D12 (2026-10-28).* `steam_appid.txt` (the
Steamworks SDK's developer mechanism) is written so games with optional
Steam features start.

## D11 — proton-launcher builds from the checkout; installs through openbsd-proton-setup (2026-10-28)

Unlike D7, `games/proton-launcher` has no distfile: it copies the
project's own `launcher/` sources (`PL_SRC`, default relative to the port
in the checkout). The ports are used in place through `PORTSDIR_PATH`
instead of being copied into `/usr/ports`, so a `git pull` is all an
update needs. `make` at the top runs `scripts/openbsd-proton-setup`, which
builds every port as the user (doas for the root steps only, each shown
and confirmed) in a build area on the largest `wxallowed` filesystem, and
installs each package right after it is built (the framework's own
dependency installs of unsigned packages fail through doas).

## D12 — Our own steamclient64.dll for Steamworks games (2026-10-28)

The user's report after testing on the real install: only Stumble Guys
ran. Most of their library uses the Steamworks API, and `SteamAPI_Init`
fails without a Steam client (tested: MECCHA CHAMELEON, Gang Beasts; on
Linux Proton without Steam, Gang Beasts starts without a user and
without costumes). The user asked to fix the port, not the games.

Decision: `steamclient/` is a client library of our own, in C, built
with the port's mingw-w64 (port `emulators/proton-steamclient`). It is
the part of Proton's design (lsteamclient + steam_helper) that does not
need Valve's client: the vtable layouts of the 212 interface versions are
generated from Proton's lsteamclient sources (`gen.sh`, layouts only),
the behaviour is written here. It reports the account proton-launcher
logged in with and the apps that account owns (from its licenses); it
does not unlock anything the account does not own, does not touch DRM
(SteamStub games keep failing), and stores stats and cloud files
locally. Online services report "offline". This replaces D10's "no Steam
emulator"; the rest of D10 (no Valve client) stands.
