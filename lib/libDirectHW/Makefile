# Include the variable definitions from the parent directory
-include ../../Makefile.inc

# Compiler and archiving tools
CC       := clang
AR       := ar
ARFLAGS  := rcs
RANLIB   := ranlib

# Honor the architecture selection passed down from the root Makefile so that
# cross and "universal" builds actually target the requested slice. Defaults to
# the native architecture when invoked standalone.
ARCHFLAGS ?=
TARGET_TRIPLE ?=
CFLAGS   := -Wall -Wextra -O2 -fPIC $(ARCHFLAGS)
ifneq ($(TARGET_TRIPLE),)
CFLAGS   += -target $(TARGET_TRIPLE)
LDARCH   := $(ARCHFLAGS) -target $(TARGET_TRIPLE)
else
LDARCH   := $(ARCHFLAGS)
endif

SDKROOT?=       $(shell xcrun --sdk macosx --show-sdk-path)
IOKIT_FW=       $(SDKROOT)/System/Library/Frameworks/IOKit.framework
IOKIT_HEADERS=  $(IOKIT_FW)/Headers

# Library and Framework names
LIB_NAME  := $(BUNDLE_NAME)
STATIC_LIB   := lib$(BUNDLE_NAME).a
DYNAMIC_LIB  := lib$(BUNDLE_NAME).dylib
FRAMEWORK_NAME := $(BUNDLE_NAME).framework

# Source and Object files
SRCS := DirectHW.c
OBJS := $(SRCS:.c=.o)
INC := -I../../include

Info.plist~: Info.plist.in
	sed \
		-e 's/__BUNDLE_NAME__/$(BUNDLE_NAME)/g' \
		-e 's/__BUNDLE_ID__/$(BUNDLE_ID)/g' \
		-e 's/__BUNDLE_VERSION__/$(BUNDLE_VERSION)/g' \
	$^ > $@

# Default target builds everything including the processed framework
all: $(STATIC_LIB) $(DYNAMIC_LIB) framework

# Rule for building the Dynamic Library (.dylib)
$(DYNAMIC_LIB): $(OBJS)
	$(CC) $(LDARCH) -dynamiclib -framework IOKit -o $@ $(OBJS)

# Rule for building the Static Library (.a)
$(STATIC_LIB): $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)
	$(RANLIB) $@

# Rule for building the macOS Framework Bundle structure
framework: $(OBJS) Info.plist~
	@echo "==> Building $(FRAMEWORK_NAME) bundle..."
	rm -rf $(FRAMEWORK_NAME)
	mkdir -p $(FRAMEWORK_NAME)/Versions/A/Resources
	$(CC) $(LDARCH) -dynamiclib -framework IOKit -install_name @rpath/$(FRAMEWORK_NAME)/Versions/A/$(LIB_NAME) -o $(FRAMEWORK_NAME)/Versions/A/$(LIB_NAME) $(OBJS)
	cp Info.plist~ $(FRAMEWORK_NAME)/Versions/A/Resources/Info.plist
	ln -sf A $(FRAMEWORK_NAME)/Versions/Current
	ln -sf Versions/Current/$(LIB_NAME) $(FRAMEWORK_NAME)/$(LIB_NAME)
	ln -sf Versions/Current/Resources $(FRAMEWORK_NAME)/Resources

# Compilation rule for object files
%.o: %.c
	$(CC) $(CFLAGS) -I$(IOKIT_HEADERS) $(INC) -c $< -o $@

# Clean up build artifacts
clean:
	rm -f $(OBJS) $(STATIC_LIB) $(DYNAMIC_LIB) Info.plist Info.plist~
	rm -rf $(FRAMEWORK_NAME)

.PHONY: all clean framework
