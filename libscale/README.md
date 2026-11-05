# libscale

libscale reads measurement data from Comfort Select USB scales (libusb), stores it in SQLite
and computes body-composition metrics from raw bio-impedance values. It has no UI dependencies:
no GTK, no sigc++, no libusb types in its public headers.

## Features

- USB scale communication (libusb) with hotplug detection on a background thread.
- SQLite persistence behind repository interfaces (`IUserRepository`,
  `IMeasurementRepository`, `IDeviceRepository`).
- Bio-impedance calculations (body fat, water, muscle mass, energy needs, RFM, ECW) with
  pluggable formula versions.
- Asynchronous synchronization with cooperative cancellation (`std::jthread`).
- Strong units (`units::Kg`, `units::Cm`, ...) instead of bare numbers.
- Installable CMake package: `find_package(libscale)` / `libscale::libscale`.

## Layout

```
include/libscale/   public headers (no third-party includes)
src/libscale/       implementation, including the private SQLite serializers
tests/              unit tests (GoogleTest, in-memory fakes and SQLite)
cmake/              libscaleConfig.cmake.in package template
test_package/       Conan consumption test
tools/              documentation checks (Doxygen + Python)
```

## Requirements

- C++20 compiler (GCC 12+ / Clang 15+), CMake 3.20+, Conan 2.
- Dependencies: `sqlite3`, `sqlite_orm`, `spdlog`, `nlohmann_json`, `libusb` (all supplied
  through Conan; on Windows libusb comes from MSYS2).
- Conan profiles should use a C++20 standard (`compiler.cppstd=gnu20`). The project also sets
  `CMAKE_CXX_STANDARD 20` itself; a profile without `cppstd` only produces a cosmetic override
  warning.

## Quick start (Conan)

Create the package locally — this builds the library, runs the unit tests and validates the
package with the `test_package/` consumer:

```bash
conan profile detect --force   # once per machine
conan create . --build=missing -s build_type=Release
```

Skip tests if you only want the package:

```bash
conan create . --build=missing -s build_type=Release -c tools.build:skip_test=True
```

### Consuming it from another Conan project

```python
# conanfile.py of the consumer
def requirements(self):
    self.requires("libscale/1.0.0")
```

```cmake
find_package(libscale REQUIRED)
target_link_libraries(my_app PRIVATE libscale::libscale)
```

The recipe sets `cmake_file_name`/`cmake_target_name`, so Conan's `CMakeDeps` provides the
`libscale::libscale` target and resolves the transitive dependencies automatically.

## Local development (no package creation)

```bash
conan install . -s build_type=Release --build=missing
cmake --preset conan-release
cmake --build --preset conan-release
cd build/Release && ctest --output-on-failure
```

`-DLIBSCALE_BUILD_TESTS=OFF` skips the test targets.

## Installing without Conan

With the dependencies discoverable through CMake (system packages, `CMAKE_PREFIX_PATH`, or a
Conan toolchain file):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DLIBSCALE_BUILD_TESTS=OFF
cmake --build build
cmake --install build --prefix /path/to/prefix
```

The install includes the headers, the static or shared library, a license copy and a
relocatable CMake package:

```cmake
find_package(libscale REQUIRED)
target_link_libraries(my_app PRIVATE libscale::libscale)
```

`libusb` is resolved through its CMake package when available and falls back to
`pkg-config` metadata otherwise (the usual case for distributions and Flatpak). Use
`-DBUILD_SHARED_LIBS=ON` to build a versioned shared library instead of a static one.

## Tagging releases

Tag releases as `libscale-vX.Y.Z` and keep the recipe `version` in sync with
`project(libscale VERSION ...)` in `CMakeLists.txt`. Consumers can pin a release with:

```bash
git clone --branch libscale-v1.0.0 --depth 1 https://github.com/SpeedProg/libscale.git
```

### Publishing binaries later

The `conan create` flow above compiles the library once per machine and caches it. If rebuilds
become painful (many machines or CI jobs), add a binary remote — Artifactory CE, Cloudsmith or
`conan_server` — and upload from CI:

```bash
conan create . --build=missing -s build_type=Release
conan remote login <remote> <user>
conan upload "libscale/*" -r <remote> --confirm
```

Note that GitHub Packages does not host Conan packages; `ghcr.io` is an OCI registry and cannot
act as a Conan remote.

## Tests and quality checks

```bash
cmake --build build/Release --target format   # clang-format
cmake --build build/Release --target lint     # clang-tidy (-fix)
cmake --build build/Release --target check-docs
```

## License

MIT — see [LICENSE](LICENSE).
