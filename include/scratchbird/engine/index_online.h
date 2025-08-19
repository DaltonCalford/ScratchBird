#ifndef SCRATCHBIRD_ENGINE_INDEX_ONLINE_H
#define SCRATCHBIRD_ENGINE_INDEX_ONLINE_H

#include "scratchbird/engine/catalog_mem.h"
#include "scratchbird/engine/index_build.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <utility>
#include <vector>

namespace scratchbird::engine
{

    enum class OnlineBuildState { Registered, Backfill, Catchup, Fence, Swapped, Aborted };

    struct IndexDelta {
        std::string key;
        std::uint64_t row_id{0};
        bool is_delete{false};
    };

    // Forward declaration to allow unique_ptr without including full definition here
    class BTreeIndex;

    // Concurrent index builder coordinating backfill, delta catch-up, and fence/swap.
    class ConcurrentIndexBuild
    {
      public:
        ConcurrentIndexBuild(IndexDefinition def, IndexBuildOptions opts, std::string base_path);
        ~ConcurrentIndexBuild();

        // Feed a delta produced by WAL while backfill/catch-up runs.
        void register_delta(const IndexDelta& d);

        // Run backfill: caller provides a scan that invokes emit(row_key, row_id) for each row.
        // Returns false if aborted.
        bool
        run_backfill(const std::function<
                     void(const std::function<void(const std::string&, std::uint64_t)>&)>& scan_fn);

        // Apply queued deltas until the queue stays below a threshold for a quiet period.
        bool run_catchup_until_quiet(std::chrono::milliseconds quiet_for,
                                     std::size_t max_queue = 0);

        // Fence writers and swap: caller supplies callbacks to enter/leave fence.
        // Applies remaining deltas under fence, then marks swapped.
        bool fence_and_swap(const std::function<void()>& fence_begin,
                            const std::function<void()>& fence_end);

        OnlineBuildState state() const
        {
            return state_.load();
        }
        IndexBuildResult result() const
        {
            return result_;
        }

      private:
        void apply_delta_locked(const IndexDelta& d);

        IndexDefinition def_;
        IndexBuildOptions opts_;
        std::string base_path_;

        // Staging index
        std::unique_ptr<BTreeIndex> staging_;

        // Delta queue and synchronization
        std::mutex mu_;
        std::condition_variable cv_;
        std::queue<IndexDelta> q_;
        std::atomic<OnlineBuildState> state_{OnlineBuildState::Registered};
        bool aborted_{false};

        IndexBuildResult result_{};

        // Monitoring counters
        std::atomic<std::uint64_t> backfill_rows_{0};
        std::atomic<std::uint64_t> delta_applied_{0};
    };

    // Lightweight monitoring registry for sys.monitoring.index_builds
    struct IndexBuildMonRow {
        std::string index_name;
        std::string relation;
        OnlineBuildState state;
        std::uint64_t backfill_rows;
        std::uint64_t delta_applied;
    };

    class IndexBuildMonitor
    {
      public:
        static void upsert(const IndexBuildMonRow& r);
        static std::vector<IndexBuildMonRow> snapshot();
        // Convenience: rows as strings for sys.monitoring.index_builds
        static std::string state_to_string(OnlineBuildState s);
        static std::vector<std::vector<std::string>> rows_as_table();

      private:
        static std::mutex mu_;
        static std::unordered_map<std::string, IndexBuildMonRow> rows_;
    };

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_INDEX_ONLINE_H
