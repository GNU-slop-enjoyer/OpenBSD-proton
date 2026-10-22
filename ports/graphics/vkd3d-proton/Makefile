# vkd3d-proton: Direct3D 12 on Vulkan, as 64-bit Windows DLLs for use with
# wine (emulators/proton-wine, emulators/wine).  Pinned to the commit used
# by Proton proton-11.0-2e (ValveSoftware/Proton bc896217ff).

ONLY_FOR_ARCHS =	amd64
DPB_PROPERTIES =	parallel

COMMENT =		Vulkan-based Direct3D 12 for wine (64-bit PE DLLs)

V =			3.0
GH_ACCOUNT =		HansKristian-Work
GH_PROJECT =		vkd3d-proton
GH_COMMIT =		212991fc2c266bc0d59f4c4ce8f80f7126508d71
DISTNAME =		vkd3d-proton-${V}.20260729
REVISION =		0

CATEGORIES =		graphics emulators

HOMEPAGE =		https://github.com/HansKristian-Work/vkd3d-proton

# LGPLv2.1+ (vkd3d-proton); MIT (dxil-spirv, dxbc-spirv is zlib,
# Vulkan-Headers and SPIRV-Headers are MIT/Apache 2.0)
PERMIT_PACKAGE =	Yes

# Submodules of the pinned commit needed by the meson build (git submodule
# status --recursive).  dxil-spirv's third_party/SPIRV-Cross and
# SPIRV-Tools are only used by its CMake tools and are left out.
SPIRV_COMMIT =		f88a2d766840fc825af1fc065977953ba1fa4a91
VULKAN_COMMIT =		ad9ce1235e88dc09287e19171dfac384db8ec32c
DXIL_SPIRV_COMMIT =	219d770799606f15abb3c57a8a9b9d19c2ae94ca
DXBC_SPIRV_COMMIT =	29c93aeecd55533a357fdd7c95be5587d1c1f506
DXBC_SPIRV_HDR_COMMIT =	c8ad050fcb29e42a2f57d9f59e97488f465c436d

SITES.spv =		https://github.com/KhronosGroup/SPIRV-Headers/archive/
SITES.vk =		https://github.com/KhronosGroup/Vulkan-Headers/archive/
SITES.dxil =		https://github.com/HansKristian-Work/dxil-spirv/archive/
SITES.dxbc =		https://github.com/doitsujin/dxbc-spirv/archive/
DISTFILES.spv =		SPIRV-Headers-{}${SPIRV_COMMIT}.tar.gz \
			SPIRV-Headers-{}${DXBC_SPIRV_HDR_COMMIT}.tar.gz
DISTFILES.vk =		Vulkan-Headers-{}${VULKAN_COMMIT}.tar.gz
DISTFILES.dxil =	dxil-spirv-{}${DXIL_SPIRV_COMMIT}.tar.gz
DISTFILES.dxbc =	dxbc-spirv-{}${DXBC_SPIRV_COMMIT}.tar.gz

BUILD_DEPENDS =		devel/meson \
			devel/mingw-w64>=13.0.0p1 \
			devel/ninja \
			graphics/glslang

SEPARATE_BUILD =	Yes
NO_TEST =		Yes

DLLDIR =		${PREFIX}/lib/vkd3d-proton/x64

# The same SPIRV-Headers commit is used twice; copy rather than move it.
SUBMODULES =		khronos/Vulkan-Headers:Vulkan-Headers-${VULKAN_COMMIT} \
			subprojects/dxil-spirv:dxil-spirv-${DXIL_SPIRV_COMMIT} \
			subprojects/dxil-spirv/subprojects/dxbc-spirv:dxbc-spirv-${DXBC_SPIRV_COMMIT} \
			subprojects/dxil-spirv/subprojects/dxbc-spirv/submodules/spirv_headers:SPIRV-Headers-${DXBC_SPIRV_HDR_COMMIT}

# GitHub archives leave submodules out; put each at its path (in order, as
# later ones live inside earlier ones).
post-extract:
.for s in ${SUBMODULES}
	if [ -d ${WRKSRC}/${s:C/:.*//} ]; then \
		rmdir ${WRKSRC}/${s:C/:.*//}; fi
	mv ${WRKDIR}/${s:C/.*://} ${WRKSRC}/${s:C/:.*//}
.endfor
.for d in khronos/SPIRV-Headers subprojects/dxil-spirv/third_party/spirv-headers
	if [ -d ${WRKSRC}/${d} ]; then rmdir ${WRKSRC}/${d}; fi
	cp -R ${WRKDIR}/SPIRV-Headers-${SPIRV_COMMIT} ${WRKSRC}/${d}
.endfor

# Without .git, vcs_tag() uses its fallbacks; vkd3d_build is the shader
# cache key, so give it the commit as "git describe --abbrev=15" would.
post-patch:
	sed -i "s/fallback : '12345678'/fallback : '${GH_COMMIT:C/^(.{15}).*/\1/}'/" \
	    ${WRKSRC}/meson.build

# A cross build (meson.port.mk configures for the host), so meson is run
# directly with a cross file naming the devel/mingw-w64 tools, including
# its widl (an installed wine's widl would otherwise be preferred).
do-configure:
	cd ${WRKSRC} && ${SETENV} ${CONFIGURE_ENV} ${LOCALBASE}/bin/meson \
	    setup ${WRKBUILD} --cross-file ${FILESDIR}/cross-win64.txt \
	    --buildtype release --prefix /vkd3d-proton --bindir x64 \
	    --libdir x64 -Denable_tests=false -Denable_extras=false

do-build:
	${SETENV} ${MAKE_ENV} ${LOCALBASE}/bin/ninja -C ${WRKBUILD} \
	    -j${MAKE_JOBS}

do-install:
	${INSTALL_DATA_DIR} ${DLLDIR}
.for d in d3d12 d3d12core
	${INSTALL_DATA} ${WRKBUILD}/libs/${d}/${d}.dll ${DLLDIR}/
.endfor
	${INSTALL_DATA_DIR} ${PREFIX}/share/doc/vkd3d-proton
	${INSTALL_DATA} ${WRKSRC}/README.md ${PREFIX}/share/doc/vkd3d-proton/

.include <bsd.port.mk>
