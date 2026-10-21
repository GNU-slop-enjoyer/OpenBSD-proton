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

Status:
- First batch: wine-11.19p2 built and tested in the VM (2026-10-21):
  change notifications 82/26 → 9/9 failures, ntdll virtual and om pass,
  kernel32 virtual and ws2_32 sock complete (both timed out before).
- The same run showed the debugger bug on stock 11.19: every process
  created with DEBUG_PROCESS aborts (`assertion "!status" failed ...
  server_init_process_done`, 9× in ntdll_test info). Second batch is in
  openbsd-fixes.diff and patches/ (58 files; the generated
  server_protocol.h, request_handlers.h and request_trace.h are patched
  directly, as the port does not run make_requests); build pending.
  The same changes pass tests/pe/sysinfo.c 24/24 in proton-wine.
