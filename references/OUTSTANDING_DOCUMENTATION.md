# Outstanding Documentation for ScratchBird

## Status Overview

### ✅ Completed Documentation
1. **Wire Protocols** - Low-level byte formats for PostgreSQL, MySQL, Firebird, TDS
2. **Data Types** - Complete type specifications for all databases + universal mapping
3. **Page Layouts** - Detailed page structures for all storage types
4. **Replication Protocols** - Shadow database and dual-channel replication specs
5. **Architecture Documents** - MGA, UUID schema, layered architecture, caching
6. **Core Specifications** - Events, triggers, UDR, context parser, bulk insert

### 📝 Outstanding Documentation Needed

## 1. SQL Grammar and Parser Specifications

### 1.1 BNF/EBNF Grammar for ScratchBird SQL
**Priority: CRITICAL**
- Complete SQL grammar in BNF/EBNF format
- Precedence rules for operators
- Reserved word list (minimal due to context-aware parsing)
- Statement termination rules
- Comment syntax (inline and block)

### 1.2 SQL Dialect Comparison Matrix
**Priority: HIGH**
```sql
-- Need documentation for:
-- PostgreSQL: $$ quoting, :: casting, RETURNING
-- MySQL: backticks, LIMIT syntax, SHOW commands
-- Firebird: EXECUTE BLOCK, LIST(), POSITION
-- MSSQL: TOP, square brackets, GO batch separator
```

### 1.3 Parser State Machine
**Priority: HIGH**
- Token types and lexer rules
- Parser states and transitions
- Error recovery strategies
- Auto-completion hook points

## 2. Binary Language Representation (BLR)

### 2.1 BLR Instruction Set
**Priority: CRITICAL**
```
blr_version
blr_begin
blr_message
blr_assignment
blr_if
blr_loop
blr_select
blr_insert
blr_update
blr_delete
... complete opcode list needed
```

### 2.2 BLR Encoding Specification
**Priority: CRITICAL**
- Opcode format
- Operand encoding
- Type descriptors
- UUID references for objects
- Optimization hints

### 2.3 BLR to Native Code Paths
**Priority: MEDIUM**
- Interpretation strategy
- JIT compilation hooks
- Vectorization opportunities
- Plan caching integration

## 3. Transaction Management Details

### 3.1 MGA Implementation Specification
**Priority: CRITICAL**
- Transaction ID generation
- Version chain management
- Garbage collection algorithm
- Transaction Inventory Page (TIP) format
- Oldest Active Transaction (OAT) tracking
- Oldest Snapshot Transaction (OST) tracking

### 3.2 Isolation Level Semantics
**Priority: HIGH**
```
READ UNCOMMITTED - Not supported (use READ COMMITTED)
READ COMMITTED   - See latest committed version
REPEATABLE READ  - Snapshot at statement start
SERIALIZABLE     - Snapshot at transaction start
```

### 3.3 Lock Manager Specification
**Priority: HIGH**
- Lock types (shared, exclusive, intent)
- Lock granularity (row, page, table)
- Deadlock detection algorithm
- Lock escalation rules
- Timeout handling

## 4. Index Implementation Details

### 4.1 B-Tree Index Algorithms
**Priority: HIGH**
**Status: ✅ COMPLETE**
- See `technical_specifications/INDEX_IMPLEMENTATION_SPEC.md`
- Page split algorithm (§1.4)
- Merge algorithm (§5.1)
- Prefix compression (§1.5)
- Suffix truncation (§1.5)
- Unique vs non-unique handling (§1.3)

### 4.2 Specialized Index Types
**Priority: MEDIUM**
**Status: ✅ COMPLETE**
- See `technical_specifications/INDEX_IMPLEMENTATION_SPEC.md` §3
```
HASH     - Hash function, bucket management (§3.1)
BITMAP   - Compression, AND/OR operations (§3.2)
GIN      - Posting list format, fastupdate (§3.3)
R-TREE   - MBR calculations, split algorithms (Future)
LSM      - Level management, compaction (Future)
COLUMN   - Dictionary encoding, RLE compression (Future)
```

## 5. Query Optimizer Specifications

### 5.1 Statistics Collection
**Priority: HIGH**
**Status: ✅ COMPLETE**
- See `technical_specifications/QUERY_OPTIMIZER_SPEC.md` §1
- Histogram format (§1.2)
- Sample size determination (§1.3)
- Update frequency (§1.3)
- Multi-column statistics (§1.1)
- Expression statistics (§1.1)

### 5.2 Cost Model
**Priority: HIGH**
**Status: ✅ COMPLETE**
- See `technical_specifications/QUERY_OPTIMIZER_SPEC.md` §2
```
seq_page_cost     = 1.0
random_page_cost  = 4.0
cpu_tuple_cost    = 0.01
cpu_index_cost    = 0.005
cpu_operator_cost = 0.0025
```

### 5.3 Join Algorithms
**Priority: HIGH**
**Status: ✅ COMPLETE**
- See `technical_specifications/QUERY_OPTIMIZER_SPEC.md` §3.3
- Nested loop implementation
- Hash join implementation
- Sort-merge join implementation
- Join order optimization
- Adaptive join selection

## 6. Network Layer Details

### 6.1 Connection Pooling Specification
**Priority: HIGH**
**Status: ✅ COMPLETE**
- See `technical_specifications/NETWORK_LAYER_SPEC.md` §1
- Pool sizing algorithms (§1.1)
- Connection validation (§1.3)
- Idle timeout handling (§1.2)
- Transaction affinity (§1.2)
- Load balancing strategies (§2.1)

### 6.2 Y-Valve Router Implementation
**Priority: CRITICAL**
**Status: ✅ COMPLETE**
- See `technical_specifications/NETWORK_LAYER_SPEC.md` §2
- Protocol detection algorithm (§3.1)
- Parser selection logic (§2.1)
- Connection context management (§1.1)
- Protocol translation hooks (§2.2)
- Error mapping tables (§3.1)

### 6.3 SSL/TLS Configuration
**Priority: HIGH**
- Certificate management
- Cipher suite selection
- Protocol version negotiation
- Client certificate validation
- SNI support

## 7. Storage Management

### 7.1 Buffer Pool Management
**Priority: HIGH**
- Page replacement algorithm (Clock-Sweep)
- Dirty page tracking
- Checkpoint algorithm
- Read-ahead strategies
- Ring buffer for sequential scans

### 7.2 Free Space Management
**Priority: HIGH**
- Free space map structure
- Visibility map structure
- Page allocation algorithm
- Space reclamation
- Autovacuum triggers

### 7.3 TOAST/LOB Management
**Priority: MEDIUM**
- Compression threshold
- Chunk size selection
- Out-of-line storage
- Compression algorithms
- Deduplication

## 8. Backup and Recovery

### 8.1 Physical Backup Specification
**Priority: HIGH**
- Page-level backup format
- Incremental backup strategy
- Parallel backup
- Compression options
- Validation procedures

### 8.2 Logical Backup Format
**Priority: MEDIUM**
- SQL dump format
- Binary dump format
- Partial backup selection
- Dependency ordering
- Large object handling

### 8.3 Point-in-Time Recovery
**Priority: HIGH**
- WAL archive management
- Recovery target specification
- Timeline management
- Partial recovery
- Standby promotion

## 9. Security Implementation

### 9.1 Authentication Methods
**Priority: HIGH**
```
password     - PBKDF2 specification
md5          - For compatibility only
scram-sha256 - SCRAM implementation
certificate  - X.509 validation
kerberos     - GSSAPI integration
ldap         - LDAP bind specification
```

### 9.2 Row-Level Security
**Priority: MEDIUM**
- Policy definition syntax
- Policy evaluation order
- Performance implications
- Audit integration
- Bypass permissions

### 9.3 Encryption Specifications
**Priority: HIGH**
- Transparent Data Encryption (TDE)
- Column-level encryption
- Key management
- Encryption algorithms
- IV generation

## 10. Monitoring and Diagnostics

### 10.1 Statistics Views
**Priority: MEDIUM**
```sql
sb_stat_activity     - Current connections
sb_stat_statements   - Query statistics
sb_stat_tables       - Table access statistics
sb_stat_indexes      - Index usage statistics
sb_stat_io           - I/O statistics
```

### 10.2 Wait Event Categories
**Priority: MEDIUM**
- Lock waits
- I/O waits
- Network waits
- CPU waits
- IPC waits

### 10.3 Trace Event Specification
**Priority: LOW**
- Event categories
- Event data format
- Filtering rules
- Output formats
- Performance overhead

## 11. Extension APIs

### 11.1 UDR (User Defined Routines) API
**Priority: MEDIUM**
- Function registration
- Type conversion
- Memory management
- Error handling
- Security context

### 11.2 Foreign Data Wrapper API
**Priority: LOW**
- FDW callbacks
- Planning hooks
- Execution hooks
- Transaction management
- Error mapping

### 11.3 Custom Index API
**Priority: LOW**
- Index method registration
- Scan callbacks
- Build callbacks
- Maintenance callbacks
- Cost estimation

## 12. Testing Specifications

### 12.1 Regression Test Format
**Priority: HIGH**
- Test case structure
- Expected output format
- Platform variations
- Parallel execution
- Coverage requirements

### 12.2 Performance Benchmarks
**Priority: MEDIUM**
- TPC-C implementation
- TPC-H queries
- Custom microbenchmarks
- Scalability tests
- Concurrency tests

### 12.3 Chaos Testing Scenarios
**Priority: LOW**
- Network partition simulation
- Disk failure simulation
- Memory pressure testing
- Clock skew testing
- Byzantine failures

## 13. Build and Deployment

### 13.1 CMake Build Specification
**Priority: HIGH** ✓ Partially Complete
- Target definitions
- Dependency management
- Feature flags
- Platform detection
- Installation rules

### 13.2 Package Formats
**Priority: LOW**
- DEB package structure
- RPM package structure
- Docker image layers
- Snap package
- Windows installer

### 13.3 Configuration Files
**Priority: MEDIUM**
```ini
# scratchbird.conf format
[server]
port = 5432
max_connections = 100

[storage]
data_directory = /var/lib/scratchbird
page_size = 16384

[security]
authentication = scram-sha256
ssl = on
```

## 14. Migration Tools

### 14.1 Schema Migration Format
**Priority: MEDIUM**
- Migration file structure
- Dependency tracking
- Rollback procedures
- Conflict resolution
- Multi-database support

### 14.2 Data Migration Strategies
**Priority: LOW**
- ETL pipeline specification
- Type conversion rules
- Large table handling
- Consistency validation
- Performance optimization

## 15. Client Library Specifications

### 15.1 Native C API
**Priority: HIGH**
```c
// Core API functions needed:
sb_connect()
sb_execute()
sb_prepare()
sb_fetch()
sb_close()
```

### 15.2 Language Bindings
**Priority: LOW**
- Python DB-API 2.0
- Java JDBC driver
- .NET ADO.NET provider
- Node.js driver
- Go database/sql driver

## Priority Summary

### Must Have (Before Alpha)
1. BLR Specification
2. SQL Grammar
3. MGA Transaction Details
4. Y-Valve Router Spec
5. Basic C API

### Should Have (Before Beta)
1. Query Optimizer Specs
2. Index Algorithms
3. Buffer Pool Management
4. Authentication Methods
5. Connection Pooling

### Nice to Have (Before Release)
1. Extension APIs
2. Migration Tools
3. Performance Benchmarks
4. Package Formats
5. Language Bindings

## Next Steps

1. **Immediate**: Generate BLR specification based on Firebird's design
2. **This Week**: Create SQL grammar in BNF format
3. **Next Week**: Document MGA implementation details
4. **Ongoing**: Fill in remaining specs as implementation progresses

## Notes

- Some specifications can be derived from implementation
- Reference implementations exist for many algorithms
- Prioritize specs that block implementation
- Keep specs version-controlled and updated