# tantu build file. Needs a C11 compiler (gcc or clang) and make.

CC      ?= cc
CFLAGS  ?= -O2
CFLAGS  += -std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wformat=2
SRC     := $(wildcard src/*.c)
SAN     := -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer

.PHONY: all test clean static

all: tantu

tantu: $(SRC) $(wildcard src/*.h)
	$(CC) $(CFLAGS) $(SRC) -o $@

# A fully static Linux binary for releases
static: $(SRC)
	$(CC) $(CFLAGS) -static $(SRC) -o tantu

# Builds with AddressSanitizer and UndefinedBehaviorSanitizer, then runs tests
test: $(SRC)
	$(CC) -std=c11 -Wall -Wextra -Werror $(SAN) $(SRC) -o tests/tantu-test
	sh tests/run.sh tests/tantu-test

clean:
	rm -f tantu tests/tantu-test
	rm -rf tests/tmp
