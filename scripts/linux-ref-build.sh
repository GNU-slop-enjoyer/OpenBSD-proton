#!/bin/sh
# Build pristine Proton wine (the tree the proton-wine port patches,
# ValveSoftware/wine e60892cc) natively on a Linux development host, as a
# reference for telling OpenBSD-specific test failures from ones that Proton
# or the test environment cause.  Nothing is installed; the build stays in
# build/linux-ref/obj.
#
# usage: scripts/linux-ref-build.sh
# Needs: build/src/git-orig (pristine tree, see scripts/make-port-patches.sh),
# the Vulkan registry XML files the port uses (fetched from the VM's
# distfiles), autoconf, gcc, clang + lld for the PE side.
#
# Then run the tests with no display, e.g.
#   env -u DISPLAY -u WAYLAND_DISPLAY WINEPREFIX=$PWD/build/linux-ref/pfx \
#       sh scripts/vm/winetest-run.sh $PWD/build/linux-ref/obj/wine OUTDIR \
#       build/linux-ref/tests/ntdll_test.exe ...
# with the test executables copied from the VM (~builder/winetest/bin), so
# both systems run the same binaries.
set -eu

top=$(cd "$(dirname "$0")/.." && pwd)
R=$top/build/linux-ref
VK=1.4.339

mkdir -p "$R"
rm -rf "$R/src"
cp -a "$top/build/src/git-orig" "$R/src"
for x in vk video; do
	[ -s "$R/$x.xml" ] || "$top/scripts/vm/run.sh" ssh \
	    "cat /home/distfiles/$x-$VK-$x.xml" > "$R/$x.xml"
done

cd "$R/src"
perl tools/make_requests
perl tools/make_specfiles
python3 dlls/winevulkan/make_vulkan -x "$R/vk.xml" -X "$R/video.xml" > /dev/null
autoreconf -fi > "$R/autoreconf.log" 2>&1

mkdir -p "$R/obj"
cd "$R/obj"
../src/configure --enable-archs=x86_64 --without-oss --without-capi \
    --without-hwloc --without-opencl --without-usb --without-v4l2 \
    --without-wayland --prefix="$R/inst" > "$R/configure.log" 2>&1
# -k: dlls/winedmo's bundled bitstream filter does not compile against
# FFmpeg 8 internals (AVBSFInternal); the tests do not need winedmo.
make -k -j"$(nproc)" > "$R/build.log" 2>&1 || true
test -x "$R/obj/server/wineserver" && test -x "$R/obj/loader/wine"
