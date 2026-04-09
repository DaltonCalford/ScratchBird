# Event Schema Registry

Structured event JSON fields:
- `event_id`
- `event_type`
- `severity`
- `occurred_at_ms`
- `cluster_config_epoch`
- `schema_epoch`
- `security_epoch`
- `db_uuid`
- `node_id`
- `shard_id`
- `message`
- `payload`

Registry behavior:
- event types are de-duplicated and sorted deterministically.
