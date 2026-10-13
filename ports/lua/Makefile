# Lua interpreter, built in build/ from the src/ submodule.

include ../port.mk

SRC   := src
BUILD := build
BIN   := $(BUILD)/lua

# Every l*.c in the tree is part of the core, the libraries, or lua.c
# (the interpreter's main). ltests.c is the internal test harness.
SRCS := $(filter-out $(SRC)/ltests.c,$(wildcard $(SRC)/l*.c))
OBJS := $(patsubst $(SRC)/%.c,$(BUILD)/%.o,$(SRCS))

CFLAGS := -O2 -Wall $(TARGET_CFLAGS)

all: $(BIN)

$(BIN): $(OBJS) $(CRT0) $(LIBC)
	$(CC) $(TARGET_LDFLAGS) -T $(LDSCRIPT) -o $@ $(CRT0) $(OBJS) -lm -lc

$(BUILD)/%.o: $(SRC)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $@

install: all
	install -Dm755 $(BIN) $(DESTDIR)/bin/lua

clean:
	rm -rf $(BUILD)

.PHONY: all install clean
