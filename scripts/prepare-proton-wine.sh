#!/bin/sh
# Turn a Proton wine git tree into a release-style source tarball: the
# generated files that Proton's build creates before configuring (server
# protocol, syscall tables/spec files, winevulkan thunks) are generated here
# so that the OpenBSD port can treat the result like an upstream wine tarball
# (autoreconf still runs in the port, CONFIGURE_STYLE=autoconf).
#
# usage: scripts/prepare-proton-wine.sh [TREE] [OUTDIR] [REV]
#   TREE    git work tree (default work/proton-port/wine)
#   OUTDIR  where the tarball goes (default build/dist)
#   REV     revision to export (default HEAD; the port uses Valve's pristine
#           submodule commit and carries OpenBSD changes as patches)
#
# Needs: git, perl, python3, curl, xz.
set -eu

TOP=$(cd "$(dirname "$0")/.." && pwd)
TREE=${1:-$TOP/work/proton-port/wine}
OUT=${2:-$TOP/build/dist}
REV=${3:-HEAD}
CACHE=$TOP/build/cache/vulkan
VK_VERSION=$(sed -n 's/^VK_XML_VERSION = "\(.*\)"$/\1/p' \
    "$TREE/dlls/winevulkan/make_vulkan")

# Pinned registry files for VK_XML_VERSION 1.4.339 (trust on first use,
# fetched 2026-10-20 from KhronosGroup/Vulkan-Docs tag v1.4.339).
VK_XML_SHA256=5ebddf02358d937d8ce9bd997b20d2be9693e48320e1a060056c26b9cfe15438
VIDEO_XML_SHA256=4480ab87c8ee6d561376fa83820af1071f85ff7fa1a26b21f521717fa04d5809

rev=$(git -C "$TREE" rev-parse --short=10 "$REV")
name=proton-wine-$rev
src=$TOP/build/src/$name

fetch_xml() {	# file sha256
	f=$CACHE/$1-$VK_VERSION.xml
	if [ ! -f "$f" ]; then
		mkdir -p "$CACHE"
		curl -fL -o "$f.tmp" "https://raw.githubusercontent.com/KhronosGroup/Vulkan-Docs/v$VK_VERSION/xml/$1.xml"
		mv "$f.tmp" "$f"
	fi
	got=$(sha256sum "$f" | cut -d' ' -f1)
	case $2 in
	@*)	echo "$1.xml sha256 (not pinned yet): $got" >&2 ;;
	"$got")	;;
	*)	echo "$1.xml: checksum mismatch ($got)" >&2; exit 1 ;;
	esac
	echo "$f"
}

vk_xml=$(fetch_xml vk "$VK_XML_SHA256")
video_xml=$(fetch_xml video "$VIDEO_XML_SHA256")

rm -rf "$src"
mkdir -p "$src" "$OUT"
git -C "$TREE" archive --format=tar "$rev" | tar -C "$src" -xf -

(
	cd "$src"
	perl tools/make_requests
	perl tools/make_specfiles
	python3 -I dlls/winevulkan/make_vulkan -x "$vk_xml" -X "$video_xml"
	echo "$rev" > PROTON_WINE_REVISION
)

tar -C "$TOP/build/src" -cf - "$name" | xz -T0 -9 > "$OUT/$name.tar.xz"
echo "$OUT/$name.tar.xz"
