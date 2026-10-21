#!/bin/sh
# Build Wine's conformance tests (winetest) for a port that was configured
# with --disable-tests.  Runs inside the VM as builder, after `make build`
# of the port.  The port's build tree is copied to build-tests in the same
# WRKDIR and reconfigured with the saved arguments and environment
# (config.status --recheck) minus --disable-tests, so unchanged objects are
# reused and the package build stays untouched.
#
# usage: winetest-build.sh WRKDIR [make targets ...]
#   e.g. winetest-build.sh /home/pobj/proton-wine-11.0.20261008 \
#            dlls/ntdll/tests/x86_64-windows/ntdll_test.exe
set -eu

WRKDIR=$1; shift
B=$WRKDIR/build-tests
export PATH=$WRKDIR/bin:$PATH
[ -f "$WRKDIR/config.site" ] && export CONFIG_SITE=$WRKDIR/config.site
# Not a precious variable, so --recheck does not preserve it (port
# CONFIGURE_ENV).
export x86_64_CC=$WRKDIR/bin/clang-pe

if [ ! -d "$B" ]; then
	cp -pR "$WRKDIR/build-amd64" "$B.tmp"
	mv "$B.tmp" "$B"
fi
cd "$B"
if grep -q -- '--disable-tests' config.status; then
	sed -i "s/ '--disable-tests'//g; s/ --disable-tests//g" config.status
	./config.status --recheck
	./config.status
fi
[ $# -gt 0 ] && gmake -j"$(sysctl -n hw.ncpuonline)" "$@"
