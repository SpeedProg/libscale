# Comfort Select Dashboard

A desktop app (built with C++ and GTKmm 4) to sync and display measurements from the Comfort Select Comfort Select scale.

## Getting Started

### Windows Driver Setup (Crucial!)
Because the scale uses a custom USB protocol, Windows by default might load a proprietary driver that blocks libusb.
To fix this:
1. Grab Zadig from https://zadig.akeo.ie/
2. Plug the scale into your PC.
3. Open Zadig, click `Options > List All Devices`
4. Pick the `SOEHNLE - Comfort Select` device.
5. Set the target driver to `WinUSB` and click **Replace Driver**.

The dashboard depends on `libscale`, which lives in [`libscale/`](libscale/README.md) and is
consumed as a local Conan package. Create it once per machine before installing the app.

### Quick Build (Linux)
```bash
conan profile detect --force   # once per machine
conan create libscale --build=missing -s build_type=Release
conan install . --build=missing -s build_type=Release
cmake --preset conan-release
cmake --build --preset conan-release
```

The binary is written to `build/Release/src/gui/comfort_select_dashboard`. Run the tests from
the build directory:

```bash
cd build/Release && ctest --output-on-failure
```

### Windows (MSYS2 MINGW64)

> [!WARNING]
> Due to C++ ABI incompatibility with GTKmm and sigc++, **compiling with MSVC is strictly unsupported.** You must use the MSYS2 UCRT64 (or MINGW64) GCC compiler for Windows builds.

```bash
pacman -S mingw-w64-x86_64-gtkmm4 mingw-w64-x86_64-libusb mingw-w64-x86_64-pkg-config
conan profile detect --force   # once per machine
conan create libscale --build=missing -s build_type=Release
conan install . --build=missing -s build_type=Release
cmake --preset conan-release
cmake --build --preset conan-release
```

## Features
* **Auto-Sync:** Auto-detects the scale over USB and syncs data immediately.
* **Local & Private:** Everything is stored offline in an SQLite database (permissions set to `chmod 600`).
* **Cairo Charts:** Nice visual progress line charts drawn with Cairo.
* **Export:** Dump your data to CSV or JSON whenever you want.
