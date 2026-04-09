Status: current_authority_beta2

# Beta 2 Emulated Donor Mapping and Shared Lowering Model

## Purpose

Define how every emulation-target donor family maps into the shared Beta 2
native-v3, AST, SBLR, and execution structures so emulated parsers become
translation layers only, not private semantic runtimes.

## Hard invariants

1. Every donor parser family shall lower into the shared AST defined in section
   `21`, the shared SBLR payloads defined in section `22`, and the shared
   runtime bindings defined in section `23`.
2. Donor parsers shall not invent donor-private AST nodes, donor-private SBLR
   opcodes, or donor-private executor semantics for a surface admitted in the
   Beta 2 shared model.
3. The native v3 parser shall admit shared donor constructs directly once they
   are promoted into Beta 2 canon. The emulated parser remains necessary for
   donor wire protocol, result shaping, error shaping, catalog overlays, and
   donor-specific spellings, not because the core AST lacks the semantics.
4. Any donor surface not backed by local source evidence remains fail-closed and
   must not be speculated into parser behavior.

## Beta 2 target family set

The Beta 2 parser target set for shared-lowering readiness is:

- `native`
- `firebird`
- `postgresql`
- `mysql`
- `mariadb`
- `sqlite`
- `cassandra`
- `mongodb`
- `neo4j`
- `redis`
- `milvus`
- `opensearch`
- `clickhouse`
- `influxdb`
- `foundationdb`
- `ignite`
- `cockroachdb`
- `citus`
- `dolt`
- `duckdb`
- `immudb`
- `tidb`
- `vitess`
- `xtdb`
- `yugabytedb`

## Shared mapping buckets

### 1. Desugar-only native-v3 admission

Mapped donors:

- Apache Ignite: `APPLY`
- Cassandra: `CREATE MATERIALIZED VIEW`
- MySQL, MariaDB, Vitess, TiDB: `REPLACE`, `INSERT IGNORE`,
  `ON DUPLICATE KEY UPDATE`
- CockroachDB, immudb: `UPSERT`
- PostgreSQL family: `CREATE MATERIALIZED VIEW`

Canonical target:

- section `21` `InsertSurfaceFlavor`
- existing `CreateViewStmt.materialized`
- existing join model with `lateral = true`

### 2. Shared select and table-source expansion

Mapped donors:

- Apache Ignite: `QUALIFY`, `PIVOT`, `UNPIVOT`, `MATCH_RECOGNIZE`
- ClickHouse: `QUALIFY`, `PREWHERE`, `ARRAY JOIN`, `LIMIT BY`, `FINAL`,
  statement-local `SETTINGS`
- DuckDB: `QUALIFY`, `PIVOT`, `UNPIVOT`
- PostgreSQL, Citus, YugabyteDB: `JSON_TABLE`, `ROWS FROM (...)`
- MySQL, MariaDB, Vitess: `JSON_TABLE`, `QUALIFY`
- CockroachDB, Dolt, TiDB, XTDB: temporal `AS OF` or
  `FOR VALID_TIME`/`FOR SYSTEM_TIME`

Canonical target:

- section `21` `SelectStmt.qualify`, `SelectStmt.prewhere`,
  `SelectStmt.array_join_items`, `SelectStmt.limit_by`,
  `SelectStmt.query_settings`
- section `21` `TableRefNode` extensions for `JSON_TABLE`, `ROWS_FROM`,
  `PIVOT`, `UNPIVOT`, `MATCH_RECOGNIZE`, `TemporalClause`, and `final`

### 3. Shared function and window metadata expansion

Mapped donors:

- PostgreSQL, Citus, YugabyteDB: `VARIADIC`, named arguments,
  ordered-set aggregates, extra window functions
- FirebirdSQL: ordered-set aggregates, `FROM FIRST`, `FROM LAST`,
  extra window functions
- MariaDB: ordered-set percentile functions, extra window functions
- DuckDB: `VARIADIC`, ordered-set aggregates, null-treatment support
- MySQL, TiDB, Vitess: extra window functions, `IGNORE NULLS`,
  `RESPECT NULLS`, `FROM FIRST`, `FROM LAST`
- Apache Ignite: ordered-set aggregate decision logic

Canonical target:

- section `21` `FunctionArgumentItem`
- section `21` `OrderedSetAggregateSpec`
- section `21` `WindowBehaviorSpec`
- section `22` `SblrFunctionCallPayloadV2`

### 4. Distribution, placement, and policy DDL

Mapped donors:

- Cassandra: keyspace surfaces
- CockroachDB: locality, placement, alter-range, changefeed, scrub
- TiDB: placement policy, split-region, flashback-adjacent control surfaces
- YugabyteDB: tablegroup, tablets, co-location, split or hash policy surfaces

Canonical target:

- section `21` `PolicySurfaceStmt`
- section `22` structured policy payloads
- section `23` DDL/control plan nodes

### 5. Session, history, and metadata show surfaces

Mapped donors:

- immudb: `USE DATABASE`, `USE SNAPSHOT ...`, `HISTORY OF`
- InfluxDB: `SHOW MEASUREMENTS`, `SHOW FIELD KEYS`, `SHOW TAG KEYS`,
  `SHOW TAG VALUES`
- Dolt: `AS OF` on `SHOW`, `SELECT`, and `CALL`

Canonical target:

- section `21` `UseDatabaseStmt`
- section `21` `SetSnapshotScopeStmt`
- section `21` `HistoryQueryStmt`
- section `21` `ShowMetadataSurfaceStmt`

### 6. Multi-model command exposure and expansion

Mapped donors:

- Cassandra: existing CQL batch or selector command exposure
- MongoDB: existing `find`, `aggregate`, `findAndModify`, `bulkWrite`, plus
  new `count`, `distinct`, `createIndexes`, `listCollections`
- Neo4j: Cypher command exposure with shared lowering
- Redis: existing PubSub plus `MULTI`, `EXEC`, `DISCARD`, `WATCH`, `UNWATCH`
- Milvus: existing core verbs plus `HybridSearch`
- OpenSearch: bulk, multi-search, and index-admin command surfaces
- FoundationDB: CLI control verbs such as coordinator, distribution,
  maintenance, throttle, and versionepoch

Canonical target:

- section `21` `MultiModelCommandStmt`
- section `22` `SblrMultiModelCommandPayload`
- section `23` `MULTI_MODEL_COMMAND_DISPATCH`

## Per-donor shared-structure contract

- `Apache Ignite`: maps to native desugar, select or table-source expansion, and
  ordered-set metadata. No Ignite-private AST is allowed.
- `Cassandra`: maps to materialized-view admission, policy DDL, and shared
  CQL command envelopes.
- `Citus`: inherits PostgreSQL shared buckets only; Citus UDF surfaces remain
  normal `CALL` or function-call lowering.
- `ClickHouse`: maps to the select-pipeline modifier block and explain-node
  families defined in sections `21` and `23`.
- `CockroachDB`: maps to temporal binding, insert-surface flavor, and policy
  DDL.
- `Dolt`: maps to temporal binding plus existing call or function carriers.
- `DuckDB`: maps to `QUALIFY`, `PIVOT`, `UNPIVOT`, and structured function-call
  metadata.
- `FirebirdSQL`: maps to shared ordered-set and generic window-function
  identity.
- `FoundationDB`: maps only through the shared multi-model command family.
- `immudb`: maps through session-snapshot and history carriers plus insert
  surface flavor.
- `InfluxDB`: maps through metadata show-surface carriers.
- `MariaDB`: maps through `JSON_TABLE`, ordered-set metadata, and generic
  window-function identity.
- `Milvus`: maps through shared command envelopes; `HybridSearch` is a new verb,
  not a new parser architecture.
- `MongoDB`: maps through shared command envelopes; new verbs extend the same
  family.
- `MySQL`: maps through insert-surface flavor, `JSON_TABLE`, `QUALIFY`, and
  window-behavior metadata.
- `Neo4j`: maps through shared Cypher command envelopes.
- `OpenSearch`: maps through shared command envelopes.
- `PostgreSQL`: maps through `JSON_TABLE`, `ROWS FROM`, structured function-call
  metadata, and generic window-function identity.
- `Redis`: maps through shared command envelopes and transaction-control verbs.
- `SQLite`: no SQLite-unique shared bucket is admitted yet; only shared SQL
  carriers already supported by local source-backed donors are authorized.
- `TiDB`: maps through MySQL-family insert and window surfaces plus temporal and
  policy DDL carriers.
- `Vitess`: maps through MySQL-family insert/window surfaces plus `JSON_TABLE`
  and `QUALIFY`.
- `XTDB`: maps through temporal-clause carriers.
- `YugabyteDB`: maps through PostgreSQL-family buckets plus policy DDL carriers.

## Canonical parser-lowering rules

1. Each donor parser shall normalize donor syntax into the Beta 2 buckets above
   before SBLR emission.
2. Each donor parser shall set origin metadata required for donor-facing render,
   plan, and error shaping, but canonical semantics shall remain shared.
3. Native v3 and donor parsers shall produce structurally equivalent canonical
   AST and SBLR for the same admitted feature.
4. Shared lowering is mandatory even when the donor parser later applies donor-
   specific render hints or catalog overlay rules.

## Sample mapping snippets

```cpp
void lowerMysqlReplaceInto(const MysqlReplaceIntoNode& donor, AstArena& arena) {
  auto* stmt = arena.make<InsertStmt>();
  stmt->surface_flavor = InsertSurfaceFlavor::MYSQL_REPLACE;
  stmt->table_path = lowerTablePath(donor.table_name);
  stmt->columns = lowerColumnList(donor.columns);
  stmt->values_rows = lowerValuesRows(donor.rows);
  emitCanonicalInsert(*stmt);
}

void lowerPostgresRowsFrom(const PgRowsFromNode& donor, AstArena& arena) {
  auto* table = arena.make<TableRefNode>();
  table->ref_type = TableRefNode::Type::ROWS_FROM;
  table->rows_from = lowerRowsFromSpec(donor.items);
  table->with_ordinality = donor.with_ordinality;
  emitCanonicalTableSource(*table);
}
```

## Required proof

1. every donor family listed here shall have a documented mapping into the
   shared Beta 2 carriers before parser-package implementation begins
2. native v3 and donor-parser emissions for equivalent shared features shall
   produce matching canonical payloads
3. no donor parser shall require a donor-private AST node or donor-private SBLR
   opcode for any surface admitted here
4. SQLite shall remain fail-closed for SQLite-unique surfaces until local donor
   evidence exists
