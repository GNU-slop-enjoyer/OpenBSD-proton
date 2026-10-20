# Freestanding PE test programs.  Needs clang, lld-link and llvm-dlltool
# (any host; no mingw toolchain or Windows SDK required).
#
#   make            build basic.exe
#   make clean

CLANG ?=	clang
DLLTOOL ?=	llvm-dlltool
LLD_LINK ?=	lld-link
OUT ?=		../../build/tests/pe

CFLAGS =	--target=x86_64-pc-windows-msvc -O2 -ffreestanding \
		-fno-stack-protector -fno-builtin -mno-stack-arg-probe \
		-Wall -Wextra -Werror

all: ${OUT}/basic.exe

${OUT}/kernel32.lib: kernel32.def
	mkdir -p ${OUT}
	${DLLTOOL} -m i386:x86-64 -d kernel32.def -l $@

${OUT}/basic.obj: basic.c
	mkdir -p ${OUT}
	${CLANG} ${CFLAGS} -c basic.c -o $@

${OUT}/basic.exe: ${OUT}/basic.obj ${OUT}/kernel32.lib
	${LLD_LINK} /nologo /subsystem:console /entry:mainCRTStartup \
		/nodefaultlib /dynamicbase /nxcompat /out:$@ \
		${OUT}/basic.obj ${OUT}/kernel32.lib

clean:
	rm -f ${OUT}/basic.exe ${OUT}/basic.obj ${OUT}/kernel32.lib

.PHONY: all clean
