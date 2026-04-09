# Dependencies - 20_Diagnostics_Audit_and_Observability

## Upstream Dependencies
- `08_Transaction_Core`
- `10_GC_and_Sweep`
- `18_Index_Framework`
- `19_Security_Model`
- `24_Catalog_Model_and_Virtual_Overlays`

## Downstream Dependents
- `30_Client_Tooling`
- `31_Conformance_Performance_and_Reliability_Gates`

## Cross-Section Contracts
1. sections `08` and `10` own the transaction, checkpoint, recovery, and sweep state that section `20` exposes.
2. section `19` owns security policy and forensic governance boundaries; section `20` owns audit-chain, redaction, and observability runtime surfaces tied to that policy.
3. section `24` owns the catalog families that persist audit segments, recovery incidents, writeback incidents, and related run metadata.
4. section `18` owns B-tree semantics and lifecycle; section `20` does not currently prove an independent B-tree operator surface beyond bounded generic observability.
5. section `31` consumes section `20` surfaces as gate evidence.
