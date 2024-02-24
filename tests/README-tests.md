`tests/Makefile` builds and runs the tests for all compilers discovered on the system. The test matrix checks different build modes, standard libraries, and library configurations.

Windows users should install `make` from MSYS2.

Run the tests using `make -C tests`.

Read the makefile for some parameters you can customize. You can limit what is tested by overriding those variables:

* `COMPILERS` - list of compilers to test. (All lists are space-separated.)
* `MODES` - list of build modes to test.
* `STANDARDS` - list of C++ standard versions to test.
* `STDLIBS` - list of C++ standard libraries to test (Clang only).
* `FMTLIBS` - list of formatting libraries to test.

E.g. `make -C tests COMPILERS=clang++` will only test Clang instead of all available compilers.

---

Run `make -f tests/Makefile set-ide-target ...` to generate `compile_commands.json` for your IDE and some VSCode config files. This requires setting the variables above to uniquely specify a single configuration, for example:

* Clang: `make -C tests set-ide-target COMPILERS=clang++ STDLIBS=libstdc++ STANDARDS=20 FMTLIBS=stdfmt MODES=sanitizers`
* GCC: `make -C tests set-ide-target COMPILERS=g++ STANDARDS=20 FMTLIBS=stdfmt MODES=sanitizers`
* MSVC: `make -C tests set-ide-target COMPILERS=cl STANDARDS=latest FMTLIBS=stdfmt MODES=sanitizers`

---

What different files are used for:

* `sanity_check.cpp` - a minimal test program that's checked by the makefile to make sure our library is sane enough to run self-tests.
* `tests.cpp` - the main tests.
* `scratchpad.cpp` - a dummy file that can be used to test things during development.
* `test_programs/` - some of the larger test programs ran by `tests.cpp` are loaded from this directory.
* `build/` - the makefile puts the build results there.
  * `build/??/tmp[.exe]` is the last executable compiled by `tests.cpp`. Run it from here if the tests fail.
