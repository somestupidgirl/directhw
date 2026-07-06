#
# DirectHW Root Unified Automation Makefile
#
# Usage:
#   make                    # Build kext + lib into $(OUT)
#   make ARCH=arm64e        # Apple Silicon target architecture
#   make ARCH=x86_64        # Intel target architecture
#   make ARCH=universal     # Fat architecture slice binding (arm64e + x86_64)
#   sudo make load          # Loads the compiled kext locally
#   make clean              # Removes root-level and subdirectory artifacts
#

MAKE := make
OUT  := out

# Project Overrides from Makefile.inc integration
-include Makefile.inc
# Must match the kext's own default (kext/Makefile: KEXTNAME ?= $(BUNDLE_NAME)),
# otherwise the staging `mv` below cannot find the built bundle.
KEXTNAME ?= $(BUNDLE_NAME)

# Detect native arch if ARCH not specified
NATIVE_ARCH := $(shell uname -m)
ifeq ($(NATIVE_ARCH),arm64)
    DEFAULT_ARCH := arm64e
else
    DEFAULT_ARCH := x86_64
endif
ARCH ?= $(DEFAULT_ARCH)

# Enforce explicit arm64e translation layer for Apple Silicon kernel spaces
ifeq ($(ARCH),arm64)
    override ARCH := arm64e
endif

# Per-architecture cross-compilation environment matrices
ifeq ($(ARCH),arm64e)
    KEXT_ARCHFLAGS := -arch arm64e
    KEXT_TRIPLE    := arm64e-apple-macos12.0
    LIB_ARCHFLAGS  := -arch arm64e
    LIB_TRIPLE     := arm64e-apple-macos12.0
else ifeq ($(ARCH),x86_64)
    KEXT_ARCHFLAGS := -arch x86_64
    KEXT_TRIPLE    := x86_64-apple-macos10.15
    LIB_ARCHFLAGS  := -arch x86_64
    LIB_TRIPLE     := x86_64-apple-macos10.15
else ifeq ($(ARCH),universal)
    KEXT_ARCHFLAGS := -arch arm64e
    KEXT_TRIPLE    := arm64e-apple-macos12.0
    LIB_ARCHFLAGS  := -arch arm64e
    LIB_TRIPLE     := arm64e-apple-macos12.0
else
    $(error Unknown ARCH=$(ARCH). Use arm64e, x86_64, or universal)
endif

KEXT_FLAGS := ARCHFLAGS="$(KEXT_ARCHFLAGS)" TARGET_TRIPLE="$(KEXT_TRIPLE)"
LIB_FLAGS  := ARCHFLAGS="$(LIB_ARCHFLAGS)"  TARGET_TRIPLE="$(LIB_TRIPLE)"

# Force linear sequence execution (prevent interleaved stdout logging collisions)
.NOTPARALLEL:

# Default execution hooks everything target pipelines
all: build_all

ifeq ($(ARCH),universal)

# Target builds for universal fat binary distributions
FWBIN := $(BUNDLE_NAME).framework/Versions/A/$(BUNDLE_NAME)

build_all:
	rm -rf $(OUT)
	mkdir -p $(OUT) $(OUT)/.arm64e $(OUT)/.x86_64
	@echo "==> Building Apple Silicon Slices..."
	$(MAKE) -C lib/libDirectHW all $(LIB_FLAGS)
	$(MAKE) -C kext debug $(KEXT_FLAGS)
	mv kext/$(KEXTNAME).kext $(OUT)/$(KEXTNAME).kext.arm64e
	cp lib/libDirectHW/libDirectHW.a     $(OUT)/.arm64e/
	cp lib/libDirectHW/libDirectHW.dylib $(OUT)/.arm64e/
	cp lib/libDirectHW/$(FWBIN)          $(OUT)/.arm64e/fwbin
	@# Keep one full framework skeleton (symlinks, Info.plist) from this slice
	cp -R lib/libDirectHW/$(BUNDLE_NAME).framework $(OUT)/$(BUNDLE_NAME).framework
	$(MAKE) -C kext clean
	$(MAKE) -C lib/libDirectHW clean
	@echo "==> Building Intel Core Slices..."
	$(MAKE) -C lib/libDirectHW all ARCHFLAGS="-arch x86_64" TARGET_TRIPLE="x86_64-apple-macos10.15"
	$(MAKE) -C kext debug ARCHFLAGS="-arch x86_64" TARGET_TRIPLE="x86_64-apple-macos10.15"
	mv kext/$(KEXTNAME).kext $(OUT)/$(KEXTNAME).kext.x86_64
	cp lib/libDirectHW/libDirectHW.a     $(OUT)/.x86_64/
	cp lib/libDirectHW/libDirectHW.dylib $(OUT)/.x86_64/
	cp lib/libDirectHW/$(FWBIN)          $(OUT)/.x86_64/fwbin
	@echo "==> Packaging Universal Fat Binaries..."
	cp -R $(OUT)/$(KEXTNAME).kext.arm64e $(OUT)/$(KEXTNAME).kext
	lipo -create $(OUT)/$(KEXTNAME).kext.arm64e/Contents/MacOS/$(KEXTNAME) \
	             $(OUT)/$(KEXTNAME).kext.x86_64/Contents/MacOS/$(KEXTNAME) \
	     -output $(OUT)/$(KEXTNAME).kext/Contents/MacOS/$(KEXTNAME)
	codesign --force --timestamp=none --sign - $(OUT)/$(KEXTNAME).kext
	@# Fuse the userspace library slices into fat binaries
	lipo -create $(OUT)/.arm64e/libDirectHW.a     $(OUT)/.x86_64/libDirectHW.a     -output $(OUT)/libDirectHW.a
	lipo -create $(OUT)/.arm64e/libDirectHW.dylib $(OUT)/.x86_64/libDirectHW.dylib -output $(OUT)/libDirectHW.dylib
	lipo -create $(OUT)/.arm64e/fwbin             $(OUT)/.x86_64/fwbin             -output $(OUT)/$(FWBIN)
	rm -rf $(OUT)/$(KEXTNAME).kext.arm64e $(OUT)/$(KEXTNAME).kext.x86_64 $(OUT)/.arm64e $(OUT)/.x86_64

else

# Target builds for single selected architecture slices
build_all:
	rm -rf $(OUT)
	mkdir -p $(OUT)
	@echo "==> Compiling Subdirectories ($(ARCH))..."
	$(MAKE) -C lib/libDirectHW all $(LIB_FLAGS)
	$(MAKE) -C kext debug $(KEXT_FLAGS)
	@echo "==> Staging Final Artifacts into $(OUT)/..."
	mv kext/$(KEXTNAME).kext $(OUT)/
	-mv kext/$(KEXTNAME).kext.dSYM $(OUT)/ 2>/dev/null || true
	mv lib/libDirectHW/libDirectHW.a lib/libDirectHW/libDirectHW.dylib lib/libDirectHW/$(BUNDLE_NAME).framework $(OUT)/

endif

# Convenience pass-through rule for testing kernel loading parameters
load: all
	$(MAKE) -C kext load KEXTBUNDLE="../$(OUT)/$(KEXTNAME).kext" KEXTNAME="$(KEXTNAME)"

unload:
	$(MAKE) -C kext unload KEXTBUNDLE="../$(OUT)/$(KEXTNAME).kext"

# Installation destinations (override on the command line if needed)
KEXT_PREFIX     ?= /Library/Extensions
INCLUDEDIR      ?= /usr/local/include
LIBDIR          ?= /usr/local/lib
FRAMEWORKDIR    ?= /Library/Frameworks
SBINDIR         ?= /usr/local/sbin
LAUNCHDAEMONDIR ?= /Library/LaunchDaemons
DAEMON_LABEL    := com.beako.utils.directhwd

# Note: install does NOT depend on `all`. Running `sudo make install` must not
# rebuild, otherwise the build artifacts in $(OUT) end up owned by root. Build
# first as a normal user (`make`), then `sudo make install`.
install:
	@test -d "$(OUT)/$(KEXTNAME).kext" || { \
		echo "error: $(OUT)/$(KEXTNAME).kext not found - run 'make' first"; exit 1; }
	@echo "==> Installing $(KEXTNAME).kext into $(KEXT_PREFIX)"
	sudo rm -rf "$(KEXT_PREFIX)/$(KEXTNAME).kext"
	sudo cp -R "$(OUT)/$(KEXTNAME).kext" "$(KEXT_PREFIX)/$(KEXTNAME).kext"
	sudo chown -R root:wheel "$(KEXT_PREFIX)/$(KEXTNAME).kext"
	sudo chmod -R 755 "$(KEXT_PREFIX)/$(KEXTNAME).kext"
	@echo "==> Installing DirectHW.h into $(INCLUDEDIR)"
	sudo mkdir -p "$(INCLUDEDIR)"
	sudo install -m 0644 include/DirectHW.h "$(INCLUDEDIR)/DirectHW.h"
	@echo "==> Installing libraries into $(LIBDIR)"
	sudo mkdir -p "$(LIBDIR)"
	sudo install -m 0644 "$(OUT)/libDirectHW.a"     "$(LIBDIR)/libDirectHW.a"
	sudo install -m 0755 "$(OUT)/libDirectHW.dylib" "$(LIBDIR)/libDirectHW.dylib"
	@echo "==> Installing DirectHW.framework into $(FRAMEWORKDIR)"
	sudo rm -rf "$(FRAMEWORKDIR)/DirectHW.framework"
	sudo cp -R "$(OUT)/DirectHW.framework" "$(FRAMEWORKDIR)/DirectHW.framework"

uninstall:
	sudo rm -rf "$(KEXT_PREFIX)/$(KEXTNAME).kext"
	sudo rm -f  "$(INCLUDEDIR)/DirectHW.h"
	sudo rm -f  "$(LIBDIR)/libDirectHW.a" "$(LIBDIR)/libDirectHW.dylib"
	sudo rm -rf "$(FRAMEWORKDIR)/DirectHW.framework"

# Install a LaunchDaemon that live-loads the kext at boot (needed on Apple
# Silicon when the kext cannot be baked into the boot kext collection). Run
# after `make install`. bootstrap also loads it immediately.
install-daemon:
	@echo "==> Installing directhwd loader into $(SBINDIR)"
	sudo mkdir -p "$(SBINDIR)"
	sudo install -o root -g wheel -m 0755 tools/directhwd.sh "$(SBINDIR)/directhwd"
	@echo "==> Installing LaunchDaemon into $(LAUNCHDAEMONDIR)"
	sudo install -o root -g wheel -m 0644 tools/$(DAEMON_LABEL).plist "$(LAUNCHDAEMONDIR)/$(DAEMON_LABEL).plist"
	@echo "==> Activating LaunchDaemon"
	-sudo launchctl bootout system "$(LAUNCHDAEMONDIR)/$(DAEMON_LABEL).plist" 2>/dev/null || true
	sudo launchctl bootstrap system "$(LAUNCHDAEMONDIR)/$(DAEMON_LABEL).plist"

uninstall-daemon:
	-sudo launchctl bootout system "$(LAUNCHDAEMONDIR)/$(DAEMON_LABEL).plist" 2>/dev/null || true
	sudo rm -f "$(LAUNCHDAEMONDIR)/$(DAEMON_LABEL).plist"
	sudo rm -f "$(SBINDIR)/directhwd"

clean:
	rm -rf $(OUT)
	$(MAKE) -C lib/libDirectHW clean
	$(MAKE) -C kext clean

.PHONY: all build_all load unload install uninstall install-daemon uninstall-daemon clean
