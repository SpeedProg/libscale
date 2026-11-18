cmake_minimum_required(VERSION 3.20)
project(libscale VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

option(LIBSCALE_BUILD_TESTS "Build the libscale unit tests" ON)

# Performance: Link Time Optimization, for optimized SHARED builds only.
# Three exclusions, each found on a real toolchain:
#   * Debug builds: -g combined with LTO breaks the MinGW linker
#     ("lto1.exe: two or more sections for .gnu.lto__...").
#   * Windows (all linkages): ld's auto-export (no dllexport annotations here)
#     rejects the section-typed symbols LTO emits ("symbol wrong type").
#   * STATIC archives (all platforms): a consumer's ld auto-loads the LTO
#     plugin whenever it sees .gnu.lto_ sections, so the archive only links
#     with the exact GCC version that built it ("bytecode stream generated
#     with LTO version 15.1 instead of the expected 16.0"). Prebuilt archives
#     must link everywhere, so they ship without LTO bytecode. Shared
#     libraries are self-contained binaries and keep the optimization.
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
if(NOT ipo_supported)
    message(STATUS "Link-time optimization not supported by this toolchain: ${ipo_error}")
endif()
if(ipo_supported AND (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel"))
    if(NOT WIN32 AND BUILD_SHARED_LIBS)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
endif()

# Reproducible builds: strip paths and timestamps
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-ffile-prefix-map=${PROJECT_SOURCE_DIR}=.)
    add_compile_options(-Wno-builtin-macro-redefined)
    add_definitions(-D__DATE__="redacted")
    add_definitions(-D__TIME__="redacted")
endif()

# Clang-specific documentation syntax diagnostics (Method 3)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wdocumentation)
    message(STATUS "Clang detected: Enabling compiler-level -Wdocumentation diagnostics")
endif()

set(LIBSCALE_SOURCES
    src/libscale/calculator.cpp
    src/libscale/database_impl.cpp
    src/libscale/device.cpp
    src/libscale/exporter.cpp
    src/libscale/manager.cpp
    src/libscale/worker.cpp
)

# BUILD_SHARED_LIBS selects the library type; static is the default.
add_library(libscale ${LIBSCALE_SOURCES})
add_library(libscale::libscale ALIAS libscale)
set_target_properties(libscale PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR})

target_include_directories(libscale PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Find dependencies using standard CMake/Conan targets
find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(SqliteOrm REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)

# PkgConfig for system dependencies
find_package(PkgConfig REQUIRED)

find_package(libusb QUIET)
if(NOT TARGET libusb::libusb)
    pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)
    add_library(libusb::libusb ALIAS PkgConfig::LIBUSB)
endif()

# Every third-party dependency is an implementation detail: no public header includes one.
target_link_libraries(libscale
    PRIVATE
        Threads::Threads
        SQLite::SQLite3
        sqlite_orm::sqlite_orm
        nlohmann_json::nlohmann_json
        libusb::libusb
        spdlog::spdlog
)

# Install the library, its headers and a relocatable CMake package so that
# consumers can use find_package(libscale) without knowing the build tree.
install(TARGETS libscale
        EXPORT libscaleTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/libscale
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.hpp")

install(EXPORT libscaleTargets
        FILE libscaleTargets.cmake
        NAMESPACE libscale::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libscale)

include(CMakePackageConfigHelpers)
# Consumers of the shared library get private dependencies through the shared
# object, so the config file only looks them up for static builds.
if(BUILD_SHARED_LIBS)
    set(LIBSCALE_PACKAGE_SHARED TRUE)
else()
    set(LIBSCALE_PACKAGE_SHARED FALSE)
endif()
configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/libscaleConfig.cmake.in
    ${PROJECT_BINARY_DIR}/libscaleConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libscale)
write_basic_package_version_file(
    ${PROJECT_BINARY_DIR}/libscaleConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(FILES
        ${PROJECT_BINARY_DIR}/libscaleConfig.cmake
        ${PROJECT_BINARY_DIR}/libscaleConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libscale)

install(FILES ${PROJECT_SOURCE_DIR}/LICENSE
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/licenses/libscale)

if(LIBSCALE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# --- Developer tooling (format, lint, documentation) ---
# Only registered when libscale is the top-level project, so the targets do not
# clash with a parent project that builds libscale via add_subdirectory().
if(NOT CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    return()
endif()

find_program(CLANG_FORMAT_EXE NAMES "clang-format")
find_program(RUN_CLANG_TIDY_EXE NAMES "run-clang-tidy")
find_program(DOXYGEN_EXE NAMES "doxygen")
find_program(PYTHON_EXE NAMES "python3" "python")

file(GLOB_RECURSE LIBSCALE_ALL_SOURCE_FILES src/*.cpp tests/*.cpp)
file(GLOB_RECURSE LIBSCALE_ALL_HEADER_FILES src/*.hpp include/*.hpp tests/*.hpp)

if(CLANG_FORMAT_EXE)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i ${LIBSCALE_ALL_SOURCE_FILES} ${LIBSCALE_ALL_HEADER_FILES}
        COMMENT "Applying clang-format to libscale sources and headers..."
    )
endif()

if(RUN_CLANG_TIDY_EXE AND NOT WIN32)
    add_custom_target(lint
        COMMAND ${RUN_CLANG_TIDY_EXE} -fix -quiet -p ${CMAKE_BINARY_DIR}
                ${LIBSCALE_ALL_SOURCE_FILES}
        COMMENT "Running clang-tidy static analysis in parallel..."
    )
endif()

if(DOXYGEN_EXE)
    configure_file(${PROJECT_SOURCE_DIR}/tools/Doxyfile.in
                   ${PROJECT_BINARY_DIR}/Doxyfile @ONLY)
    add_custom_target(check-docs-doxygen
        COMMAND ${DOXYGEN_EXE} ${PROJECT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
        COMMENT "Running strict Doxygen documentation checks for libscale..."
    )
    add_custom_target(generate-docs
        COMMAND ${DOXYGEN_EXE} ${PROJECT_BINARY_DIR}/Doxyfile
        COMMAND ${CMAKE_COMMAND} -E echo
                "Documentation generated at: file://${PROJECT_BINARY_DIR}/docs/html/index.html"
        WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
        COMMENT "Generating interactive Doxygen HTML documentation..."
    )
endif()

if(PYTHON_EXE)
    if(TARGET check-docs-doxygen)
        add_custom_target(check-docs
            COMMAND ${PYTHON_EXE} -u ${PROJECT_SOURCE_DIR}/tools/check_documentation.py
                    ${PROJECT_SOURCE_DIR}/include/libscale
            DEPENDS check-docs-doxygen
            COMMENT "Verifying libscale documentation coverage (Python Regex + Doxygen)..."
        )
    else()
        add_custom_target(check-docs
            COMMAND ${PYTHON_EXE} -u ${PROJECT_SOURCE_DIR}/tools/check_documentation.py
                    ${PROJECT_SOURCE_DIR}/include/libscale
            COMMENT "Verifying libscale documentation coverage..."
        )
    endif()
endif()
