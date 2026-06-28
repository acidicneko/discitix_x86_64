tcc:
	@mkdir -p sysroot/bin
	@cp source/tcc sysroot/bin/tcc

apps:
	@make -C source/apps

sysroot: apps tcc
	@mkdir -p sysroot
	@cp -r source/apps/build/. sysroot/bin
	@cp -r source/usr sysroot/usr

clean:
	@rm -rf sysroot
	@$(MAKE) -C source/apps clean
