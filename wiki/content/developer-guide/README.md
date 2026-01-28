# Developer Guide

**Purpose:** Comprehensive guide to ScratchBird's internal architecture, design principles, and implementation details for contributors and maintainers.

**Last Updated:** 2026-01-28

---

## Core Design Philosophy

ScratchBird is a database engine built on three fundamental principles:

1. **Multi-Generational Architecture (MGA):** Uses Firebird-style MGA for transaction isolation - NOT PostgreSQL MVCC snapshots. See [Transactions](Transactions.md) for details.

2. **Multi-Protocol Support:** A single engine supports multiple SQL dialects (PostgreSQL, MySQL, Firebird) through isolated parser layers that all compile to a common bytecode (SBLR).

3. **Strict Architectural Layers:** Clear separation between wire protocols, parsers, and the dialect-agnostic server core.

---

## Architecture Overview

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
│    • PostgreSQL Wire Protocol (port 5432)                       │
│    • MySQL Wire Protocol (port 3306)                            │
│    • Firebird Wire Protocol (port 3050)                         │
│    • ScratchBird Native Protocol (port 3092)                    │
└────────────────────────┬──────────────────────────────────────┬─┘
                         │                                      ▲
                         ▼                                      │
┌─────────────────────────────────────────────────────────────────┐
│ 3. PORT LISTENER & CONNECTION POOL MANAGER                      │
│    • Accept incoming connections                                │
│    • Route to appropriate protocol handler                      │
│    • Manage connection pools                                    │
└────────────────────────┬──────────────────────────────────────┬─┘
                         │                                      ▲
                         ▼                                      │
┌─────────────────────────────────────────────────────────────────┐
│ 4. PARSER & API LAYER (BIDIRECTIONAL)                           │
│    REQUEST: Parse SQL → Generate SBLR                           │
│    RESPONSE: Convert native results → Dialect-specific format   │
└────────────────────────┬──────────────────────────────────────┬─┘
                         │                                      ▲
                         ▼                                      │
┌─────────────────────────────────────────────────────────────────┐
│ 5. SBLR BYTECODE LAYER                                          │
│    • Internal communication format                              │
│    • 500+ opcodes, dialect-agnostic                             │
└────────────────────────┬──────────────────────────────────────┬─┘
                         │                                      ▲
                         ▼                                      │
┌─────────────────────────────────────────────────────────────────┐
│ 6. SERVER CORE (Dialect-Agnostic)                               │
│    • Query Executor (SBLR interpreter)                          │
│    • Transaction Manager (MGA)                                  │
│    • Storage Engine                                             │
│    • Index Subsystem (14 index types)                           │
│    • Catalog System                                             │
└─────────────────────────────────────────────────────────────────┘
```

**Key Rule:** If you need an API surfaced, it goes in the Parser Layer. The Server Core is 100% dialect-agnostic.

---

## Guide Sections

### Architecture & Design
- [Architecture](Architecture.md) - Component boundaries, trust model, and layer responsibilities
- [Core Engine](Core-Engine.md) - SBLR execution, validation, and engine internals

### Data & Storage
- [Storage](Storage.md) - Buffer pool, heap pages, MGA-first storage design
- [Transactions (MGA)](Transactions.md) - Multi-Generational Architecture, TIP, visibility rules

### Bytecode & Execution
- [SBLR and BLR Mapping](SBLR.md) - ScratchBird Language Representation bytecode specification

### Protocol & Parsing
- [Parsers and Emulation](Parsers.md) - SQL dialect parsers and result formatting
- [Network and Listeners](Network-Listeners.md) - Wire protocols and connection management

### Quality & Testing
- [Security](Security.md) - Authentication, authorization, and security model
- [Testing and Audit](Testing-and-Audit.md) - Test infrastructure and audit framework

---

## Critical Documents

Before implementing features, read these documents:

| Document | When to Read |
|----------|--------------|
| `MGA_RULES.md` | Before ANY transaction/index work |
| `ARCHITECTURAL_LAYERS.md` | Before ANY parser/API/wire protocol work |
| `IMPLEMENTATION_STANDARDS.md` | Before ANY implementation |
| `COMPLETION_VERIFICATION_CHECKLIST.md` | Before marking tasks complete |

---

## Source Code Organization

```
src/
├── cli/                 # Command-line tools (sb-isql, sb-server, etc.)
├── core/                # Server core (storage, catalog, buffer pool, indexes, types)
├── network/             # Connection handling, event loop, thread pool
├── protocol/            # Wire protocol and dialect adapters
│   ├── wire_protocol.cpp
│   └── adapters/        # Native, PostgreSQL, MySQL, Firebird adapters
├── parser/              # SQL parsers (V2 native + emulated dialects)
│   ├── parser_v2.cpp    # Native V2 parser
│   ├── firebird/        # Firebird SQL emulation
│   ├── postgresql/      # PostgreSQL emulation
│   └── mysql/           # MySQL emulation
├── sblr/                # SBLR bytecode (semantic analyzer, generator, executor, query compilers)
├── security/            # Authentication methods (SCRAM, Kerberos, LDAP, OAuth, SAML, MFA, TLS)
├── executor/            # Parallel query executor
├── optimizer/           # Query planner and cost model
├── spatial/             # GEOS wrapper, WKB/WKT parsing
├── geo/                 # Geodetic operations, PROJ wrapper, SRID
├── server/              # Server main and IPC
└── testing/             # Test infrastructure
```

---

## Implementation Rules

### Where Work Belongs

| Task | Layer | Location |
|------|-------|----------|
| Add PostgreSQL `pg_stat_*` views | Parser | `src/parser/postgresql/` |
| Add MySQL `SHOW TABLES` | Parser | `src/parser/mysql/` |
| Add Firebird `RDB$*` tables | Parser | `src/parser/firebird/` |
| Format results for any dialect | Parser | Result formatter in parser |
| Add new SBLR opcode | SBLR | `include/scratchbird/sblr/opcodes.h` |
| Implement new index type | Core | `src/core/` |
| Add MGA transaction feature | Core | `src/core/` |
| Support new wire protocol | Network | `src/network/` |

### Decision Flowchart

**Ask:** "Does this need to be dialect-specific?"
- **YES** → Parser Layer
- **NO** → Server Core

**Ask:** "Am I working with SQL text or wire protocol messages?"
- **SQL text** → Parser Layer
- **Wire messages** → Wire Protocol Layer

**Ask:** "Am I formatting results for a specific client type?"
- **YES** → Parser Layer (result formatter)
- **NO** → Return native format from Server Core

---

## Building and Testing

```bash
# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)

# Run tests
ctest --output-on-failure

# Run specific test suite
./tests/unit/test_parser_v2
./tests/integration/test_bitmap_dml
```

---

## Contributing

1. Read the relevant architectural documents first
2. Run the foundation audit before implementing features
3. Write tests for all code paths
4. Include restart/persistence tests for stateful features
5. Verify against `COMPLETION_VERIFICATION_CHECKLIST.md`

---

## Related Resources

- [Language Guides](../language-guides/README.md) - SQL syntax reference by dialect
- [Reference Documentation](../reference/README.md) - Functions, operators, data types
- [User Guides](../user-guides/README.md) - Feature guides for end users
