
.PHONY: install default

CFLAGS = -DMSG=\"$(BUILD_TARGET)\"

install: memtest
	mkdir -p $(out)
	cp $< $(out)

default: memtest

memshare: memtest.c
