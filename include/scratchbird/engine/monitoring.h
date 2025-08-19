#ifndef SCRATCHBIRD_ENGINE_MONITORING_H
#define SCRATCHBIRD_ENGINE_MONITORING_H

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace scratchbird::engine
{
    struct MonCounters {
        std::uint64_t fetch_hits{0};
        std::uint64_t fetch_misses{0};
        std::uint64_t flushes{0};
        std::uint64_t latch_shared{0};
        std::uint64_t latch_exclusive{0};
        std::uint64_t btree_splits{0};
    };

    class Monitoring
    {
      public:
        // Increment helpers
        static void inc_fetch_hit();
        static void inc_fetch_miss();
        static void inc_flush();
        static void inc_latch_shared();
        static void inc_latch_exclusive();
        static void inc_btree_split();

        static MonCounters snapshot();
        // Convenience for sys.monitoring rows
        static std::vector<std::vector<std::string>> rows_as_table();

      private:
        static std::atomic<std::uint64_t> fetch_hits_;
        static std::atomic<std::uint64_t> fetch_misses_;
        static std::atomic<std::uint64_t> flushes_;
        static std::atomic<std::uint64_t> latch_shared_;
        static std::atomic<std::uint64_t> latch_exclusive_;
        static std::atomic<std::uint64_t> btree_splits_;
    };
} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_MONITORING_H
