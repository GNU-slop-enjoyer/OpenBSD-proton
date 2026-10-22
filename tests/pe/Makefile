# Freestanding PE test programs.  Needs clang, lld-link and llvm-dlltool
# (any host; no mingw toolchain or Windows SDK required).
#
#   make            build basic.exe and gui.exe
#   make d3d        build d3d11probe.exe, d3d12probe.exe, d3d11render.exe,
#                   d3d12render.exe (needs a
#                   mingw-w64 C++ toolchain, e.g. devel/mingw-w64; set
#                   MINGW_CXX)
#   make dualstack build dualstack.exe (IPv4-mapped IPv6 TCP clients)
#   make sysinfo  build sysinfo.exe (process memory/CPU information and
#                 debugger attach; needs a mingw-w64 C toolchain, MINGW_CC)
#   make audio    build audio.exe (WASAPI playback timing; mingw-w64 C)
#   make syncbench  build syncbench.exe (synchronization latency; mingw-w64 C)
#   make sleeptest  build sleeptest.exe (Sleep granularity; mingw-w64 C)
#   make gsbench    build gsbench.exe (cost of %gs TEB access; mingw-w64 C)
#   make clean

CLANG ?=	clang
DLLTOOL ?=	llvm-dlltool
LLD_LINK ?=	lld-link
OUT ?=		../../build/tests/pe
MINGW_CXX ?=	x86_64-w64-mingw32-clang++
MINGW_CC ?=	x86_64-w64-mingw32-clang

CFLAGS =	--target=x86_64-pc-windows-msvc -O2 -ffreestanding \
		-fno-stack-protector -fno-builtin -mno-stack-arg-probe \
		-Wall -Wextra -Werror

all: ${OUT}/basic.exe ${OUT}/gui.exe

${OUT}/kernel32.lib: kernel32.def
	mkdir -p ${OUT}
	${DLLTOOL} -m i386:x86-64 -d kernel32.def -l $@

${OUT}/user32.lib: user32.def
	mkdir -p ${OUT}
	${DLLTOOL} -m i386:x86-64 -d user32.def -l $@

${OUT}/gdi32.lib: gdi32.def
	mkdir -p ${OUT}
	${DLLTOOL} -m i386:x86-64 -d gdi32.def -l $@

${OUT}/basic.obj: basic.c
	mkdir -p ${OUT}
	${CLANG} ${CFLAGS} -c basic.c -o $@

${OUT}/gui.obj: gui.c
	mkdir -p ${OUT}
	${CLANG} ${CFLAGS} -c gui.c -o $@

${OUT}/basic.exe: ${OUT}/basic.obj ${OUT}/kernel32.lib
	${LLD_LINK} /nologo /subsystem:console /entry:mainCRTStartup \
		/nodefaultlib /dynamicbase /nxcompat /out:$@ \
		${OUT}/basic.obj ${OUT}/kernel32.lib

${OUT}/gui.exe: ${OUT}/gui.obj ${OUT}/kernel32.lib ${OUT}/user32.lib ${OUT}/gdi32.lib
	${LLD_LINK} /nologo /subsystem:console /entry:mainCRTStartup \
		/nodefaultlib /dynamicbase /nxcompat /out:$@ \
		${OUT}/gui.obj ${OUT}/kernel32.lib ${OUT}/user32.lib \
		${OUT}/gdi32.lib

d3d: ${OUT}/d3d11probe.exe ${OUT}/d3d12probe.exe ${OUT}/d3d11render.exe ${OUT}/d3d12render.exe

${OUT}/d3d11probe.exe: d3d11probe.cpp
	mkdir -p ${OUT}
	${MINGW_CXX} -std=c++17 -O2 -Wall -Wextra -Werror -static \
		-o $@ d3d11probe.cpp -ld3d11 -ldxgi

${OUT}/d3d12probe.exe: d3d12probe.cpp
	mkdir -p ${OUT}
	${MINGW_CXX} -std=c++17 -O2 -Wall -Wextra -Werror -static \
		-o $@ d3d12probe.cpp -ld3d12

${OUT}/d3d11render.exe: d3d11render.cpp
	mkdir -p ${OUT}
	${MINGW_CXX} -std=c++17 -O2 -Wall -Wextra -Werror -static \
		-o $@ d3d11render.cpp -ld3d11

${OUT}/d3d12render.exe: d3d12render.cpp
	mkdir -p ${OUT}
	${MINGW_CXX} -std=c++17 -O2 -Wall -Wextra -Werror -static \
		-o $@ d3d12render.cpp -ld3d12

dualstack: ${OUT}/dualstack.exe

${OUT}/dualstack.exe: dualstack.c
	mkdir -p ${OUT}
	${MINGW_CC} -O2 -Wall -Wextra -Werror -o $@ dualstack.c -lws2_32

sysinfo: ${OUT}/sysinfo.exe

${OUT}/sysinfo.exe: sysinfo.c
	mkdir -p ${OUT}
	${MINGW_CC} -O2 -Wall -Wextra -Werror -o $@ sysinfo.c -lpsapi

audio: ${OUT}/audio.exe

${OUT}/audio.exe: audio.c
	mkdir -p ${OUT}
	${MINGW_CC} -O2 -Wall -Wextra -Werror -o $@ audio.c -lole32 -lm

syncbench: ${OUT}/syncbench.exe

${OUT}/syncbench.exe: syncbench.c
	mkdir -p ${OUT}
	${MINGW_CC} -O2 -Wall -Wextra -Werror -o $@ syncbench.c -lsynchronization

sleeptest: ${OUT}/sleeptest.exe

${OUT}/sleeptest.exe: sleeptest.c
	mkdir -p ${OUT}
	${MINGW_CC} -O2 -Wall -Wextra -Werror -o $@ sleeptest.c -lwinmm

gsbench: ${OUT}/gsbench.exe

${OUT}/gsbench.exe: gsbench.c
	mkdir -p ${OUT}
	${MINGW_CC} -O2 -Wall -Wextra -Werror -o $@ gsbench.c

clean:
	rm -f ${OUT}/*.exe ${OUT}/*.obj ${OUT}/*.lib

.PHONY: all audio clean d3d gsbench sleeptest syncbench sysinfo
