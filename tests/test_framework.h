#pragma once

#include <cstdlib>
#include <iostream>
#include <string>

namespace test {

inline int failures = 0;

inline void expect(bool condition, const char* expression, const char* file, int line) {
    if (!condition) {
        std::cerr << file << ':' << line << ": expectation failed: " << expression << '\n';
        ++failures;
    }
}

inline int finish(const std::string& name) {
    if (failures == 0) {
        std::cout << "PASS " << name << '\n';
        return EXIT_SUCCESS;
    }
    std::cerr << "FAIL " << name << ": " << failures << " failure(s)\n";
    return EXIT_FAILURE;
}

} // namespace test

#define EXPECT_TRUE(expression) \
    ::test::expect((expression), #expression, __FILE__, __LINE__)
