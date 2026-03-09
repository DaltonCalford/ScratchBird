# Specification: Savepoints and Subtransactions

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | storage |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird Alpha |
| **Authors** | Dalton Calford |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/transaction_manager.h`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_savepoints.cpp`

## Synopsis

This specification defines savepoints (nested transactions) which allow partial rollback within a transaction. Savepoints provide fine-grained error recovery without aborting the entire transaction.

## Scope

### In Scope

- Savepoint creation and naming
- Partial rollback to savepoint
- Savepoint release
- Savepoint stack management
- Resource tracking per savepoint

### Out of Scope

- True nested transactions (independent commit/abort)
- Distributed savepoints
- Automatic savepoints (statement-level)

## Background

Savepoints in ScratchBird:
- **Stack-based**: LIFO (Last In, First Out) semantics
- **Memory-only**: Savepoint state in backend memory
- **Resource tracking**: Pages locked, buffers pinned tracked per level
- **Rollback scope**: Only data changes, not catalog changes

## Specification

### Data Structures

#### Savepoint Structure

```cpp
struct Savepoint {
    std::string name;              // Savepoint name
    uint32_t level;                // Nesting level (0 = outer)
    uint64_t start_time;           // Creation timestamp
    
    // Resource state at savepoint creation
    std::vector<GPID> locked_pages;     // Pages locked
    std::vector<uint32_t> buffer_pins;  // Buffer pins
    std::vector<ID> temp_objects;       // Temp objects created
    
    // Transaction state
    uint64_t nrows_modified;       // Rows modified count
    uint64_t nbytes_allocated;     // Space allocated
};
```

#### Savepoint Stack

```cpp
class TransactionManager {
    // Per-backend savepoint stack
    struct SavepointStack {
        std::vector<Savepoint> stack;  // Active savepoints
        uint32_t next_level = 0;       // Next level number
        
        bool hasSavepoint(const std::string& name) const;
        Savepoint* findSavepoint(const std::string& name);
        void popToLevel(uint32_t level);
    };
    
    std::unordered_map<uint32_t, SavepointStack> backend_stacks_;
};
```

### Interface Contracts

#### Function: `createSavepoint()`

```cpp
Status TransactionManager::createSavepoint(
    uint32_t proc_id,              // Backend ID
    const std::string& name,       // Savepoint name
    ErrorContext *ctx
);
```

**Preconditions:**
- Backend has active transaction
- Savepoint name unique within transaction (or replace)

**Postconditions:**
- Savepoint created at current nesting level
- Resource state captured
- Level incremented

**Algorithm:**
```
1. Verify active transaction for proc_id
2. Get or create SavepointStack for proc_id

3. IF stack.hasSavepoint(name):
4.     IF replace_allowed:
5.         Release existing savepoint with same name
6.     ELSE:
7.         RETURN DUPLICATE_NAME

8. savepoint.level = stack.next_level++
9. savepoint.name = name
10. savepoint.start_time = now()

11. // Capture resource state
12. savepoint.locked_pages = getLockedPages(proc_id)
13. savepoint.buffer_pins = getBufferPins(proc_id)
14. savepoint.nrows_modified = getRowsModified(proc_id)

15. stack.stack.push_back(savepoint)
16. RETURN OK
```

#### Function: `rollbackToSavepoint()`

```cpp
Status TransactionManager::rollbackToSavepoint(
    uint32_t proc_id,
    const std::string& name,
    ErrorContext *ctx
);
```

**Preconditions:**
- Savepoint exists with given name
- No savepoints between current and target (LIFO)

**Postconditions:**
- All changes since savepoint undone
- Resources released
- Later savepoints removed

**Algorithm:**
```
1. Get SavepointStack for proc_id
2. target = stack.findSavepoint(name)
3. IF target == nullptr:
4.     RETURN NOT_FOUND

5. // Verify no intervening savepoints (LIFO)
6. IF target->level != stack.stack.back().level:
7.     RETURN INVALID_STATE  // Must rollback nested first

8. // Rollback changes
9. FOR each change IN getChangesSince(target->level):
10.    undoChange(change)

11. // Release resources acquired since savepoint
12. FOR each page IN getLockedPagesSince(target->level):
13.    unlockPage(proc_id, page)

14. FOR each pin IN getBufferPinsSince(target->level):
15.    unpinBuffer(pin)

16. // Restore state
17. setRowsModified(proc_id, target->nrows_modified)

18. // Pop savepoints at and above this level
19. stack.popToLevel(target->level)
20. stack.next_level = target->level + 1

21. RETURN OK
```

#### Function: `releaseSavepoint()`

```cpp
Status TransactionManager::releaseSavepoint(
    uint32_t proc_id,
    const std::string& name,
    ErrorContext *ctx
);
```

**Preconditions:**
- Savepoint exists with given name

**Postconditions:**
- Savepoint removed from stack
- Changes preserved (not rolled back)
- Resources merged to parent level

**Algorithm:**
```
1. Get SavepointStack for proc_id
2. target = stack.findSavepoint(name)
3. IF target == nullptr:
4.     RETURN NOT_FOUND

5. // Can only release most recent savepoint
6. IF target->level != stack.stack.back().level:
7.     RETURN INVALID_STATE

8. // Merge resource tracking to parent (if any)
9. IF stack.stack.size() > 1:
10.    parent = &stack.stack[stack.stack.size() - 2]
11.    parent->nrows_modified += target->nrows_modified
12.    // Locked pages remain locked (no change)

13. // Remove savepoint
14. stack.stack.pop_back()
15. stack.next_level--

16. RETURN OK
```

### Savepoint Stack State Machine

```
Savepoint Stack Operations:

[Transaction Start]
       │
       ▼ CREATE SAVEPOINT sp1
   [sp1 level=0]
       │
       ▼ CREATE SAVEPOINT sp2
  [sp1, sp2]
       │
       ▼ ROLLBACK TO sp1
   [sp1]          (sp2 removed, changes undone)
       │
       ▼ CREATE SAVEPOINT sp3
  [sp1, sp3]
       │
       ▼ RELEASE SAVEPOINT sp3
   [sp1]          (changes preserved)
       │
       ▼ COMMIT
      [END]
```

### Undo Operations

| Operation Type | Undo Action |
|----------------|-------------|
| INSERT | Delete the inserted tuple |
| DELETE | Restore tuple from back version |
| UPDATE | Restore previous version |
| DDL | Catalog changes not rolled back (limitation) |

## Invariants

1. **LIFO Order**: Savepoints released/rolled back in reverse creation order
   - Verification: Check level == stack.back().level
   
2. **Resource Cleanup**: All resources acquired after savepoint released on rollback
   - Verification: Track and release in rollback
   
3. **Savepoint Names Unique**: Within a transaction, names are unique
   - Verification: Check before creation

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `NOT_FOUND` | Savepoint doesn't exist | Return error |
| `INVALID_STATE` | Non-LIFO rollback/release | Return error |
| `DUPLICATE_NAME` | Savepoint name exists | Return error or replace |

## Limitations

1. **DDL Not Rolled Back**: CREATE, DROP, ALTER statements persist
2. ** advisory locks**: Not released on savepoint rollback
3. **Notifications**: LISTEN/NOTIFY not rolled back

## Performance Considerations

### Memory Usage
- **Per savepoint**: ~1KB + resource tracking
- **Typical stack**: 1-5 savepoints
- **Deep nesting**: 100+ savepoints possible but not recommended

### Rollback Cost
- **Data changes**: Proportional to changes since savepoint
- **Resource release**: O(resources acquired)
- **No fsync required**: Memory-only operation

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_savepoints.cpp` | Basic operations |
| `tests/unit/test_savepoints_nested.cpp` | Deep nesting |
| `tests/unit/test_savepoints_rollback.cpp` | Rollback semantics |
| `tests/unit/test_savepoints_limits.cpp` | Edge cases |

## Related Specifications

- [Transaction States](./transaction_states.md) - Parent transactions
- [Transaction Lifecycle](./transaction_lifecycle.md) - Transaction management

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | AI Agent |
