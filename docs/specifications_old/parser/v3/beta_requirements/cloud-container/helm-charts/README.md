# Helm Charts Deployment Requirements

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Scope**
- Deployment and orchestration requirements for Helm Charts targets.

**Packaging Requirements**
- Stateless binaries and configuration must be separated from data directories.
- Environment variable and config file precedence must be deterministic.

**Runtime Requirements**
- **Ports:** Listener ports must be configurable; defaults must be documented in `docs/specifications/parser/v3/network/`.
- **Health Checks:** Provide liveness and readiness checks with clear failure semantics.
- **Logging:** Structured logs with timestamps and component tags; no interactive prompts.
- **TLS:** Support external TLS termination and in‑process TLS per `docs/specifications/parser/v3/Security Design Specification/`.

**Storage Requirements**
- Persistent volumes must be supported for data and WAL-like artifacts where applicable.
- Ensure safe shutdown hooks to flush catalog and page metadata.

**Conformance Tests**
- Cold start, warm restart, and crash recovery.
- Scale up/down with clean connection draining.
- Config reload without data corruption.

**Non-Goals**
- This document does not replace core engine specifications in `docs/specifications/parser/v3/`.
- This document does not define new SQL syntax beyond the dialect specifications.


