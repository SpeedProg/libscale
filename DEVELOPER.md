# Developer Guide

Quick notes on how to set up the dev environment, compile, and run tests.

## Dev Environment
The easiest way to work on this is using the VS Code DevContainer. It already has GTK4, libusb, and everything else installed.

If you want to build on your host system manually, you'll need:
* C++20 compiler (GCC 12+ or Clang 15+)
* CMake 3.23+
* Conan 2.0+
* Development libs: `libusb-1.0-dev`, `libgtkmm-4.0-dev`, `libudev-dev`
* Quality tools: `clang-tidy`, `clang-format`

## Building the Code

We use Conan 2 for dependencies and CMake presets.

### 1. Grab Dependencies
```bash
conan install . --build=missing
```

### 2. Compile
```bash
cmake --preset conan-release
cmake --build build/build/Release
```
The compiled binary goes to `build/build/Release/comfort_select_dashboard`.

## Tests & Quality checks

### Unit Tests
Written with GoogleTest to check the calculator logic.
```bash
cd build/build/Release
ctest --output-on-failure
```

### Formatting & Linting
To run the auto-formatter:
```bash
cmake --build build/build/Release --target format
```
To run static analysis (clang-tidy):
```bash
cmake --build build/build/Release --target lint
```

### Fuzzing
To test the USB packet parser against corrupt data:
```bash
cmake --preset conan-release -DBUILD_FUZZERS=ON
cmake --build build/build/Release --target libscale_fuzzer
./build/build/Release/libscale_fuzzer
```

## Privacy & Security
* Never use raw pointer ownership. Use `std::unique_ptr` or `std::shared_ptr`.
* The SQLite DB must always be kept private (`chmod 600`).
