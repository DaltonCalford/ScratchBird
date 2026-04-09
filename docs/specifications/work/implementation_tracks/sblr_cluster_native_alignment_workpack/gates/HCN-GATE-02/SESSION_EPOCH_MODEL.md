# Session Epoch Model (Gate Snapshot)

From HCN-022 closure:
- session state pins `cluster_config_epoch`, `schema_epoch`, `security_epoch`.
- validation contract supports strict reject or replan-required behavior.
- reason codes are deterministic and externally visible.

Validated by:
- `SessionEpochPinsTest.*`
- `CatalogSessionEpochPinningTest.*`
