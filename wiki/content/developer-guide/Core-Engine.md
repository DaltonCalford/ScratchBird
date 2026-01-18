# Core Engine

**Purpose:** Documents the ScratchBird core engine - the sole authority for SBLR validation, security enforcement, and query execution.

**Status:** Alpha documentation (in progress)

---

## Overview

The core engine is the trusted heart of ScratchBird. It receives SBLR bytecode from parsers, validates it, enforces security policies, and executes queries against the storage layer. The engine is **100% dialect-agnostic** - it only understands SBLR.

```
┌─────────────────────────────────────────────────────────────┐
│                      SERVER CORE                             │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐          │
│  │  SBLR       │  │ Transaction │  │  Storage    │          │
│  │  Executor   │  │  Manager    │  │  Engine     │          │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘          │
│         │                │                │                  │
│         └────────────────┴────────────────┘                  │
│                          │                                   │
│                   ┌──────▼──────┐                            │
│                   │   Catalog   │                            │
│                   │   Manager   │                            │
│                   └─────────────┘                            │
└─────────────────────────────────────────────────────────────┘
```

---

## Component Architecture

### SBLR Executor

**Location:** `src/sblr/executor.cpp`

The executor interprets SBLR bytecode and coordinates with other engine components. It:

- Validates SBLR bytecode before execution
- Interprets 500+ opcodes
- Manages execution context and stack
- Coordinates with transaction manager for visibility
- Returns native result sets

**Key Files:**
- `include/scratchbird/sblr/executor.h` - Public API
- `include/scratchbird/sblr/opcodes.h` - Opcode definitions
- `src/sblr/executor.cpp` - Implementation

**Execution Flow:**
```
SBLR Bytecode
    │
    ▼
┌─────────────┐
│  Validate   │ ← Check bytecode integrity
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  Decode     │ ← Parse opcodes and operands
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  Execute    │ ← Run operations, check visibility
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  Return     │ ← Native result set
└─────────────┘
```

---

### Semantic Analyzer

**Location:** `src/sblr/semantic_analyzer_v2.cpp`

Performs semantic analysis on the AST before bytecode generation:

- Type checking and inference
- Name resolution (tables, columns, functions)
- Schema validation
- Constraint verification

**Key Files:**
- `include/scratchbird/sblr/semantic_analyzer_v2.h`
- `include/scratchbird/sblr/resolved_ast_v2.h`
- `src/sblr/semantic_analyzer_v2.cpp`

---

### Bytecode Generator

**Location:** `src/sblr/bytecode_generator_v2.cpp`

Converts the resolved AST into SBLR bytecode:

- Generates optimized opcode sequences
- Handles expression compilation
- Produces binary bytecode stream

**Key Files:**
- `include/scratchbird/sblr/bytecode_generator_v2.h`
- `src/sblr/bytecode_generator_v2.cpp`

---

### Transaction Manager

**Location:** `src/core/`

Implements Firebird-style MGA (Multi-Generational Architecture). See [Transactions](Transactions.md) for detailed documentation.

**Key Responsibilities:**
- Transaction lifecycle (begin, commit, rollback)
- TIP (Transaction Inventory Page) management
- Visibility determination via TIP lookups (NOT snapshots)
- OIT/OAT/OST marker management
- Sweep coordination

---

### Storage Engine

**Location:** `src/core/storage_engine.cpp`

Manages data storage with MGA-first design. See [Storage](Storage.md) for detailed documentation.

**Key Responsibilities:**
- Buffer pool management
- Heap page operations
- Back-versioning for MGA
- Index coordination

---

### Catalog Manager

**Location:** `src/core/catalog_manager.cpp`

Manages database metadata (system catalog):

- Table and column definitions
- Index metadata
- Constraint information
- User and privilege data

**Key Files:**
- `include/scratchbird/core/catalog_manager.h`
- `src/core/catalog_manager.cpp`

---

## Query Execution Pipeline

### Complete Pipeline

```
SQL Text (from client)
        │
        ▼
┌──────────────────┐
│  Parser Layer    │  Parse SQL, generate AST
│  (per dialect)   │
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│  Semantic        │  Resolve names, check types
│  Analyzer        │
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│  Query Planner   │  Generate execution plan
│  (Optimizer)     │
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│  Bytecode        │  Generate SBLR opcodes
│  Generator       │
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│  SBLR Executor   │  Execute bytecode
│  (Engine Core)   │
└────────┬─────────┘
         │
         ▼
Native Result Set
```

### Example: SELECT Execution

```sql
SELECT id, name FROM users WHERE active = true;
```

**Bytecode (simplified):**
```
OP_BEGIN_SELECT
OP_TABLE_REF "users"
OP_COLUMN_REF "id"
OP_COLUMN_REF "name"
OP_COLUMN_REF "active"
OP_LITERAL_BOOL true
OP_COMPARE_EQ
OP_WHERE
OP_EXECUTE
OP_END
```

**Execution Steps:**
1. Executor validates bytecode
2. Acquires shared lock on `users` table
3. Opens heap scan with visibility filter (MGA)
4. For each visible row, evaluates WHERE predicate
5. Projects requested columns
6. Returns result set

---

## Thread Safety

The engine is designed for concurrent access:

### Lock Hierarchy

```
Database Lock (exclusive for DDL)
    │
    ├── Table Lock (shared for reads, exclusive for schema changes)
    │       │
    │       └── Page Lock (latches for buffer pool)
    │               │
    │               └── Record Lock (row-level via MGA)
    │
    └── Catalog Lock (shared for queries, exclusive for DDL)
```

### Concurrency Model

- **Read-Read:** Fully concurrent (MGA provides visibility)
- **Read-Write:** Concurrent (readers see committed versions)
- **Write-Write:** Serialized per row (back-versioning)

---

## Index Subsystem

ScratchBird supports 11+ index types:

| Index Type | Use Case | Status |
|------------|----------|--------|
| B-Tree | General purpose, range queries | Alpha |
| Hash | Equality-only lookups | Alpha |
| Bitmap | Low-cardinality columns | Alpha |
| GIN | Full-text, arrays, JSON | Alpha |
| GIST | Spatial data | Alpha |
| SP-GIST | Partitioned spatial | Alpha |
| BRIN | Large sequential tables | Planned |
| Bloom | Probabilistic membership | Planned |
| UUID v7 optimized | Time-sorted UUIDs | Planned |

**Key Files:**
- `include/scratchbird/core/bitmap_index.h`
- `src/core/bitmap_index.cpp`
- `src/core/index_factory.cpp`
- `src/core/spgist_index.cpp`

---

## Scheduler and Jobs

**Location:** `docs/specifications/scheduler/`

The engine includes a job scheduler for:

- Background maintenance (sweep, statistics)
- Scheduled tasks
- Async operations

---

## Error Handling

Errors are propagated through a consistent error context:

```cpp
struct ErrorContext {
    int error_code;
    std::string message;
    std::string detail;
    std::string hint;
    std::string where;  // Stack trace
};
```

**Error Categories:**
- `SB_ERROR_SYNTAX` (0x1xxx) - Parse errors
- `SB_ERROR_SEMANTIC` (0x2xxx) - Type/name resolution errors
- `SB_ERROR_EXECUTION` (0x3xxx) - Runtime errors
- `SB_ERROR_CONSTRAINT` (0x4xxx) - Constraint violations
- `SB_ERROR_TRANSACTION` (0x5xxx) - Transaction errors
- `SB_ERROR_STORAGE` (0x6xxx) - I/O errors

---

## Performance Considerations

### Buffer Pool

The buffer pool caches frequently accessed pages:

```cpp
// Configuration
buffer_pool_size = 256MB;       // Default
page_size = 8192;               // 8KB default
max_dirty_pages = 0.9;          // 90% threshold for checkpoint
```

### Statistics

The engine maintains statistics for query optimization:

- Row counts per table
- Column cardinality
- Value histograms
- Index selectivity

---

## Diagnostic Tools

### EXPLAIN

View query execution plans:

```sql
EXPLAIN SELECT * FROM users WHERE id = 1;
EXPLAIN ANALYZE SELECT * FROM users WHERE id = 1;
```

### System Catalog Queries

```sql
-- List tables
SELECT * FROM sb_catalog.tables;

-- List indexes
SELECT * FROM sb_catalog.indexes;

-- View statistics
SELECT * FROM sb_catalog.statistics WHERE table_name = 'users';
```

---

## Source Code Reference

| Component | Header | Implementation |
|-----------|--------|----------------|
| Executor | `include/scratchbird/sblr/executor.h` | `src/sblr/executor.cpp` |
| Semantic Analyzer | `include/scratchbird/sblr/semantic_analyzer_v2.h` | `src/sblr/semantic_analyzer_v2.cpp` |
| Bytecode Generator | `include/scratchbird/sblr/bytecode_generator_v2.h` | `src/sblr/bytecode_generator_v2.cpp` |
| Query Planner | `include/scratchbird/optimizer/query_planner.h` | `src/optimizer/query_planner.cpp` |
| Storage Engine | `include/scratchbird/core/storage_engine.h` | `src/core/storage_engine.cpp` |
| Catalog Manager | `include/scratchbird/core/catalog_manager.h` | `src/core/catalog_manager.cpp` |
| Buffer Pool | `src/core/buffer_pool.cpp` | |
| Connection Context | `include/scratchbird/core/connection_context.h` | `src/core/connection_context.cpp` |

---

## Related Documents

- [Architecture](Architecture.md) - Overall system architecture
- [Storage](Storage.md) - Storage engine details
- [Transactions](Transactions.md) - MGA transaction system
- [SBLR](SBLR.md) - Bytecode specification
