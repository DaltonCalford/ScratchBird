# CRITICAL FINDING: Schema/Database DDL Opcode Gap

**Date:** 2025-12-26
**Severity:** CRITICAL - BLOCKS EMULATION
**Status:** RESOLVED (Schema/Database DDL wired through SBLR and executors)

---

## Executive Summary

This finding is resolved. Schema/database DDL now emits SBLR opcodes, and the executor
creates/drops/updates schema and emulated database records with cascade/force semantics.

### Status Update (2026-01-10)
- SBLR opcodes exist for CREATE/DROP/ALTER SCHEMA and CREATE/DROP/ALTER DATABASE.
- Executor handlers are implemented for schema/database DDL, with dependency blocking and force semantics.
- PostgreSQL/MySQL/Firebird parsers emit correct SBLR payloads for schema/database DDL.
- Emulation view generation is wired for CREATE/DROP DATABASE.
- ALTER DATABASE options in MySQL now map to emulated database metadata.
- ALTER SCHEMA SET PATH now moves a schema to the requested path.

---

## Emulation Architecture (As Intended)

Per user's description:

### PostgreSQL Example:
```sql
-- User executes:
CREATE DATABASE mydb;

-- Should be emulated as:
1. Create schema entry under emulation.postgres.mydb
2. Generate PostgreSQL-compatible catalog views for this "database"
3. Map subsequent operations to this schema tree
```

```sql
-- User executes:
DROP DATABASE mydb;

-- Should be emulated as:
1. Prune entire schema tree under emulation.postgres.mydb
2. Remove all child objects (CASCADE semantics)
3. Clean up catalog views
```

### Firebird Example:
```sql
-- User executes (via isql):
CREATE DATABASE '/path/to/mydb.fdb';

-- Should be emulated as:
1. Create schema entry under emulation.firebird.[derived_name]
2. Generate Firebird-compatible system tables (RDB$...)
3. Map connection to this schema tree
```

### MySQL Example:
```sql
-- User executes:
CREATE SCHEMA myschema;  -- MySQL treats SCHEMA and DATABASE as synonyms

-- Should be emulated as:
1. Create schema entry under emulation.mysql.myschema
2. Generate MySQL-compatible information_schema views
3. Map subsequent operations to this schema tree
```

---

## Historical Implementation Status (Superseded)

NOTE: The sections below capture the original audit state and are no longer accurate.
They are retained for historical context.

### SBLR Opcodes - IMPLEMENTED (as of 2026-01-10)

**Implemented:**
- `EXT_CREATE_SCHEMA`
- `EXT_DROP_SCHEMA`
- `EXT_CREATE_DATABASE`
- `EXT_DROP_DATABASE`
- `EXT_ALTER_SCHEMA`
- `EXT_ALTER_DATABASE`

**What exists:**
- `EXT_SHOW_SCHEMA = 0x66` - Show schema info (query only)
- `EXT_SHOW_DATABASE = 0x71` - Show database info (query only)
- `EXT_SHOW_SCHEMA_PATH = 0x75` - Show schema path (query only)
- `EXT_SHOW_SCHEMA_TREE = 0x76` - Show schema tree (query only)

**Verdict:** ❌ NO DDL OPCODES FOR SCHEMA/DATABASE MANAGEMENT

---

## Parser Implementation Status

### PostgreSQL Parser (`src/parser/postgresql/pg_parser_ddl.cpp`)

#### CREATE DATABASE (Lines 943-974)
```cpp
void Parser::parseCreateDatabase() {
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SHOW_DATABASE));  // ❌ WRONG!

    std::string db_name = parseIdentifier();
    emitString(db_name);

    // ... parse options but don't emit them ...
}
```

**Issues:**
1. ❌ Uses `EXT_SHOW_DATABASE` (a query opcode) as placeholder
2. ❌ Comment says "Use a placeholder" - indicates awareness this is wrong
3. ❌ Parses WITH options but doesn't emit them
4. ❌ No implementation in executor to handle this

**Verdict:** STUB/PLACEHOLDER - NOT FUNCTIONAL

#### CREATE SCHEMA (Lines 976-995)
```cpp
void Parser::parseCreateSchema() {
    std::string schema_name;

    bool if_not_exists = false;
    if (matchKeyword(TokenType::KW_IF)) {
        consumeKeyword(TokenType::KW_NOT, "Expected NOT");
        consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
        if_not_exists = true;
    }

    if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER)) {
        schema_name = parseIdentifier();
    }

    if (matchKeyword(TokenType::KW_AUTHORIZATION)) {
        parseIdentifier();  // role name
    }

    emitString(schema_name);  // ❌ NO OPCODE EMITTED!
}
```

**Issues:**
1. ❌ Does NOT emit any opcode at all
2. ❌ Only emits the schema name string
3. ❌ Parses IF NOT EXISTS but doesn't emit the flag
4. ❌ Parses AUTHORIZATION but doesn't emit it
5. ❌ Bytecode stream will be corrupt (string with no opcode)

**Verdict:** BROKEN - WILL PRODUCE INVALID BYTECODE

#### DROP DATABASE/DROP SCHEMA
- ❌ NOT IMPLEMENTED AT ALL
- No parseDropDatabase() function exists
- No parseDropSchema() function exists

**Verdict:** MISSING

---

### MySQL Parser (`src/parser/mysql/mysql_parser.cpp`)

#### CREATE DATABASE (Lines 2314-2316)
```cpp
void Parser::parseCreateDatabase() {
    // TODO: Implement
}
```

**Verdict:** ❌ STUB - NOT IMPLEMENTED

#### DROP DATABASE
- ❌ NOT FOUND

**Verdict:** MISSING

---

### Firebird Parser (`src/parser/firebird/firebird_parser.cpp`)

#### CREATE DATABASE
- ❌ NOT FOUND

#### DROP DATABASE
- ❌ NOT FOUND

**Note:** Firebird databases are usually created via gbak restore or connection string, but the emulation layer should still support this.

**Verdict:** MISSING

---

## Catalog Manager Status

### CREATE/DROP Schema Support

**Found in `src/core/catalog_manager.cpp`:**
```cpp
// Lines 1318-1373: createSchemaInternal() used during initialization
auto CatalogManager::createSchemaInternal(const std::string &schema_name, ...)

// Line 1685: Public API
auto CatalogManager::createSchema(const std::string &schema_name, ...)
```

**Exists:**
- ✅ `createSchema()` - API exists
- ✅ `createSchemaInternal()` - Implementation exists
- ✅ `dropSchema()` - Likely exists (need to verify)

**Issue:**
- ❌ NO SBLR OPCODE to call these from executor
- ❌ NO EXECUTOR HANDLER to process schema DDL

**Verdict:** Backend exists, but no connection from parsers

---

## Executor Status

**Searched for:**
- `executeCreateSchema` - NOT FOUND
- `executeDropSchema` - NOT FOUND
- `executeCreateDatabase` - NOT FOUND
- `executeDropDatabase` - NOT FOUND

**Verdict:** ❌ NO EXECUTOR HANDLERS FOR SCHEMA/DATABASE DDL

---

## Impact Analysis

### Blocking Issues

1. **PostgreSQL Emulation BLOCKED**
   - Cannot create emulated databases
   - Cannot drop emulated databases
   - Connection cannot map to schema tree
   - All PostgreSQL clients will fail on CREATE DATABASE

2. **MySQL Emulation BLOCKED**
   - Cannot create schemas (MySQL treats DATABASE = SCHEMA)
   - Cannot drop schemas
   - Multi-database applications cannot be emulated

3. **Firebird Emulation BLOCKED**
   - Cannot create emulated databases
   - Cannot initialize Firebird connections
   - RDB$ catalog views cannot be scoped to database

4. **Plan 04 (Domain DDL) IMPACTED**
   - Domains are schema-scoped
   - If schema management broken, domain management also broken
   - Cannot test domain DDL without working schemas

---

## Required Components

To fix this gap, we need:

### 1. SBLR Opcodes (New)

```cpp
// Schema/Database DDL - should be in 0x0100-0x01FF range (extended)
constexpr uint16_t EXT_CREATE_SCHEMA      = 0x0108;  // CREATE SCHEMA
constexpr uint16_t EXT_DROP_SCHEMA        = 0x0109;  // DROP SCHEMA
constexpr uint16_t EXT_ALTER_SCHEMA       = 0x010A;  // ALTER SCHEMA
constexpr uint16_t EXT_CREATE_DATABASE    = 0x010B;  // CREATE DATABASE (emulated)
constexpr uint16_t EXT_DROP_DATABASE      = 0x010C;  // DROP DATABASE (emulated)
constexpr uint16_t EXT_ALTER_DATABASE     = 0x010D;  // ALTER DATABASE (emulated)
```

**Payload Structures:**

```
EXT_CREATE_SCHEMA:
[flags:uint8] (IF_NOT_EXISTS = 0x01)
[schema_name:string]
[parent_schema_id:16] (optional, 0 = root)
[owner:string]

EXT_DROP_SCHEMA:
[flags:uint8] (IF_EXISTS = 0x01, CASCADE = 0x02, RESTRICT = 0x04)
[schema_name:string] OR [schema_id:16]

EXT_ALTER_SCHEMA:
[schema_id:16]
[action:uint8] (RENAME=1, SET_OWNER=2)
[payload varies by action]

EXT_CREATE_DATABASE:
[flags:uint8] (IF_NOT_EXISTS = 0x01, emulation_type:3bits)
[database_name:string]
[emulation_parent_schema_id:16] (e.g., emulation.postgres)
[options:variable] (encoding, template, etc.)

EXT_DROP_DATABASE:
[flags:uint8] (IF_EXISTS = 0x01, FORCE = 0x02)
[database_name:string] OR [schema_id:16]
[emulation_type:uint8] (postgres=1, mysql=2, firebird=3)
```

### 2. Executor Handlers

Need to implement in `src/sblr/executor.cpp`:
- `executeCreateSchema()` - Call CatalogManager::createSchema()
- `executeDropSchema()` - Call CatalogManager::dropSchema() with CASCADE
- `executeAlterSchema()` - Call CatalogManager::alterSchema()
- `executeCreateDatabase()` - Create schema under emulation tree + views
- `executeDropDatabase()` - Prune emulation schema tree + views
- `executeAlterDatabase()` - Modify emulation schema properties

### 3. Parser Implementations

**PostgreSQL (`pg_parser_ddl.cpp`):**
- ✅ parseCreateDatabase() emits EXT_CREATE_DATABASE
- ✅ parseCreateSchema() emits EXT_CREATE_SCHEMA
- ✅ parseDropDatabase() emits EXT_DROP_DATABASE
- ✅ parseDropSchema() emits EXT_DROP_SCHEMA
- ✅ parseAlterDatabase() emits EXT_ALTER_DATABASE
- ✅ parseAlterSchema() emits EXT_ALTER_SCHEMA

**MySQL (`mysql_parser.cpp`):**
- ✅ parseCreateDatabase() emits EXT_CREATE_DATABASE (CREATE SCHEMA synonym)
- ✅ parseDropDatabase() emits EXT_DROP_DATABASE (DROP SCHEMA synonym)
- ✅ parseAlterDatabase() emits EXT_ALTER_DATABASE (SET OPTIONS)

**Firebird (`firebird_parser.cpp`):**
- ✅ parseCreateDatabase() emits EXT_CREATE_DATABASE
- ✅ parseDropDatabase() emits EXT_DROP_DATABASE
- ✅ parseAlterDatabase() emits EXT_ALTER_DATABASE (alias support)

### 4. Catalog Manager Extensions

Verify/implement:
- ✅ `createSchema()` - Already exists
- ✅ `dropSchema()` - CASCADE + dependency blocking implemented
- ✅ `alterSchema()` - Covered by rename/update owner/move via `moveObject`
- ✅ `createEmulatedDatabase()` - Implemented + wired from executor
- ✅ `dropEmulatedDatabase()` - Implemented + wired from executor

### 5. View Generation

Emulated catalog views are generated/dropped via `EmulationViewGenerator` during
CREATE/DROP DATABASE execution. Coverage includes:

**PostgreSQL:**
- `pg_catalog.pg_database` - Database list
- `pg_catalog.pg_namespace` - Schema list
- All other pg_catalog views scoped to "database" (schema tree)

**MySQL:**
- `information_schema.SCHEMATA` - Schema list
- All information_schema views scoped to "database" (schema tree)

**Firebird:**
- `RDB$DATABASE` - Database metadata
- All RDB$ system tables scoped to "database" (schema tree)

---

## Estimated Implementation Effort

### PLAN_02B: Schema/Database DDL Infrastructure (NEW)

**Tasks:** ~20 tasks
**Estimated Time:** 60-80 hours (single developer)

**Sections:**
1. SBLR Opcode Definitions (4 hours)
2. Executor Handlers (16 hours)
3. PostgreSQL Parser Implementation (12 hours)
4. MySQL Parser Implementation (12 hours)
5. Firebird Parser Implementation (12 hours)
6. Catalog Manager Extensions (8 hours)
7. View Generation Framework (12 hours)
8. Testing (24 hours)

**Critical Path Dependencies:**
- Plan 02B MUST be completed before:
  - Plan 03B (Domain Infrastructure)
  - Plan 04 (Domain Parsers)
  - Any emulated database work

---

## Recommendations

### Option 1: Create PLAN_02B (Recommended)

Create a new plan for Schema/Database DDL that must be completed before Plan 03B and Plan 04.

**Sequence:**
1. PLAN_02B - Schema/Database DDL (60-80 hours)
2. PLAN_03B - Domain Infrastructure (138-192 hours)
3. PLAN_04 - Domain Parsers (depends on both)

**Total before Plan 04:** 198-272 hours

### Option 2: Include in Plan 03B

Merge schema/database DDL into Plan 03B as prerequisite work.

**Issue:** Plan 03B already large (35 tasks, 210 hours)
**New Total:** 55 tasks, 270-290 hours

### Option 3: Quick Stub for Plan 04 Testing

Implement minimal CREATE/DROP SCHEMA opcodes just for testing domains.

**Issue:** Violates NO STUBS rule
**Issue:** Doesn't solve emulation problem

---

## User Decision Required

1. **Should we create PLAN_02B for schema/database DDL?**
2. **Should Plan 04 wait for both Plan 02B and Plan 03B?**
3. **Is the emulation architecture description correct?**
   - PostgreSQL CREATE DATABASE → schema under emulation.postgres
   - MySQL CREATE DATABASE/SCHEMA → schema under emulation.mysql
   - Firebird CREATE DATABASE → schema under emulation.firebird
4. **Priority: Should schema/database DDL be fixed before Plan 04?**

---

## Verification Needed

Please confirm:
- ✅ Emulation architecture understanding correct?
- ✅ CREATE DATABASE should create schema under emulation tree?
- ✅ DROP DATABASE should CASCADE prune schema tree?
- ✅ Catalog views should be generated per emulated database?
- ✅ This is indeed a blocker for emulation?

---

## Next Steps (Pending User Approval)

1. Create PLAN_02B_SCHEMA_DATABASE_DDL.md
2. Define all opcodes and payloads
3. Implement executor handlers
4. Fix all three parsers
5. Add comprehensive testing
6. Then proceed with Plan 03B → Plan 04

---

**END OF CRITICAL FINDING**
