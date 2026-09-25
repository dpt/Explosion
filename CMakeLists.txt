cmake_minimum_required(VERSION 3.10)
project(Explosions C)

set(CMAKE_C_STANDARD 99)

# The particle engine: no SDL, so other projects can pull it in via
# FetchContent/add_subdirectory and link the `explosion` target.
add_library(explosion STATIC
        src/explosion.c
        src/explosion.h)
target_include_directories(explosion PUBLIC src)
target_compile_options(explosion PRIVATE -Wall -Wextra -Wpedantic)
if(NOT MSVC)
    target_link_libraries(explosion PUBLIC m)
endif()

# The SDL playground app: only when this is the top-level project.
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    find_package(SDL3 REQUIRED)

    add_executable(Explosions
            src/gradient.c
            src/gradient.h
            src/random-pool.c
            src/random-pool.h
            src/main.c)

    target_compile_options(Explosions PRIVATE -Wall -Wextra -Wpedantic)

    target_link_libraries(Explosions explosion SDL3::SDL3)
endif()
