# Valve's Proton fork of Wine with the OpenBSD support from emulators/wine
# rebased onto it.  Derived from emulators/wine (ports -current 2026-10-20,
# wine-11.19); see docs/COMPATIBILITY.md in the openbsd-proton project for
# what was rebased and why.

ONLY_FOR_ARCHS =	amd64
DPB_PROPERTIES =	parallel
USE_NOBTCFI =		Yes
USE_WXNEEDED =		Yes

COMMENT =		Valve Proton's Wine fork for 64-bit Windows applications

# Proton release proton-11.0-2e (ValveSoftware/Proton bc896217ff) pins
# this wine commit as its "wine" submodule.
GH_ACCOUNT =		ValveSoftware
GH_PROJECT =		wine
GH_COMMIT =		e60892cc2398b0a83c332a2078c7b85e47bdb42b
DISTNAME =		proton-wine-11.0.2e
PKGNAME =		proton-wine-11.0.20261008
REVISION =		0

CATEGORIES =		emulators

HOMEPAGE =		https://github.com/ValveSoftware/Proton

# LGPLv2.1+
PERMIT_PACKAGE =	Yes

# Vulkan registry used by dlls/winevulkan/make_vulkan (VK_XML_VERSION).
VK_XML_V =		1.4.339
SITES.vk =		https://raw.githubusercontent.com/KhronosGroup/Vulkan-Docs/v${VK_XML_V}/xml/
DISTFILES.vk =		vk-${VK_XML_V}-{}vk.xml \
			video-${VK_XML_V}-{}video.xml

# keep this in sync with MODCLANG_VERSION
CLANG_PE_HEADERS =		22.1.8
DISTFILES.clang-pe-headers =	wine-clang-pe-headers-${CLANG_PE_HEADERS}.tar.gz
SITES.clang-pe-headers =	https://thfr.info/distfiles/

EXTRACT_ONLY =		${ALL_DISTFILES:M*.tar.gz}

WANTLIB += X11 Xext avcodec avformat avutil c c++abi gettextpo glib-2.0
WANTLIB += gobject-2.0 gphoto2 gphoto2_port gstaudio-1.0 gstbase-1.0
WANTLIB += gstreamer-1.0 gsttag-1.0 gstvideo-1.0 intl m pcsclite
WANTLIB += pthread pulse sane

WANTLIB += lib/inotify/inotify lib/libpcap/pcap

# loaded with dlopen()
WANTLIB += SDL2 cups dbus-1 gnutls netapi vulkan xkbregistry

MODULES =		lang/clang \
			lang/python
MODPY_RUNDEP =		No

BUILD_DEPENDS =		devel/bison \
			devel/nasm
RUN_DEPENDS =		multimedia/gstreamer1/libav \
			multimedia/gstreamer1/plugins-bad \
			multimedia/gstreamer1/plugins-good \
			multimedia/gstreamer1/plugins-ugly \
			x11/gtk+4,-guic

LIB_DEPENDS =		audio/pulseaudio \
			devel/gettext,-tools \
			devel/libinotify \
			devel/sdl2 \
			graphics/ffmpeg \
			graphics/libgphoto2 \
			graphics/sane-backends \
			graphics/vulkan-loader \
			multimedia/gstreamer1/core \
			multimedia/gstreamer1/plugins-base \
			net/libpcap \
			net/samba,-main \
			print/cups,-libs \
			security/gnutls \
			security/pcsc-lite \
			x11/dbus \
			x11/xkbcommon

SEPARATE_BUILD =	Yes

USE_GMAKE =		Yes

# regenerate configure from the patched configure.ac
CONFIGURE_STYLE =	autoconf

# Self-contained tree like Proton's "files" directory, so that this can be
# installed next to emulators/wine.  gnu.port.mk always passes
# --prefix=${PREFIX}, so the prefix itself is moved.
PREFIX =		${LOCALBASE}/lib/proton-wine

CONFIGURE_ARGS =	--enable-archs=x86_64 --with-mingw --disable-tests
CONFIGURE_ARGS+=	--with-cups \
			--with-dbus \
			--with-ffmpeg \
			--with-fontconfig \
			--with-freetype \
			--with-gettext \
			--with-gettextpo \
			--with-gphoto \
			--with-gnutls \
			--with-gstreamer \
			--with-inotify \
			--with-netapi \
			--with-opengl \
			--with-pcap \
			--with-pcsclite \
			--with-pthread \
			--with-pulse \
			--with-sane \
			--with-sdl \
			--with-vulkan \
			--with-xcomposite \
			--with-xcursor \
			--with-xfixes \
			--with-xinerama \
			--with-xinput \
			--with-xinput2 \
			--with-xrandr \
			--with-xrender \
			--with-xshape \
			--with-xshm \
			--with-xxf86vm \
			--with-x

CONFIGURE_ARGS+=	--without-alsa \
			--without-capi \
			--without-coreaudio \
			--without-hwloc \
			--without-opencl \
			--without-oss \
			--without-udev

# Same as emulators/wine; see the comments there.
CONFIGURE_ARGS+=	--without-gssapi \
			--without-krb5 \
			--without-usb \
			--without-v4l2 \
			--without-wayland

# Unlike wine 11.19, wine 11.0 checks pcap/pcap.h in its global header list
# and sets PCAP_LIBS=-lpcap itself, ignoring PCAP_CFLAGS/PCAP_LIBS.  The
# net/libpcap port installs outside the default search paths (base has an
# older libpcap without pcap_init()), so add its directories here.
CONFIGURE_ENV =		x86_64_CC="${WRKDIR}/bin/clang-pe" \
			CPPFLAGS="-I${LOCALBASE}/include \
			-I${LOCALBASE}/include/libpcap" \
			LDFLAGS="-L${LOCALBASE}/lib -L${LOCALBASE}/lib/libpcap \
			-Wl,-rpath,${LOCALBASE}/lib/libpcap"

CONFIGURE_ENV+=		INOTIFY_LIBS="-L${LOCALBASE}/lib/inotify \
			-Wl,-rpath,${LOCALBASE}/lib/inotify -linotify"

AUTOCONF_VERSION =	2.73
AUTOMAKE_VERSION =	1.18

PORTHOME =		${WRKDIR}

# XXX: tests need more work
NO_TEST =		Yes

CLANG_PE_RESOURCE =	${WRKDIR}/clang-pe
LLVM_PREFIX =		${LOCALBASE}/llvm${MODCLANG_VERSION}
CLANG_RESOURCE =	${LLVM_PREFIX}/lib/clang/${MODCLANG_VERSION}
SUBST_VARS+=		CLANG_PE_RESOURCE MODCLANG_VERSION

# Generated sources that Proton's build creates before configuring
# (Proton Makefile.in: rules-wine-requests and .wine-post-source).
post-patch:
	cd ${WRKSRC} && perl tools/make_requests
	cd ${WRKSRC} && perl tools/make_specfiles
	cd ${WRKSRC} && XDG_CACHE_HOME=${WRKDIR} ${MODPY_BIN} \
		dlls/winevulkan/make_vulkan \
		-x ${FULLDISTDIR}/vk-${VK_XML_V}-vk.xml \
		-X ${FULLDISTDIR}/video-${VK_XML_V}-video.xml

pre-configure:
	${INSTALL_DATA_DIR} ${CLANG_PE_RESOURCE}/include
	cp -R ${CLANG_RESOURCE}/include/. ${CLANG_PE_RESOURCE}/include/
	cp ${WRKDIR}/wine-clang-pe-headers-${CLANG_PE_HEADERS}/*.h \
		${CLANG_PE_RESOURCE}/include/
	ln -sfh ${CLANG_RESOURCE}/lib ${CLANG_PE_RESOURCE}/lib
	${SUBST_PROGRAM} ${FILESDIR}/clang-pe ${WRKDIR}/bin/clang-pe

.include <bsd.port.mk>
