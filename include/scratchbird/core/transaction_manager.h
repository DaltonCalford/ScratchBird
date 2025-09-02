#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/ondisk.h"
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <vector>

namespace scratchbird {
namespace core {

// Forward declarations
class Database;
class BufferPool;
class PageManager;
struct ErrorContext;

// Transaction states
enum class TransactionState : uint8_t {
    ACTIVE = 0,
    COMMITTED = 1,
    ABORTED = 2,
    PREPARED = 3,  // For future 2PC support
};

// Transaction information
struct TransactionInfo {
    uint64_t xid;
    TransactionState state;
    uint64_t start_time;  // Microseconds since epoch
    uint64_t end_time;    // Microseconds since epoch (0 if active)
};

// Transaction Inventory Page (TIP) format
// TIP pages track transaction states for MVCC visibility
#pragma pack(push, 1)
struct TIPPageHeader {
    PageHeader page_header;      // Standard page header
    uint64_t min_xid;           // Minimum XID in this page
    uint64_t max_xid;           // Maximum XID in this page
    uint32_t num_transactions;   // Number of transactions in this page
    uint32_t next_tip_page;     // Next TIP page ID (0 if last)
    uint8_t reserved[20];       // Reserved for future use
};

// Each transaction entry in TIP page
struct TIPEntry {
    uint64_t xid;               // Transaction ID
    uint8_t state;              // TransactionState
    uint8_t flags;              // Reserved flags
    uint16_t reserved;          // Alignment padding
    uint64_t commit_time;       // Commit/abort timestamp
};
#pragma pack(pop)

// Transaction Manager - handles transaction lifecycle and visibility
class TransactionManager {
public:
    explicit TransactionManager(Database* db);
    ~TransactionManager();
    
    // Initialize transaction subsystem
    Status initialize(ErrorContext* ctx = nullptr);
    
    // Load existing transaction state from disk
    Status load(ErrorContext* ctx = nullptr);
    
    // Begin a new transaction
    Status begin_transaction(uint64_t& xid_out, ErrorContext* ctx = nullptr);
    
    // Commit a transaction
    Status commit_transaction(uint64_t xid, ErrorContext* ctx = nullptr);
    
    // Rollback a transaction
    Status rollback_transaction(uint64_t xid, ErrorContext* ctx = nullptr);
    
    // Get transaction state
    Status get_transaction_state(uint64_t xid, TransactionState& state_out,
                                ErrorContext* ctx = nullptr);
    
    // Check if a transaction is visible to another transaction
    bool is_transaction_visible(uint64_t xid, uint64_t snapshot_xid);
    
    // Get current transaction ID (for read-only operations)
    uint64_t get_current_xid() const { return next_xid_; }
    
    // Get active transaction (single connection for now)
    uint64_t get_active_xid() const { return active_xid_; }
    
    // Snapshot isolation support (future)
    struct Snapshot {
        uint64_t xmin;  // Oldest active XID
        uint64_t xmax;  // Next XID to be assigned
        std::vector<uint64_t> active_xids;  // Active XIDs at snapshot time
    };
    
    // Get current snapshot (for future MVCC)
    Status get_snapshot(Snapshot& snapshot_out, ErrorContext* ctx = nullptr);
    
private:
    Database* db_;
    BufferPool* buffer_pool_;
    PageManager* page_manager_;
    
    // Transaction state
    uint64_t next_xid_ = 100;       // Next XID to allocate (start at 100)
    uint64_t active_xid_ = 0;       // Currently active transaction (single connection)
    uint32_t tip_root_page_ = 0;    // Root TIP page ID
    
    // In-memory cache of recent transactions
    std::unordered_map<uint64_t, TransactionState> transaction_cache_;
    mutable std::mutex mutex_;       // Thread safety for future
    
    // Special transaction IDs
    static constexpr uint64_t INVALID_XID = 0;
    static constexpr uint64_t BOOTSTRAP_XID = 1;
    static constexpr uint64_t FROZEN_XID = 2;
    
    // TIP page management
    static constexpr uint32_t TIP_ENTRIES_PER_PAGE = 
        (8192 - sizeof(TIPPageHeader)) / sizeof(TIPEntry);  // For 8KB pages
    
    // Helper methods
    Status allocate_tip_page(uint32_t& page_id_out, ErrorContext* ctx);
    Status write_tip_entry(uint64_t xid, TransactionState state, ErrorContext* ctx);
    Status find_tip_entry(uint64_t xid, TIPEntry& entry_out, ErrorContext* ctx);
    Status flush_transaction_state(ErrorContext* ctx);
};

} // namespace core
} // namespace scratchbird