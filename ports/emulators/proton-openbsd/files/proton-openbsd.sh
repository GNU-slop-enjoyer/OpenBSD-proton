#!/bin/sh
#
# proton-openbsd: run a 64-bit Windows program the way Valve's Proton does,
# with OpenBSD's packages: Proton's wine (emulators/proton-wine), DXVK
# (graphics/dxvk) and vkd3d-proton (graphics/vkd3d-proton).
#
# Follows the behaviour of upstream Proton's "proton" script
# (ValveSoftware/Proton proton-11.0-2e, setup_prefix() and the
# PROTON_* switches) for the parts that apply without Steam.
#
# Copyright (c) 2026 The OpenBSD Proton project contributors
# Distributed under the same terms as wine (LGPL 2.1 or later).

set -u

PREFIX_DIR=${TRUEPREFIX}
WINEDIR=${PROTON_WINE_DIR:-$PREFIX_DIR/lib/proton-wine}
DXVKDIR=${PROTON_DXVK_DIR:-$PREFIX_DIR/lib/dxvk/x64}
VKD3DDIR=${PROTON_VKD3D_DIR:-$PREFIX_DIR/lib/vkd3d-proton/x64}
WINE=$WINEDIR/bin/wine
STEAMCLIENTDIR=${PROTON_STEAMCLIENT_DIR:-$PREFIX_DIR/lib/proton-steamclient}

usage() {
	cat >&2 <<-EOF
	usage: ${0##*/} run program.exe [arg ...]
	       ${0##*/} wine-program [arg ...]   (e.g. winecfg, regedit, wineboot)
	       ${0##*/} prefix                   (create or update the prefix only)
	EOF
	exit 2
}

nonzero() {
	case ${1:-0} in
	""|0) return 1 ;;
	*) return 0 ;;
	esac
}

die() {
	echo "${0##*/}: $*" >&2
	exit 1
}

[ $# -ge 1 ] || usage
[ -x "$WINE" ] || die "$WINE not found (pkg_add proton-wine)"

# Users of the default "staff" login class get 1.5 GB of data and 512
# open files unless they raise the soft limits; games need more, wine's
# server keeps a descriptor per object.  Raise both to the class maximum.
ulimit -S -d "$(ulimit -H -d)" 2>/dev/null
ulimit -S -n "$(ulimit -H -n)" 2>/dev/null

# Windows file names are Unicode; wine maps them to Unix names in the
# locale's codeset, and in the default C locale (US-ASCII) any non-ASCII
# name is unrepresentable.  Use UTF-8 for LC_CTYPE unless it already is.
if [ "$(locale charmap 2>/dev/null)" != UTF-8 ]; then
	if [ -n "${LC_ALL:-}" ]; then
		export LC_ALL=C.UTF-8
	else
		export LC_CTYPE=C.UTF-8
	fi
fi

# In-process synchronization (fsync, on futex(2)) as Proton enables it,
# unless PROTON_NO_FSYNC is set.  Processes follow the setting of the
# prefix's running wineserver.
nonzero "${PROTON_NO_FSYNC:-}" || export WINEFSYNC=1

export WINEPREFIX=${PROTON_PREFIX:-${WINEPREFIX:-${XDG_DATA_HOME:-$HOME/.local/share}/proton-openbsd/pfx}}
SYS32=$WINEPREFIX/drive_c/windows/system32

# Which implementation provides each graphics DLL (Proton's setup_prefix()):
# DXVK for d3d9/d3d10core/d3d11/dxgi, vkd3d-proton for d3d12/d3d12core,
# wine's wined3d for d3d8 unless PROTON_DXVK_D3D8 is set, and wined3d for
# all of them with PROTON_USE_WINED3D.
dxvk_dlls=""
vkd3d_dlls=""
builtin_dlls=""
if nonzero "${PROTON_USE_WINED3D:-}" || nonzero "${PROTON_USE_WINED3D11:-}"; then
	builtin_dlls="d3d12 d3d12core d3d11 d3d10core d3d9 dxgi d3d8"
else
	[ -f "$DXVKDIR/d3d11.dll" ] && dxvk_dlls="d3d11 d3d10core d3d9 dxgi"
	[ -f "$VKD3DDIR/d3d12.dll" ] && vkd3d_dlls="d3d12 d3d12core"
	if nonzero "${PROTON_DXVK_D3D8:-}" && [ -f "$DXVKDIR/d3d8.dll" ]; then
		dxvk_dlls="$dxvk_dlls d3d8"
	else
		builtin_dlls="d3d8"
	fi
	[ -n "$dxvk_dlls" ] || builtin_dlls="$builtin_dlls d3d11 d3d10core d3d9 dxgi"
	[ -n "$vkd3d_dlls" ] || builtin_dlls="$builtin_dlls d3d12 d3d12core"
fi

# Copy $2/$1.dll into system32 unless it is already identical.
install_dll() {
	[ -f "$2/$1.dll" ] || return 0
	cmp -s "$2/$1.dll" "$SYS32/$1.dll" && return 0
	cp "$2/$1.dll" "$SYS32/$1.dll.tmp" && mv "$SYS32/$1.dll.tmp" "$SYS32/$1.dll"
}

setup_prefix() {
	if [ ! -d "$SYS32" ]; then
		mkdir -p "$WINEPREFIX" || die "cannot create $WINEPREFIX"
		# Fonts must be in place before the first boot: fonts that
		# win32u first finds in wine's data directory are recorded as
		# "external", and when they later appear in C:\windows\Fonts
		# their registry entries (and wine.inf's) are deleted, which
		# leaves DirectWrite without them.
		link_fonts
		# see the mscoree/mshtml note at set_overrides()
		WINEDEBUG=-all WINEDLLOVERRIDES="$ADDON_OVERRIDES" \
		    "$WINE" wineboot -i || die "wineboot failed for $WINEPREFIX"
		"$WINEDIR/bin/wineserver" -w
	fi
	for f in $dxvk_dlls; do install_dll "$f" "$DXVKDIR"; done
	for f in $vkd3d_dlls; do install_dll "$f" "$VKD3DDIR"; done
	for f in $builtin_dlls; do
		install_dll "$f" "$WINEDIR/lib/wine/x86_64-windows"
	done
	link_fonts
	install_steam
}

# Steam games: emulators/proton-steamclient's steamclient64.dll and
# steam.exe go where Proton puts Valve's, C:\Program Files (x86)\Steam.
STEAMDIR_WIN='C:\Program Files (x86)\Steam'
install_steam() {
	steamdir=$WINEPREFIX/drive_c/Program\ Files\ \(x86\)/Steam
	[ -f "$STEAMCLIENTDIR/steamclient64.dll" ] || return 0
	mkdir -p "$steamdir"
	for f in steamclient64.dll steam.exe; do
		cmp -s "$STEAMCLIENTDIR/$f" "$steamdir/$f" && continue
		cp "$STEAMCLIENTDIR/$f" "$steamdir/$f.tmp" &&
		    mv "$steamdir/$f.tmp" "$steamdir/$f"
	done
}

# Run Steam games (SteamAppId set, as proton-launcher does) under
# steam.exe so that their Steamworks API finds a running Steam client,
# unless PROTON_NO_STEAM is set.
use_steam() {
	[ -n "${SteamAppId:-}" ] && ! nonzero "${PROTON_NO_STEAM:-}" &&
	    [ -f "$WINEPREFIX/drive_c/Program Files (x86)/Steam/steam.exe" ]
}

# Valve's wine registers the system fonts as files in C:\windows\Fonts and
# Proton symlinks them there (create_fonts_symlinks()); without them
# DirectWrite finds no fonts at all, not even Tahoma.  Like Proton, only
# symlinks are replaced, never files the user put there.
FONTDIRS="$WINEDIR/share/wine/fonts ${PROTON_FONTS_DIR:-$PREFIX_DIR/share/proton-fonts}"
link_fonts() {
	fonts=$WINEPREFIX/drive_c/windows/Fonts
	mkdir -p "$fonts"
	for d in $FONTDIRS; do
		for f in "$d"/*.ttf "$d"/*.ttc; do
			[ -f "$f" ] || continue
			l=$fonts/${f##*/}
			if [ -L "$l" ] || [ ! -e "$l" ]; then
				ln -sf "$f" "$l"
			fi
		done
	done
}

# WINEDLLOVERRIDES: "n" for the DLLs we installed, after anything the
# user set; PROTON_NO_D3D11/PROTON_NO_D3D10 disable those APIs as Proton
# does.  Without emulators/proton-wine-addons (Wine Mono and Wine Gecko),
# mscoree and mshtml are disabled: otherwise wine offers to download them
# in a dialog that blocks until clicked, also when it updates a prefix
# after a proton-wine upgrade.
ADDON_OVERRIDES=""
[ -d "$WINEDIR/share/wine/mono/wine-mono" ] || ADDON_OVERRIDES="mscoree"
[ -n "$(ls -d "$WINEDIR"/share/wine/gecko/wine-gecko-*-x86_64 2>/dev/null)" ] ||
    ADDON_OVERRIDES="${ADDON_OVERRIDES:+$ADDON_OVERRIDES,}mshtml"
[ -n "$ADDON_OVERRIDES" ] && ADDON_OVERRIDES="$ADDON_OVERRIDES="
overrides="$ADDON_OVERRIDES"
add_override() {
	overrides="${overrides:+$overrides;}$1"
}
set_overrides() {
	local_dlls=$(echo $dxvk_dlls $vkd3d_dlls | tr ' ' ',')
	if nonzero "${PROTON_NO_D3D11:-}"; then
		local_dlls=$(echo "$local_dlls" | tr ',' '\n' |
		    grep -vxE 'd3d11|dxgi' | paste -sd, -)
		add_override "d3d11="
	fi
	if nonzero "${PROTON_NO_D3D10:-}"; then
		local_dlls=$(echo "$local_dlls" | tr ',' '\n' |
		    grep -vx dxgi | paste -sd, -)
		add_override "d3d10_1,d3d10,dxgi="
	fi
	[ -n "$local_dlls" ] && add_override "$local_dlls=n"
	[ -n "${WINEDLLOVERRIDES:-}" ] && add_override "$WINEDLLOVERRIDES"
	[ -n "$overrides" ] && export WINEDLLOVERRIDES="$overrides"
}

# PROTON_LOG=1 or a list of extra WINEDEBUG channels: log to
# $PROTON_LOG_DIR/proton-<program>.log, with Proton's default channels.
setup_log() {
	if nonzero "${PROTON_LOG:-}"; then
		: "${WINEDEBUG:=+timestamp,+pid,+tid,+seh,+unwind,+threadname,+debugstr,+loaddll}"
		[ "$PROTON_LOG" != 1 ] && WINEDEBUG="$WINEDEBUG,$PROTON_LOG"
		: "${DXVK_LOG_LEVEL:=info}" "${VKD3D_DEBUG:=warn}"
		: "${VKD3D_SHADER_DEBUG:=fixme}"
		logfile=${PROTON_LOG_DIR:-$HOME}/proton-$(basename "$1" .exe).log
		echo "${0##*/}: logging to $logfile" >&2
		exec 2>>"$logfile"
	fi
	: "${WINEDEBUG:=-all}" "${DXVK_LOG_LEVEL:=none}" "${VKD3D_DEBUG:=none}"
	: "${VKD3D_SHADER_DEBUG:=none}"
	export WINEDEBUG DXVK_LOG_LEVEL VKD3D_DEBUG VKD3D_SHADER_DEBUG
}

# Machine field of a PE file's COFF header ("014c" i386, "8664" x86_64),
# empty if it is not a readable PE file.
pe_machine() {
	# OpenBSD's od zero-pads decimal output; strip it so it is not octal
	off=$(od -An -tu4 -j60 -N4 "$1" 2>/dev/null | tr -d ' ' | sed 's/^0*//')
	[ -n "$off" ] || return 0
	[ "$(od -An -c -j"$off" -N2 "$1" 2>/dev/null | tr -d ' ')" = PE ] || return 0
	od -An -tx2 -j$((off + 4)) -N2 "$1" 2>/dev/null | tr -d ' '
}

cmd=$1; shift
case $cmd in
run)
	[ $# -ge 1 ] || usage
	# Many "x64" installers are 32-bit programs; wine's own message for
	# them (failing to load syswow64\ntdll.dll) does not say why.
	if [ -f "$1" ] && [ "$(pe_machine "$1")" = 014c ]; then
		die "$1 is a 32-bit Windows program; OpenBSD cannot run 32-bit Windows code (no 32-bit user code segment). Look for a 64-bit build, an .msi package or a portable .zip."
	fi
	setup_prefix
	set_overrides
	setup_log "$1"
	use_steam && exec "$WINE" "$STEAMDIR_WIN\\steam.exe" "$@"
	exec "$WINE" "$@"
	;;
prefix)
	setup_prefix
	;;
-h|--help|help)
	usage
	;;
*)
	setup_prefix
	set_overrides
	setup_log "$cmd"
	exec "$WINE" "$cmd" "$@"
	;;
esac
