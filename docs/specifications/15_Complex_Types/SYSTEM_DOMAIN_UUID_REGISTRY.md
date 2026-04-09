# System Domain UUID Registry

Status: current_authority

## Current authoritative registry mechanism

The current authoritative surface is the deterministic system-domain ID mechanism in DomainManager, not the historical exhaustive static registry.

## Current audited registry matrix

| Audit area | Current authority | Main boundary |
| --- | --- | --- |
| deterministic ID generation | deterministicSystemDomainId computes stable IDs from system-domain identity inputs | one machine-readable export is still missing |
| bootstrap enforcement | ensureSystemDomains validates and enforces system-domain presence during bootstrap | negative-path coverage still needs tighter centralization |
| canonical and legacy compatibility | canonical and legacy compatibility handling is real in bootstrap path | exhaustive legacy breadth remains unproven |
| directly re-audited examples | sb_dom uuid_v7_internal, legacy UUID_V7, and parent-linked key-domain bootstrap behavior are re-audited examples | example-level proof is not exhaustive registry proof |
| historical exhaustive static rows | historical prose exists only as carryover evidence | not current proof until re-audited row by row |

## Current fail-closed interpretation

- the historical exhaustive UUID list is not treated as fully re-audited current truth
- only the deterministic-ID mechanism and directly re-audited examples in this pass are authoritative here
- any additional static registry rows require line-by-line re-audit before promotion back into current canonical truth
