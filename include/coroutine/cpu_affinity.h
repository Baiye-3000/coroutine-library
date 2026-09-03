#pragma once

#include <vector>

namespace coroutine {

class CpuAffinity {
public:
    static std::vector<int> online_cpus();
    static bool bind_current_thread(int cpu_id) noexcept;
};

} // namespace coroutine
