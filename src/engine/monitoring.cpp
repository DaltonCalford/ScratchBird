#include "scratchbird/engine/monitoring.h"

namespace scratchbird::engine
{
    std::atomic<std::uint64_t> Monitoring::fetch_hits_{0};
    std::atomic<std::uint64_t> Monitoring::fetch_misses_{0};
    std::atomic<std::uint64_t> Monitoring::flushes_{0};
    std::atomic<std::uint64_t> Monitoring::latch_shared_{0};
    std::atomic<std::uint64_t> Monitoring::latch_exclusive_{0};
    std::atomic<std::uint64_t> Monitoring::btree_splits_{0};

    void Monitoring::inc_fetch_hit()
    {
        fetch_hits_.fetch_add(1, std::memory_order_relaxed);
    }
    void Monitoring::inc_fetch_miss()
    {
        fetch_misses_.fetch_add(1, std::memory_order_relaxed);
    }
    void Monitoring::inc_flush()
    {
        flushes_.fetch_add(1, std::memory_order_relaxed);
    }
    void Monitoring::inc_latch_shared()
    {
        latch_shared_.fetch_add(1, std::memory_order_relaxed);
    }
    void Monitoring::inc_latch_exclusive()
    {
        latch_exclusive_.fetch_add(1, std::memory_order_relaxed);
    }
    void Monitoring::inc_btree_split()
    {
        btree_splits_.fetch_add(1, std::memory_order_relaxed);
    }

    MonCounters Monitoring::snapshot()
    {
        MonCounters m{};
        m.fetch_hits = fetch_hits_.load();
        m.fetch_misses = fetch_misses_.load();
        m.flushes = flushes_.load();
        m.latch_shared = latch_shared_.load();
        m.latch_exclusive = latch_exclusive_.load();
        m.btree_splits = btree_splits_.load();
        return m;
    }

    std::vector<std::vector<std::string>> Monitoring::rows_as_table()
    {
        auto m = snapshot();
        return {{"fetch_hits", std::to_string(m.fetch_hits)},
                {"fetch_misses", std::to_string(m.fetch_misses)},
                {"flushes", std::to_string(m.flushes)},
                {"latch_shared", std::to_string(m.latch_shared)},
                {"latch_exclusive", std::to_string(m.latch_exclusive)},
                {"btree_splits", std::to_string(m.btree_splits)}};
    }
} // namespace scratchbird::engine
