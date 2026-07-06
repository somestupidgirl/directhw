#
# All-in-one Makefile
#

MAKE=make

all: debug

debug:
	$(MAKE) -C libdirecthw

release: TARGET=release
release: debug

clean:
	$(MAKE) -C libdirecthw clean

.PHONY: all debug release clean
