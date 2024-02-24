`Makefile` builds and runs the tests for all compilers discovered on the system. The test matrix checks different build modes, standard libraries, and library configurations.

Windows users should install `make` from MSYS2.

Run the tests using `make -f tests/Makefile`.

Read the makefile for some parameters you can customize. Some of them are:

* `COMPILERS` - list of compilers to test. (All lists are space-separated.)
* `MODES` - list of build modes to test.
* `STANDARDS` - list of C++ standard versions to test.
* `STDLIBS` - list of C++ standard libraries to test (Clang only).
* `FMTLIBS` - list of formatting libraries to test.

You can pass those variables to `make` to tests only certain configurations, e.g. `make -f tests/Makefile COMPILERS=clang++ STDLIBS=libstdc++` to test only Clang with libstdc++.

Run `make -f tests/Makefile set-ide-target ...` to generate some VSC config files. This requires setting the variables above to uniquely specify just a single configuration.

What different files are used for:

* `sanity_check.cpp` - a minimal test program that's checked by the makefile to make sure our library is sane enough to run self-tests.
* `tests.cpp` - the main tests.
* `scratchpad.cpp` - a dummy file that can be used to test things during development.
* `test_programs/` - some of the larger test programs ran by `tests.cpp` are loaded from this directory.
* `build/` - the makefile puts the build results there.
  * `build/??/tmp[.exe]` is the last executable compiled by `tests.cpp`. Run it from here if the tests fail.
