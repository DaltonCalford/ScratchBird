#ifndef SCRATCHBIRD_ENGINE_SERIALIZABLE_ISOLATION_H
#define SCRATCHBIRD_ENGINE_SERIALIZABLE_ISOLATION_H

#include "scratchbird/engine/txn.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace scratchbird::engine
{
    // Forward declare if not available
    using TransactionId = std::uint64_t;
} // namespace scratchbird::engine

namespace scratchbird::engine
{

    // SERIALIZABLE isolation level implementation using predicate locking
    // and serialization graph testing (SSI - Serializable Snapshot Isolation)

    // Predicate lock types for SERIALIZABLE isolation
    enum class PredicateLockType {
        TUPLE_READ,    // Read lock on specific tuple
        TUPLE_WRITE,   // Write lock on specific tuple
        RANGE_READ,    // Read lock on range of tuples
        RANGE_WRITE,   // Write lock on range of tuples
        RELATION_READ, // Read lock on entire relation
        RELATION_WRITE // Write lock on entire relation
    };

    // Predicate lock structure
    struct PredicateLock {
        TransactionId txn_id{0};
        PredicateLockType lock_type;
        std::uint32_t relation_oid{0};
        std::uint32_t page_id{0};
        std::uint32_t tuple_id{0};
        std::string predicate_condition; // For range locks

        bool conflicts_with(const PredicateLock& other) const;
        std::string to_string() const;
    };

    // Dangerous structure detection for SSI
    struct DangerousStructure {
        TransactionId T1{0}; // First transaction
        TransactionId T2{0}; // Second transaction
        TransactionId T3{0}; // Third transaction (may be same as T1)

        enum Type {
            RW_ANTIDEPENDENCY, // T1 reads, T2 writes (rw-antidependency)
            WR_DEPENDENCY,     // T1 writes, T2 reads (wr-dependency)
            WW_DEPENDENCY      // T1 writes, T2 writes (ww-dependency)
        } type;

        std::string description;
        bool is_cycle() const
        {
            return T1 == T3;
        }
    };

    // SIREAD lock for implementing SSI
    struct SIReadLock {
        TransactionId txn_id{0};
        std::uint32_t relation_oid{0};
        std::uint32_t page_id{0};
        std::uint32_t tuple_id{0};
        bool is_relation_lock{false};
        bool is_page_lock{false};

        std::string get_key() const;
    };

    // Serializable isolation manager implementing SSI algorithm
    class SerializableIsolationManager
    {
      public:
        SerializableIsolationManager();
        ~SerializableIsolationManager();

        // Transaction lifecycle for SERIALIZABLE isolation
        bool begin_serializable_transaction(TransactionId txn_id);
        bool commit_serializable_transaction(TransactionId txn_id);
        bool rollback_serializable_transaction(TransactionId txn_id);

        // SIREAD lock management
        bool acquire_siread_lock(TransactionId txn_id, std::uint32_t relation_oid,
                                 std::uint32_t page_id = 0, std::uint32_t tuple_id = 0);
        bool release_siread_locks(TransactionId txn_id);

        // Conflict detection for reads
        bool check_read_write_conflicts(TransactionId reader_txn, std::uint32_t relation_oid,
                                        std::uint32_t page_id, std::uint32_t tuple_id);

        // Conflict detection for writes
        bool check_write_read_conflicts(TransactionId writer_txn, std::uint32_t relation_oid,
                                        std::uint32_t page_id, std::uint32_t tuple_id);

        // SSI dangerous structure detection
        bool detect_dangerous_structure(TransactionId txn_id);
        bool has_serialization_anomaly(TransactionId txn_id);

        // Predicate locking for complex queries
        bool acquire_predicate_lock(TransactionId txn_id, PredicateLockType lock_type,
                                    std::uint32_t relation_oid, const std::string& predicate = "");
        bool check_predicate_conflicts(TransactionId txn_id, PredicateLockType lock_type,
                                       std::uint32_t relation_oid,
                                       const std::string& predicate = "");

        // Serialization graph management
        bool add_dependency_edge(TransactionId from_txn, TransactionId to_txn,
                                 const std::string& dependency_type);
        bool has_cycle_in_serialization_graph();
        std::vector<TransactionId> find_cycle_transactions();

        // Monitoring and statistics
        std::size_t get_active_serializable_transactions() const;
        std::size_t get_siread_lock_count() const;
        std::size_t get_predicate_lock_count() const;
        std::size_t get_serialization_failures() const;

        // Configuration
        void set_aggressive_cleanup(bool enabled)
        {
            aggressive_cleanup_ = enabled;
        }
        void set_max_predicate_locks(std::size_t max_locks)
        {
            max_predicate_locks_ = max_locks;
        }

      private:
        mutable std::mutex serializable_mutex_;

        // Active SERIALIZABLE transactions
        std::unordered_set<TransactionId> serializable_transactions_;

        // SIREAD locks for SSI
        std::unordered_map<std::string, SIReadLock> siread_locks_;
        std::unordered_map<TransactionId, std::vector<std::string>> txn_siread_locks_;

        // Predicate locks
        std::vector<PredicateLock> predicate_locks_;

        // Serialization graph for cycle detection
        std::unordered_map<TransactionId, std::unordered_set<TransactionId>> serialization_graph_;

        // Dangerous structure tracking
        std::vector<DangerousStructure> dangerous_structures_;

        // Statistics
        std::size_t serialization_failures_{0};
        std::size_t predicate_lock_escalations_{0};

        // Configuration
        bool aggressive_cleanup_{true};
        std::size_t max_predicate_locks_{10000};

        // Internal methods
        bool is_transaction_serializable(TransactionId txn_id) const;
        void cleanup_completed_transaction(TransactionId txn_id);

        // SSI algorithm implementation
        bool check_rw_antidependency(TransactionId reader, TransactionId writer);
        bool check_wr_dependency(TransactionId writer, TransactionId reader);
        void record_dangerous_structure(TransactionId T1, TransactionId T2,
                                        DangerousStructure::Type type, const std::string& desc);

        // Cycle detection using DFS
        bool has_cycle_from_node(TransactionId start_txn,
                                 std::unordered_set<TransactionId>& visited,
                                 std::unordered_set<TransactionId>& recursion_stack) const;

        // Lock management helpers
        std::string make_siread_key(std::uint32_t relation_oid, std::uint32_t page_id,
                                    std::uint32_t tuple_id) const;
        bool conflicts_with_existing_locks(const PredicateLock& new_lock) const;

        // Maintenance operations
        void cleanup_old_siread_locks();
        void escalate_predicate_locks();
    };

    // High-level SERIALIZABLE transaction utilities

    // RAII wrapper for SERIALIZABLE transactions
    class SerializableTransaction
    {
      public:
        SerializableTransaction(SerializableIsolationManager& manager, TransactionId txn_id);
        ~SerializableTransaction();

        // Disable copy/move to ensure proper RAII
        SerializableTransaction(const SerializableTransaction&) = delete;
        SerializableTransaction& operator=(const SerializableTransaction&) = delete;
        SerializableTransaction(SerializableTransaction&&) = delete;
        SerializableTransaction& operator=(SerializableTransaction&&) = delete;

        bool commit();
        bool rollback();

        // Query operations with automatic conflict detection
        bool read_tuple(std::uint32_t relation_oid, std::uint32_t page_id, std::uint32_t tuple_id);
        bool write_tuple(std::uint32_t relation_oid, std::uint32_t page_id, std::uint32_t tuple_id);
        bool scan_relation(std::uint32_t relation_oid, const std::string& predicate = "");

        TransactionId get_transaction_id() const
        {
            return txn_id_;
        }
        bool is_active() const
        {
            return active_;
        }

      private:
        SerializableIsolationManager& manager_;
        TransactionId txn_id_;
        bool active_;
        bool committed_;
    };

    // Global SERIALIZABLE isolation management

    // Get the global SERIALIZABLE isolation manager
    SerializableIsolationManager& get_serializable_manager();

    // Check if a transaction should use SERIALIZABLE isolation
    bool should_use_serializable_isolation(TransactionId txn_id);

    // Convert isolation level string to enum
    enum class IsolationLevel { READ_UNCOMMITTED, READ_COMMITTED, REPEATABLE_READ, SERIALIZABLE };

    IsolationLevel parse_isolation_level(const std::string& level_str);
    std::string isolation_level_to_string(IsolationLevel level);

    // Validate serializable transaction consistency
    bool validate_serializable_consistency(const std::vector<TransactionId>& transaction_sequence);

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_SERIALIZABLE_ISOLATION_H
