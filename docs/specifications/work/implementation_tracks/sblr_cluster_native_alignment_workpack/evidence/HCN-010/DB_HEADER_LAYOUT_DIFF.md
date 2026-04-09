# DB Header Layout Diff

## Delta

Added fields in `DatabaseHeader`:
- `UuidV7Bytes cluster_id`
- `UuidV7Bytes node_id`
- `uint64_t cluster_config_epoch`

Updated API surface:
- `Database::cluster_id()`
- `Database::node_id()`
- `Database::cluster_config_epoch()`
- `Database::set_cluster_identity(...)`

## Persistence Path
- Values initialized in `init_header_page(...)`.
- Values updated and persisted through page-0 pin/update/sync in `set_cluster_identity(...)`.
