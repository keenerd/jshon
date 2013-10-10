# jshon - command line JSON parsing

CFLAGS := -std=c99 -Wall -pedantic -Wextra -Werror ${CFLAGS}
LDLIBS  = -ljansson
INSTALL=install
DESTDIR?=/
MANDIR=$(DESTDIR)/usr/share/man/man1/
TARGET_PATH=$(DESTDIR)/usr/bin
DISTFILES=jshon
MANFILE=jshon.1

#VERSION=$(shell date +%Y%m%d)
VERSION=$(shell git show -s --format="%ci" HEAD | cut -d ' ' -f 1 | tr -d '-')
#VERSION=$(grep "^#define JSHONVER" | cut -d ' ' -f 3)

all: $(DISTFILES)

$(DISTFILES): jshon.o

strip: $(DISFILES)
	strip --strip-all $(DISTFILES)

clean:
	rm -f *.o $(DISTFILES)

install:
	$(INSTALL) -D $(DISTFILES) $(TARGET_PATH)/$(DISTFILES)
	$(INSTALL) -D $(MANFILE) $(MANDIR)/$(MANFILE)

dist: clean
	sed -i "s/#define JSHONVER .*/#define JSHONVER ${VERSION}/" jshon.c
	mkdir jshon-${VERSION}
	cp jshon.c jshon.1 Makefile LICENSE jshon-${VERSION}
	tar czf jshon-${VERSION}.tar.gz jshon-${VERSION}
	${RM} -r jshon-${VERSION}

.PHONY: all clean dist strip

