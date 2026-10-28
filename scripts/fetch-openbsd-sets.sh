#!/bin/sh
# Fetch and verify OpenBSD release sets for the build/test VM.
#
# usage: scripts/fetch-openbsd-sets.sh [RELEASE] [ARCH]
#   RELEASE defaults to 8.0, ARCH to amd64.
#
# Sets land in openbsd-sets/RELEASE/ARCH (ignored by git).  Each file is
# checked against SHA256.sig using the release's signify base key, taken from
# the OpenBSD source tree (OPENBSD_PUBKEY overrides the location).
set -eu

REL=${1:-8.0}
ARCH=${2:-amd64}
V=$(echo "$REL" | tr -d .)
MIRROR=${OPENBSD_MIRROR:-https://cdn.openbsd.org/pub/OpenBSD}
TOP=$(cd "$(dirname "$0")/.." && pwd)
DEST=$TOP/openbsd-sets/$REL/$ARCH
PUBKEY=${OPENBSD_PUBKEY:-$TOP/openbsd-sets/openbsd-$V-base.pub}

SETS="SHA256.sig BUILDINFO cd$V.iso bsd bsd.mp bsd.rd base$V.tgz comp$V.tgz man$V.tgz
	game$V.tgz xbase$V.tgz xshare$V.tgz xfont$V.tgz xserv$V.tgz"

if [ ! -f "$PUBKEY" ]; then
	# Extract the key from the release source tarball if present.
	if [ -f "$TOP/src.tar.gz" ]; then
		tar -xzOf "$TOP/src.tar.gz" "etc/signify/openbsd-$V-base.pub" \
		    > "$PUBKEY"
	else
		echo "missing $PUBKEY; set OPENBSD_PUBKEY" >&2
		exit 1
	fi
fi

mkdir -p "$DEST"
cd "$DEST"
for f in $SETS; do
	# -C - resumes partial downloads; SHA256.sig is always refreshed.
	if [ "$f" = SHA256.sig ]; then
		curl -fL --retry 3 -o "$f" "$MIRROR/$REL/$ARCH/$f"
	else
		curl -fL --retry 3 -C - -o "$f" "$MIRROR/$REL/$ARCH/$f"
	fi
done

# shellcheck disable=SC2086
if ! python3 -I "$TOP/scripts/signify-verify.py" "$PUBKEY" SHA256.sig . \
    $(echo "$SETS" | tr ' \t' '\n\n' | grep -v '^SHA256.sig$' | grep .); then
	echo "verification failed; delete bad files and re-run" >&2
	exit 1
fi
# The installer expects an index of the set directory.
ls -l > index.txt
