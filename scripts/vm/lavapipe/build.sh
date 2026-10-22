#!/bin/sh
# Build Mesa's lavapipe (software Vulkan, llvmpipe) for OpenBSD into a
# private prefix, for running DXVK / vkd3d-proton tests in a VM without a
# GPU.  OpenBSD's xenocara Mesa ships only anv, hasvk and radv.  Nothing is
# installed system-wide; select the driver per process with
#   VK_ICD_FILENAMES=$PREFIX/share/vulkan/icd.d/lvp_icd.x86_64.json
#
# usage (in the VM, as the build user): sh build.sh [WORKDIR]
# Needs: meson, ninja, llvm (devel/llvm/22), py3-mako, py3-yaml, bison, xz.
# Mesa 25.0.7 is the version of OpenBSD 8.0's xenocara Mesa; the SHA256 is
# from https://docs.mesa3d.org/relnotes/25.0.7.html.
set -eu

V=25.0.7
SHA256=592272df3cf01e85e7db300c449df5061092574d099da275d19e97ef0510f8a6
W=${1:-$HOME/lvp}
PREFIX=$W/inst
HERE=$(cd "$(dirname "$0")" && pwd)
LLVM_LIB=$(dirname "$(ls /usr/local/llvm*/lib/libLLVM.so.* | sort | tail -1)")

mkdir -p "$W/dist"
cd "$W"
[ -f dist/mesa-$V.tar.xz ] ||
	ftp -o dist/mesa-$V.tar.xz https://archive.mesa3d.org/mesa-$V.tar.xz
[ "$(sha256 -q dist/mesa-$V.tar.xz)" = "$SHA256" ] || {
	echo "mesa-$V.tar.xz: checksum mismatch" >&2; exit 1; }

rm -rf mesa-$V build
xz -dc dist/mesa-$V.tar.xz | tar -xf -
# vk_device.c uses FALLTHROUGH (util/compiler.h) on systems with neither
# CLOCK_MONOTONIC_RAW nor CLOCK_MONOTONIC_FAST, e.g. OpenBSD.
(cd mesa-$V && patch -p0 < "$HERE/vk_device.c.patch")

ulimit -d "$(ulimit -H -d)"
# libLLVM lives in its own directory: record it as rpath.
(cd mesa-$V && meson setup ../build --prefix="$PREFIX" -Dbuildtype=release \
    -Dplatforms=x11 -Dgallium-drivers=llvmpipe -Dvulkan-drivers=swrast \
    -Dglx=disabled -Degl=disabled -Dgles1=disabled -Dgles2=disabled \
    -Dopengl=false -Dgbm=disabled -Dllvm=enabled -Dshared-llvm=enabled \
    -Dvalgrind=disabled -Dlibunwind=disabled -Dvideo-codecs= \
    -Dlmsensors=disabled -Dzstd=disabled \
    -Dc_link_args=-Wl,-rpath,"$LLVM_LIB" -Dcpp_link_args=-Wl,-rpath,"$LLVM_LIB")
ninja -C build install
echo "VK_ICD_FILENAMES=$PREFIX/share/vulkan/icd.d/lvp_icd.x86_64.json"
