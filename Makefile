CC = gcc
CFLAGS = -Werror -Wall -O2 -pipe
INTERNALCFLAGS  :=       \
	-I src/include/        \
	-ffreestanding       \
	-fno-stack-protector \
	-fno-pic -fpie       \
	-mno-80387           \
	-mno-mmx             \
	-mno-3dnow           \
	-mno-sse             \
	-mno-sse2            \
	-mno-red-zone

LDINTERNALFLAGS :=  \
	-Tlinker.ld \
	-nostdlib   \
	-shared     \
	-pie -fno-pic -fpie \
	-z max-page-size=0x1000

CFILES = $(shell find src/ -type f -name '*.c')
ASMFILES = $(shell find src/ -type f -name '*.asm')
OFILES = $(CFILES:.c=.o) $(ASMFILES:.asm=.o)

TARGET = kernel.elf
IMAGE = image.hdd

.PHONY: clean all

$(IMAGE): $(TARGET)
	@echo [PARTED] $(IMAGE)
	@dd if=/dev/zero bs=1M count=0 seek=64 of=$(IMAGE)
	@parted -s $(IMAGE) mklabel gpt
	@parted -s $(IMAGE) mkpart primary 2048s 100%
	@echo [ECHFS] $(IMAGE)
	@echfs-utils -g -p0 $(IMAGE) quick-format 512
	@echfs-utils -g -p0 $(IMAGE) import limine.cfg limine.cfg
	@echfs-utils -g -p0 $(IMAGE) import $(TARGET) $(TARGET)
	@echo [LIMINE] $(IMAGE)
	@./limine/limine-install $(IMAGE)
	@echo [ECHFS] $(IMAGE)
	@echfs-utils -g -p0 $(IMAGE) import limine/limine.sys limine.sys
	@echo [DONE]

$(TARGET): $(OFILES)
	@echo [LD] $(TARGET)
	@$(CC) $(LDINTERNALFLAGS) $(OFILES) -o $@

src/arch/idt.o: src/arch/idt.c
	@$(CC) -I src/include/ -ffreestanding -mgeneral-regs-only -c $< -o $@

#src/arch/isr.o: src/arch/isr.c
#	@$(CC) -I src/include/ -ffreestanding -mgeneral-regs-only -c $< -o $@

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
	@qemu-system-x86_64 -hda image.hdd -m 128M

all: clean $(IMAGE) run
