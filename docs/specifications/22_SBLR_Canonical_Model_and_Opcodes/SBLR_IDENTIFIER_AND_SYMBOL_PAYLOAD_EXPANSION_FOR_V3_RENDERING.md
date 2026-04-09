# SBLR_IDENTIFIER_AND_SYMBOL_PAYLOAD_EXPANSION_FOR_V3_RENDERING

## Status

Required reconstructed specification with current code-backed boundary.

## Purpose

This document defines the missing payload-expansion rules needed so SBLR can be rendered or transformed back into canonical native/v3 SQL without guesswork.

## Current code-backed boundary

The current renderer can render deterministic native SQL only when the instruction payload already carries enough user-visible information, such as:

1. names
2. schema-path components
3. classifier keys
4. literal values
5. mode or metric identifiers

When those symbolic fields are absent, the current renderer is forced to use:

1. UUID resolution
2. generic placeholders such as `<expr>`
3. contract-specific defaults

That is not sufficient for a full non-guessing rebuild target.

## Canonical expansion rule

Canonical SBLR shall explicitly carry the user-visible symbols needed for native/v3 reconstruction.

At minimum this includes:

1. variable names
2. parameter names
3. column aliases
4. table aliases
5. routine argument labels
6. named option keys
7. schema-path components
8. user-visible object names when the opcode semantics depend on them
9. classifier keys used to distinguish one render contract from another

The required retained-symbol set also includes, where applicable:

10. CTE names
11. cursor names
12. loop and block labels
13. exception or condition labels
14. security-principal names such as user, role, and group names
15. grantee and grantor display names when the statement surface is security-
    visible
16. savepoint or transaction-local label names when the V3 surface exposes them
17. package-member display names
18. active-role or session-authorization target names

## Target-language rule

These payload expansions exist to support rendering into canonical native/v3 SQL.

They do not exist to preserve original source dialect formatting or original parser-specific syntax.

## Resolver boundary

UUID-based name resolution is still useful, but it is a resolver aid, not a substitute for canonical payload completeness.

If a later agent wants a limited AI to render or transform SBLR without guessing, the symbolic payload shall already contain enough information to do so even when the resolver is unavailable.

Catalog resolution may recover only durable UUID-backed object identity.

It is not an acceptable substitute for:

1. local symbol names
2. alias names
3. security-principal display names when the principal is not recovered solely
   from durable UUID authority
4. any parser-surface name that affects canonical V3 text but is not stored as a
   durable catalog object

## Expression boundary

Expression payloads that need stable textual reconstruction shall carry enough symbolic information so the renderer does not collapse to `<expr>` except for explicitly unsupported or opaque expression families.

## Parser isolation rule

These retained symbols must be parser-independent canonical payloads.

They shall not depend on:

1. cross-parser helper tables
2. donor-parser-specific opaque blobs
3. one parser reusing another parser's private lowering metadata

Every parser must lower its own SQL to canonical SBLR, and the converter must
recover V3 from canonical payload plus canonical resolver inputs only.

## Fail-closed rule

If a required user-visible symbol is absent from:

1. retained symbol payload
2. inline statement payload
3. local overlay
4. durable UUID resolution

then the converter must fail closed.

Missing symbol payload is a canonical schema deficiency, not permission to
invent or infer text.

## Required implementer interpretation

Another agent extending the canonical SBLR container shall treat missing symbolic payload as a schema deficiency, not as permission to infer or hallucinate names during rendering.
