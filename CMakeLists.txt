cmake_minimum_required(VERSION 3.6.2)
project(lvgl_test C CXX)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(SDL REQUIRED)
find_package(SDL_ttf REQUIRED)

add_compile_options(-Wall -Wextra -Wpedantic -Wno-psabi -g)

add_subdirectory(external/lvgl)

add_executable(lvgl_test src/main.cpp)
set_property(TARGET lvgl_test PROPERTY CXX_STANDARD 17)

target_include_directories(lvgl PRIVATE include)
target_link_libraries(lvgl_test PRIVATE lvgl)

target_link_libraries(lvgl_test PUBLIC ${SDL_LIBRARY})
target_include_directories(lvgl_test PUBLIC ${SDL_INCLUDE_DIR})
target_include_directories(lvgl_test PUBLIC -lts)

