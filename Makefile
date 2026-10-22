# Cortex-M3 firmware for qemu-system-arm -M lm3s6965evb.
#   make             build build/blink.elf and build/blink-fault.elf
#   make host-test   Morse compiler/player tests on the host (ASan + UBSan)
#   make qemu-test   run both images in QEMU and check exit codes and output
#   make size        section sizes
CROSS   ?= arm-none-eabi-
CC      := $(CROSS)gcc
SIZE    := $(CROSS)size
QEMU    ?= qemu-system-arm
HOSTCC  ?= cc
CFLAGS  := -mcpu=cortex-m3 -mthumb -std=c11 -Os -g -Wall -Wextra -Werror -Wconversion \
           -ffreestanding -ffunction-sections -fdata-sections -Iinclude -Ifirmware
LDFLAGS := -nostdlib -nostartfiles -Wl,-T,firmware/link.ld -Wl,--gc-sections -lgcc
SRCS    := firmware/startup.c firmware/board.c firmware/main.c src/morse.c
DEPS    := $(SRCS) firmware/link.ld $(wildcard include/*.h firmware/*.h)

.PHONY: all host-test qemu-test test size clean
all: build/blink.elf build/blink-fault.elf

build/blink.elf: $(DEPS)
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRCS) $(LDFLAGS) -o $@

build/blink-fault.elf: $(DEPS)
	@mkdir -p build
	$(CC) $(CFLAGS) -DFAULT_TEST $(SRCS) $(LDFLAGS) -o $@

build/test_host: tests/test_host.c src/morse.c include/morse.h
	@mkdir -p build
	$(HOSTCC) -std=c11 -O1 -g -Wall -Wextra -Werror -Iinclude \
	  -fsanitize=address,undefined -fno-sanitize-recover=all tests/test_host.c src/morse.c -o $@

host-test: build/test_host
	./build/test_host

qemu-test: all
	QEMU=$(QEMU) ./scripts/qemu-test.sh

test: host-test qemu-test

size: build/blink.elf
	$(SIZE) -A $< | grep -E '^\.(text|data|bss)'

clean:
	rm -rf build
