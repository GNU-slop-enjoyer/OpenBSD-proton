#!/bin/sh
# Build an x86_64-w64-mingw32 C/C++ cross toolchain from the installed LLVM
# (clang, lld, llvm-ar, llvm-dlltool, llvm-windres) plus mingw-w64 headers,
# CRT and winpthreads and LLVM's compiler-rt builtins, libunwind, libc++abi
# and libc++.  Follows the llvm-mingw recipe (github.com/mstorsjo/llvm-mingw)
# without building a second LLVM.
#
# usage: mingw-w64-build.sh step ...
#   steps: wrappers headers crt builtins winpthreads runtimes widl
#          widl_install
# Environment:
#   TC_PREFIX   toolchain directory the wrappers point to
#   TC_DESTDIR  fake root to write into (default: none)
#   WIDL_PREFIX prefix widl is configured for (default: TC_PREFIX); widl
#               looks for IDL files in WIDL_PREFIX/x86_64-w64-mingw32/include
#   MINGW_SRC   extracted mingw-w64-vN source
#   LLVM_SRC    extracted llvm-project-N.src source
#   LLVM        installed LLVM (e.g. /usr/local/llvm22)
#   BUILD_DIR   scratch directory for build trees
#   MAKE_JOBS   parallel jobs
set -eu

TARGET=x86_64-w64-mingw32
: "${TC_PREFIX:?}" "${MINGW_SRC:?}" "${LLVM_SRC:?}" "${LLVM:?}" "${BUILD_DIR:?}"
TC_DESTDIR=${TC_DESTDIR:-}
MAKE_JOBS=${MAKE_JOBS:-1}
LLVM_MAJOR=$("$LLVM/bin/llvm-config" --version | cut -d. -f1)
SYSROOT=$TC_PREFIX/$TARGET
# Private clang resource directory: the target's compiler-rt builtins live
# here instead of in the llvm package's resource directory.
RESDIR=$TC_PREFIX/lib/clang/$LLVM_MAJOR
export PATH=$TC_PREFIX/bin:$PATH

mkdir -p "$BUILD_DIR"

wrappers() {
	d=$TC_DESTDIR$TC_PREFIX
	mkdir -p "$d/bin" "$d/lib/clang/$LLVM_MAJOR/lib"
	# The llvm package omits the freestanding headers that OpenBSD's libc
	# provides (@comment in devel/llvm/*/pkg/PLIST-main); a Windows
	# target needs them, so add them from the same LLVM source.
	rm -rf "$d/lib/clang/$LLVM_MAJOR/include"
	cp -R "$LLVM/lib/clang/$LLVM_MAJOR/include" \
	    "$d/lib/clang/$LLVM_MAJOR/include"
	for h in float inttypes iso646 limits stdarg stdbool stddef stdint \
	    varargs; do
		[ -f "$d/lib/clang/$LLVM_MAJOR/include/$h.h" ] ||
		    cp "$LLVM_SRC/clang/lib/Headers/$h.h" \
		    "$d/lib/clang/$LLVM_MAJOR/include/"
	done
	# The link-time options are wrapped in --start/--end-no-unused-arguments:
	# meson's get_supported_arguments() compiles with
	# -Werror=unused-command-line-argument and would otherwise reject every
	# flag it probes.
	# -fno-ret-protector: OpenBSD's clang enables retguard for every target,
	# and in leaf functions it keeps the cookie in a register that is
	# scratch in the SysV ABI but callee-saved on Windows (%rsi), without
	# restoring it, so callers lose %rsi (DXVK crashed creating a D3D11
	# device: DxvkShaderPipelineLibraryKey::hash() zeroed its caller's
	# %rsi).  Wine builds its PE code with -fno-ret-protector for the same
	# reason.
	for drv in clang clang++; do
		cat > "$d/bin/$TARGET-$drv" <<-EOF
		#!/bin/sh
		exec $LLVM/bin/$drv --target=$TARGET --sysroot=$SYSROOT \\
		    -resource-dir=$RESDIR --start-no-unused-arguments \\
		    -rtlib=compiler-rt -unwindlib=libunwind -stdlib=libc++ \\
		    -fuse-ld=lld -fno-ret-protector --end-no-unused-arguments "\$@"
		EOF
		chmod 755 "$d/bin/$TARGET-$drv"
	done
	ln -sf "$TARGET-clang" "$d/bin/$TARGET-gcc"
	ln -sf "$TARGET-clang" "$d/bin/$TARGET-cc"
	ln -sf "$TARGET-clang++" "$d/bin/$TARGET-g++"
	ln -sf "$TARGET-clang++" "$d/bin/$TARGET-c++"
	for t in ar ranlib nm objcopy objdump strip readelf strings size \
	    dlltool windres; do
		ln -sf "$LLVM/bin/llvm-$t" "$d/bin/$TARGET-$t"
	done
	ln -sf "$LLVM/bin/ld.lld" "$d/bin/$TARGET-ld"
}

autoconf_cross() {
	"$@" --host=$TARGET --prefix="$SYSROOT" \
	    CC=$TARGET-clang AR=$TARGET-ar RANLIB=$TARGET-ranlib \
	    DLLTOOL=$TARGET-dlltool
}

headers() {
	rm -rf "$BUILD_DIR/headers"; mkdir "$BUILD_DIR/headers"
	cd "$BUILD_DIR/headers"
	"$MINGW_SRC/mingw-w64-headers/configure" --host=$TARGET \
	    --prefix="$SYSROOT" --enable-idl --with-default-msvcrt=ucrt \
	    --with-default-win32-winnt=0x601
	gmake install
}

crt() {
	rm -rf "$BUILD_DIR/crt"; mkdir "$BUILD_DIR/crt"; cd "$BUILD_DIR/crt"
	autoconf_cross "$MINGW_SRC/mingw-w64-crt/configure" \
	    --disable-lib32 --enable-lib64 --with-default-msvcrt=ucrt
	gmake -j"$MAKE_JOBS"
	gmake install
}

winpthreads() {
	# POSIX threads on Win32, for code that links -lpthread / -pthread
	# (meson's dependency('threads') does).
	rm -rf "$BUILD_DIR/winpthreads"; mkdir "$BUILD_DIR/winpthreads"
	cd "$BUILD_DIR/winpthreads"
	autoconf_cross "$MINGW_SRC/mingw-w64-libraries/winpthreads/configure"
	gmake -j"$MAKE_JOBS"
	gmake install
	rm -f "$SYSROOT"/lib/*.la
}

widl() {
	# Native IDL compiler, as x86_64-w64-mingw32-widl (vkd3d-proton).
	rm -rf "$BUILD_DIR/widl"; mkdir "$BUILD_DIR/widl"; cd "$BUILD_DIR/widl"
	"$MINGW_SRC/mingw-w64-tools/widl/configure" --target=$TARGET \
	    --prefix="${WIDL_PREFIX:-$TC_PREFIX}"
	gmake -j"$MAKE_JOBS"
}

widl_install() {
	cd "$BUILD_DIR/widl"
	gmake install DESTDIR="$TC_DESTDIR"
}

cmake_cross() {
	cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
	    -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_SYSTEM_PROCESSOR=x86_64 \
	    -DCMAKE_C_COMPILER=$TARGET-clang \
	    -DCMAKE_CXX_COMPILER=$TARGET-clang++ \
	    -DCMAKE_C_COMPILER_TARGET=$TARGET \
	    -DCMAKE_CXX_COMPILER_TARGET=$TARGET \
	    -DCMAKE_ASM_COMPILER_TARGET=$TARGET \
	    -DCMAKE_AR="$LLVM/bin/llvm-ar" \
	    -DCMAKE_RANLIB="$LLVM/bin/llvm-ranlib" \
	    -DCMAKE_RC_COMPILER=$TARGET-windres \
	    -DCMAKE_C_COMPILER_WORKS=TRUE -DCMAKE_CXX_COMPILER_WORKS=TRUE \
	    -DCMAKE_FIND_ROOT_PATH="$SYSROOT" \
	    -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
	    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
	    -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY "$@"
}

builtins() {
	rm -rf "$BUILD_DIR/builtins"; mkdir "$BUILD_DIR/builtins"
	cd "$BUILD_DIR/builtins"
	cmake_cross -S "$LLVM_SRC/compiler-rt/lib/builtins" \
	    -DCOMPILER_RT_DEFAULT_TARGET_ONLY=TRUE \
	    -DCOMPILER_RT_USE_BUILTINS_LIBRARY=TRUE \
	    -DCOMPILER_RT_EXCLUDE_ATOMIC_BUILTIN=FALSE \
	    -DLLVM_CMAKE_DIR="$LLVM_SRC/llvm/cmake/modules" \
	    -DCMAKE_INSTALL_PREFIX="$RESDIR"
	ninja -j"$MAKE_JOBS"
	ninja install
}

runtimes() {
	rm -rf "$BUILD_DIR/runtimes"; mkdir "$BUILD_DIR/runtimes"
	cd "$BUILD_DIR/runtimes"
	cmake_cross -S "$LLVM_SRC/runtimes" \
	    -DCMAKE_INSTALL_PREFIX="$SYSROOT" \
	    -DLLVM_PATH="$LLVM_SRC/llvm" \
	    -DLLVM_ENABLE_RUNTIMES="libunwind;libcxxabi;libcxx" \
	    -DLLVM_INCLUDE_TESTS=FALSE \
	    -DLIBUNWIND_USE_COMPILER_RT=TRUE \
	    -DLIBUNWIND_ENABLE_SHARED=TRUE -DLIBUNWIND_ENABLE_STATIC=TRUE \
	    -DLIBCXX_USE_COMPILER_RT=ON \
	    -DLIBCXX_ENABLE_SHARED=TRUE -DLIBCXX_ENABLE_STATIC=TRUE \
	    -DLIBCXX_ENABLE_STATIC_ABI_LIBRARY=TRUE \
	    -DLIBCXX_CXX_ABI=libcxxabi \
	    -DLIBCXX_LIBDIR_SUFFIX="" \
	    -DLIBCXX_INCLUDE_TESTS=FALSE -DLIBCXX_INCLUDE_BENCHMARKS=FALSE \
	    -DLIBCXX_ENABLE_ABI_LINKER_SCRIPT=FALSE \
	    -DLIBCXXABI_USE_COMPILER_RT=ON \
	    -DLIBCXXABI_USE_LLVM_UNWINDER=ON \
	    -DLIBCXXABI_ENABLE_SHARED=FALSE \
	    -DLIBCXXABI_LIBDIR_SUFFIX=""
	ninja -j"$MAKE_JOBS"
	ninja install
}

for s; do
	echo "===> mingw-w64-build: $s"
	$s
done
