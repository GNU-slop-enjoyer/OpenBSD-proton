#!/usr/bin/env python3
"""Insert SYSCALL_NARROW_ARG() for narrow syscall arguments in a wine tree.

Reads the output of narrow-syscall-args.py ("path:line: Func: TYPE a, TYPE
b") and adds one SYSCALL_NARROW_ARG( arg ); per argument after the leading
declarations of each function.  Declarations whose initializer uses such
an argument (they run before the macro) are reported and must be split by
hand.  The macro itself is defined in include/wine/unixlib.h (see the
openbsd-proton patch-include_wine_unixlib_h).

usage: apply-narrow-syscall-args.py WINE_TREE INVENTORY
"""
import os
import re
import sys

TYPE = (r"(?:const\s+|volatile\s+|static\s+|unsigned\s+|signed\s+|struct\s+|"
        r"enum\s+|union\s+)*[A-Za-z_]\w*")
DECL = re.compile(r"^\s+" + TYPE +
                  r"[\s\*]+\**\s*[A-Za-z_]\w*\s*(\[[^\]]*\])?\s*(=|,|;|\[)")
NOT_DECL = re.compile(r"^\s+(return|if|for|while|do|switch|goto|case|break|"
                      r"else|SERVER_START_REQ|TRACE|WARN|FIXME|ERR)\b")


def load(inventory):
    entries = {}
    for line in open(inventory):
        m = re.match(r"(\S+):(\d+): (\w+): (.*)", line.strip())
        if not m:
            continue
        path, lineno, name, params = m.groups()
        args = [p.split()[-1] for p in params.split(", ")]
        entries.setdefault(path, []).append((int(lineno), name, args))
    return entries


def apply(tree, entries):
    problems = 0
    for path, items in sorted(entries.items()):
        full = os.path.join(tree, path)
        lines = open(full).read().split("\n")
        for lineno, name, args in sorted(items, reverse=True):
            i = lineno - 1
            while lines[i].strip() != "{":
                i += 1
            body = j = i + 1
            in_decl = False
            while True:
                s = lines[j]
                if in_decl or (s.strip() and DECL.match(s) and
                               not NOT_DECL.match(s)):
                    for a in args:
                        if re.search(r"=.*\b%s\b" % re.escape(a), s):
                            print(f"{path}:{j + 1}: {name}: '{a}' used in a "
                                  f"declaration before normalisation: "
                                  f"{s.strip()}")
                            problems += 1
                    in_decl = not s.rstrip().endswith(";")
                    j += 1
                    continue
                if not s.strip():
                    j += 1
                    continue
                break
            k = j
            while k > body and not lines[k - 1].strip():
                k -= 1
            indent = re.match(r"^(\s*)", lines[j]).group(1) or "    "
            new = [f"{indent}SYSCALL_NARROW_ARG( {a} );" for a in args]
            lines[k:k] = ([""] + new) if k > body else (new + [""])
        open(full, "w").write("\n".join(lines))
    return problems


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    problems = apply(sys.argv[1], load(sys.argv[2]))
    sys.exit(1 if problems else 0)


if __name__ == "__main__":
    main()
