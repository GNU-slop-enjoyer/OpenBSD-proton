# DXVK: Direct3D 8/9/10/11 on Vulkan, as 64-bit Windows DLLs for use with
# wine (emulators/proton-wine, emulators/wine).  Pinned to the commit used
# by Proton proton-11.0-2e (ValveSoftware/Proton bc896217ff).

ONLY_FOR_ARCHS =	amd64
DPB_PROPERTIES =	parallel

COMMENT =		Vulkan-based Direct3D 8/9/10/11 for wine (64-bit PE DLLs)

V =			2.7.1
GH_ACCOUNT =		doitsujin
GH_PROJECT =		dxvk
GH_COMMIT =		a6764047e587178283fcde4073ae6e1410af594f
DISTNAME =		dxvk-${V}.20260720

CATEGORIES =		graphics emulators

HOMEPAGE =		https://github.com/doitsujin/dxvk

# zlib (dxvk, dxbc-spirv); MIT (libdisplay-info, Vulkan-Headers,
# SPIRV-Headers); LGPL 2.1+ (mingw-directx-headers, from mingw-w64 and wine)
PERMIT_PACKAGE =	Yes

# Submodules of the pinned dxvk commit (git submodule status).
DIRECTX_COMMIT =	9df86f2341616ef1888ae59919feaa6d4fad693d
SPIRV_COMMIT =		04f10f650d514df88b76d25e83db360142c7b174
VULKAN_COMMIT =		5d94bb4dcc968cccce1f601324fcaf3eda92a52b
DXBC_SPIRV_COMMIT =	fe5a4ef1e99357c8b9f57e4cfb1195c3dae9707f
DISPLAYINFO_COMMIT =	275e6459c7ab1ddd4b125f28d0440716e4888078
# submodule of dxbc-spirv
DXBC_SPIRV_HDR_COMMIT =	c8ad050fcb29e42a2f57d9f59e97488f465c436d

SITES.dx =		https://github.com/Joshua-Ashton/mingw-directx-headers/archive/
SITES.spv =		https://github.com/KhronosGroup/SPIRV-Headers/archive/
SITES.vk =		https://github.com/KhronosGroup/Vulkan-Headers/archive/
SITES.dxbc =		https://github.com/doitsujin/dxbc-spirv/archive/
SITES.di =		https://github.com/doitsujin/libdisplay-info/archive/
DISTFILES.dx =		mingw-directx-headers-{}${DIRECTX_COMMIT}.tar.gz
DISTFILES.spv =		SPIRV-Headers-{}${SPIRV_COMMIT}.tar.gz \
			SPIRV-Headers-{}${DXBC_SPIRV_HDR_COMMIT}.tar.gz
DISTFILES.vk =		Vulkan-Headers-{}${VULKAN_COMMIT}.tar.gz
DISTFILES.dxbc =	dxbc-spirv-{}${DXBC_SPIRV_COMMIT}.tar.gz
DISTFILES.di =		libdisplay-info-{}${DISPLAYINFO_COMMIT}.tar.gz

BUILD_DEPENDS =		devel/meson \
			devel/mingw-w64 \
			devel/ninja \
			graphics/glslang

SEPARATE_BUILD =	Yes
NO_TEST =		Yes

DLLDIR =		${PREFIX}/lib/dxvk/x64

SUBMODULES =		include/native/directx:mingw-directx-headers-${DIRECTX_COMMIT} \
			include/spirv:SPIRV-Headers-${SPIRV_COMMIT} \
			include/vulkan:Vulkan-Headers-${VULKAN_COMMIT} \
			subprojects/dxbc-spirv:dxbc-spirv-${DXBC_SPIRV_COMMIT} \
			subprojects/dxbc-spirv/submodules/spirv_headers:SPIRV-Headers-${DXBC_SPIRV_HDR_COMMIT} \
			subprojects/libdisplay-info:libdisplay-info-${DISPLAYINFO_COMMIT}

# GitHub archives leave submodules out; put each at its path (in order, as
# spirv_headers lives inside dxbc-spirv).
post-extract:
.for s in ${SUBMODULES}
	if [ -d ${WRKSRC}/${s:C/:.*//} ]; then \
		rmdir ${WRKSRC}/${s:C/:.*//}; fi
	mv ${WRKDIR}/${s:C/.*://} ${WRKSRC}/${s:C/:.*//}
.endfor

# A cross build (meson.port.mk configures for the host), so meson is run
# directly with dxvk's own cross file; the x86_64-w64-mingw32-* tools come
# from devel/mingw-w64.
do-configure:
	cd ${WRKSRC} && ${SETENV} ${CONFIGURE_ENV} ${LOCALBASE}/bin/meson \
	    setup ${WRKBUILD} --cross-file ${WRKSRC}/build-win64.txt \
	    --buildtype release --prefix /dxvk --bindir x64 --libdir x64 \
	    -Dbuild_id=false

do-build:
	${SETENV} ${MAKE_ENV} ${LOCALBASE}/bin/ninja -C ${WRKBUILD} \
	    -j${MAKE_JOBS}

do-install:
	${INSTALL_DATA_DIR} ${DLLDIR}
.for d in d3d8/d3d8 d3d9/d3d9 d3d10/d3d10core d3d11/d3d11 dxgi/dxgi
	${INSTALL_DATA} ${WRKBUILD}/src/${d}.dll ${DLLDIR}/
.endfor
	${INSTALL_DATA_DIR} ${PREFIX}/share/doc/dxvk
	${INSTALL_DATA} ${WRKSRC}/README.md ${WRKSRC}/dxvk.conf \
	    ${PREFIX}/share/doc/dxvk/

.include <bsd.port.mk>
