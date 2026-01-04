# Dependency Tracking Implementation Plan

**Created:** 2025-12-14
**Status:** Ready for Implementation
**Policy:** Conservative RESTRICT-only DROP semantics

---

## Overview

This plan implements comprehensive dependency tracking and DROP semantics across all 13 database object types following the conservative RESTRICT policy defined in `CASCADE_DROP_SPECIFICATION.md`.

**Key Requirements:**
1. All dependencies tracked via dependency API
2. DROP operations RESTRICT by default (error if dependents exist)
3. Owned objects auto-drop (indexes, triggers, constraints)
4. Clear error messages listing ALL blocking dependencies
5. Schemas can only be dropped when empty
6. No CASCADE keyword support

---

## Implementation Phases

### Phase 1: Infrastructure (CRITICAL) - 12-16 hours
Core utilities and dependency tracking infrastructure improvements.

### Phase 2: Table Dependencies (CRITICAL) - 16-20 hours
Fix table CREATE/DROP dependency tracking - highest risk area.

### Phase 3: View Dependencies (CRITICAL) - 12-16 hours
Replace string matching with proper dependency API usage.

### Phase 4: Code Object Dependencies (HIGH) - 14-18 hours
Functions, procedures, packages, UDRs.

### Phase 5: Type Object Dependencies (MEDIUM) - 10-14 hours
Sequences, domains, exceptions.

### Phase 6: Schema Dependencies (MEDIUM) - 6-8 hours
Empty-only schema drop enforcement.

### Phase 7: Testing (CRITICAL) - 20-30 hours
Comprehensive test coverage for all object types.

### Phase 8: Documentation (LOW) - 4-6 hours
Update user documentation and migration guides.

**Total Estimated Effort:** 94-128 hours (12-16 developer-days)

---

## Detailed Task Breakdown

---

## PHASE 1: Infrastructure (12-16 hours)

### Task 1.1: Dependency Error Message Builder
**File:** `src/core/catalog_manager.cpp`
**Effort:** 3-4 hours
**Priority:** CRITICAL

**Implementation:**

```cpp
// Add to CatalogManager class (catalog_manager.h)
private:
    struct DependencyGroup {
        ObjectType type;
        std::vector<std::string> object_names;
    };

    auto buildDependencyErrorMessage(
        const std::string& object_name,
        ObjectType object_type,
        const std::vector<DependencyInfo>& blocking_deps,
        ErrorContext* ctx
    ) -> std::string;

// Implementation (catalog_manager.cpp)
auto CatalogManager::buildDependencyErrorMessage(
    const std::string& object_name,
    ObjectType object_type,
    const std::vector<DependencyInfo>& blocking_deps,
    ErrorContext* ctx
) -> std::string
{
    // Group dependencies by type
    std::map<ObjectType, std::vector<std::string>> grouped;

    for (const auto& dep : blocking_deps) {
        std::string dep_name = getObjectName(dep.dependent_object_id,
                                              dep.dependent_type, ctx);
        grouped[dep.dependent_type].push_back(dep_name);
    }

    // Build error message
    std::ostringstream msg;
    msg << "Cannot drop " << objectTypeToString(object_type)
        << " \"" << object_name << "\" because other objects depend on it\n";
    msg << "DETAIL:\n";

    // Format by type
    for (const auto& [type, names] : grouped) {
        msg << "  " << objectTypeToString(type) << "s:\n";
        for (const auto& name : names) {
            msg << "    - " << name << "\n";
        }
    }

    msg << "HINT: Drop dependent objects first.";

    return msg.str();
}
```

**Verification:**
- [ ] Error messages group dependencies by type
- [ ] Error messages include object names (not just IDs)
- [ ] HINT section provides actionable advice
- [ ] Format matches specification examples

---

### Task 1.2: Helper Method - Get Object Name by ID
**File:** `src/core/catalog_manager.cpp`
**Effort:** 2-3 hours
**Priority:** CRITICAL

**Implementation:**

```cpp
// Add to CatalogManager class (catalog_manager.h)
private:
    auto getObjectName(const ID& object_id, ObjectType type,
                       ErrorContext* ctx) -> std::string;

// Implementation (catalog_manager.cpp)
auto CatalogManager::getObjectName(const ID& object_id, ObjectType type,
                                   ErrorContext* ctx) -> std::string
{
    switch (type) {
        case ObjectType::TABLE: {
            auto it = table_cache_.find(object_id);
            return it != table_cache_.end() ? it->second.name : "<unknown>";
        }
        case ObjectType::VIEW: {
            std::lock_guard<std::mutex> lock(view_cache_mutex_);
            auto it = view_cache_.find(object_id);
            return it != view_cache_.end() ? it->second.name : "<unknown>";
        }
        case ObjectType::FUNCTION: {
            std::lock_guard<std::mutex> lock(psql_mutex_);
            for (const auto& [name, info] : functions_) {
                if (info.function_id == object_id) return name;
            }
            return "<unknown>";
        }
        case ObjectType::PROCEDURE: {
            std::lock_guard<std::mutex> lock(psql_mutex_);
            for (const auto& [name, info] : procedures_) {
                if (info.procedure_id == object_id) return name;
            }
            return "<unknown>";
        }
        // ... add cases for all ObjectType values
        default:
            return "<unknown type>";
    }
}
```

**Verification:**
- [ ] All ObjectType enum values handled
- [ ] Thread-safe (uses appropriate mutexes)
- [ ] Returns meaningful names for all cached objects
- [ ] Gracefully handles missing objects ("<unknown>")

---

### Task 1.3: Helper Method - Filter Owned vs Blocking Dependencies
**File:** `src/core/catalog_manager.cpp`
**Effort:** 2-3 hours
**Priority:** CRITICAL

**Implementation:**

```cpp
// Add to CatalogManager class (catalog_manager.h)
private:
    struct DependencyFilter {
        std::vector<DependencyInfo> owned;      // Auto-drop
        std::vector<DependencyInfo> blocking;   // Error if exist
    };

    auto filterDependencies(
        const ID& owner_id,
        ObjectType owner_type,
        const std::vector<DependencyInfo>& all_deps,
        ErrorContext* ctx
    ) -> DependencyFilter;

// Implementation (catalog_manager.cpp)
auto CatalogManager::filterDependencies(
    const ID& owner_id,
    ObjectType owner_type,
    const std::vector<DependencyInfo>& all_deps,
    ErrorContext* ctx
) -> DependencyFilter
{
    DependencyFilter result;

    for (const auto& dep : all_deps) {
        bool is_owned = false;

        // Determine if this dependency is owned
        switch (owner_type) {
            case ObjectType::TABLE:
                // Table owns: indexes, triggers, constraints, child-side FKs
                if (dep.dependent_type == ObjectType::INDEX ||
                    dep.dependent_type == ObjectType::TRIGGER ||
                    dep.dependent_type == ObjectType::CONSTRAINT) {
                    is_owned = true;
                }
                // Child-side FK (FK FROM this table)
                if (dep.dependent_type == ObjectType::FOREIGN_KEY &&
                    dep.dependent_object_id == owner_id) {
                    is_owned = true;
                }
                break;

            case ObjectType::VIEW:
                // View owns: triggers on view (rare)
                if (dep.dependent_type == ObjectType::TRIGGER) {
                    is_owned = true;
                }
                break;

            case ObjectType::PACKAGE:
                // Package owns: package members
                if (dep.dependent_type == ObjectType::FUNCTION ||
                    dep.dependent_type == ObjectType::PROCEDURE) {
                    // Check if function/procedure is package member
                    // (implementation depends on package structure)
                    is_owned = true;  // Simplified - needs actual check
                }
                break;

            default:
                // Most objects don't own other objects
                is_owned = false;
        }

        if (is_owned) {
            result.owned.push_back(dep);
        } else {
            result.blocking.push_back(dep);
        }
    }

    return result;
}
```

**Verification:**
- [ ] Correctly identifies owned objects for tables
- [ ] Correctly identifies owned objects for packages
- [ ] Correctly identifies blocking dependencies
- [ ] Handles all ObjectType values

---

### Task 1.4: Common DROP Implementation Template
**File:** `src/core/catalog_manager.cpp`
**Effort:** 3-4 hours
**Priority:** CRITICAL

**Implementation:**

```cpp
// Template pattern for all DROP operations
// This is a conceptual template - adapt for each object type

template<typename ObjectInfo>
Status CatalogManager::dropObjectTemplate(
    const ID& object_id,
    ObjectType object_type,
    std::unordered_map<ID, ObjectInfo>& cache,
    std::mutex& cache_mutex,
    std::function<Status(const ID&, ErrorContext*)> deleteRecord,
    ErrorContext* ctx
)
{
    // 1. Get object info
    ObjectInfo obj_info;
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        auto it = cache.find(object_id);
        if (it == cache.end()) {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                            (objectTypeToString(object_type) + " not found").c_str());
            return Status::NOT_FOUND;
        }
        obj_info = it->second;
    }

    // 2. Check for dependencies
    std::vector<DependencyInfo> all_deps;
    Status status = getDependents(object_id, all_deps, ctx);
    if (status != Status::OK) return status;

    // 3. Filter owned vs blocking dependencies
    auto filtered = filterDependencies(object_id, object_type, all_deps, ctx);

    // 4. If blocking dependencies exist, fail with error
    if (!filtered.blocking.empty()) {
        std::string error_msg = buildDependencyErrorMessage(
            obj_info.name, object_type, filtered.blocking, ctx);
        SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION, error_msg.c_str());
        return Status::CONSTRAINT_VIOLATION;
    }

    // 5. Drop owned objects (auto-cascade)
    for (const auto& owned : filtered.owned) {
        status = dropOwnedObject(owned, ctx);
        if (status != Status::OK) return status;
    }

    // 6. Soft delete the object
    status = deleteRecord(object_id, ctx);
    if (status != Status::OK) return status;

    // 7. Clear dependencies
    clearDependenciesFor(object_id, ctx);

    // 8. Remove from cache
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        cache.erase(object_id);
    }

    return Status::OK;
}
```

**Note:** This is a conceptual template. Each object type will implement its own DROP method following this pattern, adapted to its specific needs.

**Verification:**
- [ ] Pattern documented in code comments
- [ ] All DROP methods follow this flow
- [ ] Error handling is consistent
- [ ] Transactions are atomic (all-or-nothing)

---

### Task 1.5: Object Type to String Helper
**File:** `src/core/catalog_manager.cpp`
**Effort:** 1 hour
**Priority:** MEDIUM

**Implementation:**

```cpp
// Add to CatalogManager class (catalog_manager.h)
private:
    static auto objectTypeToString(ObjectType type) -> std::string;

// Implementation (catalog_manager.cpp)
auto CatalogManager::objectTypeToString(ObjectType type) -> std::string
{
    switch (type) {
        case ObjectType::TABLE: return "table";
        case ObjectType::VIEW: return "view";
        case ObjectType::INDEX: return "index";
        case ObjectType::SEQUENCE: return "sequence";
        case ObjectType::FUNCTION: return "function";
        case ObjectType::PROCEDURE: return "procedure";
        case ObjectType::TRIGGER: return "trigger";
        case ObjectType::CONSTRAINT: return "constraint";
        case ObjectType::FOREIGN_KEY: return "foreign key";
        case ObjectType::DOMAIN: return "domain";
        case ObjectType::PACKAGE: return "package";
        case ObjectType::UDR: return "UDR";
        case ObjectType::EXCEPTION: return "exception";
        case ObjectType::SYNONYM: return "synonym";
        case ObjectType::SCHEMA: return "schema";
        default: return "object";
    }
}
```

**Verification:**
- [ ] All ObjectType enum values covered
- [ ] Returns user-friendly lowercase names
- [ ] Used consistently in error messages

---

### Task 1.6: Update Dependency API Tests
**File:** `tests/test_catalog_dependencies.cpp` (create if doesn't exist)
**Effort:** 2-3 hours
**Priority:** HIGH

**Tests to add:**
- [ ] Test createDependency + getDependents
- [ ] Test replaceDependencies (add, remove, keep)
- [ ] Test clearDependenciesFor
- [ ] Test dependency cache consistency
- [ ] Test concurrent dependency operations

---

## PHASE 2: Table Dependencies (16-20 hours)

### Task 2.1: Track Table Creation Dependencies
**File:** `src/core/catalog_manager.cpp`
**Effort:** 4-5 hours
**Priority:** CRITICAL

**Current State:** Tables don't track when views/functions reference them

**Implementation:**

Tables are **referenced by** other objects (not dependent on). Dependencies are created by:
- Views → Table (when view is created, see Phase 3)
- Functions → Table (when function is created, see Phase 4)
- Foreign Keys → Table (when FK is created, see Task 2.3)

**No changes needed in createTable** - dependencies are created by dependent objects.

**Verification:**
- [ ] No action needed (dependencies created by dependents)
- [ ] Document that table dependencies are created by CREATE VIEW, CREATE FUNCTION, CREATE FK

---

### Task 2.2: Fix DROP TABLE - Check Dependencies
**File:** `src/core/catalog_manager.cpp` (line ~8891)
**Effort:** 6-8 hours
**Priority:** CRITICAL

**Current Implementation Issues:**
- Only checks for indexes (via manual `listIndexesForTable`)
- Does NOT use dependency API
- Does NOT check for views, functions, parent-side FKs

**New Implementation:**

```cpp
Status CatalogManager::dropTable(const ID& table_id, ErrorContext* ctx)
{
    // 1. Get table info
    std::lock_guard<std::mutex> lock(mutex_);
    auto table_it = table_cache_.find(table_id);
    if (table_it == table_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Table not found");
        return Status::NOT_FOUND;
    }
    const TableInfo& table_info = table_it->second;

    // 2. Check for dependencies using dependency API
    std::vector<DependencyInfo> all_deps;
    Status status = getDependents(table_id, all_deps, ctx);
    if (status != Status::OK) return status;

    // 3. Filter owned vs blocking dependencies
    auto filtered = filterDependencies(table_id, ObjectType::TABLE, all_deps, ctx);

    // 4. If blocking dependencies exist, fail with detailed error
    if (!filtered.blocking.empty()) {
        std::string error_msg = buildDependencyErrorMessage(
            table_info.name, ObjectType::TABLE, filtered.blocking, ctx);
        SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION, error_msg.c_str());
        return Status::CONSTRAINT_VIOLATION;
    }

    // 5. Drop owned objects (indexes, triggers, constraints, child-side FKs)
    // Group owned objects by type for proper drop order
    std::vector<DependencyInfo> owned_triggers;
    std::vector<DependencyInfo> owned_indexes;
    std::vector<DependencyInfo> owned_constraints;
    std::vector<DependencyInfo> owned_fks;

    for (const auto& dep : filtered.owned) {
        switch (dep.dependent_type) {
            case ObjectType::TRIGGER:
                owned_triggers.push_back(dep);
                break;
            case ObjectType::INDEX:
                owned_indexes.push_back(dep);
                break;
            case ObjectType::CONSTRAINT:
                owned_constraints.push_back(dep);
                break;
            case ObjectType::FOREIGN_KEY:
                owned_fks.push_back(dep);
                break;
            default:
                break;
        }
    }

    // Drop in proper order: triggers, indexes, constraints, FKs
    for (const auto& dep : owned_triggers) {
        status = dropTrigger(dep.dependent_object_id, ctx);
        if (status != Status::OK) return status;
    }

    for (const auto& dep : owned_indexes) {
        status = dropIndex(dep.dependent_object_id, ctx);
        if (status != Status::OK) return status;
    }

    for (const auto& dep : owned_constraints) {
        status = dropConstraint(dep.dependent_object_id, ctx);
        if (status != Status::OK) return status;
    }

    for (const auto& dep : owned_fks) {
        status = dropForeignKey(dep.dependent_object_id, ctx);
        if (status != Status::OK) return status;
    }

    // 6. Soft delete the table
    status = deleteTableRecord(table_id, ctx);
    if (status != Status::OK) return status;

    // 7. Clear dependencies
    clearDependenciesFor(table_id, ctx);

    // 8. Remove from cache
    table_cache_.erase(table_it);

    LOG_INFO(CATALOG, "Dropped table '%s' with owned objects", table_info.name.c_str());

    return Status::OK;
}
```

**Verification:**
- [ ] Fails if views depend on table
- [ ] Fails if parent-side FKs reference table
- [ ] Fails if functions/procedures use table
- [ ] Auto-drops indexes, triggers, constraints
- [ ] Auto-drops child-side FKs (FKs FROM this table)
- [ ] Error message lists ALL blocking dependencies
- [ ] Transaction is atomic (rollback on error)
- [ ] clearDependenciesFor called on success

---

### Task 2.3: Create FK Dependency When FK Created
**File:** `src/core/catalog_manager.cpp` (line ~14556)
**Effort:** 3-4 hours
**Priority:** CRITICAL

**Current Implementation Issues:**
- createForeignKey does NOT create dependency link
- Manual tracking via `table_child_fks_` and `table_parent_fks_`
- Inconsistent with dependency API

**New Implementation:**

```cpp
auto CatalogManager::createForeignKey(const std::string& fk_name,
                                     const ID& child_table_id,
                                     const ID& parent_table_id,
                                     const std::vector<std::string>& child_columns,
                                     const std::vector<std::string>& parent_columns,
                                     FKAction on_delete,
                                     FKAction on_update,
                                     FKMatchType match_type,
                                     ID& fk_id_out,
                                     bool is_deferrable,
                                     bool initially_deferred,
                                     ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(foreign_keys_cache_mutex_);

    // ... validation logic ...

    // Generate FK ID
    fk_id_out = generateUuidV7();

    // Create FK info
    ForeignKeyInfo fk_info;
    fk_info.fk_id = fk_id_out;
    fk_info.fk_name = fk_name;
    fk_info.child_table_id = child_table_id;
    fk_info.parent_table_id = parent_table_id;
    // ... populate rest of fk_info ...

    // Store in cache
    foreign_keys_cache_[fk_id_out] = fk_info;
    table_child_fks_.insert({child_table_id, fk_id_out});
    table_parent_fks_.insert({parent_table_id, fk_id_out});

    // ✅ NEW: Create dependency links
    // FK is owned by child table (child-side dependency)
    ID child_dep_id;
    Status status = createDependency(
        fk_id_out, ObjectType::FOREIGN_KEY,
        child_table_id, ObjectType::TABLE,
        DependencyType::AUTO,  // Auto-drop when child table dropped
        child_dep_id,
        ctx
    );
    if (status != Status::OK) {
        // Rollback cache changes
        foreign_keys_cache_.erase(fk_id_out);
        table_child_fks_.erase(child_table_id);
        table_parent_fks_.erase(parent_table_id);
        return status;
    }

    // FK references parent table (parent-side dependency - blocks drop)
    ID parent_dep_id;
    status = createDependency(
        parent_table_id, ObjectType::TABLE,
        fk_id_out, ObjectType::FOREIGN_KEY,
        DependencyType::NORMAL,  // Blocks parent table drop
        parent_dep_id,
        ctx
    );
    if (status != Status::OK) {
        // Rollback
        deleteDependency(child_dep_id, ctx);
        foreign_keys_cache_.erase(fk_id_out);
        table_child_fks_.erase(child_table_id);
        table_parent_fks_.erase(parent_table_id);
        return status;
    }

    // Store dependency IDs for cleanup
    fk_info.child_dependency_id = child_dep_id;
    fk_info.parent_dependency_id = parent_dep_id;
    foreign_keys_cache_[fk_id_out] = fk_info;  // Update with dep IDs

    // Persist to disk
    // ... existing persistence logic ...

    return Status::OK;
}
```

**Schema Changes:**
- [ ] Add `child_dependency_id` and `parent_dependency_id` to `ForeignKeyInfo` struct
- [ ] Update `ForeignKeyRecord` if persisted (may be in-memory only)

**Verification:**
- [ ] FK creation creates TWO dependencies (child-side and parent-side)
- [ ] Child-side dependency is DependencyType::AUTO (auto-drop)
- [ ] Parent-side dependency is DependencyType::NORMAL (blocks drop)
- [ ] Dependencies cleared when FK dropped
- [ ] Rollback on error maintains consistency

---

### Task 2.4: Fix DROP Foreign Key - Use Dependency API
**File:** `src/core/catalog_manager.cpp` (line ~14740)
**Effort:** 2-3 hours
**Priority:** HIGH

**Current Implementation Issues:**
- Manual dependency search (dead code - finds nothing)
- Should use stored dependency IDs

**New Implementation:**

```cpp
auto CatalogManager::dropForeignKey(const ID& fk_id, ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(foreign_keys_cache_mutex_);

    auto it = foreign_keys_cache_.find(fk_id);
    if (it == foreign_keys_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Foreign key not found");
        return Status::NOT_FOUND;
    }

    const ForeignKeyInfo& fk = it->second;

    // Delete dependency links using stored IDs
    if (fk.child_dependency_id.isValid()) {
        deleteDependency(fk.child_dependency_id, ctx);
    }
    if (fk.parent_dependency_id.isValid()) {
        deleteDependency(fk.parent_dependency_id, ctx);
    }

    // Remove from index maps
    auto child_range = table_child_fks_.equal_range(fk.child_table_id);
    for (auto child_it = child_range.first; child_it != child_range.second; ) {
        if (child_it->second == fk_id) {
            child_it = table_child_fks_.erase(child_it);
        } else {
            ++child_it;
        }
    }

    auto parent_range = table_parent_fks_.equal_range(fk.parent_table_id);
    for (auto parent_it = parent_range.first; parent_it != parent_range.second; ) {
        if (parent_it->second == fk_id) {
            parent_it = table_parent_fks_.erase(parent_it);
        } else {
            ++parent_it;
        }
    }

    // Soft delete from disk (if persisted)
    // ... existing deletion logic ...

    // Remove from cache
    foreign_keys_cache_.erase(it);

    LOG_INFO(CATALOG, "Dropped foreign key '%s'", fk.fk_name.c_str());

    return Status::OK;
}
```

**Verification:**
- [ ] Both dependency links deleted
- [ ] Manual tracking maps cleaned up
- [ ] No orphaned dependencies remain

---

### Task 2.5: Create Index Dependency When Index Created
**File:** `src/core/catalog_manager.cpp`
**Effort:** 2-3 hours
**Priority:** MEDIUM

**Implementation:**

Find createIndex method and add dependency tracking:

```cpp
Status CatalogManager::createIndex(/* ... params ... */)
{
    // ... existing index creation logic ...

    // ✅ NEW: Create dependency link (index → table)
    ID dep_id;
    Status status = createDependency(
        index_info.index_id, ObjectType::INDEX,
        index_info.table_id, ObjectType::TABLE,
        DependencyType::AUTO,  // Auto-drop when table dropped
        dep_id,
        ctx
    );
    if (status != Status::OK) {
        // Rollback index creation
        return status;
    }

    // Store dependency ID for cleanup
    index_info.dependency_id = dep_id;

    // ... rest of creation logic ...
}
```

**Schema Changes:**
- [ ] Add `dependency_id` to `IndexInfo` struct

**Verification:**
- [ ] Index creation creates dependency link
- [ ] Dependency is DependencyType::AUTO
- [ ] filterDependencies recognizes index as owned by table

---

### Task 2.6: Fix DROP Index - Clear Dependencies
**File:** `src/core/catalog_manager.cpp` (line ~8951)
**Effort:** 1-2 hours
**Priority:** MEDIUM

**Current Implementation:**
- Does NOT clear dependencies

**New Implementation:**

```cpp
Status CatalogManager::dropIndex(const ID &index_id, ErrorContext *ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto index_it = index_cache_.find(index_id);
    if (index_it == index_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Index not found");
        return Status::NOT_FOUND;
    }

    // ✅ NEW: Clear dependencies
    clearDependenciesFor(index_id, ctx);

    // Soft delete
    Status status = deleteIndexRecord(index_id, ctx);
    if (status != Status::OK) return status;

    // Remove from cache
    index_cache_.erase(index_it);

    return Status::OK;
}
```

**Verification:**
- [ ] Dependencies cleared when index dropped
- [ ] No orphaned dependency records

---

### Task 2.7: Create Trigger Dependency When Trigger Created
**File:** `src/core/catalog_manager.cpp` (line ~8251)
**Effort:** 1 hour
**Priority:** LOW

**Current Implementation:**
- ✅ Already creates dependencies (trigger → table, trigger → procedure)
- Uses `replaceDependencies()` correctly

**Verification:**
- [ ] Confirm existing implementation is correct
- [ ] No changes needed

---

### Task 2.8: Test Table Dependencies
**File:** `tests/test_table_dependencies.cpp` (create new)
**Effort:** 3-4 hours
**Priority:** CRITICAL

**Tests to implement:**
- [ ] Test: DROP TABLE fails if view depends on table
- [ ] Test: DROP TABLE fails if parent-side FK references table
- [ ] Test: DROP TABLE succeeds and auto-drops indexes
- [ ] Test: DROP TABLE succeeds and auto-drops triggers
- [ ] Test: DROP TABLE succeeds and auto-drops child-side FKs
- [ ] Test: DROP TABLE error message lists all blocking dependencies
- [ ] Test: DROP TABLE after dropping dependent view succeeds
- [ ] Test: DROP TABLE is atomic (rollback on error)

---

## PHASE 3: View Dependencies (12-16 hours)

### Task 3.1: Parser - Extract Table/View References from SELECT
**File:** `src/parser/` (specific file depends on parser architecture)
**Effort:** 6-8 hours
**Priority:** CRITICAL

**Current State:** Parser does NOT extract table/view references from view definition

**New Implementation:**

```cpp
// Add to view creation parser
struct ViewReferences {
    std::vector<std::pair<ID, ObjectType>> referenced_objects;
    // Could include: tables, views, functions used in SELECT
};

// Parser method to extract references
auto parseViewReferences(const std::string& select_statement,
                        CatalogManager* catalog,
                        ErrorContext* ctx) -> ViewReferences
{
    ViewReferences refs;

    // Parse SELECT statement
    // Extract all table/view names from FROM and JOIN clauses
    // Resolve names to IDs using catalog
    // Add to refs.referenced_objects

    // Example (simplified):
    // SELECT e.name FROM employees e JOIN departments d ...
    // -> Extract "employees", "departments"
    // -> Resolve to table IDs
    // -> Add (employees_id, ObjectType::TABLE)
    // -> Add (departments_id, ObjectType::TABLE)

    return refs;
}
```

**Verification:**
- [ ] Extracts all table references from FROM clause
- [ ] Extracts all table/view references from JOIN clauses
- [ ] Extracts all view references (view FROM view)
- [ ] Resolves schema-qualified names correctly
- [ ] Handles delimited identifiers (case-sensitive names)
- [ ] Handles subqueries (nested SELECTs)
- [ ] Returns empty list if parse fails (graceful degradation)

---

### Task 3.2: CREATE VIEW - Record Dependencies
**File:** `src/core/catalog_manager.cpp` (line ~10125)
**Effort:** 3-4 hours
**Priority:** CRITICAL

**Current Implementation:**
- Does NOT record dependencies
- Only stores definition as text

**New Implementation:**

```cpp
auto CatalogManager::createView(const ID& schema_id, const std::string& name,
                                const std::string& definition, bool or_replace,
                                bool check_option, bool materialized,
                                const std::vector<std::string>& column_names,
                                const ID& materialized_table_id,
                                const std::vector<std::pair<ID, ObjectType>>& referenced_objects,  // ✅ NEW PARAM
                                ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(view_cache_mutex_);

    // ... existing view creation logic ...

    // Create new view
    ViewInfo view;
    view.view_id = generateUuidV7();
    view.schema_id = schema_id;
    view.name = name;
    view.definition = definition;
    // ... set other fields ...

    view_cache_[view.view_id] = view;
    view_name_to_id_[name] = view.view_id;

    // ✅ NEW: Record dependencies
    Status status = replaceDependencies(
        view.view_id,
        ObjectType::VIEW,
        referenced_objects,  // Tables/views referenced by this view
        ctx
    );
    if (status != Status::OK) {
        // Rollback view creation
        view_cache_.erase(view.view_id);
        view_name_to_id_.erase(name);
        return status;
    }

    LOG_INFO(CATALOG, "Created view '%s' with %zu dependencies",
             name.c_str(), referenced_objects.size());

    return Status::OK;
}
```

**Executor Changes:**
- [ ] Update CREATE VIEW executor to call parser's `parseViewReferences()`
- [ ] Pass `referenced_objects` to `createView()`

**Verification:**
- [ ] Dependencies created when view created
- [ ] Dependencies include all referenced tables/views
- [ ] OR REPLACE updates dependencies correctly
- [ ] Materialized views track base table dependencies

---

### Task 3.3: DROP VIEW - Use Dependency API (Remove String Matching)
**File:** `src/core/catalog_manager.cpp` (line ~10204)
**Effort:** 2-3 hours
**Priority:** CRITICAL

**Current Implementation:**
- Uses string matching: `dep_view.definition.find(view_name)`
- Unreliable (false positives/negatives)

**New Implementation:**

```cpp
auto CatalogManager::dropView(const ID& view_id, ErrorContext* ctx) -> Status
{
    // 1. Get view info
    std::lock_guard<std::mutex> lock(view_cache_mutex_);

    auto it = view_cache_.find(view_id);
    if (it == view_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "View not found");
        return Status::NOT_FOUND;
    }

    const ViewInfo& view_info = it->second;

    // 2. Check for dependencies using dependency API (not string matching!)
    std::vector<DependencyInfo> all_deps;
    Status status = getDependents(view_id, all_deps, ctx);
    if (status != Status::OK) return status;

    // 3. Filter owned vs blocking dependencies
    auto filtered = filterDependencies(view_id, ObjectType::VIEW, all_deps, ctx);

    // 4. If blocking dependencies exist, fail with error
    if (!filtered.blocking.empty()) {
        std::string error_msg = buildDependencyErrorMessage(
            view_info.name, ObjectType::VIEW, filtered.blocking, ctx);
        SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION, error_msg.c_str());
        return Status::CONSTRAINT_VIOLATION;
    }

    // 5. Drop owned objects (triggers on view, if any)
    for (const auto& dep : filtered.owned) {
        if (dep.dependent_type == ObjectType::TRIGGER) {
            status = dropTrigger(dep.dependent_object_id, ctx);
            if (status != Status::OK) return status;
        }
    }

    // 6. Clear dependencies
    clearDependenciesFor(view_id, ctx);

    // 7. Remove from cache
    view_cache_.erase(it);
    view_name_to_id_.erase(view_info.name);

    LOG_INFO(CATALOG, "Dropped view '%s'", view_info.name.c_str());

    return Status::OK;
}
```

**Verification:**
- [ ] No string matching used
- [ ] Uses dependency API exclusively
- [ ] Fails if dependent views exist
- [ ] Fails if functions/procedures use view
- [ ] Auto-drops triggers on view (if supported)
- [ ] clearDependenciesFor called
- [ ] Error message accurate and complete

---

### Task 3.4: Test View Dependencies
**File:** `tests/test_view_dependencies.cpp` (create new)
**Effort:** 2-3 hours
**Priority:** CRITICAL

**Tests to implement:**
- [ ] Test: CREATE VIEW records dependencies on tables
- [ ] Test: CREATE VIEW records dependencies on other views
- [ ] Test: DROP VIEW fails if dependent view exists
- [ ] Test: DROP VIEW succeeds after dropping dependent view
- [ ] Test: DROP VIEW error message lists dependent views
- [ ] Test: OR REPLACE VIEW updates dependencies correctly
- [ ] Test: View → View → Table dependency chain works

---

## PHASE 4: Code Object Dependencies (14-18 hours)

### Task 4.1: Parser - Extract References from Function/Procedure Body
**File:** `src/parser/` (specific file depends on parser architecture)
**Effort:** 6-8 hours
**Priority:** HIGH

**Current State:** Parser extracts some dependencies but may be incomplete

**Enhancement:**

```cpp
// Enhance existing parser to extract ALL references
struct CodeReferences {
    std::vector<std::pair<ID, ObjectType>> referenced_objects;
    // Should include:
    // - Tables (SELECT, INSERT, UPDATE, DELETE)
    // - Views (SELECT)
    // - Functions/procedures (calls)
    // - Sequences (NEXT VALUE FOR)
    // - Exceptions (RAISE)
};

// Parser method
auto parseCodeReferences(const std::string& code_body,
                        CatalogManager* catalog,
                        ErrorContext* ctx) -> CodeReferences
{
    // Parse code body
    // Extract all object references
    // Resolve to IDs
    // Return references
}
```

**Verification:**
- [ ] Extracts table references (FROM, INTO, etc.)
- [ ] Extracts view references
- [ ] Extracts function/procedure calls
- [ ] Extracts sequence references (NEXT VALUE FOR)
- [ ] Extracts exception references (RAISE)
- [ ] Handles schema-qualified names
- [ ] Graceful degradation on parse errors

---

### Task 4.2: DROP FUNCTION/PROCEDURE - Check Dependencies
**File:** `src/core/catalog_manager.cpp` (line ~8753, ~8783)
**Effort:** 3-4 hours
**Priority:** HIGH

**Current Implementation:**
- Clears dependencies on drop (✅ good)
- Does NOT check for dependents (❌ missing)

**New Implementation:**

```cpp
auto CatalogManager::dropFunction(const std::string &name, bool if_exists,
                                  ErrorContext *ctx) -> Status
{
    std::lock_guard<std::mutex> lock(psql_mutex_);

    auto it = functions_.find(name);
    if (it == functions_.end()) {
        if (if_exists) return Status::OK;
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Function not found");
        return Status::NOT_FOUND;
    }

    const FunctionInfo& func = it->second;

    // ✅ NEW: Check for dependents
    std::vector<DependencyInfo> dependents;
    Status status = getDependents(func.function_id, dependents, ctx);
    if (status != Status::OK) return status;

    // Functions don't own other objects, so all dependents are blocking
    if (!dependents.empty()) {
        std::string error_msg = buildDependencyErrorMessage(
            name, ObjectType::FUNCTION, dependents, ctx);
        SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION, error_msg.c_str());
        return Status::CONSTRAINT_VIOLATION;
    }

    // Clear dependencies (what this function references)
    clearDependenciesFor(func.function_id, ctx);

    // Remove from cache
    functions_.erase(it);
    LOG_INFO(CATALOG, "Function '%s' dropped", name.c_str());

    return Status::OK;
}

// Same pattern for dropProcedure
```

**Verification:**
- [ ] DROP FUNCTION fails if triggers call it
- [ ] DROP FUNCTION fails if views use it
- [ ] DROP FUNCTION fails if other code calls it
- [ ] Error message lists all dependents
- [ ] clearDependenciesFor still called

---

### Task 4.3: DROP PACKAGE - Check Dependencies
**File:** `src/core/catalog_manager.cpp` (line ~15737)
**Effort:** 2-3 hours
**Priority:** HIGH

**Current Implementation:**
- Does NOT check for dependents
- CASCADE parameter ignored

**New Implementation:**

```cpp
auto CatalogManager::dropPackage(const ID& package_id, ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Get package info (need to look it up from cache/disk)
    // ... lookup logic ...

    // Check for dependents (code calling package members)
    std::vector<DependencyInfo> all_deps;
    Status status = getDependents(package_id, all_deps, ctx);
    if (status != Status::OK) return status;

    // Filter owned (package members) vs blocking (external code)
    auto filtered = filterDependencies(package_id, ObjectType::PACKAGE, all_deps, ctx);

    // If external code depends on package, fail
    if (!filtered.blocking.empty()) {
        std::string error_msg = buildDependencyErrorMessage(
            "<package_name>", ObjectType::PACKAGE, filtered.blocking, ctx);
        SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION, error_msg.c_str());
        return Status::CONSTRAINT_VIOLATION;
    }

    // Drop owned package members (functions/procedures in package)
    for (const auto& dep : filtered.owned) {
        // Drop package member
        // ... implementation depends on package structure ...
    }

    // Soft delete package
    // ... existing deletion logic ...

    // Clear dependencies
    clearDependenciesFor(package_id, ctx);

    return Status::OK;
}
```

**Verification:**
- [ ] Fails if triggers call package members
- [ ] Fails if external code calls package members
- [ ] Auto-drops package members (functions/procedures)
- [ ] Error message lists dependents
- [ ] CASCADE parameter removed (not supported)

---

### Task 4.4: DROP UDR - Check Dependencies
**File:** `src/core/catalog_manager.cpp` (line ~15486)
**Effort:** 2-3 hours
**Priority:** MEDIUM

**Implementation:** Same pattern as DROP FUNCTION

**Verification:**
- [ ] Fails if triggers call UDR
- [ ] Fails if code calls UDR
- [ ] Error message lists dependents

---

### Task 4.5: Test Code Object Dependencies
**File:** `tests/test_code_dependencies.cpp` (create new)
**Effort:** 2-3 hours
**Priority:** HIGH

**Tests to implement:**
- [ ] Test: DROP FUNCTION fails if trigger calls it
- [ ] Test: DROP PROCEDURE fails if other code calls it
- [ ] Test: DROP PACKAGE fails if external code calls members
- [ ] Test: DROP PACKAGE auto-drops package members
- [ ] Test: Error messages list all dependents

---

## PHASE 5: Type Object Dependencies (10-14 hours)

### Task 5.1: Track Sequence Usage in DEFAULT Clauses
**File:** `src/core/catalog_manager.cpp` (ALTER TABLE ADD COLUMN)
**Effort:** 4-5 hours
**Priority:** HIGH

**Implementation:**

```cpp
// In addColumn method
Status CatalogManager::addColumn(const ID& table_id, const ColumnInfo& column_info,
                                ErrorContext* ctx)
{
    // ... existing column addition logic ...

    // ✅ NEW: If column has DEFAULT using sequence, create dependency
    if (!column_info.default_value.empty()) {
        // Parse DEFAULT expression
        // Check if it contains sequence reference (e.g., NEXT VALUE FOR seq_name)
        // If yes:
        ID sequence_id = resolveSequenceName(/* extracted sequence name */, ctx);
        if (sequence_id.isValid()) {
            ID dep_id;
            Status status = createDependency(
                column_id, ObjectType::COLUMN,  // Or use table_id
                sequence_id, ObjectType::SEQUENCE,
                DependencyType::NORMAL,
                dep_id,
                ctx
            );
            // Handle error...
        }
    }

    // ... rest of logic ...
}
```

**Note:** May need to add COLUMN to ObjectType enum, or track as table→sequence dependency.

**Verification:**
- [ ] Dependency created when DEFAULT uses sequence
- [ ] Dependency cleared when DEFAULT dropped
- [ ] Dependency cleared when column dropped

---

### Task 5.2: DROP SEQUENCE - Check Dependencies
**File:** `src/core/catalog_manager.cpp` (line ~9911)
**Effort:** 2-3 hours
**Priority:** HIGH

**Current Implementation:**
- Does NOT check for columns using sequence
- CASCADE parameter ignored

**New Implementation:**

```cpp
auto CatalogManager::dropSequence(const ID& sequence_id, ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> cache_lock(sequence_cache_mutex_);

    auto it = sequence_cache_.find(sequence_id);
    if (it == sequence_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Sequence not found");
        return Status::NOT_FOUND;
    }

    std::string seq_name = it->second->name;

    // Check for dependencies (columns using this sequence)
    std::vector<DependencyInfo> dependents;
    Status status = getDependents(sequence_id, dependents, ctx);
    if (status != Status::OK) return status;

    // All dependencies are blocking (no owned objects)
    if (!dependents.empty()) {
        // Build custom error message for sequences
        std::ostringstream msg;
        msg << "Cannot drop sequence \"" << seq_name
            << "\" because columns depend on it\n";
        msg << "DETAIL:\n";
        msg << "  Columns using this sequence in DEFAULT:\n";
        for (const auto& dep : dependents) {
            // Get column name: table.column
            std::string col_name = getObjectName(dep.dependent_object_id,
                                                 dep.dependent_type, ctx);
            msg << "    - " << col_name << "\n";
        }
        msg << "HINT: Remove DEFAULT constraints first using:\n";
        msg << "  ALTER TABLE <table> ALTER COLUMN <column> DROP DEFAULT;";

        SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION, msg.str().c_str());
        return Status::CONSTRAINT_VIOLATION;
    }

    // Remove from cache
    sequence_cache_.erase(it);

    {
        std::lock_guard<std::mutex> name_lock(sequence_name_mutex_);
        sequence_name_to_id_.erase(seq_name);
    }

    LOG_INFO(CATALOG, "Dropped sequence '%s' successfully", seq_name.c_str());

    return Status::OK;
}
```

**Verification:**
- [ ] Fails if columns use sequence in DEFAULT
- [ ] Error message lists affected columns
- [ ] Error message suggests ALTER TABLE ... DROP DEFAULT

---

### Task 5.3: DROP DOMAIN - Check Dependencies
**File:** `src/core/catalog_manager.cpp` (line ~15109)
**Effort:** 2-3 hours
**Priority:** MEDIUM

**Current Implementation:**
- ✅ Uses `findColumnsByDomain()` (custom search)
- ⚠️ Returns NOT_IMPLEMENTED error (even with empty dependents check)

**New Implementation:**

Convert `findColumnsByDomain()` to use dependency API OR keep custom search but clean up logic:

```cpp
auto CatalogManager::dropDomain(const ID& domain_id, ErrorContext* ctx) -> Status
{
    // Option A: Use dependency API (if columns create domain dependencies)
    std::vector<DependencyInfo> dependents;
    Status status = getDependents(domain_id, dependents, ctx);
    if (status != Status::OK) return status;

    if (!dependents.empty()) {
        // Build error message
        std::ostringstream msg;
        msg << "Cannot drop domain because columns use it\n";
        msg << "DETAIL:\n";
        msg << "  Columns typed as this domain:\n";
        for (const auto& dep : dependents) {
            std::string col_name = getObjectName(dep.dependent_object_id,
                                                 dep.dependent_type, ctx);
            msg << "    - " << col_name << "\n";
        }
        msg << "HINT: Alter columns to base type first using:\n";
        msg << "  ALTER TABLE <table> ALTER COLUMN <column> TYPE <base_type>;";

        SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION, msg.str().c_str());
        return Status::CONSTRAINT_VIOLATION;
    }

    // Option B: Keep existing findColumnsByDomain() but remove NOT_IMPLEMENTED
    // ... use existing custom search ...
    // if (!dependent_columns.empty()) {
    //     // Same error as above
    // }

    // Soft delete domain
    // ... existing deletion logic ...

    LOG_INFO(CATALOG, "Dropped domain successfully");

    return Status::OK;
}
```

**Note:** May need to create column→domain dependencies when column created. Alternatively, keep custom search via `findColumnsByDomain()`.

**Verification:**
- [ ] Fails if columns use domain
- [ ] Error message lists affected columns
- [ ] NOT_IMPLEMENTED error removed

---

### Task 5.4: DROP EXCEPTION - Check Dependencies
**File:** `src/core/catalog_manager.cpp` (line ~15916)
**Effort:** 2-3 hours
**Priority:** LOW

**Current Implementation:**
- CASCADE parameter commented out
- Does NOT check for procedures raising exception

**New Implementation:**

```cpp
Status CatalogManager::dropException(const ID& exception_id, ErrorContext* ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Get exception info
    // ... lookup logic ...

    // Check for dependents (procedures raising this exception)
    std::vector<DependencyInfo> dependents;
    Status status = getDependents(exception_id, dependents, ctx);
    if (status != Status::OK) return status;

    if (!dependents.empty()) {
        std::string error_msg = buildDependencyErrorMessage(
            "<exception_name>", ObjectType::EXCEPTION, dependents, ctx);
        SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION, error_msg.c_str());
        return Status::CONSTRAINT_VIOLATION;
    }

    // Soft delete exception
    // ... existing deletion logic ...

    return Status::OK;
}
```

**Note:** Requires parser to extract RAISE statements and create procedure→exception dependencies.

**Verification:**
- [ ] Fails if procedures raise exception
- [ ] Error message lists procedures

---

### Task 5.5: Test Type Object Dependencies
**File:** `tests/test_type_dependencies.cpp` (create new)
**Effort:** 2-3 hours
**Priority:** MEDIUM

**Tests to implement:**
- [ ] Test: DROP SEQUENCE fails if column uses it in DEFAULT
- [ ] Test: DROP DOMAIN fails if column uses it
- [ ] Test: DROP EXCEPTION fails if procedure raises it
- [ ] Test: Error messages are correct

---

## PHASE 6: Schema Dependencies (6-8 hours)

### Task 6.1: DROP SCHEMA - Enforce Empty Check
**File:** `src/core/catalog_manager.cpp` (line ~1889)
**Effort:** 4-6 hours
**Priority:** MEDIUM

**Current Implementation:**
- Has manual checks for tables, views, sequences
- Uses CASCADE parameter (should be removed)

**New Implementation:**

```cpp
auto CatalogManager::dropSchema(const ID& schema_id, ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    // 1. Check if schema exists
    auto it = schema_cache_.find(schema_id);
    if (it == schema_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Schema not found");
        return Status::NOT_FOUND;
    }

    const SchemaInfo& schema_info = it->second;

    // 2. Count ALL objects in schema
    struct ObjectCounts {
        size_t tables = 0;
        size_t views = 0;
        size_t functions = 0;
        size_t procedures = 0;
        size_t sequences = 0;
        size_t domains = 0;
        size_t triggers = 0;
        size_t indexes = 0;
        size_t packages = 0;
        size_t udrs = 0;
        size_t exceptions = 0;
        size_t child_schemas = 0;

        size_t total() const {
            return tables + views + functions + procedures + sequences +
                   domains + triggers + indexes + packages + udrs +
                   exceptions + child_schemas;
        }
    };

    ObjectCounts counts;

    // Count tables
    for (const auto& [id, table] : table_cache_) {
        if (table.schema_id == schema_id) counts.tables++;
    }

    // Count views
    {
        std::lock_guard<std::mutex> vlock(view_cache_mutex_);
        for (const auto& [id, view] : view_cache_) {
            if (view.schema_id == schema_id) counts.views++;
        }
    }

    // Count functions
    {
        std::lock_guard<std::mutex> flock(psql_mutex_);
        for (const auto& [name, func] : functions_) {
            if (func.schema_id == schema_id) counts.functions++;
        }
    }

    // Count procedures
    {
        std::lock_guard<std::mutex> plock(psql_mutex_);
        for (const auto& [name, proc] : procedures_) {
            if (proc.schema_id == schema_id) counts.procedures++;
        }
    }

    // Count sequences
    {
        std::lock_guard<std::mutex> slock(sequence_cache_mutex_);
        for (const auto& [id, seq] : sequence_cache_) {
            if (seq->schema_id == schema_id) counts.sequences++;
        }
    }

    // ... count all other object types ...

    // Count child schemas (hierarchical)
    for (const auto& [id, child_schema] : schema_cache_) {
        if (child_schema.parent_schema_id == schema_id) {
            counts.child_schemas++;
        }
    }

    // 3. If any objects exist, fail with detailed error
    if (counts.total() > 0) {
        std::ostringstream msg;
        msg << "Cannot drop schema \"" << schema_info.name
            << "\" because it contains objects\n";
        msg << "DETAIL:\n";
        msg << "  Schema \"" << schema_info.name << "\" contains:\n";

        if (counts.tables > 0)
            msg << "    Tables: " << counts.tables << "\n";
        if (counts.views > 0)
            msg << "    Views: " << counts.views << "\n";
        if (counts.functions > 0)
            msg << "    Functions: " << counts.functions << "\n";
        if (counts.procedures > 0)
            msg << "    Procedures: " << counts.procedures << "\n";
        if (counts.sequences > 0)
            msg << "    Sequences: " << counts.sequences << "\n";
        if (counts.domains > 0)
            msg << "    Domains: " << counts.domains << "\n";
        if (counts.triggers > 0)
            msg << "    Triggers: " << counts.triggers << "\n";
        if (counts.indexes > 0)
            msg << "    Indexes: " << counts.indexes << "\n";
        if (counts.packages > 0)
            msg << "    Packages: " << counts.packages << "\n";
        if (counts.udrs > 0)
            msg << "    UDRs: " << counts.udrs << "\n";
        if (counts.exceptions > 0)
            msg << "    Exceptions: " << counts.exceptions << "\n";
        if (counts.child_schemas > 0)
            msg << "    Child Schemas: " << counts.child_schemas << "\n";

        msg << "  Total: " << counts.total() << " objects\n";
        msg << "HINT: Drop or move all objects from schema first.\n";
        msg << "  To see list: SELECT object_name, object_type FROM SYS.OBJECTS ";
        msg << "WHERE schema_name = '" << schema_info.name << "';";

        SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION, msg.str().c_str());
        return Status::CONSTRAINT_VIOLATION;
    }

    // 4. Schema is empty - safe to drop
    schema_cache_.erase(it);

    LOG_INFO(CATALOG, "Dropped empty schema '%s'", schema_info.name.c_str());

    return Status::OK;
}
```

**Verification:**
- [ ] Fails if schema contains ANY objects
- [ ] Error message lists counts by object type
- [ ] Error message shows total count
- [ ] CASCADE parameter removed
- [ ] Succeeds only when schema is completely empty

---

### Task 6.2: Test Schema Empty Check
**File:** `tests/test_schema_dependencies.cpp` (create new)
**Effort:** 2 hours
**Priority:** MEDIUM

**Tests to implement:**
- [ ] Test: DROP SCHEMA succeeds when empty
- [ ] Test: DROP SCHEMA fails if contains tables
- [ ] Test: DROP SCHEMA fails if contains views
- [ ] Test: DROP SCHEMA fails if contains any object type
- [ ] Test: DROP SCHEMA fails if contains child schemas
- [ ] Test: Error message lists all object counts

---

## PHASE 7: Testing (20-30 hours)

### Task 7.1: Integration Tests - Cross-Object Dependencies
**File:** `tests/test_dependency_integration.cpp` (create new)
**Effort:** 8-10 hours
**Priority:** CRITICAL

**Test scenarios:**
- [ ] Test: Table → View → View (3-level chain)
- [ ] Test: Table → FK → Table → View (cross-table chain)
- [ ] Test: Function → Trigger → Table → View (multi-type chain)
- [ ] Test: Sequence → Column DEFAULT → Table → View
- [ ] Test: Domain → Column → Table → View
- [ ] Test: Package → Procedure → Trigger → Table
- [ ] Test: Circular dependency detection (should not deadlock)
- [ ] Test: Complex dependency graph (10+ objects)

---

### Task 7.2: Error Message Tests
**File:** `tests/test_dependency_errors.cpp` (create new)
**Effort:** 4-5 hours
**Priority:** HIGH

**Test scenarios:**
- [ ] Test: Error message format matches specification
- [ ] Test: Dependencies grouped by type
- [ ] Test: All blocking dependencies listed
- [ ] Test: HINT section is actionable
- [ ] Test: Object names (not IDs) in error messages
- [ ] Test: Schema error shows object counts

---

### Task 7.3: Transaction Rollback Tests
**File:** `tests/test_dependency_transactions.cpp` (create new)
**Effort:** 4-6 hours
**Priority:** CRITICAL

**Test scenarios:**
- [ ] Test: DROP failure rolls back all changes
- [ ] Test: Partial auto-drop failure rolls back entire DROP
- [ ] Test: No orphaned dependencies after rollback
- [ ] Test: Cache consistency after rollback
- [ ] Test: Concurrent DROP operations (thread safety)

---

### Task 7.4: Performance Tests
**File:** `tests/test_dependency_performance.cpp` (create new)
**Effort:** 4-6 hours
**Priority:** MEDIUM

**Test scenarios:**
- [ ] Test: getDependents() with 1K dependencies (<10ms)
- [ ] Test: getDependents() with 10K dependencies (<50ms)
- [ ] Test: getDependents() with 100K dependencies (<200ms)
- [ ] Test: DROP TABLE with 1K owned objects
- [ ] Test: DROP SCHEMA empty check with 10K objects
- [ ] Test: replaceDependencies() with 1K references

---

### Task 7.5: Firebird Compatibility Tests
**File:** `tests/test_firebird_drop_semantics.cpp` (create new)
**Effort:** 3-4 hours
**Priority:** MEDIUM

**Test scenarios:**
- [ ] Test: DROP TABLE behavior matches Firebird 3.0+
- [ ] Test: DROP VIEW behavior matches Firebird
- [ ] Test: DROP SEQUENCE behavior matches Firebird
- [ ] Test: DROP DOMAIN behavior matches Firebird
- [ ] Test: Error messages similar to Firebird format

---

## PHASE 8: Documentation (4-6 hours)

### Task 8.1: Update User Documentation
**File:** `docs/user-guide/DDL_OPERATIONS.md` (create/update)
**Effort:** 2-3 hours
**Priority:** LOW

**Content:**
- Explain RESTRICT-only DROP semantics
- Document auto-drop behavior for owned objects
- Provide examples of dependency resolution
- Show how to query dependencies before drop
- Explain error messages

---

### Task 8.2: Create Migration Guide
**File:** `docs/migration/DEPENDENCY_TRACKING_MIGRATION.md` (create)
**Effort:** 2-3 hours
**Priority:** LOW

**Content:**
- Breaking changes from previous behavior
- How to identify affected code
- Step-by-step migration instructions
- Common error messages and solutions
- Query to find all dependencies in database

---

## Summary Checklist

### Phase 1: Infrastructure ✅
- [ ] Task 1.1: Dependency error message builder
- [ ] Task 1.2: Get object name by ID helper
- [ ] Task 1.3: Filter owned vs blocking dependencies
- [ ] Task 1.4: Common DROP template pattern
- [ ] Task 1.5: Object type to string helper
- [ ] Task 1.6: Update dependency API tests

### Phase 2: Table Dependencies ✅
- [ ] Task 2.1: Track table creation (no action needed)
- [ ] Task 2.2: Fix DROP TABLE - check dependencies
- [ ] Task 2.3: Create FK dependency when FK created
- [ ] Task 2.4: Fix DROP FK - use dependency API
- [ ] Task 2.5: Create index dependency when created
- [ ] Task 2.6: Fix DROP INDEX - clear dependencies
- [ ] Task 2.7: Verify trigger dependencies (already done)
- [ ] Task 2.8: Test table dependencies

### Phase 3: View Dependencies ✅
- [ ] Task 3.1: Parser - extract table/view references
- [ ] Task 3.2: CREATE VIEW - record dependencies
- [ ] Task 3.3: DROP VIEW - use dependency API
- [ ] Task 3.4: Test view dependencies

### Phase 4: Code Object Dependencies ✅
- [ ] Task 4.1: Parser - extract code references
- [ ] Task 4.2: DROP FUNCTION/PROCEDURE - check dependencies
- [ ] Task 4.3: DROP PACKAGE - check dependencies
- [ ] Task 4.4: DROP UDR - check dependencies
- [ ] Task 4.5: Test code object dependencies

### Phase 5: Type Object Dependencies ✅
- [ ] Task 5.1: Track sequence usage in DEFAULT
- [ ] Task 5.2: DROP SEQUENCE - check dependencies
- [ ] Task 5.3: DROP DOMAIN - check dependencies
- [ ] Task 5.4: DROP EXCEPTION - check dependencies
- [ ] Task 5.5: Test type object dependencies

### Phase 6: Schema Dependencies ✅
- [ ] Task 6.1: DROP SCHEMA - enforce empty check
- [ ] Task 6.2: Test schema empty check

### Phase 7: Testing ✅
- [ ] Task 7.1: Integration tests - cross-object
- [ ] Task 7.2: Error message tests
- [ ] Task 7.3: Transaction rollback tests
- [ ] Task 7.4: Performance tests
- [ ] Task 7.5: Firebird compatibility tests

### Phase 8: Documentation ✅
- [ ] Task 8.1: Update user documentation
- [ ] Task 8.2: Create migration guide

---

## Total Effort Summary

| Phase | Tasks | Estimated Hours |
|-------|-------|-----------------|
| Phase 1: Infrastructure | 6 tasks | 12-16 hours |
| Phase 2: Table Dependencies | 8 tasks | 16-20 hours |
| Phase 3: View Dependencies | 4 tasks | 12-16 hours |
| Phase 4: Code Objects | 5 tasks | 14-18 hours |
| Phase 5: Type Objects | 5 tasks | 10-14 hours |
| Phase 6: Schema | 2 tasks | 6-8 hours |
| Phase 7: Testing | 5 tasks | 20-30 hours |
| Phase 8: Documentation | 2 tasks | 4-6 hours |
| **TOTAL** | **37 tasks** | **94-128 hours** |

**Estimated timeline:** 12-16 developer-days (2.5-3 weeks at full-time pace)

---

## Implementation Order

**Week 1: Critical Infrastructure + Tables**
- Days 1-2: Phase 1 (Infrastructure)
- Days 3-5: Phase 2 (Table Dependencies)

**Week 2: Views + Code Objects**
- Days 1-3: Phase 3 (View Dependencies)
- Days 4-5: Phase 4 (Code Object Dependencies)

**Week 3: Type Objects + Testing + Documentation**
- Days 1-2: Phase 5 (Type Object Dependencies)
- Day 3: Phase 6 (Schema Dependencies)
- Days 4-5: Phase 7 (Testing - start)

**Week 4 (if needed): Complete Testing + Documentation**
- Days 1-3: Phase 7 (Testing - complete)
- Days 4-5: Phase 8 (Documentation)

---

## Risk Mitigation

**High-Risk Areas:**
1. **Parser changes** (Tasks 3.1, 4.1) - Complex, affects multiple subsystems
   - **Mitigation:** Start early, extensive testing, graceful degradation
2. **Table DROP changes** (Task 2.2) - Core operation, high usage
   - **Mitigation:** Thorough testing, transaction safety, rollback on error
3. **View dependency extraction** (Task 3.1) - Complex SQL parsing
   - **Mitigation:** Handle edge cases, test with complex queries

**Medium-Risk Areas:**
1. **Foreign key dependency tracking** (Tasks 2.3, 2.4) - Two-way dependencies
   - **Mitigation:** Clear documentation, test both directions
2. **Package dependencies** (Task 4.3) - Complex ownership model
   - **Mitigation:** Define ownership clearly, test thoroughly

---

## Success Criteria

**Completion criteria:**
- [ ] All 37 tasks completed
- [ ] All tests passing (unit + integration)
- [ ] No regressions in existing functionality
- [ ] Performance benchmarks met (<200ms for 100K deps)
- [ ] Documentation complete and reviewed
- [ ] Code review passed

**Quality criteria:**
- [ ] Error messages match specification format
- [ ] All DROP operations use dependency API consistently
- [ ] No string matching for dependency detection
- [ ] Schemas can only be dropped when empty
- [ ] Firebird compatibility maintained

---

**END OF IMPLEMENTATION PLAN**
