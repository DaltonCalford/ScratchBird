# Audit Export Sinks, Retention, and Immutability

Status: current_authority
Section owner: `20_Diagnostics_Audit_and_Observability`

## Current authority matrix

| Surface | Current state | Authority |
| --- | --- | --- |
| append-only local audit chain | current authority | `include/scratchbird/core/audit_logger.h`, `src/core/audit_logger.cpp` |
| deterministic local package export | current authority | `src/core/audit_logger.cpp` |
| deterministic local package validation | current authority | `src/core/audit_logger.cpp` |
| legal hold governance evidence | current authority | `src/core/audit_logger.cpp` |
| retention eligibility evaluation | current authority | `src/core/audit_logger.cpp` |
| remote database sink execution | fail closed | not current audited runtime authority |
| object or stream sink execution | fail closed | not current audited runtime authority |

## Canonical rule

Section `20` may claim only the local append-only chain, deterministic local
package export and validation, legal hold evidence, and retention-evaluation
behavior anchored in `AuditLogger`. It must not widen those anchors into proof
of complete remote or downstream sink execution parity.

## Redaction rule

Exported local audit packages and persisted governance payloads use the secure
redaction substrate. Secret-bearing text and endpoint authorities remain
redacted in operator-visible package content.

## Commit-truth rule

Downstream sink success is not part of commit truth. Audit export execution is
observability and governance behavior, not transaction-commit authority.

## Fail-closed boundaries

- no current proof of complete remote database delivery execution
- no current proof of complete object or stream export execution
- no current proof that downstream sink completion participates in transaction
  commit truth
