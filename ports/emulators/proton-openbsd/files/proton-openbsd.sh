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
		WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" \
		    "$WINE" wineboot -i || die "wineboot failed for $WINEPREFIX"
		"$WINEDIR/bin/wineserver" -w
	fi
	for f in $dxvk_dlls; do install_dll "$f" "$DXVKDIR"; done
	for f in $vkd3d_dlls; do install_dll "$f" "$VKD3DDIR"; done
	for f in $builtin_dlls; do
		install_dll "$f" "$WINEDIR/lib/wine/x86_64-windows"
	done
	link_fonts
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
# does.  mscoree and mshtml are disabled because Wine Mono and Wine Gecko
# are not packaged: otherwise wine offers to download them in a dialog
# that blocks until clicked, also when it updates a prefix after a
# proton-wine upgrade.
overrides="mscoree,mshtml="
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

cmd=$1; shift
case $cmd in
run)
	[ $# -ge 1 ] || usage
	setup_prefix
	set_overrides
	setup_log "$1"
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
