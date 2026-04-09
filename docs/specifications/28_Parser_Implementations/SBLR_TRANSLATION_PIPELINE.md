# SQL and Command <-> SBLR Translation Pipeline

## Purpose
Define deterministic parser-side translation and diagnostic rendering rules so a low-capability implementation agent can build parser pipelines without guessing behavior.

## Scope
- SQL dialects:
  - native
  - firebird
  - postgresql
  - mysql
- Non-SQL dialect surfaces:
  - cassandra
  - mongodb
  - neo4j
  - redis
  - milvus
- Shared target:
  - canonical `SBLR` request and response contracts
- Security and DCL lowering families

## Canonical data contracts

### ParserSessionContext
- `connection_uuid`
- `session_uuid`
- `transaction_id`
- `catalog_epoch`
- `security_epoch`
- `schema_epoch_uuid`
- `user_uuid`
- `active_role_uuid`
- `dialect_id`
- `dialect_profile_id`
- `dialect_profile_version`
- `session_language_code`
- `search_path_uuid_list`
- `feature_overrides`

### ClientRequestEnvelope
- `wire_protocol_id`
- `request_id`
- `raw_payload_bytes`
- `decoded_request_text_or_command`
- `request_kind`
- `client_encoding`
- `client_time_zone`

### TranslationArtifact
- `translation_origin`
- `normalized_input`
- `dialect_ast_or_command_tree`
- `capability_decisions`
- `canonical_ast`
- `uuid_bindings`
- `sblr_payload`
- `source_map`
- `engine_visible_sql`
- `diagnostic_template_id`

## Fixed translation algorithm

Approved translation origins:
1. client-facing parser worker
2. engine-facing emulated support UDR package

### Step 0: Catalog cache synchronization
1. ScratchBird is always in a transaction context.
2. At the start of a transaction, parser must ensure its catalog cache anchor matches the active transaction baseline.
3. If no local cache exists, parser must call `sb_catalog_snapshot_begin`.
4. If a local cache anchor exists from the previous committed transaction, parser must call `sb_catalog_delta_since_anchor`.
5. Under `AUTOCOMMIT`, if the engine piggybacks the canonical delta payload on the successful statement response, parser must reuse that payload as the next implicit transaction baseline.
6. If a statement errors, parser must not advance its cache anchor because no commit occurred and the transaction remains active.
7. If current-transaction `DDL` can make the bulk cache stale, parser must use point helpers.
8. If `sb_catalog_delta_since_anchor` returns `reset_required = true`, parser must discard the bulk cache and re-bootstrap through `sb_catalog_snapshot_begin`.

### Step 1: Decode ingress
1. Decode wire protocol frame into `ClientRequestEnvelope`.
2. Validate message framing and parser state.
3. Reject malformed frames with dialect-native protocol error.

### Step 2: Dialect parse or decode
1. If SQL dialect, parse input text using that dialect’s grammar only.
2. If command dialect, decode command payload into that dialect’s command tree.
3. Preserve source spans for every parsed unit.
4. Do not call another parser family to complete lowering.

### Step 3: Capability gate
1. For each statement, command, option, clause, and function, lookup capability in the active dialect profile.
2. Apply precedence:
   - disabled dialect -> `REJECT`
   - explicitly unsupported token -> `REJECT`
   - explicit remap rule exists -> `REMAP`
   - explicit implementation support exists -> `IMPLEMENT`
   - otherwise -> `REJECT`
3. Emit deterministic decision-log entry for each gated item.

### Step 4: Canonicalization
1. Convert dialect parse tree to canonical AST.
2. Apply remap transforms for all `REMAP` items.
3. Normalize operator semantics, casts, and function identities to canonical forms.
4. Preserve original source locations in `source_map`.

### Step 5: UUID binding
1. Resolve all persistent object names through catalog name registry using:
   - object type
   - schema path
   - session-language fallback rules
   - current transaction-local point helpers when uncommitted `DDL` could invalidate the bulk cache
2. Replace persistent object references with UUID identifiers.
3. Retain response alias metadata for egress formatting.

### Step 6: Security and DCL lowering
1. Lower security and DCL families from dialect-local syntax into canonical AST nodes.
2. Preserve security-significant fields exactly, including at minimum:
   - role names
   - grantee names
   - privilege lists
   - object paths
   - policy names
   - policy type and permissive or restrictive state
   - RLS action kind
   - domain security options
3. Do not infer authorization decisions during lowering.
4. Do not drop dialect-local unsupported clauses silently; reject or explicitly remap them.

### Step 7: SBLR emission
1. Emit `SBLR` payload from canonical AST.
2. Enforce:
   - UUID references only for persistent objects after binding
   - no parser-local semantic execution
3. Generate deterministic bytecode checksums.

### Step 8: Local pre-validation
1. Validate known opcode family.
2. Validate payload lengths and required fields.
3. Reject locally invalid payloads before IPC dispatch.

### Step 9: Build execute request
1. Construct engine execute request containing:
   - session and transaction identifiers
   - dialect id and profile version
   - original request text or command payload
   - normalized input
   - engine-visible SQL text for SQL surfaces
   - `SBLR` payload
   - source-map id
   - capability decision log
   - parser catalog anchor used for the request
2. Send request over parser-engine IPC contract.

### Step 10: Process engine response
1. Decode engine response.
2. If success, map result metadata and rows to the dialect response model.
3. If failure, map error via dialect error map and include diagnostics.

### Step 11: Render egress
1. Render names using session-language mapping with default-name fallback.
2. Apply dialect-specific identifier quoting and case rules.
3. Encode final wire-protocol response frame.
4. If exact current transaction-local rendered names are required, `sb_catalog_resolve_uuid_to_path_name` outranks cached snapshot or delta rows.

## Determinism rules
- No branch may rely on implicit parser heuristics.
- Missing capability profile entries are treated as `REJECT`.
- Missing name-resolution mappings are hard failures.
- Same input, same profile version, and same catalog state must emit byte-identical `SBLR`.
- Cache anchors must only advance at committed transaction boundaries.
- Security statement lowering must be local to the active parser family.
