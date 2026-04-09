# Spec Outline - 20_Diagnostics_Audit_and_Observability

## Purpose
Define the code-backed audit, metric, view, redaction, and bounded page-finding
surfaces that operators can use to diagnose ScratchBird runtime behavior.

## Authoritative Files
- `AUDIT_EXPORT_SINKS_RETENTION_AND_IMMUTABILITY.md`
- `STORAGE_METRICS.md`
- `BUFFER_CACHE_OBSERVABILITY.md`
- `MGA_OBSERVABILITY_AND_OPERATOR_DIAGNOSTICS.md`
- `RECOVERY_AND_CHECKPOINT_OBSERVABILITY.md`
- `PAGE_WALKER_AND_REPAIR.md`
- `TEST_CONTRACT.md`

## Scope
- append-only audit chain, deterministic local export package, legal hold, and retention evaluation
- canonical `sb_*` metric naming and policy audit
- privileged SQL views for MGA, buffer, checkpoint, recovery, sweep-resume, and writeback state
- secure diagnostics redaction for structured logs and audit payloads
- bounded page-corruption findings and repair-required visibility

## Hard invariants
1. local audit evidence is authoritative; downstream export never replaces it
2. observability describes runtime state but does not replace transaction, recovery, or storage truth owned by neighboring sections
3. secret-bearing diagnostics text and fields remain redacted across structured logs and local audit packages
4. independent B-tree operator observability remains fail closed until section-owned runtime proof exists

## Dependencies
- Upstream: `08`, `10`, `18`, `19`, `24`.
- Downstream: `30`, `31`.

## Completeness criteria
1. current audit and export surfaces are described without implying unproven remote sink execution
2. current `sb_*` metric and SQL-view surfaces are explicit
3. redaction and integrity boundaries are explicit
4. page-corruption and repair-required observability is explicit without overclaiming a broader page-walker subsystem
