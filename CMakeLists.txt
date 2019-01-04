cmake_minimum_required(VERSION 3.8.0)
project(grcb)

execute_process(COMMAND git submodule update --init -- ext/tomos WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set(TOMOS_LIB_ONLY ON CACHE BOOL "build Tomos as a library" FORCE)
add_subdirectory("ext/tomos")

add_executable(table1 "src/table1.cpp")
target_link_libraries(table1 tomos)

add_executable(table2 "src/table2.cpp")
target_link_libraries(table2 tomos)

add_executable(table3 "src/table3.cpp")
target_link_libraries(table3 tomos)

add_executable(table4 "src/table4.cpp")
target_link_libraries(table4 tomos)

