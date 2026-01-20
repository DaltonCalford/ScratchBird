# SBLR Execution Performance Research (Alpha)

Purpose: Identify proven techniques for speeding up bytecode and stored-procedure execution,
especially for hot PSQL functions and per-row triggers.

This document surveys relevant systems and translates their approaches into ScratchBird options.

---

## Observations (ScratchBird Today)

- SQL is compiled to SBLR with object references already resolved to IDs, so name lookups are
  not on the hot path.
- The remaining cost is execution overhead: interpreter dispatch, dynamic typing, per-row
  function calls, and repeated expression evaluation inside loops and triggers.

---

## What Other Engines Do

### Firebird (BLR as stored procedure bytecode)
- Firebird stores PSQL source and its BLR bytecode in system tables
  (e.g., `RDB$PROCEDURE_BLR`), which implies a compiled representation is stored and reused.
  `ScratchBird/docs/specifications/reference/firebird/FirebirdReferenceDocument.md:45288`.
- This model supports compile-once, execute-many with invalidation tracking
  (`RDB$VALID_BLR` in Firebird).

### Oracle Database (PL/SQL compilation + optional native code)
- PL/SQL units are compiled and stored in the database for reuse:
  "A package is compiled and stored in the database, where many applications can share its
  contents."
  https://docs.oracle.com/cd/E11882_01/appdev.112/e25519/overview.htm
- Oracle supports native compilation of PL/SQL into machine code:
  "Compiling PL/SQL Units for Native Execution ... compile them into native code
  (processor-dependent system code), which is stored in the SYSTEM tablespace."
  https://docs.oracle.com/cd/E11882_01/appdev.112/e25519/tuning.htm

### SQLite (SQL compiled to bytecode VM)
- SQLite translates SQL into bytecode and runs it in a virtual machine.
  The SQLite opcode reference describes this compilation and execution model.
  https://www.sqlite.org/opcode.html
- SQL must be compiled into a bytecode program using `sqlite3_prepare_v2()`; prepared
  statements are the compiled objects that can be reused.
  https://www.sqlite.org/c3ref/prepare.html

### MySQL (prepared statements for reuse)
- The PREPARE statement creates a named prepared statement for later execution:
  "The PREPARE statement prepares a SQL statement and assigns it a name ... The prepared
  statement is executed with EXECUTE and released with DEALLOCATE PREPARE."
  https://dev.mysql.com/doc/refman/8.0/en/prepare.html

### SQL Server (plan cache + native compilation for In-Memory OLTP)
- SQL Server caches compiled execution plans:
  "The part of the memory pool that is used to store execution plans is referred to as
  the plan cache."
  https://learn.microsoft.com/en-us/sql/relational-databases/query-processing-architecture-guide?view=sql-server-ver16
- In-Memory OLTP supports native compilation of stored procedures:
  "Natively compiled stored procedures are Transact-SQL stored procedures compiled to
  machine code, rather than interpreted by the query execution engine."
  https://learn.microsoft.com/en-us/sql/relational-databases/in-memory-oltp/natively-compiled-stored-procedures?view=sql-server-ver16

### PostgreSQL (plan caching + JIT for hot paths)
- PL/pgSQL caches query plans for reused execution paths.
  https://www.postgresql.org/docs/current/plpgsql-statements.html
- PostgreSQL has built-in JIT compilation using LLVM when built with `--with-llvm`, and
  JIT can accelerate expression evaluation and tuple deforming.
  https://www.postgresql.org/docs/current/jit-reason.html

---

## Techniques to Apply to SBLR

### A) Compile-Once, Execute-Many (Baseline)
Goal: ensure PSQL and triggers always reuse compiled SBLR.
- Persist compiled SBLR for PSQL/trigger modules with invalidation flags.
- Cache resolved column offsets and type descriptors at compile time.
- Use a shared plan cache for repeated statements inside PSQL loops.

### B) Interpreter Dispatch Optimization
Goal: reduce opcode dispatch overhead for hot loops.
- **Direct-threaded dispatch** (computed goto) or super-instructions.
- **Super-instructions**: fuse common opcode pairs (e.g., LOAD+ADD, LOAD+COMPARE).
- **Stack-to-register rewrite** for hot blocks to reduce stack traffic.

### C) Specialization by Type and Shape
Goal: remove dynamic type checks from hot paths.
- Generate specialized bytecode variants for common type combinations.
- Cache per-expression "fast paths" (e.g., numeric-only, text-only).
- Pre-resolve collation and charset handling where possible.

### D) Function and Trigger Inlining
Goal: avoid per-row call overhead.
- Inline small deterministic PSQL functions into the caller bytecode.
- Inline per-row triggers into the DML execution pipeline when safe.
- Provide a "no-inline" attribute for correctness-sensitive modules.

### E) JIT / AOT Native Compilation (Tiered)
Goal: native code for the hottest routines.
- Tier 0: interpreter (current).
- Tier 1: optimized bytecode (super-instructions, specialization).
- Tier 2: native JIT (LLVM/Cranelift) for hot loops and predicates.
- AOT option for frequently called functions (similar to Oracle/PG optional JIT).

### F) Vectorized Execution for PSQL Loops
Goal: reduce per-row overhead by batching.
- Evaluate filters and expressions in batches (columnar/vector style).
- Use "rowset" processing for triggers where semantics allow it.

---

## ScratchBird-Specific Recommendations (Alpha)

### Short-Term (Alpha)
- Add **SBLR plan cache** for PSQL loops and triggers (reuse compiled blocks).
- Add **super-instruction** support for a small set of hot opcode sequences.
- Add **type-specialized** expression paths for numeric and text comparisons.
- Add **inline hints** for small PSQL functions (manual or heuristics).

### Mid-Term (Alpha/Beta Boundary)
- Implement **tiered execution** (opt bytecode) and profile hot functions.
- Build a **fast-path executor** for tight loops (expression-only kernels).

### Longer-Term (Beta)
- Add **JIT/AOT** compilation of hot PSQL functions and triggers.
- Add **vectorized expression evaluation** for row batches.

---

## Risks and Tradeoffs

- JIT adds complexity, security surface, and build/toolchain dependencies.
- Specialization and inlining require careful invalidation and dependency tracking.
- Vectorization can conflict with row-level trigger semantics.

---

## Decision Matrix (Cost / Complexity / Benefit)

Legend: Low / Medium / High.

| Technique | Cost | Complexity | Benefit | Notes |
|---|---|---|---|---|
| Compile-once, execute-many (persisted SBLR + invalidation) | Low | Low | High | Firebird-style BLR storage with validity flag. |
| Shared plan cache for PSQL loops | Medium | Medium | High | Reduces repeated parse/plan inside hot loops. |
| Super-instructions | Medium | Medium | Medium | Good for frequent opcode sequences; bounded scope. |
| Direct-threaded dispatch | Medium | Medium | Medium | Requires compiler support; good interpreter speedup. |
| Type-specialized fast paths | Medium | Medium | High | Removes dynamic checks for common numeric/text ops. |
| Function/trigger inlining | Medium | High | High | Needs dependency tracking + safety rules. |
| Vectorized expression eval | High | High | High | Strong throughput gains but semantics risk. |
| JIT compilation (LLVM/Cranelift) | High | High | High | Great for hot loops; heavy tooling/security. |
| AOT native compilation for PSQL | High | High | High | Oracle/SQL Server style; longer toolchain. |

---

## Appendix: Tiered Execution Roadmap (Recommended)

### Alpha (Immediate)
- **Tier 0 (baseline)**: Current interpreter with SBLR bytecode.
- **Tier 1 (low-risk speedups)**:
  - Persisted SBLR for PSQL/triggers + invalidation flags.
  - Shared plan cache for repeated statements in loops.
  - Type-specialized fast paths for numeric/text comparisons.
  - Small super-instruction set for top opcode pairs.

### Alpha (Performance Expansion)
- **Tier 2 (profiled optimizations)**:
  - Hot-function profiling (call counts + wall time).
  - Trigger/function inlining with safety constraints.
  - Expanded super-instructions + direct-threaded dispatch.

### Beta (Optional, High-Impact)
- **Tier 3 (native execution)**:
  - JIT for hot expressions and loops (LLVM/Cranelift).
  - Optional AOT for frequently called PSQL modules.
  - Vectorized execution for eligible row batches.

---

## Action Items (Docs)

- Define SBLR "tiered execution" vocabulary in `docs/specifications/sblr`.
- Add a performance roadmap section to SBLR specs once the strategy is selected.
- Add a validation/invalidation policy for compiled PSQL modules (BLR-style).

## Related Specifications

- `SBLR_EXECUTION_PERFORMANCE_ALPHA.md`
- `SBLR_EXECUTION_PERFORMANCE_BETA.md`
