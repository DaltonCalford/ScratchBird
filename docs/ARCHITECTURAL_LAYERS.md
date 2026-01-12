# ScratchBird Architectural Layers

**Version:** 1.0
**Last Updated:** January 2026
**Status:** CRITICAL - Read before any implementation work

## Purpose

This document defines the **strict architectural layers** of ScratchBird and explains where different types of work belong. If you're confused about where to implement a feature, **READ THIS FIRST**.

---

## The Complete Round-Trip Flow

```
                         REQUEST PATH (→)
┌─────────────────────────────────────────────────────────────────┐
│ 1. CLIENT APPLICATION                                           │
│    (psql, mysql client, JDBC, ODBC, custom apps)                │
└────────────────────────┬──────────────────────────────────────┬─┘
                         │                                      ▲
                         ▼                                      │
┌─────────────────────────────────────────────────────────────────┐
│ 2. WIRE PROTOCOL LAYER                                          │
│    REQUEST: Decode incoming protocol messages                   │
│    RESPONSE: Encode outgoing protocol messages                  │
│    • PostgreSQL Wire Protocol (port 5432)                       │
│    • MySQL Wire Protocol (port 3306)                            │
│    • Firebird Wire Protocol (port 3050)                         │
│    • TDS/SQL Server Wire Protocol (port 1433)                   │
│    • ScratchBird Native Protocol (port 5433)                    │
└────────────────────────┬──────────────────────────────────────┬─┘
                         │                                      ▲
                         ▼                                      │
┌─────────────────────────────────────────────────────────────────┐
│ 3. PORT LISTENER & CONNECTION POOL MANAGER                      │
│    • Accept incoming connections                                │
│    • Route to appropriate protocol handler                      │
│    • Manage connection pools                                    │
│    • Handle authentication                                      │
└────────────────────────┬──────────────────────────────────────┬─┘
                         │                                      ▲
                         ▼                                      │
┌─────────────────────────────────────────────────────────────────┐
│ 4. PARSER & API LAYER ⭐ CRITICAL (BIDIRECTIONAL)               │
│    ┌───────────────────────────────────────────────────────┐    │
│    │ PostgreSQL Parser ↔ PostgreSQL-specific API           │    │
│    │ MySQL Parser ↔ MySQL-specific API                     │    │
│    │ Firebird Parser ↔ Firebird-specific API               │    │
│    │ MSSQL Parser ↔ MSSQL-specific API                     │    │
│    └───────────────────────────────────────────────────────┘    │
│                                                                 │
│    REQUEST: Parse SQL → Generate SBLR                           │
│    RESPONSE: Convert native results → Dialect-specific format   │
└────────────────────────┬──────────────────────────────────────┬─┘
                         │                                      ▲
                         ▼                                      │
┌─────────────────────────────────────────────────────────────────┐
│ 5. NATIVE WIRE PROTOCOL & SBLR                                  │
│    • Internal communication format                              │
│    • SBLR bytecode (500+ opcodes)                               │
│    • Dialect-agnostic representation                            │
└────────────────────────┬──────────────────────────────────────┬─┘
                         │                                      ▲
                         ▼                                      │
┌─────────────────────────────────────────────────────────────────┐
│ 6. SERVER CORE (Dialect-Agnostic)                               │
│    • Query Executor (SBLR interpreter)                          │
│    • Transaction Manager (MGA)                                  │
│    • Storage Engine                                             │
│    • Index Subsystem                                            │
│    • Catalog System                                             │
└────────────────────────┴──────────────────────────────────────┬─┘
                                                                │
                       RESPONSE PATH (←)────────────────────────┘
```

---

## Critical Rule: Where APIs Belong

### ⭐ **IF YOU NEED AN API SURFACED, IT GOES IN THE PARSER LAYER**

**Example (Request Path):**

- PostgreSQL client sends `SELECT pg_catalog.version()`
- PostgreSQL wire protocol receives the message
- **PostgreSQL Parser** recognizes `pg_catalog.version()`
- **PostgreSQL Parser** surfaces this as a PostgreSQL-specific API function
- **PostgreSQL Parser** maps it to native ScratchBird SBLR bytecode
- SBLR is sent to Server Core for execution

**Example (Response Path):**

- Server Core returns native result: `{columns: [STRING], rows: [['ScratchBird 1.0']]}`
- **PostgreSQL Parser** receives native result
- **PostgreSQL Parser** converts to PostgreSQL format (maps types to PostgreSQL OIDs, formats column names)
- Formatted result sent to Wire Protocol layer
- Wire Protocol encodes as PostgreSQL wire protocol messages

### **WRONG Approach:**

❌ Adding PostgreSQL-specific functions directly to Server Core
❌ Mixing dialect-specific code in the Storage Engine
❌ Hardcoding MySQL syntax in the Transaction Manager
❌ Formatting results in the Wire Protocol layer

### **RIGHT Approach:**

✅ Each dialect parser handles its own syntax and API surface
✅ Parsers translate everything to dialect-agnostic SBLR (request path)
✅ Parsers convert native results to dialect-specific format (response path)
✅ Server Core only understands SBLR bytecode
✅ Server Core has NO knowledge of SQL dialects
✅ Wire Protocol only handles binary encoding/decoding

---

## Layer Responsibilities

### Layer 1: Client Application

**Responsibility:** Send SQL statements or API calls via chosen wire protocol
**Examples:** `psql`, `mysql` CLI, JDBC driver, ODBC driver, Python `psycopg2`
**Implementation:** External (not part of ScratchBird core)

### Layer 2: Wire Protocol Layer

**Responsibility:** Marshal/unmarshal messages for specific protocols
**Location:** `src/network/wire_protocol/`
**Files:**

- `postgresql_protocol.cpp` - PostgreSQL wire protocol
- `mysql_protocol.cpp` - MySQL wire protocol
- `firebird_protocol.cpp` - Firebird wire protocol
- `tds_protocol.cpp` - SQL Server TDS protocol
- `native_protocol.cpp` - ScratchBird native protocol

**Key Point:** This layer only handles **binary protocol encoding/decoding**, NOT parsing or API mapping.

### Layer 3: Port Listener & Connection Pool Manager

**Responsibility:** Accept connections, authenticate, route to parser
**Location:** `src/network/connection/`
**Components:**

- Port listeners (5432, 3306, 3050, 1433, 5433)
- Connection pool management
- Authentication (SASL, MD5, cleartext, etc.)
- Session management

### Layer 4: Parser & API Layer ⭐ (BIDIRECTIONAL)

**Responsibility:** THE MOST CRITICAL LAYER FOR MULTI-DIALECT SUPPORT

**This layer handles BOTH directions:**

**REQUEST PATH (Incoming):**

1. **Receive** SQL text or API call from wire protocol
2. **Parse** using dialect-specific parser (PostgreSQL, MySQL, Firebird, MSSQL)
3. **Surface** dialect-specific APIs and functions
4. **Map** to dialect-agnostic SBLR bytecode
5. **Send** SBLR to Server Core

**RESPONSE PATH (Outgoing):**

1. **Receive** native result set from Server Core
2. **Convert** to dialect-specific format (column names, type OIDs, etc.)
3. **Format** results as expected by the specific client dialect
4. **Return** formatted results to Wire Protocol layer

**Location:** `src/parser/`
**Structure:**

```
src/parser/
├── v2/                          # V2 unified parser (NOT for emulation)
├── emulated/                    # Emulated dialect parsers
│   ├── postgresql/              # PostgreSQL parser & API surface
│   │   ├── parser.cpp           # PostgreSQL SQL parsing
│   │   ├── pg_catalog.cpp       # pg_catalog.* functions
│   │   ├── pg_functions.cpp     # PostgreSQL built-in functions
│   │   ├── result_formatter.cpp # Convert native results to PG format
│   │   └── sblr_generator.cpp   # Map to SBLR
│   ├── mysql/                   # MySQL parser & API surface
│   ├── firebird/                # Firebird parser & API surface
│   └── mssql/                   # MSSQL parser & API surface
└── common/                      # Shared utilities
```

**Critical Rules:**

- Each emulated parser is **COMPLETELY SEPARATE** from V2 parser
- DO NOT modify V2 parser for emulation purposes
- Each dialect parser **owns** its API surface area
- All parsers **output** SBLR bytecode (not dialect-specific bytecode)
- All parsers **format results** from native to dialect-specific format

### Layer 5: Native Wire Protocol & SBLR

**Responsibility:** Internal communication format
**Location:** `src/sblr/`
**Components:**

- SBLR bytecode specification (500+ opcodes)
- Bytecode serialization/deserialization
- Internal wire protocol for SBLR transport

**Key Point:** This is the **universal internal language** of ScratchBird. Everything above this layer translates to SBLR. Everything below this layer only understands SBLR.

### Layer 6: Server Core

**Responsibility:** Execute SBLR bytecode, manage data
**Location:** `src/core/`
**Components:**

- **Executor** (`src/executor/`) - SBLR bytecode interpreter
- **Transaction Manager** (`src/transaction/`) - MGA transaction system
- **Storage Engine** (`src/storage/`) - Page management, GiST
- **Index Subsystem** (`src/indexes/`) - 11+ index types
- **Catalog System** (`src/catalog/`) - System catalog tables

**Critical Rule:** Server Core is **100% dialect-agnostic**. It only understands SBLR.

---

## Example Flow: PostgreSQL Client Query (Complete Round-Trip)

```
════════════════════════════ REQUEST PATH ════════════════════════════

Client:  psql -c "SELECT current_user, pg_catalog.version()"
   │
   ▼
Wire Protocol Layer (PostgreSQL):
   └─ Decodes PostgreSQL wire protocol message
   │
   ▼
Connection Manager:
   └─ Routes to PostgreSQL parser based on connection port/type
   │
   ▼
Parser Layer (PostgreSQL):
   ├─ PostgreSQL parser parses SQL text
   ├─ Recognizes current_user (PostgreSQL session variable)
   ├─ Recognizes pg_catalog.version() (PostgreSQL-specific function)
   ├─ Surfaces these as PostgreSQL-specific APIs
   └─ Generates dialect-agnostic SBLR bytecode:
      OPCODE_GET_SESSION_VAR 'current_user'
      OPCODE_CALL_FUNCTION 'scratchbird_version' (mapped from pg_catalog.version)
   │
   ▼
Native Protocol:
   └─ SBLR bytecode sent to Server Core
   │
   ▼
Server Core (Dialect-Agnostic):
   ├─ Executor interprets SBLR opcodes
   ├─ Retrieves current_user from session context → 'postgres'
   ├─ Calls scratchbird_version() function → 'ScratchBird 1.0'
   └─ Returns native result set:
      { columns: [STRING, STRING], rows: [['postgres', 'ScratchBird 1.0']] }

════════════════════════════ RESPONSE PATH ═══════════════════════════

Server Core:
   └─ Returns native result set
   │
   ▼
Native Protocol:
   └─ Native result set sent back up
   │
   ▼
Parser Layer (PostgreSQL):
   ├─ Receives native result set
   ├─ Converts to PostgreSQL-specific format:
   │  • Column names in PostgreSQL style
   │  • Data types mapped to PostgreSQL type OIDs
   │  • Result format expected by PostgreSQL clients
   └─ Produces PostgreSQL-formatted result
   │
   ▼
Wire Protocol Layer (PostgreSQL):
   └─ Encodes result in PostgreSQL wire protocol format
      (RowDescription + DataRow + CommandComplete messages)
   │
   ▼
Client (psql):
   └─ Receives and displays:
      current_user | version
      -------------+------------------
      postgres     | ScratchBird 1.0
```

**Key Points:**

- **Request Path:** Client SQL → Wire decode → Parser (SQL to SBLR) → Server Core
- **Response Path:** Server Core → Parser (native to dialect format) → Wire encode → Client
- **Parser is bidirectional:** Handles both SQL→SBLR translation AND result format conversion

---

## Common Mistakes & How to Avoid Them

### ❌ Mistake 1: Adding dialect-specific code to Server Core

```cpp
// WRONG - in src/executor/executor.cpp
if (dialect == "postgresql") {
    handle_pg_catalog_function();
}
```

**Fix:** Move this to `src/parser/emulated/postgresql/pg_catalog.cpp` and generate appropriate SBLR.

### ❌ Mistake 2: Modifying V2 parser for emulation

```cpp
// WRONG - in src/parser/v2/parser.cpp
if (emulating_mysql) {
    parse_mysql_specific_syntax();
}
```

**Fix:** Emulated parsers are **completely separate**. Use `src/parser/emulated/mysql/parser.cpp`.

### ❌ Mistake 3: Bypassing the parser layer

```cpp
// WRONG - wire protocol directly generating SBLR
postgresql_protocol.cpp:
    generate_sblr_directly(query);
```

**Fix:** Wire protocol only handles encoding/decoding. Parser layer generates SBLR.

### ❌ Mistake 4: Mixing dialects in shared code

```cpp
// WRONG - in src/catalog/system_tables.cpp
if (dialect == "mysql") {
    return mysql_information_schema;
} else if (dialect == "postgresql") {
    return pg_catalog;
}
```

**Fix:** Each parser layer provides its own catalog views that map to the common internal catalog.

---

## Where Work Belongs: Quick Reference

| Task                                 | Layer              | Location                                              |
| ------------------------------------ | ------------------ | ----------------------------------------------------- |
| Add PostgreSQL `pg_stat_*` views     | Parser Layer       | `src/parser/emulated/postgresql/pg_stat.cpp`          |
| Add MySQL `SHOW TABLES`              | Parser Layer       | `src/parser/emulated/mysql/show_commands.cpp`         |
| Add Firebird `RDB$*` tables          | Parser Layer       | `src/parser/emulated/firebird/rdb_tables.cpp`         |
| Format results for PostgreSQL client | Parser Layer       | `src/parser/emulated/postgresql/result_formatter.cpp` |
| Format results for MySQL client      | Parser Layer       | `src/parser/emulated/mysql/result_formatter.cpp`      |
| Add new SBLR opcode                  | SBLR Layer         | `src/sblr/opcodes.h`                                  |
| Implement new index type             | Server Core        | `src/indexes/`                                        |
| Add MGA transaction feature          | Server Core        | `src/transaction/`                                    |
| Support new wire protocol            | Wire Protocol      | `src/network/wire_protocol/`                          |
| Add connection pool tuning           | Connection Manager | `src/network/connection/pool_manager.cpp`             |
| Implement storage compression        | Server Core        | `src/storage/compression/`                            |

---

## Integration with Other Critical Documents

This document works together with:

1. **[MGA_RULES.md](../MGA_RULES.md)** - Transaction system rules (Server Core)
2. **[IMPLEMENTATION_STANDARDS.md](../IMPLEMENTATION_STANDARDS.md)** - Quality standards (all layers)
3. **[EMULATED_DATABASE_PARSER_SPECIFICATION.md](specifications/EMULATED_DATABASE_PARSER_SPECIFICATION.md)** - Parser layer details

**Read Order:**

1. **THIS DOCUMENT** - Understand where work belongs
2. **EMULATED_DATABASE_PARSER_SPECIFICATION.md** - If working on parsers/APIs
3. **MGA_RULES.md** - If working on transactions/storage
4. **IMPLEMENTATION_STANDARDS.md** - Before any implementation

---

## Summary: The Golden Rules

### Complete Flow (Both Directions)

1. **Client Apps** send SQL via **Wire Protocols**
2. **Wire Protocols** decode incoming messages (REQUEST) / encode outgoing messages (RESPONSE)
3. **Connection Manager** routes to appropriate **Parser**
4. **Parser Layer** is **BIDIRECTIONAL:**
   - **REQUEST:** Parse SQL → Generate SBLR → Send to Server Core
   - **RESPONSE:** Receive native results → Convert to dialect format → Return to Wire Protocol
5. **Parsers** translate everything to/from **SBLR bytecode**
6. **Server Core** only understands **SBLR** (no dialect knowledge)
7. **Each dialect is isolated** in its own parser (no mixing)

### Critical: Parser Layer Does TWO Jobs

- **Incoming:** SQL text → SBLR bytecode
- **Outgoing:** Native results → Dialect-specific format

### If you're confused about where to put something:

**Ask:** "Does this need to be dialect-specific?"

- **YES** → Parser Layer (`src/parser/emulated/<dialect>/`)
- **NO** → Server Core (`src/core/`, `src/executor/`, `src/storage/`, etc.)

**Ask:** "Am I working with SQL text or wire protocol messages?"

- **SQL text** → Parser Layer
- **Wire messages** → Wire Protocol Layer

**Ask:** "Am I implementing a new feature or surfacing an existing one?"

- **New feature** → Server Core (implement in SBLR), then surface in parsers
- **Surfacing API** → Parser Layer (map to existing SBLR)

**Ask:** "Am I formatting results for a specific client type?"

- **YES** → Parser Layer (result formatter component)
- **NO** → Return native format from Server Core

---

**Last Updated:** January 2026
**Version:** 1.0
**Status:** MANDATORY READING
