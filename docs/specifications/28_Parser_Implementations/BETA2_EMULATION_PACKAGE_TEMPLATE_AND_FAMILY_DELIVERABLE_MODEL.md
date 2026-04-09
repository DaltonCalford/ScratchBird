# Beta 2 Emulation Package Template And Family Deliverable Model

## Purpose
Define the canonical Beta 2 emulation-package template used to implement every donor-facing compatibility family without moving donor-specific behavior into the ScratchBird core engine.

## Scope
- Common packaging and readiness rules for every Beta 2 emulation family.
- Separation of parser, compiler UDR, and engine emulation UDR responsibilities.
- Required reference-library, conformance, catalog, plan, bridge, and error-map deliverables.

## Admitted Beta 2 Families
- `Apache Ignite`: `apache_ignite` using `ignite_thin_client_protocol` and `thin_client_sql`.
- `Cassandra`: `cassandra` using `cassandra_native_v5` and `cql_wire`.
- `Citus`: `citus` using `postgresql_frontend_backend` and `sql_wire`.
- `ClickHouse`: `clickhouse` using `clickhouse_native_tcp` and `sql_wire`.
- `CockroachDB`: `cockroachdb` using `postgresql_frontend_backend` and `sql_wire`.
- `Db2`: `db2` using `drda_ddm` and `sql_wire`.
- `Dolt`: `dolt` using `mysql_classic_protocol` and `sql_wire`.
- `DuckDB`: `duckdb` using `embedded_client_api` and `embedded_sql`.
- `FirebirdSQL`: `firebirdsql` using `firebird_remote_protocol` and `sql_wire`.
- `FoundationDB`: `foundationdb` using `foundationdb_native_client_protocol` and `api_transactional`.
- `immudb`: `immudb` using `grpc_and_sql_client_api` and `grpc_sql_api`.
- `InfluxDB`: `influxdb` using `http_json_sql_influxql_line_protocol` and `http_sql_api`.
- `MariaDB`: `mariadb` using `mysql_classic_protocol_mariadb_variant` and `sql_wire`.
- `Milvus`: `milvus` using `grpc_protobuf` and `grpc_vector_api`.
- `MongoDB`: `mongodb` using `op_msg` and `document_command`.
- `MySQL`: `mysql` using `mysql_classic_protocol` and `sql_wire`.
- `Neo4j`: `neo4j` using `bolt` and `graph_bolt`.
- `OpenSearch`: `opensearch` using `http_json_rest` and `rest_json`.
- `PostgreSQL`: `postgresql` using `postgresql_frontend_backend` and `sql_wire`.
- `Redis`: `redis` using `resp2_resp3` and `command_protocol`.
- `SQLite`: `sqlite` using `sqlite_embedded_api` and `embedded_sql`.
- `SQL Server / Azure SQL`: `sqlserver` using `tds` and `sql_wire`.
- `TiDB`: `tidb` using `mysql_classic_protocol` and `sql_wire`.
- `Vitess`: `vitess` using `vtgate_mysql_protocol` and `sql_wire`.
- `XTDB`: `xtdb` using `http_json_edn_sql_xtql` and `http_sql_api`.
- `YugabyteDB`: `yugabytedb` using `postgresql_frontend_backend` and `sql_wire`.

## Deferred Beta 3 Commercial Families
- `Oracle Database`: `oracle` using `oracle_net_ttc` and `sql_wire`; deferred to Beta 3 until the remaining `TTC` grammar and packetization gaps recorded in `docs/reference/reference_library/commercial_protocol_readiness_2026-04-03/` are closed.

## Hard Invariants
1. Core engine executes SBLR and internal procedures only. It is not the donor parser, donor protocol adapter, donor catalog renderer, or donor error-text renderer.
2. Every Beta 2 emulation family is a three-part bundle:
   - parser package
   - compiler UDR package
   - emulation UDR package
3. The parser owns every client-facing concern for that donor family: connection lifecycle, protocol, authentication exchange when listener pre-auth does not close it, request decoding, datatype translation, plan rendering, result shaping, and donor-visible error mapping.
4. The compiler UDR is the only engine-side authority allowed to translate donor-generated dynamic text or helper payloads into canonical AST or SBLR for that family.
5. The emulation UDR is the only engine-side authority allowed to bootstrap donor system catalogs, family-owned helper objects, migration logic, and internal donor-client bridge routines for that family.
6. The parser, compiler UDR, and emulation UDR must lower equivalent donor fixtures to byte-identical canonical SBLR for the same profile and capability set.
7. ScratchBird `RuntimePlan` is the engine plan contract. Donor plan text is rendered from `RuntimePlan`; donor plan text is never treated as a native engine execution contract.
8. Every emulated catalog, system table, system collection, pragma, or metadata view is sandboxed to the bound database root or schema branch only.
9. No Beta 2 family may fall back to native V3, a different donor family, or compiled core-engine donor logic when any bundle part is missing or not `READY`.

## Canonical Family Bundle

| Bundle Part | Required Artifact | Owns | Forbidden |
| --- | --- | --- | --- |
| Parser package | listener-facing executable + parser package manifest | donor protocol, auth exchange, request decode, datatype translation, AST/SBLR lowering for client traffic, result shaping, donor error rendering, donor plan rendering | direct execution, direct storage access, catalog bypass, direct donor-library dependency in core engine |
| Compiler UDR package | builtin or installable UDR module | translation of donor-generated dynamic text or helper payloads to canonical AST/SBLR; verification of that translation | client socket handling, listener ownership, direct result rendering to donor clients |
| Emulation UDR package | builtin or installable UDR module | donor catalog bootstrap, empty-database defaults, overlay lifecycle, donor bridge client, migration support, family helper procedures | client protocol handling, direct execution bypass, catalog visibility outside the bound root |

## Canonical Naming Rules
1. Parser executable: `sb_parser_<family_suffix>`.
2. Listener executable: `sb_listener_<family_suffix>` when the donor family uses a listener-facing transport.
3. Parser package name: `sb_pkg_<profile_id>_parser`.
4. Compiler UDR package name: `sb_pkg_<profile_id>_compiler_udr`.
5. Emulation UDR package name: `sb_pkg_<profile_id>_emulation_udr`.
6. Compiler entrypoint name: `compiler_<profile_id>`.
7. Engine generator entrypoint name: `engine_<profile_id>`.
8. Bundle contract id: `sb_emulation_bundle_<profile_id>/v2`.

## Required Reference Inputs Per Family
1. Local 1:1 packet under `docs/reference/reference_library/emulation_1_to_1_engine_reference_packets_2026-04-02/<family>/`.
2. Family-local official web supplement under the same packet directory.
3. Error-code reference packet coverage from `docs/reference/reference_library/error_code_reference_packets_2026-04-02/`.
4. Cross-engine datatype and index matrices from the packet root.
5. Shared Beta 2 datatype, index, function, AST, SBLR, and error-envelope canon from sections `13`, `14`, `15`, `18`, `20`, `21`, `22`, `23`, `24`, `28`, and `29`.
6. Commercial families admitted from the readiness packet may begin Beta 2 implementation planning from `docs/reference/reference_library/commercial_protocol_readiness_2026-04-03/`, but they remain not-ready until items `1` and `2` are created as family-local packet deliverables.

## Listener And Handoff Contract
1. Listener accepts the transport, performs any listener-owned precheck, then hands exactly one client connection to exactly one parser worker.
2. Parser receives:
   - bound listener policy
   - target emulation profile
   - database binding context
   - TLS state
   - proxy identity state when applicable
   - request correlation seed
3. Parser must fail closed when binding context, bundle readiness, or minimum capability state is missing.
4. Parser dies when the connection ends or breaks. No donor parser worker is reused across sessions.

## Parser Contract
1. Own the full donor-facing state machine for startup, auth, session variables, prepared handles, portals or cursors, notices, async events, streaming, and cancel.
2. Convert client text, command documents, protobuf, or API calls into canonical AST/SBLR or canonical control envelopes.
3. Translate donor datatypes into ScratchBird native types or system domains before engine execution.
4. Translate engine results, diagnostics, warnings, and plans back into donor-visible rows, documents, messages, or response frames.
5. Translate `error_ref_uuid` plus typed detail slots into donor error codes, SQLSTATEs, class codes, or command errors using family-local map packs.
6. Render donor `EXPLAIN`, `PROFILE`, or plan-inspection output from canonical `RuntimePlan` and plan metrics packets.
7. Never own execution, authorization bypass, storage access, or system catalog persistence directly.

## Compiler UDR Contract
1. Entry point `compiler_<profile_id>` accepts donor-family dynamic text or helper payloads generated by engine-side routines.
2. Compiler UDR verifies:
   - active family profile
   - admitted statement or payload class
   - typed parameter shapes
   - capability gates
   - security or privilege class required for the translation
3. Compiler UDR returns only canonical artifacts:
   - AST payload
   - SBLR payload
   - canonical source-map and transform metadata
4. Compiler UDR must use the same capability table, AST shapes, and lowering semantics as the parser package for the same donor family.

## Emulation UDR Contract
1. Entry point `engine_<profile_id>` owns family-specific engine-side emulation behavior.
2. Standard procedures and functions admitted for every family:
   - `ensure_catalog_overlays(database_uuid_or_path, schema_root_uuid, options)`
   - `drop_catalog_overlays(database_uuid_or_path, options)`
   - `bootstrap_empty_database(database_uuid_or_path, options)`
   - `validate_catalog_overlays(database_uuid_or_path, options)`
   - `migrate_from_donor(connection_spec, database_uuid_or_path, options)`
   - `bridge_connect(connection_spec, options)`
3. The emulation UDR must publish one overlay object per donor-visible system table, system collection, pragma table, virtual table, or metadata view required by the donor family.
4. Empty-database defaults must match the donor family’s expected bootstrap surface for a newly created logical database.
5. All overlay predicates must restrict visibility to the emulated database root only.

## Donor Bridge And Migration Contract
1. Every family ships an internal donor client inside the emulation UDR package. It does not rely on external vendor client libraries.
2. The bridge client must support:
   - connectivity and auth verification
   - schema discovery
   - bulk extract
   - incremental catch-up when the donor surface supports it
   - validation queries
   - typed error capture for donor-to-ScratchBird error mapping audits
3. Migration phases are:
   - inspect
   - bootstrap
   - bulk copy
   - catch-up
   - validate
   - cutover
   - quarantine or failback

## Plan Rendering Contract
1. Parser requests canonical plan generation from the engine; it does not request donor plan text from the engine.
2. Family plan renderers map canonical `RuntimePlan` nodes, metrics, and hints into donor-visible `EXPLAIN` or plan documents.
3. Reverse parsing of donor plan text is out of scope. Any donor hint text is lowered to canonical hint structures before planning.
4. Every family bundle must ship golden plan fixtures for:
   - simple exact lookup
   - range scan
   - join
   - aggregate
   - sort
   - family-specific path features

## Error Mapping Contract
1. Engine emits only `error_ref_uuid` plus typed detail slots.
2. Parser-owned render packs produce human-readable ScratchBird-native text.
3. Donor parser map packs convert the same UUID and details into donor-visible error codes and text.
4. Unmapped donor surfaces must fail closed to deterministic generic donor errors rather than leak ScratchBird internal text.

## Required Deliverables Per Family
1. Family-local Beta 2 spec in section `28`.
2. Local 1:1 reference packet and official web supplement.
3. Datatype/domain translation table.
4. Index-family admission and metrics packet mapping.
5. Full parser request-class matrix.
6. Full plan renderer map.
7. Error map pack completeness report.
8. Virtual catalog bootstrap goldens for a new empty database.
9. Internal donor-client bridge test matrix.
10. Conformance corpus and upstream regression harness inventory.

## Sample Scaffold
```cpp
static const scratchbird::core::EmulationPackageScaffold kTemplate = {
    "<profile_id>",
    "sb_listener_<family_suffix>",
    "sb_parser_<family_suffix>",
    "sb_pkg_<profile_id>_parser",
    "sb_pkg_<profile_id>_compiler_udr",
    "sb_pkg_<profile_id>_emulation_udr",
    "sb_emulation_bundle_<profile_id>/v2",
    true,
    true,
    false,
    false,
};

register_emulation_entrypoint("compiler_<profile_id>", &CompilerFamily::invoke);
register_emulation_entrypoint("engine_<profile_id>", &EngineFamily::invoke);
```

## Implementation Sequence
1. Freeze family-local reference packet and official web supplement.
2. Close datatype, index, function, AST, and SBLR deltas against the shared Beta 2 canon.
3. Implement parser package and golden protocol fixtures.
4. Implement compiler UDR and prove lowering parity against parser fixtures.
5. Implement emulation UDR overlays, empty-database defaults, and bridge client.
6. Close error mapping, plan rendering, and regression harness gates.
7. Mark the family `READY` only after bundle completeness and conformance evidence exist.
