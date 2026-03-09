# Specification: Sequences (Generators)

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | catalog |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 0.1.0 |
| **Authors** | ScratchBird Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:510`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:533`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp`

## Synopsis

This specification defines sequence (generator) metadata and behavior, including sequence state, caching, ownership by columns, and the sb_sequences catalog table layout.

## Scope

### In Scope

- Sequence metadata structures (SequenceInfo, SequenceState)
- Sequence parameters (START, INCREMENT, MINVALUE, MAXVALUE)
- Sequence caching
- IDENTITY column ownership
- Sequence access patterns (NEXTVAL, CURRVAL, SETVAL)
- Generator pages (physical storage)

### Out of Scope

- IDENTITY column DDL (see `columns.md`)
- Sequence-based default values (see `columns.md`)

## Specification

### SequenceInfo Structure

**Source:** `include/scratchbird/core/catalog_manager.h:510`

```cpp
struct SequenceInfo {
    // Identity
    ID sequence_id;                 // UUIDv7 sequence identifier
    ID schema_id;                   // Containing schema
    std::string name;               // Sequence name
    bool name_is_delimited = false; // Quoted identifier flag
    ID owner_id;                    // Owner UUID
    
    // Ownership (for IDENTITY columns)
    ID owned_by_table_id{};         // Table that owns this sequence
    ID owned_by_column_id{};        // Column that owns this sequence
    
    // Sequence parameters
    int64_t current_value;          // Current value
    int64_t increment_by;           // Increment (can be negative)
    int64_t min_value;              // Minimum value
    int64_t max_value;              // Maximum value
    int64_t start_value;            // START WITH value
    int64_t cache_size;             // Cache size (1 = no cache)
    bool cycle;                     // CYCLE vs NO CYCLE
    
    // Metadata
    uint64_t created_time;
    uint64_t last_modified_time;
    
    // Temporary table support
    TempMetadataScope temp_metadata_scope = TempMetadataScope::NONE;
    ID creating_session_id{};
    uint64_t creating_transaction_id = 0;
};
```

### SequenceState Structure

**Source:** `include/scratchbird/core/catalog_manager.h:533`

```cpp
struct SequenceState {
    // Identity (copied from SequenceInfo)
    ID sequence_id;
    ID schema_id;
    std::string name;
    bool name_is_delimited = false;
    ID owner_id;
    ID owned_by_table_id{};
    ID owned_by_column_id{};
    
    // Mutable state (atomic operations)
    std::atomic<int64_t> current_value;
    
    // Configuration (protected by config_mutex)
    int64_t increment_by;
    int64_t min_value;
    int64_t max_value;
    int64_t start_value = 0;
    int64_t cache_size = 1;
    bool cycle;
    
    // Metadata
    uint64_t created_time = 0;
    uint64_t last_modified_time = 0;
    TempMetadataScope temp_metadata_scope = TempMetadataScope::NONE;
    ID creating_session_id{};
    uint64_t creating_transaction_id = 0;
    
    // Synchronization
    std::mutex config_mutex;  // Protect ALTER SEQUENCE changes
};
```

### Sequence Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| START WITH | 1 | Initial value |
| INCREMENT BY | 1 | Step size (positive/negative) |
| MINVALUE | 1 or -2^63 | Minimum allowed value |
| MAXVALUE | 2^63-1 | Maximum allowed value |
| CACHE | 1 | Values to pre-allocate |
| CYCLE | NO CYCLE | Wrap around at bounds |

### Sequence SQL Syntax

```sql
-- Create sequence
CREATE SEQUENCE order_seq
    START WITH 1000
    INCREMENT BY 1
    MINVALUE 1000
    MAXVALUE 999999999
    CACHE 20
    NO CYCLE;

-- Use sequence
SELECT NEXTVAL('order_seq');     -- Get next value
SELECT CURRVAL('order_seq');     -- Get current value
SELECT SETVAL('order_seq', 5000); -- Set current value

-- Alter sequence
ALTER SEQUENCE order_seq 
    INCREMENT BY 10
    CACHE 50;

-- Drop sequence
DROP SEQUENCE order_seq;
DROP SEQUENCE IF EXISTS order_seq;
```

### Generator Page Layout

Sequences are stored in dedicated catalog pages with atomic update capability:

```cpp
struct GeneratorPage {
    PageHeader header;
    uint32_t generator_count;
    uint32_t reserved;
    
    struct GeneratorEntry {
        ID sequence_id;
        int64_t current_value;
        int64_t increment_by;
        int64_t min_value;
        int64_t max_value;
        int64_t cache_size;
        uint8_t cycle;
        uint8_t reserved[7];
        uint64_t last_accessed;
    };
    
    GeneratorEntry entries[];
};
```

### sb_sequences Catalog Table

```cpp
struct SequenceRecord {
    // Primary key
    ID sequence_id;
    
    // Identity
    ID schema_id;
    char name[512];
    ID owner_id;
    uint8_t name_is_delimited;
    uint8_t reserved[7];
    
    // Ownership (for IDENTITY columns)
    ID owned_by_table_id;
    ID owned_by_column_id;
    
    // Sequence parameters
    int64_t current_value;
    int64_t increment_by;
    int64_t min_value;
    int64_t max_value;
    int64_t start_value;
    int64_t cache_size;
    uint8_t cycle;
    
    // Temporary table support
    uint8_t temp_metadata_scope;
    uint8_t reserved2[7];
    ID creating_session_id;
    uint64_t creating_transaction_id;
    
    // Metadata
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

### Sequence Caching

**Without Cache (CACHE = 1):**
```
Each NEXTVAL:
1. Read current_value from disk
2. Increment
3. Write new value to disk
4. Return value
```

**With Cache (CACHE = N):**
```
First NEXTVAL after cache exhausted:
1. Allocate N values from disk
2. Store cache range in memory
3. Return first value

Subsequent NEXTVAL (N-1 times):
1. Return value from memory cache
2. No disk I/O
```

**Cache Implementation:**

```cpp
struct SequenceCache {
    ID sequence_id;
    int64_t cache_start;    // First value in cache
    int64_t cache_end;      // Last value in cache (exclusive)
    int64_t next_value;     // Next value to hand out
    std::mutex mutex;       // Thread safety
};
```

### IDENTITY Column Ownership

```sql
-- IDENTITY column creates owned sequence
CREATE TABLE users (
    user_id INTEGER GENERATED ALWAYS AS IDENTITY
        (START WITH 1 INCREMENT BY 1),
    username VARCHAR(50)
);

-- Results in:
-- Column: user_id (is_identity=true, identity_always=true)
-- Sequence: auto-generated name, owned_by_table_id=users.id, 
--           owned_by_column_id=user_id.id
```

**Ownership Rules:**
1. Owned sequence dropped automatically when column dropped
2. Owned sequence renamed when column renamed
3. Cannot manually drop owned sequence (drop column instead)

### Sequence Access Functions

| Function | Description | Persistence |
|----------|-------------|-------------|
| `NEXTVAL(seq)` | Get and increment | Updates state |
| `CURRVAL(seq)` | Get current | No change |
| `LASTVAL()` | Get last NEXTVAL | Session variable |
| `SETVAL(seq, val)` | Set current | Updates state |
| `SETVAL(seq, val, bool)` | Set and optionally is_called | Updates state |

## Algorithms

### Algorithm: NEXTVAL

```
Input:  Sequence ID
Output: Next sequence value

1. Look up SequenceState in cache
2. If not cached:
   a. Load from sb_sequences
   b. Create SequenceState
   c. Add to cache

4. Check if cache exhausted:
   a. If cache_size == 1:
      - Lock sequence
      - Read current_value from disk
      - Compute next = current + increment
      - Check bounds (min/max, cycle)
      - Write next to disk
      - Unlock
      - Return next
   
   b. If cache_size > 1:
      - Lock sequence
      - If next_value >= cache_end:
         * Allocate new cache from disk
         * cache_start = disk_value
         * cache_end = disk_value + (cache_size * increment)
         * Write cache_end to disk
      - value = next_value
      - next_value += increment
      - Unlock
      - Return value
```

### Algorithm: Check Bounds

```
Input:  Proposed value, sequence parameters
Output: Adjusted value or error

1. If increment > 0:
   a. If value > max_value:
      - If cycle: return min_value
      - Else: error "sequence exceeds maximum"
   
2. If increment < 0:
   a. If value < min_value:
      - If cycle: return max_value
      - Else: error "sequence exceeds minimum"

3. Return value
```

### Algorithm: ALTER SEQUENCE

```
Input:  Sequence ID, new parameters
Output: Success/Failure

1. Acquire config_mutex
2. Validate new parameters:
   a. min_value < max_value
   b. increment != 0
   c. cache_size >= 1
3. Apply changes to SequenceState
4. Persist to sb_sequences
5. Invalidate cache entries
6. Release config_mutex
7. Return success
```

## Invariants

| ID | Invariant | Verification |
|----|-----------|-------------|
| `SEQ_INV_001` | sequence_id is valid UUIDv7 | isUuidV7Local() check |
| `SEQ_INV_002` | min_value < max_value | Parameter validation |
| `SEQ_INV_003` | increment_by != 0 | Parameter validation |
| `SEQ_INV_004` | cache_size >= 1 | Parameter validation |
| `SEQ_INV_005` | Owned sequences have valid table/column refs | Referential check |
| `SEQ_INV_006` | current_value within [min, max] | Bounds check |

## Error Handling

| Error Code | Condition | Recovery |
|------------|-----------|----------|
| `SEQUENCE_NOT_FOUND` | Sequence doesn't exist | Check name |
| `SEQUENCE_EXHAUSTED` | Exceeded max/min without CYCLE | ALTER SEQUENCE |
| `INVALID_PARAMETER` | Invalid sequence parameter | Correct parameter |
| `SEQUENCE_IN_USE` | Cannot drop owned sequence | Drop column instead |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_sequences.cpp` | Sequence CRUD |
| `tests/unit/test_sequence_cache.cpp` | Caching behavior |
| `tests/unit/test_sequence_concurrent.cpp` | Concurrent access |
| `tests/unit/test_identity_columns.cpp` | IDENTITY ownership |

## Related Specifications

- [columns.md](./columns.md) - IDENTITY columns
- [tables.md](./tables.md) - Table metadata

## Appendix

### Sequence Record Size

| Component | Size |
|-----------|------|
| Header | 48 bytes |
| Identity | 544 bytes |
| Parameters | 56 bytes |
| Ownership | 32 bytes |
| Temporary | 24 bytes |
| Metadata | 16 bytes |
| **Total** | **~720 bytes** |

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
