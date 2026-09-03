if(NOT UNIX)
    return()
endif()

add_executable(baseline_server baseline_server.cpp)
target_compile_options(baseline_server PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(baseline_server PRIVATE pthread)
