# Command Group: UTILITY + BRIDGE
Last modified: 2026-02-19

Back links:
- [Language Guide README](../README.md)
- [Command Group Index](README.md)

Series navigation:
- Previous: [SHOW](show.md)

Native v3 `parseStatementInternal()` also dispatches non-CRUD command surfaces that do not start with `CREATE`, `ALTER`, `DROP`, `SELECT`, `SET`, or `SHOW`.

## DDL/DCL Utility Surfaces

- `DECLARE EXTERNAL FUNCTION <name> ... ENTRY_POINT ... MODULE_NAME ...;`
- `INSTALL EXTENSION <name>;`
- `LOAD EXTENSION <name>;`
- `SECURITY LABEL [FOR <provider>] ON <object_type> <object_path> IS <label|NULL>;`
- `REVOKE TOKEN <token_name> [FROM USER <user_name>];`

## UDR Compile Surfaces

- `COMPILE UDR <name> ...;`
- `UDR COMPILE <name> ...;`
- `VALIDATE EMBEDDED SQL ...;`

## Query/Bridge Surfaces

- `DOC PATH FILTER ...;`
- `TS BUCKET AGG ...;`
- `SEARCH DSL ...;`
- `VECTOR ANN ...;`
- `HYBRID BRIDGE EXCHANGE ...;`
- `GRAPH PATH MATCH ...;`
- `MATCH GRAPH PATH ...;`
- `EVAL LUA ...;`
- `REDIS LUA EVAL ...;`
- `XGROUP CREATE ...;`
- `XREADGROUP STREAM ...;`
- `XCLAIM STREAM ...;`

## Admin/Operations Surfaces

- `BACKUP ...;`
- `RESTORE ...;`
- `VALIDATE DATABASE;`
- `SWEEP DATABASE;`
- `VACUUM;` / `VACUUM DATABASE;`
- `RESYNC REPLICATION CHANNEL <channel_name>;`
- `CLUSTER ...;`
- `CUBE ...;`
- `SERVICE CHANNEL ...;`
- `EXECUTE JOB <job_name|job_id> ...;`
- `CANCEL JOB RUN <run_id>;`

Coverage details for lifecycle and semantics are in:
- [Admin](../admin/README.md)
- [DDL](../ddl/README.md)
- [DML](../dml/README.md)
- [PSQL](../psql/README.md)
