# Native Infrastructure SQL

## Current code-backed truth
- The native parser exposes real extension-family parse entry points for schedule, connection-rule, token, quota-profile, extension, replication-channel, publication, subscription, CDC table, cube-control, access-method, statistics, transform, and cluster-control surfaces.
- Public beta gate tests prove at least part of the cluster-control and replication parser surface.

## Proven anchors
- `include/scratchbird/parser/parser_v3.h`
- `tests/conformance/public_beta/run_required_public_beta_gate.sh`

## Boundary
- Parser entry points are code-backed.
- Full runtime parity for each infrastructure statement family is not section-owned here and remains partial.
- Treat this file as a bounded parser-front-door inventory until a deeper contradiction pass closes each family.
