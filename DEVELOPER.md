# Developer Guide

Quick notes on how to set up the dev environment, compile, and run tests.

## Dev Environment
The easiest way to work on this is using the VS Code DevContainer. It already has GTK4, libusb, and everything else installed.

If you want to build on your host system manually, you'll need:
* C++20 compiler (GCC 12+ or Clang 15+). **On Windows**, you must use the MSYS2 UCRT64 (or MINGW64) GCC compiler. MSVC is strictly not supported due to GTKmm C++ ABI incompatibility.
* CMake 3.23+
* Conan 2.0+
* Development libs: `libusb-1.0-dev`, `libgtkmm-4.0-dev`, `libudev-dev`
* Quality tools: `clang-tidy`, `clang-format`

## Building the Code

We use Conan 2 for dependencies and CMake presets.

### 1. Grab Dependencies
Conan is installed as a development dependency inside the workspace's Python virtual environment (`.venv`).

If this is your first time using Conan on this system, you must initialize the default Conan profile first:
```bash
uv run conan profile detect --force
```

Then, you can run the install command directly using `uv run`:
```bash
uv run conan install . --build=missing
```
Alternatively, you can activate the virtual environment and run `conan`:
```bash
source .venv/bin/activate
conan install . --build=missing
```

### 2. Compile
```bash
cmake --preset conan-release
cmake --build build/Release
```
The compiled binary goes to `build/Release/src/gui/comfort_select_dashboard`.

## Tests & Quality checks

### Unit Tests
Written with GoogleTest to check the calculator logic.
```bash
cd build/Release
ctest --output-on-failure
```

### Formatting & Linting
To run the auto-formatter:
```bash
cmake --build build/Release --target format
```
To run static analysis (clang-tidy):
```bash
cmake --build build/Release --target lint
```

### Fuzzing
To test the USB packet parser against corrupt data:
```bash
cmake --preset conan-release -DBUILD_FUZZERS=ON
cmake --build build/Release --target libscale_fuzzer
./build/Release/libscale_fuzzer
```

## Privacy & Security
* Never use raw pointer ownership. Use `std::unique_ptr` or `std::shared_ptr`.
* The SQLite DB must always be kept private (`chmod 600`).
