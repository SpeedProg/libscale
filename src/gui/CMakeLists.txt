set(GUI_SOURCES
    main.cpp
    main_window.cpp
    graph.cpp
    settings.cpp
    history_row.cpp
    dashboard_view_model.cpp
)

add_executable(comfort_select_dashboard ${GUI_SOURCES})

target_include_directories(comfort_select_dashboard PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Find spdlog and its dependencies (like fmt)
find_package(spdlog REQUIRED)

target_link_libraries(comfort_select_dashboard PRIVATE 
    libscale
    ${GTKMM_LIBRARIES}
    spdlog::spdlog
)

# GTKmm include dirs are already in GTKMM_LIBRARIES if using pkg_check_modules with IMPORTED_TARGET
# but root CMakeLists.txt didn't use IMPORTED_TARGET for GTKMM.
target_include_directories(comfort_select_dashboard SYSTEM PRIVATE ${GTKMM_INCLUDE_DIRS})

# Install UI and CSS files
configure_file(${CMAKE_SOURCE_DIR}/ui/main_window.ui ${CMAKE_BINARY_DIR}/ui/main_window.ui COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/style.css ${CMAKE_BINARY_DIR}/src/gui/style.css COPYONLY)

# Install executable target
install(TARGETS comfort_select_dashboard
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Install runtime UI and CSS styling resources
install(FILES ${CMAKE_SOURCE_DIR}/ui/main_window.ui
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/de.speedprog.comfort-select-dashboard)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/style.css
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/de.speedprog.comfort-select-dashboard)
