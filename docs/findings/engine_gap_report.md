# ScratchBird Engine Gaps and Stubs (Code Review Findings)

Scope: Code inspection of `src/` and `include/` with focus on stub/partial implementations and core database features that appear missing or unclear. Findings reference concrete code locations; comments were not trusted as proof of completion.

## 1) Storage, MVCC, and GC gaps

1. **Columnstore index is largely stubbed (no durable storage, no GC)**
   - **Where**: `src/core/columnstore_index.cpp#L339` and `src/core/columnstore_index.cpp#L352`
   - **Observed**: `vacuum()` and `removeDeadEntries()` return `Status::OK` without doing any work; comments describe future behavior only. Columnstore segments are tracked in-memory; writes to disk are “in production” comments, not implemented.
   - **Impact**: Columnstore indexes will leak dead rows, never compact, and lose data across restarts (catalog is in-memory).
   - **Expected**: Persist segment catalog, implement segment read/write, implement MGA-based garbage collection (dead TIDs), and vacuum/compaction.

2. **Table page enumeration is imprecise (no table_id on heap pages)**
   - **Where**: `src/core/catalog_manager.cpp#L5818`
   - **Observed**: `enumerateTablePages()` returns *all* heap pages in a tablespace because `PageHeader` lacks `table_id` and the code explicitly cannot filter by table.
   - **Impact**: Operations using this enumeration (table migration, vacuum, table-level tasks) may operate on unrelated tables if multiple tables share a tablespace.
   - **Expected**: Add `table_id` to heap page metadata or an index from table -> page list; update enumeration to use that.

3. **Offline table migration advertises stub-only behavior but performs partial work**
   - **Where**: `src/core/catalog_manager.cpp#L6420` onward
   - **Observed**: The function logs “STUB IMPLEMENTATION: Only updating catalog metadata” but then executes a full page copy loop with TID remapping. This is contradictory and indicates incomplete or uncertain behavior. Also, ONLINE migration returns NOT_IMPLEMENTED.
   - **Impact**: Confusing behavior for operators; may give false sense of completion and could be unsafe if other parts (index TID updates, metadata updates, rollback) are not fully consistent.
   - **Expected**: Decide intended behavior (stub or full migration) and align code + logs. Implement ONLINE mode or explicitly disable with clear error. Verify copy path correctness and transactional rollback.

4. **Index TID updates missing for most index types during migration**
   - **Where**: `src/core/catalog_manager.cpp#L6292` to `src/core/catalog_manager.cpp#L6394`
   - **Observed**: TID updates are not implemented for VECTOR/HNSW, FULLTEXT, GIN, GIST, BRIN, RTREE; code logs warnings and recommends DROP+RECREATE.
   - **Impact**: Any table migration invalidates these indexes.
   - **Expected**: Implement per-index traversal and TID remapping. For example: GIN requires key tree + posting list update; GIST needs leaf traversal and bounding box recompute; HNSW needs graph edge updates.

5. **Garbage collector does not support many index types**
   - **Where**: `src/core/garbage_collector.cpp#L900` to `src/core/garbage_collector.cpp#L921`
   - **Observed**: GC only opens BTREE, HASH, GIN, BRIN, VECTOR (HNSW). All other index types are reported as “GC not implemented”.
   - **Impact**: Dead tuples in other index types are never removed, leading to index bloat and possible incorrect query results if stale pointers remain.
   - **Expected**: Add GC support for FULLTEXT, GIST, RTREE, SPGIST, BITMAP, COLUMNSTORE, LSM, etc., each calling a working `removeDeadEntries()`.

6. **R-Tree physical delete/condense is stubbed**
   - **Where**: `src/core/rtree.cpp#L904`
   - **Observed**: `condenseTree()` is a stub and does logical deletion only, logging a warning.
   - **Impact**: R-tree will not rebalance or physically remove entries, causing growth and degraded query performance.
   - **Expected**: Implement full condense algorithm or document + implement a GC pass that performs physical cleanup.

## 2) Constraint deferral and SQL correctness gaps

7. **Deferred constraint validation is a placeholder**
   - **Where**: `src/core/connection_context.cpp#L1011` to `src/core/connection_context.cpp#L1112`
   - **Observed**: Deferred checks are stored, but `validateDeferredConstraints()` always returns OK without re-checking. Initialization of deferred constraint state is also placeholder.
   - **Impact**: DEFERRABLE constraints effectively never validate at COMMIT; integrity guarantees are broken.
   - **Expected**: Implement catalog lookups and re-evaluation of deferred constraints on COMMIT; track deferral state per constraint.

8. **Deferrable flags are discarded when generating bytecode**
   - **Where**: `src/sblr/bytecode_generator_v2.cpp#L475`
   - **Observed**: FK deferrable flags are always written as 0, even though parser/semantic layer can represent deferrable constraints.
   - **Impact**: DEFERRABLE/INITIALLY DEFERRED metadata cannot reach executor/catalog, making deferrable constraints effectively unsupported.
   - **Expected**: Propagate actual deferrable flags from AST/semantic layer into bytecode and catalog.

9. **Group-by validation is missing**
   - **Where**: `src/sblr/semantic_analyzer_v2.cpp#L2796`
   - **Observed**: `validateGroupBy()` is a TODO that always returns true.
   - **Impact**: Queries with non-aggregated columns not in GROUP BY will pass semantic analysis, potentially producing undefined results.
   - **Expected**: Implement standard SQL GROUP BY validation.

10. **ALTER TABLE actions are incomplete**
   - **Where**: `src/sblr/executor.cpp#L3560`
   - **Observed**: Executor throws “ALTER TABLE action not implemented” for action codes outside a small subset.
   - **Impact**: DDL coverage is limited even if parser accepts more actions.
   - **Expected**: Implement missing ALTER TABLE actions (drop/rename/constraint modifications, etc.) and keep parser/executor aligned.

## 3) Catalog and metadata gaps

11. **SHOW commands return stub data for many object types**
   - **Where**: `src/sblr/executor.cpp#L23328` to `src/sblr/executor.cpp#L23729`
   - **Observed**: SHOW TRIGGER/PROCEDURE/FUNCTION/DOMAIN/COMMENTS/DEPENDENCIES/PACKAGE (and GRANTS/CHECKS) produce placeholder rows; they do not query real catalog data.
   - **Impact**: Metadata reporting is incomplete, making inspection/administration unreliable.
   - **Expected**: Implement catalog tables for these object types and query them in SHOW handlers.

12. **Character set and collation persistence is deferred**
   - **Where**: `src/core/charset_loader.cpp#L11` to `src/core/charset_loader.cpp#L113`
   - **Observed**: Charset/collation validation occurs, but catalog insertion is commented out; loaders return OK without persistence.
   - **Impact**: Charset/collation definitions are not stored, so they may disappear across restarts and cannot be queried.
   - **Expected**: Implement catalog tables and `insertCharset`/`insertCollation` methods in CatalogManager, then persist.

13. **Timezone catalog management is missing**
   - **Where**: `src/core/timezone_loader.cpp#L290` to `src/core/timezone_loader.cpp#L305`
   - **Observed**: `clearAllTimezones()` and `getLoadedTimezoneStats()` return NOT_IMPLEMENTED.
   - **Impact**: No way to reset or inspect timezone catalog state.
   - **Expected**: Implement these APIs in CatalogManager and wire through loader.

14. **View WITH CHECK OPTION not enforced**
   - **Where**: `src/security/view_security.cpp#L233` to `src/security/view_security.cpp#L250`
   - **Observed**: `validateCheckOption()` returns OK even when check_option is set.
   - **Impact**: Invalid rows can be inserted/updated through views, violating view semantics.
   - **Expected**: Evaluate view predicates against new row values for LOCAL/CASCADED checks.

## 4) Protocol / client access stubs (engine integration)

15. **Native/PostgreSQL/MySQL protocol adapters have unimplemented behavior**
   - **Where**:
     - `src/protocol/adapters/native_adapter.cpp#L246` (auth), `#L271` (cancel), `#L363` (describe)
     - `src/protocol/adapters/postgresql_adapter.cpp#L435` (cancel), `#L956` to `#L966` (COPY IN), `#L1957` (MD5 hash)
     - `src/protocol/adapters/mysql_adapter.cpp#L617` (password validation), `#L714` (database existence)
   - **Observed**: Authentication and protocol features are placeholders.
   - **Impact**: Wire protocol behavior is incomplete and may accept invalid clients or fail to handle core operations.
   - **Expected**: Implement full authentication, cancellation, COPY, and statement description paths.

16. **ODBC driver is mostly a stub**
   - **Where**: `src/odbc/odbc_handles.cpp#L930` to `src/odbc/odbc_handles.cpp#L1075`
   - **Observed**: Connection, execution, prepared statements, and cancel are TODO; catalog queries are stubs.
   - **Impact**: ODBC clients cannot actually use the engine.
   - **Expected**: Implement ODBC wire calls to native protocol and catalog querying.

17. **Connection pool is stubbed**
   - **Where**: `src/pool/connection_pool.cpp#L158` to `src/pool/connection_pool.cpp#L245`
   - **Observed**: Actual connection logic, execution, validation, and reset are TODO.
   - **Impact**: Pooling layer is non-functional.
   - **Expected**: Implement connection lifecycle, reset, and validation with real protocol calls.

18. **Security/auth providers are stubbed**
   - **Where**:
     - `src/security/ldap_auth.cpp#L15`
     - `src/security/kerberos_auth.cpp#L15` and `src/security/kerberos_auth.cpp#L225`/`#L573`
     - `src/security/saml_auth.cpp#L21` and `src/security/saml_auth.cpp#L635`
     - `src/core/auth_provider.cpp#L254` to `src/core/auth_provider.cpp#L337`
   - **Observed**: LDAP/AD/Kerberos/SAML are placeholders that always succeed or return “not implemented”.
   - **Impact**: Security features are non-functional or misleading.
   - **Expected**: Implement full auth flows or disable features explicitly.

## 5) Core engine features expected but not present (based on specs and code)

19. **WAL / recovery subsystem is absent (note: optional, not used for MGA transactions)**
   - **Where**: `src/core/database.cpp#L358` (wal_level = 0) and no WAL writer/reader implementation in `src/core/`.
   - **Observed**: No WAL modules or checkpoint/replay logic are present. This aligns with the Firebird MGA model for transaction handling; WAL is optional and not required for core transaction semantics.
   - **Impact**: WAL-based features (streaming replication, PITR, external durability guarantees) are unavailable. Crash recovery remains reliant on MGA/TIP per `docs/specifications/FIREBIRD_TRANSACTION_MODEL_SPEC.md`.
   - **Expected**: If WAL is planned for replication/PITR only, treat this as a missing *optional* subsystem and keep it out of core transaction-critical paths.

20. **Dependency and comment tracking infrastructure is missing**
   - **Where**: `src/sblr/executor.cpp#L23664` to `src/sblr/executor.cpp#L23729`
   - **Observed**: SHOW COMMENTS/DEPENDENCIES return stub rows; no catalog tables are used.
   - **Impact**: DDL tools cannot track dependencies or comments, which are basic metadata in DB engines.
   - **Expected**: Add catalog tables for comments and dependency graph, update DDL to populate them, and expose via SHOW.

21. **Query compilation cache is not implemented**
   - **Where**: `src/sblr/query_compiler_v2.cpp#L37` to `src/sblr/query_compiler_v2.cpp#L50`
   - **Observed**: Cache checks mention a bytecode cache but skip it.
   - **Impact**: Repeated queries always compile; no plan/bytecode cache.
   - **Expected**: Add a bytecode cache keyed by SQL + schema version or invalidation rules.

## 6) Notes on unclear or conflicting behavior

22. **Table migration function logs claim stub-only behavior while executing copy**
   - **Where**: `src/core/catalog_manager.cpp#L6604` to `src/core/catalog_manager.cpp#L6665`
   - **Observed**: Logs say no page copying, but the function proceeds to allocate and copy pages with TID remapping later in the same function.
   - **Impact**: Misleading logs and confusion about actual behavior; potential partial migration issues if index updates or metadata updates are not complete.
   - **Expected**: Reconcile log messaging with actual behavior and review end-to-end migration for correctness (catalog update, index remap, rollback, TOAST pointer updates).

---

## 7) Parser, FDW, Tests, and Compatibility Gaps (multi-dialect model)

Note on design: each parser is standalone and emits SBLR bytecode. The ScratchBird (V2) parser is the superset dialect; Firebird/MySQL/PostgreSQL parsers only need to support their native feature sets and map into ScratchBird native commands or emulate catalog structures. The gaps below are framed in that context.

23. **V2/ScratchBird parser missing CREATE/ALTER coverage**
   - **Where**: `src/parser/parser_v2.cpp#L282`, `src/parser/parser_v2.cpp#L945`
   - **Observed**: TODOs for additional CREATE object types and ALTER INDEX/VIEW/SEQUENCE, etc.
   - **Impact**: ScratchBird superset dialect lacks DDL coverage expected by core engine features.
   - **Expected**: Implement missing CREATE/ALTER paths in V2 parser to keep the superset dialect complete.

24. **V2 procedural/PSQL visitor stubs**
   - **Where**: `src/parser/ast.cpp#L1478`
   - **Observed**: Procedural language statement visitors are stub implementations.
   - **Impact**: Even if parsed, procedural constructs may not be lowered to bytecode correctly.
   - **Expected**: Implement full visitors for procedural statements used by ScratchBird/Firebird-compatible features.

25. **Firebird parser gaps (window specs, predicate variants, clause stubs)**
   - **Where**: `src/parser/firebird/firebird_parser.cpp#L874`, `src/parser/firebird/firebird_parser.cpp#L987`, `src/parser/firebird/firebird_parser.cpp#L2629`
   - **Observed**: TODOs for window specification parsing, predicate variant tracking (LIKE/CONTAINING/STARTING/SIMILAR), and clause parsing stubs.
   - **Impact**: Firebird compatibility is partial; specific syntax is not parsed or mapped to SBLR.
   - **Expected**: Implement parsing and bytecode mapping for Firebird syntax variants required by client apps.

26. **MySQL parser gaps (NULL-safe equality, placeholders, constraints, geometry types)**
   - **Where**: `src/parser/mysql/mysql_parser.cpp#L833`, `src/parser/mysql/mysql_parser.cpp#L1097`, `src/parser/mysql/mysql_parser.cpp#L1988`, `src/parser/mysql/mysql_parser.cpp#L2300` to `src/parser/mysql/mysql_parser.cpp#L2336`
   - **Observed**: NULL-safe semantics for `=` are TODO, placeholder handling is TODO, table constraints parsing is TODO, geometry type mapping is TODO, and multiple unimplemented parser branches.
   - **Impact**: MySQL dialect compatibility is incomplete for common syntax, especially constraints and geometry.
   - **Expected**: Implement these MySQL-specific syntax rules and map to ScratchBird equivalents or emulated catalog structures.

27. **PostgreSQL parser gaps (ESCAPE handling, array subscripts, CREATE stubs)**
   - **Where**: `src/parser/postgresql/pg_parser_expr.cpp#L208`, `src/parser/postgresql/pg_parser_expr.cpp#L358`, `src/parser/postgresql/pg_parser_ddl.cpp#L799`
   - **Observed**: TODO for ESCAPE in LIKE, array subscript handling, and CREATE statement stubs.
   - **Impact**: PostgreSQL dialect fails on common expressions and DDL types.
   - **Expected**: Implement these expression/DDL features and map to ScratchBird core primitives or emulation objects.

28. **FDW adapter selection is stubbed**
   - **Where**: `src/fdw/protocol_adapter.cpp#L67` to `src/fdw/protocol_adapter.cpp#L83`
   - **Observed**: Factory returns nullptr for all adapter types.
   - **Impact**: FDW integration is non-functional even if adapters exist.
   - **Expected**: Wire adapter creation to return concrete PostgreSQL/MySQL/Firebird adapters.

29. **FDW adapters have incomplete protocol handling**
   - **Where**:
     - `src/fdw/firebird_adapter.cpp#L484` (result parsing via XSQLDA)
     - `src/fdw/postgresql_adapter.cpp#L330` (error fields)
     - `src/fdw/postgresql_adapter.cpp#L382` (type OID handling)
     - `src/fdw/postgresql_adapter.cpp#L889` (MD5 auth)
   - **Observed**: TODOs in result parsing, error handling, type metadata, and auth.
   - **Impact**: FDW connectivity and data marshaling are unreliable/incomplete.
   - **Expected**: Implement protocol parsing and type mapping per dialect specifications.

30. **Tests indicate parser/bytecode gaps and disabled coverage**
   - **Where**:
     - `tests/CMakeLists.txt#L204`, `tests/CMakeLists.txt#L1026`, `tests/CMakeLists.txt#L1126` (tests disabled due to V2 parser/bytecode issues)
     - `tests/sql/test_sequences.sql#L153`, `tests/sql/test_views.sql#L63` (executor and view check-option deferred)
     - `tests/unit/test_charset_loader.cpp#L263` (catalog persistence not verified)
   - **Observed**: Multiple tests are disabled or marked TODO due to missing V2 parser features or executor stubs.
   - **Impact**: Large portions of dialect compatibility and catalog behavior are untested and possibly broken.
   - **Expected**: Restore tests as features are implemented; keep per-dialect test suites aligned with supported syntax and mappings.

31. **Compatibility/emulation gaps are not consistently implemented**
   - **Where**: Parser TODOs + catalog stubs for dialect-specific emulation (see items 23–30).
   - **Observed**: Many dialect-specific constructs are parsed partially or not at all; catalog emulation tables (for client expectations) are not uniformly populated or queried.
   - **Impact**: Clients for Firebird/MySQL/PostgreSQL may see incomplete system catalogs or unsupported syntax despite connection success.
   - **Expected**: For each dialect, define the required system catalog objects and map parser outputs to either ScratchBird-native operations or emulation structures that satisfy client metadata queries.

---

## 8) Security Architecture and UUID Resolution Gaps

32. **UUID ↔ schema/path/object-name conversion lacks a unified API (design decision: single view + indexes)**
   - **Where**: `src/sblr/executor.cpp#L23890` to `src/sblr/executor.cpp#L24170` (SHOW LOCATION/RESOLVED by name) and `src/core/catalog_manager.cpp` (object creation and name->UUID resolution, but no general UUID->path resolver).
   - **Observed**: There are name-based resolution helpers and owner-name → UUID mapping, but no central API to convert *any SQL object UUID* back to full schema path/object name across object types.
   - **Impact**: Security auditing, dependency reporting, and admin tooling cannot reliably map UUIDs to user-facing names/paths; this undermines the UUID-only object identity model.
   - **Expected**: Implement a unified resolver view (UNION across catalog tables) that exposes: object UUID, schema path, full path, name, type (and other fields as needed). Back it with hash indexes on UUIDs and B-tree indexes on schema/name to allow efficient lookup in both directions.

33. **Security Context lacks AuthKey, session UUID, emulation mode, and policy epoch**
   - **Where**: `src/core/connection_context.cpp#L894` and `include/scratchbird/core/connection_context.h` (security context fields).
   - **Observed**: ConnectionContext tracks user/role/superuser and a definer stack, but does not store AuthKey UUID, session UUID, emulation mode, or policy epoch as required by `docs/specifications/draft_security_architecture_specification.md`.
   - **Impact**: Authorization decisions lack required context (AuthKey validity window, emulation mode), and policy/version invalidation cannot be enforced reliably.
   - **Expected**: Extend ConnectionContext SecurityContext to include AuthKey UUID, session UUID, emulation mode, and policy epoch; ensure these are immutable for the lifetime of a transaction.

34. **Role switching is not transaction-bound and lacks configurable default-action policy**
   - **Where**: `src/core/connection_context.cpp#L919` to `src/core/connection_context.cpp#L940`
   - **Observed**: `setActiveRole()` and `setCurrentUser()` can be called at any time; there is no enforcement of transaction-bound role switching.
   - **Impact**: Violates the spec requirement that role switching requires transaction commit/rollback; security context is not immutable for a transaction.
   - **Expected**: Enforce role/user changes only at transaction boundaries, with a configurable per-user/role/group default action (commit vs rollback) when not specified, and align with configurable default schema/namespace settings.

35. **Approval token / declared dependency enforcement is not evident**
   - **Where**: `src/sblr/bytecode_generator_v2.cpp#L574` to `src/sblr/bytecode_generator_v2.cpp#L575` (object UUIDs emitted), `include/scratchbird/sblr/query_compiler_v2.h#L185` (dependency collection), `src/sblr/executor.cpp#L23700` (dependency reporting stub).
   - **Observed**: Bytecode includes object UUIDs and the compiler can collect dependencies, but executor-level enforcement of “declared dependencies” and issuance/validation of approval tokens (per spec) is not visible.
   - **Impact**: Engine-side authorization may be bypassed by undeclared object access or mismatched dependency sets.
   - **Expected**: Introduce an approval/authorization phase that validates declared dependencies against the security context and issues a transaction-bound approval token; enforce at execution time.

36. **AuthKey model and session linkage to transactions are unclear**
   - **Where**: No AuthKey types found in `src/core` or `src/security`; session records exist in `src/core/catalog_manager.cpp#L13189` but are not tied to ConnectionContext.
   - **Observed**: The spec requires session UUID + AuthKey UUID per transaction; ConnectionContext does not link to CatalogManager session data or AuthKey.
   - **Impact**: Audit records and authorization decisions cannot be tied to AuthKey/session scope as required.
   - **Expected**: Implement AuthKey objects (storage + validity + role/group scope) and bind sessions to connection contexts and transactions.

37. **Audit logging is not persisted or tamper-evident, and storage/encryption targets are not configurable**
   - **Where**: `src/core/audit_logger.cpp#L33` to `src/core/audit_logger.cpp#L76`
   - **Observed**: Audit events are buffered in memory; catalog persistence is a TODO, and no tamper-evident mechanism is present.
   - **Impact**: Violates the security spec requirement for append-only, tamper-evident audit logs; audit data is lost on restart.
   - **Expected**: Implement configurable audit sinks (catalog tables, filesystem logs, and optional broadcast such as Kafka). Support enable/disable per architect and enforce tamper-evident integrity (hash chain/signature/WORM) at higher security levels. Add optional encryption where decryption keys can be retrieved from another cluster member if required by policy.

38. **RLS policy evaluation lacks policy epoch / invalidation context**
   - **Where**: `include/scratchbird/sblr/executor.h#L775` (RLS helpers) and `src/core/catalog_manager.cpp#L14534` (RLS flags).
   - **Observed**: RLS helpers exist, but there is no visible policy epoch tracking or plan invalidation tied to policy changes as required by the security spec.
   - **Impact**: Cached plans could ignore policy updates or privilege changes.
   - **Expected**: Track policy epoch in the security context and invalidate plans on policy changes; enforce immutability within a transaction.

39. **Local security caching lacks configurable quorum/cluster separation safeguards**
   - **Where**: No explicit quorum-based cache controls found in `src/core` or `src/security`.
   - **Observed**: Spec intent allows local caching but requires disabling or validating against a quorum of cluster members in some modes; no mechanism is visible.
   - **Impact**: A partitioned node could serve stale or overly-permissive auth data, enabling data extraction.
   - **Expected**: Implement configurable quorum checks (N-of-M) for security authority verification, with policy-driven cache enable/disable and configurable behavior when quorum cannot be reached. Support optional encryption requiring a decryption key from another cluster member when mandated by policy.

40. **Object rename/move (schema relocation) support appears incomplete**
   - **Where**: `src/parser/parser_v2.cpp#L1016` to `src/parser/parser_v2.cpp#L1023` (RENAME TABLE parsing), `src/sblr/executor.cpp#L3469` to `src/sblr/executor.cpp#L3555` (RENAME COLUMN only), `src/core/catalog_manager.cpp#L9414` (renameColumn), no `renameTable`/`moveSchema` implementation found.
   - **Observed**: Parser recognizes `ALTER TABLE ... RENAME TABLE`, but executor/catalog only implement rename column and tablespace rename. No evidence of `ALTER ... SET SCHEMA` or object move to a new schema with optional rename.
   - **Impact**: Cannot relocate SQL objects between schemas or rename tables via SBLR/SQL, which is required for full namespace/path-based identity management.
   - **Expected**: Implement catalog + executor support for object renaming (tables, views, sequences, etc.) and schema relocation (e.g., `ALTER ... SET SCHEMA`, or a dedicated MOVE/RENAME command). Ensure UUID identity remains stable while names/paths change.

---

If you want, I can break these into per-dialect implementation checklists (ScratchBird/V2, Firebird, MySQL, PostgreSQL) and map each gap to specific specs in `docs/specifications/`.

## 9) Object Persistence Gaps

30. **Multiple SQL object types are in-memory only (no catalog persistence)**
   - **Where**:
     - Views: `src/core/catalog_manager.cpp` (ViewRecord defined but unused; `CatalogManager::createView` only updates `view_cache_`)
     - Sequences: `src/core/catalog_manager.cpp` (SequenceRecord defined but unused; `CatalogManager::createSequence` only updates `sequence_cache_`)
     - Triggers (table + database): `src/core/catalog_manager.cpp` (`CatalogManager::createTrigger`/`createDatabaseTrigger` update caches only; TriggerRecord unused)
     - Functions/Procedures: `src/core/catalog_manager.cpp` (`registerFunction`/`registerProcedure` only update maps; ProcedureRecord unused)
     - Synonyms/Foreign tables: `src/core/catalog_manager.cpp` (caches exist; no create/load/write paths)
   - **Observed**: These objects are created and renamed in memory but no on-disk catalog writes occur, so they vanish after restart and resolver cache rebuilds lose them.
   - **Impact**: Non-durable metadata, broken dependency tracking after restart, and SHOW/DDL output becomes inconsistent with prior state.
   - **Expected**: Add catalog tables and read/write paths for each object type; update CREATE/ALTER/DROP to persist via `writeRecordToHeapPage`/`updateRecordInHeapPage`, and load them on startup.

## 10) Security Configuration Matrix (Architect-Controlled)

This matrix captures security-relevant settings that must be configurable by the system architect. It serves as a readiness checklist for Beta.

| Area | Decision Point | Required Configurability | Notes / Expected Behavior |
|---|---|---|---|
| Role Switching | Default action when unspecified | Per user/role/group | Must be transaction-bound; default action (COMMIT vs ROLLBACK) is configurable. |
| Default Namespace | Starting schema/namespace | Per user/role/group | Precedence order must be configurable; do not hardcode. |
| UUID Resolver | Object name ↔ UUID mapping | Global (DB/cluster) | Unified resolver view with indexed lookups; must support all object types. |
| AuthKey Storage | Local vs cluster authority | Global (DB role in cluster) | Support local-only, cluster-authority, and mixed mappings. |
| AuthKey Cache | Local caching allowed | Global (DB/cluster policy) | Must allow disablement or quorum-gated use. |
| Quorum Verification | N-of-M threshold | Global (cluster policy) | Must support single-node (N=1) and high-security thresholds. |
| Quorum Failure Mode | Behavior on quorum loss | Global (cluster policy) | Configurable: fail-closed, restricted mode, or other architect-defined behavior. |
| Audit Sinks | Catalog/log/broadcast | Global (security level) | Must support enable/disable per sink and multi-sink fanout. |
| Audit Integrity | Tamper-evident mode | Global (security level) | Hash chain/signature/WORM required at higher levels. |
| Audit Encryption | Key source | Global (cluster policy) | Optional encryption with keys retrievable from another cluster member. |
| Policy Epoch | Invalidation strategy | Global (DB/cluster policy) | Must support both global and per-table epochs. |
| Emulation Security | Emulated object handling | Global (DB policy) | Emulated objects can be sandboxed or upgraded to full security. |
| Dynamic SQL | Enable/disable + approval | Global (security level) | Must be enforceable and auditable. |
| Plugin Capabilities | Allowed capabilities | Global (cluster policy) | Must enforce declared capabilities with optional centralized approval. |
| Key Management | Key escrow/rotation policy | Global (cluster policy) | Support rotation, revocation, and optional escrow per security level. |
| RLS Bypass | Owner/superuser bypass policy | Global (DB/cluster policy) | Must be configurable; default must not be hardcoded. |
| Metadata Visibility | Catalog exposure level | Global (security level) | Must support restricted metadata modes (no enumeration vs limited vs full). |
| Dependency Enforcement | Strictness of declared dependency checks | Global (security level) | Configurable enforcement strictness; must log violations. |
| Approval Token | Token requirement and scope | Global (security level) | Configurable on/off; token bound to transaction and dependency set. |
| Auth Source Mapping | External identity ↔ internal user/role | Global (DB/cluster policy) | Must support external auth with internal permission mapping. |
| Group Membership Source | External vs internal group resolution | Global (DB/cluster policy) | Configurable per deployment; mixed models allowed. |
| Session Limits | Max sessions/idle timeout policies | Global (DB/cluster policy) | Must be configurable per user/role/group. |
| Network Security | TLS/mTLS + client validation | Global (security level) | Required at higher levels; must be enforceable. |
| Audit Broadcasting | External streaming (e.g., Kafka) | Global (security level) | Configurable sink; should not replace authoritative audit log. |
| Break-Glass | Emergency override policy | Global (cluster policy) | Must be configurable; requires strong audit and optional quorum approval. |
| Emergency Admin | Elevated access activation | Global (cluster policy) | Configurable activation method, scope, duration, and audit requirements. |
| Export Controls | Data export restrictions | Global (security level) | Must allow allowlist/denylist by object, role, or data class; audited. |
| Legal Hold | Retention / immutability | Global (cluster policy) | Configurable retention periods and hold flags for audit and backup data. |
| Data Classification | Sensitivity tagging | Global (DB/cluster policy) | Configurable labels that drive policy decisions (RLS, export, audit). |
| Key Escrow | External key custody | Global (cluster policy) | Optional; must support externalized key custody per security level. |
| Key Rotation | Rotation cadence + triggers | Global (cluster policy) | Configurable periodic and event-driven rotation with audit trails. |
| Backup Retention | Retention and purge rules | Global (cluster policy) | Configurable by backup type (logical/physical/WAL if enabled). |
| Restore Authorization | Who can restore | Global (cluster policy) | Configurable privileges; restore actions must be audited. |
| Shadow Promotion | Promotion approval | Global (cluster policy) | Configurable quorum/approval and audit requirements. |
| Pass-Through Mode | Legacy execution controls | Global (migration policy) | Configurable scope, time window, and audit of pass-through execution. |
| Metadata Redaction | Redact object names/paths | Global (security level) | Configurable redaction for low-privileged users. |
| Session Impersonation | Impersonate controls | Global (cluster policy) | Configurable, audited, and optionally requires multi-party approval. |
| Credential Lifetimes | AuthKey/session TTL | Global (cluster policy) | Configurable TTLs, max refresh, and revocation behavior. |
| MFA Policy | MFA requirements | Global (cluster policy) | Configurable by role/group; enforced at auth time. |
| Geo-Fencing | Geographic access rules | Global (security level) | Configurable allow/deny by region/IP; audited. |
| DLP Controls | Sensitive data leakage prevention | Global (security level) | Configurable patterns/policies; enforce on export and query results. |
| Result Caps | Query result size limits | Global (DB/cluster policy) | Configurable caps per user/role/group; audited on exceed. |
| Data Provenance | Lineage tracking policy | Global (DB/cluster policy) | Configurable capture of data source lineage; surfaced to audit and policy checks. |
| Schema Governance | Migration/version enforcement | Global (DB/cluster policy) | Configurable DDL gates, schema version pinning, and review requirements. |
| Resource Governance | CPU/IO/memory budgets | Global (DB/cluster policy) | Configurable per user/role/group; enforce workload isolation. |
| Secure Deletion | Erasure strategy | Global (security level) | Configurable crypto-shred vs physical overwrite for data and backups. |
| Cross-Cluster Trust | Trust boundary rules | Global (cluster policy) | Configurable trust list and validation for AuthKey/policy exchange. |
| Data Residency | Placement constraints | Global (cluster policy) | Configurable constraints by region/zone for tablespaces/migrations. |
| Git Integration | Version control requirements | Global (DB policy) | Configurable Git-backed DDL tracking and policy enforcement. |
