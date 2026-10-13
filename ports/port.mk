# Shared settings for building ports against the Discitix build sysroot.
# Each port's Makefile includes this.
#
#   SYSROOT  headers + libs to compile/link against (built by userland/libc)
#   DESTDIR  root of the disk image that `make install` copies into

.DEFAULT_GOAL := all

PORTS_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
TOP       := $(abspath $(PORTS_DIR)/..)

SYSROOT ?= $(TOP)/userland/source/usr
DESTDIR ?= $(TOP)/userland/sysroot

CROSS ?= x86_64-linux-gnu-
CC    := $(CROSS)gcc
AR    := $(CROSS)ar
STRIP := $(CROSS)strip

# Binaries are stripped as they go into the image (the copies in each
# build/ dir keep their debug info).
INSTALL_PROGRAM := install -m755 -s --strip-program=$(STRIP)

TARGET_CFLAGS  := -I$(SYSROOT)/include -nostdlib -ffreestanding -mno-red-zone \
                  -fno-pic -no-pie
TARGET_LDFLAGS := -nostdlib -static -no-pie -L$(SYSROOT)/lib -Wl,--build-id=none

CRT0     := $(SYSROOT)/lib/crt0.o
LIBC     := $(SYSROOT)/lib/libc.a
LDSCRIPT := $(TOP)/userland/source/apps/linker.ld

$(LIBC) $(CRT0):
	@echo "error: $@ missing. Build libc first: make -C $(TOP)/userland/libc" >&2
	@exit 1
