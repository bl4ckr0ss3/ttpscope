cmake_minimum_required(VERSION 3.20)
project(ttpscope VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(GNUInstallDirs)
set(CURSES_NEED_WIDE TRUE)
find_package(Curses REQUIRED)

add_library(ttpscope_core
    src/json.c
    src/model.c
    src/paths.c
    src/rich.c
    src/render.c
    src/stix.c
    src/sync.c
    src/notes.c
)
target_include_directories(ttpscope_core PUBLIC include PRIVATE ${CURSES_INCLUDE_DIRS})
target_compile_definitions(ttpscope_core PRIVATE _POSIX_C_SOURCE=200809L)
target_compile_options(ttpscope_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)

add_executable(ttpscope src/main.c src/tui.c)
target_include_directories(ttpscope PRIVATE ${CURSES_INCLUDE_DIRS})
target_compile_definitions(ttpscope PRIVATE TTPSCOPE_VERSION="${PROJECT_VERSION}" _POSIX_C_SOURCE=200809L)
target_compile_options(ttpscope PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
target_link_libraries(ttpscope PRIVATE ttpscope_core ${CURSES_LIBRARIES})

install(TARGETS ttpscope RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES docs/ttpscope.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

include(CTest)
if(BUILD_TESTING)
    add_executable(test_json tests/test_json.c)
    target_link_libraries(test_json PRIVATE ttpscope_core)
    add_test(NAME json COMMAND test_json)

    add_executable(test_stix tests/test_stix.c)
    target_compile_definitions(test_stix PRIVATE FIXTURE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/minimal.json")
    target_link_libraries(test_stix PRIVATE ttpscope_core)
    add_test(NAME stix COMMAND test_stix)

    add_executable(test_render tests/test_render.c)
    target_link_libraries(test_render PRIVATE ttpscope_core)
    add_test(NAME render COMMAND test_render)
endif()
