set(CMAKE_CXX_CLANG_TIDY "")

find_package(GTest REQUIRED)

add_executable(scale_tests test_calculator.cpp test_device_parser.cpp test_sync.cpp)

target_link_libraries(scale_tests PRIVATE 
    libscale
    GTest::gtest_main
)

target_include_directories(scale_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_test(NAME calculator_test COMMAND scale_tests)

add_executable(scale_cli_smoke_test cli_smoke_test.cpp)
target_link_libraries(scale_cli_smoke_test PRIVATE libscale)
target_include_directories(scale_cli_smoke_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME cli_smoke_test COMMAND scale_cli_smoke_test)
