### Phase 7 — Constraints, Referential Integrity (RI), and Triggers: Detailed Plan and Reasoning

This document details the design, rationale, and step-by-step implementation plan for Phase 7, covering data integrity constraints (CHECK, NOT NULL, UNIQUE, PRIMARY KEY, FOREIGN KEY), deferrability (DEFERRABLE/INITIALLY), transaction-scoped constraint modes (SET CONSTRAINTS), and trigger infrastructure (row/statement, BEFORE/AFTER). It builds on the Current state (high-level) and integrates with Phases 1–6 already implemented.

---

### 1) Scope and objectives

- Implement full lifecycle and enforcement for:
  - CHECK constraints (row-level predicate on a single table)
  - NOT NULL (column-level)
  - UNIQUE constraints
  - PRIMARY KEY (UNIQUE + NOT NULL + identity semantics)
  - FOREIGN KEY with referential actions (NO ACTION, RESTRICT, CASCADE, SET NULL, SET DEFAULT)
- Support deferrability:
  - DEFERRABLE/NOT DEFERRABLE
  - INITIALLY IMMEDIATE/DEFERRED
  - SET CONSTRAINTS (ALL | name [, ...]) { IMMEDIATE | DEFERRED }
- Trigger framework:
  - Row-level and statement-level triggers
  - BEFORE/AFTER timing
  - Transition tables (NEW/OLD for row; INSERTED/DELETED sets for statement-level)
  - Trigger order and determinism
- Catalog persistence and metadata:
  - Extend `SDB$CONSTRAINT`, `SDB$CONSTRAINT_KEY`, `SDB$TRIGGER`, `SDB$TRIGGER_DEP`, `SDB$TRIGGER_ORDER` (or reuse existing shells) with minimal normalized schemas
- Integration:
  - Parser/DDL already supports surface keywords from earlier phases; executor and catalog integration required
  - Optimizer/executor checks: fast paths for NOT NULL, CHECK pushdown where possible; UNIQUE/PK via index enforcement
  - Transaction semantics for deferred constraints and SET CONSTRAINTS

Exit criteria:
- Complete constraints lifecycle, deferrability semantics, and trigger execution aligned with standard behavior; regression tests cover correctness including boundary cases and concurrency interactions.

---

### 2) Design principles and reasoning

- Canonical representation
  - PRIMARY KEY implemented as UNIQUE + NOT NULL set across key columns
  - UNIQUE/PK enforcement via indexes; validate definitions guarantee backing unique index exists
  - FOREIGN KEY requires referenced side to have a unique index on referenced columns (PK or UNIQUE)
  - CHECK stored as normalized expression text; initial evaluation via interpreter; future: compiled SBR for hot paths

- Deferrability model
  - Immediate constraints evaluated during statement execution; failure aborts statement
  - Deferred constraints recorded as pending validations within the transaction; evaluated at
    1) `SET CONSTRAINTS ... IMMEDIATE`, and
    2) `COMMIT`
  - FK NO ACTION (immediate by default) vs DEFERRABLE NO ACTION (deferred), plus referential actions that may fire immediately or be deferred depending on definition

- Triggers
  - BEFORE triggers can mutate NEW values (row-level) or set statement context; may veto by raising exceptions
  - AFTER triggers observe post-change state and can enqueue side effects
  - Statement-level triggers receive transition sets INSERTED/DELETED; row-level triggers receive single NEW/OLD records
  - Trigger order: stable ordering per timing and granularity; user-defined ordering supported via ordinal; catalog persists order
  - Re-entrancy and recursion: prevent infinite loops via depth counter and guard; document semantics

- Concurrency and consistency
  - UNIQUE/PK collisions resolved via index insert logic and transaction visibility rules (MGA). For immediate uniqueness, check during index insert; for deferred uniqueness (rare), collect candidates and validate at deferral points
  - FK checks: target existence and referential action application must see a consistent snapshot. For deferred FK, record pairs (child RowID, referenced key) into a per-txn set and validate/apply at deferral points
  - Deadlock avoidance: maintain ordering for constraint locks; avoid long-lived locks by deferring where configured

- Performance
  - Favor index-assisted enforcement (UNIQUE/PK/FK) over table scans
  - Pushdown CHECK where sargable to scans (optional, later); baseline: evaluate per-row touched
  - Batch-deferred validations at COMMIT to reduce repeated probes

---

### 3) Catalog model and DDL mapping

- `SDB$CONSTRAINT`
  - `oid (UUID)`
  - `schema_oid (UUID)`
  - `relation_oid (UUID)`
  - `name (string)`
  - `type (CHECK | NOT_NULL | UNIQUE | PRIMARY_KEY | FOREIGN_KEY)`
  - `deferrable (bool)`
  - `initially_deferred (bool)`
  - `check_expr (string, nullable)`
  - `fk_target_relation_oid (UUID, nullable)`
  - `fk_match_type (SIMPLE | FULL)` (optional for parity)
  - `action_update (NO ACTION|RESTRICT|CASCADE|SET NULL|SET DEFAULT)`
  - `action_delete (...)` as above

- `SDB$CONSTRAINT_KEY`
  - `constraint_oid (UUID)`
  - `position (int)`
  - `column_name (string, nullable when expression)`
  - `expression (string, nullable)`
  - `direction (ASC|DESC|NULL)`

- `SDB$TRIGGER`
  - `oid (UUID)`
  - `schema_oid (UUID)`
  - `relation_oid (UUID)`
  - `name (string)`
  - `enabled (bool)`
  - `timing (BEFORE|AFTER)`
  - `level (ROW|STATEMENT)`
  - `event (INSERT|UPDATE|DELETE|TRUNCATE)`
  - `order_ordinal (int)`
  - `body_source_oid (UUID)` -> text in `SDB$SOURCE`

- DDL
  - `ALTER TABLE ... ADD [CONSTRAINT name] PRIMARY KEY (col, ...) [DEFERRABLE|NOT DEFERRABLE] [INITIALLY DEFERRED|IMMEDIATE]`
  - `... UNIQUE (col, ...)`, `... FOREIGN KEY (col, ...) REFERENCES schema.table (col, ...) ON UPDATE ... ON DELETE ... [MATCH FULL]`
  - `... CHECK (predicate)`
  - `... ALTER CONSTRAINT ... [ENABLE|DISABLE]`
  - `SET CONSTRAINTS { ALL | name [, ...] } IMMEDIATE|DEFERRED`
  - `CREATE TRIGGER name BEFORE|AFTER INSERT|UPDATE|DELETE ON table [FOR EACH ROW] ...`

---

### 4) Execution model and enforcement

- NOT NULL
  - Enforced in tuple write path; variant for INSERT/UPDATE assignment; deferrable NOT NULL is uncommon; scope: implement as immediate only

- UNIQUE/PRIMARY KEY
  - Backing unique index required; if absent, auto-create a named system index (e.g., `sys_uniq_<oid>`) with `INCLUDE` payload as needed
  - Insert/update path attempts index insert; if conflict with visible row (per snapshot rules), error (immediate)
  - For DEFERRABLE UNIQUE/PK (optional, limited): record key to txn set; skip immediate check; on COMMIT/SET CONSTRAINTS IMMEDIATE, validate via probe and fail if duplicates exist

- CHECK
  - Evaluate predicate at row write time; deferrable: record failure candidates (rowids) and re-evaluate at deferral boundary using final values

- FOREIGN KEY
  - Definition requires referenced UNIQUE/PK
  - Immediate: on child write, probe referenced key via index; fail if missing
  - Deferred: record (child rid, key) pairs in txn set; at deferral boundary, re-probe; apply actions for DELETE/UPDATE on parent:
    - CASCADE: enqueue child updates/deletes; perform with safeguards against cycles
    - SET NULL/DEFAULT: enqueue modifications for child columns
    - RESTRICT/NO ACTION: immediate failure if violation detected

- Triggers
  - Execution cascade per statement:
    1) BEFORE STATEMENT triggers
    2) For each affected row: BEFORE ROW, write, AFTER ROW
    3) AFTER STATEMENT triggers
  - Transition data: provide NEW/OLD at row-level; capture INSERTED/DELETED sets for statement-level in memory (bounded; spill to temp if needed, future phase)
  - Error propagation: trigger exceptions abort statement; rethrow diagnostics

- SET CONSTRAINTS
  - Per-transaction state of deferrable constraints; default from `initially_deferred`
  - Alters validation schedule; immediate transition triggers evaluation of pending sets

---

### 5) Parser and catalog integration

- Parser: already supports modern features (SET CONSTRAINTS, DEFERRABLE/INITIALLY); verify acceptance end-to-end
- CatalogManager:
  - Add CRUD for constraints/keys/triggers; ensure `SDB$SOURCE` is used to store trigger body definitions
  - Validate FK definitions against existing indexes
  - Auto-create backing indexes when needed (UNIQUE/PK)

---

### 6) Planner/optimizer considerations

- NOT NULL: annotate columns as non-nullable for selectivity improvements
- CHECK: optional pushdown when predicate structurally matches CHECK; baseline: executor-level validation
- FK: optional join selectivity hints (later); baseline: no change
- Triggers: no planner involvement; handled by executor write-path orchestration

---

### 7) Storage and index interactions

- Reuse B-Tree V1 for unique index enforcement; ensure index build pipeline sets unique flag
- For cascading actions, batch child modifications per parent to limit random IO; future: multi-statement grouping

---

### 8) Concurrency, isolation, and error handling

- MGA ensures readers see consistent versions; uniqueness relies on index + snapshot rules
- Document conflict/error codes: duplicate key, check violation, FK violation (update/delete/insert), trigger exception
- Deadlocks: use existing deadlock detection; ensure cascades acquire locks in a consistent order (parent before child)

---

### 9) Telemetry and observability

- Counters: constraints validated, deferred validations run, triggers fired, cascaded ops, violations
- EXPLAIN/EXPLAIN ANALYZE (INSERT/UPDATE/DELETE later phase): show constraint/trigger hooks fired

---

### 10) Test plan (incremental)

- Unit tests per constraint type:
  - NOT NULL: insert/update violating
  - UNIQUE/PK: duplicates across transactions; visibility conflicts
  - CHECK: arithmetic/predicate cases; null handling
  - FK:
    - Immediate: insert child without parent -> fail; parent delete behaviors per action (NO ACTION, RESTRICT, CASCADE, SET NULL/DEFAULT)
    - Deferred: set deferrable; batch insert child then parent; enforce at COMMIT and on `SET CONSTRAINTS ... IMMEDIATE`
  - DEFERRABLE/INITIALLY: initial modes, toggling via SET CONSTRAINTS
- Triggers:
  - BEFORE/AFTER, ROW/STATEMENT permutations
  - NEW/OLD semantics for INSERT/UPDATE/DELETE
  - Transition tables for statement-level
  - Firing order determinism; disable/enable
- Concurrency:
  - Two concurrent writers violating uniqueness; detection and error
  - FK parent delete racing with child insert under deferred mode

---

### 11) Phased implementation breakdown

Phase 7.A — Catalog and DDL wiring
- Extend catalogs (`SDB$CONSTRAINT`, `SDB$CONSTRAINT_KEY`, `SDB$TRIGGER`)
- CatalogManager CRUD for constraints/keys/triggers
- Validate FK target uniqueness; auto-create unique indexes for UNIQUE/PK when missing
- DDL executor: `ALTER TABLE ... ADD CONSTRAINT`, `DROP CONSTRAINT`, `CREATE TRIGGER`, `ALTER TRIGGER` (enable/disable)

Phase 7.B — NOT NULL, CHECK enforcement (immediate)
- Tuple write-path checks
- Record deferrable CHECK support in txn pending set; evaluate on boundary

Phase 7.C — UNIQUE/PK enforcement
- Integrate unique index insert checks; error mapping
- Optional: limited deferrable uniqueness via pending set + commit validation

Phase 7.D — FOREIGN KEY (immediate)
- Insert/update child -> probe parent via index
- Parent delete/update -> enforce action (NO ACTION, RESTRICT, CASCADE, SET NULL/DEFAULT)

Phase 7.E — Foreign keys (deferred and SET CONSTRAINTS)
- Per-txn FK pending set; `SET CONSTRAINTS` immediate evaluation
- Commit-time validation order topologically per dependency graph

Phase 7.F — Trigger execution engine
- Trigger registry; execution orchestration for BEFORE/AFTER, ROW/STATEMENT
- Row-level context (NEW/OLD) and statement-level transition sets
- Order control and enabling/disabling

Phase 7.G — Concurrency and edge cases
- Conflict/rollback paths
- Cycle prevention in cascades; depth-bounding + error on cycle detection

Phase 7.H — Telemetry and docs
- Counters and isql SHOW surfaces (SHOW CONSTRAINTS/TRIGGERS)
- Developer/operator docs for semantics

---

### 12) Risk assessment and mitigations

- Deferrable constraints complexity: start with FK deferrable; uniqueness deferrable limited or deferred to later minor if time-constrained
- Cascading actions explosion: batch and depth-limit; clear diagnostics on truncation
- Trigger recursion/ordering: enforce maximum depth and deterministic ordering

---

### 13) Deliverables

- Working constraint engine with deferrability and proper error codes
- Trigger subsystem covering BEFORE/AFTER, ROW/STATEMENT with transition data
- isql: SHOW CONSTRAINTS/TRIGGERS; SET CONSTRAINTS support
- Comprehensive tests and CI coverage; no regressions in Phases 1–6

---

### 14) Implementation notes (integration hooks)

- Executor write-path wrappers: centralize per-statement pre/post hooks to call BEFORE/AFTER STATEMENT triggers and manage transition sets
- Transaction object extensions: pending sets for deferred CHECK/FK/UNIQUE (if enabled); `set_constraints_state`
- Catalog-driven attach: load constraint/trigger metadata on relation open; cache with invalidation on DDL
