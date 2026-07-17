tcc:
	@mkdir -p sysroot/bin
	@cp source/tcc sysroot/bin/tcc

apps:
	@make -C source/apps

lua:
	@mkdir -p sysroot/bin
	@cp source/build.lua sysroot/build.lua
	@cp source/calc.lua sysroot/calc.lua
	@cp source/lua sysroot/bin/lua

sysroot: apps tcc lua
	@mkdir -p sysroot
	@cp -r source/apps/build/. sysroot/bin
	@cp -r source/usr sysroot/usr
	@mkdir -p sysroot/boot
	@cp source/color.txt sysroot/boot/color.txt
	@cp source/apps/ls/src/main.c sysroot/tccls.c
	@mkdir -p sysroot/etc
	@cp source/etc/environment sysroot/etc/environment

clean:
	@rm -rf sysroot
	@$(MAKE) -C source/apps clean
