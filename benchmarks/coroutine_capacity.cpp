#include "coroutine/coroutine.h"

#include <chrono>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {
std::size_t peak_rss_kib() {
    std::ifstream status("/proc/self/status");
    std::string key;
    std::size_t value = 0;
    while (status >> key) {
        if (key == "VmHWM:") {
            status >> value;
            return value;
        }
        std::string ignored;
        std::getline(status, ignored);
    }
    return 0;
}
}

int main(int argc, char** argv) {
    const int count = argc > 1 ? std::atoi(argv[1]) : 100000;
    const bool pool = argc > 2 && std::string(argv[2]) == "pool";
    if (count <= 0) return EXIT_FAILURE;
    const auto started = std::chrono::steady_clock::now();
    constexpr int batch_size = 1000;
    int completed = 0;
    std::size_t maximum_rss = 0;
    for (int first = 0; first < count; first += batch_size) {
        const int batch_count = std::min(batch_size, count - first);
        std::vector<std::unique_ptr<coroutine::Coroutine>> coroutines;
        coroutines.reserve(static_cast<std::size_t>(batch_count));
        for (int index = 0; index < batch_count; ++index) {
            coroutine::StackAllocator::Options options{16 * 1024, 64 * 1024, true, pool};
            coroutines.push_back(std::make_unique<coroutine::Coroutine>([&] { ++completed; }, options));
        }
        maximum_rss = std::max(maximum_rss, peak_rss_kib());
        for (auto& coroutine : coroutines) coroutine->resume();
    }
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    std::printf("strategy=%s coroutines=%d completed=%d live_batch=%d peak_rss_kib=%zu seconds=%.6f\n",
                pool ? "guard-pool" : "guard-dynamic", count, completed, batch_size, maximum_rss, seconds);
    return completed == count ? EXIT_SUCCESS : EXIT_FAILURE;
}
