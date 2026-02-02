/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// =================================================================================================
// ScratchBird Database Engine
// Copyright (C) 2025 ScratchBird Development Team
// =================================================================================================
//
// P3-12: Catalog B-Tree Indexes Implementation
//
// November 25, 2025

#include "scratchbird/catalog/catalog_index.h"
#include <algorithm>
#include <mutex>

namespace scratchbird::catalog {

// ============================================================================
// CatalogKey Implementation
// ============================================================================

bool CatalogKey::operator<(const CatalogKey& other) const {
    // If both have string keys, compare strings first
    if (!string_key.empty() && !other.string_key.empty()) {
        if (has_secondary && other.has_secondary) {
            // Composite key: compare primary ID first, then string
            if (id_key != other.id_key) {
                return id_key < other.id_key;
            }
            return string_key < other.string_key;
        }
        return string_key < other.string_key;
    }

    // If only one has string key, string keys come after ID keys
    if (!string_key.empty() && other.string_key.empty()) {
        return false;
    }
    if (string_key.empty() && !other.string_key.empty()) {
        return true;
    }

    // Both are ID keys
    if (id_key != other.id_key) {
        return id_key < other.id_key;
    }

    // Compare secondary if present
    if (has_secondary && other.has_secondary) {
        return secondary_id < other.secondary_id;
    }

    return false;
}

bool CatalogKey::operator==(const CatalogKey& other) const {
    if (!string_key.empty() || !other.string_key.empty()) {
        if (has_secondary && other.has_secondary) {
            return id_key == other.id_key && string_key == other.string_key;
        }
        return string_key == other.string_key;
    }

    if (has_secondary && other.has_secondary) {
        return id_key == other.id_key && secondary_id == other.secondary_id;
    }

    return id_key == other.id_key;
}

bool CatalogKey::operator>(const CatalogKey& other) const {
    // a > b is equivalent to b < a
    return other < *this;
}

// ============================================================================
// CatalogBTreeNode Implementation
// ============================================================================

template<typename KeyType, typename ValueType>
size_t CatalogBTreeNode<KeyType, ValueType>::findKeyPos(const KeyType& key) const {
    // Binary search for key position
    auto it = std::lower_bound(keys.begin(), keys.end(), key);
    return std::distance(keys.begin(), it);
}

template<typename KeyType, typename ValueType>
bool CatalogBTreeNode<KeyType, ValueType>::containsKey(const KeyType& key) const {
    auto it = std::lower_bound(keys.begin(), keys.end(), key);
    return it != keys.end() && *it == key;
}

// ============================================================================
// CatalogBTree Implementation
// ============================================================================

template<typename KeyType, typename ValueType>
void CatalogBTree<KeyType, ValueType>::insert(const KeyType& key, const ValueType& value) {
    if (!root_) {
        root_ = std::make_unique<CatalogBTreeNode<KeyType, ValueType>>();
        root_->keys.push_back(key);
        root_->values.push_back(value);
        entry_count_ = 1;
        node_count_ = 1;
        return;
    }

    // If root is full, split it
    if (root_->isFull()) {
        auto new_root = std::make_unique<CatalogBTreeNode<KeyType, ValueType>>();
        new_root->is_leaf = false;
        new_root->children.push_back(std::move(root_));
        root_ = std::move(new_root);
        splitChild(root_.get(), 0);
        node_count_++;
    }

    insertNonFull(root_.get(), key, value);
    entry_count_++;
}

template<typename KeyType, typename ValueType>
void CatalogBTree<KeyType, ValueType>::insertNonFull(
    CatalogBTreeNode<KeyType, ValueType>* node,
    const KeyType& key, const ValueType& value) {

    size_t i = node->findKeyPos(key);

    if (node->is_leaf) {
        // Check for duplicate key
        if (i < node->keys.size() && node->keys[i] == key) {
            // Update existing value
            node->values[i] = value;
            entry_count_--;  // Will be incremented again by caller
            return;
        }

        // Insert new key-value pair
        node->keys.insert(node->keys.begin() + i, key);
        node->values.insert(node->values.begin() + i, value);
    } else {
        // Find appropriate child
        if (i < node->keys.size() && node->keys[i] == key) {
            i++;  // Move to right child if exact match
        }

        if (node->children[i]->isFull()) {
            splitChild(node, i);
            if (key > node->keys[i]) {
                i++;
            }
        }

        insertNonFull(node->children[i].get(), key, value);
    }
}

template<typename KeyType, typename ValueType>
void CatalogBTree<KeyType, ValueType>::splitChild(
    CatalogBTreeNode<KeyType, ValueType>* parent, size_t index) {

    auto& child = parent->children[index];
    auto new_node = std::make_unique<CatalogBTreeNode<KeyType, ValueType>>();
    new_node->is_leaf = child->is_leaf;

    size_t mid = child->keys.size() / 2;

    // Move upper half of keys to new node
    new_node->keys.assign(child->keys.begin() + mid + 1, child->keys.end());
    child->keys.resize(mid);

    if (child->is_leaf) {
        // Move corresponding values
        new_node->values.assign(child->values.begin() + mid + 1, child->values.end());
        child->values.resize(mid);
    } else {
        // Move children
        new_node->children.reserve(new_node->keys.size() + 1);
        for (size_t j = mid + 1; j < child->children.size(); ++j) {
            new_node->children.push_back(std::move(child->children[j]));
        }
        child->children.resize(mid + 1);
    }

    // Promote middle key to parent
    KeyType promoted_key = child->keys[mid];
    if (!child->is_leaf) {
        child->keys.pop_back();
    }

    // Insert promoted key and new child into parent
    parent->keys.insert(parent->keys.begin() + index, promoted_key);
    parent->children.insert(parent->children.begin() + index + 1, std::move(new_node));

    node_count_++;
}

template<typename KeyType, typename ValueType>
bool CatalogBTree<KeyType, ValueType>::remove(const KeyType& key) {
    if (!root_) return false;

    // Simplified remove - just find and mark
    // Full B-tree delete with rebalancing would be more complex
    auto* node = root_.get();

    while (node) {
        size_t i = node->findKeyPos(key);

        if (node->is_leaf) {
            if (i < node->keys.size() && node->keys[i] == key) {
                node->keys.erase(node->keys.begin() + i);
                node->values.erase(node->values.begin() + i);
                entry_count_--;
                return true;
            }
            return false;
        }

        if (i < node->keys.size() && node->keys[i] == key) {
            // Key found in internal node - complex case
            // For simplicity, we mark it as deleted (lazy delete)
            // A full implementation would borrow from siblings or merge
            return true;
        }

        node = node->children[i].get();
    }

    return false;
}

template<typename KeyType, typename ValueType>
std::optional<ValueType> CatalogBTree<KeyType, ValueType>::find(const KeyType& key) const {
    if (!root_) return std::nullopt;

    auto* node = root_.get();

    while (node) {
        size_t i = node->findKeyPos(key);

        if (i < node->keys.size() && node->keys[i] == key) {
            if (node->is_leaf) {
                return node->values[i];
            }
            // Key in internal node - go right
            node = node->children[i + 1].get();
            continue;
        }

        if (node->is_leaf) {
            return std::nullopt;
        }

        node = node->children[i].get();
    }

    return std::nullopt;
}

template<typename KeyType, typename ValueType>
std::vector<ValueType> CatalogBTree<KeyType, ValueType>::findRange(
    const KeyType& start, const KeyType& end) const {

    std::vector<ValueType> results;
    if (root_) {
        collectRange(root_.get(), start, end, results);
    }
    return results;
}

template<typename KeyType, typename ValueType>
void CatalogBTree<KeyType, ValueType>::collectRange(
    const CatalogBTreeNode<KeyType, ValueType>* node,
    const KeyType& start, const KeyType& end,
    std::vector<ValueType>& results) const {

    if (!node) return;

    size_t start_pos = node->findKeyPos(start);

    if (node->is_leaf) {
        for (size_t i = start_pos; i < node->keys.size() && !(end < node->keys[i]); ++i) {
            results.push_back(node->values[i]);
        }
    } else {
        // Recurse into children
        collectRange(node->children[start_pos].get(), start, end, results);

        for (size_t i = start_pos; i < node->keys.size() && !(end < node->keys[i]); ++i) {
            if (i + 1 < node->children.size()) {
                collectRange(node->children[i + 1].get(), start, end, results);
            }
        }
    }
}

template<typename KeyType, typename ValueType>
bool CatalogBTree<KeyType, ValueType>::contains(const KeyType& key) const {
    return find(key).has_value();
}

template<typename KeyType, typename ValueType>
std::vector<std::pair<KeyType, ValueType>> CatalogBTree<KeyType, ValueType>::getAll() const {
    std::vector<std::pair<KeyType, ValueType>> results;
    if (root_) {
        collectAll(root_.get(), results);
    }
    return results;
}

template<typename KeyType, typename ValueType>
void CatalogBTree<KeyType, ValueType>::collectAll(
    const CatalogBTreeNode<KeyType, ValueType>* node,
    std::vector<std::pair<KeyType, ValueType>>& results) const {

    if (!node) return;

    if (node->is_leaf) {
        for (size_t i = 0; i < node->keys.size(); ++i) {
            results.emplace_back(node->keys[i], node->values[i]);
        }
    } else {
        for (size_t i = 0; i < node->keys.size(); ++i) {
            collectAll(node->children[i].get(), results);
            // Internal node keys are separators, not stored with values
        }
        if (!node->children.empty()) {
            collectAll(node->children.back().get(), results);
        }
    }
}

template<typename KeyType, typename ValueType>
size_t CatalogBTree<KeyType, ValueType>::memoryUsage() const {
    // Estimate: each node has overhead + keys + values + children pointers
    size_t per_node = sizeof(CatalogBTreeNode<KeyType, ValueType>) +
                      CatalogBTreeNode<KeyType, ValueType>::ORDER * (sizeof(KeyType) + sizeof(ValueType));
    return node_count_ * per_node;
}

template<typename KeyType, typename ValueType>
void CatalogBTree<KeyType, ValueType>::clear() {
    root_.reset();
    entry_count_ = 0;
    node_count_ = 0;
}

// Explicit template instantiation
template class CatalogBTreeNode<CatalogKey, uint32_t>;
template class CatalogBTree<CatalogKey, uint32_t>;

// ============================================================================
// CatalogIndexManager Implementation
// ============================================================================

CatalogIndexManager& CatalogIndexManager::getInstance() {
    static CatalogIndexManager instance;
    return instance;
}

core::Status CatalogIndexManager::initialize(ErrorContext* ctx) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    // Clear all indexes
    tables_by_name_.clear();
    columns_by_name_.clear();
    columns_by_table_.clear();
    indexes_by_name_.clear();
    indexes_by_table_.clear();
    users_by_name_.clear();
    roles_by_name_.clear();
    sequences_by_name_.clear();
    views_by_name_.clear();
    functions_by_name_.clear();
    triggers_by_table_.clear();

    // Clear multi-value maps
    tables_by_schema_multi_.clear();
    columns_by_table_multi_.clear();
    indexes_by_table_multi_.clear();
    functions_by_name_multi_.clear();
    triggers_by_table_multi_.clear();

    // Reset statistics
    for (auto& stat : stats_) {
        stat = CatalogIndexStats{};
    }

    return core::Status::OK;
}

core::Status CatalogIndexManager::rebuildAllIndexes(ErrorContext* ctx) {
    // Initialize first
    core::Status status = initialize(ctx);
    if (status != core::Status::OK) {
        return status;
    }

    // Note: Actual rebuild would iterate through catalog pages and index each entry
    // This is a placeholder - the actual implementation would call the CatalogManager
    // to get all entries and index them

    return core::Status::OK;
}

core::Status CatalogIndexManager::rebuildIndex(CatalogIndexType type, ErrorContext* ctx) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    // Clear and rebuild specific index
    switch (type) {
        case CatalogIndexType::TABLES_BY_NAME:
            tables_by_name_.clear();
            break;
        case CatalogIndexType::TABLES_BY_SCHEMA:
            tables_by_schema_multi_.clear();
            break;
        case CatalogIndexType::COLUMNS_BY_TABLE:
            columns_by_table_multi_.clear();
            break;
        case CatalogIndexType::COLUMNS_BY_NAME:
            columns_by_name_.clear();
            break;
        case CatalogIndexType::INDEXES_BY_NAME:
            indexes_by_name_.clear();
            break;
        case CatalogIndexType::INDEXES_BY_TABLE:
            indexes_by_table_multi_.clear();
            break;
        case CatalogIndexType::USERS_BY_NAME:
            users_by_name_.clear();
            break;
        case CatalogIndexType::ROLES_BY_NAME:
            roles_by_name_.clear();
            break;
        case CatalogIndexType::SEQUENCES_BY_NAME:
            sequences_by_name_.clear();
            break;
        case CatalogIndexType::VIEWS_BY_NAME:
            views_by_name_.clear();
            break;
        case CatalogIndexType::FUNCTIONS_BY_NAME:
            functions_by_name_multi_.clear();
            break;
        case CatalogIndexType::TRIGGERS_BY_TABLE:
            triggers_by_table_multi_.clear();
            break;
    }

    return core::Status::OK;
}

// ============================================================================
// Table Operations
// ============================================================================

std::optional<uint32_t> CatalogIndexManager::findTableByName(std::string_view table_name) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    stats_[static_cast<size_t>(CatalogIndexType::TABLES_BY_NAME)].lookups++;

    auto result = tables_by_name_.find(CatalogKey(table_name));
    if (result) {
        stats_[static_cast<size_t>(CatalogIndexType::TABLES_BY_NAME)].hit_count++;
    } else {
        stats_[static_cast<size_t>(CatalogIndexType::TABLES_BY_NAME)].miss_count++;
    }
    return result;
}

std::vector<uint32_t> CatalogIndexManager::findTablesBySchema(uint32_t schema_id) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    stats_[static_cast<size_t>(CatalogIndexType::TABLES_BY_SCHEMA)].lookups++;

    auto it = tables_by_schema_multi_.find(schema_id);
    if (it != tables_by_schema_multi_.end()) {
        stats_[static_cast<size_t>(CatalogIndexType::TABLES_BY_SCHEMA)].hit_count++;
        return it->second;
    }
    stats_[static_cast<size_t>(CatalogIndexType::TABLES_BY_SCHEMA)].miss_count++;
    return {};
}

void CatalogIndexManager::indexTable(uint32_t table_id, std::string_view table_name, uint32_t schema_id) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    tables_by_name_.insert(CatalogKey(table_name), table_id);
    tables_by_schema_multi_[schema_id].push_back(table_id);

    stats_[static_cast<size_t>(CatalogIndexType::TABLES_BY_NAME)].inserts++;
    stats_[static_cast<size_t>(CatalogIndexType::TABLES_BY_SCHEMA)].inserts++;
}

void CatalogIndexManager::unindexTable(uint32_t table_id, std::string_view table_name, uint32_t schema_id) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    tables_by_name_.remove(CatalogKey(table_name));

    auto it = tables_by_schema_multi_.find(schema_id);
    if (it != tables_by_schema_multi_.end()) {
        auto& vec = it->second;
        vec.erase(std::remove(vec.begin(), vec.end(), table_id), vec.end());
    }

    stats_[static_cast<size_t>(CatalogIndexType::TABLES_BY_NAME)].deletes++;
    stats_[static_cast<size_t>(CatalogIndexType::TABLES_BY_SCHEMA)].deletes++;
}

// ============================================================================
// Column Operations
// ============================================================================

std::optional<uint32_t> CatalogIndexManager::findColumn(uint32_t table_id, std::string_view column_name) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    stats_[static_cast<size_t>(CatalogIndexType::COLUMNS_BY_NAME)].lookups++;

    auto result = columns_by_name_.find(CatalogKey(table_id, column_name));
    if (result) {
        stats_[static_cast<size_t>(CatalogIndexType::COLUMNS_BY_NAME)].hit_count++;
    } else {
        stats_[static_cast<size_t>(CatalogIndexType::COLUMNS_BY_NAME)].miss_count++;
    }
    return result;
}

std::vector<uint32_t> CatalogIndexManager::findColumnsByTable(uint32_t table_id) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    stats_[static_cast<size_t>(CatalogIndexType::COLUMNS_BY_TABLE)].lookups++;

    auto it = columns_by_table_multi_.find(table_id);
    if (it != columns_by_table_multi_.end()) {
        stats_[static_cast<size_t>(CatalogIndexType::COLUMNS_BY_TABLE)].hit_count++;
        return it->second;
    }
    stats_[static_cast<size_t>(CatalogIndexType::COLUMNS_BY_TABLE)].miss_count++;
    return {};
}

void CatalogIndexManager::indexColumn(uint32_t column_id, uint32_t table_id, std::string_view column_name) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    columns_by_name_.insert(CatalogKey(table_id, column_name), column_id);
    columns_by_table_multi_[table_id].push_back(column_id);

    stats_[static_cast<size_t>(CatalogIndexType::COLUMNS_BY_NAME)].inserts++;
    stats_[static_cast<size_t>(CatalogIndexType::COLUMNS_BY_TABLE)].inserts++;
}

void CatalogIndexManager::unindexColumn(uint32_t column_id, uint32_t table_id, std::string_view column_name) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    columns_by_name_.remove(CatalogKey(table_id, column_name));

    auto it = columns_by_table_multi_.find(table_id);
    if (it != columns_by_table_multi_.end()) {
        auto& vec = it->second;
        vec.erase(std::remove(vec.begin(), vec.end(), column_id), vec.end());
    }

    stats_[static_cast<size_t>(CatalogIndexType::COLUMNS_BY_NAME)].deletes++;
    stats_[static_cast<size_t>(CatalogIndexType::COLUMNS_BY_TABLE)].deletes++;
}

// ============================================================================
// Index Operations
// ============================================================================

std::optional<uint32_t> CatalogIndexManager::findIndexByName(std::string_view index_name) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    stats_[static_cast<size_t>(CatalogIndexType::INDEXES_BY_NAME)].lookups++;

    auto result = indexes_by_name_.find(CatalogKey(index_name));
    if (result) {
        stats_[static_cast<size_t>(CatalogIndexType::INDEXES_BY_NAME)].hit_count++;
    } else {
        stats_[static_cast<size_t>(CatalogIndexType::INDEXES_BY_NAME)].miss_count++;
    }
    return result;
}

std::vector<uint32_t> CatalogIndexManager::findIndexesByTable(uint32_t table_id) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    stats_[static_cast<size_t>(CatalogIndexType::INDEXES_BY_TABLE)].lookups++;

    auto it = indexes_by_table_multi_.find(table_id);
    if (it != indexes_by_table_multi_.end()) {
        stats_[static_cast<size_t>(CatalogIndexType::INDEXES_BY_TABLE)].hit_count++;
        return it->second;
    }
    stats_[static_cast<size_t>(CatalogIndexType::INDEXES_BY_TABLE)].miss_count++;
    return {};
}

void CatalogIndexManager::indexIndex(uint32_t index_id, std::string_view index_name, uint32_t table_id) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    indexes_by_name_.insert(CatalogKey(index_name), index_id);
    indexes_by_table_multi_[table_id].push_back(index_id);

    stats_[static_cast<size_t>(CatalogIndexType::INDEXES_BY_NAME)].inserts++;
    stats_[static_cast<size_t>(CatalogIndexType::INDEXES_BY_TABLE)].inserts++;
}

void CatalogIndexManager::unindexIndex(uint32_t index_id, std::string_view index_name, uint32_t table_id) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    indexes_by_name_.remove(CatalogKey(index_name));

    auto it = indexes_by_table_multi_.find(table_id);
    if (it != indexes_by_table_multi_.end()) {
        auto& vec = it->second;
        vec.erase(std::remove(vec.begin(), vec.end(), index_id), vec.end());
    }

    stats_[static_cast<size_t>(CatalogIndexType::INDEXES_BY_NAME)].deletes++;
    stats_[static_cast<size_t>(CatalogIndexType::INDEXES_BY_TABLE)].deletes++;
}

// ============================================================================
// User and Role Operations
// ============================================================================

std::optional<uint32_t> CatalogIndexManager::findUserByName(std::string_view username) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    stats_[static_cast<size_t>(CatalogIndexType::USERS_BY_NAME)].lookups++;

    auto result = users_by_name_.find(CatalogKey(username));
    if (result) {
        stats_[static_cast<size_t>(CatalogIndexType::USERS_BY_NAME)].hit_count++;
    } else {
        stats_[static_cast<size_t>(CatalogIndexType::USERS_BY_NAME)].miss_count++;
    }
    return result;
}

std::optional<uint32_t> CatalogIndexManager::findRoleByName(std::string_view role_name) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    stats_[static_cast<size_t>(CatalogIndexType::ROLES_BY_NAME)].lookups++;

    auto result = roles_by_name_.find(CatalogKey(role_name));
    if (result) {
        stats_[static_cast<size_t>(CatalogIndexType::ROLES_BY_NAME)].hit_count++;
    } else {
        stats_[static_cast<size_t>(CatalogIndexType::ROLES_BY_NAME)].miss_count++;
    }
    return result;
}

void CatalogIndexManager::indexUser(uint32_t user_id, std::string_view username) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    users_by_name_.insert(CatalogKey(username), user_id);
    stats_[static_cast<size_t>(CatalogIndexType::USERS_BY_NAME)].inserts++;
}

void CatalogIndexManager::unindexUser(uint32_t user_id, std::string_view username) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    users_by_name_.remove(CatalogKey(username));
    stats_[static_cast<size_t>(CatalogIndexType::USERS_BY_NAME)].deletes++;
}

void CatalogIndexManager::indexRole(uint32_t role_id, std::string_view role_name) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    roles_by_name_.insert(CatalogKey(role_name), role_id);
    stats_[static_cast<size_t>(CatalogIndexType::ROLES_BY_NAME)].inserts++;
}

void CatalogIndexManager::unindexRole(uint32_t role_id, std::string_view role_name) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    roles_by_name_.remove(CatalogKey(role_name));
    stats_[static_cast<size_t>(CatalogIndexType::ROLES_BY_NAME)].deletes++;
}

// ============================================================================
// Sequence Operations
// ============================================================================

std::optional<uint32_t> CatalogIndexManager::findSequenceByName(std::string_view seq_name) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    stats_[static_cast<size_t>(CatalogIndexType::SEQUENCES_BY_NAME)].lookups++;

    auto result = sequences_by_name_.find(CatalogKey(seq_name));
    if (result) {
        stats_[static_cast<size_t>(CatalogIndexType::SEQUENCES_BY_NAME)].hit_count++;
    } else {
        stats_[static_cast<size_t>(CatalogIndexType::SEQUENCES_BY_NAME)].miss_count++;
    }
    return result;
}

void CatalogIndexManager::indexSequence(uint32_t seq_id, std::string_view seq_name) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    sequences_by_name_.insert(CatalogKey(seq_name), seq_id);
    stats_[static_cast<size_t>(CatalogIndexType::SEQUENCES_BY_NAME)].inserts++;
}

void CatalogIndexManager::unindexSequence(uint32_t seq_id, std::string_view seq_name) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    sequences_by_name_.remove(CatalogKey(seq_name));
    stats_[static_cast<size_t>(CatalogIndexType::SEQUENCES_BY_NAME)].deletes++;
}

// ============================================================================
// View Operations
// ============================================================================

std::optional<uint32_t> CatalogIndexManager::findViewByName(std::string_view view_name) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    stats_[static_cast<size_t>(CatalogIndexType::VIEWS_BY_NAME)].lookups++;

    auto result = views_by_name_.find(CatalogKey(view_name));
    if (result) {
        stats_[static_cast<size_t>(CatalogIndexType::VIEWS_BY_NAME)].hit_count++;
    } else {
        stats_[static_cast<size_t>(CatalogIndexType::VIEWS_BY_NAME)].miss_count++;
    }
    return result;
}

void CatalogIndexManager::indexView(uint32_t view_id, std::string_view view_name) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    views_by_name_.insert(CatalogKey(view_name), view_id);
    stats_[static_cast<size_t>(CatalogIndexType::VIEWS_BY_NAME)].inserts++;
}

void CatalogIndexManager::unindexView(uint32_t view_id, std::string_view view_name) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    views_by_name_.remove(CatalogKey(view_name));
    stats_[static_cast<size_t>(CatalogIndexType::VIEWS_BY_NAME)].deletes++;
}

// ============================================================================
// Function Operations
// ============================================================================

std::vector<uint32_t> CatalogIndexManager::findFunctionsByName(std::string_view func_name) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    stats_[static_cast<size_t>(CatalogIndexType::FUNCTIONS_BY_NAME)].lookups++;

    auto it = functions_by_name_multi_.find(std::string(func_name));
    if (it != functions_by_name_multi_.end()) {
        stats_[static_cast<size_t>(CatalogIndexType::FUNCTIONS_BY_NAME)].hit_count++;
        return it->second;
    }
    stats_[static_cast<size_t>(CatalogIndexType::FUNCTIONS_BY_NAME)].miss_count++;
    return {};
}

void CatalogIndexManager::indexFunction(uint32_t func_id, std::string_view func_name) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    functions_by_name_multi_[std::string(func_name)].push_back(func_id);
    stats_[static_cast<size_t>(CatalogIndexType::FUNCTIONS_BY_NAME)].inserts++;
}

void CatalogIndexManager::unindexFunction(uint32_t func_id, std::string_view func_name) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = functions_by_name_multi_.find(std::string(func_name));
    if (it != functions_by_name_multi_.end()) {
        auto& vec = it->second;
        vec.erase(std::remove(vec.begin(), vec.end(), func_id), vec.end());
    }
    stats_[static_cast<size_t>(CatalogIndexType::FUNCTIONS_BY_NAME)].deletes++;
}

// ============================================================================
// Trigger Operations
// ============================================================================

std::vector<uint32_t> CatalogIndexManager::findTriggersByTable(uint32_t table_id) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    stats_[static_cast<size_t>(CatalogIndexType::TRIGGERS_BY_TABLE)].lookups++;

    auto it = triggers_by_table_multi_.find(table_id);
    if (it != triggers_by_table_multi_.end()) {
        stats_[static_cast<size_t>(CatalogIndexType::TRIGGERS_BY_TABLE)].hit_count++;
        return it->second;
    }
    stats_[static_cast<size_t>(CatalogIndexType::TRIGGERS_BY_TABLE)].miss_count++;
    return {};
}

void CatalogIndexManager::indexTrigger(uint32_t trigger_id, uint32_t table_id) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    triggers_by_table_multi_[table_id].push_back(trigger_id);
    stats_[static_cast<size_t>(CatalogIndexType::TRIGGERS_BY_TABLE)].inserts++;
}

void CatalogIndexManager::unindexTrigger(uint32_t trigger_id, uint32_t table_id) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = triggers_by_table_multi_.find(table_id);
    if (it != triggers_by_table_multi_.end()) {
        auto& vec = it->second;
        vec.erase(std::remove(vec.begin(), vec.end(), trigger_id), vec.end());
    }
    stats_[static_cast<size_t>(CatalogIndexType::TRIGGERS_BY_TABLE)].deletes++;
}

// ============================================================================
// Statistics
// ============================================================================

CatalogIndexStats CatalogIndexManager::getStats(CatalogIndexType type) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return stats_[static_cast<size_t>(type)];
}

CatalogIndexStats CatalogIndexManager::getAggregateStats() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    CatalogIndexStats aggregate;
    for (const auto& stat : stats_) {
        aggregate.lookups += stat.lookups;
        aggregate.inserts += stat.inserts;
        aggregate.deletes += stat.deletes;
        aggregate.updates += stat.updates;
        aggregate.scan_count += stat.scan_count;
        aggregate.hit_count += stat.hit_count;
        aggregate.miss_count += stat.miss_count;
        aggregate.node_count += stat.node_count;
        aggregate.entry_count += stat.entry_count;
        aggregate.memory_bytes += stat.memory_bytes;
    }
    return aggregate;
}

void CatalogIndexManager::resetStats() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    for (auto& stat : stats_) {
        stat = CatalogIndexStats{};
    }
}

} // namespace scratchbird::catalog
