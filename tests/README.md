# Test Notes

## Overview

Aside from simple unit testing, we are verifying things work as expected for both C & C++. We are also testing across
MSVC, GCC, and Clang with compiler optimizations both off and on.

We are using the "pl_test.h" library we created!

A lot of tests need to be added!

## Libraries
The "stb-style" libraries found in the "libs" folder are expected to work for both C and C++ standalone from the rest
of Pilot Light. The tests for these are found in the header with the library name and "_tests" appended to it. They are
being run in a simple executable with the entry points being found in "main_lib_tests.c (or .cpp)".

## Extensions
The extensions are being tested in the Pilot Light application created from "app_tests.c (or .cpp)". These are using the
"null" backend which currently just supports basic operation at the moment (meant to be run without windows or graphics).

## Running Tests

### Windows
```bash
cd tests
build_win32.bat

cd ../out
pilot_light_c.exe
pilot_light_cpp.exe
pilot_light.exe -a tests_c
pilot_light.exe -a tests_cpp
```

### MacOS
```bash
cd tests
chmod +x build_macos.sh
./build_macos.sh

cd ../out
./pilot_light_c
./pilot_light_cpp
./pilot_light -a tests_c
./pilot_light -a tests_cpp
```
### Linux
```bash
cd tests
chmod +x build_linux.sh
./build_linux.sh

cd ../out
./pilot_light_c
./pilot_light_cpp
./pilot_light -a tests_c
./pilot_light -a tests_cpp
```