# Vector Apis Integration Requirements

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Scope**
- Vector search integration requirements.

**Required Engine Capabilities**
- **Vector Types:** Must support vector storage and retrieval using canonical type encodings in `docs/specifications/parser/v3/types/`.
- **Similarity Functions:** Provide deterministic scoring functions and ordering guarantees.
- **Index Support:** Vector indexes must follow the index specs under `docs/specifications/parser/v3/indexes/`.

**Query Contract**
- Vector search must accept limit/offset and return stable ordering on tie.
- Queries must be safe under MGA visibility rules.

**Conformance Tests**
- Insert vector rows, build index, run similarity queries, verify ordering.
- Validate vector dimensionality checks and error handling.

**Non-Goals**
- This document does not replace core engine specifications in `docs/specifications/parser/v3/`.
- This document does not define new SQL syntax beyond the dialect specifications.


