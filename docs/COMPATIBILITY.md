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
- **Unix-side `NtCurrentTeb()`** (`unix_private.h`): reads the TLS slot
  directly. Wine 11.0 Unix code calls `NtCurrentTeb()` (`%gs:0x30`) far more
  than 11.19's, and a first-time `%gs` fault inside the SIGSEGV handler
  (SIGSEGV blocked) would kill the process.

## Proton-specific Linux features to check

- `dlls/ntdll/unix/fsync.c` (futex_waitv) and esync (eventfd): Linux-only;
  confirm they compile out / stay disabled on OpenBSD.
