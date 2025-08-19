#ifndef SCRATCHBIRD_ENGINE_EXECUTOR_H
#define SCRATCHBIRD_ENGINE_EXECUTOR_H

#include "scratchbird/engine/ast.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace scratchbird
{
    namespace engine
    {

        struct ExecutionResult {
            std::vector<std::string> columns;
            std::vector<std::vector<std::string>> rows;
        };

        struct NodeActuals {
            std::string node_id; // simple ordinal path like 0, 0.1, 1
            std::uint64_t rows{0};
            std::uint64_t time_ms{0};
            std::uint64_t memory_bytes{0};
        };

        // In-memory stats registry placeholder until catalog is wired
        struct IndexStats {
            std::uint32_t height{0};
            std::uint32_t leaf_pages{0};
            std::uint32_t branch_pages{0};
            std::uint64_t key_count{0};
            double ndistinct{-1.0}; // -1 means unknown; else estimate
            double correlation{0.0};
            std::vector<std::pair<std::string, double>> mcv;       // value, freq
            std::vector<std::pair<std::string, double>> histogram; // bucket boundary, cumulative
            // Rich stats additions
            std::unordered_map<std::string, double> multi_ndistinct; // key: "col" or "col1,col2"
            bool is_pk{false};
            std::uint64_t analyze_epoch{0};
            bool stale{false};
        };

        const std::unordered_map<std::string, IndexStats>& stats_registry();
        void stats_register(const std::string& index_name, const IndexStats& st);
        const IndexStats* stats_lookup(const std::string& index_name);
        void stats_mark_stale(const std::string& index_name, bool stale);
        void stats_set_pk(const std::string& index_name, bool is_pk);

        // Partition metadata registry
        struct PartitionRange {
            std::string name;
            std::string start;
            bool start_inclusive{true};
            std::string end;
            bool end_inclusive{false};
            // list partitioning support; when non-empty, this range acts as a list partition
            std::vector<std::string> list_values;
        };
        struct PartitionMap {
            std::string relation;
            std::string key_column;
            std::vector<PartitionRange> ranges; // non-overlapping, ordered
        };
        void partition_register(const PartitionMap& map);
        const PartitionMap* partition_lookup(const std::string& relation);

        // FDW-specific costing (per relation)
        struct FdwCost {
            bool is_fdw{false};
            double startup_ms{0.0};
            double per_row_ms{0.0};
            double transfer_bytes_per_row{0.0};
        };
        void fdw_register_cost(const std::string& relation, const FdwCost& cost);
        const FdwCost* fdw_lookup_cost(const std::string& relation);

        // Auto-ANALYZE controls
        void set_auto_analyze_enabled(bool enabled);
        void stats_auto_analyze_tick();

        // Stats epoch per relation (increments on ANALYZE/stats updates)
        std::uint64_t stats_relation_epoch(const std::string& relation);

        ExecutionResult execute_ast(const Ast& ast);

        // Phase 5: execute a minimal SELECT query directly from SQL text
        ExecutionResult execute_select_sql(const std::string& sql);

        // Phase 5: EXPLAIN ANALYZE execution for SELECT; returns a single-row plan with actuals
        ExecutionResult explain_analyze_select_sql(const std::string& sql);
        // Phase 6: Multiline EXPLAIN ANALYZE
        ExecutionResult explain_analyze_select_sql_multiline(const std::string& sql);
        // Optional: EXPLAIN in JSON format (single-row JSON text)
        ExecutionResult explain_json_select_sql(const std::string& sql);
        ExecutionResult explain_analyze_json_select_sql(const std::string& sql);

        // Configure the executor with a session/database path for catalog operations
        void set_executor_db_path(const std::string& path);
        const std::string& get_executor_db_path();

        // Phase 6: Prepared statement cache (minimal) for SELECT
        int prepare_select_sql(const std::string& sql);
        ExecutionResult execute_prepared_select(int handle);
        void invalidate_prepared_cache();

        // Phase 7 (initial): minimal INSERT executor with NOT NULL and CHECK enforcement
        ExecutionResult execute_insert_sql(const std::string& sql);
        ExecutionResult execute_update_sql(const std::string& sql);
        ExecutionResult execute_delete_sql(const std::string& sql);

        // Phase 7.E: session-wide deferral toggle (minimal)
        inline void set_constraints_deferred_all(bool deferred)
        {
            extern bool g_constraints_deferred_all; // defined in executor.cpp
            g_constraints_deferred_all = deferred;
        }

        // Phase 7.E: per-constraint deferral list (names)
        void set_constraints_deferred_list(const std::vector<std::string>& names, bool deferred);
        // Phase 7.E+: explicit IMMEDIATE overrides (names/ALL)
        void set_constraints_immediate_list(const std::vector<std::string>& names, bool immediate);
        void set_constraints_immediate_all(bool immediate);
        void reset_constraints_deferral();

        // Transaction-scoped hooks for deferred constraints
        // Returns empty string on success; non-empty error message on violation
        std::string executor_commit();
        void executor_rollback();

        // Optional knobs: enable/disable index use, choose join method, force join order
        struct OptimizerHints {
            bool enable_indexes{true};
            bool force_nested_loop{false};
            bool force_hash_join{false};
            bool force_join_order{false};
            bool force_merge_join{false};
            bool prefer_exists_semi_anti{false};
            double replan_drift_threshold{2.0}; // if actual/estimate > threshold, replan next time
        };
        void set_optimizer_hints(const OptimizerHints& hints);
        OptimizerHints get_optimizer_hints();

    } // namespace engine
} // namespace scratchbird

#endif // SCRATCHBIRD_ENGINE_EXECUTOR_H
