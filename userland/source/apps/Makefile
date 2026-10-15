SUBDIRS := $(filter-out build/,$(wildcard */))

all:
	@mkdir -p build
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir || exit 1; \
	done

clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean || exit 1; \
	done

.PHONY: all clean
