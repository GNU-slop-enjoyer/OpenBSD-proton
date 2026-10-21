# Launcher that combines emulators/proton-wine, graphics/dxvk and
# graphics/vkd3d-proton the way Valve's Proton does, without Steam.

ONLY_FOR_ARCHS =	amd64

COMMENT =		Proton-style launcher for wine, DXVK and vkd3d-proton

PKGNAME =		proton-openbsd-0.1
REVISION =		3
CATEGORIES =		emulators games

HOMEPAGE =		https://github.com/GNU-slop-enjoyer/OpenBSD-proton

# LGPLv2.1+
PERMIT_PACKAGE =	Yes

DISTFILES =

RUN_DEPENDS =		emulators/proton-wine>=11.0.20261008p1 \
			emulators/proton-wine-addons \
			fonts/proton-fonts \
			graphics/dxvk \
			graphics/vkd3d-proton

NO_BUILD =		Yes
NO_TEST =		Yes
PKG_ARCH =		*

do-install:
	${SUBST_PROGRAM} ${FILESDIR}/proton-openbsd.sh \
	    ${PREFIX}/bin/proton-openbsd
	${SUBST_MAN} ${FILESDIR}/proton-openbsd.1 \
	    ${PREFIX}/man/man1/proton-openbsd.1

.include <bsd.port.mk>
