#!/bin/sh
# Copy changed files from the proton-wine work tree into the VM's WRKSRC so
# an interrupted port build can resume without re-extracting.  Patches are
# still exported from the work tree afterwards (make-port-patches.sh).
#
# usage: scripts/vm/sync-wrksrc.sh FILE...   (paths relative to the tree)
set -eu

TOP=$(cd "$(dirname "$0")/../.." && pwd)
TREE=${TREE:-$TOP/work/proton-port/wine}
PKGPATH=${PKGPATH:-emulators/proton-wine}
WRKSRC=$("$TOP/scripts/vm/run.sh" ssh "cd /usr/ports/$PKGPATH && make show=WRKSRC")

for f in "$@"; do
	"$TOP/scripts/vm/run.sh" ssh "cat > '$WRKSRC/$f'" < "$TREE/$f"
	echo "synced $f"
done
