# SBLR Execution Performance Specification (Beta)

Status: Draft (Beta). Scope: native execution and batch evaluation for hot PSQL functions,
triggers, and expressions.

This specification is derived from `SBLR_EXECUTION_PERFORMANCE_RESEARCH.md`.

---

## Goals

- Introduce native execution tiers (JIT/AOT) for hot routines.
- Provide optional vectorized execution for eligible workloads.
- Preserve correctness with explicit invalidation and safety gates.

## Non-Goals (Beta)

- Mandatory JIT/AOT for all workloads (must be optional).
- Changing SQL semantics or trigger behavior.

---

## Tiered Execution Model (Beta)

### Tier 3: Native Execution (Optional)
- JIT compilation for hot expressions, loops, and predicates.
- Optional AOT compilation for frequently called PSQL modules.
- Configurable backend (LLVM or Cranelift).

### Vectorized Execution (Optional)
- Batch evaluation for filter/projection stages where semantics allow.
- Explicit exclusion for row-level triggers and side-effecting UDFs.

---

## Functional Requirements

### A) JIT Compilation
- JIT is gated by profiling thresholds and configuration.
- Generated code is cached per module + version + target architecture.
- JIT output is invalidated on dependency changes.
- All JIT paths must fall back to interpreter on errors.

### B) AOT Compilation (Optional)
- Compile selected PSQL modules to native code.
- Store artifacts in a versioned native cache (catalog or filesystem).
- Require ABI stability and architecture checks.

### C) Vectorized Execution
- Evaluate filters and expressions in batches where safe.
- Provide explicit opt-outs for triggers and side-effecting functions.
- Ensure order-dependent semantics are preserved when required.

---

## Configuration (Beta)

Use `sb_config.ini` with a dedicated `[sblr]` section and snake_case keys:

```ini
[sblr]
jit_enabled = false           # true|false
jit_backend = llvm            # llvm|cranelift
jit_hot_ms = 250
aot_enabled = false           # true|false
vectorized_enabled = false    # true|false
vectorized_batch_size = 128
```

---

## Security and Safety

- JIT/AOT must run under restricted policies; no unsafe codegen.
- Audit logging for JIT enable/disable and module compilation events.
- Explicit version and checksum validation for native artifacts.

---

## Observability

- JIT/AOT hit rates, compile time, and cache size.
- Vectorization eligibility counts and fallbacks.
- Per-module tier assignment and execution counts.

---

## Testing Requirements

- Correctness tests across tiers (interpreter vs JIT/AOT).
- Invalidation tests (schema changes, parameter changes).
- Performance benchmarks for hot PSQL loops and trigger paths.
