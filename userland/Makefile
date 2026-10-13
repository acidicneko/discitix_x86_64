# Userland: libc, the apps in source/apps, the ports in ../ports, and the
# data files, all assembled into sysroot/ (the root of the disk image).
#
#   make            same as `make sysroot`
#   make libc       newlib + stubs + crt objects into the build sysroot
#   make apps       source/apps
#   make ports      every port in ../ports
#   make clean      remove sysroot/ and app objects (ports and libc keep
#                   their build dirs; use `make -C ../ports clean` or
#                   `make -C libc clean`)

SYSROOT := $(CURDIR)/sysroot

sysroot: apps ports
	@mkdir -p $(SYSROOT)/bin $(SYSROOT)/usr $(SYSROOT)/boot $(SYSROOT)/etc
	@cp -r source/apps/build/. $(SYSROOT)/bin
	@cp -r source/usr/. $(SYSROOT)/usr
	@$(MAKE) --no-print-directory -C ../ports install DESTDIR=$(SYSROOT)
	@cp source/color.txt $(SYSROOT)/boot/color.txt
	@cp source/etc/environment $(SYSROOT)/etc/environment
	@cp source/script.sh source/build.lua source/calc.lua $(SYSROOT)/
	@cp source/apps/ls/src/main.c $(SYSROOT)/tccls.c

libc:
	@$(MAKE) -C libc

apps: libc
	@$(MAKE) -C source/apps

ports: libc
	@$(MAKE) -C ../ports

clean:
	@rm -rf $(SYSROOT)
	@$(MAKE) -C source/apps clean

.PHONY: sysroot libc apps ports clean
