# Section 34 Test Contract

Section `34` is implementation-ready only if maintained evidence covers the
current access-method behaviors it claims.

## Required certification lanes

- registry and scope
  - supported access-method families are enumerated deterministically
  - unsupported or experimental families fail closed
- heap and primary storage
  - row storage, primary scan behavior, and maintenance interaction obey the
    documented current contract
- B-tree and secondary access
  - secondary index creation, lookup, and maintenance obey the documented
    current B-tree authority
  - admitted named non-B-tree families prove deterministic create/open or
    routing behavior where section `18` marks them current
  - generalized planner or maintenance parity across all non-B-tree families is
    still not claimed unless explicitly certified
- DDL and DML interaction
  - access-method state changes obey current transaction, DDL, and maintenance
    rules from adjacent sections
- maintenance and recovery boundaries
  - fail-closed behavior is deterministic when an access method is absent,
    degraded, or not certified for a path

## Negative requirements

- no test may assume generalized planner parity across all access methods unless
  section `34` says so explicitly
- no test may treat specialized or experimental paths as fully production-equal
  to the primary heap or B-tree paths unless explicitly stated
