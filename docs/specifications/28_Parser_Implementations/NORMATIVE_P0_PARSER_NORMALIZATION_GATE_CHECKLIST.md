# Normative Checklist: P0 Parser Normalization Gate and Engine Handoff (Alpha)

## Purpose
Define strict parser-side normalization and handoff steps so low-capability implementations can emit deterministic, verifiable SBLR with no guesswork.

## Scope
- Native parser and all emulated parsers.
- SQL and non-SQL ingress paths.
- Required before any parser request reaches engine plan/cache/execution.

## Hard Invariants
1. Parser performs dialect handling only; engine remains SQL-agnostic.
2. Parser never executes query semantics locally.
3. Parser never emits unresolved object names in executable SBLR.
4. Parser must emit normalization evidence required by section 22 and section 23.
5. Same input and same profile context must produce byte-identical normalization artifacts and SBLR.

## Mandatory Inputs
- Frozen request context:
  - `connection_uuid`
  - `session_uuid`
  - `transaction_id`
  - `user_uuid`
  - `active_role_uuid`
  - `dialect_id`
  - `dialect_profile_id`
  - `dialect_profile_version`
  - `session_language_code`
  - `search_path_uuid_list`
- Raw request payload.
- Active capability profile and operator/coercion rules.

## Required Output Artifacts
- Canonical AST serialization.
- Normalization evidence fields:
  - `rule_set_id`
  - `clause_presence_bitmap`
  - `clause_order_vector`
  - `alias_rewrite_flags`
  - `operator_vector`
  - `coercion_vector`
  - `canonical_ast_hash`
  - `normalization_evidence_hash`
- UUID binding map and binding diagnostics.
- Deterministic SBLR container.

## Implementation Checklist

### N00 Context and Policy Freeze
- [ ] Freeze request context and profile versions at request start.
- [ ] Reject request if required context fields are missing.
- [ ] Reject request if dialect profile is missing or disabled.

Pass condition:
- All downstream parser stages use one immutable context and policy snapshot.

### N01 Ingress Decode
- [ ] Decode wire frame or command payload into canonical parser envelope.
- [ ] Validate framing length, type code, and payload checksum where protocol provides one.
- [ ] Reject malformed ingress with deterministic dialect-native error mapping.

Pass condition:
- Parser envelope is structurally valid before syntax parsing.

### N02 Parse to Dialect AST
- [ ] Parse ingress into dialect AST or command tree.
- [ ] Preserve exact token span mapping for each AST node.
- [ ] Reject syntax ambiguity using deterministic precedence rules.

Pass condition:
- Parsed tree and span map are complete and deterministic.

### N03 Capability Gate
- [ ] Evaluate each token/feature against capability profile.
- [ ] Resolve decision by fixed precedence:
  - disabled dialect -> reject
  - explicitly unsupported feature -> reject
  - explicit remap -> remap
  - explicit implementation support -> implement
  - no profile row -> reject
- [ ] Persist decision log row for each gated feature.

Pass condition:
- No ungated feature remains in AST.

### N04 Canonical Operator Normalization
- [ ] Map each parser-level operator to canonical operator id from section 13 rules.
- [ ] Reject any operator token that has no canonical mapping.
- [ ] Record normalized operator sequence in deterministic preorder traversal.

Pass condition:
- `operator_vector` is complete and contains only canonical operator ids.

### N05 Canonical Coercion Normalization
- [ ] Resolve coercion for each expression node using canonical coercion matrix.
- [ ] Reject unresolved implicit/explicit cast paths.
- [ ] Record coercion decisions in deterministic expression-node order.

Pass condition:
- `coercion_vector` is complete, deterministic, and valid for all typed nodes.

### N06 Canonical Clause Normalization
- [ ] Build clause presence bitmap using canonical clause codes.
- [ ] Build clause order vector from canonical clause sequence.
- [ ] Record alias rewrite flags where alias expansion occurs.
- [ ] Reject clause vectors that violate canonical ordering rules.

Pass condition:
- Clause evidence fields are complete and validator-ready.

### N07 UUID Name Resolution Batch
- [ ] Build lookup batch entries for every object reference:
  - requested object name
  - object kind
  - schema-path candidates
  - requested action class
- [ ] Send lookup batch to engine authoritative resolver.
- [ ] Apply resolver status mapping rules (`RESOLVED`, `NOT_FOUND`, `AMBIGUOUS`, `NOT_DISCOVERABLE`).

Pass condition:
- All object references are UUID-bound or deterministically rejected.

### N08 Canonical AST Serialization
- [ ] Serialize canonical AST with fixed field order, fixed map key order, and fixed numeric formatting.
- [ ] Use UTF-8 byte serialization and deterministic null encoding.
- [ ] Compute `canonical_ast_hash` over canonical AST bytes.

Pass condition:
- Canonical AST bytes are reproducible for identical inputs.

### N09 Normalization Evidence Hash
- [ ] Build normalization evidence payload in fixed field order:
  - `rule_set_id`
  - `clause_presence_bitmap`
  - `clause_order_vector`
  - `alias_rewrite_flags`
  - `operator_vector`
  - `coercion_vector`
  - `canonical_ast_hash`
- [ ] Hash payload bytes with configured hash algorithm.
- [ ] Store result as `normalization_evidence_hash`.

Pass condition:
- Identical normalization evidence inputs produce identical hash bytes.

### N10 SBLR Emission
- [ ] Emit SBLR using UUID references and canonical operator/coercion semantics.
- [ ] Include all normalization evidence fields in statement metadata.
- [ ] Include deterministic `sblr_checksum` and statement-level metadata checksums.

Pass condition:
- Emitted SBLR is structurally complete and normalization-coupled.

### N11 Parser Preflight Validation
- [ ] Validate all opcodes and payload lengths.
- [ ] Validate no unresolved symbolic names remain.
- [ ] Validate normalization fields are present and internally consistent.
- [ ] Reject payload locally on first structural violation.

Pass condition:
- Invalid SBLR never crosses IPC boundary.

### N12 Engine Envelope Build
- [ ] Build request envelope with:
  - frozen request context
  - request mode
  - SBLR payload
  - source map id
  - capability decision log id
  - normalization evidence fields and hash
  - parameter metadata and signature
- [ ] Ensure envelope includes fields required by section 23 cache key rules.

Pass condition:
- Engine receives all deterministic keying and verification inputs.

### N13 Plan-Only and Execute Paths
- [ ] For `PLAN_ONLY`, return plan metadata and opaque plan handle only.
- [ ] For `EXECUTE`, process row chunks and metadata according to dialect output contract.
- [ ] For stale plan handle rejection, rerun full N02..N12 pipeline.

Pass condition:
- Plan-only and execute paths are deterministic and replan-safe.

### N14 Error Mapping Contract
- [ ] Map parser-side failures to dialect-native error envelopes with correlation id and source span.
- [ ] Map engine-side failures to dialect-native errors without changing engine root cause code class.
- [ ] Reject any unmapped status as specification violation.

Pass condition:
- Every failure path has deterministic and complete error mapping.

### N15 Egress Render Contract
- [ ] Render names and identifiers with dialect case and quoting rules.
- [ ] Apply session language mapping and default fallback name rules.
- [ ] Preserve column ordering and output typing contract from engine response metadata.

Pass condition:
- Client-visible output is dialect-correct and stable.

## Negative Requirements
- No parser-side fallback to best-effort operator inference.
- No parser emission when normalization evidence is incomplete.
- No parser-side privilege inference.
- No parser bypass of engine UUID bind and authorization checks.

## Conformance Gates
- `P0-28-GATE-01`: N00..N03 pass for deterministic ingress and capability gating.
- `P0-28-GATE-02`: N04..N06 pass for operator/coercion/clause normalization evidence.
- `P0-28-GATE-03`: N07..N10 pass for UUID binding and normalized SBLR emission.
- `P0-28-GATE-04`: N11..N13 pass for preflight integrity and deterministic plan/execute paths.
- `P0-28-GATE-05`: N14..N15 pass for deterministic error and egress contracts.

## Cross-Section Links
- `13_Operator_Model_and_Coercion/README.md`
- `22_SBLR_Canonical_Model_and_Opcodes/SBLR_STATEMENT_PAYLOAD_SCHEMAS.md`
- `22_SBLR_Canonical_Model_and_Opcodes/SBLR_VERIFIER_AND_VALIDATION_RULES.md`
- `23_SBLR_VM_Compiler_and_Executor/NORMATIVE_P0_PLAN_AND_EXECUTION_OPTIMIZATION_CHECKLIST.md`
- `23_SBLR_VM_Compiler_and_Executor/EXECUTION_ERROR_MODEL_AND_DIAGNOSTICS.md`

## Audit normalization note (2026-03-28)
- This file is treated as a target-state worklist, not as present-day implementation proof.
- Current section `28` source authority is narrower and is centered on the native V3 parser stack plus the shipped Firebird, PostgreSQL, and MySQL parser-family seams.
- Broader normalization-gate, distributed-policy, passthrough, replication, connector, and fabric-parser claims require separate bounded proof before promotion.
