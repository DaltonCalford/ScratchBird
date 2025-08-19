#ifndef SCRATCHBIRD_ENGINE_EXECUTOR_NODES_H
#define SCRATCHBIRD_ENGINE_EXECUTOR_NODES_H

#include "scratchbird/engine/heap.h"
#include "scratchbird/engine/txn.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace scratchbird::engine
{

    // Forward declarations
    struct ExecutorContext;
    struct Instrumentation;

    // Tuple representation for executor
    using Tuple = std::vector<Value>;

    // Instrumentation counters for executor nodes
    struct Instrumentation {
        std::uint64_t input_rows{0};
        std::uint64_t output_rows{0};
        std::uint64_t filtered_rows{0};
        std::uint64_t memory_bytes_peak{0};
        std::uint64_t temp_bytes_written{0};
        std::uint64_t spills_count{0};
        std::uint64_t cpu_time_ms{0};
        std::uint64_t wall_time_ms{0};
    };

    // Execution context passed to all nodes
    struct ExecutorContext {
        std::string db_path;
        std::uint32_t work_mem_bytes{64 * 1024 * 1024}; // 64MB default
        // Future: snapshot, memory context, temp manager, collation resolver
    };

    // Base executor node interface (Volcano-style iterator)
    class ExecutorNode
    {
      public:
        virtual ~ExecutorNode() = default;

        // Lifecycle: open -> next* -> close
        virtual void open(ExecutorContext& ctx) = 0;
        virtual bool next(Tuple& out) = 0;
        virtual void close() = 0;

        // Column metadata
        virtual std::vector<std::string> columns() const = 0;

        // Instrumentation
        const Instrumentation& get_instrumentation() const
        {
            return instr_;
        }

      protected:
        Instrumentation instr_;
    };

    // Scan node for table scans
    class SeqScanNode : public ExecutorNode
    {
      public:
        SeqScanNode(const std::string& schema, const std::string& table, const std::string& alias,
                    const std::vector<std::string>& projections = {},
                    const std::string& predicate = "");

        void open(ExecutorContext& ctx) override;
        bool next(Tuple& out) override;
        void close() override;
        std::vector<std::string> columns() const override;

      private:
        std::string schema_;
        std::string table_;
        std::string alias_;
        std::vector<std::string> projections_;
        std::string predicate_;

        // Runtime state
        std::vector<std::string> columns_;
        std::vector<std::vector<Value>> rows_;
        std::size_t current_row_;
        bool opened_;
    };

    // Hash join node for equi-joins
    class HashJoinNode : public ExecutorNode
    {
      public:
        enum JoinType { Inner, LeftOuter, RightOuter, FullOuter };

        HashJoinNode(std::unique_ptr<ExecutorNode> left, std::unique_ptr<ExecutorNode> right,
                     const std::vector<std::string>& left_keys,
                     const std::vector<std::string>& right_keys, JoinType join_type = Inner,
                     const std::string& additional_predicate = "");

        void open(ExecutorContext& ctx) override;
        bool next(Tuple& out) override;
        void close() override;
        std::vector<std::string> columns() const override;

      private:
        std::unique_ptr<ExecutorNode> left_child_;
        std::unique_ptr<ExecutorNode> right_child_;
        std::vector<std::string> left_keys_;
        std::vector<std::string> right_keys_;
        JoinType join_type_;
        std::string additional_predicate_;

        // Runtime state
        std::vector<std::string> columns_;
        std::unordered_map<std::string, std::vector<Tuple>> hash_table_;
        std::vector<Tuple> current_matches_;
        std::size_t current_match_idx_;
        Tuple current_left_tuple_;
        bool left_exhausted_;
        bool opened_;

        // Helper methods
        std::string build_hash_key(const Tuple& tuple, const std::vector<std::string>& keys,
                                   const std::vector<std::string>& column_names);
        std::vector<std::size_t> get_key_indices(const std::vector<std::string>& keys,
                                                 const std::vector<std::string>& column_names);
        bool evaluate_additional_predicate(const Tuple& left, const Tuple& right);
    };

    // Nested loop join node (simpler alternative to hash join)
    class NestedLoopJoinNode : public ExecutorNode
    {
      public:
        enum JoinType { Inner, LeftOuter };

        NestedLoopJoinNode(std::unique_ptr<ExecutorNode> left, std::unique_ptr<ExecutorNode> right,
                           const std::string& join_predicate, JoinType join_type = Inner);

        void open(ExecutorContext& ctx) override;
        bool next(Tuple& out) override;
        void close() override;
        std::vector<std::string> columns() const override;

      private:
        std::unique_ptr<ExecutorNode> left_child_;
        std::unique_ptr<ExecutorNode> right_child_;
        std::string join_predicate_;
        JoinType join_type_;

        // Runtime state
        std::vector<std::string> columns_;
        Tuple current_left_tuple_;
        bool has_current_left_;
        bool left_matched_;
        bool opened_;

        bool evaluate_join_predicate(const Tuple& left, const Tuple& right);
    };

    // Project node for column projection and computed expressions
    class ProjectNode : public ExecutorNode
    {
      public:
        ProjectNode(std::unique_ptr<ExecutorNode> child,
                    const std::vector<std::string>& projections);

        void open(ExecutorContext& ctx) override;
        bool next(Tuple& out) override;
        void close() override;
        std::vector<std::string> columns() const override;

      private:
        std::unique_ptr<ExecutorNode> child_;
        std::vector<std::string> projections_;
        std::vector<std::string> columns_;
        bool opened_;
    };

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_EXECUTOR_NODES_H
