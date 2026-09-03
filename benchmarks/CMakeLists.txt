if(NOT UNIX)
    return()
endif()

add_executable(baseline_server baseline_server.cpp)
target_compile_options(baseline_server PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(baseline_server PRIVATE pthread)

add_executable(scheduler_benchmark scheduler_benchmark.cpp)
target_compile_options(scheduler_benchmark PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(scheduler_benchmark PRIVATE coroutine_runtime)
