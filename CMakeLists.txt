cmake_minimum_required(VERSION 3.29)
project(do_editor_first)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

add_subdirectory(vendored/SDL EXCLUDE_FROM_ALL)

add_executable(do_editor_first main.cpp)

set_target_properties(do_editor_first PROPERTIES WIN32_EXECUTABLE FALSE)
target_link_libraries(do_editor_first PRIVATE SDL3::SDL3)

