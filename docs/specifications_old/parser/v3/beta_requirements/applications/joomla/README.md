# Joomla Application Compatibility Requirements

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Scope**
- Application compatibility contract for Joomla.

**Target Dialect**
- The deployment must select either MySQL or PostgreSQL compatibility mode.
- All SQL emitted by the application must be parsed by the selected dialect parser and emitted into SBLR V3.

**Required Features (Dialect-Neutral)**
- CREATE/ALTER/DROP TABLE and INDEX.
- Primary key, foreign key, and unique constraints.
- Transactions with READ COMMITTED semantics.
- LIMIT/OFFSET pagination.
- UTF‑8 identifiers and UTF‑8 text storage.

**Operational Requirements**
- Connection pooling must be safe under MGA and lock ordering rules.
- Schema migrations must be fully transactional.

**Conformance Tests**
- Bootstrap install using the selected dialect.
- Upgrade/migration path.
- Full application test suite with zero SQL errors.

**Non-Goals**
- This document does not replace core engine specifications in `docs/specifications/parser/v3/`.
- This document does not define new SQL syntax beyond the dialect specifications.


