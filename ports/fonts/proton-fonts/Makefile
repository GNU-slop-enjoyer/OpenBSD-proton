# Metric-compatible replacements for common Windows fonts, generated the way
# Valve's Proton does (Makefile.in "Fonts" section), for proton-openbsd(1),
# which links them into wine prefixes.  Proton builds the CJK fonts (msyh,
# simsun, malgun) from Source Han Sans CID sources with afdko, which is not
# in ports; here they come from Adobe's prebuilt Source Han Sans region OTFs
# (the faces Proton's FontMenuNameDB files name) and are renamed/converted
# with fontTools (files/cjk-fonts.py).

COMMENT =		Proton's metric-compatible replacements for Windows fonts

V =			11.0.2e
DISTNAME =		proton-fonts-${V}
REVISION =		0
GH_ACCOUNT =		ValveSoftware
GH_PROJECT =		Proton
# proton-11.0-2e
GH_COMMIT =		bc896217ffcd67f892f952b010f898fe7bd0f0a1

# fonts/liberation-fonts submodule of that commit
LIBERATION_COMMIT =	4b0192046158094654e865245832c66d2104219e
# Source Han Sans region OTFs (Proton vendors 2.004 CID sources; 2.004R has
# no prebuilt region zips, 2.005R is used)
SHS_V =			2.005R
SITES.shs =		https://github.com/adobe-fonts/source-han-sans/releases/download/${SHS_V}/
DISTFILES.shs =		source-han-sans-${SHS_V}-{}09_SourceHanSansSC.zip \
			source-han-sans-${SHS_V}-{}08_SourceHanSansK.zip

SITES.lib =		https://github.com/liberationfonts/liberation-fonts/archive/
DISTFILES.lib =		liberation-fonts-{}${LIBERATION_COMMIT}.tar.gz

CATEGORIES =		fonts emulators

HOMEPAGE =		https://github.com/ValveSoftware/Proton

# SIL OFL 1.1 (Liberation, Noto, Source Han Sans); Ume fonts: free (see
# Proton fonts/ume/license.html); scripts: Apache 2.0 / MIT
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
	    FONTOBJ=${FONTOBJ} PYTHON=${MODPY_BIN} FILESDIR=${FILESDIR} \
	    SHS_SC=${WRKDIR}/OTF/SimplifiedChinese/SourceHanSansSC-Regular.otf \
	    SHS_K=${WRKDIR}/OTF/Korean/SourceHanSansK-Regular.otf \
	    /bin/sh ${FILESDIR}/build-fonts.sh

do-install:
	${INSTALL_DATA_DIR} ${FONTDIR}
	${INSTALL_DATA} ${FONTOBJ}/*.ttf ${FONTOBJ}/*.ttc ${FONTDIR}/
	${INSTALL_DATA_DIR} ${PREFIX}/share/doc/proton-fonts
	${INSTALL_DATA} ${WRKDIR}/liberation-fonts-${LIBERATION_COMMIT}/LICENSE \
	    ${PREFIX}/share/doc/proton-fonts/LICENSE.liberation
	${INSTALL_DATA} ${WRKDIR}/LICENSE.txt \
	    ${PREFIX}/share/doc/proton-fonts/LICENSE.source-han-sans
	${INSTALL_DATA} ${WRKSRC}/fonts/ume/license.html \
	    ${PREFIX}/share/doc/proton-fonts/LICENSE.ume.html

.include <bsd.port.mk>
