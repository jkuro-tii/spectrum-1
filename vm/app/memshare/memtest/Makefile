
.PHONY: install default

# The BUILD_TARGET variable is defined in the .nix file
CFLAGS = -DMSG=\"$(BUILD_TARGET)\"

default: memtest

install: memtest
	mkdir -p $(out)
	cp $< $(out)

memtest: memtest.c
