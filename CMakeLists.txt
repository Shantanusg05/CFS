cmake_minimum_required(VERSION 3.28.3)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "make a different build directory in the project and use cmake .. in build directory")
endif()

project(cfs_schedular)

# CHANGED — both lines below were silent no-ops in the original file:
#   set(CMAKE_CXX_STANDARD, 14)       CMake does not treat commas as
#                                      argument separators (only whitespace/
#                                      semicolons), so this actually set a
#                                      variable literally named
#                                      "CMAKE_CXX_STANDARD," (with a trailing
#                                      comma) to "14" — not what was intended.
#   set(CMAKE_CXX_STANDARD_REQUIRED)  This sets the variable to an empty
#                                      value, not ON, so it had no effect.
# Net effect: the compiler standard flag was never actually being passed.
# It happened to still compile because the compiler's default standard was
# new enough, but the project wasn't getting what it asked for.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra)

file(GLOB SOURCES
    "src/*.cpp"
)

# CHANGED: split into a library (scheduler_core) + a thin main.cpp
# executable, instead of compiling everything (including main.cpp) into one
# target. This lets the test executable link against the same scheduler
# code without pulling in (and conflicting with) main()'s entry point.
add_library(scheduler_core ${SOURCES})
target_include_directories(scheduler_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(cfs_scheduler main.cpp)
target_link_libraries(cfs_scheduler PRIVATE scheduler_core)
target_include_directories(cfs_scheduler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.4
    )
    FetchContent_MakeAvailable(Catch2)

    add_executable(scheduler_tests tests/test_scheduler.cpp)
    target_link_libraries(scheduler_tests PRIVATE scheduler_core Catch2::Catch2WithMain)
    target_include_directories(scheduler_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    include(CTest)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    include(Catch)
    catch_discover_tests(scheduler_tests)
endif()
