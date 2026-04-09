# Session Epoch Model

Pinned session epoch tuple:
- `cluster_config_epoch`: cluster topology/routing config version at session bind time.
- `schema_epoch`: schema contract version at session bind time.
- `security_epoch`: policy/authz version at session bind time.

Validation contract:
- Compare current tuple to pinned tuple at execution entry.
- On mismatch:
  - reject mode -> `Status::INVALID_TRANSACTION_STATE` + reason code.
  - replan mode -> valid status, `requires_replan=true`, reason code set.

Reason codes:
- `cluster_config_epoch_mismatch`
- `schema_epoch_mismatch`
- `security_epoch_mismatch`
