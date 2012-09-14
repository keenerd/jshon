# jshon - command line JSON parsing

CFLAGS := -std=c99 -Wall -pedantic -Wextra -Werror ${CFLAGS}
LDLIBS  = -ljansson

#VERSION=$(shell date +%Y%m%d)
VERSION=$(shell git show -s --format="%ci" HEAD | cut -d ' ' -f 1 | tr -d '-')
#VERSION=$(grep "^#define JSHONVER" | cut -d ' ' -f 3)

all: jshon

jshon: jshon.o

strip: jshon
	strip --strip-all jshon

clean:
	rm -f *.o jshon

dist: clean
	sed -i "s/#define JSHONVER .*/#define JSHONVER ${VERSION}/" jshon.c
	mkdir jshon-${VERSION}
	cp jshon.c jshon.1 Makefile jshon-${VERSION}
	tar czf jshon-${VERSION}.tar.gz jshon-${VERSION}
	${RM} -r jshon-${VERSION}

.PHONY: all clean dist strip

