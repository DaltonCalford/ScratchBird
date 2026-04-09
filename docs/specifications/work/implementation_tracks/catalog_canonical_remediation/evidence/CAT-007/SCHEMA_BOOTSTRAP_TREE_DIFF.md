# Schema Bootstrap Tree Diff

Ticket: `CAT-007`

## Removed From Fixed Bootstrap
- `root.app`
- `root.sys.sec`
- `root.sys.mon`
- `root.sys.agents`
- `root.sys.sec.srv`
- `root.remote.emulation.mssql`

## Added To Fixed Bootstrap
- `root.local`
- `root.local.instances`
- `root.local.links`
- `root.nosql`
- `root.nosql.cassandra`
- `root.nosql.mongodb`
- `root.nosql.neo4j`
- `root.nosql.redis`
- `root.nosql.milvus`
- `root.remote.fdw`
- `root.remote.links`
- `root.remote.emulation.cassandra`
- `root.remote.emulation.mongodb`
- `root.remote.emulation.neo4j`
- `root.remote.emulation.redis`
- `root.remote.emulation.milvus`
- `root.sys.information`
- `root.sys.system`
- `root.sys.monitor`
- `root.sys.config`
- `root.sys.jobs`
- `root.sys.security.auth`

## Legacy Repair Mapping
- `root.public` -> `root.users.public`
- `root.emulation` -> `root.remote.emulation`

## Invariants Enforced
- `root.schema_id == database_uuid`
- Missing canonical fixed nodes are created during load repair.
- Canonical fixed nodes are created in deterministic order.
