# ScratchBird Alpha 1.2 Requirements

**Date:** October 7, 2025
**Status:** Planning Phase
**Target:** Complete core functionality before SBLR stage
**Estimated Effort:** 20-30 weeks (aggressive timeline)

---

## Executive Summary

Alpha 1.2 completes all core database engine functionality required before beginning the SBLR (ScratchBird Low-level Runtime) stage. This release ensures a solid, fully-functional database engine that SBLR can be tested against.

**Key Principle:** No features deferred - complete the engine properly before moving to SBLR integration.

---

## Core Design Principles

### Transaction Management Architecture

**Fundamental Rule:** Connections are ALWAYS in a transaction.

#### Key Principles:

1. **Always-In-Transaction Model:**
   - A connection is ALWAYS in a transaction from the moment it's established
   - `COMMIT` automatically starts a new transaction
   - `ROLLBACK` automatically starts a new transaction
   - There is NO state where a connection exists outside of a transaction

2. **No Work Outside MGA:**
   - ALL database work occurs within the MGA (Multi-Generational Architecture) transaction system
   - This includes DDL (Data Definition Language) operations
   - This includes catalog operations
   - This includes all data modifications

3. **DDL Within Transactions:**
   - DDL commands (CREATE, ALTER, DROP) execute within transactions
   - Some DDL commands may require being in their own transaction (exclusive access)
   - But they are still within a transaction - never outside the transaction system

4. **START TRANSACTION Command:**
   - `START TRANSACTION` is used to change transaction settings, NOT to start the first transaction
   - Syntax: `START TRANSACTION [options] [COMMIT OUTSTANDING]`

   **Options:**
   - `READ ONLY` - Start a read-only transaction
   - `READ WRITE` - Start a read-write transaction (default)
   - `COMMIT OUTSTANDING` - Commit current transaction before starting new one

   **Behavior:**
   - If `COMMIT OUTSTANDING` is specified:
     - Attempts to commit the current transaction
     - If current transaction cannot commit due to errors, the command fails
     - New transaction is NOT started if commit fails
     - Only starts new transaction after successful commit

   **Examples:**
   ```sql
   -- Change to read-only mode, committing current transaction first
   START TRANSACTION READ ONLY COMMIT OUTSTANDING;

   -- Switch back to read-write after read-only queries
   START TRANSACTION READ WRITE COMMIT OUTSTANDING;

   -- Start new transaction with same settings (commits current)
   START TRANSACTION COMMIT OUTSTANDING;
   ```

5. **Implementation Requirements:**
   - ConnectionContext MUST always have a valid transaction ID (XID)
   - Transaction commit MUST immediately start new transaction
   - Transaction rollback MUST immediately start new transaction
   - Connection establishment MUST immediately start initial transaction
   - Connection close MUST rollback any outstanding transaction

6. **Implications for ConnectionContext (CRIT-002):**
   - ConnectionContext must include:
     - Current transaction ID (never NULL)
     - Transaction state (active, read-only, read-write)
     - Transaction start time
     - Transaction settings
   - Must provide atomic transition between transactions
   - Must ensure no gap exists between commit/rollback and next transaction

### Firebird Transaction Model Adoption

**Reference Specification:** `/docs/specifications/FIREBIRD_TRANSACTION_MODEL_SPEC.md`

ScratchBird adopts Firebird's proven transaction implementation model:

1. **Transaction Markers:**
   - OIT (Oldest Interesting Transaction) - for garbage collection
   - OAT (Oldest Active Transaction) - oldest running transaction
   - OST (Oldest Snapshot Transaction) - oldest SNAPSHOT transaction
   - Next Transaction ID - transaction counter

2. **Isolation Levels:**
   - SNAPSHOT (concurrency) - point-in-time consistency
   - SNAPSHOT TABLE STABILITY - table-level locking
   - READ COMMITTED (with READ CONSISTENCY) - sees latest committed changes

3. **Long-Running Transaction Management:**
   - Configurable detection thresholds
   - Automatic monitoring and warnings
   - Policies: LOG / ROLLBACK_READONLY / ROLLBACK_ALL / TERMINATE
   - Special handling for READ ONLY READ COMMITTED transactions

4. **Sweep Mechanism:**
   - Automatic sweep when (OST - OIT) > sweep_interval
   - Default interval: 20,000 transactions
   - Configurable: cooperative, background, or combined mode
   - Manual sweep trigger support

5. **Garbage Collection:**
   - Cooperative GC: queries clean garbage they encounter
   - Background GC: dedicated thread (optional)
   - Combined mode (recommended)

**Implementation Timeline:** 10 weeks (6-7 weeks with 2 developers)
**Priority:** HIGH - Required for production-quality transaction management
**Dependencies:** ConnectionContext (CRIT-002) must be implemented first

See detailed specification for complete implementation roadmap, configuration options, and monitoring queries.

---

## Requirements Overview

### 1. Complete All Data Types ✅ CRITICAL
**Objective:** Implement ALL types specified in `/docs/specifications/03_TYPES_AND_DOMAINS.md`

#### 1.1 Missing Primitive Types

**Currently Implemented:**
- ✅ INT8, INT16, INT32, INT64
- ✅ FLOAT32, FLOAT64
- ✅ CHAR, VARCHAR, TEXT
- ✅ BINARY
- ✅ DATE, TIME, TIMESTAMP
- ✅ BOOLEAN
- ✅ UUID
- ✅ JSON

**MUST IMPLEMENT:**
- ❌ INT128 - 128-bit signed integer
- ❌ UINT8, UINT16, UINT32, UINT64 - Unsigned integers
- ❌ MONEY - Fixed-precision currency type
- ❌ INTERVAL - Time interval type
- ❌ JSONB - Binary JSON format (optimized)
- ❌ XML - XML document type
- ❌ VECTOR - Vector embeddings for similarity search
- ❌ ARRAY - Multi-dimensional arrays
- ❌ COMPOSITE/RECORD - Structured types
- ⚠️ DECIMAL - Currently string storage, needs proper arithmetic

**Effort:** 4-6 weeks for all primitive types

---

### 2. Implement Complete DOMAIN System 🎯 HIGH PRIORITY

**Objective:** Full DOMAIN system as specified

#### 2.1 Basic DOMAIN Support

```sql
CREATE DOMAIN product_sku AS VARCHAR(20)
    DEFAULT 'N/A'
    NOT NULL
    CHECK (VALUE ~ '^[A-Z]{3}-[0-9]{5}$');
```

**Requirements:**
- Parser support for CREATE DOMAIN
- Catalog tables for domain storage
- Domain constraint validation
- Domain inheritance (INHERITS clause)

**Effort:** 2 weeks

---

#### 2.2 RECORD Domains (Composite Types)

```sql
CREATE DOMAIN mailing_address AS RECORD (
    street_line1 VARCHAR(100) NOT NULL,
    street_line2 VARCHAR(100),
    city VARCHAR(50) NOT NULL,
    state CHAR(2) NOT NULL,
    postal_code VARCHAR(10) NOT NULL
);
```

**Requirements:**
- RECORD type storage and operations
- ROW constructor syntax
- Dot notation field access
- EXTRACT function for fields

**Effort:** 2 weeks

---

#### 2.3 ENUM Domains

```sql
CREATE DOMAIN order_status AS ENUM (
    'DRAFT', 'SUBMITTED', 'PROCESSING', 'SHIPPED', 'DELIVERED', 'CANCELLED'
) WITH OPTIONS (WRAP = FALSE);
```

**Requirements:**
- ENUM type with ordered values
- SET NEXT VALUE operation
- GET VALUE FOR, GET POSITION FOR operations
- Enum comparison and ordering

**Effort:** 1 week

---

#### 2.4 SET Domains

```sql
CREATE DOMAIN tag_set AS SET OF VARCHAR(50);
```

**Requirements:**
- SET type storage (unordered, unique)
- SET constructor syntax: `SET['sql', 'database', 'design']`
- Set operators: `@>` (contains), `&&` (overlaps)
- Set operations: union, intersection, difference

**Effort:** 1 week

---

#### 2.5 VARIANT Type

```sql
DECLARE @old_value VARIANT;
```

**Requirements:**
- Runtime polymorphic type
- EXTRACT(DATATYPE FROM value)
- IS OF TYPE checks
- Type-safe casting

**Effort:** 2 weeks

---

#### 2.6 Advanced Domain Features

**WITH SECURITY:**
```sql
CREATE DOMAIN ssn AS VARCHAR(11)
    CHECK (VALUE ~ '^\d{3}-\d{2}-\d{4}$')
    WITH SECURITY (
        MASK_FUNCTION = 'mask_ssn',
        AUDIT_ACCESS = TRUE,
        REQUIRE_PERMISSION = 'view_pii',
        ENCRYPTION = 'AES256'
    );
```

**WITH INTEGRITY:**
```sql
CREATE DOMAIN email_address AS VARCHAR(255)
    WITH INTEGRITY (
        UNIQUE_ACROSS_DATABASE = TRUE,
        CASE_INSENSITIVE = TRUE,
        NORMALIZE_FUNCTION = 'normalize_email'
    );
```

**WITH VALIDATION:**
```sql
CREATE DOMAIN mailing_address AS RECORD (...)
    WITH VALIDATION (
        VALIDATE_FUNCTION = 'validate_address_with_usps_api',
        ON_VIOLATION = 'REJECT',
        ERROR_MESSAGE = 'The address could not be verified.'
    );
```

**WITH QUALITY:**
```sql
CREATE DOMAIN phone_number AS VARCHAR(20)
    WITH QUALITY (
        PARSE_FUNCTION = 'parse_phone_input',
        STANDARDIZE_FUNCTION = 'format_to_e164',
        ENRICH_FUNCTION = 'lookup_carrier_info'
    );
```

**Effort:** 4 weeks for all advanced features

---

**Total DOMAIN System Effort:** 12 weeks

---

### 3. Complete All Index Types 📊 HIGH PRIORITY

**Objective:** Implement ALL index types per `/docs/specifications/INDEX_IMPLEMENTATION_SPEC.md`

#### 3.1 Completed Index Types

- ✅ **B-Tree Index** (~2,256 lines, tests passing)
  - Multi-page support, range scans, compression, vacuum
- ✅ **Hash Index** (~2,254 lines, tests passing)
  - Directory and bucket pages, all operations working

#### 3.2 Missing Index Types

**GIN (Generalized Inverted Index) - CRITICAL**
- For array indexing, full-text search, JSON
- Entry tree + posting trees
- Pending list for fast inserts
- **Effort:** 6-8 weeks

**Bitmap Index**
- For low-cardinality columns
- Bitmap compression
- Set operations on bitmaps
- **Effort:** 3-4 weeks

**GIST (Generalized Search Tree)**
- For spatial data, custom types
- Extensible framework
- **Effort:** 6-8 weeks

**BRIN (Block Range Index)**
- For very large tables
- Minimal storage overhead
- **Effort:** 2-3 weeks

**VECTOR Index**
- For similarity search (HNSW, IVF)
- Required for VECTOR type
- **Effort:** 4-6 weeks

**Total Index Effort:** 21-29 weeks

**Note:** Can be parallelized - multiple developers working on different index types simultaneously.

---

### 4. 128-Character UTF-8 Identifiers ⚠️ MEDIUM PRIORITY

**Current State:**
- ✅ 128-byte buffers allocated
- ⚠️ Byte-based, not character-based

**Requirements:**
1. UTF-8 character counting utilities
2. Parser validation (128 characters, not bytes)
3. Proper handling of multibyte characters
4. Testing with various scripts (CJK, emoji, etc.)

**Effort:** 1 week

---

### 5. Comment Support for Database Objects 💬 MEDIUM PRIORITY

**Requirements:**
1. Catalog schema changes (add comment fields)
2. Parser support for COMMENT ON syntax
3. Catalog operations (set/get/remove comments)
4. Support for all object types (schemas, tables, columns, indexes, etc.)

**SQL Syntax:**
```sql
COMMENT ON TABLE schema.table IS 'Description';
COMMENT ON COLUMN schema.table.column IS 'Description';
COMMENT ON INDEX schema.index IS 'Description';
```

**Effort:** 1 week

---

### 6. Configuration File System (sb_config.ini) 📋 HIGH PRIORITY

**Requirements:**
1. INI file parser
2. Config singleton with type-safe accessors
3. Command-line argument support
4. Environment variable support
5. Replace all hardcoded magic numbers
6. Configuration validation

**Format:**
```ini
[database]
default_page_size = 16384
max_connections = 100
default_encoding = utf8

[memory]
buffer_pool_size = 128
transaction_cache_size = 10000

[storage]
toast_chunk_size = 8192
compression_enabled = true

[btree]
min_fanout = 32
max_fanout = 256

[logging]
log_level = INFO
log_file = scratchbird.log
```

**Effort:** 1-2 weeks

---

## Implementation Priorities

### Phase 1: Foundation (3-4 weeks)
1. ✅ Fix duplicate include (5 minutes)
2. Configuration file system (1-2 weeks)
3. UTF-8 character counting (1 week)
4. Comment support (1 week)

### Phase 2: Type System Completion (10-12 weeks)
1. Missing primitive types (4-6 weeks)
   - INT128, UINT*, MONEY, INTERVAL
   - JSONB, XML, VECTOR
   - ARRAY, COMPOSITE
   - DECIMAL arithmetic
2. DOMAIN system (12 weeks)
   - Basic domains (2 weeks)
   - RECORD domains (2 weeks)
   - ENUM domains (1 week)
   - SET domains (1 week)
   - VARIANT type (2 weeks)
   - Advanced features (4 weeks)

### Phase 3: Index Types (21-29 weeks, can parallelize)
1. GIN index (6-8 weeks) - CRITICAL for arrays/full-text
2. Bitmap index (3-4 weeks)
3. GIST index (6-8 weeks)
4. BRIN index (2-3 weeks)
5. VECTOR index (4-6 weeks)

### Phase 4: Polish & Testing (2-3 weeks)
1. Comprehensive testing
2. Documentation updates
3. Bug fixes
4. Performance optimization

---

## Timeline

**Sequential (1 developer):** 36-48 weeks (9-12 months)

**Parallel (3 developers):**
- Developer 1: Type system (10-12 weeks)
- Developer 2: DOMAIN system (12 weeks)
- Developer 3: Index types (21-29 weeks, but parallel work)

**Realistic with 2-3 developers:** 20-30 weeks (5-7.5 months)

---

## Success Criteria

Alpha 1.2 is complete when:

1. **Type System:**
   - [ ] All primitive types implemented and tested
   - [ ] DOMAIN system fully functional
   - [ ] RECORD, ENUM, SET, VARIANT types working
   - [ ] Advanced domain features implemented
   - [ ] All types usable in tables, indexes, queries

2. **Index Types:**
   - [ ] GIN index fully functional
   - [ ] Bitmap index fully functional
   - [ ] GIST index fully functional
   - [ ] BRIN index fully functional
   - [ ] VECTOR index fully functional
   - [ ] All index tests passing

3. **Infrastructure:**
   - [ ] 128-character UTF-8 identifiers working
   - [ ] Comment support on all objects
   - [ ] Configuration file system operational
   - [ ] All magic numbers in config file

4. **Quality:**
   - [ ] No regression in existing tests
   - [ ] All new features tested
   - [ ] Documentation complete
   - [ ] Performance acceptable

5. **Ready for SBLR:**
   - [ ] Solid, stable database engine
   - [ ] All core features working
   - [ ] Can handle complex workloads
   - [ ] Suitable for SBLR testing and development

---

## Deferred Features

**The following are deferred to AFTER Alpha 1.2:**
- SBLR stage integration (awaiting solid engine)
- Network layer
- Wire protocol implementations
- Authentication systems
- Replication
- Distributed transactions
- WAL (may add before SBLR)

---

## Risk Assessment

### High Risk
- DOMAIN system complexity (12 weeks, many interdependencies)
- GIN index complexity (6-8 weeks, critical for advanced features)
- GIST index complexity (6-8 weeks, extensible framework)

### Medium Risk
- VECTOR index (new territory for team)
- ARRAY type implementation (multidimensional complexity)
- Advanced domain features (security, validation, quality hooks)

### Low Risk
- Configuration file system
- Comment support
- UTF-8 identifiers
- Missing primitive types

---

## Dependencies

### External Libraries Needed

1. **DECIMAL Arithmetic:**
   - boost::multiprecision OR
   - GNU MPFR OR
   - Custom fixed-point

2. **UTF-8 Support:**
   - ICU library (recommended) OR
   - Custom utilities

3. **XML Support:**
   - libxml2 OR
   - pugixml

4. **Vector Operations:**
   - Eigen library OR
   - Custom implementation

---

## Conclusion

Alpha 1.2 is an ambitious release that completes the entire database engine specification before moving to SBLR integration. This ensures SBLR will be tested against a fully-functional, production-quality database engine.

**Key Decisions:**
- ✅ No deferrals - implement everything specified
- ✅ B-tree and Hash indexes already complete
- ✅ Focus on DOMAIN system and remaining index types
- ✅ SBLR stage waits for solid engine

**Estimated Delivery:** 5-7.5 months with 2-3 developers working in parallel

---

**Next Steps:**
1. Review and approve requirements
2. Prioritize work streams
3. Assign developers to parallel tracks
4. Begin implementation
5. Weekly progress reviews

---

*Requirements Document Version: 2.0*
*Date: October 7, 2025*
*Corrected based on actual implementation status*
