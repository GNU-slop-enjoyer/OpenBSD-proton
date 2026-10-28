# Steam login, game downloads and a launcher for proton-openbsd.  The
# source lives in this project's repository (launcher/), not in a
# distfile: PL_SRC points at it, which works when the port is used from
# the checkout (PORTSDIR_PATH including the repository's ports/, as
# openbsd-proton-setup does).

ONLY_FOR_ARCHS =	amd64

COMMENT =		Steam login, game downloads and launcher for proton-openbsd

PKGNAME =		proton-launcher-0.1
REVISION =		1
CATEGORIES =		games emulators

HOMEPAGE =		https://github.com/GNU-slop-enjoyer/OpenBSD-proton

# LGPLv2.1+
PERMIT_PACKAGE =	Yes

WANTLIB +=		c crypto lzma pthread ssl z zstd

DISTFILES =
PL_SRC ?=		${.CURDIR}/../../../launcher

LIB_DEPENDS =		archivers/xz \
			archivers/zstd
RUN_DEPENDS =		emulators/proton-openbsd

MAKE_FLAGS =		CC="${CC}" CFLAGS="${CFLAGS}" LOCALBASE=${LOCALBASE}
FAKE_FLAGS =		PREFIX=${TRUEPREFIX}

NO_TEST =		Yes

do-extract:
	mkdir -p ${WRKSRC}
	cp ${PL_SRC}/Makefile ${PL_SRC}/*.[ch] ${PL_SRC}/proton-launcher.1 \
	    ${WRKSRC}/

.include <bsd.port.mk>
