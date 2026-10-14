# Userland: libc, the apps in source/apps, the ports in ../ports, and the
# data files, all assembled into sysroot/ (the root of the disk image).
#
#   make            same as `make sysroot`
#   make libc       newlib + stubs + crt objects into the build sysroot
#   make apps       source/apps
#   make ports      every port in ../ports
#   make init       /sbin/init (needs liblua from the Lua port)
#   make clean      remove sysroot/ and app objects (ports and libc keep
#                   their build dirs; use `make -C ../ports clean` or
#                   `make -C libc clean`)

SYSROOT := $(CURDIR)/sysroot
STRIP   := x86_64-linux-gnu-strip

sysroot: apps ports init
	@mkdir -p $(SYSROOT)/bin $(SYSROOT)/usr $(SYSROOT)/boot $(SYSROOT)/etc
	@cp -r source/apps/build/. $(SYSROOT)/bin
	@# Strip the apps' ELF binaries (skips .gitkeep and friends)
	@for f in $(SYSROOT)/bin/*; do \
		case "$$(head -c 4 $$f)" in *ELF) $(STRIP) $$f ;; esac; \
	done
	@cp -r source/usr/. $(SYSROOT)/usr
	@$(MAKE) --no-print-directory -C ../ports install DESTDIR=$(SYSROOT)
	@cp source/color.txt $(SYSROOT)/boot/color.txt
	@cp -r source/etc/. $(SYSROOT)/etc
	@$(MAKE) --no-print-directory -C init install DESTDIR=$(SYSROOT)
	@cp source/script.sh source/build.lua source/calc.lua $(SYSROOT)/
	@cp source/apps/ls/src/main.c $(SYSROOT)/tccls.c
	@cp source/.dashrc $(SYSROOT)/.dashrc

libc:
	@$(MAKE) -C libc

apps: libc
	@$(MAKE) -C source/apps

ports: libc
	@$(MAKE) -C ../ports

init: ports
	@$(MAKE) -C init

clean:
	@rm -rf $(SYSROOT)
	@$(MAKE) -C source/apps clean

.PHONY: sysroot libc apps ports init clean
