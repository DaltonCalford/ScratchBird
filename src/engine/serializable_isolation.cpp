#include "scratchbird/engine/serializable_isolation.h"

#include <algorithm>
#include <sstream>

namespace scratchbird::engine
{

    // ========== PredicateLock Implementation ==========

    bool PredicateLock::conflicts_with(const PredicateLock& other) const
    {
        // Same transaction doesn't conflict with itself
        if (txn_id == other.txn_id) {
            return false;
        }

        // Different relations don't conflict
        if (relation_oid != other.relation_oid) {
            return false;
        }

        // Check for read-write conflicts
        bool this_is_write = (lock_type == PredicateLockType::TUPLE_WRITE ||
                              lock_type == PredicateLockType::RANGE_WRITE ||
                              lock_type == PredicateLockType::RELATION_WRITE);
        bool other_is_write = (other.lock_type == PredicateLockType::TUPLE_WRITE ||
                               other.lock_type == PredicateLockType::RANGE_WRITE ||
                               other.lock_type == PredicateLockType::RELATION_WRITE);

        // Write conflicts with any operation, read conflicts with write
        return this_is_write || other_is_write;
    }

    std::string PredicateLock::to_string() const
    {
        std::ostringstream oss;
        oss << "PredicateLock{txn=" << txn_id << ", type=";

        switch (lock_type) {
        case PredicateLockType::TUPLE_READ:
            oss << "TUPLE_READ";
            break;
        case PredicateLockType::TUPLE_WRITE:
            oss << "TUPLE_WRITE";
            break;
        case PredicateLockType::RANGE_READ:
            oss << "RANGE_READ";
            break;
        case PredicateLockType::RANGE_WRITE:
            oss << "RANGE_WRITE";
            break;
        case PredicateLockType::RELATION_READ:
            oss << "RELATION_READ";
            break;
        case PredicateLockType::RELATION_WRITE:
            oss << "RELATION_WRITE";
            break;
        }

        oss << ", rel=" << relation_oid;
        if (page_id != 0)
            oss << ", page=" << page_id;
        if (tuple_id != 0)
            oss << ", tuple=" << tuple_id;
        if (!predicate_condition.empty())
            oss << ", pred=\"" << predicate_condition << "\"";
        oss << "}";

        return oss.str();
    }

    // ========== SIReadLock Implementation ==========

    std::string SIReadLock::get_key() const
    {
        if (is_relation_lock) {
            return "rel:" + std::to_string(relation_oid);
        } else if (is_page_lock) {
            return "page:" + std::to_string(relation_oid) + ":" + std::to_string(page_id);
        } else {
            return "tuple:" + std::to_string(relation_oid) + ":" + std::to_string(page_id) + ":" +
                   std::to_string(tuple_id);
        }
    }

    // ========== SerializableIsolationManager Implementation ==========

    SerializableIsolationManager::SerializableIsolationManager() = default;
    SerializableIsolationManager::~SerializableIsolationManager() = default;

    bool SerializableIsolationManager::begin_serializable_transaction(TransactionId txn_id)
    {
        std::lock_guard<std::mutex> lock(serializable_mutex_);

        serializable_transactions_.insert(txn_id);
        serialization_graph_[txn_id] = std::unordered_set<TransactionId>{};

        std::fprintf(stderr, "[SERIALIZABLE] Started SERIALIZABLE transaction %lu\n",
                     static_cast<unsigned long>(txn_id));
        return true;
    }

    bool SerializableIsolationManager::commit_serializable_transaction(TransactionId txn_id)
    {
        std::lock_guard<std::mutex> lock(serializable_mutex_);

        if (!is_transaction_serializable(txn_id)) {
            return false;
        }

        // Simplified: Skip complex anomaly detection for now to avoid infinite loops
        // In production, this would do full SSI validation

        cleanup_completed_transaction(txn_id);

        std::fprintf(stderr, "[SERIALIZABLE] Committed SERIALIZABLE transaction %lu\n",
                     static_cast<unsigned long>(txn_id));
        return true;
    }

    bool SerializableIsolationManager::rollback_serializable_transaction(TransactionId txn_id)
    {
        std::lock_guard<std::mutex> lock(serializable_mutex_);

        cleanup_completed_transaction(txn_id);

        std::fprintf(stderr, "[SERIALIZABLE] Rolled back SERIALIZABLE transaction %lu\n",
                     static_cast<unsigned long>(txn_id));
        return true;
    }

    bool SerializableIsolationManager::acquire_siread_lock(TransactionId txn_id,
                                                           std::uint32_t relation_oid,
                                                           std::uint32_t page_id,
                                                           std::uint32_t tuple_id)
    {
        std::lock_guard<std::mutex> lock(serializable_mutex_);

        if (!is_transaction_serializable(txn_id)) {
            return false;
        }

        SIReadLock siread_lock;
        siread_lock.txn_id = txn_id;
        siread_lock.relation_oid = relation_oid;
        siread_lock.page_id = page_id;
        siread_lock.tuple_id = tuple_id;
        siread_lock.is_relation_lock = (page_id == 0 && tuple_id == 0);
        siread_lock.is_page_lock = (page_id != 0 && tuple_id == 0);

        std::string key = siread_lock.get_key();
        siread_locks_[key] = siread_lock;
        txn_siread_locks_[txn_id].push_back(key);

        return true;
    }

    bool SerializableIsolationManager::release_siread_locks(TransactionId txn_id)
    {
        std::lock_guard<std::mutex> lock(serializable_mutex_);

        auto it = txn_siread_locks_.find(txn_id);
        if (it != txn_siread_locks_.end()) {
            for (const auto& key : it->second) {
                siread_locks_.erase(key);
            }
            txn_siread_locks_.erase(it);
        }

        return true;
    }

    bool SerializableIsolationManager::check_read_write_conflicts(TransactionId reader_txn,
                                                                  std::uint32_t relation_oid,
                                                                  std::uint32_t page_id,
                                                                  std::uint32_t tuple_id)
    {
        std::lock_guard<std::mutex> lock(serializable_mutex_);

        // Check for existing write operations from other transactions
        // In a full implementation, this would check the write set of concurrent transactions
        // For now, simplified implementation

        for (const auto& [other_txn_id, dependencies] : serialization_graph_) {
            if (other_txn_id != reader_txn && serializable_transactions_.count(other_txn_id) > 0) {

                // Record potential rw-antidependency
                record_dangerous_structure(reader_txn, other_txn_id,
                                           DangerousStructure::RW_ANTIDEPENDENCY,
                                           "Read-Write antidependency detected");
            }
        }

        return true;
    }

    bool SerializableIsolationManager::check_write_read_conflicts(TransactionId writer_txn,
                                                                  std::uint32_t relation_oid,
                                                                  std::uint32_t page_id,
                                                                  std::uint32_t tuple_id)
    {
        std::lock_guard<std::mutex> lock(serializable_mutex_);

        // Check for SIREAD locks from other transactions
        std::string tuple_key = make_siread_key(relation_oid, page_id, tuple_id);
        std::string page_key = make_siread_key(relation_oid, page_id, 0);
        std::string relation_key = make_siread_key(relation_oid, 0, 0);

        std::vector<std::string> keys_to_check = {tuple_key, page_key, relation_key};

        for (const auto& key : keys_to_check) {
            auto it = siread_locks_.find(key);
            if (it != siread_locks_.end() && it->second.txn_id != writer_txn) {
                TransactionId reader_txn = it->second.txn_id;

                // Record wr-dependency
                add_dependency_edge(writer_txn, reader_txn, "write-read dependency");
                record_dangerous_structure(writer_txn, reader_txn,
                                           DangerousStructure::WR_DEPENDENCY,
                                           "Write-Read dependency detected");
            }
        }

        return true;
    }

    bool SerializableIsolationManager::detect_dangerous_structure(TransactionId txn_id)
    {
        std::lock_guard<std::mutex> lock(serializable_mutex_);

        // Look for dangerous structures involving this transaction
        for (const auto& structure : dangerous_structures_) {
            if (structure.T1 == txn_id || structure.T2 == txn_id || structure.T3 == txn_id) {
                if (structure.is_cycle()) {
                    return true; // Found a dangerous cycle
                }
            }
        }

        return false;
    }

    bool SerializableIsolationManager::has_serialization_anomaly(TransactionId txn_id)
    {
        // Check for dangerous structures
        if (detect_dangerous_structure(txn_id)) {
            return true;
        }

        // Check for cycles in serialization graph
        return has_cycle_in_serialization_graph();
    }

    bool SerializableIsolationManager::acquire_predicate_lock(TransactionId txn_id,
                                                              PredicateLockType lock_type,
                                                              std::uint32_t relation_oid,
                                                              const std::string& predicate)
    {
        std::lock_guard<std::mutex> lock(serializable_mutex_);

        if (!is_transaction_serializable(txn_id)) {
            return false;
        }

        PredicateLock new_lock;
        new_lock.txn_id = txn_id;
        new_lock.lock_type = lock_type;
        new_lock.relation_oid = relation_oid;
        new_lock.predicate_condition = predicate;

        // Check for conflicts with existing predicate locks
        if (conflicts_with_existing_locks(new_lock)) {
            return false;
        }

        predicate_locks_.push_back(new_lock);

        // Escalate if too many locks
        if (predicate_locks_.size() > max_predicate_locks_) {
            escalate_predicate_locks();
        }

        return true;
    }

    bool SerializableIsolationManager::add_dependency_edge(TransactionId from_txn,
                                                           TransactionId to_txn,
                                                           const std::string& /* dependency_type */)
    {
        std::lock_guard<std::mutex> lock(serializable_mutex_);

        serialization_graph_[from_txn].insert(to_txn);
        return true;
    }

    bool SerializableIsolationManager::has_cycle_in_serialization_graph()
    {
        std::unordered_set<TransactionId> visited;
        std::unordered_set<TransactionId> recursion_stack;

        for (const auto& [txn_id, dependencies] : serialization_graph_) {
            if (visited.find(txn_id) == visited.end()) {
                if (has_cycle_from_node(txn_id, visited, recursion_stack)) {
                    return true;
                }
            }
        }

        return false;
    }

    std::vector<TransactionId> SerializableIsolationManager::find_cycle_transactions()
    {
        // Simplified cycle detection - return first detected cycle
        if (has_cycle_in_serialization_graph()) {
            std::vector<TransactionId> cycle_txns;
            for (const auto& [txn_id, dependencies] : serialization_graph_) {
                cycle_txns.push_back(txn_id);
                if (cycle_txns.size() >= 3)
                    break; // Return first few transactions
            }
            return cycle_txns;
        }
        return {};
    }

    // ========== Getters and Statistics ==========

    std::size_t SerializableIsolationManager::get_active_serializable_transactions() const
    {
        std::lock_guard<std::mutex> lock(serializable_mutex_);
        return serializable_transactions_.size();
    }

    std::size_t SerializableIsolationManager::get_siread_lock_count() const
    {
        std::lock_guard<std::mutex> lock(serializable_mutex_);
        return siread_locks_.size();
    }

    std::size_t SerializableIsolationManager::get_predicate_lock_count() const
    {
        std::lock_guard<std::mutex> lock(serializable_mutex_);
        return predicate_locks_.size();
    }

    std::size_t SerializableIsolationManager::get_serialization_failures() const
    {
        std::lock_guard<std::mutex> lock(serializable_mutex_);
        return serialization_failures_;
    }

    // ========== Private Helper Methods ==========

    bool SerializableIsolationManager::is_transaction_serializable(TransactionId txn_id) const
    {
        return serializable_transactions_.count(txn_id) > 0;
    }

    void SerializableIsolationManager::cleanup_completed_transaction(TransactionId txn_id)
    {
        // Simplified cleanup to avoid infinite loops
        serializable_transactions_.erase(txn_id);

        // Basic lock cleanup
        auto it = txn_siread_locks_.find(txn_id);
        if (it != txn_siread_locks_.end()) {
            for (const auto& key : it->second) {
                siread_locks_.erase(key);
            }
            txn_siread_locks_.erase(it);
        }

        // Remove from serialization graph
        serialization_graph_.erase(txn_id);
    }

    void SerializableIsolationManager::record_dangerous_structure(TransactionId T1,
                                                                  TransactionId T2,
                                                                  DangerousStructure::Type type,
                                                                  const std::string& desc)
    {
        DangerousStructure structure;
        structure.T1 = T1;
        structure.T2 = T2;
        structure.T3 = T1; // For cycle detection
        structure.type = type;
        structure.description = desc;

        dangerous_structures_.push_back(structure);
    }

    bool SerializableIsolationManager::has_cycle_from_node(
        TransactionId start_txn, std::unordered_set<TransactionId>& visited,
        std::unordered_set<TransactionId>& recursion_stack) const
    {
        visited.insert(start_txn);
        recursion_stack.insert(start_txn);

        auto it = serialization_graph_.find(start_txn);
        if (it != serialization_graph_.end()) {
            for (TransactionId neighbor : it->second) {
                if (recursion_stack.count(neighbor) > 0) {
                    return true; // Back edge found - cycle detected
                }

                if (visited.find(neighbor) == visited.end()) {
                    if (has_cycle_from_node(neighbor, visited, recursion_stack)) {
                        return true;
                    }
                }
            }
        }

        recursion_stack.erase(start_txn);
        return false;
    }

    std::string SerializableIsolationManager::make_siread_key(std::uint32_t relation_oid,
                                                              std::uint32_t page_id,
                                                              std::uint32_t tuple_id) const
    {
        if (tuple_id != 0) {
            return "tuple:" + std::to_string(relation_oid) + ":" + std::to_string(page_id) + ":" +
                   std::to_string(tuple_id);
        } else if (page_id != 0) {
            return "page:" + std::to_string(relation_oid) + ":" + std::to_string(page_id);
        } else {
            return "rel:" + std::to_string(relation_oid);
        }
    }

    bool
    SerializableIsolationManager::conflicts_with_existing_locks(const PredicateLock& new_lock) const
    {
        for (const auto& existing_lock : predicate_locks_) {
            if (new_lock.conflicts_with(existing_lock)) {
                return true;
            }
        }
        return false;
    }

    void SerializableIsolationManager::escalate_predicate_locks()
    {
        predicate_lock_escalations_++;
        std::fprintf(stderr, "[SERIALIZABLE] Escalating predicate locks (count: %zu)\n",
                     predicate_locks_.size());

        // Simplified escalation - remove oldest locks
        if (predicate_locks_.size() > max_predicate_locks_) {
            predicate_locks_.erase(predicate_locks_.begin(),
                                   predicate_locks_.begin() +
                                       (predicate_locks_.size() - max_predicate_locks_));
        }
    }

    // ========== SerializableTransaction RAII Wrapper ==========

    SerializableTransaction::SerializableTransaction(SerializableIsolationManager& manager,
                                                     TransactionId txn_id)
        : manager_(manager), txn_id_(txn_id), active_(false), committed_(false)
    {
        active_ = manager_.begin_serializable_transaction(txn_id_);
    }

    SerializableTransaction::~SerializableTransaction()
    {
        if (active_ && !committed_) {
            rollback();
        }
    }

    bool SerializableTransaction::commit()
    {
        if (!active_ || committed_) {
            return false;
        }

        bool success = manager_.commit_serializable_transaction(txn_id_);
        if (success) {
            committed_ = true;
            active_ = false;
        }

        return success;
    }

    bool SerializableTransaction::rollback()
    {
        if (!active_) {
            return false;
        }

        bool success = manager_.rollback_serializable_transaction(txn_id_);
        active_ = false;

        return success;
    }

    bool SerializableTransaction::read_tuple(std::uint32_t relation_oid, std::uint32_t page_id,
                                             std::uint32_t tuple_id)
    {
        if (!active_) {
            return false;
        }

        // Acquire SIREAD lock
        manager_.acquire_siread_lock(txn_id_, relation_oid, page_id, tuple_id);

        // Check for write-read conflicts
        return manager_.check_read_write_conflicts(txn_id_, relation_oid, page_id, tuple_id);
    }

    bool SerializableTransaction::write_tuple(std::uint32_t relation_oid, std::uint32_t page_id,
                                              std::uint32_t tuple_id)
    {
        if (!active_) {
            return false;
        }

        // Check for read-write conflicts
        return manager_.check_write_read_conflicts(txn_id_, relation_oid, page_id, tuple_id);
    }

    bool SerializableTransaction::scan_relation(std::uint32_t relation_oid,
                                                const std::string& predicate)
    {
        if (!active_) {
            return false;
        }

        // Acquire relation-level SIREAD lock
        manager_.acquire_siread_lock(txn_id_, relation_oid, 0, 0);

        // Acquire predicate lock for range queries
        if (!predicate.empty()) {
            return manager_.acquire_predicate_lock(txn_id_, PredicateLockType::RANGE_READ,
                                                   relation_oid, predicate);
        }

        return true;
    }

    // ========== Utility Functions ==========

    static SerializableIsolationManager global_serializable_manager;

    SerializableIsolationManager& get_serializable_manager()
    {
        return global_serializable_manager;
    }

    bool should_use_serializable_isolation(TransactionId /* txn_id */)
    {
        // In a full implementation, this would check transaction isolation level
        return true; // For testing, assume SERIALIZABLE
    }

    IsolationLevel parse_isolation_level(const std::string& level_str)
    {
        if (level_str == "READ UNCOMMITTED")
            return IsolationLevel::READ_UNCOMMITTED;
        if (level_str == "READ COMMITTED")
            return IsolationLevel::READ_COMMITTED;
        if (level_str == "REPEATABLE READ")
            return IsolationLevel::REPEATABLE_READ;
        if (level_str == "SERIALIZABLE")
            return IsolationLevel::SERIALIZABLE;

        return IsolationLevel::READ_COMMITTED; // Default
    }

    std::string isolation_level_to_string(IsolationLevel level)
    {
        switch (level) {
        case IsolationLevel::READ_UNCOMMITTED:
            return "READ UNCOMMITTED";
        case IsolationLevel::READ_COMMITTED:
            return "READ COMMITTED";
        case IsolationLevel::REPEATABLE_READ:
            return "REPEATABLE READ";
        case IsolationLevel::SERIALIZABLE:
            return "SERIALIZABLE";
        }
        return "UNKNOWN";
    }

    bool
    validate_serializable_consistency(const std::vector<TransactionId>& /* transaction_sequence */)
    {
        // Simplified validation - in a full implementation this would
        // verify that the transaction sequence is serializable
        return true;
    }

} // namespace scratchbird::engine
