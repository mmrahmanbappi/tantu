# tantu build file. Needs a C11 compiler (gcc or clang) and make.

CC      ?= cc
CFLAGS  ?= -O2
CFLAGS  += -std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wformat=2
ifeq ($(shell uname -s),Darwin)
CFLAGS  += -D_DARWIN_C_SOURCE
SANDEF  := -D_DARWIN_C_SOURCE
endif
SRC     := $(filter-out src/embedded.c src/vendor.c,$(wildcard src/*.c)) src/embedded.c
VENDOR  := src/vendor.c
ASSETS  := $(shell find themes starters ui assets -type f 2>/dev/null)
SAN     := -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer

.PHONY: all test clean static cosmo

all: tantu

tools/embed: tools/embed.c
	$(CC) -O2 -std=c11 -Wall -Wextra tools/embed.c -o $@

src/embedded.c: tools/embed $(ASSETS)
	./tools/embed $@ themes starters ui assets

# Third-party code is compiled on its own, without warnings
vendor.o: $(VENDOR) $(wildcard src/vendor/*.h)
	$(CC) -O2 -w -c $(VENDOR) -o $@

tantu: $(SRC) $(wildcard src/*.h) vendor.o
	$(CC) $(CFLAGS) $(SRC) vendor.o -lm -o $@

# A fully static Linux binary for releases
static: $(SRC) vendor.o
	$(CC) $(CFLAGS) -static $(SRC) vendor.o -lm -o tantu

# One file that runs on Linux, macOS, Windows and BSD (needs cosmocc on PATH)
cosmo: $(SRC)
	cosmocc -O2 -w -c $(VENDOR) -o vendor-cosmo.o
	cosmocc -O2 -std=c11 -Wall -Wextra $(SRC) vendor-cosmo.o -lm -o tantu.com

# Builds with AddressSanitizer and UndefinedBehaviorSanitizer, then runs tests
test: $(SRC) vendor.o
	$(CC) -O1 -g -w -fsanitize=address -c $(VENDOR) -o tests/vendor-san.o
	$(CC) -std=c11 -Wall -Wextra -Werror $(SANDEF) $(SAN) $(SRC) tests/vendor-san.o -lm -o tests/tantu-test
	sh tests/run.sh tests/tantu-test

clean:
	rm -f tantu tantu.com tests/tantu-test tools/embed src/embedded.c vendor.o vendor-cosmo.o tests/vendor-san.o vendor-cosmo.o
	rm -rf tests/tmp
