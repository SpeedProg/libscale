set(LIBSCALE_SOURCES
    calculator.cpp
    device.cpp
    manager.cpp
    database_impl.cpp
    exporter.cpp
    worker.cpp
)

add_library(libscale STATIC ${LIBSCALE_SOURCES})

target_include_directories(libscale PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Find dependencies using standard CMake/Conan targets
find_package(SQLite3 REQUIRED)
find_package(SqliteOrm REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)

# PkgConfig for system dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(SIGCPP REQUIRED sigc++-3.0)

find_package(libusb QUIET)
if(NOT TARGET libusb::libusb)
    pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)
    add_library(libusb::libusb ALIAS PkgConfig::LIBUSB)
endif()

target_link_libraries(libscale
    PUBLIC
        libusb::libusb
        SQLite::SQLite3
        sqlite_orm::sqlite_orm
        nlohmann_json::nlohmann_json
        ${SIGCPP_LIBRARIES}
    PRIVATE
        spdlog::spdlog
)

target_include_directories(libscale SYSTEM PUBLIC
    ${SIGCPP_INCLUDE_DIRS}
)

# Install libscale static library
install(TARGETS libscale
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
