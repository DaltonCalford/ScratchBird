# Normative Checklist: Emulated Parser Layer Baseline (Alpha)

## Purpose
Define the mandatory runtime and translation contract shared by all emulated parser families.

## Scope
- Firebird, PostgreSQL, MySQL, Cassandra, MongoDB, Neo4j, Redis, and Milvus parser families.
- Client-facing 1:1 compatibility boundary at parser edge.
- Parser-to-server translation and authentication behavior over IPC/SBWP.

## Hard Invariants
1. Emulated parser must provide 1:1 client-facing protocol and dialect behavior for its target engine profile.
2. Parser must translate requests/responses to and from ScratchBird SBLR contracts.
3. Engine/server remains authoritative for authentication, authorization, and execution.
4. If server reports no open ScratchBird database, parser authentication must fail automatically.
5. Parser worker assigned to a client connection must be torn down on disconnect; next connection receives a fresh parser worker.
6. Emulated parser `CREATE DATABASE` behavior is logical emulation mapping only and must not create physical database files.
7. Physical database create is allowed only by server-side create flows, embedded-local create flows, or bridge-authorized cluster flows.
8. Every emulated parser family is only the client-facing half of the family
   package; engine-facing support lives in the matching support UDR package.
9. Emulated listener service for a family is forbidden unless the matching
   support UDR package is installed, validated, and `READY`.

## Parser Families (Mandatory)
- `firebird`
- `postgresql`
- `mysql`
- `cassandra`
- `mongodb`
- `neo4j`
- `redis`
- `milvus`

## Required Pre-Authentication Gate
Before dialect auth exchange is accepted, parser must run server precheck:

Request:
`IPC_AUTH_PRECHECK_REQ`
- `parser_family`
- `listener_instance_uuid`
- `requested_database_selector` (nullable)

Response:
`IPC_AUTH_PRECHECK_RSP`
- `ALLOW`
- `DENY_NO_OPEN_DATABASE`
- `DENY_DATABASE_NOT_OPEN`
- `DENY_POLICY`

Required behavior:
1. `ALLOW` only path that may proceed to dialect auth.
2. `DENY_NO_OPEN_DATABASE` must map to dialect-auth-failure envelope and close connection.
3. Parser must not attempt local auth bypass or local execution when denied.

## Connection Lifecycle Contract (Per Connection)
### EPL00 Accept and Assign
- [ ] Listener accepts connection on parser-family bound port.
- [ ] Listener assigns one fresh parser worker from warm pool or new spawn.

### EPL01 Precheck and Auth
- [ ] Parser performs `IPC_AUTH_PRECHECK_REQ`.
- [ ] Parser proceeds with dialect auth only when precheck is `ALLOW`.
- [ ] Parser maps failed precheck to dialect-auth-failure and terminates connection.

### EPL02 Session and Request Handling
- [ ] Parser opens server session after auth success.
- [ ] Parser translates client protocol and dialect commands to SBLR request envelopes.
- [ ] Parser maps server responses to dialect-native protocol and error envelopes.

### EPL03 Disconnect and Teardown
- [ ] On client disconnect, parser closes server session/transaction context deterministically.
- [ ] Parser zeroizes credential/session buffers.
- [ ] Parser worker process transitions to terminate/recycle and is not reused for a new client.

Pass condition:
- Every new client connection receives a fresh parser worker lifecycle.

## Database Creation Policy Matrix
| Path | Physical ScratchBird Database Create | Logical Emulated Database Create |
| --- | --- | --- |
| Embedded local client (engine library) | `ALLOW` (no auth required) | `ALLOW` |
| Server-native management path | `ALLOW` (server policy enforced) | `ALLOW` |
| Emulated parser path | `DENY` | `ALLOW` only when base ScratchBird database is open |
| ScratchBird bridge cluster path | `ALLOW` (bridge policy enforced) | `ALLOW` |

Rules:
1. First install with network access requirement must create default ScratchBird database locally before IP clients can authenticate.
2. Emulated parser `CREATE DATABASE` maps to schema/catalog/domain visibility
   bootstrap inside an existing open ScratchBird database through the matching
   support UDR package.
3. Emulated parser `CREATE DATABASE` must be rejected when no base ScratchBird database is open.

## Listener Readiness Rule for Emulated Ports
1. Listener port bound to an emulated parser family must not report `READY` until warm parser workers for that family are available.
2. If warm count drops to zero, listener remains bound but must reject new connection assignments with deterministic overload/unavailable error until replenished.
3. Listener port bound to an emulated parser family must not start or report
   `READY` unless the matching support UDR package is `READY`.

## Support Package Boundary Rule
1. Parser package handles client protocol, client SQL or command parsing,
   normalization, and response mapping only.
2. Engine-facing dynamic SQL or command translation and family-owned non-core
   operations are handled only by the matching support UDR package.
3. Parser worker must not attempt to substitute support-UDR-owned behavior with
   local parser-side execution.

## Conformance Gates
- `P28-EMPL-BASE-01`: pre-auth no-open-database gate and deterministic auth-fail mapping.
- `P28-EMPL-BASE-02`: one-connection-per-parser-worker lifecycle and teardown zeroization.
- `P28-EMPL-BASE-03`: create-database policy matrix enforcement.
- `P28-EMPL-BASE-04`: listener family-readiness and warm pool enforcement.
- `P28-EMPL-BASE-05`: parser-package and support-package completeness gate.

## Evidence Artifacts
- `docs/specifications/work/conformance/parser_layer/common/PREAUTH_GATE_RESULTS.csv`
- `docs/specifications/work/conformance/parser_layer/common/PARSER_LIFECYCLE_RECYCLE_RESULTS.csv`
- `docs/specifications/work/conformance/parser_layer/common/CREATE_DATABASE_POLICY_RESULTS.csv`
- `docs/specifications/work/conformance/parser_layer/common/LISTENER_FAMILY_READINESS_RESULTS.csv`
- `docs/specifications/work/conformance/parser_layer/common/EMULATION_PACKAGE_COMPLETENESS_RESULTS.csv`

## Cross-Section Links
- `docs/specifications/27_Native_Handshake/AUTH_NEGOTIATION_AND_POLICY.md`
- `docs/specifications/17_Functions_and_Procedures/NORMATIVE_UDR_EMULATED_ENGINE_SUPPORT_CHECKLIST.md`
- `docs/specifications/29_Listener_and_Server_Orchestration/CONNECTION_AND_SESSION_LIFECYCLE.md`
- `docs/specifications/29_Listener_and_Server_Orchestration/PARSER_POOL_ASSIGNMENT_AND_SCALING.md`

## Audit normalization note (2026-03-28)
- This file is now treated as target-state-only or checklist-only material.
- Current section-28 source proof does not show a shipped dedicated parser implementation, parser-agent executable, and listener/runtime lane for this family.
- Nearby native-V3 feature vocabulary, catalog entries, or runtime terminology are not sufficient to promote this family into current dedicated parser parity.

## Hardening promotion note (2026-03-28)
- section `28` now carries explicit capability-state vocabulary for parser implementation proof lanes:
  - `supported_native_v3`
  - `supported_emulated_sql_family`
  - `supported_scaffold_or_udr_boundary`
  - `bounded_shipped_front_door`
  - `checklist_only`
  - `target_state_only`
  - `fail_closed`
- dedicated parser-family proof must be anchored to live parser code plus shipped parser-agent or listener/runtime seams, not to checklist presence alone
- native-V3 internal feature vocabulary must not be promoted into dedicated external parser-family parity without family-local source proof
- universal capability-profile generation, universal corpus cardinality, and universal wire parity claims remain non-authoritative unless backed by generated or runtime evidence
