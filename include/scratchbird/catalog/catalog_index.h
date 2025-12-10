// =================================================================================================
// ScratchBird Database Engine
// Copyright (C) 2025 ScratchBird Development Team
// =================================================================================================
//
// P3-12: Catalog B-Tree Indexes
//
// Provides fast B-tree indexed lookup for system catalog tables.
// Replaces linear scans with O(log n) lookups for:
// - Table name lookups
// - Column lookups by table_id
// - Index name lookups
// - User and role lookups
//
// Impact: 100-1000x faster catalog lookups for large databases
//
// November 25, 2025

#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/gpid.h"
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <shared_mutex>
#include <optional>
#include <cstdint>
#include <array>
#include <unordered_map>

namespace scratchbird::catalog {

using core::ErrorContext;

// Forward declarations
class CatalogManager;
class TableEntry;
class ColumnEntry;
class IndexEntry;
class UserEntry;
class RoleEntry;

// ============================================================================
// Catalog Index Types
// ============================================================================

// Index type enumeration
enum class CatalogIndexType : uint8_t {
    TABLES_BY_NAME,          // sb_tables(table_name) -> table_id
    TABLES_BY_SCHEMA,        // sb_tables(schema_id) -> table_ids
    COLUMNS_BY_TABLE,        // sb_columns(table_id) -> column_ids
    COLUMNS_BY_NAME,         // sb_columns(table_id, column_name) -> column_id
    INDEXES_BY_NAME,         // sb_indexes(index_name) -> index_id
    INDEXES_BY_TABLE,        // sb_indexes(table_id) -> index_ids
    USERS_BY_NAME,           // sb_users(username) -> user_id
    ROLES_BY_NAME,           // sb_roles(role_name) -> role_id
    SEQUENCES_BY_NAME,       // sb_sequences(sequence_name) -> sequence_id
    VIEWS_BY_NAME,           // sb_views(view_name) -> view_id
    FUNCTIONS_BY_NAME,       // sb_functions(function_name) -> function_ids
    TRIGGERS_BY_TABLE,       // sb_triggers(table_id) -> trigger_ids
};

// Index statistics
struct CatalogIndexStats {
    uint64_t lookups = 0;             // Total lookups performed
    uint64_t inserts = 0;             // Total inserts
    uint64_t deletes = 0;             // Total deletes
    uint64_t updates = 0;             // Total updates
    uint64_t scan_count = 0;          // Full scans (fallback)
    uint64_t hit_count = 0;           // Successful index lookups
    uint64_t miss_count = 0;          // Index lookup misses
    size_t node_count = 0;            // Number of B-tree nodes
    size_t entry_count = 0;           // Total entries indexed
    size_t memory_bytes = 0;          // Memory usage

    double hitRatio() const {
        uint64_t total = hit_count + miss_count;
        return total > 0 ? static_cast<double>(hit_count) / total : 0.0;
    }
};

// ============================================================================
// B-Tree Index Entry (generic key-value)
// ============================================================================

// Key types supported by catalog indexes
struct CatalogKey {
    std::string string_key;           // For name lookups
    uint32_t id_key = 0;              // For ID lookups
    uint32_t secondary_id = 0;        // For composite keys (e.g., table_id + column_name)
    bool has_secondary = false;

    CatalogKey() = default;
    explicit CatalogKey(std::string_view name) : string_key(name) {}
    explicit CatalogKey(uint32_t id) : id_key(id) {}
    CatalogKey(uint32_t primary, std::string_view secondary)
        : string_key(secondary), id_key(primary), has_secondary(true) {}
    CatalogKey(uint32_t primary, uint32_t secondary)
        : id_key(primary), secondary_id(secondary), has_secondary(true) {}

    // Comparison operators
    bool operator<(const CatalogKey& other) const;
    bool operator>(const CatalogKey& other) const;
    bool operator==(const CatalogKey& other) const;
    bool operator!=(const CatalogKey& other) const { return !(*this == other); }
};

// ============================================================================
// B-Tree Node
// ============================================================================

template<typename KeyType = CatalogKey, typename ValueType = uint32_t>
class CatalogBTreeNode {
public:
    static constexpr size_t ORDER = 64;  // B-tree order (max children)
    static constexpr size_t MIN_KEYS = ORDER / 2 - 1;
    static constexpr size_t MAX_KEYS = ORDER - 1;

    bool is_leaf = true;
    std::vector<KeyType> keys;
    std::vector<ValueType> values;  // Only for leaf nodes
    std::vector<std::unique_ptr<CatalogBTreeNode>> children;

    CatalogBTreeNode() = default;

    bool isFull() const { return keys.size() >= MAX_KEYS; }
    bool isUnderflow() const { return keys.size() < MIN_KEYS; }

    // Search for key position
    size_t findKeyPos(const KeyType& key) const;

    // Check if node contains key
    bool containsKey(const KeyType& key) const;
};

// ============================================================================
// Catalog B-Tree Index
// ============================================================================

template<typename KeyType = CatalogKey, typename ValueType = uint32_t>
class CatalogBTree {
public:
    CatalogBTree() = default;
    ~CatalogBTree() = default;

    // Insert a key-value pair
    void insert(const KeyType& key, const ValueType& value);

    // Remove a key
    bool remove(const KeyType& key);

    // Search for a value by key
    std::optional<ValueType> find(const KeyType& key) const;

    // Search for all values matching a prefix (for range queries)
    std::vector<ValueType> findRange(const KeyType& start, const KeyType& end) const;

    // Check if key exists
    bool contains(const KeyType& key) const;

    // Get all values (for iteration)
    std::vector<std::pair<KeyType, ValueType>> getAll() const;

    // Statistics
    size_t size() const { return entry_count_; }
    size_t nodeCount() const { return node_count_; }
    size_t memoryUsage() const;

    // Clear all entries
    void clear();

private:
    std::unique_ptr<CatalogBTreeNode<KeyType, ValueType>> root_;
    size_t entry_count_ = 0;
    size_t node_count_ = 0;

    void insertNonFull(CatalogBTreeNode<KeyType, ValueType>* node,
                       const KeyType& key, const ValueType& value);
    void splitChild(CatalogBTreeNode<KeyType, ValueType>* parent, size_t index);
    void collectRange(const CatalogBTreeNode<KeyType, ValueType>* node,
                      const KeyType& start, const KeyType& end,
                      std::vector<ValueType>& results) const;
    void collectAll(const CatalogBTreeNode<KeyType, ValueType>* node,
                    std::vector<std::pair<KeyType, ValueType>>& results) const;
};

// ============================================================================
// Catalog Index Manager
// ============================================================================

class CatalogIndexManager {
public:
    static CatalogIndexManager& getInstance();

    // Initialize catalog indexes
    core::Status initialize(ErrorContext* ctx = nullptr);

    // Rebuild all indexes from catalog data
    core::Status rebuildAllIndexes(ErrorContext* ctx = nullptr);

    // Rebuild a specific index
    core::Status rebuildIndex(CatalogIndexType type, ErrorContext* ctx = nullptr);

    // ========================================================================
    // Table Lookups
    // ========================================================================

    // Find table by name
    std::optional<uint32_t> findTableByName(std::string_view table_name) const;

    // Find tables by schema
    std::vector<uint32_t> findTablesBySchema(uint32_t schema_id) const;

    // Index maintenance for tables
    void indexTable(uint32_t table_id, std::string_view table_name, uint32_t schema_id);
    void unindexTable(uint32_t table_id, std::string_view table_name, uint32_t schema_id);

    // ========================================================================
    // Column Lookups
    // ========================================================================

    // Find column by table_id and column_name
    std::optional<uint32_t> findColumn(uint32_t table_id, std::string_view column_name) const;

    // Find all columns for a table
    std::vector<uint32_t> findColumnsByTable(uint32_t table_id) const;

    // Index maintenance for columns
    void indexColumn(uint32_t column_id, uint32_t table_id, std::string_view column_name);
    void unindexColumn(uint32_t column_id, uint32_t table_id, std::string_view column_name);

    // ========================================================================
    // Index Lookups
    // ========================================================================

    // Find index by name
    std::optional<uint32_t> findIndexByName(std::string_view index_name) const;

    // Find indexes by table
    std::vector<uint32_t> findIndexesByTable(uint32_t table_id) const;

    // Index maintenance
    void indexIndex(uint32_t index_id, std::string_view index_name, uint32_t table_id);
    void unindexIndex(uint32_t index_id, std::string_view index_name, uint32_t table_id);

    // ========================================================================
    // User and Role Lookups
    // ========================================================================

    // Find user by name
    std::optional<uint32_t> findUserByName(std::string_view username) const;

    // Find role by name
    std::optional<uint32_t> findRoleByName(std::string_view role_name) const;

    // Index maintenance for users/roles
    void indexUser(uint32_t user_id, std::string_view username);
    void unindexUser(uint32_t user_id, std::string_view username);
    void indexRole(uint32_t role_id, std::string_view role_name);
    void unindexRole(uint32_t role_id, std::string_view role_name);

    // ========================================================================
    // Sequence Lookups
    // ========================================================================

    std::optional<uint32_t> findSequenceByName(std::string_view seq_name) const;
    void indexSequence(uint32_t seq_id, std::string_view seq_name);
    void unindexSequence(uint32_t seq_id, std::string_view seq_name);

    // ========================================================================
    // View Lookups
    // ========================================================================

    std::optional<uint32_t> findViewByName(std::string_view view_name) const;
    void indexView(uint32_t view_id, std::string_view view_name);
    void unindexView(uint32_t view_id, std::string_view view_name);

    // ========================================================================
    // Function Lookups (multiple overloads possible)
    // ========================================================================

    std::vector<uint32_t> findFunctionsByName(std::string_view func_name) const;
    void indexFunction(uint32_t func_id, std::string_view func_name);
    void unindexFunction(uint32_t func_id, std::string_view func_name);

    // ========================================================================
    // Trigger Lookups
    // ========================================================================

    std::vector<uint32_t> findTriggersByTable(uint32_t table_id) const;
    void indexTrigger(uint32_t trigger_id, uint32_t table_id);
    void unindexTrigger(uint32_t trigger_id, uint32_t table_id);

    // ========================================================================
    // Statistics
    // ========================================================================

    CatalogIndexStats getStats(CatalogIndexType type) const;
    CatalogIndexStats getAggregateStats() const;

    // Clear statistics
    void resetStats();

private:
    CatalogIndexManager() = default;

    // Individual indexes
    CatalogBTree<CatalogKey, uint32_t> tables_by_name_;
    CatalogBTree<CatalogKey, uint32_t> tables_by_schema_;  // Multi-valued
    CatalogBTree<CatalogKey, uint32_t> columns_by_name_;   // Composite key
    CatalogBTree<CatalogKey, uint32_t> columns_by_table_;  // Multi-valued
    CatalogBTree<CatalogKey, uint32_t> indexes_by_name_;
    CatalogBTree<CatalogKey, uint32_t> indexes_by_table_;  // Multi-valued
    CatalogBTree<CatalogKey, uint32_t> users_by_name_;
    CatalogBTree<CatalogKey, uint32_t> roles_by_name_;
    CatalogBTree<CatalogKey, uint32_t> sequences_by_name_;
    CatalogBTree<CatalogKey, uint32_t> views_by_name_;
    CatalogBTree<CatalogKey, uint32_t> functions_by_name_;  // Multi-valued
    CatalogBTree<CatalogKey, uint32_t> triggers_by_table_;  // Multi-valued

    // Statistics per index
    mutable std::array<CatalogIndexStats, 12> stats_;

    // Read-write lock for thread safety
    mutable std::shared_mutex mutex_;

    // Multi-value indexes (one key -> multiple values)
    std::unordered_map<uint32_t, std::vector<uint32_t>> tables_by_schema_multi_;
    std::unordered_map<uint32_t, std::vector<uint32_t>> columns_by_table_multi_;
    std::unordered_map<uint32_t, std::vector<uint32_t>> indexes_by_table_multi_;
    std::unordered_map<std::string, std::vector<uint32_t>> functions_by_name_multi_;
    std::unordered_map<uint32_t, std::vector<uint32_t>> triggers_by_table_multi_;
};

// ============================================================================
// Inline convenience functions
// ============================================================================

// Quick lookup functions that use the singleton
inline std::optional<uint32_t> lookupTableByName(std::string_view name) {
    return CatalogIndexManager::getInstance().findTableByName(name);
}

inline std::optional<uint32_t> lookupColumn(uint32_t table_id, std::string_view name) {
    return CatalogIndexManager::getInstance().findColumn(table_id, name);
}

inline std::optional<uint32_t> lookupIndexByName(std::string_view name) {
    return CatalogIndexManager::getInstance().findIndexByName(name);
}

inline std::optional<uint32_t> lookupUserByName(std::string_view name) {
    return CatalogIndexManager::getInstance().findUserByName(name);
}

} // namespace scratchbird::catalog
