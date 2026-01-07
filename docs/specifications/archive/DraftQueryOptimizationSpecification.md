# ScratchBird Vectorized SELECT Executor Spec (Alpha, repo-mapped)

## 1) Goals, non-goals, invariants

### 1.1 Goals

- Replace/augment the current row-at-a-time `Executor::executeSelect` loop in `src/sblr/executor.cpp` with a **vectorized SELECT pipeline** (batch-at-a-time).

- Preserve MGA visibility exactly as enforced today by:
  
  - `core::StorageEngine::isVisible(xmin, xmax, current_xid)` in `src/core/storage_engine.cpp`
  
  - and the underlying `TransactionManager::{isTransactionVisible,isVersionVisible,...}` in `include/scratchbird/core/transaction_manager.h`.

- Preserve security enforcement (permissions + RLS) with identical semantics:
  
  - Use `Executor::checkPermission(...)` for object/column permissions.
  
- Introduce vectorized RLS evaluation for SELECT (new), while keeping current write-path RLS checks as-is.

### 1.2 Target outcome (Beta readiness)

By the end of this effort, ScratchBird should support:

- A vectorized SELECT execution path with batch-at-a-time operators.

- Correct MGA visibility, permissions, and RLS semantics identical to the row executor.

- A clear feature flag and fallback path to the existing executor (`vector_select=off|on|auto`).

- A defined compatibility surface for parser/SBLR bytecode changes to enable future operators.

- A test plan that compares row executor vs vector executor for supported query shapes.

Performance expectations (Alpha/Beta baseline)

- Vector path should reduce per-row overhead vs the current row loop, even if storage remains row-oriented.

- Correctness and security take priority over micro-optimizations in Phase 1.

### 1.3 Non-goals (Alpha)

- No need to wire the cost-based `QueryPlanner` into runtime immediately (but structure must accommodate it).

- No JIT required initially.

- No full parallel execution in Phase 1 (but define plan hooks).

### 1.4 Invariants

**Visibility:** For every tuple returned by vectorized SELECT, the tuple header `(xmin,xmax)` must pass the same predicate currently applied inside `core::HeapScanIterator::next`:

`if (engine_->isVisible(hdr->xmin, hdr->xmax, engine_->getCurrentXid())) { ... }`

**Security:**

- Permissions: must call `Executor::checkPermission(table_id, SELECT, columns)` before producing any rows.

- RLS: must be enforced for SELECT. Since you currently enforce RLS on DML, SELECT RLS must be added in a way consistent with your policy structures (details below).

### 1.5 Parser/SBLR compatibility contract (required)

This spec governs any parser or SBLR changes that affect SELECT planning or execution for the vector path.

Any change to:

- the SELECT bytecode format,

- expression opcodes (WHERE, projection, RLS),

- column reference encoding,

must preserve compatibility with the vector plan builder or explicitly update it.

If a change makes vector planning ambiguous or incomplete, the system must fall back to the row executor in `vector_select=auto`.

### 1.6 Traceability requirement (mandatory)

All parser and SBLR work related to SELECT planning/execution must reference this spec.

Minimum requirement:

- add a short reference in the relevant code change (comment or doc entry) in the form:

  `Spec: docs/specifications/DraftQueryOptimizationSpecification.md`

This ensures future diffs clearly document spec alignment.

### 1.7 Parser/SBLR checklist (required for changes)

Use this checklist for any parser or SBLR change that affects SELECT planning or execution.

- If SELECT bytecode format changes, update the vector plan builder or force fallback (`vector_select=auto`).

- If expression opcodes or AST nodes used by WHERE/projection/RLS change, update the vector expression binder/evaluator.

- If column reference encoding changes, update `CatalogManager::extractColumnRefsFromBytecode` and vector column resolution.

- If new SELECT forms are enabled (joins, aggregates, window, set ops), update the vector support matrix and fallback detection.

- If policy predicate parsing changes, ensure `RLSBundle` resolution uses the same binder/typing rules.

- Ensure `vector_select=auto` falls back for unsupported features and `vector_select=on` returns a clear error.

- Add spec reference in the change: `Spec: docs/specifications/DraftQueryOptimizationSpecification.md`

- Update row-vs-vector parity tests for any newly supported shape.

---

## 2) Architecture overview (ScratchBird modules)

### 2.1 New modules (recommended paths)

Create a new subsystem under something like:

- `include/scratchbird/vecexec/`

- `src/vecexec/`

Key files/classes:

- `vecexec/batch.h` (Vector, Batch, Selection)

- `vecexec/operator.h` (Operator interface)

- `vecexec/exec_context.h` (ExecContext + memory/cancel hooks)

- `vecexec/operators/seq_scan.{h,cpp}`

- `vecexec/operators/filter.{h,cpp}`

- `vecexec/operators/project.{h,cpp}`

- `vecexec/operators/limit.{h,cpp}`

- later: join/agg/sort

### 2.2 Integration point: Executor dispatch

In `src/sblr/executor.cpp`, in/near `executeSelect`, add:

- a feature flag (session / config):
  
  - `vector_select = off|on|auto`

Dispatch logic:

- if vector engine supports query shape → run vector engine

- else fallback to existing bytecode / row path

This keeps compatibility with “emulated engines” and incomplete operator coverage.

---

## 3) Execution interface and batch format (exact)

### 3.1 ExecContext: include MGA + security context

`ExecContext` must include pointers you already have in `Executor`:

- db / catalog access

- connection context (isolation level; current user/roles)

- `StorageEngine*` (to call isVisible and create scans)

- `CancellationToken*` optional

### 3.2 Operator API

Use pull-based vector iterator:

- `open(ctx)`

- `next(ctx, Batch& out) -> NextStatus`

- `close(ctx)`

### 3.3 Batch layout: columnar with selection vector

Even though storage is row/page-oriented, the scan operator will **materialize** a batch into column vectors.

Minimum viable `Batch`:

- `count`

- `Selection sel` (optional active list)

- `std::vector<Vector> cols`

`Vector`:

- fixed-width: contiguous `T[]`

- null bitmap optional

- varlen: (Phase 1) copy into batch-owned varlen buffer for safety

Batch size default: 1024 (tunable compile-time + runtime config).

---

## 4) Storage access design (critical ScratchBird mapping)

You have two practical implementation paths for the vector scan given current storage interfaces:

### Option A (fastest to ship): “Batchifying” the existing scan iterator

- Extend `core::HeapScanIterator` with a `nextBatch(...)` method *or* wrap it.

**Wrapper approach (no storage refactor):**

- Use `StorageEngine::createScan(table_id)` as today.

- Repeatedly call `scan_iter->next(&tuple)` and fill arrays until batch full / EOS.

- For each tuple, you already know `HeapScanIterator::next` has *already* applied `isVisible` internally, so the wrapper doesn’t need to recheck MGA.

**Pros:** minimal risk, MGA correctness “for free.”  
**Cons:** still pays some per-row iterator overhead; but the big win is vectorized expression/projection and less overhead downstream.

### Option B (better long-term): Storage-level batch read with visibility as a vector filter

- Add a storage API that reads N tuples from a page range into a scratch buffer and returns headers + raw field pointers, then apply `StorageEngine::isVisible` in a tight loop.

**Pros:** much higher scan throughput.  
**Cons:** more invasive, must be careful with tuple lifetimes and page pinning.

**Spec decision:** implement **Option A in Phase 1**, design interfaces so Phase 2 can switch SeqScanOperator to Option B without changing other operators.

---

## 5) Security & RLS integration (ScratchBird-specific)

### 5.1 Permission enforcement

Before opening the plan:

- call `Executor::checkPermission(table_id, PermissionType::SELECT, &needed_columns)`.

“needed_columns” comes from:

- projected columns

- predicate columns

- RLS predicate columns (if enforced)

This call is a **hard gate**: no rows produced before it succeeds.

### 5.2 RLS for SELECT (spec-level, correctness-first)

Summary decision

- RLS for SELECT is a mandatory filter stage that runs after MGA visibility and before user WHERE.

- Phase 1 evaluates RLS row-by-row via a compatibility bridge that reuses existing policy machinery, while keeping the decode path vector-friendly.

- Phase 2 compiles RLS USING predicates into vector expression programs and evaluates them like other filters.

#### 5.2.1 Semantics and threat model

What SELECT RLS must do

- For each base table referenced by a query, if `shouldEnforceRLS(table_id)` is true, then:

- Determine the set of applicable policies for the current user/roles (`policyAppliesToUser`).

- For each MGA-visible row, evaluate the policy predicates and decide whether the row can be returned.

Policy combination rule (must be explicit)

- Permissive policies are OR'd together.

- Restrictive policies are AND'd together.

- Final allow = (OR over permissive, default FALSE if none) AND (AND over restrictive, default TRUE if none).

If ScratchBird's current policy model differs, encode that rule explicitly and test it.

Leakage constraints (security-critical)

- RLS must be applied before any user-visible output (including LIMIT short-circuiting).

- RLS must not be bypassed by predicate pushdown or index scan unless equivalence is proven.

- RLS "denies" should filter, not error (unless your dialect dictates errors).

#### 5.2.2 Pipeline placement (Scan vs Filter vs late materialization)

Required pipeline ordering for a single table

`SeqScan (MGA-visible rows) -> RLSFilter -> WHERE Filter -> Project -> Limit`

Why this order

- MGA must be enforced first because policies should not see rows not visible under MGA rules.

- RLS runs before WHERE to avoid timing/row-count leakage and to match standard semantics.

RLS as an operator

- RLS stays a distinct operator (RLSFilterOperator) in Phase 1 and Phase 2.

- Future index scans with late materialization must still apply RLS after values are fetched.

Plan builder requirement

- Compute `required_columns_for_rls(table)` separately from WHERE/projection columns.

#### 5.2.3 Policy representation in the vector engine

Existing entry points in `src/sblr/executor.cpp`

- `shouldEnforceRLS(table_id)`

- `policyAppliesToUser(policy_info)`

- `checkRLSPolicies(table_id, row_values, columns, policy_type, is_with_check)`

Proposed internal representation for vector plans

```
enum class PolicyMode { Permissive, Restrictive };

struct RLSPredicate {
  PolicyMode mode;
  ResolvedExpr expr;  // resolved expression or compiled program
};

struct RLSBundle {
  core::ID table_id;
  std::vector<RLSPredicate> permissive;
  std::vector<RLSPredicate> restrictive;
  std::vector<ColumnRef> required_columns;
};
```

ResolvedExpr is whatever your semantic analyzer produces today (or a thin wrapper). Do not store raw SQL text if you can avoid it.

#### 5.2.4 Batch evaluation strategy (Phase 1 bridge -> Phase 2 vector-native)

Phase 1 (correctness bridge)

- RLSFilterOperator receives a Batch with decoded vectors for `required_columns`.

- Evaluate policies row-by-row via a scalar evaluator that reads from vectors without constructing per-cell `TypedValue`.

Row view adapter (no per-cell TypedValue)

```
struct RowView {
  const Batch* batch;
  uint32_t row_index;  // physical index (after selection)
  bool is_null(uint32_t col) const;
  int64_t get_i64(uint32_t col) const;
  StringView get_string(uint32_t col) const;
  // other typed accessors as needed
};
```

Policy evaluation logic (explicit)

```
bool allow = true;
for (restrictive : restrictive_preds) {
  allow &= eval(restrictive.expr, row);
  if (!allow) return false;
}

if (!permissive_preds.empty()) {
  bool any = false;
  for (permissive : permissive_preds) {
    any |= eval(permissive.expr, row);
    if (any) break;
  }
  allow &= any;
}

return allow;
```

NULL handling

- Treat NULL predicate result as FALSE for row admission unless current ScratchBird semantics are different.

Phase 2 (vector-native)

- Compile each RLSPredicate to a vector expression program.

- Evaluate like WHERE: inputs are vectors + selection, output is refined selection.

#### 5.2.5 Permission checks and column-level privileges

Permission checks

- Call `Executor::checkPermission(table_id, SELECT, &columns)` before any rows are produced.

- `columns` must include projection, WHERE, and RLS columns.

Decision on RLS column privileges

- Safest default: include RLS columns in the permission check (strict but secure).

- If you later introduce policy owners/definers, you can allow internal RLS access without user column privileges, but it must be explicit and tested.

#### 5.2.6 File-by-file work items (RLS for SELECT)

Policy bundle resolution

- New: `src/vecexec/policy_resolver.cpp` (build RLSBundle from catalog).

- Factor `shouldEnforceRLS` and `policyAppliesToUser` into a shared helper (e.g., `src/security/rls.cpp`) so both executors share identical logic.

Expression resolution

- Resolve policy predicates using the same binder/type-checker used for WHERE.

- Store as `ResolvedExpr` with resolved column refs.

Scalar evaluator

- New: `src/vecexec/expr/scalar_eval.cpp` with `eval_bool(ResolvedExpr, RowView)`.

RLS operator

- New: `src/vecexec/operators/rls_filter.cpp`.

Plan builder integration

- In `src/vecexec/plan_builder.cpp`, wrap table scans with RLSFilterOperator when needed.

Fallback rules

- Unsupported policy features trigger vector fallback (auto) or explicit error (vector_select=on).

### 5.3 Ordering of security vs other filters

Pipeline must be:

`Scan (already MGA-visible) -> RLSFilter -> WHERE Filter -> Project -> Limit`

You *may* merge RLS + WHERE into one combined predicate later, but keep them separate initially to prevent unsafe pushdown assumptions.

---

## 6) Data decoding and expression evaluation

### 6.1 Tuple layout (ScratchBird)

- Tuple format is `TupleHeader + [null bitmap] + column data`, as used in `Executor::executeInsert` and `Executor::deserializeTuple` in `src/sblr/executor.cpp`.

- Tuple header is defined in `include/scratchbird/core/heap_page.h` (`TupleHeader::null_bitmap_offset`, `infomask`, etc).

- Varlen fields are length-prefixed: `uint32_t len + bytes` (VARCHAR, TEXT, CHAR, BINARY, VARBINARY, BLOB, BYTEA, JSON, JSONB, XML, VECTOR).

- Encrypted fields are stored as `uint32_t len + encrypted_record_bytes` (see `readEncryptedValue` in `src/sblr/executor.cpp`).

- Heap scans return pointers into pinned pages; vector decode must copy bytes into batch-owned buffers.

### 6.2 Existing decode path

- `Executor::deserializeTuple(...)` in `src/sblr/executor.cpp` produces `std::vector<TypedValue>` per row and is used for WHERE/projection today.

- `computeColumnLayout(...)` in `src/core/storage_engine.cpp` computes per-column offsets and sizes and is already used for index key extraction.

- `Executor::executeInsert` shows the canonical serialization for INT/REAL/DECIMAL/STRING/DATE/TIME/TIMESTAMP/BINARY types; vector decode must mirror this.

### 6.3 Vector decode goals (Phase 1)

- Decode only needed columns (RLS + WHERE + projection).

- Avoid per-cell `TypedValue` allocations; use typed vectors instead.

- Keep varlen bytes in batch-owned buffers (no dangling pointers to page memory).

- Preserve `deserializeTuple` semantics (deleted tuples, encryption handling, date/time formatting).

### 6.4 Vector types and batch layout

Minimum viable structures:

```
struct Vector {
  core::DataType type;
  void* data;                  // fixed-width array
  std::vector<uint8_t> nulls;  // 1 = null, 0 = not-null
};

struct VarlenVector {
  core::DataType type;
  std::vector<uint32_t> offsets;
  std::vector<uint32_t> lengths;
  std::vector<uint8_t> data;   // batch-owned bytes
  std::vector<uint8_t> nulls;
};
```

Phase 1 can keep a simple union/variant around these; avoid per-value heap allocation.

### 6.5 Decoder algorithm (Phase 1)

- Fetch tuples via `StorageEngine::createScan` + `HeapScanIterator::next`.

- For each tuple:

  - Validate header and skip `TupleHeader::isDeleted()` (match `deserializeTuple` behavior).

  - Determine column offsets/sizes:

    - Option A: refactor `computeColumnLayout` into a shared helper (e.g., `include/scratchbird/core/tuple_layout.h` + `src/core/tuple_layout.cpp`).

    - Option B: copy that logic into `src/vecexec/tuple_decoder.cpp` for Phase 1 (documented duplication).

  - For each needed column:

    - If null bit set: mark null in vector.

    - Else decode by type:

      - Fixed-width: `memcpy` into vector slot.

      - Varlen: read `len`, copy bytes into `VarlenVector::data`, store offset/len.

      - DATE/TIME/TIMESTAMP: match `Executor::deserializeTuple` output semantics. Option A: store native ints in vectors and convert during projection. Option B: convert to string and store in varlen vector (exact row-path parity).

      - Encrypted: read encrypted record, decrypt via `DomainManager::decryptValue`, and store the decrypted value in the vector (typed). Creating a temporary `TypedValue` is acceptable for encrypted columns only.

### 6.6 Shared encryption helpers

- `getColumnEncryptionState`, `readEncryptedValue`, and `decryptValueForColumn` currently live in `src/sblr/executor.cpp`.

- Move these to a shared module (e.g., `src/core/column_encryption.cpp` or `src/security/column_encryption.cpp`) so vector decoder can reuse them without depending on the row executor.

### 6.7 Expression engine integration

Phase 1:

- Interpreted vector kernels for comparisons/boolean ops/arithmetic.

- Scalar fallback allowed for string ops not yet vectorized.

- Refine the selection vector in-place.

Constant folding:

- Reuse existing folding logic in `src/sblr/bytecode_generator_v2.cpp`, but emit vector expressions.

---

## 7) Operator set (Phase 1 and beyond)

### Phase 1 operators (must-have)

- `SeqScanOperator` (wrapper around `StorageEngine::createScan` / iterator)

- `RLSFilterOperator` (bridge implementation ok)

- `FilterOperator` (WHERE)

- `ProjectOperator`

- `LimitOperator`

### Phase 2–3 (core DB features)

- `IndexScanOperator` + `FetchOperator` (late materialization)

- `HashJoinOperator` (inner + left)

- `NestedLoopJoinOperator` (fallback)

### Phase 4+

- `HashAggregateOperator`

- `SortOperator` + external spill

- `TopNOperator` optimization

### 7.1 Selection vector semantics

- `Batch.count` is the physical row count in vectors.

- If `Batch.sel.active` is false, logical rows are `0..count-1`.

- If `Batch.sel.active` is true, only `sel.idx[0..sel.count-1]` are visible.

- Filters update only the selection vector; they do not reorder or compact data.

- Projections preserve selection unless they explicitly compact.

### 7.2 SeqScanOperator (iterator-backed) pseudocode

Implementation target: `src/vecexec/operators/seq_scan.{h,cpp}`

```
NextStatus SeqScanOperator::next_batch(ExecContext& ctx, Batch& out) {
  out.reset();
  while (out.count < batch_size_) {
    core::Tuple tuple;
    auto st = scan_->next(&tuple, nullptr);  // MGA visibility already applied
    if (st == Status::NOT_FOUND) break;
    if (st != Status::OK) return NextStatus::ERROR;

    // Match row executor semantics
    auto* hdr = reinterpret_cast<const core::TupleHeader*>(tuple.data);
    if (hdr->isDeleted()) {
      continue;
    }

    if (!decoder_.decode(tuple, needed_columns_, out)) {
      return NextStatus::ERROR;
    }
  }

  if (out.count == 0) {
    return NextStatus::END;
  }

  out.sel.reset_identity(out.count);
  return NextStatus::OK;
}
```

### 7.3 FilterOperator pseudocode (WHERE and RLS)

Implementation target: `src/vecexec/operators/filter.{h,cpp}` and `src/vecexec/operators/rls_filter.{h,cpp}`

```
NextStatus FilterOperator::next_batch(ExecContext& ctx, Batch& out) {
  Batch in;
  auto st = child_->next_batch(ctx, in);
  if (st != NextStatus::OK) return st;

  Selection sel = in.sel.active ? in.sel : Selection::identity(in.count);
  uint32_t w = 0;
  for (uint32_t i = 0; i < sel.count; ++i) {
    uint32_t ridx = sel.idx[i];
    if (predicate_.eval(in, ridx)) {
      sel.idx[w++] = ridx;
    }
  }
  sel.count = w;
  sel.active = true;
  in.sel = sel;

  out = std::move(in);
  return NextStatus::OK;
}
```

### 7.4 ProjectOperator pseudocode (selection-preserving)

Implementation target: `src/vecexec/operators/project.{h,cpp}`

```
NextStatus ProjectOperator::next_batch(ExecContext& ctx, Batch& out) {
  Batch in;
  auto st = child_->next_batch(ctx, in);
  if (st != NextStatus::OK) return st;

  out.count = in.count;
  out.sel = in.sel.active ? in.sel : Selection::identity(in.count);
  out.cols.clear();
  out.cols.reserve(project_exprs_.size());

  for (const auto& expr : project_exprs_) {
    if (expr.is_column_ref()) {
      out.cols.push_back(in.cols[expr.column_slot()]);
      continue;
    }
    out.cols.push_back(eval_project_expr(expr, in, out.sel));
  }

  return NextStatus::OK;
}
```

Phase 1 scope:

- Column refs and constants are supported in projection.

- Complex expressions use scalar fallback over selection (vector-native kernels later).

---

## 8) Parallelism (designed-in, phased)

Even if Phase 1 is single-threaded, define plan nodes now for:

- `Exchange(Gather)`

- `Exchange(DistributeHash(keys))`

- `Exchange(Broadcast)`

Phase 3:

- introduce a `TaskScheduler`

- introduce scan morsels (page ranges)

- parallelize scan and independent pipelines first

Critical: your MGA visibility uses `current_xid` from engine/connection; this is read-only for SELECT, so parallel scan is safe as long as transaction context is shared read-only and storage iterators are thread-safe (or you build per-thread iterators).

---

## 9) Cost model additions (ScratchBird alignment)

Your optimizer code already has placeholders for parallel costs. Extend cost model to include:

- “vector CPU per tuple” constants for each operator type

- batch overhead constants

- index scan random IO penalty (even if approximate)

- join build/probe costs

- memory footprint estimates for hash join/agg/sort

Initially, planner can be heuristic:

- if sargable predicate exists and selectivity below threshold → index scan

- if equality join and estimated rows above threshold → hash join

- else nested loop

---

## 10) Integration plan (exact edits)

### 10.1 Add new vector SELECT entry point

In `src/sblr/executor.cpp`:

- Add `Executor::executeSelectVectorized(...)`

- Add dispatch in `executeSelect(...)`:

Pseudo-structure:

1. build semantic query object (already)

2. compute needed columns

3. `checkPermission`

4. if `vector_select` enabled and query supported:
   
   - build PhysicalPlan
   
   - run `VectorExecutor`
   
   - return rows to client

5. else fallback old path

### 10.2 Plan builder

Create `src/vecexec/plan_builder.cpp`

- Input: semantic AST (from Semantic Analyzer V2 output)

- Output: operator tree

Supported query subset phase 1:

- single-table SELECT

- simple WHERE

- projection expressions

- limit/offset (offset optional)

---

## 11) Observability

Add per-operator metrics:

- input rows, output rows

- filter selectivity

- time in open/next/close

- decoded bytes and varlen bytes copied

Hook into existing `src/optimizer/query_profiler.cpp` concepts by adding a new sink.

---

## 12) Compatibility & error behavior

- If any unsupported function/type/operator encountered:
  
  - return “unsupported in vector engine” internally and fallback (auto mode), or error if `vector_select=on`.

- Permission errors must occur before any row output.

- RLS failures must filter rows silently (standard RLS semantics), not error, unless your dialect does otherwise.

---

## 13) Migration strategy (feature flags and staged rollout)

- `vector_select=off` default for Alpha

- `vector_select=auto` for dev builds after Milestone 2

- Add query-shape detection:
  
  - only enable for safe subset

- Keep EXPLAIN showing whether vector path chosen

---

## 14) Testing (ScratchBird-specific)

### 14.1 Differential testing harness

Add a test mode that runs each query twice:

- old executor

- vector executor  
  Compare:

- bag-equality (unless ORDER BY)

- exact ordering with ORDER BY  
  Also compare error codes/messages for permission denials.

### 14.2 MGA visibility tests

Specifically test:

- tuples with xmin/xmax patterns around current_xid

- concurrent transactions if supported

- isolation level variations (since `isVisible` depends on it)

### 14.3 RLS tests

- policies applying to user vs not

- USING predicates with NULLs and complex expressions

- ensure vector selection matches row semantics

---

# Key decisions (resolved)

1. **Phase 1 scan uses wrapper over `StorageEngine::createScan` + `HeapScanIterator::next`**  
   Rationale: MGA correctness is already embedded; minimal risk.

2. **Columnar batches + selection vectors**  
   Rationale: best long-term for vector execution; works fine even with row-store.

3. **RLS enforced as explicit operator; bridge implementation can call existing per-row helpers initially**  
   Rationale: correctness first; later compile policies to vector expressions.

4. **Plan-driven executor (new physical plan), not vector bytecode**  
   Rationale: avoids VM overhead, aligns with future optimizer and parallelism.

---

# Phased engineering plan with acceptance criteria

## Milestone 1: Vector runtime + single-table SELECT

**Implement**

- Batch/Vector/Selection

- SeqScan wrapper (iterator-backed)

- Filter, Project, Limit

- Minimal expression engine (ints/bools/strings basic)

**Accept**

- Differential tests pass for simple selects

- No MGA/security regressions

## Milestone 2: Security correctness for SELECT

**Implement**

- Permission precheck (`Executor::checkPermission`)

- RLSFilterOperator (bridge calling existing policy evaluator row-by-row)

- Needed-column extraction includes RLS + WHERE + projection

**Accept**

- RLS/permission test matrix passes

- No rows emitted before permission approval

## Milestone 3: Performance upgrades

**Implement**

- Avoid per-row `Value` materialization where possible (typed decode into vectors)

- Batch buffer reuse pool

**Accept**

- Scan+filter benchmark shows clear CPU reduction vs row executor

## Milestone 4+: Index scans, joins, agg, sort

(As previously outlined.)



# Updates

# 
