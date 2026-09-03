#include "coroutine/cpu_affinity.h"

#include <pthread.h>
#include <sched.h>
#include <unistd.h>

namespace coroutine {

std::vector<int> CpuAffinity::online_cpus() {
    const long count = ::sysconf(_SC_NPROCESSORS_ONLN);
    std::vector<int> result;
    for (long cpu = 0; cpu < count; ++cpu) {
        cpu_set_t set;
        CPU_ZERO(&set);
        if (::sched_getaffinity(0, sizeof(set), &set) == 0 && CPU_ISSET(static_cast<int>(cpu), &set)) {
            result.push_back(static_cast<int>(cpu));
        }
    }
    return result;
}

bool CpuAffinity::bind_current_thread(int cpu_id) noexcept {
    if (cpu_id < 0) return false;
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu_id, &set);
    return ::pthread_setaffinity_np(::pthread_self(), sizeof(set), &set) == 0;
}

} // namespace coroutine
