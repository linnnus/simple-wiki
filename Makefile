.POSIX:
.PHONY:  all install uninstall clean

CC     := cc
CFLAGS := -W -O $(shell pkg-config --cflags libgit2)
CFLAGS += -g3 -O0 -fsanitize=address,undefined -fsanitize-trap
CFLAGS += -Wall -Wextra -Wconversion -Wdouble-promotion \
          -Wno-unused-parameter -Wno-unused-function -Wno-sign-conversion
LDLIBS := -lm $(shell pkg-config --libs libgit2)
PREFIX ?= /usr/local

all: build/simplewiki

install: build/simplewiki
	mkdir -p $(PREFIX)/bin
	mkdir -p $(PREFIX)/share/man/man1
	cp -f build/simplewiki $(PREFIX)/bin
	gzip <doc/simplewiki.1 >$(PREFIX)/share/man/man1/simplewiki.1.gz

uninstall:
	rm -f $(PREFIX)/bin/simplewiki
	rm -f $(PREFIX)/share/man/man1/simplewiki.1.gz
	rmdir $(PREFIX)/bin >/dev/null 2>&1 || true
	rmdir $(PREFIX)/share/man/man1 >/dev/null 2>&1 || true

build/simplewiki: build/main.o build/die.o build/arena.o build/strutil.o build/creole.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o build/simplewiki $^ $(LDLIBS)

build/creole-test: build/creole-test.o build/creole.o
	$(CC) $(CFLAGS) -o $@ $^

build/creole-test.o: src/creole-test.c
build/main.o: src/main.c src/arena.h src/die.h src/strutil.h src/creole.h
build/arena.o: src/arena.c src/arena.h
build/die.o: src/die.c src/die.h
build/strutil.o: src/strutil.c src/strutil.h src/arena.h
build/creole.o: src/creole.c

build/%.o: src/%.c | build/
	$(CC) $(CFLAGS) -c -o $@ $<

build/:
	mkdir -p build/

clean:
	rm -f build/

