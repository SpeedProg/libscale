set(CMAKE_CXX_CLANG_TIDY "")

find_package(GTest REQUIRED)
find_package(spdlog REQUIRED)

# scale_tests covers the libscale core plus the GUI logic that only depends on
# core libraries (view model, settings), so it stays headless and hardware-free.
add_executable(scale_tests
    test_calculator.cpp
    test_dashboard_view_model.cpp
    test_database.cpp
    test_device_parser.cpp
    test_exporter.cpp
    test_settings.cpp
    test_sync.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/dashboard_view_model.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/settings.cpp
)

target_link_libraries(scale_tests PRIVATE 
    libscale
    GTest::gtest_main
    spdlog::spdlog
)

target_include_directories(scale_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_test(NAME scale_tests COMMAND scale_tests)

add_executable(scale_cli_smoke_test cli_smoke_test.cpp)
target_link_libraries(scale_cli_smoke_test PRIVATE libscale)
target_include_directories(scale_cli_smoke_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME cli_smoke_test COMMAND scale_cli_smoke_test)
