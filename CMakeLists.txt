cmake_minimum_required(VERSION 4.2)
project(Explosions C)

set(CMAKE_C_STANDARD 11)

find_package(SDL3 REQUIRED)

add_executable(Explosions
        src/explosion.c)

target_compile_options(Explosions PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(Explosions SDL3::SDL3)