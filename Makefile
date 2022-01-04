CC = gcc
CFLAGS = -Wextra -Wall -O2 -pipe
INTERNALCFLAGS  :=       \
	-I src/include/      \
	-ffreestanding       \
	-mno-red-zone	 	\
	-fno-pic -fpie		\
	-fno-stack-protector \
	-mgeneral-regs-only	\
	-fno-exceptions

LDINTERNALFLAGS :=  \
	-Tmisc/linker.ld \
	-nostdlib   \
	-shared     \
	-pie -fno-pic -fpie \
	-z max-page-size=0x1000

CFILES = $(shell find src/ -type f -name '*.c')
ASMFILES = $(shell find src/ -type f -name '*.asm')
OFILES = $(CFILES:.c=.o) $(ASMFILES:.asm=.o)
INITRD_FILES = misc/initrd/arru.txt arru.txt misc/initrd/hello.txt hello.txt misc/initrd/uname.txt uname.txt

TARGET = build/kernel.elf
IMAGE = build/image.hdd

.PHONY: clean all setup

$(IMAGE): $(TARGET)
	@echo [PARTED] $(IMAGE)
	@dd if=/dev/zero bs=1M count=0 seek=64 of=$(IMAGE)
	@parted -s $(IMAGE) mklabel gpt
	@parted -s $(IMAGE) mkpart primary 2048s 100%
	@echo [ECHFS] $(IMAGE)
	@echfs-utils -g -p0 $(IMAGE) quick-format 512
	@echfs-utils -g -p0 $(IMAGE) import misc/limine.cfg limine.cfg
	@echfs-utils -g -p0 $(IMAGE) import $(TARGET) kernel.elf
	@echo [ARRUFS]
	@./misc/initrd/arru-util misc/initrd/initrd.img write $(INITRD_FILES)
	@echfs-utils -g -p0 $(IMAGE) import misc/initrd/initrd.img initrd.img
	@echo [LIMINE] $(IMAGE)
	@./limine/limine-install $(IMAGE)
	@echo [ECHFS] $(IMAGE)
	@echfs-utils -g -p0 $(IMAGE) import limine/limine.sys limine.sys
	@echo [DONE]

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
	@rm $(OFILES) $(TARGET) $(IMAGE)

run:
	@echo [RUN] $(IMAGE)
	@qemu-system-x86_64 -hda $(IMAGE) -m 128M

setup:
	@echo Building and installing echFS utils
	@make -C echfs
	@make install -C echfs
	@echo Building Limine
	@make -C limine

all: clean $(IMAGE) run
