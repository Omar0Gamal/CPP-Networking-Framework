cmake_minimum_required(VERSION 3.20)
project(DarkFireNetworking VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DARKFIRE_BUILD_TESTS "Build DarkFire test suite" ON)
option(DARKFIRE_BUILD_EXAMPLES "Build standalone runnable examples" ON)

# -----------------------------------------------------------------------------
# DarkFire Core Library (Modern Target-Based Interface)
# -----------------------------------------------------------------------------
add_library(darkfire_net INTERFACE)
add_library(darkfire::net ALIAS darkfire_net)

target_include_directories(darkfire_net INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Libs/asio-1.28.1/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_definitions(darkfire_net INTERFACE
    ASIO_STANDALONE
)

if (WIN32)
    target_compile_definitions(darkfire_net INTERFACE
        _WIN32_WINNT=0x0A00
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_link_libraries(darkfire_net INTERFACE ws2_32)
else()
    target_link_libraries(darkfire_net INTERFACE pthread)
endif()

if (MSVC)
    target_compile_options(darkfire_net INTERFACE
        /W4
        /permissive-
        /Zc:preprocessor
        /bigobj
    )
else()
    target_compile_options(darkfire_net INTERFACE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# -----------------------------------------------------------------------------
# Test Suite
# -----------------------------------------------------------------------------
if (DARKFIRE_BUILD_TESTS)
    enable_testing()

    add_executable(darkfire_tests
        tests/test_main.cpp
        tests/test_serialization.cpp
        tests/test_lockfree_queue.cpp
        tests/test_security_crc.cpp
        tests/test_game_primitives.cpp
        tests/test_tcp_udp.cpp
    )

    target_link_libraries(darkfire_tests PRIVATE darkfire_net)
    add_test(NAME DarkFireTestSuite COMMAND darkfire_tests)
endif()

# -----------------------------------------------------------------------------
# Runnable Examples
# -----------------------------------------------------------------------------
if (DARKFIRE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
