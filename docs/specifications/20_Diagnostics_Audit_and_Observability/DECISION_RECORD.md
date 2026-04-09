# Decision Record - 20_Diagnostics_Audit_and_Observability

## Scope
- audit chain integrity and local export packages
- metric contract policy and SQL-view inventory
- secure diagnostics redaction
- bounded recovery, writeback, buffer, MGA, and page-finding observability

## Decisions
- `AuditLogger` local append-only chain, deterministic local package export and validation, legal hold, and retention evaluation are canonical section `20` runtime truth.
- `MetricContractPolicy` plus `MgaObservabilityContract` define the current canonical `sb_*` metric contract.
- `ObservabilityContract` defines the current privileged SQL-view inventory for MGA, buffer, checkpoint, recovery, writeback, and sweep-resume surfaces.
- secure diagnostics redaction is mandatory for structured logs and exported local audit package payloads.
- section `20` currently proves bounded page-finding and repair-required visibility, not a broader standalone page-walker subsystem.
- section `20` does not currently prove an independent `sb_btree_*` operator observability surface.

## Open gaps
- broader remote sink execution parity
- independent `sb_btree_*` view and alert surfaces
- broader standalone page-walker service or control-plane ownership
- privileged de-redaction workflows beyond the current fail-closed redaction substrate
