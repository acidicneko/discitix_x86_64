# dash, built out-of-tree in build/ from the src/ submodule.

include ../port.mk

SRC   := $(CURDIR)/src
BUILD := $(CURDIR)/build
BIN   := $(BUILD)/src/dash

CONFIGURE_ENV := \
	CC="$(CC)" \
	CFLAGS="$(TARGET_CFLAGS) -DHAVE_WAIT3=1 -DHAVE_KILLPG=1" \
	LDFLAGS="$(TARGET_LDFLAGS)" \
	LIBS="$(CRT0) $(LIBC)"

all: $(BUILD)/Makefile
	@# dash's own Makefile doesn't know about libc; relink when it changes.
	@if [ $(LIBC) -nt $(BIN) ] || [ $(CRT0) -nt $(BIN) ]; then rm -f $(BIN); fi
	$(MAKE) -C $(BUILD)

$(SRC)/configure:
	cd $(SRC) && ./autogen.sh

$(BUILD)/Makefile: $(SRC)/configure | $(LIBC) $(CRT0)
	@if [ -f $(SRC)/config.status ]; then \
		echo "error: $(SRC) has an in-tree build; run 'make -C $(SRC) distclean' first" >&2; \
		exit 1; \
	fi
	mkdir -p $(BUILD)
	cd $(BUILD) && $(CONFIGURE_ENV) $(SRC)/configure \
		--build=x86_64-linux-gnu --host=x86_64-elf \
		--disable-nls --without-libedit

install: all
	@mkdir -p $(DESTDIR)/bin
	$(INSTALL_PROGRAM) $(BIN) $(DESTDIR)/bin/dash

clean:
	rm -rf $(BUILD)

.PHONY: all install clean
