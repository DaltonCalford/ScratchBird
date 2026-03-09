# Specification: Triggers

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:11520`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:11543`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:11567`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp`

## Synopsis

This specification defines trigger metadata for both table-level triggers (DML triggers) and database-level triggers (Firebird-style session/transaction triggers).

## Scope

### In Scope

- Table trigger types (BEFORE, AFTER, INSTEAD OF)
- Trigger events (INSERT, UPDATE, DELETE)
- Trigger granularity (FOR EACH ROW, FOR EACH STATEMENT)
- Database triggers (ON CONNECT, ON TRANSACTION, etc.)
- Trigger procedure invocation
- Trigger ordering (POSITION)
- Trigger conditions (WHEN clause)

### Out of Scope

- Trigger procedure implementation (see PSQL specs)
- Trigger execution engine
- Trigger firing order resolution

## Specification

### Trigger Timing

**Source:** `include/scratchbird/core/catalog_manager.h:11520`

```cpp
enum class TriggerTiming : uint8_t {
    BEFORE = 0,      // Before operation
    AFTER = 1,       // After operation
    INSTEAD_OF = 2   // Replaces operation (views only)
};
```

**Timing Behavior:**

| Timing | When Fired | Can Modify Data | Can Cancel Operation |
|--------|------------|-----------------|---------------------|
| BEFORE | Before row operation | Yes | Yes (via exception) |
| AFTER | After row operation | Yes (other tables) | No |
| INSTEAD_OF | Replaces operation | Yes | N/A |

### Trigger Events

**Source:** `include/scratchbird/core/catalog_manager.h:11528`

```cpp
enum class TriggerEvent : uint8_t {
    INSERT = 0,
    UPDATE = 1,
    DELETE = 2
};
```

**Event Mask:**

```cpp
uint8_t event_mask = 0;
event_mask |= (1 << static_cast<uint8_t>(TriggerEvent::INSERT));
event_mask |= (1 << static_cast<uint8_t>(TriggerEvent::UPDATE));
// Trigger fires on INSERT or UPDATE
```

### Trigger Granularity

**Source:** `include/scratchbird/core/catalog_manager.h:11536`

```cpp
enum class TriggerGranularity : uint8_t {
    FOR_EACH_ROW = 0,       // Once per affected row
    FOR_EACH_STATEMENT = 1  // Once per statement (future)
};
```

### Database Trigger Events

**Source:** `include/scratchbird/core/catalog_manager.h:11567`

```cpp
enum class DatabaseTriggerEvent : uint8_t {
    ON_CONNECT = 0,              // Client connects
    ON_DISCONNECT = 1,           // Client disconnects
    ON_TRANSACTION_START = 2,    // Transaction starts
    ON_TRANSACTION_COMMIT = 3,   // Transaction commits
    ON_TRANSACTION_ROLLBACK = 4  // Transaction rolls back
};
```

### Table TriggerInfo Structure

**Source:** `include/scratchbird/core/catalog_manager.h:11543`

```cpp
struct TriggerInfo {
    // Identity
    ID trigger_id;                      // UUIDv7 trigger identifier
    std::string trigger_name;           // Trigger name
    bool name_is_delimited = false;     // Quoted identifier flag
    ID table_id;                        // Table this trigger is on
    std::string table_name;             // Table name (convenience)
    
    // Trigger characteristics
    TriggerTiming timing;               // BEFORE, AFTER, INSTEAD_OF
    uint8_t event_mask = 0;             // Bitmask of INSERT|UPDATE|DELETE
    TriggerGranularity granularity;     // FOR_EACH_ROW, FOR_EACH_STATEMENT
    
    // Procedure to invoke
    std::string procedure_name;         // Name of procedure to call
    
    // State
    bool enabled = true;                // Can be disabled
    
    // P2-8: Statement-level trigger support
    std::string old_table_alias;        // REFERENCING OLD TABLE AS
    std::string new_table_alias;        // REFERENCING NEW TABLE AS
    std::string when_expression;        // Optional WHEN condition
    
    // Metadata
    uint64_t created_time = 0;
};
```

### Database TriggerInfo Structure

**Source:** `include/scratchbird/core/catalog_manager.h:11577`

```cpp
struct DatabaseTriggerInfo {
    // Identity
    ID trigger_id;                      // UUIDv7 trigger identifier
    std::string trigger_name;           // Trigger name (unique)
    ID owner_id;                        // Owner UUID
    
    // Event
    DatabaseTriggerEvent event;         // Which event fires this trigger
    
    // State
    bool active = true;                 // ACTIVE vs INACTIVE
    int32_t position = 0;               // Execution order (lower first)
    
    // Procedure to invoke
    std::string procedure_name;         // Procedure to call: name()
    
    // Metadata
    uint64_t created_time = 0;
};
```

### Trigger SQL Syntax

```sql
-- Simple BEFORE INSERT trigger
CREATE TRIGGER trg_employees_bi
    BEFORE INSERT ON employees
    FOR EACH ROW
    EXECUTE PROCEDURE sp_set_created_date();

-- AFTER UPDATE trigger
CREATE TRIGGER trg_orders_au
    AFTER UPDATE ON orders
    FOR EACH ROW
    EXECUTE PROCEDURE sp_log_order_change();

-- Multi-event trigger
CREATE TRIGGER trg_audit_all
    BEFORE INSERT OR UPDATE OR DELETE ON audit_table
    FOR EACH ROW
    EXECUTE PROCEDURE sp_audit_trail();

-- Trigger with WHEN condition
CREATE TRIGGER trg_high_value_orders
    AFTER INSERT ON orders
    FOR EACH ROW
    WHEN (NEW.total_amount > 10000)
    EXECUTE PROCEDURE sp_notify_manager();

-- INSTEAD OF trigger (for views)
CREATE TRIGGER trg_view_io
    INSTEAD OF INSERT ON my_view
    FOR EACH ROW
    EXECUTE PROCEDURE sp_handle_view_insert();
```

### Database Trigger SQL Syntax

```sql
-- Connect trigger
CREATE TRIGGER trg_on_connect
    ON CONNECT
    EXECUTE PROCEDURE sp_log_connection();

-- Transaction trigger
CREATE TRIGGER trg_tx_start
    ON TRANSACTION START
    EXECUTE PROCEDURE sp_init_transaction_context();

-- Disconnect trigger
CREATE TRIGGER trg_on_disconnect
    ON DISCONNECT
    EXECUTE PROCEDURE sp_cleanup_session();
```

### sb_triggers Catalog Table

```cpp
struct TriggerRecord {
    // Primary key
    ID trigger_id;
    
    // Identity
    ID table_id;                    // NULL for database triggers
    char trigger_name[512];
    ID owner_id;
    uint8_t name_is_delimited;
    uint8_t reserved[7];
    
    // Trigger characteristics
    uint8_t timing;                 // TriggerTiming
    uint8_t event_mask;             // Bitmask of events
    uint8_t granularity;            // TriggerGranularity
    uint8_t is_database_trigger;    // 1 if database trigger
    
    // Database trigger specific
    uint8_t database_event;         // DatabaseTriggerEvent (if applicable)
    int32_t position;               // Execution order
    uint8_t active;
    uint8_t reserved2[2];
    
    // Procedure reference
    char procedure_name[512];
    
    // Statement-level aliases (P2-8)
    char old_table_alias[128];
    char new_table_alias[128];
    
    // WHEN condition
    ID when_expression_oid;         // TOAST reference
    
    // Metadata
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

### Trigger Firing Order

**Table Trigger Order:**

```
Statement: INSERT INTO employees (...)

1. BEFORE STATEMENT triggers (future)
2. For each row:
   a. BEFORE ROW triggers (by creation order)
      - trg_employees_bi (position implicit)
   b. Perform INSERT
   c. AFTER ROW triggers (by creation order)
      - trg_employees_ai
3. AFTER STATEMENT triggers (future)
```

**Position-Based Ordering (Database Triggers):**

```sql
-- Lower position fires first
CREATE TRIGGER trg_first ON CONNECT POSITION 0 ...
CREATE TRIGGER trg_second ON CONNECT POSITION 10 ...
CREATE TRIGGER trg_third ON CONNECT POSITION 20 ...
```

### Trigger Context Variables

**Available in trigger procedures:**

| Variable | Description | Available In |
|----------|-------------|--------------|
| OLD | Old row values | UPDATE, DELETE |
| NEW | New row values | INSERT, UPDATE |
| TG_NAME | Trigger name | All |
| TG_WHEN | BEFORE/AFTER/INSTEAD_OF | All |
| TG_LEVEL | ROW/STATEMENT | All |
| TG_OP | INSERT/UPDATE/DELETE | All |
| TG_TABLE_NAME | Table name | Table triggers |
| TG_TABLE_SCHEMA | Schema name | Table triggers |
| TG_NARGS | Number of arguments | All |
| TG_ARGV[] | Trigger arguments | All |

## Algorithms

### Algorithm: Fire Table Trigger

```
Input:  Table ID, operation (INSERT/UPDATE/DELETE), 
        timing (BEFORE/AFTER), row data
Output: Modified row or error

1. Build trigger list:
   a. SELECT * FROM sb_triggers
      WHERE table_id = ?
      AND timing = ?
      AND (event_mask & ?) != 0
      AND granularity = FOR_EACH_ROW
      AND enabled = true
   b. Order by created_time

2. For each trigger:
   a. If trigger has WHEN condition:
      - Evaluate condition with OLD/NEW
      - If false: skip trigger
   
   b. Set up trigger context:
      - TG_NAME = trigger_name
      - TG_WHEN = timing
      - TG_OP = operation
      - OLD/NEW = row data
   
   c. Execute procedure_name
   
   d. If BEFORE trigger:
      - Capture modified NEW values
      - If exception raised: abort operation
   
   e. If AFTER trigger:
      - Changes affect other tables only
      - Cannot modify current row

3. Return modified row (for BEFORE)
   or success (for AFTER)
```

### Algorithm: Fire Database Trigger

```
Input:  Event type (ON_CONNECT, etc.), connection/transaction context
Output: Success or error

1. Build trigger list:
   a. SELECT * FROM sb_triggers
      WHERE is_database_trigger = 1
      AND database_event = ?
      AND active = true
   b. ORDER BY position ASC

2. For each trigger:
   a. Set up context:
      - TG_NAME = trigger_name
      - TG_OP = event_name
   
   b. Execute procedure_name
   
   c. If exception raised:
      - Log error
      - Continue with next trigger (unless critical)

3. Return success
```

### Algorithm: Create Trigger

```
Input:  Table ID (or database), trigger name, timing, events,
        procedure name, options
Output: Trigger ID

1. Validate table exists (if table trigger)
2. Validate procedure exists
3. Check for naming conflicts
4. Generate UUIDv7 for trigger_id
5. If WHEN condition provided:
   a. Parse and validate
   b. Store in TOAST if needed
6. Create TriggerRecord
7. If database trigger:
   a. Verify event type valid
   b. Assign position if not specified
8. Commit transaction
```

## Invariants

| ID | Invariant | Verification |
|----|-----------|-------------|
| `TRG_INV_001` | trigger_id is valid UUIDv7 | isUuidV7Local() check |
| `TRG_INV_002` | table_id references valid table (if table trigger) | Foreign key |
| `TRG_INV_003` | procedure_name references valid procedure | Procedure lookup |
| `TRG_INV_004` | INSTEAD_OF only on views | Table type check |
| `TRG_INV_005` | Database trigger names are unique | Unique index |
| `TRG_INV_006` | Position values are non-negative | Validation |

## Error Handling

| Error Code | Condition | Recovery |
|------------|-----------|----------|
| `TRIGGER_EXISTS` | Name conflict | Choose different name |
| `INVALID_TIMING` | Invalid timing for table type | Correct timing |
| `PROCEDURE_NOT_FOUND` | Referenced procedure doesn't exist | Create procedure |
| `INSTEAD_OF_ON_TABLE` | INSTEAD_OF on regular table | Use on view only |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_triggers.cpp` | Trigger CRUD |
| `tests/unit/test_trigger_firing.cpp` | Trigger execution order |
| `tests/unit/test_database_triggers.cpp` | Database triggers |
| `tests/unit/test_trigger_when.cpp` | WHEN conditions |

## Related Specifications

- [functions.md](./functions.md) - Trigger procedures
- [tables.md](./tables.md) - Table triggers
- [views.md](./views.md) - INSTEAD OF triggers

## Appendix

### Trigger Record Size

| Component | Size |
|-----------|------|
| Header | 48 bytes |
| Identity | 544 bytes |
| Characteristics | 16 bytes |
| Procedure ref | 512 bytes |
| Aliases | 256 bytes |
| Metadata | 16 bytes |
| **Total** | **~1392 bytes** |

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
