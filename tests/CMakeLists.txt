set(CMAKE_CXX_CLANG_TIDY "")

find_package(GTest REQUIRED)

add_executable(libscale_tests
    test_calculator.cpp
    test_database.cpp
    test_device_parser.cpp
    test_exporter.cpp
    test_sync.cpp
)

target_link_libraries(libscale_tests PRIVATE
    libscale::libscale
    GTest::gtest_main
)

add_test(NAME libscale_tests COMMAND libscale_tests)
