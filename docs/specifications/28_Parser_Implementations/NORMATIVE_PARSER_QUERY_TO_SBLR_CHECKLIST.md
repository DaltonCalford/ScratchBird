# Normative Checklist: Parser Query to SBLR Pipeline (Alpha)

## Purpose
Provide a strict, step-by-step implementation checklist for parser workers so a low-capability implementation agent can build deterministic parser behavior without design guesses.

## Scope
- native parser and all emulated parsers
- plan-only and execute requests
- SQL and non-SQL dialect ingress
- parser-side committed catalog snapshot and delta synchronization
- security and DCL lowering families

## Hard invariants
1. Parser never executes query semantics locally.
2. Parser never bypasses engine authorization.
3. Parser emits UUID-based `SBLR` references only after binding.
4. Engine remains SQL-agnostic; parser handles dialect-specific text and wire protocols.
5. Same input plus same profile plus same catalog view must emit byte-identical `SBLR`.
6. ScratchBird is always in a transaction; parser cache synchronization is tied to transaction boundaries.
7. Each parser lowers its own syntax locally; no cross-parser lowering dependency is allowed.

## Required output artifacts
- deterministic `SBLR` payload
- UUID binding request and binding map
- source map
- capability decision log
- engine-visible SQL or command trace
- dialect-native response mapping record
- parser catalog anchor used for the request

## Implementation checklist

### P00 Context freeze
- [ ] Read and freeze session context for the request.
- [ ] Reject if transaction is not active.
- [ ] Reject if parser dialect profile is missing.
- [ ] Create immutable request correlation id.

### P01 Ingress decode
- [ ] Decode wire frame into canonical request envelope.
- [ ] Validate payload length and framing.
- [ ] Reject malformed ingress with dialect-native protocol error.

### P02 Catalog baseline sync
- [ ] If no committed parser cache exists, call `sb_catalog_snapshot_begin`.
- [ ] If a committed parser cache exists, call `sb_catalog_delta_since_anchor`.
- [ ] If the previous successful autocommit response already carried the canonical delta, reuse it instead of re-calling the delta helper.
- [ ] If the helper returns `reset_required = true`, discard local bulk cache and reissue `sb_catalog_snapshot_begin`.
- [ ] If the previous statement failed, do not advance the catalog anchor because no commit occurred and the transaction remains active.

### P03 Dialect parse or decode
- [ ] For SQL dialects, parse text into the active dialect AST only.
- [ ] For command dialects, decode the active dialect command tree only.
- [ ] Preserve source spans for every parsed unit.
- [ ] Do not invoke another parser family to complete parsing or lowering.

### P04 Capability gate
- [ ] Evaluate every statement and clause against dialect profile.
- [ ] Resolve each item to one of: `IMPLEMENT`, `REMAP`, `REJECT`.
- [ ] Persist deterministic decision-log entries.

### P05 Canonicalization
- [ ] Transform dialect tree into canonical AST.
- [ ] Apply remap rules for all `REMAP` decisions.
- [ ] Normalize operators, casts, and function identities to canonical forms.
- [ ] Preserve source-map links.

### P06 Name resolution and discoverability-safe binding
- [ ] Build object-lookup batch entries.
- [ ] Use committed bulk cache for baseline candidate discovery.
- [ ] If current-transaction `DDL` can affect binding, use point helpers.
- [ ] Never emit object-existence details for non-discoverable objects unless explicit admin diagnostic mode is active.

### P07 Security and DCL lowering
- [ ] Lower `CREATE USER`, `ALTER USER`, `CREATE ROLE`, `CREATE GROUP`, `GRANT`, `REVOKE`, `SET ROLE`, `SET SESSION AUTHORIZATION`, policy DDL, RLS table actions, and domain-security clauses through the active parser family only.
- [ ] Preserve privilege lists, object paths, policy names, role names, grantee names, and domain security options exactly.
- [ ] Reject unsupported dialect-local security clauses instead of silently dropping them.
- [ ] Preserve security-significant source spans for error mapping.

### P08 Parameter extraction and signature
- [ ] Extract all parameter placeholders in canonical order.
- [ ] Create the typed parameter-metadata vector.
- [ ] Compute `parameter_signature` deterministically from that vector.

### P09 SBLR emission
- [ ] Emit `SBLR` from canonical AST with UUID references only.
- [ ] Include statement-normalization evidence fields required by section `22`.
- [ ] Include deterministic checksums.

### P10 Parser preflight validation
- [ ] Validate opcode symbols are known.
- [ ] Validate payload lengths and required fields.
- [ ] Validate normalization-evidence presence.
- [ ] Reject locally invalid `SBLR` before IPC dispatch.

### P11 Engine request build
- [ ] Build engine request envelope with frozen session context, request mode, canonical `SBLR`, source map id, capability decision log, engine-visible SQL or command trace, parameter metadata and signature, parser catalog anchor, and optional parser hints.
- [ ] Send over native IPC and `SBWP` contract.

### P12 Engine response handling
- [ ] On `PLAN_ONLY` success, decode plan handle and plan metadata.
- [ ] On `EXECUTE` success, decode result chunks and preserve column order and type-mapping contract.
- [ ] If autocommit committed successfully and canonical parser delta is piggybacked, record it as the next baseline.
- [ ] On error, map engine error to dialect-native error code, include correlation id and source span, and do not advance parser catalog anchor.

### P13 Egress render
- [ ] Render names using session-language policy and fallback rules.
- [ ] Apply dialect case and quoting rules.
- [ ] Encode response in client wire protocol.

## Negative requirements
- Parser must not synthesize missing authorization decisions.
- Parser must not infer unknown capability rows.
- Parser must not execute partial responses after hard engine failure.
- Parser must not advance committed cache anchors after failed statements or rolled-back transactions.
- Parser must not rely on another parser package to lower security or DCL syntax.
