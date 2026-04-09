# Domain Event Log Schema

Ticket: HCN-034

## Record fields
- `cluster_config_epoch` (`uint64`, required, monotonic non-decreasing)
- `schema_epoch` (`uint64`, required, monotonic non-decreasing)
- `event_type` (`CREATE | ALTER | DROP`)
- `domain_id` (`UUID`, required, non-zero)
- `definition_hash` (`string`, required for `CREATE`/`ALTER`, optional for `DROP`)

## Apply semantics
1. Validate non-zero `domain_id` and non-zero epochs.
2. Reject if either epoch regresses against previously applied event.
3. For `CREATE`/`ALTER`: write `domain_hashes_[domain_id] = definition_hash`.
4. For `DROP`: erase `domain_hashes_[domain_id]`.
5. Append full event to `event_log_` for replay/audit visibility.

## Join manifest projection
- `exportManifest(...)` projects current `domain_hashes_` into `(domain_id, definition_hash)` entries.
- `validateJoinManifest(...)` compares remote manifest against local projection and reports deterministic mismatch reasons.
