CC = gcc
CFLAGS = -Wextra -Wall -O2 -pipe -g
INTERNALCFLAGS  :=       \
	-I src/include/      \
	-ffreestanding       \
	-mno-red-zone	 	\
	-fno-pic -fno-pie \
	-fno-stack-protector \
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
	@$(MAKE) -C userland

$(IMAGE): $(TARGET) userland
	@echo "[STRIPCTL]"
	@./misc/initrd/build.sh
	@echo [CREATE IMAGE]
	@dd if=/dev/zero of=$(IMAGE) bs=1M count=64
	@parted -s $(IMAGE) mklabel msdos
	@parted -s $(IMAGE) mkpart primary fat32 1MiB 100%
	@LOOPDEV=$$(sudo losetup --find --show --partscan $(IMAGE)) && \
	sudo mkfs.fat -F32 $${LOOPDEV}p1 && \
	mkdir -p build/mnt && \
	sudo mount $${LOOPDEV}p1 build/mnt && \
	sudo cp $(TARGET) build/mnt/kernel.elf && \
	sudo cp misc/initrd/initrd.img build/mnt/initrd.img && \
	sudo cp misc/limine.conf build/mnt/limine.conf && \
	sudo cp limine/limine*.sys build/mnt/ && \
	sudo ./limine/limine bios-install $(IMAGE) && \
	sudo umount build/mnt && \
	sudo losetup -d $${LOOPDEV}
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

all: clean $(IMAGE) run
