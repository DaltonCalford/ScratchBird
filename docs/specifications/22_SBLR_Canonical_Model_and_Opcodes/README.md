# Section 22: SBLR Canonical Model and Opcodes

Status: current_authority

## Purpose

This section defines the canonical ScratchBird Logical Representation, SBLR, contract that all SQL parsers, renderers, verifiers, and compiler front doors must use. SBLR is the durable logical interchange layer between dialect-specific parsing and shared engine execution planning.

## Core invariants

- SBLR is the canonical parser-to-engine interchange format.
- Parsers must not hand unresolved catalog object names to the engine when durable object identity is required.
- Durable object references in SBLR are resolved through catalog helper contracts, not by engine-side name guessing.
- Parser sessions begin from a committed catalog baseline obtained through sb_catalog_snapshot_begin.
- Parser sessions refresh committed catalog state through sb_catalog_delta_since_anchor at new-transaction boundaries, or after a successful autocommit command when the next transaction begins.
- Point lookup helpers sb_catalog_resolve_name_to_uuid and sb_catalog_resolve_uuid_to_path_name remain authoritative for single-object resolution and rendering.
- Uncommitted local DDL overlays remain parser-session local until commit publication.
- The verifier is mandatory; an SBLR container that fails verification must not be executed.
- The normalized retained-symbol substrate is part of canonical SBLR authority
  for Beta 1 language-lane closure; the current inline retained-name substrate
  is compatibility input, not sufficient final scope

## Scope

This section owns:

- the canonical SBLR container and serialization model
- opcode family ownership and extension rules
- statement payload schemas
- domain payload schemas used by current v3 paths
- feature-to-opcode mapping
- verifier requirements
- parser normalization requirements before SBLR emission

## Out of scope

This section does not own:

- compiler, planner, and executor runtime behavior; see section 23
- catalog publication and schema epoch rules; see section 24
- parser dialect behavior and query-to-SBLR translation pipeline ownership; see section 28
- transaction publication rules; see section 08

## Direct audit lookup anchors

- `src/sblr/v3_container.cpp` search key `SECTION_RETAINED_SYMBOLS`
- `src/sblr/v3_validator.cpp` search key `validateRetainedSymbolPayload(`
- `src/parser/v3_emitter.cpp` search key `buildNormalizedRetainedSymbolPayload(`
- `src/sblr/ast_sblr_lowerer.cpp` search key `buildNormalizedRetainedSymbolPayload(`

## Canonical files

<!-- AUTO-GENERATED:FILE-LIST:START -->
- [AST_TO_SBLR_EMISSION_RULES.md](AST_TO_SBLR_EMISSION_RULES.md)
- [BETA2_DONOR_DIALECT_SBLR_PAYLOAD_AND_OPCODE_EXPANSION_MODEL.md](BETA2_DONOR_DIALECT_SBLR_PAYLOAD_AND_OPCODE_EXPANSION_MODEL.md)
- [BETA2_FUNCTION_SURFACE_SBLR_COMPLETION_MODEL.md](BETA2_FUNCTION_SURFACE_SBLR_COMPLETION_MODEL.md)
- [BETA2_SBLR_TYPE_PAYLOAD_AND_EXTRACT_SET_EXTENSION_MODEL.md](BETA2_SBLR_TYPE_PAYLOAD_AND_EXTRACT_SET_EXTENSION_MODEL.md)
- [DECISION_RECORD.md](DECISION_RECORD.md)
- [DEPENDENCIES.md](DEPENDENCIES.md)
- [ERROR_ENVELOPE_UUID_DETAIL_CHAIN_AND_TRANSPORT_MODEL.md](ERROR_ENVELOPE_UUID_DETAIL_CHAIN_AND_TRANSPORT_MODEL.md)
- [PARSER_NORMALIZATION_TO_SBLR_VALIDATION_MATRIX.md](PARSER_NORMALIZATION_TO_SBLR_VALIDATION_MATRIX.md)
- [SBLR_CANONICAL_NAME_METADATA_SCOPE_AND_ERASURE_BOUNDARY.md](SBLR_CANONICAL_NAME_METADATA_SCOPE_AND_ERASURE_BOUNDARY.md)
- [SBLR_CANONICAL_SERIALIZATION_AND_CONTAINER.md](SBLR_CANONICAL_SERIALIZATION_AND_CONTAINER.md)
- [SBLR_DOMAIN_PAYLOADS_V3.md](SBLR_DOMAIN_PAYLOADS_V3.md)
- [SBLR_EXPRESSION_OPERATOR_AND_TYPE_MAPPING.md](SBLR_EXPRESSION_OPERATOR_AND_TYPE_MAPPING.md)
- [SBLR_FEATURE_TO_OPCODE_MATRIX.md](SBLR_FEATURE_TO_OPCODE_MATRIX.md)
- [SBLR_IDENTIFIER_AND_SYMBOL_PAYLOAD_EXPANSION_FOR_V3_RENDERING.md](SBLR_IDENTIFIER_AND_SYMBOL_PAYLOAD_EXPANSION_FOR_V3_RENDERING.md)
- [SBLR_NAME_SYMBOL_AND_CONTEXT_RETENTION_EXPANSION.md](SBLR_NAME_SYMBOL_AND_CONTEXT_RETENTION_EXPANSION.md)
- [SBLR_OPCODE_FAMILIES_AND_SYMBOLS.md](SBLR_OPCODE_FAMILIES_AND_SYMBOLS.md)
- [SBLR_STATEMENT_PAYLOAD_SCHEMAS.md](SBLR_STATEMENT_PAYLOAD_SCHEMAS.md)
- [SBLR_SYMBOL_NAME_SLOT_AND_RENDER_HINT_MODEL.md](SBLR_SYMBOL_NAME_SLOT_AND_RENDER_HINT_MODEL.md)
- [SBLR_TO_V3_NAME_SYMBOL_AND_CONTEXT_RECOVERY_PAYLOAD_MODEL.md](SBLR_TO_V3_NAME_SYMBOL_AND_CONTEXT_RECOVERY_PAYLOAD_MODEL.md)
- [SBLR_TO_V3_NATIVE_SQL_RENDERING_AND_DETERMINISM_MODEL.md](SBLR_TO_V3_NATIVE_SQL_RENDERING_AND_DETERMINISM_MODEL.md)
- [SBLR_TO_V3_RENDERING_AND_FIDELITY_RULES.md](SBLR_TO_V3_RENDERING_AND_FIDELITY_RULES.md)
- [SBLR_V3_CODEC_AND_CANONICALIZATION_RULES.md](SBLR_V3_CODEC_AND_CANONICALIZATION_RULES.md)
- [SBLR_VERIFIER_AND_VALIDATION_RULES.md](SBLR_VERIFIER_AND_VALIDATION_RULES.md)
- [SPEC_OUTLINE.md](SPEC_OUTLINE.md)
- [TEST_CONTRACT.md](TEST_CONTRACT.md)
<!-- AUTO-GENERATED:FILE-LIST:END -->
