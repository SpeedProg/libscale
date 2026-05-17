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

### Quick Build (Linux)
```bash
mkdir build && cd build
conan install .. --build=missing
cmake .. -DCMAKE_TOOLCHAIN_FILE=build/generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

### Windows (MSYS2 MINGW64)
```bash
pacman -S mingw-w64-x86_64-gtkmm4
mkdir build && cd build
conan install .. --build=missing
cmake .. -G "MinGW Makefiles" -DCMAKE_TOOLCHAIN_FILE=build/generators/conan_toolchain.cmake
cmake --build .
```

## Features
* **Auto-Sync:** Auto-detects the scale over USB and syncs data immediately.
* **Local & Private:** Everything is stored offline in an SQLite database (permissions set to `chmod 600`).
* **Cairo Charts:** Nice visual progress line charts drawn with Cairo.
* **Export:** Dump your data to CSV or JSON whenever you want.
