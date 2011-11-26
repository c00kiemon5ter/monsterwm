CFLAGS+= -Wall
LDADD+= -lX11
LDFLAGS=
EXEC=dminiwm

PREFIX?= /usr/local
BINDIR?= $(PREFIX)/bin

CC=gcc

all: $(EXEC)

dminiwm: dminiwm.o
	$(CC) $(LDFLAGS) -s -Os -o $@ $+ $(LDADD)

install: all
	install -Dm 755 dminiwm $(DESTDIR)$(BINDIR)/dminiwm

clean:
	rm -fv dminiwm *.o

