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
