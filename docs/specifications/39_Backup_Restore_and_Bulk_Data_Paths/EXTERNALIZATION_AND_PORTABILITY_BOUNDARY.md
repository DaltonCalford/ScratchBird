# Externalization and Portability Boundary

This file owns the portability boundary for exported or backup artifacts.

## Portability matrix

| Topic | Current state | Current truth | Explicit exclusion |
| --- | --- | --- | --- |
| same-engine restore portability | current_bounded | portability claims may be made only for current proven restore flows | not cross-engine compatibility |
| cross-version portability | partial | cross-version portability remains bounded to explicit lifecycle proof | not universal forward/backward restore guarantees |
| logical export portability | partial | logical export portability may exist only for explicitly named formats or tools | not broad ecosystem compatibility |
| host/platform portability | fail_closed | no blanket artifact portability across every platform or deployment mode is implied | not a full archival interchange standard |

## Canonical rules

1. Portability language must stay narrower than availability of a file or dump format.
2. Cross-version and cross-platform claims require direct proof.
3. Cross-engine portability remains fail-closed unless explicitly owned elsewhere.

## Explicit non-guarantees

- no cross-engine backup compatibility
- no guaranteed cross-version restore for every artifact
- no archival interchange-standard claim
