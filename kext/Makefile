-include ../Makefile.inc

KEXTNAME      ?= $(BUNDLE_NAME)
COPYRIGHT      ?= ""
SIGNCERT       ?= -

SDKROOT?=       $(shell xcrun --sdk macosx --show-sdk-path)
KERNEL_FW=      $(SDKROOT)/System/Library/Frameworks/Kernel.framework
KERNEL_HEADERS= $(KERNEL_FW)/Headers
KERNEL_LIBDIR=  $(SDKROOT)/usr/lib
IOKIT_FW=       $(SDKROOT)/System/Library/Frameworks/IOKit.framework
IOKIT_HEADERS=  $(IOKIT_FW)/Headers

KEXTBUNDLE?=         $(KEXTNAME).kext
KEXTMACHO?=          $(KEXTNAME).out

ARCHFLAGS?=          -arch arm64e
TARGET_TRIPLE?=      arm64e-apple-macos12.0

PREFIX?=             /Library/Extensions
DEVELOPER_DIR?=      /Applications/Xcode.app/Contents/Developer
MACOSX_VERSION_MIN?= 12.0

CC=       $(shell xcrun -find -sdk $(SDKROOT) cc)
CODESIGN= $(shell xcrun -find -sdk $(SDKROOT) codesign)

# DirectHW Kext Source Files mapping
CXXSRCS:= DirectHW.cpp
CSRCS:=   DirectHW_kmod.c
OBJS:=    $(CXXSRCS:.cpp=.o) $(CSRCS:.c=.o)

CFLAGS+= -DKEXTNAME_S=\"$(BUNDLE_NAME)\" \
	  -DKEXTVERSION_S=\"$(BUNDLE_VERSION)\" \
	  -DKEXTBUILD_S=\"$(BUNDLE_BUILD)\" \
	  -DBUNDLEID_S=\"$(BUNDLE_ID)\"

TIME_STAMP:= $(shell date +"%Y/%m/%dT%H:%M:%S%z")
CFLAGS+=     -D__TS__=\"$(TIME_STAMP)\"
CFLAGS+=     -DKERNEL

# Structural Kernel Core Alignments
CFLAGS+= -DCONFIG_PERSONAS \
	  -DCONFIG_AUDIT \
	  -DCONFIG_DTRACE \
	  -DAPPLEFIRESTORM

ARCH_SYS_INCLUDES := -isystem $(KERNEL_HEADERS)/arm64 -isystem $(KERNEL_HEADERS)/arm

CFLAGS+= -target $(TARGET_TRIPLE) \
	  -isysroot $(SDKROOT) \
	  $(ARCHFLAGS) \
	  -nostdinc \
	  -fno-builtin \
	  -fno-common \
	  -mkernel \
	  -fno-stack-check \
	  -fno-stack-protector \
	  -Wno-cast-function-type-mismatch \
	  -I../include \
	  -isystem $(KERNEL_HEADERS) \
	  $(ARCH_SYS_INCLUDES)

CFLAGS+= -Werror -Wall -Wextra \
	  -Wno-unused-parameter \
	  -Wno-unused-variable \
	  -Wno-unused-value \
	  -Wno-vla \
	  -Wno-gnu-folding-constant \
	  -Wno-visibility \
	  -Wno-unused-function \
	  -Wno-deprecated-declarations

# Filter configurations for Apple IOKit Kernel C++ Dialect
CXXFLAGS := $(filter-out -x c -std=gnu99,$(CFLAGS)) \
            -x c++ -std=gnu++17 -fno-rtti -fno-exceptions -fapple-kext \
            -Wno-error=cast-function-type-mismatch -Wno-cast-function-type-mismatch

LDFLAGS+= -target $(TARGET_TRIPLE) \
	   -isysroot $(SDKROOT) \
	   $(ARCHFLAGS) \
	   -nostdlib \
	   -Xlinker -kext \
	   -Xlinker -export_dynamic

# libkmod.a provides the _start/_stop entry points referenced by kmod_info.
LIBS+= -L$(KERNEL_LIBDIR) -lkmod

all: debug

%.o: %.cpp DirectHW.hpp DirectHW_arch.hpp
	$(CC) $(CXXFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(KEXTMACHO): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)
	otool -h $@

Info.plist~: Info.plist.in
	sed \
		-e 's/__OSBUILD__/$(shell /usr/bin/sw_vers -buildVersion)/g' \
		-e 's/__CLANGVER__/$(shell $(CC) -v 2>&1 | grep version)/g' \
		-e 's/__BUNDLE_NAME__/$(BUNDLE_NAME)/g' \
		-e 's/__BUNDLE_MACHO__/$(BUNDLE_MACHO)/g' \
		-e 's/__BUNDLE_SERVICE__/$(BUNDLE_SERVICE)/g' \
		-e 's/__BUNDLE_CLIENT__/$(BUNDLE_CLIENT)/g' \
		-e 's/__BUNDLE_BUILD__/$(BUNDLE_BUILD)/g' \
		-e 's/__BUNDLE_VERSION__/$(BUNDLE_VERSION)/g' \
		-e 's/__BUNDLE_ID__/$(BUNDLE_ID)/g' \
	$^ > $@

$(KEXTBUNDLE): $(KEXTMACHO) Info.plist~
	mkdir -p $@/Contents/MacOS
	cp $< $@/Contents/MacOS/$(KEXTNAME)
	cp Info.plist~ $@/Contents/Info.plist
	# All Info.plist edits must happen BEFORE codesign, otherwise they
	# invalidate the signature. OSBundleLibraries is already provided by
	# Info.plist.in, so it is not added here.
ifdef COPYRIGHT
	/usr/libexec/PlistBuddy -c 'Add :NSHumanReadableCopyright string "$(COPYRIGHT)"' $@/Contents/Info.plist
endif
ifdef MACOSX_VERSION_MIN
	/usr/libexec/PlistBuddy -c 'Add :LSMinimumSystemVersion string "$(MACOSX_VERSION_MIN)"' $@/Contents/Info.plist
endif
ifdef SIGNCERT
	$(CODESIGN) --force --timestamp=none --sign $(SIGNCERT) $@
endif
	touch $@
	dsymutil $(ARCHFLAGS) -o $(KEXTNAME).kext.dSYM $@/Contents/MacOS/$(KEXTNAME)

debug: CFLAGS += -O0 -g
debug: $(KEXTBUNDLE)

release: CFLAGS += -O2
release: $(KEXTBUNDLE)

load: $(KEXTBUNDLE)
	sudo chown -R root:wheel $<
	sudo sync
	sudo kextutil $<
	sudo chown -R '$(USER):$(shell id -gn)' $<

stat:
	kextstat | grep $(KEXTNAME)

unload:
	sudo kextunload $(KEXTBUNDLE)

install: $(KEXTBUNDLE) uninstall
	test -d "$(PREFIX)"
	sudo cp -r $< "$(PREFIX)/$<"
	sudo chmod -R 755 "$(PREFIX)/$<"
	sudo chown -R root:wheel "$(PREFIX)/$<"

uninstall:
	test -d "$(PREFIX)"
	test -e "$(PREFIX)/$(KEXTBUNDLE)" && \\
	sudo rm -rf "$(PREFIX)/$(KEXTBUNDLE)" || true

clean:
	rm -rf $(KEXTBUNDLE) $(KEXTBUNDLE).dSYM Info.plist~ $(OBJS) $(KEXTMACHO)

.PHONY: all debug release load stat unload install uninstall clean
