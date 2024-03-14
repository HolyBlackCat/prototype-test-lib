# Configuration:

# Guess host and target OS.
ifneq ($(findstring Msys,$(call safe_shell,uname -o)),)
HOST_OS := windows
else
HOST_OS := linux
endif

ifeq ($(OS),Windows_NT)# Quasi-MSYS2 also sets this, by the way.
TARGET_OS := windows
else
TARGET_OS := linux
endif

$(info HOST_OS = $(HOST_OS))
$(info TARGET_OS = $(TARGET_OS))

# Compilers to test. We try to find them automatically.
COMPILERS := $(shell bash -c 'compgen -c g++; compgen -c clang++; compgen -c cl; compgen -c clang-cl' | grep -Po '^((clan)?g\+\+(-[0-9]+)?|cl$(if $(filter windows,$(TARGET_OS)),|clang-cl))(?=.exe)?$$' | sort -hr -t- -k2 | uniq)
$(if $(COMPILERS),,$(error Unable to detect compilers, set `COMPILERS=??` to a space-separated compiler list))

# Build modes.
MODES := sanitizers msan debug release

MODEFLAGS-sanitizers := -g -fsanitize=address -fsanitize=undefined -D_GLIBCXX_DEBUG
MODEFLAGS-msan       := -g -fsanitize=memory
MODEFLAGS-debug      := -g
MODEFLAGS-release    := -O3 -flto

# C++ versions.
# We automatically deduplicate equivalent spellings of the same version, if the compiler supports both.
STANDARDS := 20 2a 23 2b latest

# C++ standard libraries.
# Compilers other than Clang always use `stock` here.
STDLIBS := stock libstdc++ libc++

# Which formatting libraries to test.
FMTLIBS := stdfmt libfmt

# Combinations that should be skipped, in form of `$(compiler)_$(stdlib)_c++$(stdver)`, e.g. `clang++_libstdc++_c++2b`
SKIP :=

# Where to build.
BUILD_DIR := build

# Those command-line arguments are passed to our test executables.
ARGS :=

# The standard build flags.
CXXFLAGS ?=
LDFLAGS ?=

# Some standard build tools and configurations, common for all (or most) compilers.
AR ?= ar

# File extensions.
ifeq ($(TARGET_OS),windows)
EXE_EXT := .exe
DLL_EXT := .dll
else
EXE_EXT :=
DLL_EXT := .so
endif

# If specified, this wrapper is used to run executables.
EXE_RUNNER := $(if $(and $(filter windows,$(TARGET_OS)),$(filter linux,$(HOST_OS))),wine)
$(if $(EXE_RUNNER),$(info EXE_RUNNER = $(EXE_RUNNER)))

# --- Variables mostly for internal use:

# Those variables need to be baked into VSCode config when selecting the current target.
override config_var_list := HOST_OS TARGET_OS COMPILERS MODES STANDARDS STDLIBS FMTLIBS EXE_RUNNER ARGS

# If set to 0, disables automatic capability discovery for the detected compilers.
# You must manually set all the variables above to sane values.
AUTO_DETECT_CAPABILITIES := 1
override AUTO_DETECT_CAPABILITIES := $(filter-out 0,$(AUTO_DETECT_CAPABILITIES))

# If set to 0, disables automatic test rebuild. Runs the existing test binaries, or fails trying.
AUTO_REBUILD_TESTS := 1
override AUTO_REBUILD_TESTS := $(filter-out 0,$(AUTO_REBUILD_TESTS))

# ---

# Disable localizations.
export LANG=
# Disable MSVC localization, just in case.
export VSLANG=1033

override space := $(strip) $(strip)
override comma := ,
override define lf :=
$(call)
$(call)
endef

# Used to create variables in functions in a safer way. E.g. `$(call var,x := 42)`.
override var = $(eval override $(subst $,$$$$,$1))

# Encloses $1 in single quotes, with proper escaping for the shell.
override quote = '$(subst ','"'"',$1)'

# Encloses $2 in double quotes, with some generic escaping.
override doublequote = "$(call doublequote_only_escape,$1)"
# Same, but only does escaping without the outer quotes.
override doublequote_only_escape = $(subst ",\",$(subst \,\\,$1))

# Whether `--trace` was passed.
override tracing = $(if $(filter --trace,$(MAKEFLAGS)),1)

# ---

# Make sure the working directory is correct.
$(if $(wildcard ../include/taut/taut.hpp),,$(error The working directory is incorrect. Run from the repo root as `make -C tests`, or from the tests directory.))

# Tests if code string $4 compiles on compiler $1, stdlib $2, and C++ version $3 (C++ version can be omitted).
override test_compile = $(strip\
	$(if $(msvc_like-$1),\
		$(call var,_file := $(shell mktemp))\
		$(file >$(_file).cpp,$4)\
		$(call test_compile_low,$1 /nologo /EHsc /std:c++$3 /Zs $(_file).cpp $(CXXFLAGS))\
	,\
		$(call test_compile_low,echo '$(subst $(lf),'$$'\n'',$4)' | $1 -fsyntax-only $(if $(filter stock,$2),,-stdlib=$2) $(if $3,-std=c++$3) -xc++ - $(CXXFLAGS))\
	)\
)
override test_compile_low = $(if $(tracing),$(info Test compilation: $1))$(shell $1 $(if $(tracing),,>/dev/null 2>/dev/null))$(if $(filter 0,$(.SHELLSTATUS)),1)

# Analyze compiler capabilties.
$(info Compilers:)
$(foreach comp,$(COMPILERS),\
	$(call, ### Whether this is a MSVC-like compiler or not.)\
	$(call var,msvc_like-$(comp) := $(if $(filter %cl,$(comp)),1))\
	$(call, ### Check supported C++ versions and standard libraries.)\
	$(call var,stdvers-$(comp) := $(STANDARDS))\
	$(call var,stdlibs-$(comp) := $(STDLIBS))\
	$(if $(AUTO_DETECT_CAPABILITIES),\
		$(if $(msvc_like-$(comp)),\
			$(call var,_supported_stdvers := $(shell LANG= $(comp) /nologo /? | grep /std: | grep -Po '(?<=c\+\+)[0-9a-z]+'))\
			$(call var,_supported_stdlibs := stock)\
		,\
			$(if $(findstring clang++,$(lastword $(subst /, ,$(comp)))),\
				$(call var,_supported_stdvers := $(shell LANG= $(comp) --std=blah -x c++ - </dev/null 2>&1 | grep -Po '(?<=c\+\+)[0-9a-z]+'))\
				$(call var,_supported_stdlibs := $(strip $(foreach x,libstdc++ libc++,\
					$(if $(call test_compile,$(comp),$x,20,#include <version>),$x)\
				)))\
			,\
				$(call var,_supported_stdvers := $(shell LANG= $(comp) --help -v 2>&1 | grep -P '^\s{0,8}-std=c\+\+' | grep -v 'Same as' | grep -Po '(?<=-std=c\+\+)[0-9a-z]+(?= )'))\
				$(call var,_supported_stdlibs := stock)\
			)\
		)\
		$(call, ### Filter C++ versions and standard libraries.)\
		$(if $(_supported_stdvers),,$(error Can't determine supported C++ versions for `$(comp)`))\
		$(if $(_supported_stdlibs),,$(error Can't determine supported C++ standard libraries for `$(comp)`))\
		$(call var,stdvers-$(comp) := $(filter $(_supported_stdvers),$(stdvers-$(comp))))\
		$(call var,stdlibs-$(comp) := $(filter $(_supported_stdlibs),$(stdlibs-$(comp))))\
	)\
	$(if $(stdvers-$(comp)),,$(error All supported C++ versions are disabled for `$(comp)`))\
	$(if $(stdlibs-$(comp)),,$(error All supported C++ standard libraries are disabled for `$(comp)`))\
	$(call, ### Check supported formatting libraries.)\
	$(foreach s,$(stdlibs-$(comp)),\
		$(call var,fmtlibs-$(comp)-$s := $(FMTLIBS))\
		$(if $(AUTO_DETECT_CAPABILITIES),\
			$(call var,_supported_fmtlibs :=\
				$(if $(call test_compile,$(comp),$s,20,#include <format>$(lf)int main(){(void)std::format("{}",42);}),stdfmt)\
				$(if $(call test_compile,$(comp),$s,20,#include <fmt/format.h>$(lf)int main(){(void)fmt::format("{}",42);}),libfmt)\
			)\
			$(if $(_supported_fmtlibs),,$(error Can't determine supported formatting libraries for `$(comp)-$s`))\
			$(call var,fmtlibs-$(comp)-$s := $(filter $(_supported_fmtlibs),$(fmtlibs-$(comp)-$s)))\
		)\
		$(if $(fmtlibs-$(comp)-$s),,$(error All supported formatting libraries are disabled for `$(comp)-$s`))\
	)\
	$(call, ### Check supported build modes.)\
	$(call var,modes-$(comp) := $(MODES))\
	$(if $(msvc_like-$(comp)),\
		$(call var,modes-$(comp) := $(filter-out msan,$(modes-$(comp))))\
	)\
	$(if $(modes-$(comp)),,$(error All build modes are disabled for `$(comp)`))\
	$(call, ### Log.)\
	$(info $(shell printf "* %-12s msvc_like=%-2s standards=%-10s modes=%-30s stdlibs=%-17s"\
		$(comp)\
		$(if $(msvc_like-$(comp)),1,0)\
		$(subst $(space),$(comma),$(stdvers-$(comp)))\
		$(subst $(space),$(comma),$(modes-$(comp)))\
		$(call quote,$(subst $(space),$(comma),$(foreach s,$(stdlibs-$(comp)),$s($(fmtlibs-$(comp)-$s)))))\
	))\
)

# A list of all configurations we're testing.
override all_configs :=

# Helpers for `snippet_make_recipes` to generate different kinds of env variable lists.
override env_vars_to_shell = $1=$(call quote,$2)
override env_vars_to_json = $(call doublequote,$1):$(call doublequote,$2),

# Generates recipes for a specific compiler and configuration.
# Reads following variables: `comp`, `stdlib`, `stdver`, `fmtlib`, `mode`. Also uses `msvc_like:$(comp)` to determine what compiler we're using.
override define snippet_make_recipes =

$(call var,_config := $(comp)_$(stdlib)_c++$(stdver)_$(fmtlib)_$(mode))
$(call var,all_configs += $(_config))

$(if $(filter undefined environment,$(origin MODEFLAGS-$(mode))),$(error Unknown build mode: $(mode)))

$(call var,_cxx := $(comp) $(if $(filter-out stock,$(stdlib)),-stdlib=$(stdlib)) -std=c++$(stdver) $(MODEFLAGS-$(mode)) -DCFG_TA_USE_LIBFMT=$(if $(filter libfmt,$(fmtlib)),1,0)\
	-pedantic-errors -Wall -Wextra -Wdeprecated -Wextra-semi -Wimplicit-fallthrough -Wconversion\
	-Werror\
	$(CXXFLAGS)\
)

# The output directory.
$(call var,_build_dir := $(BUILD_DIR)/$(_config))
$(_build_dir):
	@mkdir -p $$@

# Object file for the static library.
$(call var,_obj_file_static := $(BUILD_DIR)/$(_config)/taut-static.o)
$(_obj_file_static): ../source/taut.cpp $(wildcard ../include/taut/*) | $(_build_dir)
	$$(info [$(_config)] Compiling: $$(notdir $$@))
	@$(_cxx) -c $$< -o $$@ -I../include -fvisibility=hidden

# Object file for the dynamic library.
$(call var,_obj_file_shared := $(BUILD_DIR)/$(_config)/taut.o)
$(_obj_file_shared): ../source/taut.cpp $(wildcard ../include/taut/*) | $(_build_dir)
	$$(info [$(_config)] Compiling: $$(notdir $$@))
	@$(_cxx) -c $$< -o $$@ -I../include -fvisibility=hidden -fPIC -DCFG_TA_SHARED

# Static library.
$(call var,_static_lib := $(BUILD_DIR)/$(_config)/libtaut-static.a)
$(_static_lib): $(_obj_file_static) | $(_build_dir)
	$(call, ### Remove the old library because `ar` always appends to the existing one.)
	$$(info [$(_config)] Creating static library: $$(notdir $$@))
	@rm -f $$@
	@ar rcs $$@ $$^

# Shared library.
$(call var,_shared_lib := $(BUILD_DIR)/$(_config)/libtaut$(DLL_EXT))
$(_shared_lib): $(_obj_file_shared) | $(_build_dir)
	$$(info [$(_config)] Creating shared library: $$(notdir $$@))
	@$(_cxx) -shared $$^ -o $$@ $(LDFLAGS)

# All the test executables.
$(call var,_all_exe_sources := tests.cpp scratchpad.cpp $(wildcard test_programs/*.cpp))
$(call var,_all_exes :=)
$(foreach source,$(_all_exe_sources),$(eval $(snippet_test_program)))

# The path for logs.
$(call var,_tests_log := $(BUILD_DIR)/$(_config)/tests_log.txt)

# The env variables for the main test executable. This is essentially an x-macro list.
$(eval override _tests_env_list = \
	$$(call $$1,LD_LIBRARY_PATH,$(BUILD_DIR)/$(_config))\
	$$(call $$1,VERBOSE,$(if $(tracing),1,0))\
	$$(call $$1,COMPILER_COMMAND,$(_cxx) -I../include -DCFG_TA_SHARED)\
	$$(call $$1,LINKER_FLAGS,-L$(dir $(_shared_lib)) -ltaut $(LDFLAGS))\
	$$(call $$1,OUTPUT_DIR,$(BUILD_DIR)/$(_config))\
	$$(call $$1,EXT_EXE,$(EXT_EXE))\
	$$(call $$1,EXE_RUNNER,$(EXE_RUNNER))\
)
# The env variables as a single string for the shell.
$(call var,_tests_env := $(call _tests_env_list,env_vars_to_shell))

# Building all binaries.
.PHONY: build_$(_config)
build_$(_config): $(_all_exes)

# Running the tests.
.PHONY: test_$(_config)
test_$(_config): $(if $(AUTO_REBUILD_TESTS),$(_all_exes))
	$(call, ### Run the sanity check.)
	$$(call,$$(shell LD_LIBRARY_PATH=$(BUILD_DIR)/$(_config) $(EXE_RUNNER) ./$(BUILD_DIR)/$(_config)/_sanity_check$(EXT_EXE) $(if $(tracing),,>/dev/null 2>/dev/null)))$$(if $$(filter-out 0,$$(.SHELLSTATUS)),$$(error Sanity check failed for `$(_config)` (happy path)))
	$$(call,$$(shell LD_LIBRARY_PATH=$(BUILD_DIR)/$(_config) $(EXE_RUNNER) ./$(BUILD_DIR)/$(_config)/_sanity_check$(EXT_EXE) $(if $(tracing),,>/dev/null 2>/dev/null) --fail-assertion))$$(if $$(filter 0,$$(.SHELLSTATUS)),$$(error Sanity check failed for `$(_config)` (assertion failure)))
	$$(call,$$(shell LD_LIBRARY_PATH=$(BUILD_DIR)/$(_config) $(EXE_RUNNER) ./$(BUILD_DIR)/$(_config)/_sanity_check$(EXT_EXE) $(if $(tracing),,>/dev/null 2>/dev/null) --throw-exception))$$(if $$(filter 0,$$(.SHELLSTATUS)),$$(error Sanity check failed for `$(_config)` (thrown exception)))
	$$(info [$(_config)] Sanity check passed)
	$(call, ### Run main tests.)
	@$(_tests_env) $(EXE_RUNNER) $(BUILD_DIR)/$(_config)/tests$(EXT_EXE) $(ARGS) >$(_tests_log) 2>$(_tests_log) || (cat $(_tests_log) && false)
	@echo '[$(_config)] Main tests passed'
	@true

endef
# A recipe for compiling a test program. Accepts the same variables as `snippet_make_recipes`, and also `source` for the source file.
override define snippet_test_program =
$(call var,_this_exe := $(BUILD_DIR)/$(_config)/$(if $(findstring /,$(source)),_)$(notdir $(source:.cpp=$(EXT_EXE))))
$(call var,_all_exes += $(_this_exe))
$(_this_exe): $(source) $(_shared_lib) | $(_build_dir)
	$$(info [$(_config)] Creating executable: $(notdir $(_this_exe)))
	@$(_cxx) $$(filter %.cpp,$$^) -I../include -DCFG_TA_SHARED -L$(dir $(_shared_lib)) -ltaut -o $$@
endef

$(foreach comp,$(COMPILERS),$(foreach stdlib,$(stdlibs-$(comp)),$(foreach stdver,$(stdvers-$(comp)),$(foreach fmtlib,$(fmtlibs-$(comp)-$(stdlib)),$(foreach mode,$(MODES),\
	$(if $(filter $(comp)_$(stdlib)_c++$(stdver)_$(fmtlib),$(SKIP)),,\
		$(eval $(snippet_make_recipes))\
	)\
)))))

.DEFAULT_GOAL := test_all
.PHONY: test_all
# Runs all the individual `test_*` targets, to run all tests.
test_all: $(addprefix test_,$(all_configs))

.PHONY: build_all
# Runs all the individual `build_*` targets, to build all test binaries.
build_all: $(addprefix build_,$(all_configs))

# Cleans the binaries and temporary files.
.PHONY: clean
clean:
	@rm -rf $(BUILD_DIR)

# Configures VSC for a specific configuration.
# The configuration variables must be set to uniquely identify a single configuration.
.PHONY: set-ide-target
set-ide-target:
ifneq ($(words $(all_configs)),1)
	$(error Must specify a single configuration, currently choosing from: $(all_configs))
else
# Here we're using some variables left over from the loop above. Since we only had one iteration, this is safe.
# First, launch.json:
	@echo $(call quote,// Don't edit directly, this file is generated from `launch.default.json`.) >../.vscode/launch.json
	@cat ../.vscode/launch.default.json >../.vscode/launch.json
	@sed 's|<EXECUTABLE>|'$(call quote,$(subst |,\|,$(call doublequote_only_escape,$(BUILD_DIR)/$(_config)/tests$(EXT_EXE))))'|g' -i ../.vscode/launch.json
	@sed 's|<ARGS>|'$(call quote,$(subst |,\|,$(call doublequote_only_escape,$(ARGS))))'|g' -i ../.vscode/launch.json
	@sed 's|/\*<ENV_JSON>\*/|'$(call quote,$(subst |,\|,$(call _tests_env_list,env_vars_to_json)))'|g' -i ../.vscode/launch.json
# tasks.json:
	@echo $(call quote,// Don't edit directly, this file is generated from `launch.default.json`.) >../.vscode/tasks.json
	@cat ../.vscode/tasks.default.json >../.vscode/tasks.json
	@sed 's|<CFG_VARS>|'$(call quote,$(subst |,\|,$(call doublequote_only_escape,$(foreach x,$(config_var_list),$x=$(call quote,$($x))))))'|g' -i ../.vscode/tasks.json
# compile_commands.json:
	@echo '[' >../compile_commands.json
	$(call var,_once := 1)
	@true $(foreach s,../source/taut.cpp $(_all_exe_sources),\
		&& echo '   $(if $(_once),$(call var,_once :=) ,$(comma)){"directory": "$(abspath .)", "file": "$(abspath $s)", "command": "$(filter-out -Werror,$(_cxx)) -I../include -DCFG_TA_SHARED"}' >>../compile_commands.json\
	)
	@echo ']' >>../compile_commands.json
endif
	@true
