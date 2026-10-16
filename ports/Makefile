# Build and install every port. Each lives in ports/<name>/ with its
# upstream source (a git submodule) in ports/<name>/src.
#
#   make                  build all ports
#   make dash             build one
#   make install          copy all into DESTDIR (default userland/sysroot)
#   make install-lua      install one
#   make clean            remove all port build directories

PORTS := dash lua tcc suckless_utils doom

all: $(PORTS)

$(PORTS):
	$(MAKE) -C $@

install: $(addprefix install-,$(PORTS))

install-%:
	$(MAKE) -C $* install

clean:
	for p in $(PORTS); do $(MAKE) -C $$p clean; done

.PHONY: all install clean $(PORTS)
