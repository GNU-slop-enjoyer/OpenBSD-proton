# Architecture

How the pieces fit together on OpenBSD/amd64, and where the OpenBSD-specific
parts live. Status of each piece: [TESTING.md](TESTING.md); reasons for the
choices: [DECISIONS.md](DECISIONS.md).

## Components and packages

```
proton-openbsd run game.exe            (emulators/proton-openbsd, sh)
  │  sets WINEPREFIX, WINEFSYNC=1, LC_CTYPE=C.UTF-8, WINEDLLOVERRIDES
  │  first run: wineboot, links fonts, installs DXVK / vkd3d-proton DLLs
  ▼
/usr/local/lib/proton-wine/bin/wine    (emulators/proton-wine)
  ├─ PE side (Windows DLLs, built by clang for x86_64-windows)
  │    ntdll.dll kernel32.dll user32.dll … d3d11.dll (wined3d) …
  │    + DXVK d3d9/d3d10core/d3d11/dxgi.dll   (graphics/dxvk)
  │    + vkd3d-proton d3d12/d3d12core.dll     (graphics/vkd3d-proton)
  │    + Wine Mono, Wine Gecko                (emulators/proton-wine-addons)
  ├─ Unix side (ELF .so, built by base clang)
  │    ntdll.so  win32u.so  winex11.so  winepulse.so  winevulkan.so …
  │      └─ X11 (Xenocara), Mesa OpenGL/Vulkan, PulseAudio→sndio, gnutls/gcrypt
  └─ wineserver (one per prefix): objects, handles, messages, async I/O
fonts: /usr/local/share/proton-fonts   (fonts/proton-fonts)
```

`devel/mingw-w64` is only a build dependency (DXVK and vkd3d-proton are
built with meson and the mingw-w64 cross toolchain on top of base LLVM).

## Process model

Each Windows process is a native OpenBSD process running the wine loader;
its Windows code runs in the same address space as ntdll.so. Windows
system calls enter ntdll.so through wine's syscall dispatcher; operations on
shared state are requests to wineserver over a Unix socket and pipes.

## OpenBSD-specific mechanisms

| Need | OpenBSD constraint | Mechanism | Where |
|---|---|---|---|
| TEB access through `%gs:` | no user-settable `%gs` base | fault on `%gs:` instructions, rewrite them to `%fs:` or a stub | ntdll `gs_openbsd.c` (from emulators/wine) |
| stacks | stack pointer must be in MAP_STACK memory at syscalls/faults | thread stacks remapped MAP_STACK; native threads run on libpthread stacks | ntdll `thread.c`, `virtual.c` |
| W^X | no W|X mappings outside wxallowed filesystems | binaries linked `-z wxneeded`, installed under /usr/local | port Makefile |
| signals to one thread of another process | no cross-process thread signals | server writes (signal, tid) into a shared array and sends SIGUSR2; a thread relays with thrkill(); the calling thread claims its own entries when a server call returns | ntdll/server `sig_openbsd.c` |
| server access to client memory | ptrace only for descendants | client does the copy in a system APC: APC_COPY_MEMORY (NtRead/WriteVirtualMemory), APC_WRITE_MEMORY (PEB BeingDebugged) | ntdll `server.c`, server `ptrace.c`, `protocol.def` |
| fast synchronization (fsync) | futex(2) but no futex_waitv | single-object waits FUTEX_WAIT; multi-object waiters register in a table in the fsync shm and sleep on their own word | `include/wine/fsync_openbsd.h`, ntdll/server `fsync.c` |
| timers | 100 Hz clock, sleeps end on a tick | after timeBeginPeriod(<10 ms) or WINE_PRECISE_SLEEP=1, sleep whole ticks and yield the rest; server-path timed waits poll the objects for the last two ticks | ntdll `sync.c`, kernel32 `sync.c` |
| reparse points | no extended attributes | data as the content of the renamed `name?` file, or `.wine-reparse` inside a directory | ntdll `file.c`, server `fd.c` |
| process and CPU information | no /proc | sysctl(2) KERN_PROC, KERN_CPUSTATS | ntdll `process.c`, `system.c`, server `process.c` |
| change notifications | no inotify | libinotify (kqueue); server falls back to the name it opened a directory with | server `change.c` |
| local host name | no nss-myhostname | getaddrinfo() answered from interface addresses | ws2_32 `unixlib.c` |
| window manager | cwm activates windows on every EnterNotify | WM_TAKE_FOCUS ignored when focused; pointer grab waits only with a pressed button | winex11 `event.c` |

The wine changes live in `work/proton-port/wine` (git, branch `openbsd`) and
are exported as ports patches (`scripts/make-port-patches.sh`); each patch
file starts with an explanation.

## Graphics paths

- Direct3D 8–11 through wined3d → OpenGL → Mesa (works without a GPU via
  llvmpipe, tested).
- Direct3D 9–11 through DXVK → Vulkan → Mesa radv/anv (needs real hardware,
  untested); Direct3D 12 through vkd3d-proton → Vulkan (same).
- The launcher chooses per DLL with WINEDLLOVERRIDES (DXVK by default,
  `PROTON_USE_WINED3D=1` for wined3d).

## Sound and input

winepulse.drv → libpulse → pulseaudio (autostarted) → sndiod. Keyboard,
mouse and windows through winex11 (Xenocara). Game controllers through
SDL2 (`--with-sdl`), untested.
