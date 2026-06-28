APP = mkdir

CC = x86_64-linux-gnu-gcc

CFLAGS += \
		-I../../usr/include \
		-nostdlib \
		-ffreestanding \
		-mno-red-zone \
		-fno-pic \
		-no-pie \
		-Wa,--noexecstack

LDFLAGS += \
		-L../../usr/lib \
		-T ../linker.ld \
		-nostdlib \
		-static \
		-no-pie \
		-Wl,--build-id=none

SRCS := $(wildcard src/*.c)
OBJS := $(SRCS:.c=.o)

RUNTIME = ../../usr/lib/crt0.o

all: $(APP)

$(APP): $(OBJS)
	$(CC) $(LDFLAGS) $(RUNTIME) $(OBJS) -lc -o ../build/$@ 
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(APP)

.PHONY: all clean
