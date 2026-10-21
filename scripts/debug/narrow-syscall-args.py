#!/usr/bin/env python3
"""List Unix-side wine syscalls that take 8- or 16-bit arguments by value.

On x86_64 the syscall dispatcher (dlls/ntdll/unix/signal_x86_64.c) moves
the Windows caller's arguments into SysV registers with 64-bit moves.  The
Windows ABI leaves the upper bits of narrow arguments undefined, but clang
compiles SysV callees assuming 8/16-bit arguments are zero/sign-extended to
32 bits (GCC does not).  With a clang-built Unix side (OpenBSD, macOS) every
such parameter can read as a different value.  This prints the affected
functions so the fix can be checked for completeness.

usage: narrow-syscall-args.py WINE_TREE SYSCALLS_H [SYSCALLS_H ...]
  SYSCALLS_H: generated dlls/ntdll/ntsyscalls.h, dlls/win32u/win32syscalls.h
"""
import os
import re
import sys

NARROW = {
    "BOOLEAN", "BYTE", "UCHAR", "CHAR", "CCHAR", "KPROCESSOR_MODE",
    "USHORT", "WORD", "WCHAR", "SHORT", "ATOM", "LANGID", "INT8", "UINT8",
    "INT16", "UINT16", "bool", "char", "short",
}


def syscall_names(paths):
    names = set()
    for p in paths:
        for m in re.finditer(r"SYSCALL_ENTRY\(\s*0x[0-9a-f]+,\s*(\w+),",
                             open(p, errors="replace").read()):
            names.add(m.group(1))
    return names


def definitions(tree, dirs):
    # "NTSTATUS WINAPI NtFoo( type a, ... )\n{" at file scope
    pat = re.compile(r"^[A-Za-z_][\w \*]*?\b(?:WINAPI|SYSCALL_API)\s+(\w+)\s*"
                     r"\(([^;{]*?)\)\s*\{", re.M | re.S)
    for d in dirs:
        for root, _, files in os.walk(os.path.join(tree, d)):
            for f in files:
                if not f.endswith(".c"):
                    continue
                path = os.path.join(root, f)
                text = open(path, errors="replace").read()
                for m in pat.finditer(text):
                    line = text.count("\n", 0, m.start()) + 1
                    yield m.group(1), m.group(2), os.path.relpath(path, tree), line


def narrow_params(params):
    out = []
    for p in params.split(","):
        p = re.sub(r"/\*.*?\*/", "", p).strip()
        if not p or "*" in p or "[" in p:
            continue
        words = re.sub(r"\b(const|volatile|unsigned|signed)\b",
                       lambda m: "" if m.group(1) in ("const", "volatile")
                       else m.group(1), p).split()
        if len(words) < 2:
            continue
        typ = " ".join(words[:-1])
        base = typ.replace("unsigned ", "").replace("signed ", "")
        if base in NARROW:
            out.append(f"{typ} {words[-1]}")
    return out


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    tree = sys.argv[1]
    names = syscall_names(sys.argv[2:])
    seen = set()
    for name, params, path, line in definitions(
            tree, ["dlls/ntdll/unix", "dlls/win32u"]):
        if name not in names or name in seen:
            continue
        seen.add(name)
        narrow = narrow_params(params)
        if narrow:
            print(f"{path}:{line}: {name}: {', '.join(narrow)}")
    missing = sorted(names - seen)
    print(f"# {len(seen)} of {len(names)} syscalls found; "
          f"{len(missing)} not found by the parser", file=sys.stderr)


if __name__ == "__main__":
    main()
