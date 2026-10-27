# proton-launcher.  Works with BSD make and GNU make.

PROG =		proton-launcher
SRCS =		main.c steam.c cm.c depot.c net.c pb.c kv.c crypto.c util.c
OBJS =		main.o steam.o cm.o depot.o net.o pb.o kv.o crypto.o util.o

PREFIX ?=	/usr/local
LOCALBASE ?=	/usr/local
CC ?=		cc
CFLAGS ?=	-O2 -pipe
# Kept apart from CFLAGS/LDFLAGS so that setting those keeps them.
PL_CFLAGS =	-Wall -Wextra -Wno-unused-parameter -I${LOCALBASE}/include
PL_LDFLAGS =	-L${LOCALBASE}/lib
LDLIBS =	-lssl -lcrypto -lz -llzma -lzstd -lpthread

all: ${PROG}

${PROG}: ${OBJS}
	${CC} ${LDFLAGS} ${PL_LDFLAGS} -o $@ ${OBJS} ${LDLIBS}

.c.o:
	${CC} ${CFLAGS} ${PL_CFLAGS} -c $<

${OBJS}: util.h pb.h net.h cm.h steam.h kv.h depot.h crypto.h

install: ${PROG}
	install -d ${DESTDIR}${PREFIX}/bin ${DESTDIR}${PREFIX}/man/man1
	install -m 755 ${PROG} ${DESTDIR}${PREFIX}/bin/
	install -m 644 proton-launcher.1 ${DESTDIR}${PREFIX}/man/man1/

clean:
	rm -f ${PROG} ${OBJS}

.PHONY: all install clean
