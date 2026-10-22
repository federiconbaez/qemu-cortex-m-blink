# Morse compiler and player, tested on the host.
#   make host-test   tests with ASan + UBSan
HOSTCC  ?= cc

.PHONY: host-test clean
build/test_host: tests/test_host.c src/morse.c include/morse.h
	@mkdir -p build
	$(HOSTCC) -std=c11 -O1 -g -Wall -Wextra -Werror -Iinclude \
	  -fsanitize=address,undefined -fno-sanitize-recover=all tests/test_host.c src/morse.c -o $@

host-test: build/test_host
	./build/test_host

clean:
	rm -rf build
