APP = sh

CC = x86_64-linux-gnu-gcc

CFLAGS += \
		-I../../include \
		-nostdlib \
		-ffreestanding \
		-mno-red-zone \
		-fno-pic \
		-no-pie \
		-Wa,--noexecstack

LDFLAGS += \
		-L../../lib \
		-T ../linker.ld \
		-nostdlib \
		-static \
		-no-pie \
		-Wl,--build-id=none

SRCS := $(wildcard src/*.c)
OBJS := $(SRCS:.c=.o)

RUNTIME = ../../lib/crt.o ../../lib/stubs.o

all: $(APP)

$(APP): $(OBJS)
	$(CC) $(LDFLAGS) $(RUNTIME) $(OBJS) -lc -o $@ 
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(APP)

.PHONY: all clean
