# ICU 68 as Windows DLLs (icuuc68, icuin68, icudt68) for proton-wine.
# Wine's icu.dll forwards to them; .NET (Core) 5 and later loads icu.dll
# for globalization and stops at start-up without them (Stardew Valley,
# most XNA/FNA/MonoGame ports on .NET 6+).  Valve's Proton ships the same
# ICU version (Makefile.in, ICU_VER 6.8.2); here it is built from source
# with the mingw-w64 cross compiler.  A native build of the same ICU
# provides the tools the cross build needs (--with-cross-build).

ONLY_FOR_ARCHS =	amd64

COMMENT =		ICU 68 Windows DLLs for proton-wine

V =			68.2
DISTNAME =		icu4c-${V:S/./_/}-src
PKGNAME =		proton-icu-${V}
CATEGORIES =		textproc emulators

SITES =			https://github.com/unicode-org/icu/releases/download/release-${V:S/./-/}/
EXTRACT_SUFX =		.tgz

HOMEPAGE =		https://icu.unicode.org/

# ICU License (MIT-style), Unicode Data Files and Software License
PERMIT_PACKAGE =	Yes

BUILD_DEPENDS =		devel/mingw-w64>=13.0.0p1

USE_GMAKE =		Yes
COMPILER =		base-clang
PKG_ARCH =		*

WRKDIST =		${WRKDIR}/icu
ICU_SRC =		${WRKDIST}/source
NATIVE =		${WRKDIR}/build-native
CROSS =			${WRKDIR}/build-x86_64-windows

ICU_OPTS =		--disable-tests --disable-samples --disable-extras \
			--disable-icuio --disable-layoutex

# The cross build needs the native one finished (its tools and
# icucross.mk), so it is configured in do-build.
do-configure:
	mkdir -p ${NATIVE} ${CROSS}
	cd ${NATIVE} && ${SETENV} ${CONFIGURE_ENV} CC="${CC}" CXX="${CXX}" \
	    ${ICU_SRC}/configure ${ICU_OPTS}

do-build:
	cd ${NATIVE} && ${MAKE_PROGRAM} -j${MAKE_JOBS}
	cd ${CROSS} && ${SETENV} PATH="${PATH}" \
	    CC=x86_64-w64-mingw32-clang CXX=x86_64-w64-mingw32-clang++ \
	    CFLAGS="-O2" CXXFLAGS="-O2" LDFLAGS="-static" \
	    ${ICU_SRC}/configure --host=x86_64-w64-mingw32 \
	    --with-cross-build=${NATIVE} --disable-tools \
	    --with-data-packaging=library ${ICU_OPTS}
	cd ${CROSS} && ${MAKE_PROGRAM} -j${MAKE_JOBS}

do-install:
	${INSTALL_DATA_DIR} ${PREFIX}/lib/proton-icu/x86_64-windows
.for l in icuuc68 icuin68 icudt68
	${INSTALL_DATA} ${CROSS}/lib/$l.dll ${PREFIX}/lib/proton-icu/x86_64-windows/
.endfor

NO_TEST =		Yes

.include <bsd.port.mk>
