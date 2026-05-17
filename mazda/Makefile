# Multi-target cross-compile Makefile for LD_PRELOAD-able shared libraries
# that run on the Mazda CMU (ARM Cortex-A9 NEON, glibc/Linux). Uses the
# m3-toolchain submodule.
#
# Layout convention: each entry in $(PATCHES) names an OEM .so we patch
# with our own preload shim. Sources for patch <name> live in
# patches/<name>/, and the build produces build/<cfg>/libpatch-<name>.so.
#
# Adding a new patch:
#   1. mkdir patches/<oem-lib-name>
#   2. drop one or more .cpp files into it
#   3. append <oem-lib-name> to $(PATCHES) below
#
# Targets:
#   make                -> all release libs (default)
#   make debug          -> all debug libs
#   make release        -> all release libs
#   make all            -> debug + release
#   make <name>         -> release lib for one patch (e.g. `make blmjciaapa`)
#   make <name>-debug   -> debug   lib for one patch (e.g. `make blmjciaapa-debug`)
#   make clean          -> wipe build/

M3TOOLCHAIN := ./m3-toolchain
TRIPLE      := arm-cortexa9_neon-linux-gnueabi
CXX         := $(M3TOOLCHAIN)/bin/$(TRIPLE)-g++
SYSROOT     := $(M3TOOLCHAIN)/$(TRIPLE)/sysroot

# OEM libraries we patch. Order doesn't matter.
PATCHES := blmjciaapa

# Flags shared by all patches and both configurations.
# -fPIC is required for code that goes into a shared object.
# -static-libstdc++ keeps the .so off the device's older libstdc++ at
# load time (same rationale as the original single-binary build).
COMMON_CXXFLAGS := -std=c++11 \
                   --sysroot=$(SYSROOT) \
                   -march=armv7-a -mtune=cortex-a9 -mfpu=neon \
                   -fPIC \
                   -fvisibility=hidden \
                   -static-libstdc++
COMMON_LDFLAGS  := --sysroot=$(SYSROOT) -shared -static-libstdc++ \
                   -Wl,--no-undefined \
                   -lpthread -ldl

# Debug: no optimization, full debug info, no LTO so symbols/inlining are sane.
DEBUG_CXXFLAGS   := $(COMMON_CXXFLAGS) -O0 -g3 -DDEBUG
DEBUG_LDFLAGS    := $(COMMON_LDFLAGS)

# Release: -O3 with LTO across translation units, dead-section GC, stripped.
RELEASE_CXXFLAGS := $(COMMON_CXXFLAGS) -O3 -DNDEBUG \
                    -ffunction-sections -fdata-sections \
                    -flto
RELEASE_LDFLAGS  := $(COMMON_LDFLAGS) -O3 -flto \
                    -Wl,--gc-sections -Wl,-s

BUILD_DIR   := build
DEBUG_DIR   := $(BUILD_DIR)/debug
RELEASE_DIR := $(BUILD_DIR)/release

# Per-patch derived names: build/<cfg>/libpatch-<name>.so
DEBUG_LIBS   := $(addprefix $(DEBUG_DIR)/libpatch-,$(addsuffix .so,$(PATCHES)))
RELEASE_LIBS := $(addprefix $(RELEASE_DIR)/libpatch-,$(addsuffix .so,$(PATCHES)))

.PHONY: all debug release clean $(PATCHES) $(addsuffix -debug,$(PATCHES))
.DEFAULT_GOAL := release

all: debug release
debug: $(DEBUG_LIBS)
release: $(RELEASE_LIBS)

# Convenience aliases:
#   `make blmjciaapa`        -> build/release/libpatch-blmjciaapa.so
#   `make blmjciaapa-debug`  -> build/debug/libpatch-blmjciaapa.so
$(PATCHES): %: $(RELEASE_DIR)/libpatch-%.so
$(addsuffix -debug,$(PATCHES)): %-debug: $(DEBUG_DIR)/libpatch-%.so

# Object-file pattern rules: mirror the patches/ tree under build/<cfg>/.
#   build/release/<patch>/foo.o  <-  patches/<patch>/foo.cpp
$(DEBUG_DIR)/%.o: patches/%.cpp
	@mkdir -p $(@D)
	$(CXX) -MD $(DEBUG_CXXFLAGS) -c $< -o $@

$(RELEASE_DIR)/%.o: patches/%.cpp
	@mkdir -p $(@D)
	$(CXX) -MD $(RELEASE_CXXFLAGS) -c $< -o $@

# Per-patch link rules generated via $(eval). Sources are discovered with
# $(wildcard) at parse time, so adding a new .cpp under an existing
# patches/<name>/ is a no-op for the Makefile, but adding a new patch
# directory means appending its name to $(PATCHES) above.
define gen_patch_rules
$(DEBUG_DIR)/libpatch-$(1).so: $(patsubst patches/$(1)/%.cpp,$(DEBUG_DIR)/$(1)/%.o,$(wildcard patches/$(1)/*.cpp))
	$$(CXX) $$^ $$(DEBUG_LDFLAGS) -Wl,-soname,libpatch-$(1).so -o $$@

$(RELEASE_DIR)/libpatch-$(1).so: $(patsubst patches/$(1)/%.cpp,$(RELEASE_DIR)/$(1)/%.o,$(wildcard patches/$(1)/*.cpp))
	$$(CXX) $$^ $$(RELEASE_LDFLAGS) -Wl,-soname,libpatch-$(1).so -o $$@

-include $(patsubst patches/$(1)/%.cpp,$(DEBUG_DIR)/$(1)/%.d,$(wildcard patches/$(1)/*.cpp))
-include $(patsubst patches/$(1)/%.cpp,$(RELEASE_DIR)/$(1)/%.d,$(wildcard patches/$(1)/*.cpp))
endef

$(foreach p,$(PATCHES),$(eval $(call gen_patch_rules,$(p))))

clean:
	rm -rf $(BUILD_DIR)
