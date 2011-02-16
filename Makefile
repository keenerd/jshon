# jshon - command line JSON parsing

CC      ?=  gcc
CFLAGS  += -std=c99 -Wall -pedantic -Wextra -Werror
LDFLAGS += -ljansson

SRC = jshon.c
OBJ = ${SRC:.c=.o}

VERSION=$(shell date +%Y%m%d)

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

dist: clean
	mkdir jshon-${VERSION}
	cp jshon.c jshon.1 Makefile jshon-${VERSION}
	tar czf jshon-${VERSION}.tar.gz jshon-${VERSION}
	${RM} -r jshon-${VERSION}

.PHONY: all clean dist strip

