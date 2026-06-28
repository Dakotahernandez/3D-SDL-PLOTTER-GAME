cmake_minimum_required(VERSION 3.15)
project(raytracer_sdl LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ---------------------------------------------------------------------------
# Core ray tracing library (SDL-free, embeddable in the game)
# ---------------------------------------------------------------------------
add_library(raytracer STATIC
    src/RayTracer.cpp
    src/Sphere.cpp
)

# Consumers include either <raytracer/RayTracer.h> or the flat headers.
target_include_directories(raytracer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/raytracer
)

# Renderer uses std::thread.
find_package(Threads REQUIRED)
target_link_libraries(raytracer PUBLIC Threads::Threads)

# Provide a namespaced alias so the game can do: target_link_libraries(game raytracer::raytracer)
add_library(raytracer::raytracer ALIAS raytracer)

# ---------------------------------------------------------------------------
# Optional SDL2 demo / driver (only built when SDL2 is available)
# ---------------------------------------------------------------------------
find_package(SDL2 QUIET)

if(SDL2_FOUND)
    add_executable(raytracer_demo examples/demo_main.cpp)
    target_link_libraries(raytracer_demo PRIVATE raytracer ${SDL2_LIBRARIES})
    target_include_directories(raytracer_demo PRIVATE ${SDL2_INCLUDE_DIRS})
    # Some SDL2 installs export a target instead of variables.
    if(TARGET SDL2::SDL2)
        target_link_libraries(raytracer_demo PRIVATE SDL2::SDL2)
    endif()
    message(STATUS "SDL2 found: building raytracer_demo")
else()
    message(STATUS "SDL2 not found: skipping raytracer_demo (core library still builds)")
endif()
