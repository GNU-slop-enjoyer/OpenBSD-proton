# x86_64-w64-mingw32 cross toolchain for building Windows PE code (DXVK,
# vkd3d-proton) on OpenBSD.  The compiler, assembler and linker are the
# installed devel/llvm; this port adds the target's headers and libraries.

ONLY_FOR_ARCHS =	amd64
DPB_PROPERTIES =	parallel

COMMENT =		mingw-w64 cross toolchain for 64-bit Windows using LLVM

V =			13.0.0
DISTNAME =		mingw-w64-v${V}
PKGNAME =		mingw-w64-${V}
REVISION =		1
EXTRACT_SUFX =		.tar.bz2

# keep in sync with devel/llvm/${LLVM_MAJOR} (same distfile)
LLVM_MAJOR =		22
LLVM_V =		${LLVM_MAJOR}.1.8
LLVM_DISTNAME =		llvm-project-${LLVM_V}.src

CATEGORIES =		devel

HOMEPAGE =		https://www.mingw-w64.org/

# mingw-w64 headers/CRT: ZPL 2.1 and public domain; winpthreads: MIT;
# compiler-rt, libunwind, libc++abi, libc++: Apache 2.0 with LLVM exceptions
PERMIT_PACKAGE =	Yes

SITES =			${SITE_SOURCEFORGE:=mingw-w64/mingw-w64/mingw-w64-release/}
SITES.llvm =		https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVM_V}/
DISTFILES =		${DISTNAME}${EXTRACT_SUFX}
DISTFILES.llvm =	${LLVM_DISTNAME}.tar.xz

# x86_64-w64-mingw32-widl
WANTLIB =		c

BUILD_DEPENDS =		devel/cmake/core \
			devel/llvm/${LLVM_MAJOR} \
			devel/ninja
RUN_DEPENDS =		devel/llvm/${LLVM_MAJOR}

USE_GMAKE =		Yes

LLVM =			${LOCALBASE}/llvm${LLVM_MAJOR}
TC_DIR =		mingw-w64
TARGET =		x86_64-w64-mingw32

# The libraries are built with a toolchain staged in ${WRKDIR}/stage and
# then copied; the wrappers are regenerated for the installed location.
BUILD_ENV +=		MINGW_SRC=${WRKDIST} \
			LLVM_SRC=${WRKDIR}/${LLVM_DISTNAME} \
			LLVM=${LLVM} \
			BUILD_DIR=${WRKDIR}/build \
			MAKE_JOBS=${MAKE_JOBS}
STEPS =			headers crt builtins winpthreads runtimes widl

do-build:
	cd ${WRKDIR} && ${SETENV} ${MAKE_ENV} ${BUILD_ENV} \
	    TC_PREFIX=${WRKDIR}/stage WIDL_PREFIX=${TRUEPREFIX}/${TC_DIR} \
	    /bin/sh ${FILESDIR}/mingw-w64-build.sh wrappers ${STEPS}

do-install:
	${INSTALL_DATA_DIR} ${PREFIX}/${TC_DIR}
	cp -R ${WRKDIR}/stage/${TARGET} ${WRKDIR}/stage/lib ${PREFIX}/${TC_DIR}/
	cd ${WRKDIR} && ${SETENV} ${MAKE_ENV} ${BUILD_ENV} \
	    TC_PREFIX=${TRUEPREFIX}/${TC_DIR} TC_DESTDIR=${DESTDIR} \
	    /bin/sh ${FILESDIR}/mingw-w64-build.sh wrappers widl_install
	${INSTALL_DATA_DIR} ${PREFIX}/bin
.for t in clang clang++ gcc g++ cc c++ ar ranlib nm objcopy objdump strip \
    readelf strings size dlltool windres ld widl
	ln -sf ../${TC_DIR}/bin/${TARGET}-${t} ${PREFIX}/bin/${TARGET}-${t}
.endfor

# Compile, link and inspect a C++ program; running it needs Wine.
do-test:
	rm -rf ${WRKDIR}/test && mkdir ${WRKDIR}/test
	cp ${FILESDIR}/hello.cpp ${WRKDIR}/test/
	cd ${WRKDIR}/test && ${WRKDIR}/stage/bin/${TARGET}-clang++ \
	    -std=c++17 -O2 -static -o hello.exe hello.cpp
	${WRKDIR}/stage/bin/${TARGET}-objdump -p ${WRKDIR}/test/hello.exe | \
	    grep 'DLL Name'

.include <bsd.port.mk>
