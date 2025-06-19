cmake_minimum_required(VERSION 3.30)

project(gitkf)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include("thirdparty/CMakeRC.cmake")

include_directories(
    .
)

add_subdirectory(src)
add_subdirectory(thirdparty)

set_target_properties(gitkf PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")