#ifndef SCRATCHBIRD_ENGINE_TXN_H
#define SCRATCHBIRD_ENGINE_TXN_H

#include "scratchbird/engine/alloc.h"
#include "scratchbird/engine/ods.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace scratchbird::engine
{

    enum class TxnState : std::uint8_t {
        Idle = 0,
        Active = 1,
        Prepared = 2,
        Committed = 3,
        Aborted = 4
    };

    struct Transaction {
        std::uint64_t id{0};
        TxnState state{TxnState::Idle};
    };

    struct SnapshotRC {
        std::uint64_t cutoff_committed_id{0};
        std::uint64_t own_xid{0};
    };

    struct SnapshotRR {
        std::uint64_t cutoff_committed_id{0};
        std::uint64_t own_xid{0};
    };

    class TransactionManager
    {
      public:
        explicit TransactionManager(FileMap fmap, std::uint32_t page_size)
            : fmap_(std::move(fmap)), page_size_(page_size)
        {
        }

        // Create first TIP page and mark all entries idle
        void init_seed();

        // Transaction primitives
        Transaction begin();
        void commit(Transaction& tx);
        void rollback(Transaction& tx);
        void prepare(Transaction& tx);

        // Snapshot (Read Committed)
        SnapshotRC snapshot_read_committed(std::uint64_t own_xid = 0) const;
        // Repeatable Read snapshot
        SnapshotRR snapshot_repeatable_read(std::uint64_t own_xid = 0) const;

        // TIP state helpers
        TxnState read_txn_state(std::uint64_t txn_id) const;
        inline std::uint8_t read_txn_status(std::uint64_t txn_id) const
        {
            return static_cast<std::uint8_t>(read_txn_state(txn_id));
        }

        // Maintenance: sweep committed-deleted tuples in a heap page
        void sweep_heap_page(FileMap& fmap, std::uint32_t page_no) const;

        // ID generators (stubbed)
        std::uint64_t next_transaction_id()
        {
            return ++last_txn_id_;
        }
        std::uint64_t next_attachment_id()
        {
            return ++last_att_id_;
        }
        std::uint64_t next_statement_id()
        {
            return ++last_stmt_id_;
        }

        std::uint32_t tip_page_no() const
        {
            return tip_page_no_;
        }

        // Write-write conflict management (simple lock manager)
        bool acquire_write_lock(const ods::RowId& rid, std::uint64_t xid);
        void release_write_lock(const ods::RowId& rid, std::uint64_t xid);
        bool detect_deadlock(std::uint64_t waiting_xid, std::uint64_t holding_xid) const;

      private:
        void write_tip_state(std::uint64_t txn_id, TxnState state);
        std::size_t tip_slot_for(std::uint64_t txn_id) const;
        std::uint32_t page_size_{4096};
        FileMap fmap_;
        std::uint32_t tip_page_no_{0};
        std::uint64_t last_committed_xid_{0};
        std::uint64_t last_txn_id_{0};
        std::uint64_t last_att_id_{0};
        std::uint64_t last_stmt_id_{0};
        // locks: rid -> holder xid
        std::unordered_map<std::uint64_t, std::uint64_t> write_locks_;
        // wait-for edges: waiter -> holder
        std::unordered_multimap<std::uint64_t, std::uint64_t> wait_for_;
    };

    // Deadlock victim selection strategies
    enum class DeadlockVictimPolicy {
        YoungTransaction,    // Abort newest transaction (default)
        OldTransaction,      // Abort oldest transaction
        FewestLocks,        // Abort transaction holding fewest locks
        LowestCost          // Abort transaction with lowest estimated cost
    };

    // Global, minimal lock manager (Phase 3 WW-conflict + deadlock detection)
    struct LockManager {
        static bool acquire_write_lock(const ods::RowId& rid, std::uint64_t xid);
        static void release_write_lock(const ods::RowId& rid, std::uint64_t xid);
        static bool detect_deadlock(std::uint64_t waiting_xid, std::uint64_t holding_xid);
        
        // Enhanced deadlock resolution with victim selection
        static void set_deadlock_victim_policy(DeadlockVictimPolicy policy);
        static DeadlockVictimPolicy get_deadlock_victim_policy();
        static std::uint64_t choose_deadlock_victim(const std::vector<std::uint64_t>& cycle);
        static std::vector<std::uint64_t> find_deadlock_cycle(std::uint64_t waiting_xid, std::uint64_t holding_xid);

      private:
        static std::unordered_map<std::uint64_t, std::uint64_t> s_write_locks;   // rid -> holder
        static std::unordered_multimap<std::uint64_t, std::uint64_t> s_wait_for; // waiter -> holder
        static std::unordered_map<std::uint64_t, std::uint64_t> s_txn_start_times; // xid -> start_time
        static std::unordered_map<std::uint64_t, std::uint64_t> s_txn_lock_counts; // xid -> lock_count
        static DeadlockVictimPolicy s_victim_policy;
        static std::mutex s_mutex;
    };

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_TXN_H
