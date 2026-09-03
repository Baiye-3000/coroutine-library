add_executable(config_test
    unit/config_test.cpp
)
target_link_libraries(config_test PRIVATE coroutine_runtime)
target_compile_options(config_test PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
add_test(NAME config_test COMMAND config_test)

add_executable(load_balancer_test unit/load_balancer_test.cpp)
target_link_libraries(load_balancer_test PRIVATE coroutine_runtime)
target_compile_options(load_balancer_test PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
add_test(NAME load_balancer_test COMMAND load_balancer_test)

add_executable(metrics_test unit/metrics_test.cpp)
target_link_libraries(metrics_test PRIVATE coroutine_runtime)
target_compile_options(metrics_test PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
add_test(NAME metrics_test COMMAND metrics_test)

add_executable(deque_test unit/deque_test.cpp)
target_link_libraries(deque_test PRIVATE coroutine_runtime)
target_compile_options(deque_test PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)
add_test(NAME deque_test COMMAND deque_test)

add_executable(scheduler_test integration/scheduler_test.cpp)
target_link_libraries(scheduler_test PRIVATE coroutine_runtime)
target_compile_options(scheduler_test PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)
add_test(NAME scheduler_test COMMAND scheduler_test)

add_executable(scheduler_stress stress/scheduler_stress.cpp)
target_link_libraries(scheduler_stress PRIVATE coroutine_runtime)
target_compile_options(scheduler_stress PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)
add_test(NAME scheduler_stress COMMAND scheduler_stress)

add_executable(timer_test unit/timer_test.cpp)
target_link_libraries(timer_test PRIVATE coroutine_runtime)
target_compile_options(timer_test PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>)
add_test(NAME timer_test COMMAND timer_test)

add_executable(fd_registry_test unit/fd_registry_test.cpp)
target_link_libraries(fd_registry_test PRIVATE coroutine_runtime)
target_compile_options(fd_registry_test PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>)
add_test(NAME fd_registry_test COMMAND fd_registry_test)

add_executable(coroutine_test unit/coroutine_test.cpp)
target_link_libraries(coroutine_test PRIVATE coroutine_runtime)
target_compile_options(coroutine_test PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>)
add_test(NAME coroutine_test COMMAND coroutine_test)

add_executable(hook_test integration/hook_test.cpp)
target_link_libraries(hook_test PRIVATE coroutine_runtime)
target_compile_options(hook_test PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>)
add_test(NAME hook_test COMMAND hook_test)

add_executable(poll_hook_test integration/poll_hook_test.cpp)
target_link_libraries(poll_hook_test PRIVATE coroutine_runtime)
target_compile_options(poll_hook_test PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>)
add_test(NAME poll_hook_test COMMAND poll_hook_test)
