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

# Windows has no RPATH for the build tree (unlike Linux), so a shared build
# leaves liblibscale.dll in the build root while the test executable lives in
# tests/ — ctest then dies with 0xc0000135 (STATUS_DLL_NOT_FOUND). Place the
# library next to the executable after linking. Harmless (copies the static
# archive instead) for static builds.
if(WIN32)
    add_custom_command(TARGET libscale_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:libscale> $<TARGET_FILE_DIR:libscale_tests>)
endif()

add_test(NAME libscale_tests COMMAND libscale_tests)
