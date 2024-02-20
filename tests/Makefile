# Compilers to test. We try to find them automatically.
COMPILERS := $(shell bash -c 'compgen -c g++; compgen -c clang++; compgen -c cl; compgen -c clang-cl' | grep -Po '^((clan)?g\+\+(-[0-9]+)?|cl|clang-cl)(?=.exe)?$$' | sort -hr -t- -k2 | uniq)
$(if $(COMPILERS),,$(error Unable to detect compilers, set `COMPILERS=??` to a space-separated compiler list))

# Build modes.
MODES := asan-ubsan msan default release

MODEFLAGS-asan-ubsan := -g -fsanitize=address -fsanitize=undefined
MODEFLAGS-msan       := -g -fsanitize=memory
MODEFLAGS-default    :=
MODEFLAGS-release    := -O3 -flto

# C++ versions.
# We automatically deduplicate equivalent spellings of the same version, if the compiler supports both.
STANDARDS := 20 2a 23 2b latest

# C++ standard libraries.
STDLIBS := libstdc++ libc++ msvc-stl

# Combinations that should be skipped, in form of `$(compiler)_$(stdlib)_c++$(stdver)`, e.g. `clang++_libstdc++_c++2b`
SKIP :=

# Where to build.
BUILD_DIR := build

# Some standard build tools and configurations, common for all (or most) compilers.
AR = ar

ifeq ($(OS),Windows_NT)# Quasi-MSYS2 also sets this, by the way.
TARGET_OS := windows
else
TARGET_OS := linux
endif

ifeq ($(TARGET_OS),windows)
DLL_EXT := .dll
else
DLL_EXT := .so
endif

# ---

# Disable localizations.
export LANG=

override space := $(strip) $(strip)
override comma := ,

# Used to create variables in functions in a safer way. E.g. `$(call var,x := 42)`.
override var = $(eval override $(subst $,$$$$,$1))

# ---

# Make sure the working directory is correct.
$(if $(wildcard ../include/taut/taut.hpp),,$(error The working directory is incorrect. Run from the repo root as `make -C tests`, or from the tests directory.))

# Analyze compiler capabilties.
$(info Compilers:)
$(foreach comp,$(COMPILERS),\
	$(call, ### Whether this is a MSVC-like compiler or not.)\
	$(call var,msvc_like-$(comp) := $(if $(filter %cl,$(comp)),1))\
	$(call, ### Check supported C++ versions and standard libraries.)\
	$(if $(msvc_like-$(comp)),\
		$(call var,stdvers-$(comp) := $(shell LANG= $(comp) /nologo /? | grep /std: | grep -Po '(?<=c\+\+)[0-9a-z]+'))\
		$(call var,stdlibs-$(comp) := msvc-stl)\
	,\
		$(if $(findstring clang++,$(lastword $(subst /, ,$(comp)))),\
			$(call var,stdvers-$(comp) := $(shell LANG= $(comp) --std=blah -x c++ - </dev/null 2>&1 | grep -Po '(?<=c\+\+)[0-9a-z]+'))\
			$(call var,stdlibs-$(comp) := $(strip $(foreach x,libstdc++ libc++,\
				$(call,$(shell echo "#include <version>" | $(comp) -stdlib=$x -x c++ -fsyntax-only - 2>/dev/null))$(if $(filter 0,$(.SHELLSTATUS)),$x)\
			)))\
		,\
			$(call var,stdvers-$(comp) := $(shell LANG= $(comp) --help -v 2>&1 | grep -P '^\s{0,8}-std=c\+\+' | grep -v 'Same as' | grep -Po '(?<=-std=c\+\+)[0-9a-z]+(?= )'))\
			$(call var,stdlibs-$(comp) := libstdc++)\
		)\
	)\
	$(call, ### Filter C++ standards.)\
	$(if $(stdvers-$(comp)),,$(error Can't determine supported C++ versions for `$(comp)`))\
	$(call var,stdvers-$(comp) := $(filter $(STANDARDS),$(stdvers-$(comp))))\
	$(if $(stdvers-$(comp)),,$(error All supported C++ versions are disabled for `$(comp)`))\
	$(call, ### Filter C++ standard libraries.)\
	$(if $(stdlibs-$(comp)),,$(error Can't determine supported C++ standard libraries for `$(comp)`))\
	$(call var,stdlibs-$(comp) := $(filter $(STDLIBS),$(stdlibs-$(comp))))\
	$(if $(stdlibs-$(comp)),,$(error All supported C++ standard libraries are disabled for `$(comp)`))\
	$(call, ### Check supported build modes.)\
	$(call var,modes-$(comp) := $(MODES))\
	$(if $(msvc_like-$(comp)),\
		$(call var,modes-$(comp) := $(filter-out msan,$(modes-$(comp))))\
	)\
	$(if $(modes-$(comp)),,$(error All build modes are disabled for `$(comp)`))\
	$(call, ### Log.)\
	$(info $(shell printf "* %-12s msvc_like=%-2s standards=%-10s stdlibs=%-17s modes=%s" $(comp) $(if $(msvc_like-$(comp)),1,0) $(subst $(space),$(comma),$(stdvers-$(comp))) $(subst $(space),$(comma),$(stdlibs-$(comp))) $(subst $(space),$(comma),$(modes-$(comp)))))\
)

# A list of all configurations we're testing.
override all_configs :=

# Generates recipes for a specific compiler and configuration.
# Reads following variables: `comp`, `stdlib`, `stdver`, `mode`. Also uses `msvc_like:$(comp)` to determine what compiler we're using.
override define make_recipes =

$(call var,_config := $(comp)_$(stdlib)_c++$(stdver)_$(mode))
$(call var,all_configs += $(_config))

$(call var,_cxx := $(comp) -stdlib=$(stdlib) -std=c++$(stdver) -fPIC)

# Object file for the library.
$(call var,_obj_file := $(BUILD_DIR)/$(_config)/taut.o)
$(_obj_file): ../source/taut.cpp $(wildcard ../include/taut/*)
	$$(info [$(_config)] Compiling: $$(notdir $$<))
	@mkdir -p $$(dir $$@)
	@$(_cxx) -c $$< -o $$@ -I../include

# Static library.
$(call var,_static_lib := $(BUILD_DIR)/$(_config)/libtaut-static.a)
$(_static_lib): $(_obj_file)
	$(call, ### Remove the old library because `ar` always appends to the existing one.)
	$$(info [$(_config)] Creating static library: $$(notdir $$@))
	@rm -f $$@
	@ar rcs $$@ $$^

# Shared library.
$(call var,_shared_lib := $(BUILD_DIR)/$(_config)/libtaut$(DLL_EXT))
$(_shared_lib): $(_obj_file)
	$$(info [$(_config)] Creating shared library: $$(notdir $$@))
	@$(_cxx) -shared $$^ -o $$@

.PHONY: test_$(_config)
test_$(_config): $(_static_lib) $(_shared_lib)

endef
$(foreach comp,$(COMPILERS),$(foreach stdlib,$(stdlibs-$(comp)),$(foreach stdver,$(stdvers-$(comp)),$(foreach mode,$(MODES),\
	$(if $(filter $(comp)_$(stdlib)_c++$(stdver),$(SKIP)),,\
		$(eval $(make_recipes))\
	)\
))))

.DEFAULT_GOAL := test_all
.PHONY: test_all
test_all: $(addprefix test_,$(all_configs))

.PHONY: clean
clean:
	@rm -rf $(BUILD_DIR)
