# Wine Mono (.NET) and Wine Gecko (mshtml) for emulators/proton-wine, the
# versions Proton proton-11.0-2e bundles (Makefile.in: WINEMONO_VER,
# GECKO_VER), unpacked into proton-wine's data directory the same way.
# These are prebuilt Windows (PE) binaries from the upstream projects.

COMMENT =		Wine Mono and Wine Gecko for Proton's wine

MONO_V =		11.2.0
GECKO_V =		2.47.4
DISTNAME =		proton-wine-addons-${MONO_V}
PKGNAME =		proton-wine-addons-${MONO_V}.${GECKO_V}
CATEGORIES =		emulators

HOMEPAGE =		https://gitlab.winehq.org/wine/wine/-/wikis/Wine-Mono

# Wine Mono: MIT/X11, LGPL and others (see its LICENSE files);
# Wine Gecko: MPL 2.0
PERMIT_PACKAGE =	Yes

SITES.mono =		https://github.com/madewokherd/wine-mono/releases/download/wine-mono-${MONO_V}/
SITES.gecko =		https://dl.winehq.org/wine/wine-gecko/${GECKO_V}/
DISTFILES.mono =	wine-mono-${MONO_V}-x86.tar.xz
DISTFILES.gecko =	wine-gecko-${GECKO_V}-x86_64.tar.xz
EXTRACT_SUFX =		.tar.xz

RUN_DEPENDS =		emulators/proton-wine

NO_BUILD =		Yes
NO_TEST =		Yes
PKG_ARCH =		*

DATADIR =		${PREFIX}/lib/proton-wine/share/wine

do-install:
	${INSTALL_DATA_DIR} ${DATADIR}/mono ${DATADIR}/gecko
	cp -R ${WRKDIR}/wine-mono-${MONO_V} ${DATADIR}/mono/
	ln -sf wine-mono-${MONO_V} ${DATADIR}/mono/wine-mono
	cp -R ${WRKDIR}/wine-gecko-${GECKO_V}-x86_64 ${DATADIR}/gecko/
	chown -R ${SHAREOWN}:${SHAREGRP} ${DATADIR}/mono ${DATADIR}/gecko
	find ${DATADIR}/mono ${DATADIR}/gecko -type d -exec chmod ${DIRMODE} {} +
	find ${DATADIR}/mono ${DATADIR}/gecko -type f -exec chmod ${SHAREMODE} {} +

.include <bsd.port.mk>
