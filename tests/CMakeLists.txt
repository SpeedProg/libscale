set(CMAKE_CXX_CLANG_TIDY "")

find_package(GTest REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)

# The dashboard view model uses sigc++ signals; libscale itself no longer depends on sigc++.
find_package(PkgConfig REQUIRED)
pkg_check_modules(SIGCPP REQUIRED sigc++-3.0)

# gui_tests covers the GUI logic that only depends on core libraries (view model,
# settings), so it stays headless and hardware-free.
add_executable(gui_tests
    test_dashboard_view_model.cpp
    test_settings.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/dashboard_view_model.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/settings.cpp
)

target_link_libraries(gui_tests PRIVATE 
    libscale::libscale
    GTest::gtest_main
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    ${SIGCPP_LIBRARIES}
)

target_include_directories(gui_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${SIGCPP_INCLUDE_DIRS}
)

add_test(NAME gui_tests COMMAND gui_tests)

add_executable(scale_cli_smoke_test cli_smoke_test.cpp)
target_link_libraries(scale_cli_smoke_test PRIVATE libscale::libscale)
target_include_directories(scale_cli_smoke_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME cli_smoke_test COMMAND scale_cli_smoke_test)
