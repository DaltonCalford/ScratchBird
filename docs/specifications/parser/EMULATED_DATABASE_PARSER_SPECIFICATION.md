# Emulated Database Parser Specification

**CRITICAL: Read this document before ANY work on database emulation parsers**

**Version:** 1.0
**Last Updated:** 2025-12-09
**Status:** MANDATORY SPECIFICATION

---

## References
- `docs/specifications/SCHEMA_PATH_RESOLUTION.md`
- `docs/specifications/wire_protocols/POSTGRESQL_EMULATION_BEHAVIOR.md`
- `docs/specifications/wire_protocols/MYSQL_EMULATION_BEHAVIOR.md`
- `docs/specifications/wire_protocols/FIREBIRD_EMULATION_BEHAVIOR.md`

---

## ABSOLUTE RULES (DO NOT VIOLATE)

### Rule 1: Parser Independence
**Emulated database parsers (Firebird, PostgreSQL, MySQL, etc.) are COMPLETELY SEPARATE from the ScratchBird V2 parser.**

- DO NOT use the V2 parser for emulated database parsing
- DO NOT modify the V2 parser to accommodate emulated databases
- DO NOT share parser code between V2 and emulated parsers
- DO NOT import V2 parser headers into emulated parsers
- Each emulated database has its OWN parser in its OWN directory

**HOWEVER, it IS valid to:**
- Use the V2 parser as a **reference/guide** for structural patterns
- Study how V2 maps SQL constructs to SBLR opcodes
- Follow similar coding style and organization
- Learn from V2's approach to SBLR generation

The V2 parser serves as documentation for how to generate correct SBLR bytecode.
Copy the patterns, not the code.

**Clarification:**
- ScratchBird V2 is the primary/core parser and supports ALL engine functionality.
- Emulated parsers are separate and must only implement the subset supported by their native engines.
- The engine executes only SBLR; parsers are isolated front-ends that generate it.

### Rule 2: No Legacy Parser Usage
**The legacy parser has been removed.**

- DO NOT reintroduce legacy parser code
- DO NOT write code that depends on legacy parser behavior
- DO NOT reference legacy parser code patterns

### Rule 3: SBLR is the Common Layer
**All parsers generate SBLR bytecode that the executor runs.**

- ScratchBird V2 Parser → SBLR → Executor
- Firebird Parser → SBLR → Executor
- PostgreSQL Parser → SBLR → Executor (future)
- MySQL Parser → SBLR → Executor (future)

The SBLR bytecode format is the SAME regardless of which parser generated it.

### Rule 4: Tablespace DDL is Rejected in Emulated Parsers
Emulated parsers must reject tablespace DDL and TABLESPACE clauses. ScratchBird V2 is the
only parser that can create or attach on-disk tablespace files.

- Reject `CREATE/ALTER/DROP/ATTACH/DETACH TABLESPACE`
- Reject `TABLESPACE` clauses in `CREATE TABLE`, `CREATE INDEX`, and `CREATE DATABASE`
- Emit a parser error that explains tablespaces are only supported by ScratchBird SQL

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                        SQL Input Layer                               │
├─────────────────┬─────────────────┬─────────────────┬───────────────┤
│ ScratchBird SQL │   Firebird SQL  │  PostgreSQL SQL │   MySQL SQL   │
│    (native)     │   (emulated)    │   (emulated)    │  (emulated)   │
└────────┬────────┴────────┬────────┴────────┬────────┴───────┬───────┘
         │                 │                 │                │
         ▼                 ▼                 ▼                ▼
┌────────────────┐ ┌───────────────┐ ┌───────────────┐ ┌─────────────┐
│ ScratchBird V2 │ │   Firebird    │ │  PostgreSQL   │ │   MySQL     │
│     Parser     │ │    Parser     │ │    Parser     │ │   Parser    │
│  (parser_v2/)  │ │  (firebird/)  │ │ (postgresql/) │ │  (mysql/)   │
└────────┬───────┘ └───────┬───────┘ └───────┬───────┘ └──────┬──────┘
         │                 │                 │                │
         ▼                 ▼                 ▼                ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    SBLR Bytecode (Common Format)                     │
└─────────────────────────────────────────────────────────────────────┘
                                   │
                                   ▼
┌─────────────────────────────────────────────────────────────────────┐
│                         SBLR Executor                                │
│                    (Single implementation)                           │
└─────────────────────────────────────────────────────────────────────┘
                                   │
                                   ▼
┌─────────────────────────────────────────────────────────────────────┐
│                      ScratchBird Storage Engine                      │
└─────────────────────────────────────────────────────────────────────┘
```

---

## File Organization

### ScratchBird V2 Parser (DO NOT MODIFY for emulation)
```
include/scratchbird/parser/
├── parser_v2.h              # V2 parser interface
├── lexer_v2.h               # V2 lexer
├── ast_v2.h                 # V2 AST nodes
└── semantic_analyzer_v2.h   # V2 semantic analysis

src/parser/
├── parser_v2.cpp
├── lexer_v2.cpp
├── ast_v2.cpp
└── semantic_analyzer_v2.cpp

src/sblr/
├── bytecode_generator_v2.cpp  # V2 → SBLR (for ScratchBird SQL only)
└── executor.cpp               # Executes SBLR from ANY parser
```

### Firebird Parser (SEPARATE implementation)
```
include/scratchbird/parser/firebird/
├── firebird_lexer.h         # Firebird-specific lexer
├── firebird_parser.h        # Firebird-specific parser
├── firebird_token.h         # Firebird token types
└── firebird_codegen.h       # Firebird → SBLR generator

src/parser/firebird/
├── firebird_lexer.cpp
├── firebird_parser.cpp
├── firebird_parser_ddl.cpp  # DDL parsing
├── firebird_parser_dml.cpp  # DML parsing
├── firebird_parser_psql.cpp # PSQL parsing
└── firebird_codegen.cpp     # Direct SBLR generation
```

### Future: PostgreSQL Parser (SEPARATE implementation)
```
include/scratchbird/parser/postgresql/
├── pg_lexer.h
├── pg_parser.h
└── pg_codegen.h

src/parser/postgresql/
├── pg_lexer.cpp
├── pg_parser.cpp
└── pg_codegen.cpp
```

---

## Emulated Database Concepts

### Schema-Based Emulation

Emulated databases are implemented as **schemas within ScratchBird**, not separate physical databases.

See `docs/specifications/SCHEMA_PATH_RESOLUTION.md` for authoritative path resolution rules
and relative/absolute semantics.

```
ScratchBird Database (single .sbdb file)
└── /sys                           # System schemas
└── /remote                        # Remote/emulation databases
    └── /emulation
        └── /firebird              # Firebird emulation root
            └── /localhost         # Server name
                └── /employee      # "Database" = schema
                    ├── EMP        # User table
                    ├── DEPT       # User table
                    └── RDB$*      # System table views
```

### CREATE DATABASE in Firebird

When a Firebird client issues `CREATE DATABASE 'employee'`:
1. ScratchBird creates a new SCHEMA: `/remote/emulation/firebird/localhost/employee`
2. NOT a new physical database file
3. The Firebird parser translates this to schema creation SBLR

### Hidden System Columns

ScratchBird has internal system columns (transaction IDs, row versions, UUIDs) that:
- ARE stored in the physical data
- ARE maintained by the engine
- ARE NOT visible to emulated database clients
- ARE filtered out by the emulated parser when generating SELECT bytecode

The Firebird parser generates SBLR that excludes these columns from result sets.

---

## Firebird Parser Requirements

### 1. Lexer Requirements

The Firebird lexer must handle:
- ~200 reserved keywords (different from ScratchBird)
- SQL Dialect 1, 2, 3 support
- Firebird-specific operators: `!<`, `!>`, `~=`, `^=`
- FIRST/SKIP instead of LIMIT/OFFSET
- `:=` assignment operator for PSQL
- `Q'{...}'` Q-string literals
- `_charset'string'` charset-prefixed strings

### 2. Parser Requirements

The Firebird parser must:
- Parse Firebird 5.0 SQL syntax directly
- Generate SBLR bytecode directly (no intermediate AST conversion)
- Support all Firebird DDL, DML, and PSQL statements
- NOT depend on V2 parser infrastructure
- NOT modify any V2 parser code

### 3. SBLR Generation Requirements

The Firebird codegen must:
- Generate SBLR bytecode that the executor understands
- Exclude hidden system columns from SELECT results
- Translate Firebird table names to schema-qualified paths
- Handle Firebird-specific functions by mapping to SBLR equivalents

### 4. System Table Requirements

The Firebird parser must support queries against:
- RDB$ tables (38 system tables) - mapped to ScratchBird catalog views
- MON$ tables (12 monitoring tables) - mapped to runtime statistics
- SEC$ tables (4 security tables) - mapped to security views

### 5. Monitoring View Mapping (All Dialects)

Emulated dialects must map their monitoring/system views to ScratchBird's native
SQL monitoring views defined in `operations/MONITORING_SQL_VIEWS.md`.

| Emulated View | ScratchBird Source |
| --- | --- |
| pg_stat_activity | sys.sessions + sys.statements |
| pg_locks | sys.locks |
| pg_stat_database | sys.performance |
| pg_stat_bgwriter | sys.performance |
| pg_stat_all_tables | sys.table_stats |
| information_schema.PROCESSLIST | sys.sessions |
| performance_schema.data_locks | sys.locks |
| performance_schema.global_status | sys.performance |
| MON$ATTACHMENTS | sys.sessions |
| MON$LOCKS | sys.locks |
| MON$STATEMENTS | sys.statements |
| MON$DATABASE | sys.performance |
| MON$TRANSACTIONS | sys.transactions |
| MON$IO_STATS | sys.io_stats |

Column-level mappings must preserve dialect expectations (names, types, and
nullable behavior), even if data is sourced from a shared ScratchBird view.
See `operations/MONITORING_DIALECT_MAPPINGS.md` for the per-column contract.

---

## Implementation Checklist

When implementing an emulated database parser:

- [ ] Create separate directory under `include/scratchbird/parser/{database}/`
- [ ] Create separate directory under `src/parser/{database}/`
- [ ] Implement lexer with database-specific keywords and operators
- [ ] Implement parser with database-specific syntax
- [ ] Implement codegen that produces SBLR directly
- [ ] DO NOT import or use V2 parser headers
- [ ] DO NOT modify V2 parser code
- [ ] Test that SBLR output is correct format for executor
- [ ] Implement system column filtering for SELECT
- [ ] Implement schema path translation

---

## Common Mistakes to Avoid

### WRONG: Modifying V2 to support Firebird
```cpp
// DON'T DO THIS - modifying V2 bytecode generator for Firebird
void BytecodeGeneratorV2::generateSelectListDialectCompatible(...) {
    // This is WRONG - V2 should not have Firebird-specific code
}
```

### WRONG: Using V2 parser for Firebird SQL
```cpp
// DON'T DO THIS
ParserV2 parser;
auto ast = parser.parse(firebird_sql);  // WRONG - V2 doesn't understand Firebird
```

### RIGHT: Separate Firebird parser
```cpp
// DO THIS
FirebirdParser parser;
auto bytecode = parser.parseAndGenerate(firebird_sql);  // Generates SBLR directly
executor.execute(bytecode);
```

---

## Questions and Answers

**Q: Can I use V2 parser code as a reference?**
A: YES. Use V2 as a guide for:
- How to structure the parser
- How to map SQL constructs to SBLR opcodes
- Coding patterns and organization
But write NEW code, don't import or copy-paste V2 code directly.

**Q: Can I reuse V2 AST node types for Firebird?**
A: No. The Firebird parser should generate SBLR directly, not AST nodes that need conversion.

**Q: Can I share utility functions between V2 and Firebird parsers?**
A: Only if they are truly generic (e.g., string escaping). Parser logic must be separate.

**Q: What if Firebird and ScratchBird SQL have identical syntax for something?**
A: Study how V2 handles it, then implement equivalent logic in the Firebird parser.
The Firebird parser must be self-contained but can follow the same patterns.

**Q: How do I test that my SBLR output is correct?**
A: Compare output with V2-generated SBLR for equivalent queries, verify executor accepts it.
Use the same SBLR opcodes - the bytecode format is shared, only the parser is separate.

---

## References

- Firebird 5.0 Language Reference (PDF in docs/specifications/)
- SBLR Opcodes: `include/scratchbird/sblr/opcodes.h`
- Executor Implementation: `src/sblr/executor.cpp`
- Schema Architecture: `docs/specifications/SCHEMA_ARCHITECTURE.md`
