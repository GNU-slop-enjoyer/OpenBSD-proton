# Proposed change for OpenBSD ports `emulators/wine` (wine-11.19)

Not sent anywhere yet. Prepared from the openbsd-proton project, where the
same bug was found and fixed in `emulators/proton-wine`
(docs/COMPATIBILITY.md, "Narrow syscall arguments and clang").

## Problem

On x86_64, wine's syscall dispatcher passes the Windows caller's argument
registers and stack slots to the Unix-side `Nt*` functions as 64-bit
values. The Windows ABI leaves the upper bits of 8/16-bit arguments
undefined. Clang (OpenBSD's compiler) compiles SysV functions assuming
the caller zero/sign-extended such arguments to 32 bits; GCC does not.
`NtQueryDirectoryObject` in wine-11.19's `ntdll.so` tests `restart` with
`testl %r8d,%r8d` while kernelbase stores it with `movb $0x0,0x20(%rsp)`,
so a FALSE `restart` reads as TRUE whenever that stack slot holds garbage
(an endless `GetLogicalDrives()` loop in proton-wine's `kernel32_test
drive`; wine-11.19 passes that test only by luck of the stack contents).

## Change

`SYSCALL_NARROW_ARG()` in `include/wine/unixlib.h` (an empty asm with a
`"+r"` operand on x86_64 clang) at the entry of each of the 46 implemented
ntdll/win32u syscalls with BOOLEAN/WORD/BYTE/WCHAR/ATOM/LANGID arguments
(53 arguments), found by `scripts/debug/narrow-syscall-args.py` over all
748 implemented syscalls of wine-11.19's generated syscall tables.

- `narrow-syscall-args.diff`: plain diff against wine-11.19 with the port's
  42 patches applied (15 files).
- `patches/`: the port's complete patch set with that change folded in
  (`make-port-patches.sh` output for the 15 touched files; hunk context
  lines differ cosmetically from `make update-patches` output).

## Status

- Applied to and tested in `emulators/proton-wine` (wine 11.0 base):
  codegen checked (`testb %r8b,%r8b`), `kernel32_test drive` passes, full
  winetest re-run with no new failures attributable to it (docs/TESTING.md).
- For wine-11.19 (2026-10-21, OpenBSD 8.0 VM): the port with `patches/`
  builds and packages (wine-11.19p1 locally; no new warnings: the same 8
  implicit-declaration warnings as before), `ntdll.so` now tests
  `restart` with `testb`, `kernel32_test drive` and `ntdll_test om`
  pass, `basic.exe` 6/6 and `gui.exe` 5/5; sampled units ntdll file,
  user32 win, kernel32 sync show no new failures. Not sent anywhere:
  sending needs the project owner's OK.

## Further fixes (openbsd-fixes.diff, folded into patches/)

Found in proton-wine, ported to wine-11.19 (same code paths):
directory change notifications (wineserver had no path for watched
directories), boolean/multicast socket options, reconnect after a failed
connect, malloc in signal context (get_mapping_info), and the
cross-process copy APC's fault handling, and (second batch) process
memory counters and CPU times from sysctl(2), and debugging without
ptrace: a new APC_WRITE_MEMORY and an init_process_done "debugged" reply
field replace the server's ptrace() write of PEB BeingDebugged.
ws2_32 resolves the local host name (and "") from the interface addresses
when the system cannot (no nss-myhostname on OpenBSD; ws2_32 protocol test
19 -> 1 failures in proton-wine).
Exception/thread contexts report the real %fs selector (OpenBSD's
sigcontext has none; 40 ntdll exception test failures in proton-wine).
Third batch (2026-10-22, not built for 11.19 yet; tested in proton-wine):
relayed signals for the calling thread are delivered when its server call
returns (system APCs queued during the call ran late through the SIGUSR2
relay), and precise Sleep() for processes that call timeBeginPeriod(<10)
or set WINE_PRECISE_SLEEP=1 (the 100 Hz clock made Sleep(1) take 20 ms).
Directory reparse points (symlinks/junctions to directories) and any
reparse tag; in proton-wine kernel32_test file went from a crash to 0
failures and ntdll_test file from 124 to 27 failures.
Fourth batch (2026-10-22): %gs instructions shorter than 5 bytes (e.g.
mov %gs:(%rcx),%rax) get a jmp rel8 to int3 padding between functions that
holds the jmp rel32 to their stub, instead of faulting on every execution
(tests/pe/gsbench.c: ~5 us -> 5.3 ns when such padding is in reach;
gs_openbsd.c is identical in both ports, so the patch is the same file).
Fifth batch (2026-10-22): precise timed waits on the server path in
precise-sleep mode (wine 11.19 has no fsync, so every timed wait goes to
the server): in the 11.19 build tree WaitForSingleObject(ev, 1) 20 ms ->
1.00 ms with WINE_PRECISE_SLEEP=1, unchanged by default; kernel32 sync 0
failures with WINE_PRECISE_SLEEP=1. Not packaged for 11.19.
Also (not OpenBSD-specific, present in upstream master as of 2026-10-22):
winex11 formats the Xkb layout key "%s:%s" with a NULL variant; OpenBSD's
libc logs 'vfprintf %s NULL in "%s:%s"' to syslog for every process that
loads winex11 (193 lines in the VM's /var/log/messages). Fixed with an
empty variant; a fresh wineboot in the 11.19 build tree logs nothing.
This one could go to WineHQ directly.
Likewise wineserver's request trace passes a NULL cur_data to %.*s for
empty strings (`WINEDEBUG=+server`), logged the same way; fixed in
server/trace.c (patches/patch-server_trace_c, new: the port does not
patch that file).

Status:
- First batch: wine-11.19p2 built and tested in the VM (2026-10-21):
  change notifications 82/26 → 9/9 failures, ntdll virtual and om pass,
  kernel32 virtual and ws2_32 sock complete (both timed out before).
- The same run showed the debugger bug on stock 11.19: every process
  created with DEBUG_PROCESS aborts (`assertion "!status" failed ...
  server_init_process_done`, 9× in ntdll_test info). Second batch is in
  openbsd-fixes.diff and patches/ (58 files; the generated
  server_protocol.h, request_handlers.h and request_trace.h are patched
  directly, as the port does not run make_requests).
- Full bundle built and tested as local wine-11.19p3 (2026-10-21): no
  assertion in ntdll_test info, tests/pe/sysinfo.c 25/25, basic 6/6,
  gui 5/5, ws2_32 protocol 19 -> 1 (a passing todo), ntdll exception
  without the 40 %fs mismatches, ntdll om/virtual 0, kernel32 console 0.
  Same 8 implicit-declaration warnings as the original port. Not sent
  anywhere: sending needs the project owner's OK.
- Batches 1-3 built and tested as local wine-11.19p4 (2026-10-22; test
  binaries from the proton-wine 11.0 tree, so Proton-specific tests fail
  on stock 11.19): ntdll om 0 failures, kernel32 pipe 0, kernel32 sync 0,
  kernel32 file 1 (completes), ws2_32 protocol 1 (the passing todo),
  ntdll file 35, no debugger assertion; sysinfo 25/25, basic 6/6;
  Sleep(1) after timeBeginPeriod(1) 1.00 ms (20 ms before), Sleep(16)
  30 ms by default and 16.00 ms with WINE_PRECISE_SLEEP=1. ntdll info's
  860 failures are the Proton-only kernel-object-pointer test (info.c:957;
  1466 with p3), ntdll exception crashes in the Wow64 CPU-area part as with
  p3. Batch 4 (%gs padding) is not built for 11.19 yet.
