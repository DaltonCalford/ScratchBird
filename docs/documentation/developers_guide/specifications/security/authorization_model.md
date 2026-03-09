# Specification: Authorization Model

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | security/authorization |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 0.1.0 |
| **Authors** | ScratchBird Security Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/permission_cache.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp:16605`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/security/view_security.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/permission_cache.h`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/security/view_security.h`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_security_issues.cpp`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/integration/test_security_phase2.cpp`

## Synopsis

This specification defines the ScratchBird authorization model, including GRANT/REVOKE operations, permission inheritance through roles, the permission cache with LRU eviction and TTL expiration, and view security context management with DEFINER/INVOKER modes.

## Scope

### In Scope

- GRANT/REVOKE permission operations
- Permission checking with caching
- Role-based permission inheritance
- View security contexts (DEFINER/INVOKER)
- Security barriers and check options
- Permission cache invalidation strategies

### Out of Scope

- Authentication flows (see `authentication_flow.md`)
- Row-Level Security policies (see `rls_policy_enforcement.md`)
- Column-Level Security masking (see `cls_column_masking.md`)
- Audit logging of authorization decisions

## Background

ScratchBird implements a SQL-standard authorization model with these key components:

1. **Permission Records**: Store grants from users/roles to objects
2. **Permission Cache**: LRU cache with TTL for performance
3. **View Security**: Stack-based context for nested view resolution
4. **Role Inheritance**: Users can have multiple roles with combined permissions

## Specification

### Data Structures

```cpp
// From catalog_manager.cpp:5216-5228 (PermissionRecord)
struct PermissionRecord {
    ID permission_id;        // UUIDv7 - unique permission identifier
    ID grantee_id;           // User/role UUID receiving permission
    ID grantor_id;           // User UUID who granted permission
    ID object_id;            // Object UUID (table, schema, etc.)
    uint32_t permissions;    // Bitmask of privileges
    uint8_t grant_option;    // 1 if WITH GRANT OPTION
    uint8_t object_type;     // ObjectType enum value
    // ... timestamps
};
```

```cpp
// From catalog_manager.cpp:5253-5264 (ColumnPermissionRecord)
struct ColumnPermissionRecord {
    ID permission_id;        // UUIDv7
    ID grantee_id;           // User/role UUID
    ID grantor_id;           // Who granted
    ID object_id;            // Table UUID
    uint32_t column_id;      // Column ordinal or ID
    uint32_t permissions;    // Column-specific bitmask (SELECT=1, UPDATE=2, REFERENCES=4)
    uint8_t grant_option;    // WITH GRANT OPTION
};
```

```cpp
// From /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/permission_cache.h
struct CacheKey {
    ID user_id;
    ID object_id;
    PermissionObjectType object_type;
    uint32_t privilege;
    
    bool operator==(const CacheKey& other) const;
};

struct CacheEntry {
    bool has_permission;
    std::chrono::steady_clock::time_point timestamp;
    uint64_t access_count;
    uint64_t policy_epoch_global;
    uint64_t policy_epoch_table;
};
```

```cpp
// From /home/dcalford/CliWork/ScratchBird/include/scratchbird/security/view_security.h:35-60
class ViewSecurityContext {
    uint32_t caller_id_;           // Original calling user
    uint32_t effective_user_id_;   // User to check permissions as
    ViewSecurityOptions options_;  // DEFINER/INVOKER, barrier, check_option
    
public:
    bool isDefiner() const { 
        return options_.mode == ViewSecurityMode::DEFINER; 
    }
    bool hasSecurityBarrier() const { 
        return options_.security_barrier; 
    }
};
```

### Permission Bitmasks

```cpp
// Object-level permissions (catalog_manager.cpp:5294)
enum ObjectPermissions : uint32_t {
    EXECUTE = 1,      // Execute function/procedure
    SELECT  = 2,      // Read data
    INSERT  = 4,      // Add data
    UPDATE  = 8,      // Modify data
    DELETE  = 16,     // Remove data
    USAGE   = 32,     // Use schema/sequence
    CREATE  = 64,     // Create objects
    DROP    = 128,    // Drop objects
    ALTER   = 256,    // Alter objects
    INDEX   = 512,    // Create indexes
    TRIGGER = 1024,   // Create triggers
    REFERENCES = 2048 // Foreign key references
};

// Schema default permissions (catalog_manager.cpp:14547)
const uint32_t DEFAULT_SCHEMA_PERMISSIONS = 0x0FFF; // Read, write, create
```

### Interface Contracts

#### Function: `PermissionCache::checkPermission()`

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/permission_cache.cpp:188
bool PermissionCache::checkPermission(
    CatalogManager* catalog,
    const CacheKey& key,
    PermissionCheckMode mode,
    ErrorContext* ctx);
```

**Preconditions:**
- Catalog manager is initialized and available
- Cache key contains valid user_id, object_id, and privilege

**Postconditions:**
- Returns true if user has permission, false otherwise
- Cache may be updated with new entry
- Statistics updated (hit/miss counts)

**Algorithm:**
1. Check SecurityQuorum decision
2. If cache allowed and mode == CACHED:
   - Look up in cache with epoch validation
   - Return cached result if valid
3. Query catalog directly (`catalog->hasPermission()`)
4. If cache allowed, insert result with current epochs
5. Return result

**Thread Safety:**
- Thread-safe via `std::shared_mutex`
- Shared lock for reads, exclusive lock for writes

#### Function: `ViewSecurityManager::enterView()`

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/security/view_security.cpp:153
ViewSecurityContext ViewSecurityManager::enterView(
    uint32_t view_id,
    uint32_t caller_id);
```

**Preconditions:**
- View is registered with security options
- Caller has permission to access view

**Postconditions:**
- New context pushed to thread-local stack
- Effective user determined by DEFINER/INVOKER mode

**Algorithm:**
1. Look up view options from registry
2. Determine effective_user_id:
   - DEFINER mode: use view owner_id
   - INVOKER mode: use caller_id
3. Create ViewSecurityContext
4. Push to current_stack_
5. Return context

#### Function: `CatalogManager::grantPermission()`

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp:47580
Status CatalogManager::grantPermission(
    const ID& grantor_id,
    const ID& grantee_id, 
    const ID& object_id,
    ObjectType object_type,
    uint32_t permissions,
    bool with_grant_option,
    ErrorContext* ctx);
```

**Preconditions:**
- Grantor has GRANT OPTION on the object
- Grantee exists (user, role, or group)
- Object exists and is valid type

**Postconditions:**
- Permission record created or updated
- Cache invalidated for affected user/object
- Epoch incremented for cache coherence

**Error Handling:**
- `PERMISSION_DENIED`: Grantor lacks GRANT OPTION
- `NOT_FOUND`: Grantee or object doesn't exist
- `CONSTRAINT_VIOLATION`: Duplicate permission with conflict

### Algorithms

#### Algorithm: Permission Check with Cache

```
Input:  user_id, object_id, privilege, check_mode
Output: Boolean - has permission

1. SECURITY QUORUM CHECK
   decision = quorum_.evaluate()
   if decision == DENY:
       return false
   cache_allowed = (decision == ALLOW_CACHE)

2. EPOCH LOOKUP (if cache_allowed)
   global_epoch = catalog->getSecurityPolicyEpoch()
   if object_type == TABLE:
       table_epoch = catalog->getTablePolicyEpoch(object_id)

3. CACHE LOOKUP (if mode == CACHED && cache_allowed)
   key = CacheKey{user_id, object_id, object_type, privilege}
   entry = cache_.find(key)
   
   if entry exists AND not expired:
       if entry.policy_epoch_global == global_epoch AND
          entry.policy_epoch_table == table_epoch:
           stats_.hit_count++
           return entry.has_permission
   
   stats_.miss_count++

4. DIRECT PERMISSION CHECK
   has_perm = catalog->hasPermission(
       user_id, object_id, object_type, privilege)

5. CACHE UPDATE (if cache_allowed)
   cache_.insert(key, has_perm, global_epoch, table_epoch)

6. RETURN has_perm
```

**Complexity:**
- Cache hit: O(1) hash lookup
- Cache miss: O(N) where N = number of permission records
- Space: O(max_entries) bounded by LRU limit

#### Algorithm: View Security Context Resolution

```
Input:  view_id, caller_id
Output: effective_user_id for permission checks

1. PUSH VIEW CONTEXT
   options = view_registry_.getOptions(view_id)
   
   if options.mode == DEFINER:
       effective_id = options.owner_id
   else:
       effective_id = caller_id
   
   context = ViewSecurityContext(caller_id, effective_id, options)
   stack_.push(context)

2. NESTED VIEW RESOLUTION
   for each nested_view in view_stack (innermost to outermost):
       if nested_view.isDefiner():
           return nested_view.owner_id  // First DEFINER wins
   
   return caller_id  // All INVOKER - use original caller

3. POP VIEW CONTEXT (on view exit)
   stack_.pop()
```

#### Algorithm: GRANT with Permission Merge

```
Input:  grantor, grantee, object, perms, with_grant_option
Output: Status

1. VALIDATE grantor has GRANT OPTION on object
   grantor_perms = getEffectivePermissions(grantor, object)
   if not (grantor_perms & GRANT_OPTION):
       return PERMISSION_DENIED

2. CHECK for existing permission record
   existing = findPermissionRecord(grantee, object)
   
   if existing exists:
       // Merge permissions
       new_perms = existing.permissions | perms
       if with_grant_option:
           existing.grant_option = true
       existing.permissions = new_perms
       updateRecord(existing)
   else:
       // Create new record
       record = PermissionRecord{
           permission_id = generateUUIDv7(),
           grantee_id = grantee,
           grantor_id = grantor,
           object_id = object,
           permissions = perms,
           grant_option = with_grant_option
       }
       insertRecord(record)

3. INVALIDATE CACHE
   permission_cache_.invalidateUser(grantee)
   permission_cache_.invalidateObject(object)
   incrementPolicyEpoch()

4. RETURN OK
```

### State Machines

```
Permission Cache Entry Lifecycle

┌───────────┐    Insert    ┌───────────┐
│  ABSENT   │ ───────────► │  ACTIVE   │
└───────────┘              └───────────┘
                                  │
                                  │ TTL expired
                                  ▼
                           ┌───────────┐
                    Access │  EXPIRED  │
                     ┌──── │           │
                     │     └───────────┘
                     │            │
                     │            │ Re-insert
                     │            ▼
                     │     ┌───────────┐
                     │     │  EVICTED  │
                     │     └───────────┘
                     │
                     └───────────────────► Refresh
```

| State | Condition | Action |
|-------|-----------|--------|
| ABSENT | Not in cache | Insert on first check |
| ACTIVE | Valid entry with matching epochs | Return cached value |
| EXPIRED | TTL exceeded | Remove, re-query catalog |
| STALE | Epoch mismatch | Remove, re-query catalog |
| EVICTED | LRU eviction | Remove oldest entries |

### Decision Trees

```
Permission Check
│
├─ SecurityQuorum DENY? ──Yes──► DENY
│
├─ Cache enabled and CACHED mode?
│   ├─ Entry in cache?
│   │   ├─ TTL valid? ──No──► Remove entry, continue
│   │   ├─ Epochs match? ──No──► Remove entry, continue
│   │   └─ Return cached result
│   └─ Cache miss ──► Continue to catalog check
│
├─ Query catalog permissions
│   ├─ Direct grant? ──Yes──► ALLOW
│   ├─ Role inheritance? ──Yes──► Check role permissions
│   ├─ Public grant? ──Yes──► ALLOW
│   └─ No grants ──► DENY
│
└─ Update cache with result (if caching enabled)
```

## Invariants

1. **Cache Consistency**: Cache entries are invalidated when permissions change
   - Verification: `invalidateUser()`, `invalidateObject()` called on GRANT/REVOKE
   - Epoch-based validation for distributed invalidation

2. **Permission Hierarchy**: Owners have all permissions on their objects
   - Verification: Owner check in `hasPermission()`

3. **GRANT OPTION Required**: Only users with GRANT OPTION can grant permissions
   - Verification: `enforce_permissions()` in catalog_manager.cpp:16618

4. **DEFINER View Isolation**: DEFINER views execute with owner's privileges
   - Verification: `ViewSecurityStack::effectiveUserId()` walks stack

5. **Atomic Permission Updates**: Permission changes are atomic
   - Verification: Single-record updates with cache invalidation

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `PERMISSION_DENIED` | User lacks required privilege | Request grant from owner |
| `PERMISSION_DENIED` | No GRANT OPTION for grantor | Request grant WITH GRANT OPTION |
| `NOT_FOUND` | Grantee/object doesn't exist | Verify names, retry |
| `CONSTRAINT_VIOLATION` | Duplicate permission | Use UPDATE instead of INSERT |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `test_security_issues.cpp` | Basic permission checks |
| `test_security_phase2.cpp` | Phase 2 security integration |
| `test_catalog_security_acl_abac_graph_token_quota_settings_contract.cpp` | ACL/ABAC tests |

## Related Specifications

- `authentication_flow.md` - User identity establishment
- `privilege_types.md` - All privilege type definitions
- `acl_format.md` - ACL storage format
- `default_privileges.md` - Default privileges system
- `rls_policy_enforcement.md` - Row-Level Security
- `cls_column_masking.md` - Column-Level Security
- `audit_logging.md` - Permission check logging

## Appendix

### Glossary

| Term | Definition |
|------|------------|
| GRANT | SQL command to give permissions to a user/role |
| REVOKE | SQL command to remove permissions |
| DEFINER | View security mode using view creator's privileges |
| INVOKER | View security mode using caller's privileges |
| Security Barrier | Prevents predicate push-down through views |
| Check Option | Validates DML through views satisfies WHERE clause |
| Epoch | Monotonic counter for cache invalidation |

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
