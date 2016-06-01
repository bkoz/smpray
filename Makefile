#
# Makefile for "smpray".
#

VERSION=1.0
CC=gcc
CFLAGS=-O2

.PHONY: all

all: smpray

smpray: smpray.o
	$(CC) $(CFLAGS) smpray.o -o smpray -lglut -lm -lpthread -lGL -lGLU

smpray.o: smpray.c

clean:
	rm -f *.o smpray smpray*.tar.gz

tar: clean
	cd ..; tar czvf smpray-$(VERSION).tar.gz --exclude .svn smpray-$(VERSION)

install: all
	install smpray		/usr/local/bin
	install smpray.1	/usr/local/share/man/man1

uninstall:
	rm -f /usr/local/bin/smpray
	rm -f /usr/local/share/man/man1/smpray.1
