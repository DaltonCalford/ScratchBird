#pragma once

#ifdef SCRATCHBIRD_FAULT_INJECT
#include <atomic>
#include <string>
#include <unordered_map>

namespace scratchbird {
namespace chaos {

class FaultInjector {
public:
    static FaultInjector& instance() {
        static FaultInjector inst;
        return inst;
    }

    void set(const std::string& key, int trigger_countdown) {
        flags_[key].store(trigger_countdown, std::memory_order_relaxed);
    }

    bool should_fail(const std::string& key) {
        auto it = flags_.find(key);
        if (it == flags_.end()) return false;
        int cur = it->second.load(std::memory_order_relaxed);
        if (cur <= 0) return false;
        return it->second.fetch_sub(1, std::memory_order_relaxed) == 1;
    }

private:
    std::unordered_map<std::string, std::atomic<int>> flags_;
};

} // namespace chaos
} // namespace scratchbird

#define SB_FAULT(key) if (scratchbird::chaos::FaultInjector::instance().should_fail(key)) { throw std::runtime_error("Injected fault: " key); }

#else

#define SB_FAULT(key) do { } while(0)

#endif

