# SBLR Execution Performance Specification (Alpha)

Status: Draft (Alpha). Scope: reduce interpreter overhead for hot PSQL functions and triggers
without introducing native code generation.

This specification is derived from `SBLR_EXECUTION_PERFORMANCE_RESEARCH.md`.

---

## Goals

- Improve per-row execution speed for PSQL functions and triggers.
- Reuse compiled SBLR across calls and sessions when safe.
- Keep correctness and invalidation rules explicit and conservative.
- Avoid native code generation in Alpha.

## Non-Goals (Alpha)

- JIT/AOT native compilation.
- Vectorized execution across row batches (Beta).
- Cross-architecture native artifact storage.

---

## Tiered Execution Model (Alpha)

### Tier 0: Baseline Interpreter
Current SBLR stack machine execution.

### Tier 1: Optimized Bytecode
Low-risk speedups without changing semantics:
- Super-instructions for common opcode sequences.
- Type-specialized expression fast paths.
- Pre-resolved column offsets and collation metadata.

### Tier 2: Profiled Optimizations
Selective optimizations for hot routines:
- Per-module profiling (call count, inclusive time).
- Function/trigger inlining with safety constraints.
- Expanded super-instruction catalog based on profiling.

---

## Functional Requirements

### A) Persisted SBLR for PSQL Modules
- Store compiled SBLR for procedures, functions, and triggers in system tables.
- Maintain a validity flag similar to Firebird's `RDB$VALID_BLR`.
- Invalidate on dependency changes (domains, columns, parameter signatures).
- Recompile on first use after invalidation.

### B) Shared Plan Cache for PSQL Loops
- Cache statement plans for repeated SQL inside PSQL loops.
- Key by (statement text, resolved object IDs, parameter types).
- Expose a cache invalidation path when dependent objects change.

### C) Super-Instructions
- Define a small, fixed set of fused opcodes for hot patterns.
- Examples: LOAD+COMPARE, LOAD+ADD, LOAD+STORE, PUSH+CALL.
- Bytecode generator can emit fused ops when enabled.

### D) Type-Specialized Fast Paths
- Specialize numeric and text comparisons to bypass dynamic type checks.
- Use guards to fall back to generic evaluation when types mismatch.

### E) Function/Trigger Inlining
- Inline small, deterministic PSQL functions into callers.
- Inline triggers into DML execution paths when safe.
- Must respect security context, permissions, and transactional semantics.
- Provide a NO_INLINE attribute for opt-out.

### F) Profiling and Hotness
- Record call counts and inclusive execution time per module.
- Use thresholds to trigger Tier 2 optimizations.
- Provide a session/system toggle for profiling.

---

## Configuration (Alpha)

Use `sb_config.ini` with a dedicated `[sblr]` section and snake_case keys:

```ini
[sblr]
optimization_level = 0      # 0..2
superinsn_enabled = true
type_specialization = true
inline_threshold_bytes = 256
profile_enabled = true
profile_hot_ms = 50
```

---

## Observability

Expose counters for:
- Interpreter vs optimized bytecode executions.
- Inline hits/misses.
- Per-module call counts and time spent.

Possible surfaces:
- `sys.performance` extensions (if available).
- CLI diagnostic output.

---

## Correctness and Invalidation

- Any dependency change invalidates compiled SBLR and inline caches.
- Fall back to Tier 0 on any validation failure.
- All optimizations must preserve NULL and collation semantics.

---

## Testing Requirements

- Regression tests for identical results across tiers.
- Hot-loop microbenchmarks for PSQL + triggers.
- Invalidation tests (schema changes, domain changes, parameter changes).
