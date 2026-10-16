CC = gcc
CFLAGS = -Wextra -Wall -O2 -pipe -g
INTERNALCFLAGS  :=       \
	-I src/include/      \
	-ffreestanding       \
	-mno-red-zone	 	\
	-fno-pic -fno-pie \
	-fno-stack-protector \
	-fno-stack-clash-protection \
	-fcf-protection=none \
	-U_FORTIFY_SOURCE \
	-mgeneral-regs-only	\
	-fno-exceptions \
	-mcmodel=kernel

LDINTERNALFLAGS :=  \
	-Tmisc/linker.ld \
	-ffreestanding\
	-nostdlib   \
	-nodefaultlibs\
	-z max-page-size=0x1000 \
	-z noexecstack \
	-Wl,--build-id=none \
	-no-pie

CFILES = $(shell find src/ -type f -name '*.c')
ASMFILES = $(shell find src/ -type f -name '*.asm')
OFILES = $(CFILES:.c=.o) $(ASMFILES:.asm=.o)
TARGET = build/kernel.elf
IMAGE = build/image.hdd

.PHONY: clean all setup userland

# Build userland programs first
userland:
	@echo "[USERLAND]"
	@$(MAKE) -C userland/ sysroot	

INITRD   = misc/initrd/initrd.img
LIMINE   = limine/limine
STRIPCTL = stripFS/build/stripctl

$(LIMINE):
	@$(MAKE) -C limine

$(STRIPCTL):
	@$(MAKE) -C stripFS

$(INITRD): $(STRIPCTL)
	@echo "[STRIPCTL]"
	@./misc/initrd/build.sh

# The FAT32 partition starts 1 MiB into the image; mtools addresses it as
# image@@offset, so no loop device, mount or root is needed.
PART = $(IMAGE)@@1M

$(IMAGE): $(TARGET) userland $(LIMINE) $(INITRD)
	@echo [CREATE IMAGE]
	@command -v mformat >/dev/null || { echo "error: mtools not found (install the mtools package)" >&2; exit 1; }
	@rm -f $(IMAGE)
	@dd if=/dev/zero of=$(IMAGE) bs=1M count=64 status=none
	@parted -s $(IMAGE) mklabel msdos mkpart primary fat32 1MiB 100%
	@mformat -i $(PART) -F -v DISCITIX ::
	@find userland/sysroot -mindepth 1 -maxdepth 1 -exec mcopy -Q -s -m -i $(PART) {} ::/ \;
	@mdir -b -i $(PART) ::/boot >/dev/null 2>&1 || mmd -i $(PART) ::/boot
	@mcopy -Q -o -i $(PART) $(TARGET) ::/boot/kernel.elf
	@mcopy -Q -o -i $(PART) $(INITRD) ::/boot/initrd.img
	@mcopy -Q -o -i $(PART) misc/limine.conf $(wildcard limine/limine*.sys) ::/boot/
	@mcopy -Q -o -i $(PART) misc/default.psf ::/font.psf
	@./limine/limine bios-install $(IMAGE) >/dev/null
	@echo "[DONE]"

$(TARGET): $(OFILES)
	@echo [LD] $(TARGET)
	@$(CC) $(LDINTERNALFLAGS) $(OFILES) -o $@

%.o: %.c
	@echo [CC] $@
	@$(CC) $(CFLAGS) $(INTERNALCFLAGS) -c $< -o $@

%.o: %.asm
	@echo [ASM] $@
	@nasm -felf64 -o $@ $<

clean:
	@echo [CLEAN] 
	@rm -f $(OFILES) $(TARGET) $(IMAGE)
	@$(MAKE) -C userland clean

run:
	@echo [RUN] $(IMAGE)
	@qemu-system-x86_64 -drive format=raw,file=$(IMAGE) -m 128M -serial stdio

setup:
	@echo Building Limine
	@make -C limine
	@echo Building stripctl
	@make -C stripFS
	@cd userland && ./setup-userland.sh

all: clean $(IMAGE) run
