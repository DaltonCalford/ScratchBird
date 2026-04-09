# Test Contract

Section `24` is implementation-ready only if maintained evidence covers:

- database startup initializes virtual catalog registration through the current catalog startup path
- `emulation_profile` rows gate engine-specific overlay handler registration where documented
- `information_schema`, `sys_catalog`, and currently supported engine-specific overlay surfaces are queryable through the virtual catalog layer
- charset and timezone loaders populate currently supported resource rows and lookup paths
- persisted catalog family inventory matches the documented current groups
- config key/value/history and dedicated listener-topology families materialize
  through the catalog bootstrap path and remain catalog-root-backed after reopen
- target-local durable configuration truth stays split between scalar config
  rows and dedicated listener-topology rows
- metadata invalidation and schema-publication paths remain commit-bound and MGA-consistent
- branch and changeset narratives remain fail-closed unless explicitly promoted
- Beta 2 external snapshot, manifest, schema-version, and pointer rows
  materialize with deterministic overlay visibility and quarantine behavior
- Beta 2 distributed-query node-capability, locality-cost, exchange-policy, and
  queue-state rows materialize with deterministic visibility and bounded-staleness
  disclosure
- Beta 2 temporal table, history binding, and period-policy rows materialize
  with deterministic visibility and schema-lockstep enforcement
- Beta 2 graph catalog, vertex binding, and edge binding rows materialize with
  UUID-stable identity and deterministic overlay visibility
- Beta 2 shard-policy, shard-range, shard-placement, shard-migration, and
  cutover-fence rows materialize with epoch-consistent history
- Beta 2 cube refresh, freshness-watermark, rewrite-contract, and job rows
  materialize with deterministic operator visibility

This section does not require proof of unsupported donor-parity or branch-model narratives.
