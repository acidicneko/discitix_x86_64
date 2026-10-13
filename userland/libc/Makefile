# Discitix libc: newlib plus the Discitix syscall stubs and crt objects.
#
# Installs into the build sysroot that apps and ports compile against
# (SYSROOT, default userland/source/usr):
#   include/                    newlib headers, then include/ from here on top
#   lib/libc.a                  newlib's libc.a plus our objects: stubs.o
#                               (syscall wrappers), compat.o (POSIX bits
#                               newlib lacks here) and posix/ (regex and
#                               fnmatch, vendored from newlib's sources)
#   lib/libm.a libg.a ...       the rest of newlib, unchanged
#   lib/crt0.o                  startup code for gcc-linked programs
#   lib/crt1.o crti.o crtn.o    what tcc links (crt1.o is crt0.o)
#
# newlib itself is built by build-newlib.sh into NEWLIB_PREFIX the first
# time it's needed.

CROSS ?= x86_64-linux-gnu-
CC    := $(CROSS)gcc
AR    := $(CROSS)ar

HERE          := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
SYSROOT       ?= $(abspath $(HERE)/../source/usr)
NEWLIB_PREFIX ?= $(abspath $(HERE)/../newlib)
NEWLIB        := $(NEWLIB_PREFIX)/x86_64-elf
BUILD         := $(HERE)/build

CFLAGS := -I$(SYSROOT)/include -nostdlib -ffreestanding -mno-red-zone \
          -fno-pic -no-pie -Wa,--noexecstack

NEWLIB_LIBS := libm.a libg.a libnosys.a nosys.specs
OVERLAY     := $(shell find $(HERE)/include -type f)
CRT         := crt0.o crt1.o crti.o crtn.o

LIBC_OBJS   := $(BUILD)/stubs.o $(BUILD)/compat.o \
               $(patsubst $(HERE)/posix/%.c,$(BUILD)/posix/%.o,\
                 $(filter-out $(HERE)/posix/engine.c,$(wildcard $(HERE)/posix/*.c)))
# (engine.c is #included by regexec.c, not compiled on its own)
# newlib members replaced by our own versions. They must be removed rather
# than shadowed: the linker takes the first archive member defining a
# symbol, and newlib's members come first.
NEWLIB_OVERRIDDEN := libc_a-system.o

# newlib's vendored sources: K&R definitions, GNU-only fnmatch flags
POSIX_CFLAGS := -O2 -D_GNU_SOURCE -I$(HERE)/posix -Wno-old-style-definition

all: $(SYSROOT)/lib/libc.a $(addprefix $(SYSROOT)/lib/,$(CRT) $(NEWLIB_LIBS))

newlib: $(NEWLIB)/lib/libc.a

$(NEWLIB)/lib/libc.a:
	$(HERE)/build-newlib.sh $(NEWLIB_PREFIX)

# Headers: newlib's, then our overlay replacing/adding files on top.
$(BUILD)/.headers: $(NEWLIB)/lib/libc.a $(OVERLAY)
	@mkdir -p $(SYSROOT)/include $(BUILD)
	cp -r $(NEWLIB)/include/. $(SYSROOT)/include/
	cp -r $(HERE)/include/. $(SYSROOT)/include/
	@touch $@

headers: $(BUILD)/.headers

$(BUILD)/%.o: $(HERE)/%.c $(HERE)/internal.h $(BUILD)/.headers
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/posix/%.o: $(HERE)/posix/%.c $(wildcard $(HERE)/posix/*.h) $(BUILD)/.headers
	@mkdir -p $(BUILD)/posix
	$(CC) $(CFLAGS) $(POSIX_CFLAGS) -c $< -o $@

$(BUILD)/crt0.o: $(HERE)/crt.c $(BUILD)/.headers
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/crt1.o: $(BUILD)/crt0.o
	cp $< $@

# Start from newlib's pristine libc.a every time, so stale or removed stubs
# never linger in the archive.
$(SYSROOT)/lib/libc.a: $(NEWLIB)/lib/libc.a $(LIBC_OBJS)
	@mkdir -p $(SYSROOT)/lib
	cp $(NEWLIB)/lib/libc.a $@.tmp
	$(AR) d $@.tmp $(NEWLIB_OVERRIDDEN)
	$(AR) rcs $@.tmp $(LIBC_OBJS)
	mv $@.tmp $@

$(SYSROOT)/lib/crt%.o: $(BUILD)/crt%.o
	@mkdir -p $(SYSROOT)/lib
	cp $< $@

$(addprefix $(SYSROOT)/lib/,$(NEWLIB_LIBS)): $(SYSROOT)/lib/%: $(NEWLIB)/lib/libc.a
	@mkdir -p $(SYSROOT)/lib
	cp $(NEWLIB)/lib/$* $@

clean:
	rm -rf $(BUILD)

.PHONY: all newlib headers clean
.SECONDARY:
