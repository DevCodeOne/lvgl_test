cmake_minimum_required(VERSION 3.6.2)
project(lvgl_test C CXX)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_compile_options(-Wall -Wextra -Wpedantic -Wno-psabi -g)

add_subdirectory(external/lvgl)

add_executable(lvgl_test src/main.cpp)
set_property(TARGET lvgl_test PROPERTY CXX_STANDARD 17)

target_include_directories(lvgl PRIVATE include)
target_link_libraries(lvgl_test PUBLIC lvgl)
target_link_libraries(lvgl_test PUBLIC ts)

