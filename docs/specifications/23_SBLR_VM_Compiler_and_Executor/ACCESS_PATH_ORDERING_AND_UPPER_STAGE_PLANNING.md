# Access Path Ordering and Upper Stage Planning

Status: current_authority

Current authority:
- access-path descriptors and plan nodes in optimizer headers
- `query_planner.cpp`
- `join_ordering.cpp`
- `plan_payload.h`

## Current guarantees

- planning tracks exactness, visibility-enforcement, queryability, ordering, ordered-prefix, and parallel metadata where the current runtime populates them
- runtime-plan payloads can expose candidate families, exactness class, ordered-output properties, recheck requirements, and chosen path details

## Non-claims

- a fully closed upper-stage planning contract across every execution family
- a universal guarantee that every named path family has independent planning or runtime proof
