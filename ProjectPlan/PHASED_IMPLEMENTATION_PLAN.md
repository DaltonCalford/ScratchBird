# ScratchBird Phased Implementation Plan
## From Alpha to Production - Building with the End in Mind

## Overview

Each phase builds on the previous, with hooks and stubs for future functionality. Developers know what's coming and can prepare infrastructure accordingly.

---

## ALPHA PHASE 1: Foundation (1.01 - 1.05)

### Alpha 1.01 - Engine Bootstrap & Database Creation
**Goal**: Minimal viable engine that creates valid database files

**Deliverables**:
```cpp
// Engine responds to version query
scratchbird --version  // Returns: "ScratchBird Alpha 1.01"

// Create database with specified page size
scratchbird create database mydb.sdb --page-size=8192
scratchbird create database test.sdb --page-size=16384
```

**Database File Structure**:
```
Page 0: Header Page
- Magic number: "SCRATCHBIRD\x00\x01"
- Version: 1.01
- Page size: 8192/16384/32768
- Page count
- Schema tree root page
- System catalog root page
- Free page list head
- Transaction inventory page (TIP) location
- UUID of database

Page 1: System Catalog Root
- Tables table (sys_tables)
- Columns table (sys_columns)
- Indexes table (sys_indexes)
- Schemas table (sys_schemas)
- All with proper UUID assignments

Page 2: Schema Tree Root
- [root] node with UUID
- [sys] node with UUID
- [sec] node with UUID
- [agents] node with UUID
- [app] node with UUID
- [remote] node with UUID
- [users] node with UUID
- [roles] node with UUID

Page 3-N: Initial system tables data
- Populated with base schema structure
- All objects have UUIDs assigned
- Parent-child relationships established
```

**Tests**:
```cpp
TEST(Alpha101, EngineVersion) {
    EXPECT_EQ(Engine::version(), "1.01");
}

TEST(Alpha101, CreateDatabase) {
    vector<size_t> page_sizes = {8192, 16384, 32768};
    
    for (size_t page_size : page_sizes) {
        string db_path = "test_" + to_string(page_size) + ".sdb";
        
        // Create database
        ASSERT_TRUE(Engine::create_database(db_path, page_size));
        
        // Verify file exists and size is correct
        ASSERT_TRUE(filesystem::exists(db_path));
        ASSERT_EQ(filesystem::file_size(db_path) % page_size, 0);
        
        // Open and verify header
        Database db(db_path);
        ASSERT_EQ(db.page_size(), page_size);
        ASSERT_EQ(db.version(), "1.01");
        
        // Verify schema tree exists
        ASSERT_TRUE(db.schema_exists("[root]"));
        ASSERT_TRUE(db.schema_exists("[root].[sys]"));
        ASSERT_TRUE(db.schema_exists("[root].[sec]"));
        
        // Verify system tables exist
        ASSERT_TRUE(db.table_exists("[root].[sys].tables"));
        ASSERT_TRUE(db.table_exists("[root].[sys].columns"));
        
        // Verify UUIDs assigned
        auto root_uuid = db.get_schema_uuid("[root]");
        ASSERT_FALSE(root_uuid.is_nil());
    }
}
```

**Hooks for Future**:
- Page type enumeration includes all future types
- Header has slots for WAL location, replication info
- Schema tree nodes have slots for remote connections
- System tables include columns for future features (even if NULL)

---

### Alpha 1.02 - Basic Page I/O & Buffer Pool
**Goal**: Read/write pages with simple buffer management

**Deliverables**:
- Page read/write operations
- Simple buffer pool (no fancy caching yet)
- Page checksums
- Basic page types (Header, Data, Index, Free)

**Tests**:
- Write page, read back, verify checksum
- Buffer pool holds N pages
- Page eviction (simple LRU)

**Hooks for Future**:
- Page structure supports MVCC headers
- Buffer pool interface supports future Direct I/O
- Statistics counters for adaptive caching

---

### Alpha 1.03 - Heap Storage & Tuple Management
**Goal**: Store and retrieve tuples in heap pages

**Deliverables**:
- Tuple format with header (UUID, version info)
- Insert tuple into heap
- Scan heap for tuples
- Delete flag (not actual removal)

**Tests**:
- Insert 1000 tuples, scan back
- Tuple has correct UUID
- Deleted tuples marked but present

**Hooks for Future**:
- Tuple header has transaction ID slots
- Version chain pointers (even if unused)
- Space for row-level security flags

---

### Alpha 1.04 - Basic B-Tree Implementation
**Goal**: Create and search B-tree indexes

**Deliverables**:
- B-tree page structure
- Insert into B-tree
- Search B-tree
- Primary key constraint

**Tests**:
- Create index on table
- Insert preserves sort order
- Search finds correct tuple
- Duplicate detection for unique index

**Hooks for Future**:
- Node structure supports concurrent access flags
- Slots for bitmap/GIN/GiST metadata
- Multi-version index entries

---

### Alpha 1.05 - Simple SQL Parser
**Goal**: Parse basic CREATE TABLE and SELECT statements

**Deliverables**:
- Tokenizer
- Basic SQL grammar (CREATE TABLE, SELECT * FROM)
- AST generation
- Simple BLR generation

**Tests**:
- Parse "CREATE TABLE t (id INTEGER)"
- Parse "SELECT * FROM t"
- Generate correct BLR
- Syntax error detection

**Hooks for Future**:
- Parser state machine extensible
- AST nodes for all SQL features
- BLR opcodes defined for all operations

---

## ALPHA PHASE 2: Core Functionality (2.01 - 2.05)

### Alpha 2.01 - Transaction Foundation (MGA Preparation)
**Goal**: Basic transaction infrastructure

**Deliverables**:
- Transaction ID generation
- Transaction Inventory Page (TIP)
- BEGIN/COMMIT/ROLLBACK commands
- Transaction context

**Tests**:
- Start transaction, get ID
- Commit updates TIP
- Rollback restores state

**Hooks for Future**:
- TIP structure supports MGA
- Transaction has snapshot data structure
- Isolation level field (even if not used)

---

### Alpha 2.02 - Basic Query Execution
**Goal**: Execute simple INSERT/SELECT

**Deliverables**:
- BLR interpreter
- INSERT execution
- SELECT * execution
- Simple WHERE clause (column = value)

**Tests**:
- INSERT adds tuple
- SELECT returns tuple
- WHERE filters correctly

**Hooks for Future**:
- Executor supports parallel flag
- Statistics collection points
- Cost tracking infrastructure

---

### Alpha 2.03 - Schema Navigation
**Goal**: Implement hierarchical schema system

**Deliverables**:
- Current schema concept
- Path resolution ([root].[app].[table])
- Relative paths (../../table)
- Search path

**Tests**:
- Navigate schema tree
- Resolve relative paths
- Search path finds objects

**Hooks for Future**:
- Remote schema mount points
- Synonym infrastructure
- Virtual schema support

---

### Alpha 2.04 - System Catalog Queries
**Goal**: Query system tables

**Deliverables**:
- Query [root].[sys].tables
- Query [root].[sys].columns
- SHOW TABLES command
- SHOW SCHEMAS command

**Tests**:
- List all tables
- Get table columns
- Navigate schema tree via SQL

---

### Alpha 2.05 - Persistence & Recovery
**Goal**: Database survives restart

**Deliverables**:
- Flush dirty pages
- Reload buffer pool
- Consistent state after crash

**Tests**:
- Insert data, restart, data still there
- Crash during write, recovery succeeds

**Hooks for Future**:
- WAL infrastructure (even if not used)
- Checkpoint mechanism
- Recovery modes

---

## BETA PHASE 1: MGA & MVCC (1.01 - 1.05)

### Beta 1.01 - Multi-Generational Architecture
**Goal**: Implement Firebird-style MGA

**Deliverables**:
- Version chains
- Transaction snapshots
- Garbage collection basics
- Lock-free reads

**Tests**:
- Reader doesn't block writer
- Each transaction sees consistent snapshot
- Old versions cleaned up

---

### Beta 1.02 - Isolation Levels
**Goal**: Support standard isolation levels

**Deliverables**:
- Read Committed
- Repeatable Read
- Serializable (if possible with MGA)

**Tests**:
- Dirty read prevention
- Phantom read tests
- Serialization anomaly detection

---

### Beta 1.03 - Advanced SQL Parsing
**Goal**: Parse complex SQL

**Deliverables**:
- JOIN syntax
- Subqueries
- CTEs
- Window functions (parsing only)

**Tests**:
- Parse complex queries
- Generate correct AST
- BLR generation for complex queries

---

### Beta 1.04 - Query Optimization
**Goal**: Basic cost-based optimizer

**Deliverables**:
- Statistics collection
- Cost model
- Join order optimization
- Index selection

**Tests**:
- Chooses index when selective
- Optimal join order for 3-table join

---

### Beta 1.05 - Stored Procedures
**Goal**: CREATE PROCEDURE with BLR storage

**Deliverables**:
- Parse CREATE PROCEDURE
- Store BLR in catalog
- Execute stored procedure
- Parameters and return values

**Tests**:
- Create and call procedure
- Procedure survives restart
- Parameter passing works

---

## BETA PHASE 2: Advanced Features (2.01 - 2.05)

### Beta 2.01 - Triggers & Events
**Goal**: Trigger system with positions

**Deliverables**:
- CREATE TRIGGER
- Position-based execution
- POST_EVENT command
- Event subscription API

**Tests**:
- Triggers fire in position order
- Events delivered to subscribers

---

### Beta 2.02 - Security Foundation
**Goal**: Users, roles, and permissions

**Deliverables**:
- CREATE USER/ROLE
- GRANT/REVOKE
- Login authentication
- Permission checking

**Tests**:
- User can login
- Permissions enforced
- Role inheritance works

---

### Beta 2.03 - Network Protocol
**Goal**: Basic client/server

**Deliverables**:
- TCP listener
- Native protocol
- Client library
- Connection handling

**Tests**:
- Remote connection works
- Multiple concurrent clients
- Query/response cycle

---

### Beta 2.04 - WAL Implementation
**Goal**: Write-ahead logging for durability

**Deliverables**:
- WAL writer
- WAL replay
- Checkpoint mechanism
- Crash recovery

**Tests**:
- Recovery after crash
- No data loss
- Performance acceptable

---

### Beta 2.05 - Version Control Integration
**Goal**: Metadata version control

**Deliverables**:
- Change capture
- Git sync agent
- Rollback capability
- Schema snapshots

**Tests**:
- Changes logged
- Git repo updated
- Can rollback DDL

---

## RELEASE CANDIDATE PHASE (RC 1.0)

### RC 1.01 - Multi-Protocol Support
**Goal**: PostgreSQL and MySQL wire protocols

**Deliverables**:
- PostgreSQL wire protocol
- MySQL wire protocol
- Protocol detection
- Y-Valve routing

**Tests**:
- psql connects successfully
- mysql client connects
- Correct protocol auto-detected

---

### RC 1.02 - Federation
**Goal**: Foreign data wrappers

**Deliverables**:
- PostgreSQL FDW
- MySQL FDW
- Remote schema mounting
- Cross-database queries

**Tests**:
- Query remote PostgreSQL
- Join local and remote tables

---

### RC 1.03 - Performance Optimization
**Goal**: Production-ready performance

**Deliverables**:
- Parallel query execution
- Advanced buffer management
- Query result caching
- Connection pooling

**Tests**:
- TPC-C benchmark
- TPC-H benchmark
- Concurrent user testing

---

### RC 1.04 - High Availability
**Goal**: Replication and failover

**Deliverables**:
- Streaming replication
- Automatic failover
- Point-in-time recovery
- Backup/restore tools

**Tests**:
- Failover under load
- No data loss
- Recovery time < 30s

---

### RC 1.05 - Production Hardening
**Goal**: Ready for production

**Deliverables**:
- Comprehensive testing
- Documentation complete
- Monitoring integration
- Security audit passed

**Tests**:
- 30-day stability test
- Security penetration test
- Performance regression suite

---

## Success Criteria for Each Phase

### Alpha Success = "It Works"
- Basic functionality present
- Core architecture proven
- Development can continue

### Beta Success = "It's Complete"  
- All major features implemented
- Integration working
- Ready for testing

### RC Success = "It's Ready"
- Performance acceptable
- Stability proven
- Production deployable

---

## Development Guidelines

### Every Phase Must:
1. **Build on previous work** - No rewrites
2. **Include hooks for future** - Think ahead
3. **Have comprehensive tests** - Regression prevention
4. **Update documentation** - Stay current
5. **Maintain backward compatibility** - No breaking changes

### Code Structure:
```cpp
// Even in Alpha 1.01, include future structures
struct TupleHeader {
    UUID tuple_id;
    TransactionId xmin;      // Creating transaction (unused in Alpha)
    TransactionId xmax;      // Deleting transaction (unused in Alpha)
    CommandId cmin;          // Creating command (unused in Alpha)
    CommandId cmax;          // Deleting command (unused in Alpha)
    ItemPointer t_ctid;      // Current tuple ID
    uint16_t t_infomask;     // Various flags
    uint16_t t_infomask2;    // More flags
    // Ready for MGA from day one!
};
```

This phased approach ensures we build toward the complete vision while delivering working software at each step!