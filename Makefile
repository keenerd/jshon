# jshon - a simple AUR downloader

CC      ?= gcc
CFLAGS  +=-std=c99 -Wall -pedantic -Wextra -Werror
LDFLAGS +=-ljansson

SRC = jshon.c
OBJ = ${SRC:.c=.o}

all: jshon

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}:
jshon: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

strip: jshon
	strip --strip-all jshon

clean:
	rm -f *.o jshon

.PHONY: all clean strip


