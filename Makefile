.POSIX:
.PHONY:  all install uninstall clean

CC     ?= cc
CFLAGS := -W -O $(shell pkg-config --cflags libgit2)
CFLAGS += -g3 -O0 -fsanitize=address,undefined -fsanitize-trap
CFLAGS += -Wall -Wextra -Wconversion -Wdouble-promotion \
          -Wno-unused-parameter -Wno-unused-function -Wno-sign-conversion
LDLIBS := -lm $(shell pkg-config --libs libgit2)
PREFIX ?= /usr/local

all: build/simplewiki

install: build/simplewiki build/creole
	mkdir -p $(PREFIX)/bin
	mkdir -p $(PREFIX)/share/man/man1
	cp -f build/simplewiki $(PREFIX)/bin
	cp -f build/creole $(PREFIX)/bin
	gzip <doc/simplewiki.1 >$(PREFIX)/share/man/man1/simplewiki.1.gz

uninstall:
	rm -f $(PREFIX)/bin/simplewiki
	rm -f $(PREFIX)/bin/creole
	rm -f $(PREFIX)/share/man/man1/simplewiki.1.gz
	rmdir $(PREFIX)/bin >/dev/null 2>&1 || true
	rmdir $(PREFIX)/share/man/man1 >/dev/null 2>&1 || true

build/simplewiki: build/simplewiki_main.o build/die.o build/arena.o build/strutil.o build/creole.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

build/creole_test: build/creole_test_main.o build/creole.o
	$(CC) $(CFLAGS) -o $@ $^

build/creole: build/creole_util_main.o build/creole.o
	$(CC) $(CFLAGS) -o $@ $^

build/creole_test_main.o: src/creole_test_main.c
build/simplewiki_main.o: src/simplewiki_main.c src/arena.h src/die.h src/strutil.h src/creole.h
build/arena.o: src/arena.c src/arena.h
build/die.o: src/die.c src/die.h
build/strutil.o: src/strutil.c src/strutil.h src/arena.h
build/creole.o: src/creole.c
build/creole_util_main.o: src/creole_util_main.c src/creole.h

build/%.o: src/%.c | build/
	$(CC) $(CFLAGS) -c -o $@ $<

build/:
	mkdir -p build/

clean:
	rm -rf build/

