# /sbin/init: a C host for /etc/init/init.lua. Links the Lua library that
# ports/lua installs into the build sysroot.

include ../../ports/port.mk

BUILD := build
BIN   := $(BUILD)/init
LUA   := $(SYSROOT)/lib/liblua.a

CFLAGS := -O2 -Wall -Wextra $(TARGET_CFLAGS)

all: $(BIN)

$(BIN): $(BUILD)/init.o $(LUA) $(CRT0) $(LIBC)
	$(CC) $(TARGET_LDFLAGS) -T $(LDSCRIPT) -o $@ $(CRT0) $(BUILD)/init.o -llua -lm -lc

$(BUILD)/init.o: init.c $(SYSROOT)/include/lua.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $@

$(LUA) $(SYSROOT)/include/lua.h:
	@echo "error: $@ missing. Build the Lua port first: make -C $(TOP)/ports lua" >&2
	@exit 1

install: all
	@mkdir -p $(DESTDIR)/sbin
	$(INSTALL_PROGRAM) $(BIN) $(DESTDIR)/sbin/init

clean:
	rm -rf $(BUILD)

.PHONY: all install clean
