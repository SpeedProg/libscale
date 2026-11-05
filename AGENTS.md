# AGENTS.md

C++20 desktop app: GTKmm 4 GUI (`comfort_select_dashboard`) on top of `libscale`, which talks to a
Comfort Select USB scale via libusb and stores data in SQLite. Conan 2 supplies most libraries;
GTKmm 4 / sigc++-3.0 / fontconfig come from system pkg-config.

`libscale/` is a self-contained subproject (own CMake project, recipe, tests, README) prepared for
a split into its own GitHub repository. The dashboard consumes it as a local Conan package
(`conan create libscale` → `requires("libscale/1.0.0")` → `find_package(libscale)`).

## Build & verify (commands verified on Linux)

```bash
conan profile detect --force                                    # first time only
conan create libscale --build=missing -s build_type=Release      # builds libscale + its tests + test_package
conan install . --build=missing -s build_type=Release            # dashboard deps (see note)
cmake --preset conan-release
cmake --build --preset conan-release                             # binary: build/Release/src/gui/comfort_select_dashboard
cd build/Release && ctest --output-on-failure                    # gui_tests + cli_smoke_test
```

- `CMakeUserPresets.json` hardcodes `build/Release/generators/CMakePresets.json`, so keep
  `-s build_type=Release` (Conan's default profile is Release) or `--preset conan-release` disappears.
  Re-run `conan create libscale` after changing library code, otherwise the app builds against the
  cached older package.
- Library-only work (no GTKmm required): `cd libscale && conan install . -s build_type=Release
  --build=missing && cmake --preset conan-release && cmake --build --preset conan-release`, then
  `cd build/Release && ctest`. Full library docs live in `libscale/README.md`.
- `cmake --install libscale/build/Release --prefix <dir>` installs the library with headers and a
  CMake package (`find_package(libscale)`, target `libscale::libscale`); the recipe sets the same
  target name for Conan consumers. Add `-DBUILD_SHARED_LIBS=ON` for a versioned shared library.
- Single gtest case: `./build/Release/tests/gui_tests --gtest_filter='DashboardViewModelTest.*'`
  (library cases: `libscale/build/Release/tests/libscale_tests --gtest_filter='CalculatorTest.*'`).
  Tests use in-memory fakes — no scale hardware or display required.
- Debian/Ubuntu deps: `libgtkmm-4.0-dev libusb-1.0-0-dev libudev-dev pkg-config` (what CI installs).
- Helper targets (created only when the tool is installed): `format` (clang-format), `lint`
  (run-clang-tidy, **runs with `-fix` and rewrites sources**), `check-docs`, `generate-docs`.
- `DEVELOPER.md` also documents a DevContainer / `uv run conan` workflow.

## Conventions & gotchas

- `.clang-tidy` is strict (`WarningsAsErrors: '*'`): CamelCase classes; `lower_case`
  functions/variables/namespaces. Suppressions use `// NOLINT(check-name): reason`.
- Configure/build only through the Conan presets — no bare `cmake ..` / `make` invocations.
- No raw owning pointers: use `std::unique_ptr` / `std::shared_ptr`.
- Physical metrics use the strong types in `libscale/include/libscale/units.hpp` (`units::Cm`,
  `units::Kg`, ...), not bare numbers.
- Commit messages: capitalized imperative mood, no `feat:` / `fix:` prefixes (see `git log`).
- Library public API in `libscale/include/libscale` needs Doxygen comments on classes/structs/enums
  and public (non-`override`) methods; `libscale/tools/check_documentation.py` enforces this. The
  GUI headers are checked the same way via `tools/check_documentation.py` (`include/gui`).
- `check-docs`/`generate-docs`/`format`/`lint` exist in both projects (registered only when
  libscale is configured standalone) and are not covered by CI — verify locally.
- Source lists are explicit: add new `.cpp` files to `libscale/CMakeLists.txt` (`LIBSCALE_SOURCES`),
  `libscale/tests/CMakeLists.txt`, `src/gui/CMakeLists.txt`, or `tests/CMakeLists.txt` (the
  `file(GLOB ...)` calls only feed `format`/`lint`).
- CI (`.github/workflows/build.yml`, mirrored in `.forgejo/workflows/build.yml`) builds, packages and
  runs ctest on Linux — it never runs lint or docs. Verify those locally.
- `DEVELOPER.md`'s fuzzing section is stale: no `BUILD_FUZZERS` option or `libscale_fuzzer` target
  exists.

## Windows

- Root CMake rejects MSVC with a `FATAL_ERROR` (GTKmm/sigc++ C++ ABI). Use MSYS2 UCRT64 GCC:
  `pacman -S mingw-w64-x86_64-gtkmm4`; see the Windows CI job for the Conan profile (GCC + Ninja).
- The scale needs a one-time Zadig WinUSB driver replacement (README).

## Architecture

- `libscale/include/libscale` + `libscale/src/libscale` → the `libscale` library: `HealthManager`
  orchestrates `IDatabaseManager` (sqlite_orm/SQLite), `IScaleDevice` (libusb + hotplug thread),
  `IBioImpedanceCalculator`, and `SyncWorker` (`std::jthread`, cooperative cancellation).
  Empirical formula constants live in the private header `libscale/src/libscale/formula_config.hpp`;
  SQLite serializers and the `libscaleConfig.cmake.in` package template also live under
  `libscale/src` and `libscale/cmake`. Public headers include no third-party dependency (no
  libusb/sigc++/GTKmm), so the library installs and is consumed on its own.
  The recipe `libscale/conanfile.py` carries the version (currently `1.0.0`) and must stay in sync
  with `project(libscale VERSION ...)` in `libscale/CMakeLists.txt` and the dashboard's
  `requires("libscale/<version>")`.
- `include/gui` + `src/gui` → GTKmm app. `MainWindow` binds widgets by ID from `ui/main_window.ui`
  (Gtk::Builder); range/data logic is in `DashboardViewModel` (MVVM); charts are custom Cairo
  (`graph.cpp`).
- GTK is not thread-safe: background sync results must be bridged to the main loop via
  `Glib::Dispatcher` (documented in `worker.hpp`), never by touching widgets from the worker.
- UI/CSS are runtime files, not compiled resources: `find_resource()` searches cwd, `../ui`, etc.;
  CMake copies `ui/main_window.ui` and `src/gui/style.css` into the build tree at configure time.
- App state: `~/.local/share/libscale/` — `data.db` (kept `chmod 600`), `settings.json`,
  `app.log`. Single instance enforced by GApplication ID `de.speedprog.comfort-select-dashboard`,
  which matches the Flatpak, desktop entry and AppStream identifiers.
- Flatpak manifest `de.speedprog.comfort-select-dashboard.yml` pins all dependencies itself and is
  built in CI. It builds `libscale/` as its own cmake-ninja module (no Conan inside the sandbox);
  after the repository split that module becomes a pinned `git` source. `libscaleConfig.cmake`
  resolves libusb through its CMake package or falls back to pkg-config.
