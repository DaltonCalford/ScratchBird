#include "scratchbird/engine/txn.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <mutex>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace scratchbird::engine
{

    void TransactionManager::init_seed()
    {
        // Allocate a TIP page via Allocator: page layout = header + bytes for txn statuses
        // For now, put TIP at page 2 (after header and first PIP) if free
        std::vector<std::uint8_t> page(page_size_, 0);
        ods::PageHeader* hdr = reinterpret_cast<ods::PageHeader*>(page.data());
        hdr->page_no = 2;
        hdr->space_id = 1;
        hdr->type = static_cast<std::uint16_t>(ods::PageType::Tip);
        hdr->page_size = page_size_;
        // mark all txn slots idle (0)
        hdr->checksum = 0;
        hdr->checksum = ods::crc32c(page.data(), page.size());
        fmap_.write_page(hdr->page_no, page.data());
        tip_page_no_ = hdr->page_no;
    }

    std::size_t TransactionManager::tip_slot_for(std::uint64_t txn_id) const
    {
        const std::uint32_t slots = ods::transPerTIP(page_size_);
        return static_cast<std::size_t>(txn_id % slots);
    }

    TxnState TransactionManager::read_txn_state(std::uint64_t txn_id) const
    {
        std::vector<std::uint8_t> page(page_size_, 0);
        fmap_.read_page(tip_page_no_, page.data());
        std::size_t slot = tip_slot_for(txn_id);
        const std::uint8_t* base = page.data() + 64; // TIP body starts after header reserve
        return static_cast<TxnState>(base[slot]);
    }

    void TransactionManager::write_tip_state(std::uint64_t txn_id, TxnState state)
    {
        std::vector<std::uint8_t> page(page_size_, 0);
        fmap_.read_page(tip_page_no_, page.data());
        std::size_t slot = tip_slot_for(txn_id);
        std::uint8_t* base = page.data() + 64;
        base[slot] = static_cast<std::uint8_t>(state);
        auto* hdr = reinterpret_cast<ods::PageHeader*>(page.data());
        hdr->checksum = 0;
        hdr->checksum = ods::crc32c(page.data(), page.size());
        fmap_.write_page(tip_page_no_, page.data());
    }

    Transaction TransactionManager::begin()
    {
        Transaction tx{};
        tx.id = next_transaction_id();
        tx.state = TxnState::Active;
        write_tip_state(tx.id, tx.state);
        return tx;
    }

    void TransactionManager::prepare(Transaction& tx)
    {
        // Scaffolding: mark TIP as Prepared; no distributed side effects yet
        tx.state = TxnState::Prepared;
        write_tip_state(tx.id, tx.state);
    }

    void TransactionManager::commit(Transaction& tx)
    {
        tx.state = TxnState::Committed;
        write_tip_state(tx.id, tx.state);
        if (tx.id > last_committed_xid_) {
            last_committed_xid_ = tx.id;
        }
    }

    void TransactionManager::rollback(Transaction& tx)
    {
        tx.state = TxnState::Aborted;
        write_tip_state(tx.id, tx.state);
    }

    SnapshotRC TransactionManager::snapshot_read_committed(std::uint64_t own_xid) const
    {
        SnapshotRC s{};
        // Conservative cutoff for RC: allow up to the greater of last committed and last issued
        s.cutoff_committed_id =
            (last_committed_xid_ > last_txn_id_) ? last_committed_xid_ : last_txn_id_;
        s.own_xid = own_xid;
        return s;
    }

    SnapshotRR TransactionManager::snapshot_repeatable_read(std::uint64_t own_xid) const
    {
        SnapshotRR s{};
        s.cutoff_committed_id = last_committed_xid_;
        s.own_xid = own_xid;
        return s;
    }

    void TransactionManager::sweep_heap_page(FileMap& fmap, std::uint32_t page_no) const
    {
        std::vector<std::uint8_t> page(page_size_, 0);
        fmap.read_page(page_no, page.data());
        auto* ph = reinterpret_cast<ods::PageHeader*>(page.data());
        if (ph->type != static_cast<std::uint16_t>(ods::PageType::HeapData))
            return;
        // Read header
        std::uint16_t tuples_region = sizeof(ods::PageHeader) + sizeof(ods::HeapPageHeader);
        ods::HeapPageHeader hh{};
        std::memcpy(&hh, page.data() + sizeof(ods::PageHeader), sizeof hh);
        // Iterate slots and clear those with committed deleted_xid
        bool changed = false;
        for (std::uint16_t i = 0; i < hh.num_slots; ++i) {
            std::size_t spos =
                page.size() - (static_cast<std::size_t>(i) + 1) * ods::HEAP_SLOT_SIZE_BYTES;
            std::uint16_t off = 0;
            std::memcpy(&off, page.data() + spos, 2);
            if (off == 0)
                continue;
            if (off < tuples_region || off + sizeof(ods::TupleHeader) > page.size())
                continue;
            ods::TupleHeader th{};
            std::memcpy(&th, page.data() + off, sizeof th);
            if (th.deleted_xid != 0 && read_txn_state(th.deleted_xid) == TxnState::Committed) {
                std::uint16_t zero = 0;
                std::memcpy(page.data() + spos, &zero, 2);
                changed = true;
            }
        }
        if (changed) {
            ph->checksum = 0;
            ph->checksum = ods::crc32c(page.data(), page.size());
            fmap.write_page(page_no, page.data());
        }
    }

    // Static members for LockManager
    std::unordered_map<std::uint64_t, std::uint64_t> LockManager::s_write_locks;
    std::unordered_multimap<std::uint64_t, std::uint64_t> LockManager::s_wait_for;
    std::unordered_map<std::uint64_t, std::uint64_t> LockManager::s_txn_start_times;
    std::unordered_map<std::uint64_t, std::uint64_t> LockManager::s_txn_lock_counts;
    DeadlockVictimPolicy LockManager::s_victim_policy = DeadlockVictimPolicy::YoungTransaction;
    std::mutex LockManager::s_mutex;

    bool LockManager::acquire_write_lock(const ods::RowId& rid, std::uint64_t xid)
    {
        std::lock_guard<std::mutex> g(s_mutex);
        std::uint64_t key = ods::pack_rowid(rid);
        auto it = s_write_locks.find(key);
        
        // Track transaction start time (first lock acquisition)
        if (s_txn_start_times.find(xid) == s_txn_start_times.end()) {
            s_txn_start_times[xid] = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        }
        
        if (it == s_write_locks.end()) {
            s_write_locks[key] = xid;
            // Increment lock count for this transaction
            s_txn_lock_counts[xid]++;
            return true;
        }
        if (it->second == xid)
            return true;
        s_wait_for.emplace(xid, it->second);
        
        // Enhanced deadlock detection with victim selection
        auto cycle = find_deadlock_cycle(xid, it->second);
        if (!cycle.empty()) {
            std::uint64_t victim = choose_deadlock_victim(cycle);
            // Remove wait edge for victim (current implementation assumes caller handles abort)
            auto range = s_wait_for.equal_range(xid);
            for (auto p = range.first; p != range.second; ++p) {
                if (p->second == it->second) {
                    s_wait_for.erase(p);
                    break;
                }
            }
            // Return false if current transaction is victim, true if other transaction is victim
            return (victim != xid);
        }
        return false;
    }

    void LockManager::release_write_lock(const ods::RowId& rid, std::uint64_t xid)
    {
        std::lock_guard<std::mutex> g(s_mutex);
        std::uint64_t key = ods::pack_rowid(rid);
        auto it = s_write_locks.find(key);
        if (it != s_write_locks.end() && it->second == xid) {
            s_write_locks.erase(it);
            // Decrement lock count for this transaction
            if (s_txn_lock_counts[xid] > 0) {
                s_txn_lock_counts[xid]--;
            }
            // Clean up transaction metadata if no locks remaining
            if (s_txn_lock_counts[xid] == 0) {
                s_txn_lock_counts.erase(xid);
                s_txn_start_times.erase(xid);
            }
        }
        s_wait_for.erase(xid);
    }

    bool LockManager::detect_deadlock(std::uint64_t waiting_xid, std::uint64_t holding_xid)
    {
        // DFS from holding to see if we can reach waiting
        std::unordered_set<std::uint64_t> visited;
        std::stack<std::uint64_t> st;
        st.push(holding_xid);
        while (!st.empty()) {
            auto cur = st.top();
            st.pop();
            if (cur == waiting_xid)
                return true;
            if (!visited.insert(cur).second)
                continue;
            auto range = s_wait_for.equal_range(cur);
            for (auto p = range.first; p != range.second; ++p)
                st.push(p->second);
        }
        return false;
    }

    // Enhanced deadlock resolution functions
    
    void LockManager::set_deadlock_victim_policy(DeadlockVictimPolicy policy)
    {
        std::lock_guard<std::mutex> g(s_mutex);
        s_victim_policy = policy;
    }
    
    DeadlockVictimPolicy LockManager::get_deadlock_victim_policy()
    {
        std::lock_guard<std::mutex> g(s_mutex);
        return s_victim_policy;
    }
    
    std::vector<std::uint64_t> LockManager::find_deadlock_cycle(std::uint64_t waiting_xid, std::uint64_t holding_xid)
    {
        std::vector<std::uint64_t> cycle;
        std::unordered_set<std::uint64_t> visited;
        std::unordered_map<std::uint64_t, std::uint64_t> parent;
        std::stack<std::uint64_t> st;
        
        st.push(holding_xid);
        parent[holding_xid] = 0; // Mark as root
        
        while (!st.empty()) {
            auto cur = st.top();
            st.pop();
            
            if (cur == waiting_xid) {
                // Found cycle, reconstruct path
                std::uint64_t node = waiting_xid;
                cycle.push_back(node);
                while (parent[node] != 0) {
                    node = parent[node];
                    cycle.push_back(node);
                }
                std::reverse(cycle.begin(), cycle.end());
                cycle.push_back(waiting_xid); // Complete the cycle
                return cycle;
            }
            
            if (!visited.insert(cur).second)
                continue;
                
            auto range = s_wait_for.equal_range(cur);
            for (auto p = range.first; p != range.second; ++p) {
                if (parent.find(p->second) == parent.end()) {
                    parent[p->second] = cur;
                    st.push(p->second);
                }
            }
        }
        
        return cycle; // Empty if no cycle found
    }
    
    std::uint64_t LockManager::choose_deadlock_victim(const std::vector<std::uint64_t>& cycle)
    {
        if (cycle.empty()) return 0;
        
        switch (s_victim_policy) {
            case DeadlockVictimPolicy::YoungTransaction: {
                // Choose transaction with highest start time (most recent)
                std::uint64_t victim = cycle[0];
                std::uint64_t max_start_time = 0;
                for (auto xid : cycle) {
                    auto it = s_txn_start_times.find(xid);
                    if (it != s_txn_start_times.end() && it->second > max_start_time) {
                        max_start_time = it->second;
                        victim = xid;
                    }
                }
                return victim;
            }
            
            case DeadlockVictimPolicy::OldTransaction: {
                // Choose transaction with lowest start time (oldest)
                std::uint64_t victim = cycle[0];
                std::uint64_t min_start_time = UINT64_MAX;
                for (auto xid : cycle) {
                    auto it = s_txn_start_times.find(xid);
                    if (it != s_txn_start_times.end() && it->second < min_start_time) {
                        min_start_time = it->second;
                        victim = xid;
                    }
                }
                return victim;
            }
            
            case DeadlockVictimPolicy::FewestLocks: {
                // Choose transaction holding fewest locks
                std::uint64_t victim = cycle[0];
                std::uint64_t min_locks = UINT64_MAX;
                for (auto xid : cycle) {
                    auto it = s_txn_lock_counts.find(xid);
                    std::uint64_t lock_count = (it != s_txn_lock_counts.end()) ? it->second : 0;
                    if (lock_count < min_locks) {
                        min_locks = lock_count;
                        victim = xid;
                    }
                }
                return victim;
            }
            
            case DeadlockVictimPolicy::LowestCost: {
                // Simple heuristic: choose transaction with fewest locks as proxy for cost
                // In a full implementation, this could integrate with query cost estimates
                std::uint64_t victim = cycle[0];
                std::uint64_t min_locks = UINT64_MAX;
                for (auto xid : cycle) {
                    auto it = s_txn_lock_counts.find(xid);
                    std::uint64_t lock_count = (it != s_txn_lock_counts.end()) ? it->second : 0;
                    if (lock_count < min_locks) {
                        min_locks = lock_count;
                        victim = xid;
                    }
                }
                return victim;
            }
            
            default:
                return cycle[0]; // Fallback to first transaction in cycle
        }
    }

} // namespace scratchbird::engine
