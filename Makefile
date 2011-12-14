VERSION = cookies-git
WMNAME  = monsterwm

PREFIX ?= /usr/local
BINDIR ?= ${PREFIX}/bin
MANPREFIX = ${PREFIX}/share/man

X11INC = /usr/include/X11
X11LIB = /usr/lib/X11

INCS = -I. -I/usr/include -I${X11INC}
LIBS = -L/usr/lib -lc -L${X11LIB} -lX11

CPPFLAGS = -DVERSION=\"${VERSION}\" -DWMNAME=\"${WMNAME}\"
CFLAGS   = -g -std=c99 -pedantic -Wall -Wextra -Os ${INCS} ${CPPFLAGS}
LDFLAGS  = -g ${LIBS}

CC 	 = cc
EXEC = ${WMNAME}

SRC = ${WMNAME}.c
OBJ = ${SRC:.c=.o}

all: options ${WMNAME}

options:
	@echo ${WMNAME} build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${OBJ}: config.h

config.h:
	@echo creating $@ from config.def.h
	@cp config.def.h $@

monsterwm: ${OBJ}
	@echo CC -o $@
	@${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	@echo cleaning
	@rm -fv ${WMNAME} ${OBJ} ${WMNAME}-${VERSION}.tar.gz

install: all
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f ${WMNAME} ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/${WMNAME}

uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/${WMNAME}

.PHONY: all options clean install uninstall
