cmake_minimum_required(VERSION 3.28.3)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "make a different build directory in the project and use cmake .. in build directory")
endif()

project(cfs_scheduler)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra)

file(GLOB SOURCES CONFIGURE_DEPENDS
    "src/*.cpp"
)

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
