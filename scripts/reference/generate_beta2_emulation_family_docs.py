#!/usr/bin/env python3
from __future__ import annotations

import csv
import ssl
import textwrap
import urllib.error
import urllib.request
from collections import defaultdict
from pathlib import Path


ROOT = Path("/home/dcalford/CliWork/ScratchBird")
PACKET_ROOT = (
    ROOT
    / "docs/reference/reference_library/emulation_1_to_1_engine_reference_packets_2026-04-02"
)
TECH_ROOT = ROOT / "docs/reference/workspace_library/technical_specs"
SPEC_ROOT = ROOT / "docs/specifications/28_Parser_Implementations"
AUTHORITATIVE_INVENTORY = ROOT / "docs/specifications/AUTHORITATIVE_SPEC_INVENTORY.md"
SECTION_README = SPEC_ROOT / "README.md"
SECTION_TEST_CONTRACT = SPEC_ROOT / "TEST_CONTRACT.md"
PACKET_README = PACKET_ROOT / "README.md"
TECH_README = TECH_ROOT / "README.md"
TECH_WEB_MANIFEST = TECH_ROOT / "WEB_SOURCES_MANIFEST.md"
COMMON_SPEC = (
    SPEC_ROOT / "BETA2_EMULATION_PACKAGE_TEMPLATE_AND_FAMILY_DELIVERABLE_MODEL.md"
)
PACKET_WEB_INDEX = PACKET_ROOT / "OFFICIAL_WEB_REFERENCE_SUPPLEMENT_INDEX.md"
PACKET_WEB_MANIFEST = PACKET_ROOT / "OFFICIAL_WEB_REFERENCE_SUPPLEMENT_MANIFEST.csv"
TECH_EMULATION_WEB_SOURCES = TECH_ROOT / "EMULATION_BETA2_ENGINE_WEB_SOURCES_20260402.md"
ERROR_PACKET_README = (
    ROOT
    / "docs/reference/reference_library/error_code_reference_packets_2026-04-02/README.md"
)

SECTION_ORDER = [
    "datatypes",
    "indexes",
    "parser_ast",
    "wire_protocol",
    "authentication",
    "client_bridge",
    "plan_output",
    "error_codes",
    "page_optimizations",
    "regression_tests",
    "catalogs_bootstrap",
]

SECTION_LABELS = {
    "datatypes": "(a) Datatypes",
    "indexes": "(b) Indexes",
    "parser_ast": "(c) Parser to SB AST / V3 Dialect",
    "wire_protocol": "(d) Wire Protocol",
    "authentication": "(e) Authentication",
    "client_bridge": "(f) Client Bridge / UDR Target Surface",
    "plan_output": "(g) Plan Layout / Optimizer Output",
    "error_codes": "(h) Error Codes",
    "page_optimizations": "(i) Page Types and Storage Optimizations",
    "regression_tests": "(j) Regression Tests and Tooling",
    "catalogs_bootstrap": "(k) Catalog / System Tables / New Empty Database",
}

USER_AGENT = "ScratchBird Reference Builder/2026-04-02"


def normalize_markdown(text: str) -> str:
    return textwrap.dedent(text).replace("\n        ", "\n").lstrip() + "\n"


ENGINE_META = {
    "firebird": {
        "display": "FirebirdSQL",
        "profile_id": "firebirdsql",
        "spec_token": "FIREBIRDSQL",
        "suffix": "fb",
        "surface_class": "sql_wire",
        "protocol": "firebird_remote_protocol",
        "shared_lowering": "firebird_dedicated_family",
        "listener_mode": "required",
        "supports_message_blr": True,
        "supports_executable_blr": True,
        "catalog_surface": "RDB$ system-table overlays filtered to the emulated database root.",
        "bridge_surface": "internal Firebird-compatible attachment client used by migration and bridge UDR flows.",
        "regression_surface": "isql-driven Firebird compatibility suites and Firebird project regression tooling.",
        "donor_reason": "native Firebird wire, BLR-adjacent payloads, system tables, and error surfaces must remain first-class.",
        "delta_bullets": [
            "Parser must preserve Firebird attachment, transaction, blob, and service-manager framing semantics.",
            "Compiler UDR must admit SQL text plus Firebird BLR-origin dynamic payload helpers for engine-owned translation paths.",
            "Engine UDR must bootstrap `RDB$*` overlays with empty-database defaults and Firebird-style object visibility.",
        ],
        "web_docs": [
            {
                "title": "Firebird 5 Language Reference",
                "purpose": "official donor SQL, datatype, system-table, and error-text reference",
                "url": "https://www.firebirdsql.org/file/documentation/chunk/en/refdocs/fblangref50/firebird-50-language-reference.html",
                "save_relpath": "docs/reference/workspace_library/technical_specs/firebird/firebird-50-language-reference_20260210.html",
            },
            {
                "title": "Firebird 5 Quick Start Guide",
                "purpose": "official donor startup and client-surface reference",
                "url": "https://www.firebirdsql.org/file/documentation/html/en/firebirddocs/qsg5/firebird-5-quickstartguide.html",
                "save_relpath": "docs/reference/workspace_library/technical_specs/firebird/firebird-5-quickstart-guide_20260402.html",
            },
        ],
    },
    "postgresql": {
        "display": "PostgreSQL",
        "profile_id": "postgresql",
        "spec_token": "POSTGRESQL",
        "suffix": "pg",
        "surface_class": "sql_wire",
        "protocol": "postgresql_frontend_backend",
        "shared_lowering": "postgresql_family",
        "listener_mode": "required",
        "supports_message_blr": False,
        "supports_executable_blr": False,
        "catalog_surface": "`pg_catalog`, `information_schema`, and donor-visible `pg_*` plan and stats overlays.",
        "bridge_surface": "internal libpq-compatible client used by migration, passthrough, and UDR bridge flows.",
        "regression_surface": "PostgreSQL frontend/backend protocol harnesses, `psql`, and upstream regression suites.",
        "donor_reason": "PostgreSQL is a primary shipped compatibility target and sets the canonical PostgreSQL-family baseline.",
        "delta_bullets": [
            "Parser must own startup, simple query, extended query, COPY, binary format, and notice response shaping.",
            "Compiler UDR must lower PostgreSQL family helper SQL, `SHOW`, `SET`, catalog probes, and server-generated statements through shared AST/SBLR structures.",
            "Engine UDR must maintain `pg_catalog` overlay views, stable OID-visible mappings, and empty-database bootstrap rows.",
        ],
        "web_docs": [
            {
                "title": "PostgreSQL Protocol",
                "purpose": "official frontend/backend protocol reference",
                "url": "https://www.postgresql.org/docs/current/protocol.html",
                "save_relpath": "docs/reference/workspace_library/technical_specs/postgresql/protocol_current_20260402.html",
            },
            {
                "title": "PostgreSQL Using EXPLAIN",
                "purpose": "official plan-render and explain output reference",
                "url": "https://www.postgresql.org/docs/current/using-explain.html",
                "save_relpath": "docs/reference/workspace_library/technical_specs/postgresql/using-explain_current_20260402.html",
            },
        ],
    },
    "mysql": {
        "display": "MySQL",
        "profile_id": "mysql",
        "spec_token": "MYSQL",
        "suffix": "mysql",
        "surface_class": "sql_wire",
        "protocol": "mysql_classic_protocol",
        "shared_lowering": "mysql_family",
        "listener_mode": "required",
        "supports_message_blr": False,
        "supports_executable_blr": False,
        "catalog_surface": "`mysql`, `information_schema`, `performance_schema`, and `sys` overlays filtered to the emulated root.",
        "bridge_surface": "internal libmysql-compatible client used by migration and bridge UDR operations.",
        "regression_surface": "MySQL protocol harnesses plus `mysql-test-run.pl` compatibility suites.",
        "donor_reason": "MySQL is a shipped SQL-wire compatibility target and anchors the broader MySQL-family bundle model.",
        "delta_bullets": [
            "Parser must own classic protocol handshake, auth plugin negotiation, prepared statements, multi-resultsets, and session state tracking.",
            "Compiler UDR must normalize MySQL-family dynamic SQL, helper statements, and donor-visible metadata probes to shared AST/SBLR.",
            "Engine UDR must publish MySQL catalog overlays and default empty-database rows without exposing out-of-branch objects.",
        ],
        "web_docs": [
            {
                "title": "MySQL information_schema.TABLES",
                "purpose": "official system-table and catalog column reference",
                "url": "https://dev.mysql.com/doc/refman/8.4/en/information-schema-tables-table.html",
                "save_relpath": "docs/reference/workspace_library/technical_specs/mysql/information-schema-tables-table_8_4_20260210.html",
            },
            {
                "title": "MySQL InnoDB Online DDL",
                "purpose": "official DDL and metadata behavior reference",
                "url": "https://docs.oracle.com/cd/E17952_01/mysql-8.4-en/innodb-online-ddl.html",
                "save_relpath": "docs/reference/workspace_library/technical_specs/mysql/innodb-online-ddl_8_4_20260401.html",
            },
        ],
    },
    "mariadb": {
        "display": "MariaDB",
        "profile_id": "mariadb",
        "spec_token": "MARIADB",
        "suffix": "mariadb",
        "surface_class": "sql_wire",
        "protocol": "mysql_classic_protocol_mariadb_variant",
        "shared_lowering": "mysql_family",
        "listener_mode": "required",
        "supports_message_blr": False,
        "supports_executable_blr": False,
        "catalog_surface": "MariaDB-visible `mysql` and `information_schema` overlays plus family-specific compatibility views.",
        "bridge_surface": "internal MariaDB-compatible client used by migration and federated bridge paths.",
        "regression_surface": "MariaDB protocol fixtures and `mariadb-test-run.pl` compatibility suites.",
        "donor_reason": "MariaDB extends the MySQL family with donor-visible syntax, optimizer, and system-catalog deltas that need an independent bundle.",
        "delta_bullets": [
            "Parser reuses MySQL wire and SQL family infrastructure but must admit MariaDB-only syntax and result-shape deltas.",
            "Compiler UDR must preserve MariaDB-specific metadata and helper statements rather than collapsing them into plain MySQL text.",
            "Engine UDR must materialize MariaDB-visible catalog rows separately from MySQL overlays when client-visible semantics differ.",
        ],
        "web_docs": [
            {
                "title": "MariaDB Data Types",
                "purpose": "official donor datatype reference",
                "url": "https://mariadb.com/kb/en/data-types/",
                "save_relpath": "docs/reference/workspace_library/technical_specs/mariadb/data-types_20260402.html",
            },
            {
                "title": "MariaDB EXPLAIN",
                "purpose": "official donor plan and explain reference",
                "url": "https://mariadb.com/kb/en/explain/",
                "save_relpath": "docs/reference/workspace_library/technical_specs/mariadb/explain_20260402.html",
            },
        ],
    },
    "cassandra": {
        "display": "Cassandra",
        "profile_id": "cassandra",
        "spec_token": "CASSANDRA",
        "suffix": "cassandra",
        "surface_class": "cql_wire",
        "protocol": "cassandra_native_v5",
        "shared_lowering": "cql_family",
        "listener_mode": "required",
        "supports_message_blr": False,
        "supports_executable_blr": False,
        "catalog_surface": "`system`, `system_schema`, and `system_views` overlays with keyspace-root filtering.",
        "bridge_surface": "internal Cassandra-native client used for migration and validation against donor clusters.",
        "regression_surface": "native protocol fixtures, `cqlsh`, and donor distributed/unit test suites.",
        "donor_reason": "Cassandra requires a CQL-native protocol, schema metadata, and indexing model rather than an SQL-wire clone.",
        "delta_bullets": [
            "Parser must own native protocol framing, CQL statement classification, paging state, and metadata response shaping.",
            "Compiler UDR must lower CQL DDL, DML, and schema-introspection commands to shared AST/SBLR without inventing SQL-only semantics.",
            "Engine UDR must publish `system_*` overlays and bootstrap keyspaces so every emulated database root is sandboxed.",
        ],
        "web_docs": [
            {
                "title": "Cassandra Storage Engine",
                "purpose": "official donor storage and write-path reference",
                "url": "https://cassandra.apache.org/doc/stable/cassandra/architecture/storage-engine.html",
                "save_relpath": "docs/reference/workspace_library/technical_specs/cassandra/storage-engine_cassandra_stable_20260402.html",
            },
            {
                "title": "Cassandra Virtual Tables",
                "purpose": "official donor system and metadata table reference",
                "url": "https://cassandra.apache.org/doc/4.1/cassandra/operating/virtualtables.html",
                "save_relpath": "docs/reference/workspace_library/technical_specs/cassandra/virtualtables_20260210.html",
            },
        ],
    },
    "clickhouse": {
        "display": "ClickHouse",
        "profile_id": "clickhouse",
        "spec_token": "CLICKHOUSE",
        "suffix": "clickhouse",
        "surface_class": "sql_wire",
        "protocol": "clickhouse_native_tcp",
        "shared_lowering": "clickhouse_sql_family",
        "listener_mode": "required",
        "supports_message_blr": False,
        "supports_executable_blr": False,
        "catalog_surface": "`system` database overlays and MergeTree-visible metadata surfaces.",
        "bridge_surface": "internal ClickHouse native client used by migration and plan-validation flows.",
        "regression_surface": "ClickHouse query suites, native-client fixtures, and explain plan goldens.",
        "donor_reason": "ClickHouse introduces donor-visible datatypes, explain layouts, and index families not covered by SQL-wire families alone.",
        "delta_bullets": [
            "Parser must admit ClickHouse SQL modifiers, settings, and explain forms already mapped into the Beta2 donor dialect canon.",
            "Compiler UDR must lower MergeTree helper SQL, system-table queries, and family-owned command forms to shared SBLR.",
            "Engine UDR must expose `system` overlays and ClickHouse-style storage or settings introspection without leaking cross-root metadata.",
        ],
        "web_docs": [
            {
                "title": "ClickHouse Data Types",
                "purpose": "official donor datatype and encoding reference",
                "url": "https://clickhouse.com/docs/sql-reference/data-types",
                "save_relpath": "docs/reference/workspace_library/technical_specs/clickhouse/data-types_20260402.html",
            },
            {
                "title": "ClickHouse EXPLAIN",
                "purpose": "official donor plan rendering reference",
                "url": "https://clickhouse.com/docs/sql-reference/statements/explain",
                "save_relpath": "docs/reference/workspace_library/technical_specs/clickhouse/explain_20260402.html",
            },
        ],
    },
    "duckdb": {
        "display": "DuckDB",
        "profile_id": "duckdb",
        "spec_token": "DUCKDB",
        "suffix": "duckdb",
        "surface_class": "embedded_sql",
        "protocol": "embedded_client_api",
        "shared_lowering": "embedded_sql_family",
        "listener_mode": "optional",
        "supports_message_blr": False,
        "supports_executable_blr": False,
        "catalog_surface": "DuckDB system-table and pragma overlays exposed inside the emulated schema root.",
        "bridge_surface": "internal embedded-client bridge used for migration and UDR passthrough without external donor libraries.",
        "regression_surface": "DuckDB SQL test corpus and local-shell style explain/query fixtures.",
        "donor_reason": "DuckDB uses an embedded SQL surface with donor-visible datatypes and plan formatting that do not map cleanly to wire families.",
        "delta_bullets": [
            "Parser package is still mandatory, but the family may ship as a library shim plus optional listener rather than a mandatory socket server.",
            "Compiler UDR must own donor-specific PRAGMA, system-table, and helper statement translation.",
            "Engine UDR must bootstrap DuckDB-visible catalog surfaces and extension-visible metadata views inside the branch sandbox.",
        ],
        "web_docs": [
            {
                "title": "DuckDB Data Types Overview",
                "purpose": "official donor datatype reference",
                "url": "https://duckdb.org/docs/stable/sql/data_types/overview",
                "save_relpath": "docs/reference/workspace_library/technical_specs/duckdb/data-types_20260402.html",
            },
            {
                "title": "DuckDB EXPLAIN Guide",
                "purpose": "official donor explain and plan formatting reference",
                "url": "https://duckdb.org/docs/stable/guides/meta/explain",
                "save_relpath": "docs/reference/workspace_library/technical_specs/duckdb/explain_20260402.html",
            },
        ],
    },
    "influxdb": {
        "display": "InfluxDB",
        "profile_id": "influxdb",
        "spec_token": "INFLUXDB",
        "suffix": "influxdb",
        "surface_class": "http_sql_api",
        "protocol": "http_json_sql_influxql_line_protocol",
        "shared_lowering": "time_series_api_family",
        "listener_mode": "required",
        "supports_message_blr": False,
        "supports_executable_blr": False,
        "catalog_surface": "time-series metadata and system-table overlays filtered to the emulated database root.",
        "bridge_surface": "internal HTTP and client-API bridge used for migration, replay, and donor validation.",
        "regression_surface": "InfluxDB API fixtures, SQL/InfluxQL corpus, and donor command-tool compatibility tests.",
        "donor_reason": "InfluxDB combines SQL, InfluxQL, and line-protocol ingestion with time-series metadata and storage rules.",
        "delta_bullets": [
            "Parser must distinguish SQL, InfluxQL, and write-line flows while producing shared AST/SBLR or ingest envelopes.",
            "Compiler UDR must lower system queries and server-generated donor text for both SQL and InfluxQL paths.",
            "Engine UDR must own bucket/database overlays, time-series metadata views, and bootstrap rows for empty donors.",
        ],
        "web_docs": [
            {
                "title": "InfluxDB 3 Core Docs Home",
                "purpose": "official donor landing page for SQL, InfluxQL, API, and system references",
                "url": "https://docs.influxdata.com/influxdb3/core/",
                "save_relpath": "docs/reference/workspace_library/technical_specs/influxdb/docs-home_20260402.html",
            },
            {
                "title": "InfluxDB 3 SQL Queries",
                "purpose": "official donor SQL reference",
                "url": "https://docs.influxdata.com/influxdb3/core/query-data/sql/",
                "save_relpath": "docs/reference/workspace_library/technical_specs/influxdb/sql_20260402.html",
            },
        ],
    },
    "mongodb": {
        "display": "MongoDB",
        "profile_id": "mongodb",
        "spec_token": "MONGODB",
        "suffix": "mongodb",
        "surface_class": "document_command",
        "protocol": "op_msg",
        "shared_lowering": "document_command_family",
        "listener_mode": "required",
        "supports_message_blr": False,
        "supports_executable_blr": False,
        "catalog_surface": "`admin`, `local`, `config`, and synthetic collection metadata overlays.",
        "bridge_surface": "internal MongoDB-compatible client used by migration, sync, and bridge UDR flows.",
        "regression_surface": "MongoDB command fixtures, shell-driven tests, and explain or stats goldens.",
        "donor_reason": "MongoDB requires BSON, document commands, OP_MSG framing, and collection/index metadata beyond SQL-wire assumptions.",
        "delta_bullets": [
            "Parser must own OP_MSG framing, command envelopes, BSON conversion, and explain/result document shaping.",
            "Compiler UDR must lower donor command helpers, generated queries, and metadata probes into shared document-oriented SBLR carriers.",
            "Engine UDR must expose collection and system metadata views plus empty-database bootstrap defaults for donor-visible admin collections.",
        ],
        "web_docs": [
            {
                "title": "MongoDB dbStats",
                "purpose": "official donor admin and stats command reference",
                "url": "https://www.mongodb.com/docs/v8.0/reference/command/dbStats/",
                "save_relpath": "docs/reference/workspace_library/technical_specs/mongodb/dbStats_8_0_20260210.html",
            },
            {
                "title": "MongoDB WiredTiger",
                "purpose": "official donor storage and page-optimization reference",
                "url": "https://www.mongodb.com/docs/manual/core/wiredtiger/",
                "save_relpath": "docs/reference/workspace_library/technical_specs/mongodb/wiredtiger_mongodb_manual_20260402.html",
            },
        ],
    },
    "neo4j": {
        "display": "Neo4j",
        "profile_id": "neo4j",
        "spec_token": "NEO4J",
        "suffix": "neo4j",
        "surface_class": "graph_bolt",
        "protocol": "bolt",
        "shared_lowering": "graph_query_family",
        "listener_mode": "required",
        "supports_message_blr": False,
        "supports_executable_blr": False,
        "catalog_surface": "graph metadata, `SHOW` surfaces, and procedure-backed synthetic system views.",
        "bridge_surface": "internal Bolt-compatible client used by graph migration and bridge UDR routines.",
        "regression_surface": "Bolt protocol fixtures, Cypher compatibility corpus, and plan output goldens.",
        "donor_reason": "Neo4j exposes a graph data model, Bolt wire protocol, and procedure or explain surfaces that differ materially from SQL-wire engines.",
        "delta_bullets": [
            "Parser must own Bolt handshake, message framing, Cypher lowering, and graph-value translation.",
            "Compiler UDR must lower donor procedure calls, plan requests, and generated Cypher text into shared graph-capable SBLR structures.",
            "Engine UDR must bootstrap Neo4j-visible metadata views and empty graph/system defaults under the emulated root.",
        ],
        "web_docs": [
            {
                "title": "Neo4j SHOW INDEXES",
                "purpose": "official donor index metadata surface reference",
                "url": "https://neo4j.com/docs/cypher-manual/current/indexes-for-search-performance/indexes/",
                "save_relpath": "docs/reference/workspace_library/technical_specs/neo4j/show-indexes_20260210.html",
            },
            {
                "title": "Neo4j db.stats.retrieve",
                "purpose": "official donor procedure and stats reference",
                "url": "https://neo4j.com/docs/operations-manual/current/reference/procedures/",
                "save_relpath": "docs/reference/workspace_library/technical_specs/neo4j/procedures-db-stats-retrieve_20260210.html",
            },
        ],
    },
    "opensearch": {
        "display": "OpenSearch",
        "profile_id": "opensearch",
        "spec_token": "OPENSEARCH",
        "suffix": "opensearch",
        "surface_class": "rest_json",
        "protocol": "http_json_rest",
        "shared_lowering": "search_document_family",
        "listener_mode": "required",
        "supports_message_blr": False,
        "supports_executable_blr": False,
        "catalog_surface": "index, shard, cluster, and `_cat`-style synthetic metadata views under the emulated branch.",
        "bridge_surface": "internal HTTP/JSON client used by migration, validation, and passthrough UDR flows.",
        "regression_surface": "REST API fixtures, query explain goldens, and admin-surface compatibility tests.",
        "donor_reason": "OpenSearch exposes REST/JSON semantics, mapping types, plan-style explain output, and cluster metadata instead of SQL-wire surfaces.",
        "delta_bullets": [
            "Parser must own HTTP routing, JSON request decoding, mapping and search API normalization, and response rendering.",
            "Compiler UDR must lower donor-generated query JSON, script wrappers, and helper operations to shared document-search SBLR carriers.",
            "Engine UDR must publish synthetic `_cat`, index settings, and cluster metadata views filtered to the emulated root.",
        ],
        "web_docs": [
            {
                "title": "OpenSearch Circuit Breaker",
                "purpose": "official donor runtime and resource-governance reference",
                "url": "https://docs.opensearch.org/latest/install-and-configure/configuring-opensearch/circuit-breaker/",
                "save_relpath": "docs/reference/workspace_library/technical_specs/opensearch/circuit-breaker_opensearch_latest_20260402.html",
            },
            {
                "title": "OpenSearch Explain API",
                "purpose": "official donor explain and plan-shape reference",
                "url": "https://docs.opensearch.org/latest/api-reference/search-apis/explain/",
                "save_relpath": "docs/reference/workspace_library/technical_specs/opensearch/explain_api_20260402.html",
            },
        ],
    },
    "redis": {
        "display": "Redis",
        "profile_id": "redis",
        "spec_token": "REDIS",
        "suffix": "redis",
        "surface_class": "command_protocol",
        "protocol": "resp2_resp3",
        "shared_lowering": "key_command_family",
        "listener_mode": "required",
        "supports_message_blr": False,
        "supports_executable_blr": False,
        "catalog_surface": "synthetic `INFO`, `COMMAND`, ACL, keyspace, and module metadata views.",
        "bridge_surface": "internal RESP-compatible client used for migration and bridge passthrough.",
        "regression_surface": "RESP2/RESP3 fixtures, command-golden corpus, and admin/introspection response goldens.",
        "donor_reason": "Redis compatibility depends on RESP framing, command families, type metadata, and non-SQL result shaping.",
        "delta_bullets": [
            "Parser must own RESP2/RESP3 framing, HELLO negotiation, command dispatch, push notifications, and error rendering.",
            "Compiler UDR must translate engine-origin command text or helper payloads into shared command-oriented SBLR envelopes.",
            "Engine UDR must expose synthetic metadata rows for `INFO`, ACL, modules, and keyspace introspection inside the emulated root.",
        ],
        "web_docs": [
            {
                "title": "Redis INFO Command",
                "purpose": "official donor stats and metadata response reference",
                "url": "https://redis.io/commands/info/",
                "save_relpath": "docs/reference/workspace_library/technical_specs/redis/info-command_20260210.html",
            },
            {
                "title": "Redis Memory Optimization",
                "purpose": "official donor storage and optimization reference",
                "url": "https://redis.io/docs/latest/operate/oss_and_stack/management/optimization/memory-optimization/",
                "save_relpath": "docs/reference/workspace_library/technical_specs/redis/memory-optimization_redis_latest_20260402.html",
            },
        ],
    },
    "milvus": {
        "display": "Milvus",
        "profile_id": "milvus",
        "spec_token": "MILVUS",
        "suffix": "milvus",
        "surface_class": "grpc_vector_api",
        "protocol": "grpc_protobuf",
        "shared_lowering": "vector_api_family",
        "listener_mode": "required",
        "supports_message_blr": False,
        "supports_executable_blr": False,
        "catalog_surface": "collection, partition, index, and load-state metadata views mapped into emulated system schemas.",
        "bridge_surface": "internal gRPC Milvus-compatible client used for migration and remote bridge flows.",
        "regression_surface": "Milvus API fixtures, collection/index goldens, and vector-explain compatibility suites.",
        "donor_reason": "Milvus exposes a vector-native gRPC API, collection metadata, and index-state surfaces rather than a classic SQL wire.",
        "delta_bullets": [
            "Parser must own gRPC method routing, protobuf validation, streaming behavior, and vector payload translation.",
            "Compiler UDR must lower engine-origin helper requests, admin commands, and query plans into shared vector-capable SBLR operations.",
            "Engine UDR must bootstrap collection and index metadata overlays with empty-database defaults for each emulated root.",
        ],
        "web_docs": [
            {
                "title": "Milvus describeIndex",
                "purpose": "official donor index metadata reference",
                "url": "https://milvus.io/api-reference/java/v2.2.x/Index/describeIndex%28%29.md",
                "save_relpath": "docs/reference/workspace_library/technical_specs/milvus/describe_index_20260210.html",
            },
            {
                "title": "Milvus getCollectionStatistics",
                "purpose": "official donor collection stats and metadata reference",
                "url": "https://milvus.io/api-reference/java/v2.3.x/v1/Collection/getCollectionStatistics.md",
                "save_relpath": "docs/reference/workspace_library/technical_specs/milvus/get_collection_statistics_20260210.html",
            },
        ],
    },
    "sqlite": {
        "display": "SQLite",
        "profile_id": "sqlite",
        "spec_token": "SQLITE",
        "suffix": "sqlite",
        "surface_class": "embedded_sql",
        "protocol": "sqlite_embedded_api",
        "shared_lowering": "embedded_sql_family",
        "listener_mode": "optional",
        "supports_message_blr": False,
        "supports_executable_blr": False,
        "catalog_surface": "`sqlite_schema`, pragma, and introspection overlays with branch-root filtering.",
        "bridge_surface": "internal SQLite-compatible bridge used for migration and file-ingest or passthrough workflows.",
        "regression_surface": "SQLite shell fixtures, pragma goldens, and file-format compatibility suites.",
        "donor_reason": "SQLite is an embedded engine without a standalone server protocol, so the family needs a library-shim parser model and strong catalog/file references.",
        "delta_bullets": [
            "Parser package is a library-shim and optional listener profile rather than a required socket listener family.",
            "Compiler UDR must own pragma lowering, generated SQL translation, and donor helper statements for embedded-use parity.",
            "Engine UDR must publish `sqlite_schema` and pragma-visible overlays plus empty-database bootstrap objects.",
        ],
        "web_docs": [
            {
                "title": "SQLite Datatypes",
                "purpose": "official donor datatype and affinity reference",
                "url": "https://www.sqlite.org/datatype3.html",
                "save_relpath": "docs/reference/workspace_library/technical_specs/sqlite/datatype3_20260402.html",
            },
            {
                "title": "SQLite EXPLAIN QUERY PLAN",
                "purpose": "official donor plan rendering reference",
                "url": "https://www.sqlite.org/eqp.html",
                "save_relpath": "docs/reference/workspace_library/technical_specs/sqlite/explain-query-plan_20260402.html",
            },
        ],
    },
    "dolt": {
        "display": "Dolt",
        "profile_id": "dolt",
        "spec_token": "DOLT",
        "suffix": "dolt",
        "surface_class": "sql_wire",
        "protocol": "mysql_classic_protocol",
        "shared_lowering": "mysql_family",
        "listener_mode": "required",
        "supports_message_blr": False,
        "supports_executable_blr": False,
        "catalog_surface": "MySQL-compatible catalogs plus `dolt_*` tables, diff/history metadata, and branch-aware views.",
        "bridge_surface": "internal Dolt-compatible client used for migration, diff validation, and bridge UDR flows.",
        "regression_surface": "MySQL-wire fixtures plus Dolt SQL and version-control command suites.",
        "donor_reason": "Dolt reuses MySQL wire but adds version-control SQL surfaces and system tables that need family-specific overlays.",
        "delta_bullets": [
            "Parser reuses MySQL protocol machinery but must admit `DOLT_*` calls, version-control DDL/DML, and donor result-shape deltas.",
            "Compiler UDR must preserve Dolt-specific helper statements and history or diff queries rather than collapsing them into generic MySQL text.",
            "Engine UDR must materialize `dolt_*` metadata overlays and empty-database version-control bootstrap rows per emulated root.",
        ],
        "web_docs": [
            {
                "title": "Dolt SQL Support",
                "purpose": "official donor SQL surface reference",
                "url": "https://docs.dolthub.com/sql-reference/sql-support",
                "save_relpath": "docs/reference/workspace_library/technical_specs/dolt/sql-support_20260402.html",
            },
            {
                "title": "Dolt Data Types",
                "purpose": "official donor datatype reference",
                "url": "https://docs.dolthub.com/sql-reference/sql-support/data-description",
                "save_relpath": "docs/reference/workspace_library/technical_specs/dolt/data-types_20260402.html",
            },
        ],
    },
    "foundationdb": {
        "display": "FoundationDB",
        "profile_id": "foundationdb",
        "spec_token": "FOUNDATIONDB",
        "suffix": "foundationdb",
        "surface_class": "api_transactional",
        "protocol": "foundationdb_native_client_protocol",
        "shared_lowering": "transactional_api_family",
        "listener_mode": "optional",
        "supports_message_blr": False,
        "supports_executable_blr": False,
        "catalog_surface": "directory-layer, keyspace, and metadata overlays rather than SQL system tables.",
        "bridge_surface": "internal FoundationDB-compatible client used for migration, tuple encoding, and bridge passthrough.",
        "regression_surface": "FoundationDB binding tests, tuple-layer fixtures, and transactional API goldens.",
        "donor_reason": "FoundationDB is a transactional API platform, so emulation depends on tuple, versionstamp, directory, and error semantics rather than SQL-wire behavior.",
        "delta_bullets": [
            "Parser family may ship primarily as a client-library or service shim rather than a mandatory listener.",
            "Compiler UDR must lower donor helper strings and generated transactional operations into shared API-capable SBLR envelopes.",
            "Engine UDR must expose directory-layer and metadata overlays while preserving branch sandboxing and stable synthetic identities.",
        ],
        "web_docs": [
            {
                "title": "FoundationDB Developer Guide",
                "purpose": "official donor transactional API and architecture reference",
                "url": "https://apple.github.io/foundationdb/developer-guide.html",
                "save_relpath": "docs/reference/workspace_library/technical_specs/foundationdb/developer-guide_20260402.html",
            },
            {
                "title": "FoundationDB Data Modeling",
                "purpose": "official donor tuple and keyspace modeling reference",
                "url": "https://apple.github.io/foundationdb/data-modeling.html",
                "save_relpath": "docs/reference/workspace_library/technical_specs/foundationdb/data-modeling_20260402.html",
            },
        ],
    },
    "vitess": {
        "display": "Vitess",
        "profile_id": "vitess",
        "spec_token": "VITESS",
        "suffix": "vitess",
        "surface_class": "sql_wire",
        "protocol": "vtgate_mysql_protocol",
        "shared_lowering": "mysql_family",
        "listener_mode": "required",
        "supports_message_blr": False,
        "supports_executable_blr": False,
        "catalog_surface": "MySQL-compatible metadata plus VTGate and sharding-visible synthetic system views.",
        "bridge_surface": "internal Vitess-compatible client used for migration, sharding validation, and bridge passthrough.",
        "regression_surface": "Vitess SQL suites, VTGate protocol fixtures, and explain output goldens.",
        "donor_reason": "Vitess uses MySQL protocol but adds routing, VTGate metadata, and distributed plan semantics that need a separate family model.",
        "delta_bullets": [
            "Parser reuses MySQL wire foundations but must admit VTGate session variables, comments, routing hints, and explain forms.",
            "Compiler UDR must preserve Vitess-specific helper SQL and topology-aware metadata probes.",
            "Engine UDR must expose synthetic topology and sharding views alongside MySQL-compatible catalogs.",
        ],
        "web_docs": [
            {
                "title": "Vitess MySQL Compatibility",
                "purpose": "official donor compatibility and protocol-positioning reference",
                "url": "https://vitess.io/docs/22.0/reference/compatibility/mysql-compatibility/",
                "save_relpath": "docs/reference/workspace_library/technical_specs/vitess/mysql-compatibility_20260402.html",
            },
            {
                "title": "Vitess Execution Plans",
                "purpose": "official donor plan and VTGate execution reference",
                "url": "https://vitess.io/docs/22.0/concepts/execution-plans/",
                "save_relpath": "docs/reference/workspace_library/technical_specs/vitess/execution-plans_20260402.html",
            },
        ],
    },
    "immudb": {
        "display": "immudb",
        "profile_id": "immudb",
        "spec_token": "IMMUDB",
        "suffix": "immudb",
        "surface_class": "grpc_sql_api",
        "protocol": "grpc_and_sql_client_api",
        "shared_lowering": "immutable_sql_family",
        "listener_mode": "required",
        "supports_message_blr": False,
        "supports_executable_blr": False,
        "catalog_surface": "immudb system and metadata views mapped into sandboxed overlays.",
        "bridge_surface": "internal immudb client used for migration and donor validation without external libraries.",
        "regression_surface": "immudb API fixtures, SQL corpus, and admin or audit compatibility tests.",
        "donor_reason": "immudb combines SQL, verified transaction history, and gRPC/admin APIs that need family-specific mapping.",
        "delta_bullets": [
            "Parser must own SQL plus API/admin request carriers and verified-history metadata rendering.",
            "Compiler UDR must lower donor helper SQL and control-plane requests into shared SBLR plus audit-bound carriers.",
            "Engine UDR must expose immudb metadata, verified-history views, and empty-database defaults under the emulated root.",
        ],
        "web_docs": [
            {
                "title": "immudb Docs Home",
                "purpose": "official donor landing page for SQL, API, auth, and admin reference",
                "url": "https://docs.immudb.io/master/",
                "save_relpath": "docs/reference/workspace_library/technical_specs/immudb/docs-home_20260402.html",
            },
            {
                "title": "immudb SQL Reference",
                "purpose": "official donor SQL surface reference",
                "url": "https://docs.immudb.io/1.1.0/reference/sql",
                "save_relpath": "docs/reference/workspace_library/technical_specs/immudb/sql-reference_20260402.html",
            },
        ],
    },
    "xtdb": {
        "display": "XTDB",
        "profile_id": "xtdb",
        "spec_token": "XTDB",
        "suffix": "xtdb",
        "surface_class": "http_sql_api",
        "protocol": "http_json_edn_sql_xtql",
        "shared_lowering": "bitemporal_api_family",
        "listener_mode": "required",
        "supports_message_blr": False,
        "supports_executable_blr": False,
        "catalog_surface": "information schema plus XTDB transaction-log and temporal metadata overlays.",
        "bridge_surface": "internal XTDB-compatible HTTP client used for migration, audit, and bridge UDR operations.",
        "regression_surface": "XTDB SQL/XTQL corpus, HTTP API fixtures, and bitemporal query goldens.",
        "donor_reason": "XTDB introduces SQL plus XTQL/EDN and temporal metadata surfaces that require explicit family-level mapping.",
        "delta_bullets": [
            "Parser must admit both SQL and XTQL-family carriers plus temporal clause rendering.",
            "Compiler UDR must lower donor helper text and temporal introspection requests into shared AST/SBLR structures.",
            "Engine UDR must expose XTDB-visible transaction, history, and schema metadata under the emulated root.",
        ],
        "web_docs": [
            {
                "title": "XTDB Docs Home",
                "purpose": "official donor landing page for SQL, XTQL, API, and admin reference",
                "url": "https://docs.xtdb.com/",
                "save_relpath": "docs/reference/workspace_library/technical_specs/xtdb/docs-home_20260402.html",
            },
            {
                "title": "XTDB Reference Guide",
                "purpose": "official donor SQL surface reference",
                "url": "https://docs.xtdb.com/reference/main.html",
                "save_relpath": "docs/reference/workspace_library/technical_specs/xtdb/sql-overview_20260402.html",
            },
        ],
    },
    "tidb": {
        "display": "TiDB",
        "profile_id": "tidb",
        "spec_token": "TIDB",
        "suffix": "tidb",
        "surface_class": "sql_wire",
        "protocol": "mysql_classic_protocol",
        "shared_lowering": "mysql_family",
        "listener_mode": "required",
        "supports_message_blr": False,
        "supports_executable_blr": False,
        "catalog_surface": "MySQL-compatible catalogs plus `CLUSTER_*`, statistics, and TiDB-specific metadata views.",
        "bridge_surface": "internal TiDB-compatible client used for migration and bridge flows.",
        "regression_surface": "MySQL-wire fixtures plus TiDB SQL, explain, and admin command suites.",
        "donor_reason": "TiDB extends MySQL protocol with distributed SQL metadata, plan output, and admin surfaces that need a separate family bundle.",
        "delta_bullets": [
            "Parser reuses MySQL wire foundations but must admit TiDB-specific `ADMIN`, `SHOW`, and plan-surface extensions.",
            "Compiler UDR must preserve TiDB-generated helper SQL and cluster metadata probes.",
            "Engine UDR must expose TiDB-specific system and statistics views in addition to MySQL-compatible overlays.",
        ],
        "web_docs": [
            {
                "title": "TiDB Data Type Overview",
                "purpose": "official donor datatype reference",
                "url": "https://docs.pingcap.com/tidb/stable/data-type-overview/",
                "save_relpath": "docs/reference/workspace_library/technical_specs/tidb/data-type-overview_20260402.html",
            },
            {
                "title": "TiDB EXPLAIN Overview",
                "purpose": "official donor plan and explain reference",
                "url": "https://docs.pingcap.com/tidb/stable/explain-overview/",
                "save_relpath": "docs/reference/workspace_library/technical_specs/tidb/explain-overview_20260402.html",
            },
        ],
    },
    "cockroachdb": {
        "display": "CockroachDB",
        "profile_id": "cockroachdb",
        "spec_token": "COCKROACHDB",
        "suffix": "cockroachdb",
        "surface_class": "sql_wire",
        "protocol": "postgresql_frontend_backend",
        "shared_lowering": "postgresql_family",
        "listener_mode": "required",
        "supports_message_blr": False,
        "supports_executable_blr": False,
        "catalog_surface": "`pg_catalog`, `information_schema`, `crdb_internal`, and cluster metadata overlays.",
        "bridge_surface": "internal PostgreSQL-compatible client used for migration, bridge UDR, and distributed validation.",
        "regression_surface": "PostgreSQL-wire fixtures plus CockroachDB SQL and admin-surface compatibility suites.",
        "donor_reason": "CockroachDB uses PostgreSQL wire but introduces distributed metadata, explain shapes, and syntax that require a family-specific layer.",
        "delta_bullets": [
            "Parser reuses PostgreSQL wire foundations but must admit Cockroach-specific statements, session variables, and result-shape deltas.",
            "Compiler UDR must preserve `SHOW`, `EXPLAIN`, and `crdb_internal` helper surfaces in canonical lowering.",
            "Engine UDR must publish `crdb_internal` and Cockroach-specific system overlays alongside PostgreSQL-compatible catalogs.",
        ],
        "web_docs": [
            {
                "title": "CockroachDB Online Schema Changes",
                "purpose": "official donor DDL lifecycle reference",
                "url": "https://www.cockroachlabs.com/docs/stable/online-schema-changes",
                "save_relpath": "docs/reference/workspace_library/technical_specs/cockroachdb/online-schema-changes_stable_20260401.html",
            },
            {
                "title": "CockroachDB PostgreSQL Compatibility",
                "purpose": "official donor protocol and behavior compatibility reference",
                "url": "https://www.cockroachlabs.com/docs/stable/postgresql-compatibility",
                "save_relpath": "docs/reference/workspace_library/technical_specs/cockroachdb/postgresql-compatibility_20260402.html",
            },
        ],
    },
    "yugabytedb": {
        "display": "YugabyteDB",
        "profile_id": "yugabytedb",
        "spec_token": "YUGABYTEDB",
        "suffix": "yugabytedb",
        "surface_class": "sql_wire",
        "protocol": "postgresql_frontend_backend",
        "shared_lowering": "postgresql_family",
        "listener_mode": "required",
        "supports_message_blr": False,
        "supports_executable_blr": False,
        "catalog_surface": "PostgreSQL-compatible catalogs plus YugabyteDB metadata and distributed-system overlays.",
        "bridge_surface": "internal PostgreSQL-compatible client used for YSQL migration and bridge UDR operations.",
        "regression_surface": "PostgreSQL-wire fixtures plus Yugabyte SQL and admin compatibility suites.",
        "donor_reason": "The current donor packet evidences the SQL-facing YugabyteDB family; that family builds on PostgreSQL wire with donor-specific distributed metadata and explain output.",
        "delta_bullets": [
            "Parser reuses PostgreSQL wire foundations for the SQL-facing YugabyteDB family while preserving donor-specific syntax and metadata.",
            "Compiler UDR must preserve Yugabyte helper SQL, distributed metadata probes, and explain variants.",
            "Engine UDR must publish YugabyteDB-visible catalogs and system metadata overlays inside the emulated root.",
        ],
        "web_docs": [
            {
                "title": "YugabyteDB Docs Home",
                "purpose": "official donor landing page for YSQL, admin, and metadata reference",
                "url": "https://docs.yugabyte.com/preview/",
                "save_relpath": "docs/reference/workspace_library/technical_specs/yugabytedb/docs-home_20260402.html",
            },
            {
                "title": "YugabyteDB YSQL Data Types",
                "purpose": "official donor SQL datatype reference",
                "url": "https://docs.yugabyte.com/preview/api/ysql/datatypes/",
                "save_relpath": "docs/reference/workspace_library/technical_specs/yugabytedb/ysql-datatypes_20260402.html",
            },
        ],
    },
    "citus": {
        "display": "Citus",
        "profile_id": "citus",
        "spec_token": "CITUS",
        "suffix": "citus",
        "surface_class": "sql_wire",
        "protocol": "postgresql_frontend_backend",
        "shared_lowering": "postgresql_family",
        "listener_mode": "required",
        "supports_message_blr": False,
        "supports_executable_blr": False,
        "catalog_surface": "PostgreSQL-compatible catalogs plus `pg_dist_*` and distributed metadata overlays.",
        "bridge_surface": "internal PostgreSQL-compatible client used for migration and distributed validation.",
        "regression_surface": "PostgreSQL-wire fixtures plus Citus distributed SQL and admin-surface compatibility suites.",
        "donor_reason": "Citus uses PostgreSQL wire but adds distributed tables, metadata, and plan-shape surfaces that need a dedicated family layer.",
        "delta_bullets": [
            "Parser reuses PostgreSQL wire foundations but must preserve Citus-specific DDL, UDFs, metadata, and explain output.",
            "Compiler UDR must lower distributed helper SQL and coordinator-visible system probes without collapsing them into plain PostgreSQL text.",
            "Engine UDR must expose `pg_dist_*` overlays and empty-database distributed metadata defaults under the emulated root.",
        ],
        "web_docs": [
            {
                "title": "Citus Reference DDL",
                "purpose": "official donor distributed DDL reference",
                "url": "https://docs.citusdata.com/en/stable/develop/reference_ddl.html",
                "save_relpath": "docs/reference/workspace_library/technical_specs/citus/reference-ddl_20260402.html",
            },
            {
                "title": "Citus Table Management",
                "purpose": "official donor distributed metadata and admin reference",
                "url": "https://docs.citusdata.com/en/stable/admin_guide/table_management.html",
                "save_relpath": "docs/reference/workspace_library/technical_specs/citus/table-management_20260402.html",
            },
        ],
    },
    "apache_ignite": {
        "display": "Apache Ignite",
        "profile_id": "apache_ignite",
        "spec_token": "APACHE_IGNITE",
        "suffix": "ignite",
        "surface_class": "thin_client_sql",
        "protocol": "ignite_thin_client_protocol",
        "shared_lowering": "ignite_sql_family",
        "listener_mode": "required",
        "supports_message_blr": False,
        "supports_executable_blr": False,
        "catalog_surface": "`INFORMATION_SCHEMA`, system views, and cluster metadata overlays filtered to the emulated root.",
        "bridge_surface": "internal Ignite-compatible thin client used for migration and bridge UDR flows.",
        "regression_surface": "thin-client protocol fixtures, SQL corpus, and system-view compatibility suites.",
        "donor_reason": "Apache Ignite combines SQL with thin-client protocol, cluster metadata, and cache/table duality that require a dedicated family spec.",
        "delta_bullets": [
            "Parser must admit Ignite thin-client framing, SQL text, and donor-specific metadata/result carriers.",
            "Compiler UDR must lower Ignite-generated SQL and helper commands into shared AST/SBLR while preserving donor semantics.",
            "Engine UDR must publish Ignite system views and empty-database metadata defaults under the sandboxed schema root.",
        ],
        "web_docs": [
            {
                "title": "Apache Ignite Data Types",
                "purpose": "official donor datatype reference",
                "url": "https://ignite.apache.org/docs/ignite3/latest/sql-reference/data-types",
                "save_relpath": "docs/reference/workspace_library/technical_specs/apache_ignite/data-types_20260402.html",
            },
            {
                "title": "Apache Ignite Clients Overview",
                "purpose": "official donor client and protocol reference",
                "url": "https://ignite.apache.org/docs/ignite3/latest/developers-guide/clients/overview",
                "save_relpath": "docs/reference/workspace_library/technical_specs/apache_ignite/clients-overview_20260402.html",
            },
        ],
    },
}


def rel(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def slug(text: str) -> str:
    chars = []
    for ch in text.lower():
        if ch.isalnum():
            chars.append(ch)
    return "".join(chars)


def load_packet_manifest() -> dict[str, dict[str, str]]:
    manifest_path = PACKET_ROOT / "EMULATION_REFERENCE_PACKET_MANIFEST.csv"
    rows: dict[str, dict[str, str]] = {}
    with manifest_path.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            key = Path(row["packet_dir"]).name
            rows[key] = row
    return rows


def load_gap_manifest() -> dict[str, list[dict[str, str]]]:
    gap_path = PACKET_ROOT / "EMULATION_REFERENCE_GAP_MANIFEST.csv"
    grouped: dict[str, list[dict[str, str]]] = defaultdict(list)
    with gap_path.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            grouped[slug(row["engine"])].append(row)
    return grouped


def load_summary(packet_dir_name: str) -> dict[str, dict[str, str]]:
    summary_path = PACKET_ROOT / packet_dir_name / "source_authority_summary.csv"
    data: dict[str, dict[str, str]] = {}
    with summary_path.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            data[row["section"]] = row
    return data


def fetch_if_missing(url: str, destination: Path) -> str:
    if destination.exists():
        return "existing"
    destination.parent.mkdir(parents=True, exist_ok=True)
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    try:
        with urllib.request.urlopen(
            request, timeout=45, context=ssl.create_default_context()
        ) as response:
            destination.write_bytes(response.read())
        return "downloaded"
    except urllib.error.HTTPError as exc:
        return f"http_error_{exc.code}"
    except urllib.error.URLError as exc:
        return f"url_error_{exc.reason}"
    except Exception as exc:  # noqa: BLE001
        return f"error_{type(exc).__name__}"


def scaffold_fields(meta: dict[str, object]) -> dict[str, object]:
    profile_id = meta["profile_id"]
    suffix = meta["suffix"]
    return {
        "listener_executable": f"sb_listener_{suffix}",
        "parser_executable": f"sb_parser_{suffix}",
        "parser_package": f"sb_pkg_{profile_id}_parser",
        "compiler_package": f"sb_pkg_{profile_id}_compiler_udr",
        "emulation_package": f"sb_pkg_{profile_id}_emulation_udr",
        "compiler_entrypoint": f"compiler_{profile_id}",
        "engine_entrypoint": f"engine_{profile_id}",
        "bundle_contract_id": f"sb_emulation_bundle_{profile_id}/v2",
    }


def render_common_spec(engine_keys: list[str]) -> str:
    family_lines = []
    for key in engine_keys:
        meta = ENGINE_META[key]
        family_lines.append(
            f"- `{meta['display']}`: `{meta['profile_id']}` using `{meta['protocol']}` and `{meta['surface_class']}`."
        )
    return normalize_markdown(
        f"""\
        # Beta 2 Emulation Package Template And Family Deliverable Model

        ## Purpose
        Define the canonical Beta 2 emulation-package template used to implement every donor-facing compatibility family without moving donor-specific behavior into the ScratchBird core engine.

        ## Scope
        - Common packaging and readiness rules for every Beta 2 emulation family.
        - Separation of parser, compiler UDR, and engine emulation UDR responsibilities.
        - Required reference-library, conformance, catalog, plan, bridge, and error-map deliverables.

        ## Admitted Beta 2 Families
        {chr(10).join(family_lines)}

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
        static const scratchbird::core::EmulationPackageScaffold kTemplate = {{
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
        }};

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
        """
    )


def render_engine_spec(
    engine_key: str,
    packet_row: dict[str, str],
    gap_rows: list[dict[str, str]],
    summary_rows: dict[str, dict[str, str]],
    web_status_rows: list[dict[str, str]],
) -> str:
    meta = ENGINE_META[engine_key]
    scaffold = scaffold_fields(meta)
    web_manifest_rel = (
        PACKET_ROOT / engine_key / "OFFICIAL_WEB_REFERENCE_SUPPLEMENT.md"
    ).relative_to(ROOT)
    packet_readme_rel = (PACKET_ROOT / engine_key / "README.md").relative_to(ROOT)
    gap_lines = []
    donor_gaps = [row for row in gap_rows if row["source_kind"] == "donor"]
    scratchbird_gaps = [row for row in gap_rows if row["source_kind"] == "scratchbird"]
    comparison_gaps = [row for row in gap_rows if row["source_kind"] == "comparison"]
    if donor_gaps:
        for row in donor_gaps:
            gap_lines.append(f"- donor evidence gap: `{row['section']}` -> {row['detail']}")
    if comparison_gaps:
        for row in comparison_gaps:
            gap_lines.append(
                f"- comparison deepening required: `{row['section']}` -> {row['detail']}"
            )
    if scratchbird_gaps:
        for row in scratchbird_gaps:
            gap_lines.append(
                f"- current ScratchBird implementation gap: `{row['section']}` -> {row['detail']}"
            )
    if not gap_lines:
        gap_lines.append("- no packet-level gaps are currently recorded for this family.")

    summary_lines = []
    for section in SECTION_ORDER:
        row = summary_rows.get(section)
        if row is None:
            continue
        summary_lines.append(
            f"- {SECTION_LABELS[section]}: donor `{row['donor_paths_present']}/{row['donor_paths_total']}` paths, ScratchBird `{row['scratchbird_paths_present']}/{row['scratchbird_paths_total']}` paths."
        )

    web_available = sum(1 for row in web_status_rows if row["status"] in {"existing", "downloaded"})
    web_total = len(web_status_rows)
    return normalize_markdown(
        f"""\
        # Beta 2 Emulation Family Model - {meta['display']}

        ## Purpose
        Define the Beta 2 emulation bundle required to present ScratchBird as a donor-compatible `{meta['display']}` endpoint while keeping donor-specific parsing, protocol, catalog, bridge, and error behavior outside the core engine.

        ## Family Identity
        - display name: `{meta['display']}`
        - `profile_id`: `{meta['profile_id']}`
        - primary surface class: `{meta['surface_class']}`
        - primary donor protocol or carrier: `{meta['protocol']}`
        - shared lowering base: `{meta['shared_lowering']}`
        - listener mode: `{meta['listener_mode']}`
        - listener executable: `{scaffold['listener_executable']}`
        - parser executable: `{scaffold['parser_executable']}`
        - parser package: `{scaffold['parser_package']}`
        - compiler UDR package: `{scaffold['compiler_package']}`
        - emulation UDR package: `{scaffold['emulation_package']}`
        - compiler entrypoint: `{scaffold['compiler_entrypoint']}`
        - engine generator entrypoint: `{scaffold['engine_entrypoint']}`
        - bundle contract id: `{scaffold['bundle_contract_id']}`
        - supports `MESSAGE_BLR`: `{"yes" if meta['supports_message_blr'] else "no"}`
        - supports `EXECUTABLE_BLR`: `{"yes" if meta['supports_executable_blr'] else "no"}`

        ## Admission Reason
        {meta['donor_reason']}

        ## Authoritative Reference Inputs
        - local source-backed packet: `{packet_readme_rel.as_posix()}`
        - official donor web supplement: `{web_manifest_rel.as_posix()}`
        - error-code packet root: `{rel(ERROR_PACKET_README)}`
        - packet manifest state: protocol adapter `{packet_row['protocol_adapter']}`, parser agent `{packet_row['parser_agent']}`, bridge adapter `{packet_row['fdw_adapter']}`, catalog overlay `{packet_row['catalog_overlay']}`, compatibility suite `{packet_row['compat_suite']}`
        - official donor web references available now: `{web_available}/{web_total}`

        ## Current Reference Coverage Snapshot
        {chr(10).join(summary_lines)}

        ## Parser Package Contract
        1. Own the full `{meta['display']}` client-facing request lifecycle for `{meta['protocol']}`.
        2. Translate donor-visible datatypes, result metadata, errors, and plan requests into canonical ScratchBird AST, SBLR, and metrics packets.
        3. Keep all donor-visible session state, prepared handles, streaming state, and async or notice surfaces inside the parser process boundary for one connection only.
        4. Fail closed when the bundle is not `READY`, when the emulated database binding is unavailable, or when the request class is not admitted by the family capability set.

        ## Compiler UDR Contract
        1. `"{scaffold['compiler_entrypoint']}"` is the only engine-side entrypoint allowed to translate donor-generated text or helper payloads for `{meta['display']}`.
        2. It must reuse the same capability, AST, datatype, and function admission rules as the parser package.
        3. It must verify privilege class and request shape before emitting AST or SBLR.
        4. It must never render donor protocol frames or donor-visible text directly to a client.

        ## Engine Generator And Emulation UDR Contract
        1. `"{scaffold['engine_entrypoint']}"` owns the family-specific engine-side emulation support.
        2. It must bootstrap and validate: {meta['catalog_surface']}
        3. It must ship the internal donor client required for: {meta['bridge_surface']}
        4. It must provide migration, bridge, catalog-bootstrap, empty-database-default, and validation routines for this family.

        ## Donor-Specific Beta 2 Deltas
        {chr(10).join(f"- {line}" for line in meta['delta_bullets'])}

        ## Regression And Bridge Requirements
        - regression baseline: {meta['regression_surface']}
        - internal bridge requirement: {meta['bridge_surface']}
        - family plan renderer must convert canonical `RuntimePlan` output into donor-visible explain or profile formats for `{meta['display']}`.
        - family error map pack must cover every donor error code admitted by the local packet and official donor docs.

        ## Current Evidence Gaps To Preserve During Implementation
        {chr(10).join(gap_lines)}

        ## Sample Bundle Registration
        ```cpp
        static const scratchbird::core::EmulationPackageScaffold k{meta['spec_token'].title().replace('_', '')}Scaffold = {{
            "{meta['profile_id']}",
            "{scaffold['listener_executable']}",
            "{scaffold['parser_executable']}",
            "{scaffold['parser_package']}",
            "{scaffold['compiler_package']}",
            "{scaffold['emulation_package']}",
            "{scaffold['bundle_contract_id']}",
            true,
            true,
            {"true" if meta['supports_message_blr'] else "false"},
            {"true" if meta['supports_executable_blr'] else "false"},
        }};

        register_emulation_entrypoint("{scaffold['compiler_entrypoint']}", &{meta['spec_token'].title().replace('_', '')}Compiler::invoke);
        register_emulation_entrypoint("{scaffold['engine_entrypoint']}", &{meta['spec_token'].title().replace('_', '')}Engine::invoke);
        ```

        ## Beta 2 Completion Rule
        `{meta['display']}` is `READY` only after parser, compiler UDR, emulation UDR, web and local reference inputs, virtual catalog goldens, plan render goldens, donor error-map pack, and regression harness evidence are all present.
        """
    )


def render_packet_web_supplement(
    engine_key: str,
    summary_rows: dict[str, dict[str, str]],
    gap_rows: list[dict[str, str]],
    web_rows: list[dict[str, str]],
) -> tuple[str, str]:
    meta = ENGINE_META[engine_key]
    available = sum(1 for row in web_rows if row["status"] in {"existing", "downloaded"})
    total = len(web_rows)
    if any(row["source_kind"] == "donor" for row in gap_rows):
        verdict = "web_supplemented_local_clone_gap_remains"
    elif available == total and total > 0:
        verdict = "ready_for_beta2_family_spec"
    elif available > 0:
        verdict = "partially_supplemented_follow_up_possible"
    else:
        verdict = "web_capture_failed"

    summary_lines = []
    for section in SECTION_ORDER:
        row = summary_rows.get(section)
        if row is None:
            continue
        summary_lines.append(
            f"- {SECTION_LABELS[section]}: donor `{row['donor_file_total']}` files from `{row['donor_paths_present']}/{row['donor_paths_total']}` authority roots; ScratchBird `{row['scratchbird_file_total']}` files from `{row['scratchbird_paths_present']}/{row['scratchbird_paths_total']}` roots."
        )
    web_lines = []
    for row in web_rows:
        web_lines.append(
            f"- {row['title']}: `{row['status']}`\n  Source: {row['url']}\n  Saved: `{row['save_relpath']}`\n  Purpose: {row['purpose']}"
        )
    gap_lines = []
    if gap_rows:
        for row in gap_rows:
            gap_lines.append(
                f"- `{row['section']}` / `{row['source_kind']}` / `{row['status']}`: {row['detail']}"
            )
    else:
        gap_lines.append("- no gaps recorded in the current packet manifest.")
    markdown = normalize_markdown(
        f"""\
        # {meta['display']} Official Web Reference Supplement

        This supplement extends the local-source packet for `{meta['display']}` with official donor documentation captured from the public donor documentation set.

        ## Local Packet Baseline
        - packet directory: `{rel(PACKET_ROOT / engine_key)}`
        - packet README: `{rel(PACKET_ROOT / engine_key / 'README.md')}`
        - coverage summary:
        {chr(10).join(summary_lines)}

        ## Official Donor Web Sources
        {chr(10).join(web_lines)}

        ## Remaining Packet Gaps
        {chr(10).join(gap_lines)}

        ## Supplement Verdict
        - status: `{verdict}`
        - official donor pages available locally: `{available}/{total}`
        - use this supplement plus the local packet as the Beta 2 family reference baseline.
        """
    )
    return markdown, verdict


def update_simple_list_file(path: Path, required_line: str) -> None:
    content = path.read_text(encoding="utf-8")
    if required_line in content:
        return
    path.write_text(content.rstrip() + "\n" + required_line + "\n", encoding="utf-8")


def ensure_readme_entry(path: Path, anchor: str, new_line: str) -> None:
    content = path.read_text(encoding="utf-8")
    if new_line in content:
        return
    marker = anchor
    if marker not in content:
        return
    content = content.replace(marker, marker + "\n" + new_line, 1)
    path.write_text(content, encoding="utf-8")


def ensure_test_contract_entries() -> None:
    content = SECTION_TEST_CONTRACT.read_text(encoding="utf-8")
    if "### Suite U: Beta 2 Emulation Family Bundle And Reference Readiness" in content:
        return
    insertion = textwrap.dedent(
        """

        ### Suite U: Beta 2 Emulation Family Bundle And Reference Readiness
        - `U-001`: every Beta 2 family publishes parser, compiler UDR, and emulation UDR package manifests with one matching `bundle_contract_id`.
        - `U-002`: every Beta 2 family has both a local 1:1 packet and an official donor web supplement before listener exposure is enabled.
        - `U-003`: every Beta 2 family ships empty-database overlay goldens proving donor-visible system rows for a new logical database.
        - `U-004`: parser and compiler UDR lowering of shared donor fixtures are byte-identical at the emitted SBLR layer.
        - `U-005`: every Beta 2 family ships donor plan render goldens produced from canonical `RuntimePlan` inputs.
        - `U-006`: every Beta 2 family ships a donor error-map completeness artifact keyed by `error_ref_uuid`.
        - `U-007`: every Beta 2 family bridge client passes attestation, auth, and transaction-lifecycle tests without external donor client libraries.
        - `U-008`: any missing bundle part or missing reference artifact keeps the family in a deterministic not-ready state.
        """
    )
    marker = "## Required Fixtures"
    if marker in content:
        content = content.replace(marker, insertion + "\n" + marker, 1)
        path = SECTION_TEST_CONTRACT
        path.write_text(content, encoding="utf-8")


def sync_authoritative_inventory(spec_paths: list[Path]) -> None:
    current = {
        line.strip()
        for line in AUTHORITATIVE_INVENTORY.read_text(encoding="utf-8").splitlines()
        if line.strip()
    }
    for path in spec_paths:
        current.add(rel(path))
    AUTHORITATIVE_INVENTORY.write_text(
        "\n".join(sorted(current)) + "\n",
        encoding="utf-8",
    )


def main() -> None:
    packet_manifest = load_packet_manifest()
    gap_manifest = load_gap_manifest()
    engine_keys = sorted(ENGINE_META.keys(), key=lambda key: ENGINE_META[key]["display"].lower())

    web_manifest_rows: list[dict[str, str]] = []
    packet_manifest_rows: list[dict[str, str]] = []
    generated_spec_paths: list[Path] = [COMMON_SPEC]

    COMMON_SPEC.write_text(render_common_spec(engine_keys), encoding="utf-8")

    for engine_key in engine_keys:
        meta = ENGINE_META[engine_key]
        packet_row = packet_manifest[engine_key]
        summary_rows = load_summary(engine_key)
        gap_rows = gap_manifest.get(slug(meta["display"]), [])
        engine_packet_dir = PACKET_ROOT / engine_key
        engine_packet_dir.mkdir(parents=True, exist_ok=True)

        engine_web_rows: list[dict[str, str]] = []
        engine_manifest_path = engine_packet_dir / "OFFICIAL_WEB_REFERENCE_MANIFEST.csv"
        for doc in meta["web_docs"]:
            save_path = ROOT / doc["save_relpath"]
            status = fetch_if_missing(doc["url"], save_path)
            row = {
                "engine": meta["display"],
                "title": doc["title"],
                "purpose": doc["purpose"],
                "url": doc["url"],
                "save_relpath": doc["save_relpath"],
                "status": status,
            }
            engine_web_rows.append(row)
            web_manifest_rows.append(row)

        with engine_manifest_path.open("w", newline="", encoding="utf-8") as handle:
            writer = csv.DictWriter(
                handle,
                fieldnames=["engine", "title", "purpose", "url", "save_relpath", "status"],
            )
            writer.writeheader()
            writer.writerows(engine_web_rows)

        supplement_md, verdict = render_packet_web_supplement(
            engine_key, summary_rows, gap_rows, engine_web_rows
        )
        (engine_packet_dir / "OFFICIAL_WEB_REFERENCE_SUPPLEMENT.md").write_text(
            supplement_md, encoding="utf-8"
        )

        packet_manifest_rows.append(
            {
                "engine": meta["display"],
                "packet_dir": rel(engine_packet_dir),
                "official_web_docs_available": str(
                    sum(
                        1
                        for row in engine_web_rows
                        if row["status"] in {"existing", "downloaded"}
                    )
                ),
                "official_web_docs_total": str(len(engine_web_rows)),
                "supplement_verdict": verdict,
                "family_spec": rel(
                    SPEC_ROOT
                    / f"BETA2_EMULATION_FAMILY_{meta['spec_token']}_MODEL.md"
                ),
            }
        )

        spec_path = SPEC_ROOT / f"BETA2_EMULATION_FAMILY_{meta['spec_token']}_MODEL.md"
        spec_path.write_text(
            render_engine_spec(
                engine_key,
                packet_row,
                gap_rows,
                summary_rows,
                engine_web_rows,
            ),
            encoding="utf-8",
        )
        generated_spec_paths.append(spec_path)

    with PACKET_WEB_MANIFEST.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=[
                "engine",
                "packet_dir",
                "official_web_docs_available",
                "official_web_docs_total",
                "supplement_verdict",
                "family_spec",
            ],
        )
        writer.writeheader()
        writer.writerows(packet_manifest_rows)

    PACKET_WEB_INDEX.write_text(
        "# Official Web Reference Supplement Index\n\n"
        "This index records the family-local official donor web supplements that extend the local source-backed emulation packets.\n\n"
        + "\n".join(
            [
                f"- [{row['engine']}]({Path(row['packet_dir']).name}/OFFICIAL_WEB_REFERENCE_SUPPLEMENT.md): `{row['supplement_verdict']}` with `{row['official_web_docs_available']}/{row['official_web_docs_total']}` official docs captured."
                for row in packet_manifest_rows
            ]
        )
        + "\n",
        encoding="utf-8",
    )

    tech_lines = [
        "# Emulation Beta 2 Engine Web Sources 2026-04-02",
        "",
        "Official donor documentation reused or downloaded to strengthen the Beta 2 emulation-family packet set.",
        "",
    ]
    current_engine = None
    for row in sorted(web_manifest_rows, key=lambda item: (item["engine"].lower(), item["title"].lower())):
        if row["engine"] != current_engine:
            current_engine = row["engine"]
            tech_lines.extend([f"## {current_engine}", ""])
        tech_lines.extend(
            [
                f"- Source: {row['url']}",
                f"  Saved: `{row['save_relpath']}`",
                f"  Purpose: {row['purpose']}",
                f"  Status: `{row['status']}`",
            ]
        )
    TECH_EMULATION_WEB_SOURCES.write_text("\n".join(tech_lines) + "\n", encoding="utf-8")

    ensure_readme_entry(
        TECH_README,
        "## Contents",
        "- [Emulation Beta 2 Engine Web Sources 2026-04-02](EMULATION_BETA2_ENGINE_WEB_SOURCES_20260402.md)",
    )
    ensure_readme_entry(
        PACKET_README,
        "## Shared assets",
        "- `OFFICIAL_WEB_REFERENCE_SUPPLEMENT_INDEX.md`\n- `OFFICIAL_WEB_REFERENCE_SUPPLEMENT_MANIFEST.csv`",
    )
    ensure_readme_entry(
        SECTION_README,
        "## Entry Points",
        "- `BETA2_EMULATION_PACKAGE_TEMPLATE_AND_FAMILY_DELIVERABLE_MODEL.md`",
    )
    update_simple_list_file(
        TECH_WEB_MANIFEST,
        "- See `EMULATION_BETA2_ENGINE_WEB_SOURCES_20260402.md` for the Beta 2 donor-family supplement set.",
    )
    ensure_test_contract_entries()
    sync_authoritative_inventory(generated_spec_paths)


if __name__ == "__main__":
    main()
