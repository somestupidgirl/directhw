#
# All-in-one Makefile
#

MAKE=make

all: debug

debug:
	$(MAKE) -C libDirectHW

release: TARGET=release
release: debug

clean:
	$(MAKE) -C libDirectHW clean

.PHONY: all debug release clean
