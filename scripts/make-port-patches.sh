#!/bin/sh
# Write OpenBSD ports-style patches (patches/patch-<path>) describing the
# differences between two prepared source trees, in the format produced by
# the ports framework's "make update-patches" ("Index:" header, ".orig"
# suffix on the old name, paths relative to WRKSRC, applied with -p0).
#
# usage: scripts/make-port-patches.sh ORIGTREE NEWTREE PATCHDIR [REFDIR]
#
# Existing patch-* files in PATCHDIR are replaced.  Files present only in
# NEWTREE become patches that create them.  The explanatory text above the
# "Index:" line is kept from the old patch of the same name, or else taken
# from REFDIR (e.g. upstream/ports/emulators/wine/patches).
set -eu

ORIG=$(cd "${1:?usage: $0 ORIGTREE NEWTREE PATCHDIR}" && pwd)
NEW=$(cd "${2:?}" && pwd)
PATCHDIR=${3:?}
REFDIR=${4:-}

mkdir -p "$PATCHDIR"
HEADERS=$(mktemp -d)
trap 'rm -rf "$HEADERS"' EXIT
for d in "$REFDIR" "$PATCHDIR"; do	# later wins: our own headers first
	[ -n "$d" ] && [ -d "$d" ] || continue
	for p in "$d"/patch-*; do
		[ -f "$p" ] || continue
		h=$(sed '/^Index: /,$d' "$p")
		[ -n "$h" ] && printf '%s\n\n' "$h" > "$HEADERS/$(basename "$p")"
	done
done
rm -f "$PATCHDIR"/patch-*

# Generated in the port (post-patch: tools/make_requests), never patched.
diff -rq -N -x PROTON_WINE_REVISION -x autom4te.cache \
    -x server_protocol.h -x request_handlers.h -x request_trace.h \
    "$ORIG" "$NEW" |
    sed -n "s|^Files $ORIG/\(.*\) and $NEW/.* differ\$|\1|p" | sort |
while read -r f; do
	name=patch-$(echo "$f" | sed 's|[/.]|_|g')
	{
		[ -s "$HEADERS/$name" ] && cat "$HEADERS/$name"
		echo "Index: $f"
		# GNU diff -N treats a missing file as empty
		diff -u -N -p --label "$f.orig" --label "$f" \
		    "$ORIG/$f" "$NEW/$f" || [ $? -eq 1 ]
	} > "$PATCHDIR/$name"
	echo "$name"
done
