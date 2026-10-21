# Platform compatibility work

## OpenBSD constraints that shape wine (from source + commit history)

Source: `upstream/ports/emulators/wine/patches`, and the topic commits on
`github.com/NethIafin/wine` branch `openbsd-master` (`33de78813f`, 2026-10-20;
blobless clone in `upstream/nethiafin-wine`, `git log wine-11.19..HEAD`).

| Constraint | Wine adaptation | Commits (NethIafin/wine) |
|---|---|---|
| Stack pointer must be inside a `MAP_STACK` mapping at every syscall/fault, or the kernel kills the process | Native thread stacks come from libpthread (`pthread_attr_setstacksize`; base from `pthread_stackseg_np`); Windows stacks handled separately | fa455ed521, b30c5edb4d, 5499ee27ff, e3a23f3dbf |
| No user-settable `%gs` base | Decode and rewrite `%gs:` instructions on fault (`gs_openbsd.c`) | 170254738e, adfc3e9e32, 70499fd021, 5b12d98254, 589ef1cb33, 4a9263fc5a |
| ptrace limited to descendants by default | `APC_COPY_MEMORY` server-mediated cross-process memory copy | 4809c10a03, b11626b775 |
| Signal delivery to specific threads of another process | Server keeps (signal, tid) targets; `sig_openbsd.c` | b8e5c01f1d, 2260017d64, 4c6c02a908 |
| W^X | loader linked `-z wxneeded`, PIE; build on `wxallowed` fs | 84e91a974c |
| Clang return protector on by default | `-fno-ret-protector` for PE code | 84e91a974c |
| 100 Hz scheduler clock | winepulse timer catch-up | 2a3b4d18e5 |
| Xenocara X server privsep, no SysV shm attach | `use_egl = FALSE` default | 65c7bf22c0 |

## Rebase onto Proton's wine (`ValveSoftware/wine` @ `e60892cc`, wine 11.0 base)

Work tree: `work/proton-port/wine` (branch `openbsd`, local clone, not pushed).
Measured with `scripts/check-port-patches.sh` (log
`logs/patchcheck-proton-wine.log`): 12 clean, 18 fuzz, 12 failed patches
(30 of 80 hunks in failing patches).

Resolved so far:
- `configure.ac`: OpenBSD SDL2 soname case, keeping Valve's `$SDL2_LIBS`.
- `dlls/ntdll/Makefile.in`: add `unix/gs_openbsd.c` (`UNWIND_LIBS` already present).
- `server/process.h`: `signal_targets` member.
- `dlls/winex11.drv/x11drv_main.c`: `use_egl = FALSE` on OpenBSD.
- `include/wine/server_protocol.h`, `server/request_*.h`: regenerated with
  `tools/make_requests` (protocol version 931). Proton's build regenerates these too.

Judged not applicable (needs runtime confirmation):
- winepulse timer patch: Valve schedules periods from PulseAudio
  `pa_context_rttime_restart` absolute deadlines; `pulse_timer_loop` is a no-op.
  Confirmed at run time (session 3, `tests/pe/audio.c`): event-driven
  WASAPI gets an event every 10.0 ms on average (max 20 ms), the audio
  clock runs at 0.997–0.998 of real time, no underruns.
- winex11 keyboard `keyc2scan()` OpenBSD keycodes: no such function in the 11.0 tree.

Ported by hand (work tree commits `43e3b82`, `3247aa5`; **not compiled yet**):
- **Thread stacks** (`thread.c`, `virtual.c`, `unix_private.h`): in 11.0 all
  stacks come from `virtual_alloc_thread_stack()`, which now remaps them
  `MAP_STACK` (`set_stack_mapping`). Secondary threads run on a
  libpthread-allocated stack: OpenBSD's `pthread_attr_setstack()` writes every
  page of the caller's range and re-mmaps it (`lib/librthread/rthread_attr.c`),
  which faults on Wine's read-only guard page. Their `kernel_stack` is set
  from `pthread_stackseg_np()` in `start_thread()`; the initial thread keeps
  its Wine kernel stack; `virtual_free_teb()` no longer frees kernel stacks on
  OpenBSD. Signal stacks need nothing: `sigaltstack(2)` calls
  `uvm_map_remap_as_stack()` (`sys/kern/kern_sig.c:573`).
- **Signals** (`signal_x86_64.c`): sigcontext trapno/err reconstruction,
  `%gs` stub exit and rewrite in SEGV, native-thread guards in INT/QUIT/USR1,
  SIGUSR2 relay handler. Valve's `set_thread_teb()` called Linux `arch_prctl`
  unconditionally; on OpenBSD it now stores the TEB in the loader TLS slot and
  initialises the slot offset on first use (it runs before
  `signal_init_process()`).
- ~~Unix-side `NtCurrentTeb()` override~~ — removed after the first
  compile: with `WINE_UNIX_LIB` Unix code calls the out-of-line
  `NtCurrentTeb()` in `thread.c` (`pthread_getspecific(teb_key)`), never
  `%gs` (`include/winnt.h`). Only the signal handlers' own code paths are
  `%gs`-sensitive.

## Proton-specific code that assumed Linux/glibc (fixed; compile status in TESTING.md)

Found by compiling `emulators/proton-wine` in the VM; each is one work-tree
commit and one port patch with a header.

| Code | Problem | Fix |
|---|---|---|
| `ntdll/unix/fsync.c`, `server/fsync.c` | Linux futex/futex_waitv via `syscall(2)` (OpenBSD libc has no `syscall`) | `__linux__` guard, ENOSYS stubs; fsync never enabled elsewhere |
| `ntdll/unix/sync.c`, `server/inproc_sync.c` | `ntsync_tmp.h` includes `<linux/types.h>` | include only on Linux; existing server fallback used |
| `ntdll/unix/loader.c` | `<malloc.h>`, `mallopt(M_PERTURB)` (glibc) | `__GLIBC__` guard |
| `ntdll/unix/signal_x86_64.c` | `set_thread_teb()` used `arch_prctl`; `jmp %rcx` rejected by clang's assembler | OpenBSD TLS-slot path; `jmp *%rcx` |
| `ntdll/unix/socket.c` | `uintptr_t` without `<stdint.h>`; `TCP_KEEPINTVL`/`TCP_KEEPCNT` | upstream-style include and guards |
| `ntdll/unix/system.c` (OpenBSD hunk) | 11.19 `cpu_count` global absent in 11.0 | `peb->NumberOfProcessors` |
| `winebus.sys/Makefile.in` | missing `INOTIFY_CFLAGS` (pre-dates upstream fix) | backport |
| `winegstreamer/media-converter` | `pthread_mutex_t` without `<pthread.h>` | include |
| `winedmo` PCM byte-order BSF | defines an FFmpeg bitstream filter via FFmpeg-internal layouts; only works with libavcodec < 60 | public-API path for FFmpeg ≥ 6: null filter + in-place sample reversal |
| `configure.ac` pcap check (11.0) | ignores `PCAP_CFLAGS/LIBS` | port adds libpcap dirs to `CPPFLAGS`/`LDFLAGS` |

## Runtime bugs found by DXVK testing (session 2)

Found by running a D3D11 probe (`tests/pe/d3d11probe.cpp`) with DXVK under
proton-wine. The first two are wine bugs that upstream wine already fixed
after 11.0; the third is a DXVK bug still present upstream.

| Code | Problem | Fix |
|---|---|---|
| `winex11.drv/opengl.c` `X11DRV_WineGL_InitOpenglInfo` | kept `glGetString(GL_EXTENSIONS)` after destroying the GLX context; page fault in `x11drv_init_wgl_extensions` during `vkCreateInstance` | backport wine `f68eb9e43d` (strdup) |
| `winex11.drv/display.c` `strip_driver_extra` | compacts modes in place with `memcpy` (overlapping). **OpenBSD libc's memcpy aborts on overlap** (syslog: "backwards memcpy"), glibc tolerates it; killed explorer.exe and the probe | backport wine `1897b27f25` (memmove) |
| dxvk `src/util/util_singleton.h` | `acquire()` counted a user before constructing; when `DxvkInstance` threw (no Vulkan device) the next `acquire()` returned null → access violation in dxgi.dll | count only after construction (`ports/graphics/dxvk/patches`) |

Finding the memcpy caller: `scripts/debug/memcpy-overlap.c` is an
LD_PRELOAD shim that prints a module+offset backtrace for every overlapping
`memcpy` and then copies with `memmove`. The abort itself leaves no stderr
output; look in `/var/log/messages` for "backwards memcpy". Other
overlapping copies in wine would fail the same way on OpenBSD, so the shim is
worth running when a process dies with SIGABRT and no message.

## Windows-target toolchain on OpenBSD (`ports/devel/mingw-w64`)

| Issue | Detail | Handling |
|---|---|---|
| clang freestanding headers missing | `devel/llvm/22` lists `float.h inttypes.h iso646.h limits.h stdarg.h stdbool.h stddef.h stdint.h varargs.h` as `@comment` in `PLIST-main` (base libc provides them for OpenBSD targets). mingw-w64's `float.h` does `#include_next <float.h>` | private clang resource dir (`-resource-dir`) = installed one + those 9 headers from the same LLVM source tarball. (emulators/wine solves the same problem with a separate `wine-clang-pe-headers` distfile) |
| compiler-rt builtins location | must not write into the llvm package's resource dir | installed into the private resource dir |
| meson flag probing | meson's `get_supported_arguments()` adds `-Werror=unused-command-line-argument`; wrapper flags `-rtlib/-unwindlib/-stdlib` are unused when compiling, so **every probed flag was silently dropped** (DXVK built without `-DDXVK_WSI_WIN32`, `-msse2`, …, then failed at run time with "DXVK_WSI_DRIVER environment variable unset") | wrap them in `--start-no-unused-arguments … --end-no-unused-arguments` (as llvm-mingw does) |
| `-lpthread` | meson `dependency('threads')` → mingw driver links `-lpthread` | build mingw-w64 winpthreads |

## Narrow syscall arguments and clang (found by winetest, session 2)

`kernel32_test drive` spun forever under proton-wine: `GetLogicalDrives()`
enumerates `\DosDevices` with `NtQueryDirectoryObject(…, single_entry=TRUE,
restart=FALSE, &ctx)`, and the server always received index 0.

Cause: the x86_64 syscall dispatcher (`dlls/ntdll/unix/signal_x86_64.c`)
moves the Windows caller's arguments into SysV registers with 64-bit
`movq`s. The Windows ABI leaves the upper bits of 8/16-bit arguments
undefined (kernelbase stores `restart` with `movb $0x0,0x20(%rsp)`). Clang
compiles SysV callees assuming 8/16-bit arguments are already extended to
32 bits — `NtQueryDirectoryObject` tests `restart` with `testl %r8d,%r8d` —
while GCC (Linux builds, the Steam runtime) re-extends in the callee. So on
any clang-built Unix side (OpenBSD, also macOS) a FALSE can read as TRUE
depending on stack garbage. Verified with a minimal test: clang emits
`cmpl $1,%r8d` for a SysV `unsigned char` argument and `cmpb` for an
`ms_abi` one. wine-11.19's ntdll.so has the identical `testl` and passes
the drive test only because that stack slot happens to be clean.

Fix (proton-wine p1): `SYSCALL_NARROW_ARG()` in `include/wine/unixlib.h`
(an empty `asm` with a `"+r"` operand on x86_64 clang) at the entry of
every implemented syscall with such arguments: 44 functions, 51 arguments
in ntdll and win32u, listed by `scripts/debug/narrow-syscall-args.py`
(it parses the definitions of all 738 implemented syscalls in the
generated syscall tables). Candidate for emulators/wine and upstream wine.

## Fonts in Proton prefixes (found by winetest, session 2)

Valve's wine.inf registers the system fonts as plain file names
(`"Tahoma (TrueType)"="tahoma.ttf"`, `"Arial (TrueType)"="arial.ttf"`,
…), i.e. files in `C:\windows\Fonts`, which Proton's `proton` script fills
with symlinks to its own fonts and to wine's (`create_fonts_symlinks()`).
DirectWrite builds its system collection only from that registry key. In
a prefix made by plain `wineboot`, the files are missing: `dwrite_test
font`/`analyzer` fail (DWRITE_E_NOFONT for Tahoma) and crash; upstream
wine-11.19 passes in the same situation because its wine.inf does not
register them this way.

Ordering matters: if the links are created *after* the first boot,
win32u has already recorded wine's fonts as "External Fonts" (data
directory paths); on the next start it finds them in `C:\windows\Fonts`,
treats the external entries as stale and deletes them — together with the
wine.inf values of the same name — so Tahoma vanishes from the registry.
`proton-openbsd` therefore links wine's fonts (and `PROTON_FONTS_DIR`)
before `wineboot -i`, like Proton. With that, both dwrite units pass
(0 failures). Proton's own fonts are packaged as `fonts/proton-fonts` (built like
Proton's Makefile: fontforge renames Liberation to Arial/Times New
Roman/Georgia/Courier New, `merge.py` with fontTools builds Microsoft Sans
Serif and Nirmala UI from Noto); the launcher links them too. Proton's CJK fonts
(msyh, simsun, malgun, msgothic) are built with fontTools from Adobe's
prebuilt Source Han Sans OTFs and Proton's Ume fonts instead of afdko.

## Sockets (found by `ws2_32_test sock`, session 2)

| Difference | Effect | Status |
|---|---|---|
| getsockopt of boolean options returns the flag bit (`so_options & SO_OOBINLINE` = 0x100, `t_flags & TF_NODELAY` = 4, `l_onoff` = 0x80) | Windows programs comparing with 1/TRUE misbehave; with a 1-byte buffer a set option reads as 0 | **fixed** (p2): `do_getsockopt_bool()`, `l_onoff` normalised |
| IP_MULTICAST_TTL / IP_MULTICAST_LOOP are `u_char` and must be set with length 1 (`ip_setmoptions`: `m_len != 1` → EINVAL) | setting them the Windows way (int) failed with WSAEINVAL — LAN discovery/multicast | **fixed** (p2) on OpenBSD |
| other options reject lengths ≠ sizeof(int) | only odd-length probes in the test | open (edge case) |
| no per-socket TCP_KEEPIDLE/TCP_KEEPCNT/TCP_KEEPINTVL (sysctl only) | SIO_KEEPALIVE_VALS/TCP_KEEPALIVE cannot be honoured | platform limitation |
| connect() on a TCP socket whose previous connect failed returns EINVAL (Windows allows the retry) | a retrying client never connects; the test hangs in accept() | fixed (p3): wineserver `sock_recreate_unix_socket()` replaces the Unix socket before a retried connect; fd caching is deferred until connected |

## 32-bit installers (found with real applications, session 2)

Installers of 64-bit programs are often 32-bit executables (7-Zip
`7z2301-x64.exe`, Notepad++ `npp.*.Installer.x64.exe` are i386 PE
stubs). They cannot run on OpenBSD (FEASIBILITY.md §2); wine only reports
"failed to load …syswow64\ntdll.dll c0000135". `proton-openbsd run` now
detects 32-bit PE files and says so. Workarounds: the program's `.msi`
(64-bit msiexec works, e.g. 7-Zip) or a portable archive (Notepad++).

## Directory change notifications and file names (session 2)

| Issue | Fix |
|---|---|
| wineserver needs a path to add an inotify watch; `get_path_from_fd()` only knew `/proc/self/fd` and `F_GETPATH`, neither exists on OpenBSD, so no watch was ever added (ReadDirectoryChangesW / FindFirstChangeNotification never fired) | fall back to the name the server opened the directory with (`get_fd_unix_name()`); kernel32 change 82 → 9 failures. Applies to emulators/wine too |
| non-ASCII Windows file names are unrepresentable in the C locale (US-ASCII), the default on OpenBSD | proton-openbsd sets `LC_CTYPE=C.UTF-8` if the locale is not UTF-8 |
| reparse points (FSCTL_SET/GET_REPARSE_POINT; Windows symlinks/junctions) are only partly emulated without extended attributes | known limitation (~100 ntdll file test failures) |

## Signal-context safety (found by winetest, session 2)

System APCs (another process mapping, allocating, reading or writing this
process's memory) can run in the SIGUSR1 handler that interrupted
arbitrary code.

| Issue | Fix |
|---|---|
| `get_mapping_info()` malloc()s; OpenBSD's malloc aborts on re-entry ("malloc(): recursive call") where glibc would rarely deadlock | first query into a stack buffer; allocate only for image mappings |
| OpenBSD's cross-process copy (APC_COPY_MEMORY, used because OpenBSD cannot ptrace non-children) copied with fault handling that does not work in signal context; an invalid range killed the target ("nested exception on signal stack") | `virtual_uninterrupted_read/write_memory()` (page protections checked first); partial writes report the bytes written, partial reads 0, as on Windows |
| programs that switch the stack pointer onto memory they allocated themselves (not via RtlCreateUserStack/fibers) fault fatally on OpenBSD, which requires the stack pointer to lie in a MAP_STACK mapping at faults and syscalls | known limitation (fibers and thread stacks are MAP_STACK) |

## Host information and debugging without ptrace (found via the Linux reference run, session 2)

| Issue | Fix |
|---|---|
| process memory counters (GetProcessMemoryInfo, NtQueryInformationProcess ProcessVmCounters, NtQuerySystemInformation process list) were all 0: only Linux /proc, macOS and libprocstat had implementations | ntdll `fill_vm_counters()` and wineserver `get_process_vm_counters` read `kinfo_proc` from `sysctl(KERN_PROC_PID)`: working set = `p_vm_rssize`, private usage = data + stack (anonymous mmap is counted in data), virtual size as ps(1) VSZ (`p_vm_map_size` is not filled in for unprivileged callers) |
| per-CPU and idle times (SystemProcessorPerformanceInformation, GetSystemTimes) were wine's fake counters 1/2/3, so CPU-load calculations were meaningless | `kern.cpustats` per online CPU (`KERN_CPUSTATS`, skips offline/SMT-disabled CPUs) at `kern.clockrate` stathz; kernel time includes idle as on Windows |
| wineserver writes PEB `BeingDebugged` with ptrace(); OpenBSD only allows ptrace on descendants (`kern.global_ptrace=0`) and clients are not the server's children. DebugActiveProcess failed with access denied (no AeDebug/winedbg attach), and **every process created with DEBUG_PROCESS aborted** at startup (`assert(!status)` after init_process_done) | new system APC `APC_WRITE_MEMORY` (≤ 8 bytes inline): the target writes its own memory; queued while the target is suspended (debugger attach/detach), so it runs before it resumes. init_process_done returns `debugged` and the new process sets its own flag (an APC there would race the client's check). Tested: `tests/pe/sysinfo.c` 24/24 |
| libgcrypt missing from configure: Proton's bcrypt does ECDH secret agreement with gcrypt (`STATUS_NOT_IMPLEMENTED` otherwise) | port dependency security/libgcrypt + `--with-gcrypt` (p6) |

## Window manager focus loop (session 3)

| Issue | Fix |
|---|---|
| Under cwm (OpenBSD base's default window manager) an idle wine program, explorer.exe and wineserver used most of a CPU. Proton's `set_focus()` calls `wait_grab_pointer()`, which grabs and ungrabs the pointer; the ungrab generates an EnterNotify (mode NotifyUngrab), cwm activates the client on every EnterNotify without checking the mode and sends WM_TAKE_FOCUS, which calls `set_focus()` again. Upstream wine-11.19 has no `wait_grab_pointer()` | winex11 ignores WM_TAKE_FOCUS when the window already is the foreground window and owns the X input focus (p7). cwm could also check `e->mode` in its EnterNotify handler (not changed; base system) |
| The same mechanism with two Wine windows (e.g. two conhost windows): each WM_TAKE_FOCUS → `set_focus()` → `wait_grab_pointer()` grab/ungrab → EnterNotify → cwm activates the window under the pointer → WM_TAKE_FOCUS for the other window; `_NET_ACTIVE_WINDOW` changed every millisecond and the conhosts never got to serve their consoles | `wait_grab_pointer()` only grabs while a mouse button is pressed (`XQueryPointer`), since window-manager move/resize grabs need one (p8) |

## Debugging tools on OpenBSD (session 3)

- `winedbg` works now that debugger attach works (p6): `winedbg --command
  "info process"`, then `printf 'attach 0x<pid>\nbt 0x<tid>\ndetach\n' |
  winedbg` (per thread: `bt all` makes winedbg itself crash on some
  frames).
- lldb (base) needs a raised data-size limit (`ulimit -d $(ulimit -H -d)`)
  or it aborts with "LLVM ERROR: out of memory"; it reads wine cores fine,
  live attach to wine processes produced no output.
- `ktrace -i -t S` records pinsyscalls(2) violations (the kernel's
  explanation is otherwise only uprintf'd to the controlling tty).
