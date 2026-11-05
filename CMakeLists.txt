cmake_minimum_required(VERSION 3.20)
project(ComfortSelectDashboard VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

# Windows: Omit non-essential API headers (like legacy Winsock 1) to prevent conflicts with Winsock 2
if(WIN32)
    if(MSVC)
        message(FATAL_ERROR "MSVC is strictly unsupported for Windows builds due to GTKmm C++ ABI incompatibility. You are currently using the MSVC compiler (cl.exe). Please use the MSYS2 UCRT64 (or MINGW64) GCC compiler. Ensure your Conan profile uses GCC and your CMake generator is set to 'Ninja' or 'Unix Makefiles'. See README.md and DEVELOPER.md for instructions.")
    endif()
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
endif()


# Performance: Link Time Optimization
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT error)
if(ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# Reproducible Builds: Strip paths and timestamps
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-ffile-prefix-map=${CMAKE_SOURCE_DIR}=.)
    add_compile_options(-Wno-builtin-macro-redefined)
    add_definitions(-D__DATE__="redacted")
    add_definitions(-D__TIME__="redacted")
endif()

# Clang-specific documentation syntax diagnostics (Method 3)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wdocumentation)
    message(STATUS "Clang detected: Enabling compiler-level -Wdocumentation diagnostics")
endif()

# Clang-Tidy & Clang-Format integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
find_program(RUN_CLANG_TIDY_EXE NAMES "run-clang-tidy")
find_program(CLANG_FORMAT_EXE NAMES "clang-format")


file(GLOB_RECURSE ALL_SOURCE_FILES 
    src/*.cpp 
    tests/*.cpp
)

file(GLOB_RECURSE ALL_HEADER_FILES
    src/*.hpp
    include/*.hpp
    tests/*.hpp
)

if(CLANG_FORMAT_EXE)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i ${ALL_SOURCE_FILES} ${ALL_HEADER_FILES}
        COMMENT "Applying clang-format to all source and header files..."
    )
endif()

if(RUN_CLANG_TIDY_EXE AND NOT WIN32)
    add_custom_target(lint
        COMMAND ${RUN_CLANG_TIDY_EXE} -fix -quiet -p ${CMAKE_BINARY_DIR} ${ALL_SOURCE_FILES}
        COMMENT "Running clang-tidy static analysis in parallel..."
    )
endif()

# Custom target for Doxygen-based strict warning verification (Method 1)
find_program(DOXYGEN_EXE NAMES "doxygen")
if(DOXYGEN_EXE)
    add_custom_target(check-docs-doxygen
        COMMAND ${DOXYGEN_EXE} ${CMAKE_SOURCE_DIR}/tools/Doxyfile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running strict Doxygen documentation checks..."
    )
    add_custom_target(generate-docs
        COMMAND ${DOXYGEN_EXE} ${CMAKE_SOURCE_DIR}/tools/Doxyfile
        COMMAND ${CMAKE_COMMAND} -E echo "Documentation generated successfully! View HTML at: file://${CMAKE_BINARY_DIR}/docs/html/index.html"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating interactive Doxygen HTML documentation..."
    )
endif()

# Global check-docs target combining Python regex and Doxygen verifiers
find_program(PYTHON_EXE NAMES "python3" "python")
if(PYTHON_EXE)
    if(TARGET check-docs-doxygen)
        add_custom_target(check-docs
            COMMAND ${PYTHON_EXE} -u ${CMAKE_SOURCE_DIR}/tools/check_documentation.py
            DEPENDS check-docs-doxygen
            COMMENT "Verifying documentation coverage (Python Regex + Doxygen)..."
        )
    else()
        add_custom_target(check-docs
            COMMAND ${PYTHON_EXE} -u ${CMAKE_SOURCE_DIR}/tools/check_documentation.py
            COMMENT "Verifying Doxygen documentation coverage..."
        )
    endif()
endif()

# Find GTKmm 4.0 via PkgConfig
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED gtkmm-4.0 fontconfig)

# Build libscale (Core)
add_subdirectory(src/libscale)

# Build GUI app
add_subdirectory(src/gui)

# Build Tests
enable_testing()
add_subdirectory(tests)

# Install Desktop Launcher
install(FILES de.speedprog.comfort-select-dashboard.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)

# Install Scalable SVG Icon
install(FILES resources/de.speedprog.comfort-select-dashboard.svg
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps)

# Install AppStream Metainfo
install(FILES resources/de.speedprog.comfort-select-dashboard.metainfo.xml
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo)
