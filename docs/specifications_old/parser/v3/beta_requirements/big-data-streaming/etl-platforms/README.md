# Etl Platforms Connector Requirements

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Scope**
- Integration requirements for Etl Platforms connectors.

**Connectivity**
- Must support JDBC and/or native wire protocol connections.
- Bulk read/write operations must be chunked deterministically.

**Pushdown Rules**
- Predicate pushdown is allowed only for expressions that map directly to SBLR opcodes.
- Unsupported predicates must be evaluated client-side with a clear marker.

**Fault Tolerance**
- Reads must be restartable from stable offsets.
- Writes must be idempotent or have defined retry semantics.

**Conformance Tests**
- Batch read with predicate pushdown.
- Streaming read with resume after interruption.
- Bulk insert with rollback on error.

**Non-Goals**
- This document does not replace core engine specifications in `docs/specifications/parser/v3/`.
- This document does not define new SQL syntax beyond the dialect specifications.


