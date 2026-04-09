# DB Header Layout Diff (Gate Snapshot)

From HCN-010 closure:
- Added `cluster_id`, `node_id`, `cluster_config_epoch` to `DatabaseHeader`.
- Added persisted update API `Database::set_cluster_identity(...)`.

Validation anchor:
- `DatabaseClusterIdentityTest.*` passed.
