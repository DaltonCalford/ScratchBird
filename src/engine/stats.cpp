#include "scratchbird/engine/executor.h"
#include "scratchbird/engine/index_btree.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>
#include <random>

namespace scratchbird::engine
{

    static std::unordered_map<std::string, IndexStats> g_stats;
    static std::mutex g_mu;
    static std::atomic<std::uint64_t> g_analyze_epoch{1};
    static std::atomic<bool> g_auto_analyze{false};
    static std::unordered_map<std::string, PartitionMap> g_partitions;
    static std::unordered_map<std::string, FdwCost> g_fdw_costs;
    static std::unordered_map<std::string, std::uint64_t> g_rel_epoch; // relation -> epoch

    const std::unordered_map<std::string, IndexStats>& stats_registry()
    {
        return g_stats;
    }

    void stats_register(const std::string& index_name, const IndexStats& st)
    {
        std::lock_guard<std::mutex> lg(g_mu);
        IndexStats cpy = st;
        cpy.analyze_epoch = g_analyze_epoch.load(std::memory_order_relaxed);
        cpy.stale = false;
        g_stats[index_name] = std::move(cpy);
        g_rel_epoch[index_name] = cpy.analyze_epoch;
    }

    // Simple periodic auto-analyze trigger: bump epoch and clear staleness
    static void auto_analyze_tick()
    {
        if (!g_auto_analyze.load(std::memory_order_relaxed))
            return;
        // In a real system, we'd schedule per-relation sampling here.
        g_analyze_epoch.fetch_add(1, std::memory_order_relaxed);
        for (auto& kv : g_stats)
            kv.second.stale = false;
    }

    void set_auto_analyze_enabled(bool enabled)
    {
        g_auto_analyze.store(enabled, std::memory_order_relaxed);
    }

    void stats_auto_analyze_tick()
    {
        auto_analyze_tick();
    }

    std::uint64_t stats_relation_epoch(const std::string& relation)
    {
        auto it = g_rel_epoch.find(relation);
        if (it == g_rel_epoch.end())
            return 0;
        return it->second;
    }

    const IndexStats* stats_lookup(const std::string& index_name)
    {
        auto it = g_stats.find(index_name);
        if (it == g_stats.end())
            return nullptr;
        return &it->second;
    }

    void stats_mark_stale(const std::string& index_name, bool stale)
    {
        std::lock_guard<std::mutex> lg(g_mu);
        auto it = g_stats.find(index_name);
        if (it != g_stats.end())
            it->second.stale = stale;
    }

    void stats_set_pk(const std::string& index_name, bool is_pk)
    {
        std::lock_guard<std::mutex> lg(g_mu);
        auto it = g_stats.find(index_name);
        if (it != g_stats.end())
            it->second.is_pk = is_pk;
    }

    void partition_register(const PartitionMap& map)
    {
        std::lock_guard<std::mutex> lg(g_mu);
        g_partitions[map.relation] = map;
    }

    const PartitionMap* partition_lookup(const std::string& relation)
    {
        auto it = g_partitions.find(relation);
        if (it == g_partitions.end())
            return nullptr;
        return &it->second;
    }

    void fdw_register_cost(const std::string& relation, const FdwCost& cost)
    {
        std::lock_guard<std::mutex> lg(g_mu);
        g_fdw_costs[relation] = cost;
    }

    const FdwCost* fdw_lookup_cost(const std::string& relation)
    {
        auto it = g_fdw_costs.find(relation);
        if (it == g_fdw_costs.end())
            return nullptr;
        return &it->second;
    }

    // Helpers to compute ndistinct/mcv/histogram from a sample of keys
    static void compute_moments(const std::vector<std::string>& sample, IndexStats& out)
    {
        if (sample.empty())
            return;
        // ndistinct estimate: count distinct in sample; use Good–Turing-ish correction
        std::vector<std::string> sorted = sample;
        std::sort(sorted.begin(), sorted.end());
        std::size_t d = 0;
        std::size_t singletons = 0;
        for (std::size_t i = 0; i < sorted.size();) {
            std::size_t j = i + 1;
            while (j < sorted.size() && sorted[j] == sorted[i])
                ++j;
            std::size_t freq = j - i;
            if (freq == 1)
                ++singletons;
            ++d;
            i = j;
        }
        double n = static_cast<double>(sample.size());
        double f1 = static_cast<double>(singletons);
        double nd = d + (f1 > 0 ? (f1 * f1) / (2.0 * (n - f1)) : 0.0);
        out.ndistinct = nd;
        // MCV: top K values
        std::vector<std::pair<std::string, int>> counts;
        counts.reserve(d);
        for (std::size_t i = 0; i < sorted.size();) {
            std::size_t j = i + 1;
            while (j < sorted.size() && sorted[j] == sorted[i])
                ++j;
            counts.push_back({sorted[i], static_cast<int>(j - i)});
            i = j;
        }
        std::sort(counts.begin(), counts.end(),
                  [](auto& a, auto& b) { return a.second > b.second; });
        int topk = std::min<int>(counts.size(), 10);
        out.mcv.clear();
        for (int i = 0; i < topk; ++i)
            out.mcv.push_back({counts[i].first, counts[i].second / n});
        // Histogram: equally spaced buckets over sorted keys
        out.histogram.clear();
        int buckets = 10;
        for (int b = 1; b <= buckets; ++b) {
            std::size_t pos = static_cast<std::size_t>(std::round((b * n) / (buckets + 1)));
            if (pos >= sorted.size())
                pos = sorted.size() - 1;
            out.histogram.push_back({sorted[pos], static_cast<double>(pos + 1) / n});
        }
        // Correlation: monotonicity approx using Spearman rho surrogate on ranks vs row_id order
        // Here we assume page-order correlates with key-order; set moderate positive default
        out.correlation = 0.5;
    }

    // Optional: advance analyze epoch (e.g., after ANALYZE batch)
    static void advance_analyze_epoch()
    {
        g_analyze_epoch.fetch_add(1, std::memory_order_relaxed);
    }

    // Convenience: collect stats for a BTreeIndex by scanning keys (in-order) up to a cap
    IndexStats collect_btree_stats(const BTreeIndex&)
    {
        IndexStats st{};
        // Placeholder: cannot access private traversal here; rely on BTreeIndex::compute_stats
        // The caller should merge BTreeStats into IndexStats and optionally pass a sample of keys.
        return st;
    }

} // namespace scratchbird::engine
