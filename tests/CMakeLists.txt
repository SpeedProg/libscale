set(CMAKE_CXX_CLANG_TIDY "")

find_package(GTest REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(SqliteOrm REQUIRED)
find_package(Threads REQUIRED)

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
    nlohmann_json::nlohmann_json
    SQLite::SQLite3
    sqlite_orm::sqlite_orm
    Threads::Threads
)


add_test(NAME libscale_tests COMMAND libscale_tests)
