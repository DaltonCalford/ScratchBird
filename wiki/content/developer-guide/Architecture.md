# Architecture

**Purpose:** Defines ScratchBird's architectural layers, component boundaries, trust model, and where different types of work belong.

**Last Updated:** 2026-01-30

---

## The Complete Round-Trip Flow

```
═══════════════════════════════ REQUEST PATH ═══════════════════════════════

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
      OPCODE_CALL_FUNCTION 'scratchbird_version'
   │
   ▼
Native Protocol:
   └─ SBLR bytecode sent to Server Core
   │
   ▼
Server Core (Dialect-Agnostic):
   ├─ Executor interprets SBLR opcodes
   ├─ Retrieves current_user from session context → 'postgres'
   ├─ Calls scratchbird_version() function → 'ScratchBird Alpha'
   └─ Returns native result set:
      { columns: [STRING, STRING], rows: [['postgres', 'ScratchBird Alpha']] }

═══════════════════════════════ RESPONSE PATH ══════════════════════════════

Server Core:
   └─ Returns native result set
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

---

## Layer Responsibilities

### Layer 1: Client Application

**Responsibility:** Send SQL statements or API calls via chosen wire protocol

**Examples:** `psql`, `mysql` CLI, JDBC driver, ODBC driver, Python `psycopg2`

**Implementation:** External (not part of ScratchBird core)

---

### Layer 2: Wire Protocol Layer

**Responsibility:** Marshal/unmarshal messages for specific protocols

**Location:** `src/protocol/` (wire protocol) and `src/protocol/adapters/` (dialect adapters)

**Supported Protocols:**
| Protocol | Port | Status |
|----------|------|--------|
| ScratchBird Native | 3092 | Alpha |
| PostgreSQL | 5432 | Alpha |
| MySQL | 3306 | Alpha |
| Firebird | 3050 | Alpha |
| TDS/SQL Server | 1433 | Post-gold |

**Key Point:** This layer only handles **binary protocol encoding/decoding**, NOT parsing or API mapping.

---

### Layer 3: Port Listener & Connection Pool Manager

**Responsibility:** Accept connections, authenticate, route to parser

**Location:** `src/network/` and `src/security/`

**Components:**
- Port listeners (5432, 3306, 3050, 3092)
- Connection handler, event loop, thread pool
- Authentication: SCRAM-SHA-256, Kerberos/GSSAPI, LDAP, OAuth 2.0, SAML, MFA, TLS client certificates
- Session management and connection context
- Login attempt tracking and password policy enforcement

---

### Layer 4: Parser & API Layer (BIDIRECTIONAL)

**Responsibility:** THE MOST CRITICAL LAYER FOR MULTI-DIALECT SUPPORT

This layer handles BOTH directions:

**REQUEST PATH (Incoming):**
1. **Receive** SQL text or API call from wire protocol
2. **Parse** using dialect-specific parser (PostgreSQL, MySQL, Firebird)
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
├── parser_v2.cpp              # V2 unified parser (native ScratchBird)
├── lexer_v2.cpp               # V2 lexer
├── ast_v2.cpp                 # V2 AST nodes
├── parser_state_v2.cpp        # V2 parser state management
├── schema_path_v2.cpp         # Schema path resolution
├── sb_parser_main.cpp         # Parser entry point
├── firebird/                  # Firebird emulated parser
│   ├── firebird_parser.cpp    # Firebird SQL parsing
│   └── firebird_lexer.cpp     # Firebird lexer
├── postgresql/                # PostgreSQL emulated parser
│   ├── pg_parser.cpp          # PostgreSQL SQL parsing
│   ├── pg_parser_ddl.cpp      # PostgreSQL DDL statements
│   ├── pg_parser_dml.cpp      # PostgreSQL DML statements
│   ├── pg_parser_expr.cpp     # PostgreSQL expressions
│   ├── pg_parser_misc.cpp     # PostgreSQL misc statements
│   └── pg_lexer.cpp           # PostgreSQL lexer
└── mysql/                     # MySQL emulated parser
    ├── mysql_parser.cpp       # MySQL SQL parsing
    └── mysql_lexer.cpp        # MySQL lexer

src/sblr/
├── executor.cpp               # SBLR bytecode interpreter
├── bytecode_generator_v2.cpp  # Bytecode generation from V2 AST
├── bytecode_validator.cpp     # SBLR validation
├── semantic_analyzer_v2.cpp   # Semantic analysis
├── query_compiler_v2.cpp      # Native query compilation
├── expression_evaluator.cpp   # Expression evaluation
├── firebird_query_compiler.cpp  # Firebird dialect query compilation
├── postgresql_query_compiler.cpp # PostgreSQL dialect query compilation
├── mysql_query_compiler.cpp   # MySQL dialect query compilation
├── gin_extractors.cpp         # GIN index key extraction
├── index_cache.cpp            # Index operation cache
└── query_result_cache.cpp     # Result caching
```

**Critical Rules:**
- Each emulated parser is **COMPLETELY SEPARATE** from V2 parser
- DO NOT modify V2 parser for emulation purposes
- Each dialect parser **owns** its API surface area
- All parsers **output** SBLR bytecode (not dialect-specific bytecode)
- All parsers **format results** from native to dialect-specific format

---

### Layer 5: Native Wire Protocol & SBLR

**Responsibility:** Internal communication format

**Location:** `src/sblr/`

**Components:**
- SBLR bytecode specification (500+ opcodes)
- Bytecode serialization/deserialization
- Internal wire protocol for SBLR transport

**Key Point:** This is the **universal internal language** of ScratchBird. Everything above this layer translates to SBLR. Everything below this layer only understands SBLR.

---

### Layer 6: Server Core

**Responsibility:** Execute SBLR bytecode, manage data

**Location:** `src/core/`

**Components:**
- **Executor** (`src/sblr/executor.cpp`) - SBLR bytecode interpreter
- **Transaction Manager** - MGA transaction system
- **Storage Engine** (`src/core/storage_engine.cpp`) - Page management
- **Index Subsystem** (`src/core/`) - 14 index types (B-tree, Hash, GiST, GIN, SP-GiST, BRIN, R-tree, Bitmap, LSM-Tree, HNSW, Columnstore, Full-text, Inverted, expression)
- **Catalog System** (`src/core/catalog_manager.cpp`) - System catalog tables

**Critical Rule:** Server Core is **100% dialect-agnostic**. It only understands SBLR.

---

## Component Trust Model

```
┌─────────────────────────────────────────────────────────────┐
│                    TRUST BOUNDARIES                          │
├─────────────────────────────────────────────────────────────┤
│  UNTRUSTED                                                   │
│  ├─ Client Applications                                      │
│  ├─ Wire Protocol Layer                                      │
│  └─ Listener (never parses SQL, never bypasses engine)       │
├─────────────────────────────────────────────────────────────┤
│  SEMI-TRUSTED (validated by engine)                          │
│  └─ Parser Layer (generates SBLR, validated before execution)│
├─────────────────────────────────────────────────────────────┤
│  TRUSTED (authoritative)                                     │
│  └─ Server Core Engine                                       │
│      • SBLR validation and execution                         │
│      • Security enforcement                                  │
│      • Transaction isolation                                 │
│      • Data integrity                                        │
└─────────────────────────────────────────────────────────────┘
```

**Key Principle:** The engine is the sole authority for security, SBLR validation, and execution. Neither the listener nor the parser can bypass engine enforcement.

---

## Where Work Belongs: Quick Reference

| Task | Layer | Location |
|------|-------|----------|
| Add PostgreSQL `pg_stat_*` views | Parser | `src/parser/postgresql/` |
| Add MySQL `SHOW TABLES` | Parser | `src/parser/mysql/` |
| Add Firebird `RDB$*` tables | Parser | `src/parser/firebird/` |
| Format results for PostgreSQL client | Parser | `src/parser/postgresql/` |
| Format results for MySQL client | Parser | `src/parser/mysql/` |
| Add new SBLR opcode | SBLR | `include/scratchbird/sblr/opcodes.h` |
| Implement new index type | Core | `src/core/` |
| Add MGA transaction feature | Core | `src/core/` |
| Support new wire protocol | Network | `src/protocol/` and `src/protocol/adapters/` |
| Add connection pool tuning | Network | `src/network/` |
| Implement storage compression | Core | `src/core/` |

---

## Common Mistakes & How to Avoid Them

### Mistake 1: Adding dialect-specific code to Server Core

```cpp
// WRONG - in src/core/executor.cpp
if (dialect == "postgresql") {
    handle_pg_catalog_function();
}
```

**Fix:** Move this to `src/parser/postgresql/pg_catalog.cpp` and generate appropriate SBLR.

### Mistake 2: Modifying V2 parser for emulation

```cpp
// WRONG - in src/parser/parser_v2.cpp
if (emulating_mysql) {
    parse_mysql_specific_syntax();
}
```

**Fix:** Emulated parsers are **completely separate**. Use `src/parser/mysql/mysql_parser.cpp`.

### Mistake 3: Bypassing the parser layer

```cpp
// WRONG - wire protocol directly generating SBLR
postgresql_protocol.cpp:
    generate_sblr_directly(query);
```

**Fix:** Wire protocol only handles encoding/decoding. Parser layer generates SBLR.

### Mistake 4: Mixing dialects in shared code

```cpp
// WRONG - in src/core/catalog_manager.cpp
if (dialect == "mysql") {
    return mysql_information_schema;
} else if (dialect == "postgresql") {
    return pg_catalog;
}
```

**Fix:** Each parser layer provides its own catalog views that map to the common internal catalog.

---

## Decision Flowchart

When implementing a new feature, ask:

**Q1: Does this need to be dialect-specific?**
- **YES** → Parser Layer (`src/parser/<dialect>/`)
- **NO** → Server Core (`src/core/`, `src/sblr/`)

**Q2: Am I working with SQL text or wire protocol messages?**
- **SQL text** → Parser Layer
- **Wire messages** → Wire Protocol Layer

**Q3: Am I implementing a new feature or surfacing an existing one?**
- **New feature** → Server Core (implement in SBLR), then surface in parsers
- **Surfacing API** → Parser Layer (map to existing SBLR)

**Q4: Am I formatting results for a specific client type?**
- **YES** → Parser Layer (result formatter component)
- **NO** → Return native format from Server Core

---

## Related Documents

- [Core Engine](Core-Engine.md) - Engine internals and SBLR execution
- [Parsers and Emulation](Parsers.md) - Parser layer details
- [Network and Listeners](Network-Listeners.md) - Wire protocols and connection management
- [Transactions (MGA)](Transactions.md) - Transaction system rules
