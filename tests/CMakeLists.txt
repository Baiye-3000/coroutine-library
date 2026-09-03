add_executable(config_test
    unit/config_test.cpp
)
target_link_libraries(config_test PRIVATE coroutine_runtime)
target_compile_options(config_test PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
add_test(NAME config_test COMMAND config_test)
