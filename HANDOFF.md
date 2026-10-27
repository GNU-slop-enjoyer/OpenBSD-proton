# OpenBSD Proton — complete handoff (2026-10-22)

> **Update 2026-10-28 (session 4):** the move to the new PC happened by
> rebuilding, not copying: the repo lives in `~/src/OpenBSD-proton`, the VM
> was reinstalled (`vm/openbsd.qcow2`, 16 vCPU/12 GB, and a clean copy
> `vm/fresh-test.qcow2` for installer tests, `VM_DISK=… VM_SSH_PORT=2223`),
> and `work/proton-port/wine` no longer exists — recreate it from Valve's
> `e60892cc` plus `ports/emulators/proton-wine/patches` when wine needs
> changing. Phase 2 became `proton-launcher` (no Valve client, by the
> user's choice) and Phase 3 is `scripts/openbsd-proton-setup` (`make`).
> State and next steps: `docs/PROGRESS.md`, first entry. Keep host RAM use
> under 25 GB (user's request).

This file is written for a new Claude Code session on a new machine that has
**none of the previous conversation**. Read it top to bottom once, then
`CLAUDE.md` (the standing mission and rules), then `docs/PROGRESS.md` (the
detailed log). Everything a successor needs is here or linked from here.

Contents

1. Prompt for the new session (copy-paste)
2. What the project is and where it stands
3. Moving to the new PC (what to copy, how, and how to check it)
4. The machines: host and OpenBSD VM
5. Repository and working-directory map
6. Everyday workflows (edit → build → test → package)
7. Everything that was changed, port by port
8. Test results so far
9. Open problems, with exact next steps
10. Phase 2: Steam on OpenBSD (plan)
11. Phase 3 (last): a smart CLI installer (spec)
12. Rules, approvals and the user's preferences
13. Pitfalls that already cost time
14. Appendix: the previous session's memory notes (verbatim)

---

## 1. Prompt for the new session (copy-paste)

> You are continuing the OpenBSD Proton project in `~/OpenBSD-proton`.
> Read `HANDOFF.md` completely, then `CLAUDE.md`, then `docs/PROGRESS.md`.
> Recreate the memory notes from HANDOFF.md §14 in your memory directory.
> First verify the move (HANDOFF.md §3.4: VM boots, packages present,
> `basic.exe`, `dualstack.exe`, `d3d11render.exe` on lavapipe pass); record
> the new host in `docs/TESTING.md` "Machines". Then work in this order,
> committing and pushing a new feature branch off `main` after each step:
> (1) the open problems in HANDOFF.md §9 that the new machine's resources
> unblock (start with the wine 11.0 build that decides whether the
> kernel32 volume heap corruption is upstream or Proton's);
> (2) Phase 2, Steam (§10) — investigate first, report blockers honestly,
> never circumvent DRM, never store credentials;
> (3) Phase 3, the installer (§11), last.
> The real-hardware GPU test is done by the user at the very end. Ask the
> user before sending anything upstream, merging to `main`, or doing
> anything privileged on the host.

---

## 2. What the project is and where it stands

Goal: run Windows games on OpenBSD with Valve's Proton stack, packaged as
OpenBSD ports. Target: **OpenBSD 8.0 amd64**. Development so far: Gentoo
Linux host + an OpenBSD 8.0 QEMU/KVM VM **without a GPU**.

Ports in this repository (`ports/`), all building and packaging in the VM:

| Port | Version | What it is |
|---|---|---|
| `emulators/proton-wine` | 11.0.20261008**p18** (Valve wine `e60892cc`) | Proton's wine with the OpenBSD port's patches + 71 patch files of our fixes; installs to `/usr/local/lib/proton-wine` |
| `emulators/proton-wine-addons` | Mono 11.2.0, Gecko 2.47.4 | Wine Mono/Gecko |
| `fonts/proton-fonts` | 11.0.2e p0 | Proton's replacement fonts |
| `graphics/dxvk` | 2.7.1 (Proton rev `a6764047`) **p1** | D3D8/9/10/11 → Vulkan |
| `graphics/vkd3d-proton` | 3.0 (Proton rev `212991fc`) p0 | D3D12 → Vulkan |
| `devel/mingw-w64` | 13.0.0 **p1** | Windows cross toolchain on base LLVM 22 (build dependency) |
| `emulators/proton-openbsd` | 0.1p7 | `proton-openbsd run prog.exe` launcher + man page |

What is **tested and working** in the VM (details: `docs/TESTING.md`):

- 64-bit console/GUI programs, .NET (Wine Mono), Gecko; 7-Zip, Notepad++;
  OpenTTD, SuperTuxKart (OpenGL/llvmpipe, keyboard/mouse, PulseAudio→sndio).
- **fsync** reimplemented on OpenBSD futex(2) (0.35 µs vs 17 µs waits), on by
  default in the launcher.
- **DXVK and vkd3d-proton on lavapipe** (software Vulkan built for testing):
  `d3d11render` 8/8, `d3d12render` 12/12, Dear ImGui D3D9/10/11/12 examples in
  windows, GZDoom (Vulkan renderer), Godot 4.7.2 editor (Vulkan and D3D12).
- Wine conformance tests: **226/278 units pass** with fsync (p16,
  `docs/results/winetest-proton-wine-p16-fsync.txt`); remaining failures are
  explained in TESTING/COMPATIBILITY.
- Precise Sleep/timed waits, directory reparse points, dual-stack IPv6 TCP
  clients, debugger without ptrace, sysctl-based process info, and more (§7).

What is **not** working / not tested:

- 32-bit Windows programs (OpenBSD has no 32-bit code segment for user
  processes) — many installers are 32-bit; Steam's installer probably too.
- Real GPU (radv/anv) — never tested; the user does it last.
- Steam, anti-cheat, VR; dual-stack UDP/listening sockets; DOS hidden/system
  attributes; a Proton heap corruption in `kernel32_test volume` (§9).

GitHub: `GNU-slop-enjoyer/OpenBSD-proton`. `main` = `aa21b7f` (PR #8 merged
by the user, contains everything up to `d356358`). Old feature branch:
`stage-a/openbsd-vm-baseline`. **Start a new feature branch from `main`.**

---

## 3. Moving to the new PC

### 3.1 What must be copied

Git only holds the small, reviewable part. Copy the **whole directory**
`~/OpenBSD-proton` (≈ 60 GB), preserving sparse files and permissions:

| Path | Size | In git? | Needed for |
|---|---|---|---|
| `ports/ docs/ scripts/ tests/ upstream-submissions/ *.md LICENSE` | ~5 MB | yes | everything |
| `vm/openbsd.qcow2` | 44 GB (sparse) | no | the VM with all installed packages, build trees, test prefixes, lavapipe, games |
| `vm/id_ed25519`, `vm/known_hosts`, `vm/root-password` | tiny | no (secret) | ssh into the VM — **keep private, never commit** |
| `work/proton-port/wine/` | 410 MB | no | **source of truth for all wine changes** (git repo, branch `openbsd`, 42 commits on Valve's `e60892cc`) |
| `build/src/git-orig`, `build/src/git-new` | part of 8.5 GB | no | exporting port patches (`scripts/make-port-patches.sh`) |
| `build/src/wine-11.19`, `wine1119-{base,chg,all}` | part of build/ | no | regenerating the emulators/wine 11.19 bundle |
| `build/linux-ref/` | part of build/ | no | Linux reference wine build + test exes (separates OpenBSD-only failures) |
| `build/wine110-ref/` | part of build/ | no | **unfinished** wine 11.0 build (killed for memory); safe to delete and redo |
| `upstream/` (ports, Proton with submodules, nethiafin-wine) | 3 GB | no | OpenBSD ports tree, Proton sources |
| `openbsd-sets/`, `src.tar.gz`, `sys.tar.gz` | 1.2 GB | no | verified OpenBSD 8.0 release sets/sources (VM reinstall) |
| `distfiles/` | 2.3 GB | no | locally fetched distfiles for port prototyping |
| `logs/` | 7 MB | no | VM console and fetched build logs |

Suggested copy (old PC → new PC, VM stopped first):

    scripts/vm/run.sh stop
    rsync -aHS --info=progress2 ~/OpenBSD-proton/ newpc:~/OpenBSD-proton/

(`-S` keeps the qcow2 sparse.) Alternatively shrink the image first:
`qemu-img convert -O qcow2 vm/openbsd.qcow2 vm/openbsd-compact.qcow2`.

### 3.2 Claude Code memory

Memory lives outside the repo, in
`~/.claude/projects/<path-derived-name>/memory/` (old PC:
`~/.claude/projects/-home-gentoo-OpenBSD-proton/memory/`). The directory name
depends on the absolute project path, so on the new PC it may differ. §14
reproduces every note verbatim — recreate them (or copy the old directory
if the path is the same). The old conversation transcript
(`…/bd1ce8da-e6ee-4f3e-98d2-2cc5bee569c1.jsonl`, very large) is optional.

### 3.3 New host requirements

- Linux with KVM (`/dev/kvm` accessible to the user), QEMU
  (`qemu-system-x86_64`), `ssh`, `git`, `python3` (+ Pillow for screenshot
  conversion), `curl`, `rsync`. For the Linux reference build: gcc, clang,
  lld, autoconf, the usual wine build deps.
- `gh` CLI is **not** installed on the old host (PRs had to be opened by the
  user in the browser). Installing it on the new PC is useful.
- GitHub push access: the old host pushed over SSH as `GNU-slop-enjoyer`
  with the user's key. On the new PC check `git push --dry-run` first.

### 3.4 Verify the move (first thing to do)

    cd ~/OpenBSD-proton
    git status && git log --oneline -3
    VM_MEM=16384 VM_CPUS=12 scripts/vm/run.sh start   # size to the new host
    scripts/vm/run.sh ssh 'uname -a; pkg_info -q | grep -E "proton|dxvk|vkd3d|mingw"; df -h /home'
    scripts/vm/run.sh ssh 'export DISPLAY=:1; pgrep -x Xvfb || (Xvfb :1 -screen 0 1024x768x24 &); sleep 2; pgrep -x cwm || (DISPLAY=:1 nohup cwm >/dev/null 2>&1 &)'
    scripts/vm/run.sh ssh 'export DISPLAY=:1 WINEPREFIX=~/pfx-pkg WINEDEBUG=-all; W=/usr/local/lib/proton-wine/bin/wine; $W ~/basic.exe | grep -c "^ok"; $W ~/pe/dualstack.exe | grep -c "^ok"'
    scripts/vm/run.sh ssh 'export DISPLAY=:1 VK_ICD_FILENAMES=$HOME/lvp/inst/share/vulkan/icd.d/lvp_icd.x86_64.json PROTON_PREFIX=$HOME/pfx-lvp3; cd ~/d3dtest; proton-openbsd run ./d3d11render.exe | tail -3'

Expected: 6, 17, and `ok 8 - right half cleared blue`. If the VM's
`/etc/hostname`, IP or sshd changed nothing else is needed — QEMU user
networking forwards 127.0.0.1:2222. The first wine run after a package
update rewrites the prefix and can take minutes.

Check `scripts/vm/run.sh` for `VM_MEM`, `VM_CPUS`, `VM_SSH_PORT`. The old
VM ran with 6 vCPUs and 4 GB RAM (host: 8 cores, 7 GB); give it far more on
the new PC — wine builds go from ~2 h to much less, and parallel test runs
become possible.

---

## 4. The machines

### Host (old PC)

Gentoo Linux, kernel 6.18, 8 cores, 7 GB RAM, LLVM 19/21/22 under
`/usr/lib/llvm/*`, no gdb/lldb/valgrind installed (a valgrind 3.27.1 was
built in a temporary scratch directory, not kept).

### OpenBSD 8.0 VM (`vm/openbsd.qcow2`)

- Created unattended by `scripts/vm/install.py` from the verified 8.0 sets
  (`docs/BUILDING.md` §1–3); provisioned by `scripts/vm/provision.sh`.
- ssh: `scripts/vm/run.sh ssh '…'` → `builder@127.0.0.1:2222` with
  `vm/id_ed25519`; `VM_USER=root` for root. `builder` has `doas`.
- Disk: `/home` 30.6 GB, **85 % used (4.4 GB free)** — clean up before big
  builds (§13).
- `/etc/mk.conf`: `WRKOBJDIR=/home/pobj`, `PACKAGE_REPOSITORY=/home/packages`,
  `DISTDIR=/home/distfiles`, `PLIST_REPOSITORY=/home/plist`.
- `/usr/ports` = 8.0 release ports tree + this project's ports copied in
  (`/usr/ports/emulators/proton-wine` etc.); `/usr/ports-current` = -current
  tree. `/usr/ports/emulators/wine` is **stock** (the 11.19 bundle build was
  restored).
- Installed packages of note: `proton-wine-11.0.20261008p18`,
  `proton-wine-addons`, `proton-fonts`, `dxvk-2.7.1.20260720p1`,
  `vkd3d-proton-3.0.20260729p0`, `mingw-w64-13.0.0p1`, `proton-openbsd-0.1p7`,
  `wine-11.19p4` (local build with our bundle batches 1–3), `llvm-22`,
  `meson`, `pulseaudio`, `spirv-tools`, `py3-mako`, `py3-yaml`.
- X: `Xvfb :1 -screen 0 1024x768x24` + `cwm`, **started by hand after each VM boot** (§3.4) (OpenBSD's default WM; it exposed focus bugs a bare
  Xvfb hides). Audio:
  `pulseaudio -n --daemonize=yes --exit-idle-time=-1 --load=module-null-sink --load=module-native-protocol-unix`
  (or the launcher autostarts pulseaudio → sndiod).

`~builder` layout in the VM:

| Path | What |
|---|---|
| `/home/pobj/proton-wine-11.0.20261008/` | **proton-wine build tree** (`build-amd64/`, `wine-e60892cc…/` WRKSRC, `bin/` wrappers); `$B/wine` runs uninstalled builds |
| `/home/pobj/wine-11.19/` | emulators/wine 11.19 build tree (bundle) |
| `~/winetest/bin/*.exe` | 29 wine conformance test executables (from the 11.0 tree) |
| `~/winetest/out-*` | runs: `out-p16f` = baseline (p16 + fsync) |
| `~/winetest-run.sh` | copy of `scripts/vm/winetest-run.sh` |
| `~/pe/` | built PE tests: sysinfo, audio, syncbench, sleep1(test), gsbench, dualstack, clobber, … |
| `~/basic.exe ~/gui.exe` | the first PE tests (`tests/pe/basic.c`, `gui.c`) |
| `~/d3dtest/` | d3d11probe/render, d3d12probe/render, d3dbrk |
| `~/lvp/inst/` | **lavapipe** (Mesa 25.0.7) — `VK_ICD_FILENAMES=$HOME/lvp/inst/share/vulkan/icd.d/lvp_icd.x86_64.json` |
| `~/imgui/` | Dear ImGui 1.91.9 source + dx9/10/11/12 example exes |
| `~/gzdoom/game/` | GZDoom 4.14.2 + freedoom2.wad |
| `~/godot/bin/` | Godot 4.7.2 editor (win64) |
| `~/openttd`, `~/stk`, `~/npp`, 7-Zip files | earlier app tests |
| `~/pfx-pkg` (installed package), `pfx-buildtree` (build tree), `pfx-wt-p13`, `pfx-wt-p16f` (winetest), `pfx-ds`, `pfx-lvp`, `pfx-lvp2/3/4` (lavapipe tests), `pfx-voltest`, `pfx-wine1119`, `pfx-proton`, `pfx-dotnet` | wine prefixes (~600 MB each; delete freely, they are recreated) |
| `~/build-logs/` | port build logs |
| `~/*.sh` | ad-hoc test scripts from the session (pkg-p15.sh packages proton-wine; t-*.sh tests) |

---

## 5. Repository map

| Path | Content |
|---|---|
| `CLAUDE.md` | standing mission and rules (authoritative) |
| `HANDOFF.md` | this file |
| `README.md`, `LICENSE` | overview; LGPL-2.1-or-later for this repo's own files |
| `docs/PROGRESS.md` | chronological log, newest first, with next actions |
| `docs/TESTING.md` | every test run with date, machine, command, result (table) |
| `docs/COMPATIBILITY.md` | every OpenBSD/Linux difference found and how it was handled |
| `docs/ARCHITECTURE.md`, `DECISIONS.md`, `DEPENDENCIES.md`, `FEASIBILITY.md`, `ROADMAP.md`, `BUILDING.md`, `USING.md` | as named |
| `docs/results/*.txt` | winetest summaries (Linux reference, p7, p8, p12 fsync, p13, p16 fsync) |
| `docs/screenshots/` | evidence screenshots |
| `ports/` | the seven ports (§2) |
| `scripts/vm/` | VM install/run/provision, `port-build.sh` (detached ports builds), `sync-wrksrc.sh` (copy changed work-tree files into the VM build tree), `winetest-build.sh`, `winetest-run.sh`, `lavapipe/build.sh` |
| `scripts/make-port-patches.sh` | diff two trees into ports-style `patches/patch-*` keeping explanatory headers |
| `scripts/linux-ref-build.sh`, `winetest-compare.sh` | Linux reference build; compare two winetest summaries |
| `scripts/debug/` | narrow-syscall-arg analysis, memcpy-overlap preload, etc. |
| `tests/pe/` | PE test programs + Makefile (`make d3d`, `make dualstack`, …; `MINGW_CC`/`MINGW_CXX`) |
| `upstream-submissions/emulators-wine/` | fixes that also apply to OpenBSD's own emulators/wine 11.19: `patches/` (full port patch set), `openbsd-fixes.diff`, README with test results. **Not sent anywhere** (user's decision). |

---

## 6. Everyday workflows

### 6.1 Changing proton-wine (the common loop)

1. Edit in `work/proton-port/wine` (git, branch `openbsd`).
2. Copy changed files into the VM build tree and build there:

       cd work/proton-port/wine
       ../../../scripts/vm/sync-wrksrc.sh dlls/ntdll/unix/sync.c server/sock.c
       cd ../../..
       scripts/vm/run.sh ssh 'cd /home/pobj/proton-wine-11.0.20261008/build-amd64 && PATH=/home/pobj/proton-wine-11.0.20261008/bin:$PATH gmake -j6 dlls/ntdll/ntdll.so server/wineserver'

3. Test with the build-tree wine: `B=/home/pobj/proton-wine-11.0.20261008/build-amd64; WINEPREFIX=~/pfx-buildtree $B/wine prog.exe`
   (kill its server with `$B/server/wineserver -k`; **never share a prefix
   between the build-tree wine and the installed package at the same time**).
4. Commit in the work tree (message explains why; trailer
   `Co-Authored-By: …` as instructed by the harness).
5. Export patches:

       git -C work/proton-port/wine archive HEAD | tar -C build/src/git-new -xf -
       scripts/make-port-patches.sh build/src/git-orig build/src/git-new \
           ports/emulators/proton-wine/patches upstream/ports/emulators/wine/patches

   New patch files need an explanatory header (text before `Index:`);
   existing headers are kept — extend them when the change is new.
6. Bump `REVISION` in `ports/emulators/proton-wine/Makefile`.
7. Package in the VM: copy the port dir (`tar … | ssh 'rm -rf …/patches && cp -R …'`),
   then `make clean=fake; make package` (the build tree already has the
   changes, so this only repackages), then
   `doas env TRUSTED_PKG_PATH=/home/packages/amd64/all pkg_add -u proton-wine`
   (the framework's own dependency install of fresh packages fails — install
   manually). `~builder/pkg-p15.sh` does exactly this (edit the pN in it).
8. Record in `docs/TESTING.md`, update `docs/PROGRESS.md`, commit, push.

### 6.2 Full conformance run

    scripts/vm/run.sh ssh 'export DISPLAY=:1 WINEPREFIX=~/pfx-wt-pNN WINEFSYNC=1 UNIT_TIMEOUT=300; cd ~/winetest/bin && nohup sh -c "~/winetest-run.sh /usr/local/lib/proton-wine/bin/wine ~/winetest/out-pNN *.exe > ~/winetest/run-pNN.log 2>&1; echo DONE >> ~/winetest/run-pNN.log" >/dev/null 2>&1 </dev/null &'
    # ~1.5 h on the old VM; then:
    scripts/winetest-compare.sh docs/results/winetest-proton-wine-p16-fsync.txt <new summary>

The runner sets a UTF-8 locale and runs tests in `OUTDIR/cwd` (both matter:
non-ASCII file names; ole32's "winetest" file). Summary columns:
`exe unit status executed failures todo skipped rc seconds`.

### 6.3 Linux reference (decides "OpenBSD bug or Proton/Wine behaviour")

`build/linux-ref/obj/wine` + `build/linux-ref/tests/*.exe`, prefix in a
scratch dir, run with `env -u DISPLAY -u WAYLAND_DISPLAY`. Results:
`docs/results/winetest-linux-ref.txt`.

### 6.4 Toolchain / DXVK / vkd3d-proton

Rebuild order: `devel/mingw-w64` → `graphics/dxvk`, `graphics/vkd3d-proton`
(they require `mingw-w64>=13.0.0p1`). Anything PE built with the old
toolchain must be rebuilt (retguard, §7).

### 6.5 Software Vulkan in the VM

`scripts/vm/lavapipe/build.sh` (Mesa 25.0.7, one patch, private prefix).
Use `VK_ICD_FILENAMES=…/lvp_icd.x86_64.json`. Screenshots:
`xwd -root -display :1 -out f.xwd`, convert on the host with a tiny Python
xwd reader (24 bpp → `BGR` raw mode with PIL; ImageMagick on the old host
lacked the xwd delegate).

### 6.6 emulators/wine 11.19 bundle

Trees in `build/src`: `wine-11.19` (pristine), `wine1119-base` (+port
patches), `wine1119-chg` (base + our fixes → `openbsd-fixes.diff` via
`diff -ruN -x '*.orig' wine1119-base wine1119-chg | sed …`),
`wine1119-all` (everything → bundle `patches/` as diffs against
`wine-11.19`). Test builds: put the bundle into `/usr/ports/emulators/wine`
in the VM (save `patches.orig`), bump REVISION locally, build, test, then
restore stock (`upstream/ports/emulators/wine/Makefile`). Test prefixes for
11.19 need `WINEDLLOVERRIDES="mscoree,mshtml="` (no Mono installed; its
download dialog blocks wineboot on Xvfb).

---

## 7. Everything that was changed

### proton-wine patch history (each pN = REVISION; details in the patch headers and work-tree commits)

| Rev | Change |
|---|---|
| p0 | Port of Valve wine `e60892cc` with OpenBSD's emulators/wine patches (rejects resolved); backported GL_EXTENSIONS use-after-free and overlapping-memcpy fixes |
| p1–p5 | narrow syscall argument ABI fix (clang), fonts, change notifications (libinotify path fix), socket option fixes (boolean options, multicast TTL/LOOP, reconnect after failed connect via `sock_recreate_unix_socket`), signal-context malloc, copy-APC fault handling |
| p6 | bcrypt via libgcrypt/gmp; VM counters and CPU times from sysctl; debugging without ptrace (APC_WRITE_MEMORY, `debugged` flag; protocol 932) |
| p7 | getaddrinfo local host name fallback; WM_TAKE_FOCUS no-op when focused (cwm loop) |
| p8 | pointer grab only while a button is pressed (cwm focus ping-pong); gcry_check_version |
| p9 | real `%fs` selector in contexts |
| p10 | **fsync on futex(2)** (WINEFSYNC=1); **precise Sleep** after timeBeginPeriod(<10) or WINE_PRECISE_SLEEP=1 |
| p11 | relayed signals for the calling thread claimed when a server call returns |
| p12 | fsync waiter table (no thundering herd) |
| p13 | server cleans killed threads' fsync waiter entries |
| p14 | **directory reparse points** (`.wine-reparse` inside the directory), any reparse tag |
| p15 | short `%gs` instructions: `jmp rel8` to int3 padding holding the `jmp rel32` (5 µs → 5 ns per execution) |
| p16 | precise timed waits on the server path |
| p17 | NULL pointer for `%.*s` in wineserver request traces (syslog noise) |
| p18 | **dual-stack IPv6 TCP clients** emulated (`sock_connect_dualstack`) |

### Other ports

- `devel/mingw-w64` p1: wrappers pass **`-fno-ret-protector`** — OpenBSD
  clang's retguard in Windows-target leaf functions clobbered `%rsi`
  (callee-saved on Win64) and crashed DXVK's `D3D11CreateDevice`.
- `graphics/dxvk` p1: `patch-src_util_util_misc_h` — no division by a 0 Hz
  refresh rate (Xvfb) in Present(); a backport of upstream DXVK `0aea852`.
  Also the singleton fix (`patch-src_util_util_singleton_h`).
- `emulators/proton-openbsd` 0.1p7: fsync default, PulseAudio autostart,
  `PROTON_USE_WINED3D`, `PROTON_NO_FSYNC`, `WINE_PRECISE_SLEEP` documented in
  `proton-openbsd(1)`.

### emulators/wine 11.19 bundle (not sent upstream)

Batches 1–5 + winex11 NULL Xkb variant + server trace fix; batches 1–3
tested as local wine-11.19p4 (results in its README).

---

## 8. Test results so far

The authoritative table is `docs/TESTING.md`. Headline numbers:

- winetest p16 + fsync: pass 226 / fail 45 / crash 5 / timeout 1 / empty 1
  (Linux reference and p12/p13 in `docs/results`).
- PE tests: basic 6/6, gui 5/5, sysinfo 25/25, audio 15/15, dualstack 17/17,
  d3d11render 8/8 (wined3d and DXVK), d3d12render 12/12.
- Benchmarks: fsync wait 0.35 µs (server 17 µs); Sleep(1) 1.00 ms in precise
  mode; `%gs` access 3–5 ns after rewrite.
- Real programs: listed in §2.

---

## 9. Open problems and exact next steps

1. **Proton heap corruption in `kernel32_test volume`** (also on Linux with
   pristine Proton: glibc "double free or corruption (out)"; wine 11.19 with
   the same OpenBSD changes is clean). Reached via a directory symlink to a
   volume mount point. Four replacement allocators and valgrind could not
   catch it — see `docs/COMPATIBILITY.md` "Reparse points and known
   file-system gaps". **Next:** on the new PC build upstream wine 11.0
   (`build/wine110-ref`, tarball sha256 `c07a6857…0701` already downloaded;
   configure like `scripts/linux-ref-build.sh`, `make -j<n>`) and run
   `kernel32_test.exe volume`. Crash → upstream bug fixed between 11.0 and
   11.19 (bisect upstream wine). No crash → a Proton patch (fetch Valve's
   wine with full history and bisect). Valgrind needs a larger client
   address space than 128 GB to run this wine.
2. **Dual-stack UDP and listening sockets**: not emulated (needs two Unix
   sockets per handle, wineserver polling + client fd cache). Only do it if
   a real program needs it.
3. **DOS hidden/system attributes**: no xattrs/UF_HIDDEN on OpenBSD;
   documented limitation.
4. **File reparse points with data**: data lives in the file content;
   documented limitation.
5. **Remaining winetest failures**: classified in TESTING/COMPATIBILITY;
   many OpenBSD-only socket option differences (TCP keepalive tuning,
   IP_RECVTOS, privileged ports) are platform limits.
6. **Rare silent SIGABRT** (seen with p5–p8): not reproducible in 55 runs
   since; reopen only if seen again (run under `script(1)` to capture the
   kernel's `uprintf`).
7. **Real-hardware GPU test** (user, last): same tests without
   `VK_ICD_FILENAMES`; also performance comparison wined3d vs DXVK.
8. **Upstream submissions**: prepared, **not sent** — needs the user's OK
   each time.

---

## 10. Phase 2: Steam on OpenBSD (plan — investigate before building)

The user wants Steam with login, licenses and the "runtime", using this
Proton port. There is **no native OpenBSD Steam client** and Valve's Linux
Steam runtime (pressure-vessel containers, Linux namespaces) cannot run on
OpenBSD. Realistic architecture: **the Windows Steam client inside a
proton-wine prefix**, launching games in the same prefix so games' Steamworks
calls (`steam_api64.dll`) talk to that client. This is how Steam worked on
plain wine before Proton existed.

Investigation steps (record every finding in docs, honestly):

1. **Architecture of the current Windows Steam client.** Steam moved to a
   64-bit Windows client in 2024–2026 — verify which binaries are 64-bit
   (`steam.exe`, `steamwebhelper.exe` (Chromium/CEF), `steamservice.exe`,
   `GameOverlayUI.exe`, `steamerrorreporter.exe`). Any required 32-bit part
   is a hard blocker on OpenBSD (§2); find a 64-bit path or document it.
2. **Installer.** `SteamSetup.exe` is probably a 32-bit NSIS installer → it
   cannot run. Extract it with 7-Zip on the host instead (NSIS archives
   unpack), or fetch the client package directly the way the bootstrapper
   does. Only from Valve's official servers; record URLs and hashes.
3. **steamwebhelper (CEF).** Expect sandbox problems: try
   `-no-cef-sandbox`, `-cef-disable-gpu`, `-cef-disable-d3d11`, software
   rendering; check winhttp/ws2_32/crypt32 failures. The login UI and the
   store are CEF pages.
4. **Login**: the user logs in interactively (Steam Guard). **Never store
   credentials or tokens in the repo, logs or docs.** Do not automate the
   user's credentials.
5. **Licenses / library / downloads**: through the client once logged in.
   A fallback for downloading owned games without the client is the
   open-source `steamctl` (Python, runs natively on OpenBSD) or
   DepotDownloader — they use the user's own login and licenses; games then
   run with `proton-openbsd`, but games that need a running Steam client
   (Steamworks) still need the Windows client running.
6. **Launching games**: `proton-openbsd run` semantics inside the Steam
   prefix — either make Steam the launcher (games inherit the prefix and
   env: WINEFSYNC, DXVK overrides, fonts) or add a `proton-openbsd steam`
   subcommand that starts the client with the right environment.
7. **What not to do**: no DRM circumvention, no Steam emulators (Goldberg
   etc.), no bypassing of anti-cheat. Steam's ToS allow running the client
   under compatibility layers; respect licensing.
8. **Tests**: a free-to-play or open-source game on Steam (e.g. a Source
   SDK base or free game the user owns) through Steam + DXVK, first on
   lavapipe, then on real hardware (user).

Deliverables: a port or script that installs the Steam client into a
prefix (no redistribution of Valve binaries in the repo — download at
install time from Valve), `proton-openbsd steam`, docs, tests.

---

## 11. Phase 3 (last): a smart CLI installer

Goal: someone with a fresh OpenBSD 8.0 amd64 install runs one command and
gets the whole stack (and optionally Steam), with clear checks and errors.

Spec:

- **Language**: POSIX `sh`/`ksh` (OpenBSD base has no Python; Perl is in
  base too if needed). Single entry point, e.g. `openbsd-proton-setup`,
  shipped in this repo and as a port (`emulators/proton-openbsd` or a new
  `sysutils/openbsd-proton-setup`).
- **Preflight checks** with actionable messages: OpenBSD version/arch;
  `/usr/local` on a `wxallowed` filesystem; user in a login class with large
  `datasize`/`openfiles` (`/etc/login.conf`, needs root, explain why); free
  disk; X running; GPU detection (`dmesg`/`pkg_info`: amdgpu → radv,
  inteldrm → anv, none → wined3d/lavapipe); sndiod running; doas configured.
- **Install modes**: (a) binary packages from a published package repository
  (needs signing — OpenBSD packages are signed with `signify`; set up a
  project key and document `PKG_PATH`/trusted key), (b) build from source
  through the ports tree (copies the ports into `/usr/ports/mystuff`, builds
  in dependency order: mingw-w64 → dxvk, vkd3d-proton → proton-wine,
  addons, fonts → proton-openbsd, with progress and log files).
- **Post-install**: create the default prefix, run the test programs
  (basic, d3d11render, audio) and report pass/fail; optional Steam step
  (Phase 2) with the user logging in themselves.
- **Behaviour**: idempotent (re-running upgrades), `--dry-run`, `--yes`,
  `--uninstall`, `--log`, never runs privileged steps without saying what
  and why (uses `doas`), no telemetry, no credentials stored.
- Document it in README/USING; test it on a fresh VM image (snapshot the
  qcow2 before, `qemu-img snapshot`).

---

## 12. Rules, approvals and preferences

- `CLAUDE.md` is authoritative (keep upstream checkouts intact, no
  privileged host commands, no disabled checks, honest reporting, keep
  docs/PROGRESS.md current).
- The user authorized autonomous work and **pushing feature branches**
  (verify with `git push --dry-run`). Never force-push, never push or merge
  to `main` without asking (the user merges PRs), never push to OpenBSD or
  Valve repositories.
- **Upstream submissions**: only with the user's explicit OK per item.
  Status: DXVK refresh fix — already upstream (`0aea852`), nothing to send;
  everything else unsent.
- **Third-party Windows binaries**: approved categories (Wine Mono/Gecko,
  small open-source apps, open-source games, test tools) — only inside the
  VM, from official sites, checksums verified and recorded.
- **License**: this repo is LGPL-2.1-or-later.
- The user prefers hands-off progress, short status updates, and asking all
  approval questions at once. They do the real-hardware test last.
- Commit messages end with the co-author trailer the harness specifies.

---

## 13. Pitfalls that already cost time

- `pgrep -f X`/`pkill -f X` match the ssh shell's own command line → use
  `[x]yz` patterns; keep kill and launch in separate ssh calls.
- OpenBSD `grep` has no `\|` → use `grep -E 'a|b'`. OpenBSD `sed` has no
  `\n` in replacements → use perl. OpenBSD `tar` has no `-J` → `xz -dc | tar -xf -`.
- `nohup … &` over ssh holds the session unless stdin is `</dev/null`; but a
  trailing `&` on a whole `&&` chain also gives tar an empty stdin — run the
  copy and the background job in **separate** ssh calls.
- Claude Code's tool timeout is 600 s: run long builds/tests **detached in
  the VM** and poll with a waiting loop (`until grep -q DONE …`).
- `/etc/mk.conf` sets WRKOBJDIR (env overrides are ignored); PLIST_REPOSITORY
  means repackaging needs a REVISION bump; `make update-plist` after changing
  installed files.
- VM `/home` fills up: each prefix ~600 MB. Winetest leaves
  `Temp/Please Remove Me/tmpdir*` at mode 0200 — `chmod 700` each, then rm.
- The first run of a new package rewrites the prefix (slow); wine 11.19
  prefixes without Mono need `WINEDLLOVERRIDES="mscoree,mshtml="`.
- Never use one prefix with two different wine builds at once.
- Building wine on the host while the VM runs starved the VM (and the old
  host ran out of memory). On the new PC give both enough RAM, or build with
  fewer jobs.
- Allocator/heap debugging: OpenBSD malloc options `S`, `F`, `J`;
  `kern.nosuidcoredump`; lldb needs `ulimit -d $(ulimit -H -d)`;
  `winedbg` works (breakpoints, `bt`, `info regs`); live lldb attach does
  not. `ktrace -i -t S` records pinsyscalls violations.
- DXVK/vkd3d: test programs must print unbuffered (`setvbuf`) or a crash
  hides their TAP output; use winedbg breakpoints + `llvm-objdump`/
  `llvm-symbolizer` on the unstripped DLL for crashes.

---

## 14. Appendix: memory notes (verbatim, recreate these)

**MEMORY.md (index)**

- [Push autonomy](push-autonomy.md) — user authorized autonomous work + pushing feature branches after push check
- [Project state](project-state.md) — VM, 8.0 vs -current ports split, proton-wine work tree; read docs/PROGRESS.md first
- [VM shell pitfalls](vm-shell-pitfalls.md) — pgrep/pkill self-match, BSD grep -E, mk.conf WRKOBJDIR, PLIST repo
- [Binaries approval](binaries-approval.md) — third-party Windows binaries OK in VM; real-HW test last; "fix everything"
- [Upstream approvals](upstream-approvals.md) — 2026-10-22: DXVK fix PR ok, PR to main (no merge), LGPL license; nothing else upstream

**push-autonomy** (feedback): User said: check first whether pushing to the
GitHub repo works; if not, stop and tell them; if it does, start the project
and "do whatever you want". Why: hands-off progress, GitHub as the
collaboration point. How to apply: verify push with `git push --dry-run`
(SSH key auth as GNU-slop-enjoyer); commit milestones and push the feature
branch without asking. Still never push to main, force-push, or touch
upstream OpenBSD/Valve repos (CLAUDE.md).

**project-state** (project): Handoff is docs/PROGRESS.md (read first). Test
target is a local QEMU/KVM OpenBSD 8.0 VM (`scripts/vm/run.sh start`, ssh as
builder or VM_USER=root, key in vm/). In the VM `/usr/ports` is the 8.0
release tree plus overlays; -current kept at `/usr/ports-current`; packaging
-current ports against 8.0 packages fails the WANTLIB check — don't bypass
checks. Proton wine source of truth: `work/proton-port/wine` (branch
`openbsd`, git-ignored); patches exported with scripts/make-port-patches.sh.
OpenBSD clang only warns on implicit function declarations — grep build logs
for "undeclared function". Ports: devel/mingw-w64, graphics/dxvk,
graphics/vkd3d-proton, emulators/proton-openbsd. Narrow-syscall-arg clang
ABI bug: scripts/debug/narrow-syscall-args.py + apply-narrow-syscall-args.py.
Linux reference build separates OpenBSD-only winetest failures
(scripts/winetest-compare.sh). GUI tests with cwm on :1. Built-dependency
installs need manual `doas env TRUSTED_PKG_PATH=... pkg_add`. Debug: winedbg
(per-thread bt), lldb needs `ulimit -d` raised, `ktrace -t S` for
pinsyscalls. As of 2026-10-22: proton-wine p18 installed; wine-11.19p4
(local, bundle batches 1-3) installed, /usr/ports/emulators/wine stock.
11.19 bundle trees in build/src (wine-11.19 pristine, wine1119-base,
wine1119-chg → openbsd-fixes.diff, wine1119-all → bundle patches/). Wine
11.19 prefixes without Mono need WINEDLLOVERRIDES="mscoree,mshtml=".
lavapipe at ~builder/lvp/inst. mingw-w64 must use -fno-ret-protector.
Screenshots: `xwd -root -display :1`, python xwd reader (24 bpp → BGR).

**vm-shell-pitfalls** (feedback): see §13 (all items there come from this
note plus later sessions).

**binaries-approval** (feedback): On 2026-10-21 the user approved (all four):
Wine Mono + Gecko (official WineHQ builds), small real open-source apps
(7-Zip, Notepad++ …), open-source games (SuperTuxKart, OpenTTD …), and
Windows test tools (winetest.exe, samples) — only inside the OpenBSD VM, from
official project sites, verifying checksums/signatures where published. The
real-hardware OpenBSD Vulkan test comes last (the user does it); for open
technical items "just fix everything". How to apply: download these without
asking again; never run them on the host; record source URL + checksum per
binary in docs.

**upstream-approvals** (feedback): 2026-10-22: send the DXVK 0 Hz fix
upstream (turned out to be already upstream as 0aea852 — nothing sent);
nothing else upstream yet (wine/WineHQ fixes, OpenBSD emulators/wine bundle
stay in-repo). PR to main opened via the browser and **merged by the user**
(PR #8). License LGPL-2.1-or-later. The user does the real-hardware test
after everything else. How to apply: don't send other patches upstream or
merge to main without asking again.

New notes for the next session to add:

- **new-pc** (project): moved to a new, much larger PC on 2026-10-24 (the
  user's Saturday build); the old host had 8 cores/7 GB and starved the VM.
  Size the VM generously; long builds no longer need to be serialized.
- **next-phases** (project): after §9, Phase 2 Steam (§10), Phase 3 installer
  (§11) — installer is the last step; the user tests real hardware at the end.
