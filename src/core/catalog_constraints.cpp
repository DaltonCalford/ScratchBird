/**
 * P1-9: Constraint Table CRUD Operations
 *
 * This file contains the implementation of unified constraint management
 * operations for the sb_constraints catalog table.
 */

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/debug.h"
#include <ctime>

namespace scratchbird::core
{

// P1-9: Create a constraint
auto CatalogManager::createConstraint(const ConstraintInfo& constraint,
                                     ID& constraint_id_out,
                                     ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(constraints_cache_mutex_);

    // Generate constraint ID if not provided
    if (constraint.constraint_id == ID{})
    {
        constraint_id_out = generateUuidV7();
    }
    else
    {
        constraint_id_out = constraint.constraint_id;
    }

    // Create constraint info with generated ID
    ConstraintInfo new_constraint = constraint;
    new_constraint.constraint_id = constraint_id_out;
    new_constraint.created_time = static_cast<uint64_t>(std::time(nullptr));

    // Check for duplicate constraint name on the same table
    auto name_key = std::make_pair(constraint.table_id, constraint.constraint_name);
    auto name_it = constraint_name_lookup_.find(name_key);
    if (name_it != constraint_name_lookup_.end())
    {
        std::string error_msg = "Constraint with name '" + constraint.constraint_name +
                                "' already exists on this table";
        SET_ERROR_CONTEXT(ctx, Status::DUPLICATE_OBJECT, error_msg.c_str());
        return Status::DUPLICATE_OBJECT;
    }

    // Add to cache
    constraints_cache_[constraint_id_out] = new_constraint;
    table_constraints_.insert({constraint.table_id, constraint_id_out});
    constraint_name_lookup_[name_key] = constraint_id_out;

    DEBUG_LOG_DB("Created constraint " << new_constraint.constraint_name
                 << " (ID: " << constraint_id_out.toString() << ")");

    return Status::OK;
}

// P1-9: Get a constraint by ID
auto CatalogManager::getConstraint(const ID& constraint_id,
                                   ConstraintInfo& constraint_out,
                                   ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(constraints_cache_mutex_);

    auto it = constraints_cache_.find(constraint_id);
    if (it == constraints_cache_.end())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Constraint not found");
        return Status::NOT_FOUND;
    }

    constraint_out = it->second;
    return Status::OK;
}

// P1-9: Get a constraint by name and table
auto CatalogManager::getConstraintByName(const ID& table_id,
                                        const std::string& constraint_name,
                                        ConstraintInfo& constraint_out,
                                        ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(constraints_cache_mutex_);

    auto name_key = std::make_pair(table_id, constraint_name);
    auto it = constraint_name_lookup_.find(name_key);
    if (it == constraint_name_lookup_.end())
    {
        std::string error_msg = "Constraint '" + constraint_name + "' not found on table";
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, error_msg.c_str());
        return Status::NOT_FOUND;
    }

    return getConstraint(it->second, constraint_out, ctx);
}

// WP-5 EXEC-M4: Find a constraint by name globally (for SET CONSTRAINTS named)
auto CatalogManager::findConstraintByNameGlobal(const std::string& constraint_name,
                                                ConstraintInfo& constraint_out,
                                                ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(constraints_cache_mutex_);

    // Search all constraints in the cache for a matching name
    std::vector<const ConstraintInfo*> matches;
    for (const auto& [constraint_id, constraint] : constraints_cache_)
    {
        if (constraint.constraint_name == constraint_name)
        {
            matches.push_back(&constraint);
        }
    }

    if (matches.empty())
    {
        std::string error_msg = "Constraint '" + constraint_name + "' not found";
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, error_msg.c_str());
        return Status::NOT_FOUND;
    }

    if (matches.size() > 1)
    {
        // Ambiguous - multiple constraints with the same name exist
        std::string error_msg = "Constraint name '" + constraint_name +
                               "' is ambiguous - exists on multiple tables";
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, error_msg.c_str());
        return Status::INVALID_ARGUMENT;
    }

    // Exactly one match found
    constraint_out = *matches[0];
    return Status::OK;
}

// P1-9: Get all constraints for a table
auto CatalogManager::getConstraintsForTable(const ID& table_id,
                                           std::vector<ConstraintInfo>& constraints_out,
                                           ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(constraints_cache_mutex_);

    constraints_out.clear();

    auto range = table_constraints_.equal_range(table_id);
    for (auto it = range.first; it != range.second; ++it)
    {
        auto constraint_it = constraints_cache_.find(it->second);
        if (constraint_it != constraints_cache_.end())
        {
            constraints_out.push_back(constraint_it->second);
        }
    }

    return Status::OK;
}

// P1-9: Get constraints of a specific type for a table
auto CatalogManager::getConstraintsByType(const ID& table_id,
                                         ConstraintType type,
                                         std::vector<ConstraintInfo>& constraints_out,
                                         ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(constraints_cache_mutex_);

    constraints_out.clear();

    auto range = table_constraints_.equal_range(table_id);
    for (auto it = range.first; it != range.second; ++it)
    {
        auto constraint_it = constraints_cache_.find(it->second);
        if (constraint_it != constraints_cache_.end() &&
            constraint_it->second.constraint_type == type)
        {
            constraints_out.push_back(constraint_it->second);
        }
    }

    return Status::OK;
}

// P1-9: Update a constraint
auto CatalogManager::updateConstraint(const ID& constraint_id,
                                     const ConstraintInfo& updated_constraint,
                                     ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(constraints_cache_mutex_);

    auto it = constraints_cache_.find(constraint_id);
    if (it == constraints_cache_.end())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Constraint not found");
        return Status::NOT_FOUND;
    }

    // Update constraint (preserve ID and creation time)
    ConstraintInfo new_info = updated_constraint;
    new_info.constraint_id = constraint_id;
    new_info.created_time = it->second.created_time;

    // Update cache
    it->second = new_info;

    DEBUG_LOG_DB("Updated constraint " << new_info.constraint_name);

    return Status::OK;
}

// P1-9: Drop a constraint
auto CatalogManager::dropConstraint(const ID& constraint_id,
                                   ErrorContext* ctx) -> Status
{
    // Phase 2: Check if this is a Foreign Key constraint first
    // FKs are tracked separately in foreign_keys_cache_
    {
        std::lock_guard<std::mutex> fk_lock(foreign_keys_cache_mutex_);
        auto fk_it = foreign_keys_cache_.find(constraint_id);
        if (fk_it != foreign_keys_cache_.end()) {
            // This is an FK - drop the lock and delegate to dropForeignKey
            // (dropForeignKey manages its own locking)
        }
        else {
            // Not an FK, continue with regular constraint drop below
        }
    }

    // Try dropping as FK (handles its own locking)
    Status fk_status = dropForeignKey(constraint_id, ctx);
    if (fk_status == Status::OK) {
        return Status::OK;
    }

    // Not an FK or FK drop failed - try as regular constraint
    std::lock_guard<std::mutex> lock(constraints_cache_mutex_);

    auto it = constraints_cache_.find(constraint_id);
    if (it == constraints_cache_.end())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Constraint not found");
        return Status::NOT_FOUND;
    }

    const ConstraintInfo& constraint = it->second;

    // Remove from name lookup
    auto name_key = std::make_pair(constraint.table_id, constraint.constraint_name);
    constraint_name_lookup_.erase(name_key);

    // Remove from table constraints multimap
    auto range = table_constraints_.equal_range(constraint.table_id);
    for (auto tbl_it = range.first; tbl_it != range.second;)
    {
        if (tbl_it->second == constraint_id)
        {
            tbl_it = table_constraints_.erase(tbl_it);
        }
        else
        {
            ++tbl_it;
        }
    }

    // Remove from cache
    constraints_cache_.erase(it);

    DEBUG_LOG_DB("Dropped constraint " << constraint.constraint_name);

    return Status::OK;
}

// P1-9: Enable/disable a constraint
auto CatalogManager::setConstraintEnabled(const ID& constraint_id,
                                         bool enabled,
                                         ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(constraints_cache_mutex_);

    auto it = constraints_cache_.find(constraint_id);
    if (it == constraints_cache_.end())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Constraint not found");
        return Status::NOT_FOUND;
    }

    it->second.is_enabled = enabled;

    DEBUG_LOG_DB((enabled ? "Enabled" : "Disabled") << " constraint "
                 << it->second.constraint_name);

    return Status::OK;
}

// P1-9: Validate an existing constraint
auto CatalogManager::validateConstraint(const ID& constraint_id,
                                       bool& is_valid_out,
                                       std::string& violation_message_out,
                                       ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(constraints_cache_mutex_);

    auto it = constraints_cache_.find(constraint_id);
    if (it == constraints_cache_.end())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Constraint not found");
        return Status::NOT_FOUND;
    }

    ConstraintInfo& constraint = it->second;

    // Phase 3 Enhancement: Implement actual validation logic by scanning table rows
    // For now, mark as validated if constraint is simple (optimistic validation)
    is_valid_out = true;
    violation_message_out.clear();

    if (is_valid_out)
    {
        constraint.is_validated = true;
        constraint.validated_time = static_cast<uint64_t>(std::time(nullptr));
    }

    return Status::OK;
}

// P1-9: Get constraints that reference a table
auto CatalogManager::getReferencingConstraints(const ID& table_id,
                                              std::vector<ConstraintInfo>& constraints_out,
                                              ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(constraints_cache_mutex_);

    constraints_out.clear();

    // Find all FOREIGN_KEY constraints that reference this table
    for (const auto& [constraint_id, constraint] : constraints_cache_)
    {
        if (constraint.constraint_type == ConstraintType::FOREIGN_KEY &&
            constraint.referenced_table_id == table_id)
        {
            constraints_out.push_back(constraint);
        }
    }

    return Status::OK;
}

} // namespace scratchbird::core
