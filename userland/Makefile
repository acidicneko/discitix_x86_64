CC = gcc
CFLAGS = -Idlibc -nostdlib -nostartfiles -static -fno-pic -no-pie -ffreestanding -mno-red-zone
LDFLAGS = -T linker.ld -nostdlib -static -Wl,--build-id=none

# Output directory for binaries
BUILD = build

# C runtime (linked into every app)
CRT = dlibc/crt.c

# Find all app directories (those with a main.c)
APP_DIRS = $(dir $(wildcard */main.c))
APP_NAMES = $(patsubst %/,%,$(filter-out dlibc/,$(APP_DIRS)))
APPS = $(addprefix $(BUILD)/,$(APP_NAMES))

all: $(BUILD) $(APPS)

$(BUILD):
	@mkdir -p $(BUILD)

# Build each app from its directory + crt.c
$(BUILD)/%: %/main.c $(CRT) linker.ld
	@echo "[USERLAND] $@"
	@$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(CRT) $<

clean:
	@rm -rf $(BUILD)

.PHONY: all clean
