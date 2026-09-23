# tantu build file. Needs a C11 compiler (gcc or clang) and make.

CC      ?= cc
CFLAGS  ?= -O2
CFLAGS  += -std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wformat=2
SRC     := $(filter-out src/embedded.c,$(wildcard src/*.c)) src/embedded.c
ASSETS  := $(shell find themes starters ui -type f 2>/dev/null)
SAN     := -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer

.PHONY: all test clean static cosmo

all: tantu

tools/embed: tools/embed.c
	$(CC) -O2 -std=c11 -Wall -Wextra tools/embed.c -o $@

src/embedded.c: tools/embed $(ASSETS)
	./tools/embed $@ themes starters ui

tantu: $(SRC) $(wildcard src/*.h)
	$(CC) $(CFLAGS) $(SRC) -o $@

# A fully static Linux binary for releases
static: $(SRC)
	$(CC) $(CFLAGS) -static $(SRC) -o tantu

# One file that runs on Linux, macOS, Windows and BSD (needs cosmocc on PATH)
cosmo: $(SRC)
	cosmocc -O2 -std=c11 -Wall -Wextra $(SRC) -o tantu.com

# Builds with AddressSanitizer and UndefinedBehaviorSanitizer, then runs tests
test: $(SRC)
	$(CC) -std=c11 -Wall -Wextra -Werror $(SAN) $(SRC) -o tests/tantu-test
	sh tests/run.sh tests/tantu-test

clean:
	rm -f tantu tantu.com tests/tantu-test tools/embed src/embedded.c
	rm -rf tests/tmp
