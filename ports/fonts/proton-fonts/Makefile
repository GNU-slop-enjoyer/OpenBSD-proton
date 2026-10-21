# Metric-compatible replacements for common Windows fonts, generated the way
# Valve's Proton does (Makefile.in "Fonts" section), for proton-openbsd(1),
# which links them into wine prefixes.  The CJK fonts Proton also builds
# (msyh, simsun, malgun from Source Han Sans; msgothic from Ume) need afdko,
# which is not in ports, and are not included.

COMMENT =		Proton's metric-compatible replacements for Windows fonts

V =			11.0.2e
DISTNAME =		proton-fonts-${V}
GH_ACCOUNT =		ValveSoftware
GH_PROJECT =		Proton
# proton-11.0-2e
GH_COMMIT =		bc896217ffcd67f892f952b010f898fe7bd0f0a1

# fonts/liberation-fonts submodule of that commit
LIBERATION_COMMIT =	4b0192046158094654e865245832c66d2104219e
SITES.lib =		https://github.com/liberationfonts/liberation-fonts/archive/
DISTFILES.lib =		liberation-fonts-{}${LIBERATION_COMMIT}.tar.gz

CATEGORIES =		fonts emulators

HOMEPAGE =		https://github.com/ValveSoftware/Proton

# SIL OFL 1.1 (Liberation, Noto); scripts: Apache 2.0 / MIT (merge.py)
PERMIT_PACKAGE =	Yes

MODULES =		lang/python
MODPY_RUNDEP =		No

BUILD_DEPENDS =		print/fontforge \
			print/py-fonttools

NO_TEST =		Yes
PKG_ARCH =		*

FONTOBJ =		${WRKDIR}/obj-fonts
FONTDIR =		${PREFIX}/share/proton-fonts

do-build:
	${SETENV} ${MAKE_ENV} FONTSRC=${WRKSRC}/fonts \
	    LIBSRC=${WRKDIR}/liberation-fonts-${LIBERATION_COMMIT}/src \
	    FONTOBJ=${FONTOBJ} PYTHON=${MODPY_BIN} \
	    /bin/sh ${FILESDIR}/build-fonts.sh

do-install:
	${INSTALL_DATA_DIR} ${FONTDIR}
	${INSTALL_DATA} ${FONTOBJ}/*.ttf ${FONTDIR}/
	${INSTALL_DATA_DIR} ${PREFIX}/share/doc/proton-fonts
	${INSTALL_DATA} ${WRKDIR}/liberation-fonts-${LIBERATION_COMMIT}/LICENSE \
	    ${PREFIX}/share/doc/proton-fonts/LICENSE.liberation

.include <bsd.port.mk>
