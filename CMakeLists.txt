cmake_minimum_required(VERSION 3.28)

project(TextGame)

set(CMAKE_CXX_STANDARD 17)

set(CURSES_NEED_NCURSES TRUE)
set(CURSES_NEED_WIDE TRUE)
include(FindCurses)
find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIRS})

# -Wall
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(TextGame ${SOURCES})

target_link_libraries(TextGame ${CURSES_LIBRARIES} ncursesw)

