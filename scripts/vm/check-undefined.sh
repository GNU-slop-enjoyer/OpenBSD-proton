#!/bin/sh
# Report undefined dynamic symbols in wine's Unix libraries that no library
# they can see defines.  Unix .so files are linked with unresolved symbols
# allowed, so a missing function (e.g. a glibc extension) only shows up when
# the library is loaded.  Runs on the OpenBSD guest.
#
# usage: check-undefined.sh WINEROOT
#   WINEROOT is an installed or fake tree containing bin/wine and
#   lib/wine/x86_64-unix (e.g. /usr/local/lib/proton-wine).
set -eu

ROOT=$1
UNIX=$ROOT/lib/wine/x86_64-unix
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

defined() {	# object -> defined dynamic symbols
	nm -D -g "$1" 2>/dev/null | awk '$2 ~ /^[TDBRVWiu]$/ { print $3 }'
}

# Symbols every unix library may use: the loader, ntdll.so, win32u.so and
# the base libraries that OpenBSD shared objects don't record as NEEDED.
{
	defined "$ROOT/bin/wine"
	for so in ntdll.so win32u.so; do defined "$UNIX/$so"; done
	for lib in c pthread m; do
		defined "$(ls /usr/lib/lib$lib.so.* | sort -V | tail -1)"
	done
} | sort -u > "$tmp/base"

status=0
for so in "$UNIX"/*.so; do
	cp "$tmp/base" "$tmp/avail"
	for lib in $(objdump -p "$so" | awk '$1 == "NEEDED" { print $2 }'); do
		path=$(ls /usr/lib/"$lib" /usr/local/lib/"$lib" \
		    /usr/X11R6/lib/"$lib" /usr/local/lib/*/"$lib" 2>/dev/null |
		    head -1) || true
		[ -n "$path" ] && defined "$path" >> "$tmp/avail"
	done
	sort -u -o "$tmp/avail" "$tmp/avail"
	# strong undefined references only (weak ones may stay unresolved)
	nm -D "$so" | awk '$1 == "U" { print $2 }' | sort -u > "$tmp/undef"
	missing=$(comm -23 "$tmp/undef" "$tmp/avail")
	if [ -n "$missing" ]; then
		echo "$(basename "$so"):" $missing
		status=1
	fi
done
exit $status
