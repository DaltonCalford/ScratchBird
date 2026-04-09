# HCN-034 Evidence Bundle

Implements domain control-plane replication and join hash validation for PH3.

Coverage:
- Domain control-plane event log append contract (`CREATE`, `ALTER`, `DROP`)
- Monotonic epoch validation for `cluster_config_epoch` and `schema_epoch`
- Domain manifest export and join-time hash/identity validation
- Deterministic mismatch reason codes for repair tooling
