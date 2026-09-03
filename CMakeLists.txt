cmake_minimum_required(VERSION 3.22)

project(coroutine_runtime VERSION 0.1.0 LANGUAGES CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "coroutine_runtime requires a Linux target")
endif()

include(CTest)
find_package(Threads REQUIRED)

option(COROUTINE_BUILD_TESTS "Build coroutine runtime tests" ${BUILD_TESTING})
option(COROUTINE_BUILD_BENCHMARKS "Build Linux benchmark targets" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(coroutine_runtime STATIC
    src/config.cpp
    src/load_balancer.cpp
    src/metrics_collector.cpp
    src/cpu_affinity.cpp
    src/scheduler.cpp
    src/work_stealing_deque.cpp
    src/timer_queue.cpp
    src/event_loop.cpp
    src/fd_registry.cpp
    src/coroutine.cpp
    src/stack_allocator.cpp
    src/hook.cpp
    src/poll_hook.cpp
    src/socket_hooks.cpp
)
target_include_directories(coroutine_runtime PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)
target_link_libraries(coroutine_runtime PUBLIC Threads::Threads ${CMAKE_DL_LIBS})

if(COROUTINE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(COROUTINE_BUILD_BENCHMARKS)
    if(UNIX)
        add_subdirectory(benchmarks)
    else()
        message(WARNING "Benchmark targets require Linux and were skipped")
    endif()
endif()

message(STATUS "coroutine runtime M0 skeleton configured")
message(STATUS "Target platform: ${CMAKE_SYSTEM_NAME}")
