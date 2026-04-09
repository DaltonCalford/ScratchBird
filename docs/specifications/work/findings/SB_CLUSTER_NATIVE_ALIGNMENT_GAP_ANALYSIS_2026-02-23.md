# SB Cluster and Hybrid Native Alignment Gap Analysis

Date: 2026-02-23
Scope: Compare the following specification set against current ScratchBird implementation and provide implementation guidance with proof anchors.

Input specifications:
- `SB-CLUSTER-SWS-MGA-01.md`
- `SB-CLUSTER-IMPLEMENTATION-WORKPLAN .md`
- `SB-CLUSTER-THREAT-MODEL-AND-FAILURE-ANALYSIS.md`
- `SB-OBS-METRICS-MONITORING-01.md`

## Method
- Read the four source specs.
- Validate required behaviors against ScratchBird parser, emitter, executor, catalog, telemetry, and server runtime code.
- Use unit tests as positive/negative proof where available.
- Classify each requirement as `implemented`, `partial`, or `missing`.

## Executive Summary
- Current state is strong on parser/catalog scaffolding and weak on runtime closure for cluster safety semantics.
- Cluster control SQL currently maps to vNext opcodes, but runtime still rejects these through bridge contract `BRG_0406`.
- Observability foundations exist (Prometheus/OpenMetrics export, `sys.*` introspection), but metric naming and view schema diverge from SB-OBS requirements.
- Hybrid native compilation foundations are present in catalog schema and section-23 specs, but end-to-end compile/execute runtime integration remains incomplete.

## Strengths (What Is Already Good)
1. Cluster control parser coverage exists for workload/admission/set/show control families.
2. Emitter contracts map cluster feature keys to stable vNext opcodes.
3. Catalog model is extensive for cluster, shard, routing, admission, healing, and fabric entities.
4. Native artifact catalog structures are already present (`sblr_module`, `sblr_plan`, `sblr_artifact`, `sblr_compile_queue`).
5. Contract tests already enforce parser/emitter mapping and bridge rejection behavior deterministically.

Proof anchors:
- Parser cluster control entry points:
  - `src/parser/parser_v3.cpp:15418`
  - `src/parser/parser_v3.cpp:15464`
  - `src/parser/parser_v3.cpp:15521`
  - `src/parser/parser_v3.cpp:15600`
- Emitter mappings:
  - `src/parser/v3_emitter.cpp:2801`
- Cluster/fabric catalog structures:
  - `include/scratchbird/core/catalog_manager.h:5582`
  - `include/scratchbird/core/catalog_manager.h:6386`
  - `include/scratchbird/core/catalog_manager.h:6472`
- Native artifact catalog structures:
  - `include/scratchbird/core/catalog_manager.h:2608`
  - `include/scratchbird/core/catalog_manager.h:2663`
  - `include/scratchbird/core/catalog_manager.h:2708`
- Contract tests:
  - `tests/unit/test_parser_v3_nosql_emitter_contract.cpp:197`
  - `tests/unit/test_sblr_vnext_executor_dispatch_contract.cpp:1297`

## Gap Matrix (Spec vs Implementation)

| Requirement | Status | Proof | Impact | Recommendation |
| --- | --- | --- | --- | --- |
| Single-writer-per-shard enforced in runtime write path with fencing token | missing | Cluster opcodes are routed to vNext bridge then rejected (`BRG_0406`), not executed: `src/sblr/executor.cpp:65798`, `src/sblr/executor.cpp:65856` | High correctness risk for cluster mode delivery | Implement write-path leader-term + fencing validation before commit |
| Leader term and fencing token model (`leader_term`, token checks) | missing | No runtime evidence of leader-term validation in executor dispatch path; cluster control currently reject-only | Split-brain protection not enforceable | Add leader-term state and token validation API in transaction/write coordinator |
| Cluster identity persisted in DB header (`cluster_id`, `node_id`, `cluster_config_epoch`) | missing | Header fields include MGA metadata but no cluster identity fields: `include/scratchbird/core/database.h:79` | Cannot guarantee persistent cluster identity contracts | Extend DB header versioned layout and migration path |
| Injectable TimeSource abstraction | missing | UUIDv7 uses direct `system_clock::now`: `src/core/uuidv7.cpp:23` | Hard to test deterministic lease and epoch behavior | Add `TimeSource` interface; wire through UUID, leasing, and heartbeat logic |
| StorageLockProvider abstraction replacing direct `flock` | partial | Direct `flock` remains in database and daemon code: `src/core/database.cpp:108`, `src/server/daemon.cpp:162` | Portability/testability risk | Introduce platform lock provider abstraction and remove direct lock calls from business logic |
| Epoch pinning model (`cluster_config_epoch`, `schema_epoch`, `security_epoch`) in session context | partial | Session pins `policy_epoch_global/table`, not explicit cluster/schema/security epoch tuple: `include/scratchbird/core/catalog_manager.h:2951`, `src/server/server_session.cpp:1841` | Stale-plan and stale-routing behavior underspecified | Expand session context and invalidation checks to include explicit cluster/schema/security epochs |
| GTXID model `(shard_id, local_txn_id)` | missing | No explicit GTXID runtime contract surfaced in executor/tx manager code paths reviewed | Replication/audit ordering ambiguity across shards | Add GTXID structure and API contracts for commit, replication, and audit streams |
| Snapshot vector and CWM publication for cross-shard reads | missing | No implemented snapshot-vector behavior found in cross-shard runtime paths | Cross-shard read consistency undefined | Add CWM publication per shard and vector pinning in router |
| SCL replication and follower apply ordering | missing | No shard commit log runtime pipeline in current executor path; catalog has shard/replica metadata only | Replication correctness not delivered | Implement SCL append/apply service with idempotent ordering guarantees |
| GC safe horizon `min(OST_shard, RWM_shard)` | missing | MGA header has OIT/OAT/OST fields, but no cluster-safe RWM integration in GC enforcement | Potential premature reclamation in distributed mode | Extend sweep/GC gating with follower lag and snapshot registry |
| Domain control-plane replication (`DOMAIN_CREATE/ALTER/DROP`) | partial | Rich domain/catalog primitives exist; no demonstrated control-plane replicated domain log pipeline | Cluster domain convergence not guaranteed | Implement control-plane domain event log and join/rejoin hash verification |
| Required SHOW surfaces (`SHOW NODES/SHARDS/REPLICATION LAG/GC HORIZONS/SNAPSHOT REGISTRY`) | partial | Parser currently supports `SHOW CLUSTER STATE/ROUTING PLAN/ADMISSION STATUS` only: `src/parser/parser_v3.cpp:15611` | Operator visibility below spec baseline | Expand control grammar and wire to runtime/system views |
| Threat-model validation suites (split brain/failover/replay/snapshot retention) | missing | Dispatch contract currently validates rejection behavior rather than runtime closure: `tests/unit/test_sblr_vnext_executor_dispatch_contract.cpp:1388` | Safety requirements unproven | Add deterministic adversarial integration suite and gate artifacts |
| OpenMetrics endpoint support | partial | Export implementation exists, but `path` parameter currently unused by handler: `src/core/telemetry.cpp:1166` | Operational ambiguity for endpoint routing | Enforce endpoint-path routing and auth policy in listener/manager |
| SB-OBS metric naming (`sb_*`) | missing | Metrics currently use `scratchbird_*` prefix: `src/core/telemetry.cpp:903`, `src/core/telemetry.cpp:1060` | Naming contract mismatch for dashboards/alerts | Add `sb_*` canonical names; optionally keep `scratchbird_*` as compatibility aliases |
| SQL observability view schema (`sys.metrics.runtime`, `sys.metrics.health`, `sys.cluster.metrics.*`) | missing | Current views include `performance`, `cache_stats`, `buffer_pool_stats`, `shard_status`: `src/catalog/sys_catalog.cpp:435`, `src/catalog/sys_catalog.cpp:890` | Spec-required SQL observability contract not met | Add required view families with stable schemas |
| `/healthz` and `/readyz` readiness contract | partial | Manager currently exposes MCP status-style ready fields, not explicit HTTP health endpoints: `src/server/sb_manager_main.cpp:848`, `src/server/sb_manager_main.cpp:885` | Integration mismatch with operational tooling | Add explicit health endpoints and readiness state model |
| Structured event stream with epoch context | partial | Audit/event infrastructure exists, but SB-OBS-required event schema with cluster/schema/security epoch tuple not fully evidenced | Incident triage and compliance gaps | Add stable event schema including required epoch fields |
| Hybrid native explicit-compile path | partial | Section-23 spec exists and artifact catalog structures exist; runtime closure still bridge/reject for many vNext families | Performance optimization path delayed | Implement explicit compile APIs and artifact selector/fallback runtime |
| Optional JIT with suppression hints and queue backpressure | partial | Spec says `JIT_ALLOWED` optional; runtime implementation evidence not complete in executor path | Risk of uncontrolled latency if enabled prematurely | Deliver explicit-only first, then async JIT queue with deny hints and strict SLO gates |

## Examples / Proof Scenarios

### Example A: Parser coverage exists but runtime closure is absent
- Input SQL: `CREATE CLUSTER WORKLOAD CLASS ...`
- Parser/emitter test proves opcode mapping (`SBLR3_CLUSTER_WORKLOAD_CLASS`):
  - `tests/unit/test_parser_v3_nosql_emitter_contract.cpp:209`
- Executor dispatch currently sends this family through bridge reject path:
  - `src/sblr/executor.cpp:65856`
- Result: control SQL is syntactically accepted but not operationally implemented.

### Example B: Observability export exists but SB-OBS contract is not met
- OpenMetrics export function exists:
  - `src/core/telemetry.cpp:640`
- Metric names are `scratchbird_*`, not `sb_*`:
  - `src/core/telemetry.cpp:903`
- Required `sys.metrics.runtime`/`sys.metrics.health` view names are absent from current `sys_catalog` table list:
  - `src/catalog/sys_catalog.cpp:435`

### Example C: Hybrid artifact model has foundation but not full runtime wiring
- Artifact structures and APIs exist:
  - `include/scratchbird/core/catalog_manager.h:2663`
  - `src/core/catalog_manager.cpp:90511`
- No evidence that artifact APIs are invoked by executor/native runtime selection paths in current dispatch flow.
- Result: storage schema is ready, runtime closure remains to be implemented.

## Pros and Cons of the Current Direction

Pros:
- Existing parser and catalog scaffolding reduces implementation risk and shortens delivery timeline.
- Hybrid model keeps SBLR canonical and preserves rebasing portability.
- Existing rejection contracts (`BRG_0406`) avoid unsafe partial activation.

Cons:
- Significant gap between accepted control syntax and executable runtime behavior.
- Observability contract divergence can create operational blind spots.
- Cluster safety semantics are currently mostly design-time, not runtime-enforced.

## Recommended Implementation Order
1. Close core safety invariants first: cluster identity, epochs, fencing, routing checks.
2. Implement replication and GC-safe horizon before enabling broader cluster operations.
3. Align observability contracts (`sb_*`, required SQL views, health endpoints) before beta claims.
4. Deliver hybrid native explicit compile for routines first; defer JIT until latency and fallback metrics are stable.

## Delivery Readiness Assessment
- Parser/control surface readiness: Medium.
- Catalog/schema readiness: Medium-High.
- Runtime safety closure readiness: Low.
- Observability contract readiness: Low-Medium.
- Hybrid explicit-compile readiness: Medium.
- Hybrid JIT readiness: Low (should remain disabled by default).
