# steamclient64.dll and steam.exe for proton-openbsd: an open
# implementation of the Steam client interfaces that games' Steamworks
# API (steam_api64.dll) talks to.  Like games/proton-launcher, the source
# lives in this project's repository (steamclient/); SC_SRC points at it.

ONLY_FOR_ARCHS =	amd64

COMMENT =		open Steam client library for games under proton-openbsd

PKGNAME =		proton-steamclient-0.1
CATEGORIES =		emulators games

HOMEPAGE =		https://github.com/GNU-slop-enjoyer/OpenBSD-proton

# LGPLv2.1+
PERMIT_PACKAGE =	Yes

DISTFILES =
SC_SRC ?=		${.CURDIR}/../../../steamclient

BUILD_DEPENDS =		devel/mingw-w64>=13.0.0p1

# Windows (PE) binaries, built with the mingw-w64 cross compiler
MAKE_FLAGS =		MINGW_CC=x86_64-w64-mingw32-clang \
			MINGW_CFLAGS="-O2"
FAKE_FLAGS =		PREFIX=${TRUEPREFIX}

PKG_ARCH =		*
NO_TEST =		Yes

do-extract:
	mkdir -p ${WRKSRC}
	cp ${SC_SRC}/Makefile ${SC_SRC}/*.[ch] ${SC_SRC}/gen.sh \
	    ${SC_SRC}/impl.list ${WRKSRC}/

.include <bsd.port.mk>
