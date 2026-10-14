# Lua, built in build/ from the src/ submodule.
#
#   build/lua       interpreter, installed to /bin/lua
#   build/liblua.a  library for embedding Lua (e.g. in init), installed
#                   with its headers into the build sysroot
#                   (SYSROOT/lib, SYSROOT/include) so other programs can
#                   link with -llua -lm

include ../port.mk

SRC   := src
BUILD := build
BIN   := $(BUILD)/lua
LIB   := $(BUILD)/liblua.a

# ltests.c is the internal test harness; lua.c is the interpreter's main
LIB_SRCS := $(filter-out $(SRC)/ltests.c $(SRC)/lua.c,$(wildcard $(SRC)/l*.c))
LIB_OBJS := $(patsubst $(SRC)/%.c,$(BUILD)/%.o,$(LIB_SRCS))

HEADERS     := lua.h luaconf.h lualib.h lauxlib.h
SYS_HEADERS := $(addprefix $(SYSROOT)/include/,$(HEADERS))

CFLAGS := -O2 -Wall $(TARGET_CFLAGS) -include $(CURDIR)/discitix.h

all: $(BIN) $(SYSROOT)/lib/liblua.a $(SYS_HEADERS)

$(BIN): $(BUILD)/lua.o $(LIB) $(CRT0) $(LIBC)
	$(CC) $(TARGET_LDFLAGS) -T $(LDSCRIPT) -o $@ $(CRT0) $(BUILD)/lua.o $(LIB) -lm -lc

$(LIB): $(LIB_OBJS)
	rm -f $@
	$(AR) rcs $@ $^

$(BUILD)/%.o: $(SRC)/%.c discitix.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $@

$(SYSROOT)/lib/liblua.a: $(LIB)
	@mkdir -p $(dir $@)
	cp $< $@

$(SYSROOT)/include/%.h: $(SRC)/%.h
	@mkdir -p $(dir $@)
	cp $< $@

install: all
	@mkdir -p $(DESTDIR)/bin
	$(INSTALL_PROGRAM) $(BIN) $(DESTDIR)/bin/lua

clean:
	rm -rf $(BUILD)

.PHONY: all install clean
