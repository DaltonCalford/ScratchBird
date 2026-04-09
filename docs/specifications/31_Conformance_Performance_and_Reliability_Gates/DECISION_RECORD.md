# Section 31 Decision Record

## Current decisions

1. Gate claims are valid only when backed by maintained evidence artifacts and defined stage policy.
2. Current gate authority is bounded by maintained runners, maintained corpora, and maintained artifact schemas.
3. Unsupported or unmaintained gate families are documented as fail-closed boundaries, not provisional promises.
4. Performance claims must state corpus, environment class, metric contract, and acceptance threshold.
5. Reliability and recovery claims must reflect the current MGA engine model and current restart semantics.
6. Platform and compatibility claims must be limited to the explicitly documented certification scope.

## Rejected interpretations

- Treating inventory spreadsheets or planning matrices as proof of executed certification.
- Treating benchmark anecdotes as release-significant evidence.
- Treating replication, cluster, forensic, or rollback ambitions as current release guarantees without maintained gate lanes.
