# tcc for Discitix: a native compiler that runs on Discitix.
#
# Steps (all in build/, out-of-tree from the src/ submodule):
#   1. configure with the on-target search paths, then build x86_64-tcc,
#      a host-side compiler for Discitix
#   2. use it to build libtcc1.a (tcc's runtime support library)
#   3. use it to build tcc itself, statically linked against our libc
#
# On Discitix, tcc's lib dir {B} is /usr/local/lib/tcc (from --prefix);
# it holds tcc's own headers and libtcc1.a. System headers and libraries
# come from /usr/include and /usr/lib.

include ../port.mk

SRC   := $(CURDIR)/src
BUILD := $(CURDIR)/build
XTCC  := $(BUILD)/x86_64-tcc
BIN   := $(BUILD)/tcc
LIBTCC1 := $(BUILD)/libtcc1.a

TCCDIR := /usr/local/lib/tcc

# TARGETOS_Discitix drops tcc -run, which the Discitix fork stubs out.
CONFIGURE_FLAGS := \
	--prefix=/usr/local \
	--sysincludepaths='{B}/include:/usr/include' \
	--libpaths='{B}:/usr/lib' \
	--crtprefix='{B}:/usr/lib' \
	--extra-cflags=-DTARGETOS_Discitix

# Compile with tcc's own headers plus the build sysroot's, never the host's.
XTCC_FLAGS := -nostdinc -I$(SRC)/include -isystem $(SYSROOT)/include \
              -DTCC_TARGET_X86_64 -DTARGETOS_Discitix

all: $(BIN) $(LIBTCC1)

$(BUILD)/config.mak:
	mkdir -p $(BUILD)
	cd $(BUILD) && $(SRC)/configure $(CONFIGURE_FLAGS)

$(XTCC): $(BUILD)/config.mak $(wildcard $(SRC)/*.c $(SRC)/*.h)
	$(MAKE) -C $(BUILD) x86_64-tcc

$(BUILD)/%.o: $(SRC)/lib/%.c $(XTCC) $(LIBC)
	$(XTCC) $(XTCC_FLAGS) -c $< -o $@

$(LIBTCC1): $(BUILD)/libtcc1.o $(BUILD)/va_list.o
	rm -f $@
	$(AR) rcs $@ $^

# Linking needs libtcc1.a and the crt objects in one directory for -B, so
# libtcc1.a also goes into the build sysroot next to libc's crt*.o.
$(SYSROOT)/lib/libtcc1.a: $(LIBTCC1)
	cp $< $@

# CONFIG_TCC_SEMLOCK=0: Discitix has no threads (and no sem_*).
$(BIN): $(XTCC) $(SYSROOT)/lib/libtcc1.a $(LIBC) $(CRT0) \
        $(wildcard $(SRC)/*.c $(SRC)/*.h)
	$(XTCC) -DONE_SOURCE=1 -DCONFIG_TCC_SEMLOCK=0 $(XTCC_FLAGS) -I$(BUILD) \
		-B$(SYSROOT)/lib -L$(SYSROOT)/lib -static \
		-o $@ $(SRC)/tcc.c -lc -lm

install: all
	@mkdir -p $(DESTDIR)/bin
	$(INSTALL_PROGRAM) $(BIN) $(DESTDIR)/bin/tcc
	install -Dm644 $(LIBTCC1) $(DESTDIR)$(TCCDIR)/libtcc1.a
	mkdir -p $(DESTDIR)$(TCCDIR)/include
	cp $(SRC)/include/*.h $(DESTDIR)$(TCCDIR)/include/

clean:
	rm -rf $(BUILD)

.PHONY: all install clean
