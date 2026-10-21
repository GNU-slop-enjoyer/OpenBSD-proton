#!/bin/sh
# Dry-run an OpenBSD port's patches against another source tree and report
# which apply cleanly, which need offset/fuzz, and which fail.
#
# usage: scripts/check-port-patches.sh SRCTREE [PATCHDIR]
#   PATCHDIR defaults to upstream/ports/emulators/wine/patches.
#
# OpenBSD ports patches are relative to WRKSRC (patch -p0 with a/ b/ style
# "path.orig" headers), which is what the ports framework uses as well.
# The source tree is never modified (--dry-run).
set -eu

TOP=$(cd "$(dirname "$0")/.." && pwd)
SRC=${1:?usage: $0 SRCTREE [PATCHDIR]}
PATCHDIR=${2:-$TOP/upstream/ports/emulators/wine/patches}

clean=0 fuzzy=0 failed=0
for p in "$PATCHDIR"/patch-*; do
	name=$(basename "$p")
	out=$(cd "$SRC" && patch -p0 -E -s --dry-run --forward < "$p" 2>&1) &&
	    rc=0 || rc=$?
	if [ $rc -ne 0 ]; then
		failed=$((failed + 1))
		echo "FAIL  $name"
		echo "$out" | sed 's/^/      /'
	elif [ -n "$out" ] || (cd "$SRC" && patch -p0 --dry-run --forward \
	    < "$p" 2>&1 | grep -q -E 'offset|fuzz'); then
		fuzzy=$((fuzzy + 1))
		echo "FUZZ  $name"
	else
		clean=$((clean + 1))
		echo "OK    $name"
	fi
done
echo "summary: $clean clean, $fuzzy offset/fuzz, $failed failed"
[ $failed -eq 0 ]
