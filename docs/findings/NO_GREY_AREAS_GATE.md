# No-Grey-Areas Gate Checklist (V3)

This checklist enumerates remaining ambiguity sources that can mislead a low‑capability AI. Each item is closed only when the spec is updated with explicit, deterministic behavior or an explicit reject policy.

## Checklist

1. **V3 README optional-extension language**
   - Requirement: Explicitly define which dialects/features are supported by default and the exact reject behavior when disabled.
   - Status: Closed
   - Resolution: Added explicit reject policy in `docs/specifications/parser/v3/README.md`.

2. **Server architecture TBD registry format**
   - Requirement: Replace “format TBD” with a concrete registry format or an explicit “disabled in V3” rule and a deterministic error.
   - Status: Closed
   - Resolution: Defined registry as catalog-backed (`sys.registry`) with explicit enable/disable rules in `docs/specifications/parser/v3/SCRATCHBIRD_SERVER_ARCHITECTURE_CONSOLIDATED.md`.

3. **MySQL parser gap tracker TODOs**
   - Requirement: Replace TODOs with explicit emission/reject rules so the parser has deterministic behavior.
   - Status: Closed
   - Resolution: Converted TODO placeholders to explicit `ERR_FEATURE_UNSUPPORTED` emission in `docs/specifications/parser/v3/MYSQL_PARSER_IMPLEMENTATION_GAPS.md`.

4. **Transaction management design TODOs**
   - Requirement: Replace TODOs with authoritative rules or move to archive.
   - Status: Closed
   - Resolution: Converted to authoritative requirements in `docs/specifications/parser/v3/design/TRANSACTION_MANAGEMENT_DESIGN.md`.

5. **Sweep mechanism TODOs**
   - Requirement: Replace TODOs with authoritative rules or move to archive.
   - Status: Closed
   - Resolution: Implemented required foreground sweep algorithm in `docs/specifications/parser/v3/design/SWEEP_MECHANISM_DESIGN.md`.

6. **Data type persistence “Alpha/optional packed numeric” wording**
   - Requirement: Convert to V3 rules and explicit accept/reject behavior.
   - Status: Closed
   - Resolution: Removed Alpha/Beta wording and forbade packed NUMERIC in `docs/specifications/parser/v3/types/DATA_TYPE_PERSISTENCE_AND_CASTS.md`.

7. **Memory management “optional optional extension” WAL language**
   - Requirement: Normalize and define WAL behavior (disabled for V3 recovery).
   - Status: Closed
   - Resolution: Normalized WAL scope language in `docs/specifications/parser/v3/MEMORY_MANAGEMENT.md` and `docs/specifications/parser/v3/server/MEMORY_MANAGEMENT.md`.

8. **Temporary tables WAL language**
   - Requirement: Normalize and define WAL behavior (disabled for V3 recovery).
   - Status: Closed
   - Resolution: Normalized WAL scope language in `docs/specifications/parser/v3/TEMPORARY_TABLES_SPECIFICATION.md` and `docs/specifications/parser/v3/server/TEMPORARY_TABLES_SPECIFICATION.md`.

9. **Index specs with optional WAL/GPU/rebuilds**
   - Requirement: Mark each optional feature as configurable with defaults and explicit reject/ignore behavior.
   - Status: Closed
   - Resolution: Added V3 optional extension policy in `docs/specifications/parser/v3/indexes/INDEX_ARCHITECTURE.md`.

10. **Character sets/collations “optional ICU integration”**
   - Requirement: Define required fallback when ICU is not enabled.
   - Status: Closed
   - Resolution: Added ICU fallback/reject rules in `docs/specifications/parser/v3/types/character_sets_and_collations.md`.

11. **NoSQL language tracker “Draft” status**
   - Requirement: Replace status with authoritative or move to archive if not enforced.
   - Status: Closed
   - Resolution: Added explicit reject policy in `docs/specifications/parser/v3/beta_requirements/nosql/NOSQL_LANGUAGE_SPEC_TRACKER.md`.

12. **Replication tracker draft statuses**
   - Requirement: Replace status with authoritative or move to archive if not enforced.
   - Status: Closed
   - Resolution: Added explicit reject policy in replication index docs under `docs/specifications/parser/v3/beta_requirements/replication/uuidv7-optimized/`.

13. **Core parser specs with “optional extension” dialects**
   - Requirement: Explicit reject behavior when disabled, no silent acceptance.
   - Status: Closed
   - Resolution: Added mandatory reject rule in `docs/specifications/parser/v3/parser/EMULATED_DATABASE_PARSER_SPECIFICATION.md` and V3 README.
