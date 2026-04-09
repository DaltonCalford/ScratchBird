#!/usr/bin/env python3
from __future__ import annotations

import csv
import re
from collections import Counter, defaultdict
from pathlib import Path


ROOT = Path("/home/dcalford/CliWork/ScratchBird")
CLONES_LOCAL = ROOT / "docs/reference/project_clones/local_existing"
CLONES_REMOTE = ROOT / "docs/reference/project_clones/remote"
OUT_ROOT = (
    ROOT
    / "docs/reference/reference_library/emulation_1_to_1_engine_reference_packets_2026-04-02"
)

TYPE_SYSTEM = ROOT / "src/core/type_system.cpp"
CORE_TYPES = ROOT / "include/scratchbird/core/types.h"
INDEX_FACTORY = ROOT / "src/core/index_factory.cpp"
VIRTUAL_CATALOG = ROOT / "src/catalog/virtual_catalog.cpp"
BOOTSTRAP_TEST = ROOT / "tests/unit/test_catalog_database_bootstrap.cpp"
REFERENCE_LIBRARY_INDEX = ROOT / "docs/reference/reference_library/REFERENCE_PACKET_INDEX.md"
REFERENCE_LIBRARY_README = ROOT / "docs/reference/reference_library/README.md"


def rel(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def donor_clone_root(engine: dict) -> Path:
    base = engine.get("clone_base", "local_existing")
    if base == "remote":
        return CLONES_REMOTE / engine["clone"]
    return CLONES_LOCAL / engine["clone"]


ENGINE_META = [
    {
        "id": "firebird",
        "display": "FirebirdSQL",
        "type_key": "FIREBIRD",
        "scratchbird_profile": "firebirdsql",
        "clone": "firebird",
        "token": "firebird",
        "donor_sources": {
            "datatypes": [
                "src/jrd/fields.h",
                "src/common/dsc.h",
            ],
            "indexes": [
                "src/jrd/btr.cpp",
                "src/jrd/idx.cpp",
            ],
            "parser_ast": [
                "src/dsql/parse.y",
            ],
            "wire_protocol": [
                "src/remote/protocol.cpp",
                "src/remote/protocol.h",
            ],
            "authentication": [
                "src/auth/SecurityDatabase/LegacyManagement.epp",
                "src/auth/trusted/AuthSspi.cpp",
            ],
            "client_bridge": [
                "src/yvalve",
                "src/isql",
            ],
            "plan_output": [
                "src/jrd/optimizer/Optimizer.cpp",
                "src/dsql/StmtNodes.cpp",
            ],
            "error_codes": [
                "src/include/firebird/impl/msg/jrd.h",
                "src/include/gen/Firebird.pas",
            ],
            "page_optimizations": [
                "src/jrd/ods.h",
                "src/jrd/btr.cpp",
                "src/jrd/pag.cpp",
            ],
            "catalogs_bootstrap": [
                "src/jrd/ini.epp",
                "src/jrd/relations.h",
            ],
            "regression_tests": [
                "src/isql/tests",
                "src/jrd/tests",
                "src/common/tests",
            ],
        },
        "index_rows": [
            ("ascending_or_descending_btree", "BTREE", "SB_NATIVE_INDEX_PRESENT", "Firebird core ordered index surface"),
            ("expression_index", "BTREE", "SB_NATIVE_INDEX_PRESENT", "Expression trees are stored separately from physical family"),
        ],
    },
    {
        "id": "postgresql",
        "display": "PostgreSQL",
        "type_key": "POSTGRESQL",
        "scratchbird_profile": "postgresql",
        "clone": "postgresql",
        "token": "postgresql",
        "donor_sources": {
            "datatypes": [
                "src/include/catalog/pg_type.dat",
            ],
            "indexes": [
                "src/include/catalog/pg_am.dat",
                "src/backend/access",
            ],
            "parser_ast": [
                "src/backend/parser/gram.y",
            ],
            "wire_protocol": [
                "doc/src/sgml/protocol.sgml",
                "src/backend/tcop/postgres.c",
            ],
            "authentication": [
                "src/backend/libpq/auth.c",
                "doc/src/sgml/client-auth.sgml",
            ],
            "client_bridge": [
                "src/interfaces/libpq",
            ],
            "plan_output": [
                "src/backend/commands/explain.c",
            ],
            "error_codes": [
                "src/backend/utils/errcodes.txt",
            ],
            "page_optimizations": [
                "src/include/storage/bufpage.h",
                "src/backend/access/heap",
                "src/backend/access/nbtree",
            ],
            "catalogs_bootstrap": [
                "src/include/catalog",
                "src/backend/catalog",
                "src/bin/initdb",
            ],
            "regression_tests": [
                "src/test/regress/GNUmakefile",
                "src/test/isolation",
                "src/test/modules",
            ],
        },
        "index_rows": [
            ("btree", "BTREE", "SB_NATIVE_INDEX_PRESENT", "Shared ordered exact family"),
            ("hash", "HASH", "SB_NATIVE_INDEX_PRESENT", "Shared exact hash family"),
            ("gin", "GIN", "SB_NATIVE_INDEX_PRESENT", "Same named generalized inverted family"),
            ("gist", "GIST", "SB_NATIVE_INDEX_PRESENT", "Same named generalized search tree family"),
            ("spgist", "SPGIST", "SB_NATIVE_INDEX_PRESENT", "Same named space-partitioned family"),
            ("brin", "BRIN", "SB_NATIVE_INDEX_PRESENT", "Same named summary-range family"),
        ],
    },
    {
        "id": "mysql",
        "display": "MySQL",
        "type_key": "MYSQL",
        "scratchbird_profile": "mysql",
        "clone": "mysql",
        "token": "mysql",
        "donor_sources": {
            "datatypes": [
                "include/field_types.h",
                "sql/field.cc",
            ],
            "indexes": [
                "sql/sql_yacc.yy",
                "storage/innobase/include/dict0dict.h",
            ],
            "parser_ast": [
                "sql/sql_yacc.yy",
            ],
            "wire_protocol": [
                "sql/protocol_classic.cc",
                "sql-common/client.cc",
            ],
            "authentication": [
                "sql/auth/sql_authentication.cc",
                "sql/auth/sha2_password.cc",
            ],
            "client_bridge": [
                "libmysql",
                "client",
            ],
            "plan_output": [
                "sql/opt_explain.cc",
            ],
            "error_codes": [
                "share/messages_to_clients.txt",
            ],
            "page_optimizations": [
                "storage/innobase/include/page0page.h",
                "storage/innobase/include/dict0dict.h",
            ],
            "catalogs_bootstrap": [
                "scripts/mysql_system_tables.sql",
                "scripts/mysql_system_tables_data.sql",
            ],
            "regression_tests": [
                "mysql-test/mysql-test-run.pl",
                "mysql-test/t",
            ],
        },
        "index_rows": [
            ("btree", "BTREE", "SB_NATIVE_INDEX_PRESENT", "Shared ordered exact family"),
            ("hash", "HASH", "SB_NATIVE_INDEX_PRESENT", "Shared exact hash family"),
            ("fulltext", "FULLTEXT", "SB_NATIVE_INDEX_PRESENT", "Shared text-search family"),
            ("spatial", "RTREE", "SB_NATIVE_INDEX_PRESENT", "Spatial index mapped to SB R-tree family"),
        ],
    },
    {
        "id": "mariadb",
        "display": "MariaDB",
        "type_key": "MYSQL",
        "scratchbird_profile": "mariadb",
        "clone": "mariadb",
        "token": "mariadb",
        "donor_sources": {
            "datatypes": [
                "include/mysql_com.h",
                "sql/field.cc",
            ],
            "indexes": [
                "sql/sql_yacc.yy",
                "storage/innobase/include/dict0dict.h",
            ],
            "parser_ast": [
                "sql/sql_yacc.yy",
            ],
            "wire_protocol": [
                "sql/protocol.cc",
                "client",
            ],
            "authentication": [
                "sql/sql_acl.cc",
                "plugin/auth_gssapi",
            ],
            "client_bridge": [
                "libmariadb",
                "client",
            ],
            "plan_output": [
                "sql/sql_explain.cc",
            ],
            "error_codes": [
                "sql/share/errmsg-utf8.txt",
            ],
            "page_optimizations": [
                "storage/innobase/include/page0page.h",
                "storage/columnstore",
            ],
            "catalogs_bootstrap": [
                "scripts/mariadb_system_tables.sql",
                "scripts/mariadb_system_tables_data.sql",
            ],
            "regression_tests": [
                "mysql-test/mariadb-test-run.pl",
                "mysql-test",
            ],
        },
        "index_rows": [
            ("btree", "BTREE", "SB_NATIVE_INDEX_PRESENT", "Shared ordered exact family"),
            ("hash", "HASH", "SB_NATIVE_INDEX_PRESENT", "Shared exact hash family"),
            ("fulltext", "FULLTEXT", "SB_NATIVE_INDEX_PRESENT", "Shared text-search family"),
            ("spatial", "RTREE", "SB_NATIVE_INDEX_PRESENT", "Spatial index mapped to SB R-tree family"),
        ],
    },
    {
        "id": "cassandra",
        "display": "Cassandra",
        "type_key": "CASSANDRA",
        "scratchbird_profile": "cassandra",
        "clone": "cassandra",
        "token": "cassandra",
        "donor_sources": {
            "datatypes": [
                "src/java/org/apache/cassandra/db/marshal",
            ],
            "indexes": [
                "src/java/org/apache/cassandra/index",
            ],
            "parser_ast": [
                "src/antlr/Parser.g",
                "src/antlr/Cql.g",
            ],
            "wire_protocol": [
                "doc/native_protocol_v5.spec",
            ],
            "authentication": [
                "src/java/org/apache/cassandra/auth",
                "pylib/cqlshlib/authproviderhandling.py",
            ],
            "client_bridge": [
                "pylib/cqlshlib",
                "bin/cqlsh.py",
            ],
            "plan_output": [
                "src/java/org/apache/cassandra/cql3/statements",
                "src/java/org/apache/cassandra/service/ClientState.java",
            ],
            "error_codes": [
                "src/java/org/apache/cassandra/exceptions",
            ],
            "page_optimizations": [
                "src/java/org/apache/cassandra/db",
                "src/java/org/apache/cassandra/io/sstable",
            ],
            "catalogs_bootstrap": [
                "src/java/org/apache/cassandra/schema",
                "src/java/org/apache/cassandra/db/SystemKeyspace.java",
            ],
            "regression_tests": [
                "test/unit",
                "test/distributed",
                ".build/run-tests.sh",
            ],
        },
        "index_rows": [
            ("secondary_index", "HASH", "SB_GENERIC_RUNTIME_OR_PROFILE_REQUIRED", "No Cassandra-specific exact secondary index type in SB"),
            ("sasi", "CASSANDRA_SASI", "SB_NATIVE_INDEX_PRESENT", "Dedicated Cassandra SASI family exists"),
            ("sai", "CASSANDRA_SAI", "SB_NATIVE_INDEX_PRESENT", "Dedicated Cassandra SAI family exists"),
        ],
    },
    {
        "id": "clickhouse",
        "display": "ClickHouse",
        "type_key": "CLICKHOUSE",
        "scratchbird_profile": "clickhouse",
        "clone": "clickhouse",
        "token": "clickhouse",
        "donor_sources": {
            "datatypes": [
                "src/DataTypes",
            ],
            "indexes": [
                "src/Storages/MergeTree",
            ],
            "parser_ast": [
                "src/Parsers",
            ],
            "wire_protocol": [
                "src/Core/Protocol.cpp",
                "src/Server/TCPHandler.cpp",
            ],
            "authentication": [
                "src/Access",
            ],
            "client_bridge": [
                "src/Client/Connection.cpp",
            ],
            "plan_output": [
                "src/Interpreters/InterpreterExplainQuery.cpp",
            ],
            "error_codes": [
                "src/Common/ErrorCodes.cpp",
            ],
            "page_optimizations": [
                "src/Storages/MergeTree",
                "src/Storages/System",
            ],
            "catalogs_bootstrap": [
                "src/Storages/System",
                "src/Databases",
            ],
            "regression_tests": [
                "tests/queries",
            ],
        },
        "index_rows": [
            ("sorting_or_primary_key", "STL_SORT", "SB_GENERIC_RUNTIME_OR_PROFILE_REQUIRED", "Ordered storage key rather than standalone secondary index"),
            ("minmax", "ZONEMAP", "SB_NATIVE_INDEX_PRESENT", "Shared summary min/max family"),
            ("bloom_filter", "BLOOM", "SB_NATIVE_INDEX_PRESENT", "Shared summary-filter family"),
            ("ngrambf_v1", "NGRAM", "SB_NATIVE_INDEX_PRESENT", "Shared n-gram family"),
        ],
    },
    {
        "id": "duckdb",
        "display": "DuckDB",
        "type_key": "DUCKDB",
        "scratchbird_profile": "duckdb",
        "clone": "duckdb",
        "token": "duckdb",
        "donor_sources": {
            "datatypes": [
                "src/include/duckdb/common/types.hpp",
                "src/common/types",
            ],
            "indexes": [
                "src/storage/index.cpp",
                "src/execution/index",
                "src/storage/table_index_list.cpp",
            ],
            "parser_ast": [
                "src/parser/parser.cpp",
                "third_party/libpg_query",
            ],
            "wire_protocol": [
                "src/include/duckdb/main/client_context.hpp",
            ],
            "authentication": [
                "src/main/connection.cpp",
            ],
            "client_bridge": [
                "src/include/duckdb/main",
                "tools/shell",
            ],
            "plan_output": [
                "src/main/relation/explain_relation.cpp",
                "src/verification/explain_statement_verifier.cpp",
                "src/main/relation/explain_relation.cpp",
            ],
            "error_codes": [
                "src/common/exception.cpp",
            ],
            "page_optimizations": [
                "src/storage",
            ],
            "catalogs_bootstrap": [
                "src/function/table/system",
                "src/catalog/default",
            ],
            "regression_tests": [
                "test/sql",
            ],
        },
        "index_rows": [
            ("art", "ART", "SB_NATIVE_INDEX_PRESENT", "Dedicated ART family exists"),
            ("zonemap", "ZONEMAP", "SB_NATIVE_INDEX_PRESENT", "Dedicated summary min/max family exists"),
        ],
    },
    {
        "id": "influxdb",
        "display": "InfluxDB",
        "type_key": "INFLUXDB",
        "scratchbird_profile": "influxdb",
        "clone": "influxdb",
        "token": "influxdb",
        "donor_sources": {
            "datatypes": [
                "influxdb3_types/src",
                "influxdb3_write/src",
            ],
            "indexes": [
                "influxdb3_write/src",
                "influxdb3_cache/src",
            ],
            "parser_ast": [
                "iox_query_influxql_rewrite/src",
                "influxdb3_server/src",
            ],
            "wire_protocol": [
                "influxdb3_server/src",
                "influxdb3_client/src",
            ],
            "authentication": [
                "influxdb3_authz/src",
            ],
            "client_bridge": [
                "influxdb3_client/src",
                "influxdb3_py_api/src",
            ],
            "plan_output": [
                "influxdb3_query_executor/src",
            ],
            "error_codes": [
                "influxdb3_server/src",
            ],
            "page_optimizations": [
                "influxdb3_wal/src",
                "object_store_utils/src",
            ],
            "catalogs_bootstrap": [
                "influxdb3_catalog/src",
                "influxdb3_system_tables/src",
            ],
            "regression_tests": [
                "influxdb3/tests",
                "influxdb3_server/tests",
                "run-tests.sh",
            ],
        },
        "index_rows": [],
    },
    {
        "id": "mongodb",
        "display": "MongoDB",
        "type_key": "MONGODB",
        "scratchbird_profile": "mongodb",
        "clone": "mongodb",
        "token": "mongodb",
        "donor_sources": {
            "datatypes": [
                "src/mongo/bson",
            ],
            "indexes": [
                "src/mongo/db/index",
                "src/mongo/db/query",
            ],
            "parser_ast": [
                "src/mongo/db/commands",
                "src/mongo/idl",
            ],
            "wire_protocol": [
                "src/mongo/rpc",
            ],
            "authentication": [
                "src/mongo/db/auth",
            ],
            "client_bridge": [
                "src/mongo/client",
            ],
            "plan_output": [
                "src/mongo/db/query/explain.cpp",
                "src/mongo/db/query/plan_explainer_impl.cpp",
            ],
            "error_codes": [
                "src/mongo/base/error_codes.yml",
            ],
            "page_optimizations": [
                "src/mongo/db/storage",
                "src/third_party/wiredtiger",
            ],
            "catalogs_bootstrap": [
                "src/mongo/db",
                "src/mongo/db/s",
            ],
            "regression_tests": [
                "jstests",
            ],
        },
        "index_rows": [
            ("default_btree", "BTREE", "SB_GENERIC_RUNTIME_OR_PROFILE_REQUIRED", "MongoDB default exact secondary index can ride SB ordered family"),
            ("2d", "MONGODB_2D", "SB_NATIVE_INDEX_PRESENT", "Dedicated MongoDB planar geo family exists"),
            ("2dsphere", "MONGODB_2DSPHERE", "SB_NATIVE_INDEX_PRESENT", "Dedicated MongoDB spherical geo family exists"),
            ("geo_haystack", "MONGODB_GEO_HAYSTACK", "SB_NATIVE_INDEX_PRESENT", "Dedicated MongoDB geoHaystack family exists"),
            ("wildcard", "MONGODB_WILDCARD", "SB_NATIVE_INDEX_PRESENT", "Dedicated MongoDB wildcard family exists"),
            ("encrypted_range", "MONGODB_ENCRYPTED_RANGE", "SB_NATIVE_INDEX_PRESENT", "Dedicated encrypted range family exists"),
            ("text", "FULLTEXT", "SB_GENERIC_RUNTIME_OR_PROFILE_REQUIRED", "Can ride SB text-search family"),
            ("hashed", "HASH", "SB_GENERIC_RUNTIME_OR_PROFILE_REQUIRED", "Can ride SB exact hash family"),
        ],
    },
    {
        "id": "neo4j",
        "display": "Neo4j",
        "type_key": "NEO4J",
        "scratchbird_profile": "neo4j",
        "clone": "neo4j",
        "token": "neo4j",
        "donor_sources": {
            "datatypes": [
                "community/values",
                "community/graphdb-api",
            ],
            "indexes": [
                "community/index",
                "community/fulltext-index",
                "community/spatial-index",
            ],
            "parser_ast": [
                "community/cypher",
            ],
            "wire_protocol": [
                "community/bolt",
            ],
            "authentication": [
                "community/security",
            ],
            "client_bridge": [
                "community/graphdb-api",
                "community/server",
            ],
            "plan_output": [
                "community/cypher",
                "community/kernel",
            ],
            "error_codes": [
                "community/neo4j-exceptions",
                "community/kernel",
            ],
            "page_optimizations": [
                "community/index",
                "community/kernel",
            ],
            "catalogs_bootstrap": [
                "community/kernel",
                "community/security",
            ],
            "regression_tests": [
                "community/kernel-test",
                "community/gbptree-tests",
            ],
        },
        "index_rows": [
            ("lookup", "NEO4J_LOOKUP", "SB_NATIVE_INDEX_PRESENT", "Dedicated Neo4j lookup family exists"),
            ("range", "NEO4J_RANGE", "SB_NATIVE_INDEX_PRESENT", "Dedicated Neo4j range family exists"),
            ("text", "NEO4J_TEXT", "SB_NATIVE_INDEX_PRESENT", "Dedicated Neo4j text family exists"),
            ("point", "NEO4J_POINT", "SB_NATIVE_INDEX_PRESENT", "Dedicated Neo4j point family exists"),
            ("vector", "NEO4J_VECTOR", "SB_NATIVE_INDEX_PRESENT", "Dedicated Neo4j vector family exists"),
        ],
    },
    {
        "id": "opensearch",
        "display": "OpenSearch",
        "type_key": "OPENSEARCH",
        "scratchbird_profile": "opensearch",
        "clone": "opensearch",
        "token": "opensearch",
        "donor_sources": {
            "datatypes": [
                "server/src/main/java/org/opensearch/index/mapper",
            ],
            "indexes": [
                "server/src/main/java/org/opensearch/index",
                "server/src/main/java/org/opensearch/search",
            ],
            "parser_ast": [
                "server/src/main/java/org/opensearch/index/query",
                "server/src/main/java/org/opensearch/rest",
            ],
            "wire_protocol": [
                "server/src/main/java/org/opensearch/rest",
                "server/src/main/java/org/opensearch/transport",
            ],
            "authentication": [
                "plugins",
                "server/src/main/java/org/opensearch/rest",
            ],
            "client_bridge": [
                "client",
                "server/src/main/java/org/opensearch/rest",
            ],
            "plan_output": [
                "server/src/main/java/org/opensearch/action/admin/indices/validate/query",
                "server/src/main/java/org/opensearch/search",
            ],
            "error_codes": [
                "server/src/main/java/org/opensearch/OpenSearchServerException.java",
                "server/src/main/java/org/opensearch/ResourceNotFoundException.java",
            ],
            "page_optimizations": [
                "server/src/main/java/org/opensearch/index",
                "server/src/main/java/org/opensearch/indices",
            ],
            "catalogs_bootstrap": [
                "server/src/main/java/org/opensearch/cluster/metadata",
            ],
            "regression_tests": [
                "server/src/test",
            ],
        },
        "index_rows": [
            ("inverted", "INVERTED", "SB_NATIVE_INDEX_PRESENT", "Dedicated inverted family exists"),
            ("ngram", "NGRAM", "SB_NATIVE_INDEX_PRESENT", "Dedicated n-gram family exists"),
            ("prefix_or_search_as_you_type", "TRIE", "SB_GENERIC_RUNTIME_OR_PROFILE_REQUIRED", "Trie-like path available in SB"),
            ("vector_hnsw", "HNSW", "SB_NATIVE_INDEX_PRESENT", "Dedicated ANN family exists"),
            ("geo", "RTREE", "SB_GENERIC_RUNTIME_OR_PROFILE_REQUIRED", "Geo path can ride SB generalized spatial family"),
        ],
    },
    {
        "id": "redis",
        "display": "Redis",
        "type_key": "REDIS",
        "scratchbird_profile": "redis",
        "clone": "redis",
        "token": "redis",
        "donor_sources": {
            "datatypes": [
                "src/object.c",
                "src/server.h",
            ],
            "indexes": [
                "src/db.c",
                "src/t_zset.c",
                "src/rax.c",
            ],
            "parser_ast": [
                "src/server.c",
                "src/networking.c",
            ],
            "wire_protocol": [
                "src/networking.c",
                "src/resp_parser.c",
            ],
            "authentication": [
                "src/acl.c",
            ],
            "client_bridge": [
                "src/networking.c",
                "src/cluster.c",
            ],
            "plan_output": [
                "src/server.c",
            ],
            "error_codes": [
                "src/server.c",
                "src/networking.c",
            ],
            "page_optimizations": [
                "src/object.c",
                "src/quicklist.c",
                "src/listpack.c",
            ],
            "catalogs_bootstrap": [
                "src/db.c",
                "src/server.c",
            ],
            "regression_tests": [
                "tests",
            ],
        },
        "index_rows": [
            ("string", "REDIS_STRING", "SB_NATIVE_INDEX_PRESENT", "Dedicated Redis string family exists"),
            ("hash", "REDIS_HASH", "SB_NATIVE_INDEX_PRESENT", "Dedicated Redis hash family exists"),
            ("list", "REDIS_LIST", "SB_NATIVE_INDEX_PRESENT", "Dedicated Redis list family exists"),
            ("set", "REDIS_SET", "SB_NATIVE_INDEX_PRESENT", "Dedicated Redis set family exists"),
            ("zset", "REDIS_ZSET", "SB_NATIVE_INDEX_PRESENT", "Dedicated Redis sorted-set family exists"),
            ("stream", "REDIS_STREAM", "SB_NATIVE_INDEX_PRESENT", "Dedicated Redis stream family exists"),
            ("bitmap", "REDIS_BITMAP", "SB_NATIVE_INDEX_PRESENT", "Dedicated Redis bitmap family exists"),
            ("hll", "REDIS_HLL", "SB_NATIVE_INDEX_PRESENT", "Dedicated Redis HLL family exists"),
            ("geo", "REDIS_GEO", "SB_NATIVE_INDEX_PRESENT", "Dedicated Redis geo family exists"),
        ],
    },
    {
        "id": "milvus",
        "display": "Milvus",
        "type_key": "MILVUS",
        "scratchbird_profile": "milvus",
        "clone_base": "local_existing",
        "clone": "milvus",
        "token": "milvus",
        "donor_sources": {
            "datatypes": [
                "internal/core/src/common/Types.h",
                "pkg/proto",
            ],
            "indexes": [
                "internal/core/src/index",
                "internal/proxy",
            ],
            "parser_ast": [
                "internal/proxy",
                "internal/http",
            ],
            "wire_protocol": [
                "pkg/proto",
                "internal/distributed/proxy",
                "internal/http",
            ],
            "authentication": [
                "internal/proxy",
                "internal/rootcoord",
            ],
            "client_bridge": [
                "client",
                "pkg/proto",
            ],
            "plan_output": [
                "internal/proxy",
                "internal/querynodev2",
            ],
            "error_codes": [
                "pkg/proto",
                "internal/util",
            ],
            "page_optimizations": [
                "internal/core/src/index",
                "internal/core/src/storage",
            ],
            "catalogs_bootstrap": [
                "internal/rootcoord",
                "pkg/proto",
            ],
            "regression_tests": [
                "tests",
            ],
        },
        "index_rows": [
            ("flat", "VECTOR_FLAT", "SB_NATIVE_INDEX_PRESENT", "Dedicated flat vector family exists"),
            ("bin_flat", "VECTOR_BIN_FLAT", "SB_NATIVE_INDEX_PRESENT", "Dedicated binary flat vector family exists"),
            ("ivf_flat", "IVF_FLAT", "SB_NATIVE_INDEX_PRESENT", "Dedicated IVF flat family exists"),
            ("bin_ivf_flat", "BIN_IVF_FLAT", "SB_NATIVE_INDEX_PRESENT", "Dedicated IVF binary family exists"),
            ("ivf_pq", "IVF_PQ", "SB_NATIVE_INDEX_PRESENT", "Dedicated IVF PQ family exists"),
            ("ivf_sq8", "IVF_SQ8", "SB_NATIVE_INDEX_PRESENT", "Dedicated IVF SQ8 family exists"),
            ("hnsw", "HNSW", "SB_NATIVE_INDEX_PRESENT", "Dedicated HNSW family exists"),
            ("annoy", "ANNOY", "SB_NATIVE_INDEX_PRESENT", "Dedicated ANNOY family exists"),
            ("nsg", "NSG", "SB_NATIVE_INDEX_PRESENT", "Dedicated NSG family exists"),
            ("diskann", "DISKANN", "SB_NATIVE_INDEX_PRESENT", "Dedicated DiskANN family exists"),
            ("scann", "SCANN", "SB_NATIVE_INDEX_PRESENT", "Dedicated ScaNN family exists"),
            ("gpu_cagra", "GPU_CAGRA", "SB_NATIVE_INDEX_PRESENT", "Dedicated GPU CAGRA family exists"),
        ],
    },
    {
        "id": "sqlite",
        "display": "SQLite",
        "type_key": "SQLITE",
        "scratchbird_profile": "sqlite",
        "clone_base": "remote",
        "clone": "sqlite",
        "token": "sqlite",
        "packet_scope_note": "No standalone SQLite donor clone was found under the local reference-clone tree in this checkout. This packet records the expected donor authority paths as missing evidence instead of inferring support.",
        "donor_sources": {
            "datatypes": [
                "src/parse.y",
                "src/sqliteInt.h",
            ],
            "indexes": [
                "src/build.c",
                "src/btree.c",
            ],
            "parser_ast": [
                "src/parse.y",
            ],
            "wire_protocol": [
                "src/shell.c",
            ],
            "authentication": [
                "src/auth.c",
            ],
            "client_bridge": [
                "src/shell.c",
            ],
            "plan_output": [
                "src/explain.c",
                "src/select.c",
            ],
            "error_codes": [
                "src/sqliteInt.h",
                "src/main.c",
            ],
            "page_optimizations": [
                "src/pager.c",
                "src/btree.c",
            ],
            "catalogs_bootstrap": [
                "src/build.c",
                "src/pragma.c",
            ],
            "regression_tests": [
                "test",
            ],
        },
        "index_rows": [],
    },
    {
        "id": "dolt",
        "display": "Dolt",
        "type_key": "MYSQL",
        "scratchbird_profile": "dolt",
        "clone_base": "remote",
        "clone": "dolt",
        "token": "dolt",
        "family_reference_packet": "mysql",
        "packet_scope_note": "Dolt exposes a MySQL-compatible SQL surface plus Dolt-specific versioning, diff, merge, and branch semantics. This donor packet records Dolt-local evidence and should be read alongside the MySQL family packet for shared wire and base SQL compatibility.",
        "donor_sources": {
            "datatypes": [
                "go/libraries/doltcore/schema/typeinfo",
                "go/libraries/doltcore/sqle",
            ],
            "indexes": [
                "go/libraries/doltcore/schema/index.go",
                "go/store/prolly",
            ],
            "parser_ast": [
                "go/libraries/doltcore/sqle",
                "go/cmd/dolt/commands/sql.go",
            ],
            "wire_protocol": [
                "integration-tests/mysql-client-tests",
                "go/libraries/doltcore/sqle",
            ],
            "authentication": [
                "go/libraries/doltcore/sqle",
                "integration-tests/bats/mutual-tls-auth.bats",
            ],
            "client_bridge": [
                "integration-tests/mysql-client-tests",
                "go/cmd/dolt/commands/sql.go",
            ],
            "plan_output": [
                "go/libraries/doltcore/sqle",
                "integration-tests/bats/sql-shell.bats",
            ],
            "error_codes": [
                "go/libraries/doltcore/sqle",
            ],
            "page_optimizations": [
                "go/store/prolly",
                "go/store",
            ],
            "catalogs_bootstrap": [
                "go/libraries/doltcore/doltdb/system_table.go",
                "go/libraries/doltcore/sqle",
                "integration-tests/bats/system-tables.bats",
            ],
            "regression_tests": [
                "integration-tests/bats",
                "integration-tests/mysql-client-tests",
                "integration-tests/compatibility",
                "integration-tests/transactions",
            ],
        },
        "index_rows": [],
    },
    {
        "id": "foundationdb",
        "display": "FoundationDB",
        "type_key": "FOUNDATIONDB",
        "scratchbird_profile": "foundationdb",
        "clone_base": "remote",
        "clone": "foundationdb",
        "token": "foundationdb",
        "packet_scope_note": "FoundationDB is represented in this packet as a transactional key-value substrate with tuple, special-key-space, client API, and system-key authorities. It is not reduced to a relational SQL engine model.",
        "donor_sources": {
            "datatypes": [
                "fdbclient/include/fdbclient/Tuple.h",
                "fdbclient/include/fdbclient/FDBTypes.h",
                "fdbclient/TupleVersionstamp.cpp",
            ],
            "indexes": [
                "fdbclient/include/fdbclient/SpecialKeySpace.h",
                "fdbclient/SystemData.cpp",
            ],
            "parser_ast": [
                "fdbcli",
            ],
            "wire_protocol": [
                "fdbrpc",
                "flow/protocolversion",
            ],
            "authentication": [
                "fdbrpc/tests/AuthzTlsTest.cpp",
                "fdbclient/include/fdbclient/BackupTLSConfig.h",
                "fdbserver",
            ],
            "client_bridge": [
                "bindings",
                "fdbcli",
            ],
            "plan_output": [
                "fdbcli",
                "fdbclient/ManagementAPI.cpp",
            ],
            "error_codes": [
                "flow/include/flow/error_definitions.h",
            ],
            "page_optimizations": [
                "fdbclient/SystemData.cpp",
                "fdbserver",
            ],
            "catalogs_bootstrap": [
                "fdbclient/SystemData.cpp",
                "fdbclient/include/fdbclient/SystemData.h",
                "fdbclient/include/fdbclient/SpecialKeySpace.h",
            ],
            "regression_tests": [
                "tests",
                "fdbcli/tests",
                "bindings/bindingtester",
            ],
        },
        "index_rows": [],
    },
    {
        "id": "vitess",
        "display": "Vitess",
        "type_key": "MYSQL",
        "scratchbird_profile": "vitess",
        "clone_base": "remote",
        "clone": "vitess",
        "token": "vitess",
        "family_reference_packet": "mysql",
        "packet_scope_note": "Vitess fronts MySQL-compatible SQL and wire semantics with vtgate, vttablet, and vindex routing layers. This packet records Vitess-local authorities and uses the MySQL family packet as the core compatibility baseline.",
        "donor_sources": {
            "datatypes": [
                "go/sqltypes/type.go",
                "go/vt/proto/query/query.pb.go",
                "go/vt/sqlparser/sql.y",
            ],
            "indexes": [
                "go/vt/sqlparser/sql.y",
                "go/vt/vtgate/vindexes",
                "go/vt/proto/vschema",
            ],
            "parser_ast": [
                "go/vt/sqlparser",
            ],
            "wire_protocol": [
                "go/mysql",
                "go/vt/proto",
            ],
            "authentication": [
                "go/mysql",
                "go/vt/vtgate",
                "data/test/mysql_ldap_auth_config.json",
            ],
            "client_bridge": [
                "go/cmd/vtgateclienttest",
                "java/jdbc",
                "test/client_test.sh",
            ],
            "plan_output": [
                "go/vt/vtgate",
                "examples/vtexplain",
            ],
            "error_codes": [
                "go/vt/vterrors",
                "go/mysql",
            ],
            "page_optimizations": [
                "go/vt/vttablet",
                "go/vt/vtgate",
            ],
            "catalogs_bootstrap": [
                "go/vt/sidecardb/schema",
                "go/vt/vtgate",
                "data/test/schema",
                "go/vt/proto/vschema",
            ],
            "regression_tests": [
                "go/test",
                "test",
                "go/test/endtoend/mysqlserver",
            ],
        },
        "index_rows": [],
    },
    {
        "id": "immudb",
        "display": "immudb",
        "type_key": "IMMUDB",
        "scratchbird_profile": "immudb",
        "clone_base": "remote",
        "clone": "immudb",
        "token": "immudb",
        "packet_scope_note": "immudb is represented here as an immutable SQL/document store with embedded SQL catalog, append-only store, and gRPC client protocol surfaces.",
        "donor_sources": {
            "datatypes": [
                "embedded/sql/stmt.go",
                "embedded/sql/sql_grammar.y",
            ],
            "indexes": [
                "embedded/sql/sql_grammar.y",
                "embedded/sql/catalog.go",
                "embedded/tbtree",
            ],
            "parser_ast": [
                "embedded/sql/sql_grammar.y",
                "embedded/sql/sql_parser.go",
            ],
            "wire_protocol": [
                "pkg/api",
                "pkg/client",
            ],
            "authentication": [
                "pkg/auth",
            ],
            "client_bridge": [
                "pkg/client",
                "cmd/immuclient",
            ],
            "plan_output": [
                "embedded/sql",
            ],
            "error_codes": [
                "pkg/errors",
                "embedded/sql",
            ],
            "page_optimizations": [
                "embedded/tbtree",
                "embedded/store",
            ],
            "catalogs_bootstrap": [
                "embedded/sql/catalog.go",
                "pkg/database",
            ],
            "regression_tests": [
                "test",
                "cmd/cmdtest",
            ],
        },
        "index_rows": [],
    },
    {
        "id": "xtdb",
        "display": "XTDB",
        "type_key": "XTDB",
        "scratchbird_profile": "xtdb",
        "clone_base": "remote",
        "clone": "xtdb",
        "token": "xtdb",
        "packet_scope_note": "XTDB is represented as a temporal SQL platform with XTQL, pgwire, Flight SQL, and trie/block catalog surfaces rather than a classic static SQL-catalog bootstrap.",
        "donor_sources": {
            "datatypes": [
                "core/src/main/clojure/xtdb/sql.clj",
                "core/src/main/clojure/xtdb/sql/parse.clj",
            ],
            "indexes": [
                "core/src/main/clojure/xtdb/trie_catalog.clj",
                "core/src/main/clojure/xtdb/block_catalog.clj",
            ],
            "parser_ast": [
                "core/src/main/clojure/xtdb/sql/parse.clj",
                "core/src/main/clojure/xtdb/sql.clj",
            ],
            "wire_protocol": [
                "core/src/main/clojure/xtdb/pgwire.clj",
                "core/src/main/clojure/xtdb/pgwire/io.clj",
                "core/src/main/clojure/xtdb/flight_sql.clj",
            ],
            "authentication": [
                "core/src/main/clojure/xtdb/authn.clj",
            ],
            "client_bridge": [
                "api/src",
                "core/src/main/clojure/xtdb/pgwire.clj",
                "core/src/main/clojure/xtdb/flight_sql.clj",
            ],
            "plan_output": [
                "src/test/clojure/xtdb/xtql/plan_test.clj",
                "src/test/clojure/xtdb/logical_plan_test.clj",
            ],
            "error_codes": [
                "core/src/main/clojure/xtdb/pgwire.clj",
                "core/src/main/clojure/xtdb/sql.clj",
            ],
            "page_optimizations": [
                "core/src/main/clojure/xtdb/block_catalog.clj",
                "core/src/main/clojure/xtdb/trie_catalog.clj",
            ],
            "catalogs_bootstrap": [
                "core/src/main/clojure/xtdb/information_schema.clj",
                "core/src/main/clojure/xtdb/table_catalog.clj",
                "core/src/main/clojure/xtdb/db_catalog.clj",
                "core/src/main/clojure/xtdb/trie_catalog.clj",
            ],
            "regression_tests": [
                "src/test",
                "core/src/test",
            ],
        },
        "index_rows": [],
    },
    {
        "id": "tidb",
        "display": "TiDB",
        "type_key": "MYSQL",
        "scratchbird_profile": "tidb",
        "clone_base": "remote",
        "clone": "tidb",
        "token": "tidb",
        "family_reference_packet": "mysql",
        "packet_scope_note": "TiDB exposes a MySQL-compatible SQL and wire surface while adding its own planner, infoschema, DDL, and distributed execution layers. This packet records TiDB-local authorities and uses the MySQL packet as the shared baseline where the donor reuses MySQL semantics.",
        "donor_sources": {
            "datatypes": [
                "pkg/parser/mysql/type.go",
                "pkg/parser/parser.y",
                "pkg/types",
            ],
            "indexes": [
                "pkg/parser/parser.y",
                "pkg/planner/core",
            ],
            "parser_ast": [
                "pkg/parser/parser.y",
                "pkg/parser/ast",
            ],
            "wire_protocol": [
                "pkg/server/conn.go",
                "pkg/server/internal",
            ],
            "authentication": [
                "pkg/privilege",
                "pkg/server/conn.go",
            ],
            "client_bridge": [
                "pkg/server",
                "br",
            ],
            "plan_output": [
                "pkg/planner/core",
            ],
            "error_codes": [
                "errors.toml",
                "pkg/util/dbterror",
            ],
            "page_optimizations": [
                "pkg/store",
                "pkg/table",
            ],
            "catalogs_bootstrap": [
                "pkg/infoschema",
                "pkg/session/bootstrap.go",
            ],
            "regression_tests": [
                "tests/integrationtest",
                "br/tests",
            ],
        },
        "index_rows": [],
    },
    {
        "id": "cockroachdb",
        "display": "CockroachDB",
        "type_key": "POSTGRESQL",
        "scratchbird_profile": "cockroachdb",
        "clone_base": "remote",
        "clone": "cockroachdb",
        "token": "cockroachdb",
        "family_reference_packet": "postgresql",
        "packet_scope_note": "CockroachDB speaks a PostgreSQL-style wire protocol but owns its own parser, optimizer, catalog, distributed execution, and pgcode layers. This packet keeps the donor-local sources explicit instead of collapsing them into plain PostgreSQL.",
        "donor_sources": {
            "datatypes": [
                "pkg/sql/sem/tree",
                "pkg/sql/types",
            ],
            "indexes": [
                "pkg/sql/catalog",
                "pkg/sql/opt",
            ],
            "parser_ast": [
                "pkg/sql/parser",
                "pkg/sql/sem/tree",
            ],
            "wire_protocol": [
                "pkg/sql/pgwire",
            ],
            "authentication": [
                "pkg/security",
                "pkg/sql/pgwire",
            ],
            "client_bridge": [
                "pkg/sql/pgwire",
                "pkg/cli",
            ],
            "plan_output": [
                "pkg/sql/opt",
                "pkg/sql",
            ],
            "error_codes": [
                "pkg/sql/pgwire/pgcode",
                "pkg/sql/pgwire/pgerror",
            ],
            "page_optimizations": [
                "pkg/storage",
                "pkg/sql/catalog",
            ],
            "catalogs_bootstrap": [
                "pkg/sql/catalog",
                "pkg/sql/catalog/systemschema",
                "pkg/sql/catalog/bootstrap/metadata.go",
            ],
            "regression_tests": [
                "pkg/sql/logictest",
                "pkg/cmd/roachtest",
            ],
        },
        "index_rows": [],
    },
    {
        "id": "yugabytedb",
        "display": "YugabyteDB",
        "type_key": "POSTGRESQL",
        "scratchbird_profile": "yugabytedb",
        "clone_base": "remote",
        "clone": "yugabytedb",
        "token": "yugabytedb",
        "family_reference_packet": "postgresql",
        "packet_scope_note": "YugabyteDB carries an embedded PostgreSQL front-end plus DocDB, Odyssey, and distributed tablet layers. This packet extracts the PostgreSQL-derived frontend from the donor repo directly and records the Yugabyte-specific routing and storage authorities alongside it.",
        "donor_sources": {
            "datatypes": [
                "src/postgres/src/include/catalog/pg_type.dat",
            ],
            "indexes": [
                "src/postgres/src/include/catalog/pg_am.dat",
                "src/postgres/src/backend/access",
            ],
            "parser_ast": [
                "src/postgres/src/backend/parser/gram.y",
            ],
            "wire_protocol": [
                "src/postgres/src/backend/tcop/postgres.c",
                "src/odyssey/sources",
            ],
            "authentication": [
                "src/postgres/src/backend/libpq/auth.c",
                "src/odyssey/sources/auth.c",
            ],
            "client_bridge": [
                "src/postgres/src/interfaces/libpq",
                "src/odyssey/sources",
            ],
            "plan_output": [
                "src/postgres/src/backend/commands/explain.c",
            ],
            "error_codes": [
                "src/postgres/src/backend/utils/errcodes.txt",
                "src/odyssey/third_party/kiwi/kiwi/error_codes.h",
            ],
            "page_optimizations": [
                "src/postgres/src/include/storage/bufpage.h",
                "src/yb/docdb",
            ],
            "catalogs_bootstrap": [
                "src/postgres/src/include/catalog",
                "src/postgres/src/backend/catalog",
                "src/postgres/src/bin/initdb",
            ],
            "regression_tests": [
                "src/postgres/src/test/regress",
                "src/postgres/src/test/isolation",
                "src/postgres/third-party-extensions/pgvector/test",
            ],
        },
        "index_rows": [],
    },
    {
        "id": "citus",
        "display": "Citus",
        "type_key": "POSTGRESQL",
        "scratchbird_profile": "citus",
        "clone_base": "remote",
        "clone": "citus",
        "token": "citus",
        "family_reference_packet": "postgresql",
        "packet_scope_note": "The local Citus clone is an extension overlay, not a full PostgreSQL source tree. This packet therefore records Citus-owned distributed planner, metadata, error, CDC, and regression surfaces, and relies on the PostgreSQL packet for shared core frontend semantics that are not vendored in the Citus repo.",
        "donor_sources": {
            "datatypes": [
                "src/include/distributed/type_utils.h",
                "src/test/regress/data/enum_and_composite_types.csv",
                "src/test/regress/data/datetime_types.csv",
            ],
            "indexes": [
                "src/backend/distributed/planner",
                "src/backend/distributed/commands",
            ],
            "parser_ast": [
                "src/backend/distributed",
            ],
            "wire_protocol": [
                "src/backend/distributed",
            ],
            "authentication": [
                "src/backend/distributed",
            ],
            "client_bridge": [
                "src/backend/distributed",
            ],
            "plan_output": [
                "src/backend/distributed/planner",
            ],
            "error_codes": [
                "src/include/distributed/error_codes.h",
            ],
            "page_optimizations": [
                "src/backend/distributed",
            ],
            "catalogs_bootstrap": [
                "src/include/distributed",
                "src/backend/distributed",
            ],
            "regression_tests": [
                "src/test/regress",
                "src/test/cdc",
                "src/test/tap",
            ],
        },
        "index_rows": [],
    },
    {
        "id": "apache_ignite",
        "display": "Apache Ignite",
        "type_key": "IGNITE",
        "scratchbird_profile": "apache_ignite",
        "clone_base": "remote",
        "clone": "apache_ignite",
        "token": "apache_ignite",
        "packet_scope_note": "Apache Ignite is represented as a distributed SQL/data-grid platform with Calcite planning, thin-client/ODBC protocol handlers, binary marshalling, H2-backed index types, and system-view metadata surfaces.",
        "donor_sources": {
            "datatypes": [
                "modules/core/src/main/java/org/apache/ignite/internal/processors/query",
                "modules/indexing/src/test/java/org/apache/ignite/sqltests/SqlDataTypesCoverageTests.java",
            ],
            "indexes": [
                "modules/indexing/src/main/java/org/apache/ignite/internal/processors/query/h2/database/H2IndexType.java",
                "modules/indexing",
            ],
            "parser_ast": [
                "modules/calcite/src/main/java/org/apache/ignite/internal/processors/query/calcite",
            ],
            "wire_protocol": [
                "modules/core/src/main/java/org/apache/ignite/internal/processors/odbc/ClientListenerNioListener.java",
                "modules/clients",
            ],
            "authentication": [
                "modules/core/src/main/java/org/apache/ignite/internal/processors/security",
                "modules/core/src/main/java/org/apache/ignite/IgniteAuthenticationException.java",
            ],
            "client_bridge": [
                "modules/clients",
                "modules/core/src/main/java/org/apache/ignite/internal/processors/odbc",
            ],
            "plan_output": [
                "modules/calcite/src/main/java/org/apache/ignite/internal/processors/query/calcite",
                "modules/indexing",
            ],
            "error_codes": [
                "modules/core/src/main/java/org/apache/ignite/IgniteAuthenticationException.java",
                "modules/indexing/src/main/java/org/apache/ignite/internal/processors/query",
            ],
            "page_optimizations": [
                "modules/core/src/main/java/org/apache/ignite/internal/processors/cache/persistence",
                "modules/indexing",
            ],
            "catalogs_bootstrap": [
                "modules/indexing/src/main/java/org/apache/ignite/internal/processors/query/h2/sys/view",
                "modules/core/src/main/java/org/apache/ignite/spi/systemview",
                "modules/indexing/src/test/java/org/apache/ignite/internal/processors/query/SqlSystemViewsSelfTest.java",
            ],
            "regression_tests": [
                "modules/indexing/src/test",
                "modules/compatibility",
                "modules/core/src/test",
            ],
        },
        "index_rows": [],
    },
]


SB_SHARED_FILES = {
    "datatypes": [
        "include/scratchbird/core/types.h",
        "src/core/type_system.cpp",
        "src/core/domain_manager.cpp",
        "tests/unit/domains",
    ],
    "indexes": [
        "include/scratchbird/core/catalog_manager.h",
        "src/core/index_factory.cpp",
        "src/core/index_params.cpp",
        "src/optimizer/index_family_lowering.cpp",
    ],
    "parser_ast": [
        "src/parser/parser_v3.cpp",
        "src/parser/ast_v3.cpp",
        "src/parser/lexer_v3.cpp",
        "src/parser/v3_emitter.cpp",
        "src/sblr/v3_opcodes.generated.cpp",
    ],
    "wire_protocol": [
        "include/scratchbird/protocol/wire_protocol.h",
        "src/protocol/wire_protocol.cpp",
    ],
    "authentication": [
        "src/security/auth_method.cpp",
        "src/security/auth_manager.cpp",
        "src/security/scram_auth.cpp",
        "tests/unit/test_auth_policy_protocol_parity.cpp",
    ],
    "client_bridge": [
        "src/fdw",
        "src/udr/type_mapping.cpp",
    ],
    "plan_output": [
        "src/optimizer/plan_payload.cpp",
        "src/parser/v3_emitter.cpp",
        "src/sblr/native_sql_renderer.cpp",
    ],
    "error_codes": [
        "include/scratchbird/ipc/parser_agent.h",
        "include/scratchbird/ipc/postgresql_parser_agent.h",
        "include/scratchbird/ipc/mysql_parser_agent.h",
        "include/scratchbird/ipc/firebird_parser_agent.h",
    ],
    "page_optimizations": [
        "src/core/columnstore.cpp",
        "src/core/btree.cpp",
        "src/core/hash_index.cpp",
        "src/core/gist_index.cpp",
        "src/core/gin_index.cpp",
        "src/core/rtree.cpp",
    ],
    "catalogs_bootstrap": [
        "src/catalog/virtual_catalog.cpp",
        "src/catalog",
        "tests/unit/test_catalog_database_bootstrap.cpp",
    ],
    "regression_tests": [
        "tests/compatibility",
        "tests/compatibility/scripts",
        "tests/results/full_run_metrics/20260401T032214Z",
    ],
}


def parse_type_rows() -> list[dict[str, str]]:
    pattern = re.compile(
        r'\{"([^"]+)",\s*"([^"]+)",\s*EmulatedStorageKind::([A-Z_]+),\s*DataType::([A-Z0-9_]+),\s*"([^"]*)",\s*"([^"]*)"\}'
    )
    rows = []
    text = TYPE_SYSTEM.read_text(encoding="utf-8")
    for match in pattern.finditer(text):
        rows.append(
            {
                "engine_name": match.group(1),
                "emulated_type": match.group(2),
                "storage_kind": match.group(3),
                "canonical_type": match.group(4),
                "domain_hint": match.group(5),
                "parser_rule_hint": match.group(6),
            }
        )
    return rows


def parse_index_registry() -> list[dict[str, str]]:
    pattern = re.compile(
        r'\{IndexType::([A-Z0-9_]+),\s*"([^"]+)",\s*IndexStorageModel::([A-Z_]+),\s*IndexRuntimeClass::([A-Z_]+),\s*'
        r'(true|false),\s*(true|false),\s*(true|false),\s*(true|false),\s*(true|false),\s*(true|false),\s*(true|false),\s*(true|false)\}'
    )
    rows = []
    text = INDEX_FACTORY.read_text(encoding="utf-8")
    for match in pattern.finditer(text):
        rows.append(
            {
                "index_type": match.group(1),
                "canonical_name": match.group(2),
                "storage_model": match.group(3),
                "runtime_class": match.group(4),
                "supports_build": match.group(5),
                "supports_insert": match.group(6),
                "supports_remove": match.group(7),
                "file_based": match.group(8),
                "supports_search": match.group(9),
                "supports_vector": match.group(10),
                "supports_summary": match.group(11),
                "supports_ordering": match.group(12),
            }
        )
    return rows


def parse_virtual_catalog_registry() -> list[dict[str, str]]:
    rows = []
    pattern = re.compile(
        r"CatalogManager::EmulationEngine::([A-Z_]+)\)\s*\{\s*// Register ([^\n]+)\s*router\.registerHandler\(ProtocolType::([A-Z_]+),\s*std::make_unique<([A-Za-z0-9_]+)>",
        re.S,
    )
    text = VIRTUAL_CATALOG.read_text(encoding="utf-8")
    for match in pattern.finditer(text):
        rows.append(
            {
                "emulation_engine": match.group(1),
                "comment": " ".join(match.group(2).split()),
                "protocol_type": match.group(3),
                "handler": match.group(4),
            }
        )
    return rows


def parse_bootstrap_paths() -> list[str]:
    text = BOOTSTRAP_TEST.read_text(encoding="utf-8")
    pattern = re.compile(r'"([^"]+)"')
    anchor = text.split("kCanonicalPaths[] = {", 1)[1].split("};", 1)[0]
    return pattern.findall(anchor)


def normalize_token(value: str) -> str:
    return re.sub(r"[^A-Z0-9]+", "_", value.upper()).strip("_")


def normalize_compact(value: str) -> str:
    return re.sub(r"[^A-Z0-9]+", "", value.upper())


def parse_native_data_types() -> list[str]:
    text = CORE_TYPES.read_text(encoding="utf-8")
    if "enum class DataType : uint16_t" not in text:
        return []
    block = text.split("enum class DataType : uint16_t", 1)[1].split("};", 1)[0]
    rows: list[str] = []
    seen: set[str] = set()
    for match in re.finditer(r"^\s*([A-Z][A-Z0-9_]+)\s*=", block, re.M):
        name = match.group(1)
        if name in seen:
            continue
        seen.add(name)
        rows.append(name)
    return rows


def inventory_row(
    engine: dict,
    surface_class: str,
    surface_kind: str,
    donor_surface: str,
    evidence_path: Path,
    evidence_note: str,
) -> dict[str, str]:
    return {
        "engine": engine["display"],
        "engine_id": engine["id"],
        "surface_class": surface_class,
        "surface_kind": surface_kind,
        "donor_surface": donor_surface,
        "donor_surface_key": normalize_token(donor_surface),
        "evidence_path": rel(evidence_path),
        "evidence_note": evidence_note,
    }


def add_inventory_row(
    rows: list[dict[str, str]],
    seen: set[tuple[str, str, str, str]],
    engine: dict,
    surface_class: str,
    surface_kind: str,
    donor_surface: str,
    evidence_path: Path,
    evidence_note: str,
) -> None:
    key = (engine["id"], surface_class, surface_kind, normalize_token(donor_surface))
    if key in seen:
        return
    seen.add(key)
    rows.append(
        inventory_row(
            engine=engine,
            surface_class=surface_class,
            surface_kind=surface_kind,
            donor_surface=donor_surface,
            evidence_path=evidence_path,
            evidence_note=evidence_note,
        )
    )


def extract_enum_members(text: str, start_marker: str, skip: set[str] | None = None) -> list[str]:
    if start_marker not in text:
        return []
    block = text.split(start_marker, 1)[1].split("};", 1)[0]
    rows: list[str] = []
    seen: set[str] = set()
    for match in re.finditer(r"^\s*([A-Za-z][A-Za-z0-9_]*)\s*(?:=\s*[^,]+)?\s*,", block, re.M):
        name = match.group(1)
        if skip and name in skip:
            continue
        if name in seen:
            continue
        seen.add(name)
        rows.append(name)
    return rows


def add_pattern_surfaces(
    rows: list[dict[str, str]],
    seen: set[tuple[str, str, str, str]],
    engine: dict,
    surface_class: str,
    surface_kind: str,
    evidence_path: Path,
    text: str,
    patterns: list[tuple[str, str]],
    note: str,
) -> None:
    for pattern, donor_surface in patterns:
        if re.search(pattern, text, re.M):
            add_inventory_row(
                rows,
                seen,
                engine,
                surface_class,
                surface_kind,
                donor_surface,
                evidence_path,
                note,
            )


def split_top_level_commas(text: str) -> list[str]:
    parts: list[str] = []
    current: list[str] = []
    depth = 0
    quote: str | None = None
    escape = False
    for ch in text:
        if quote:
            current.append(ch)
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == quote:
                quote = None
            continue
        if ch in {"'", '"', "`"}:
            quote = ch
            current.append(ch)
            continue
        if ch in {"(", "[", "{"}:
            depth += 1
        elif ch in {")", "]", "}"} and depth:
            depth -= 1
        if ch == "," and depth == 0:
            part = "".join(current).strip()
            if part:
                parts.append(part)
            current = []
            continue
        current.append(ch)
    tail = "".join(current).strip()
    if tail:
        parts.append(tail)
    return parts


def parse_sql_columns(body: str) -> list[tuple[str, str]]:
    rows: list[tuple[str, str]] = []
    for chunk in split_top_level_commas(body):
        compact = " ".join(chunk.split())
        upper = compact.upper()
        if not compact or upper.startswith(
            (
                "PRIMARY KEY",
                "UNIQUE",
                "KEY ",
                "INDEX ",
                "FULLTEXT",
                "SPATIAL",
                "CONSTRAINT",
                "CHECK",
                "FOREIGN KEY",
            )
        ):
            continue
        match = re.match(r'[`"]?([A-Za-z0-9_$]+)[`"]?\s+(.+)', compact)
        if not match:
            continue
        rows.append((match.group(1), match.group(2)))
    return rows


def count_pg_dat_rows(path: Path) -> int:
    text = path.read_text(encoding="utf-8")
    return len(re.findall(r"^\s*\{", text, re.M))


def parse_string_constant_map(path: Path, language: str) -> dict[str, str]:
    text = path.read_text(encoding="utf-8")
    if language == "java":
        pattern = re.compile(r'(?:public|private)\s+static\s+final\s+String\s+([A-Za-z0-9_]+)\s*=\s*"([^"]+)";')
    else:
        pattern = re.compile(r'pub\s+const\s+([A-Z0-9_]+):\s*&str\s*=\s*"([^"]+)";')
    return {match.group(1): match.group(2) for match in pattern.finditer(text)}


def extract_postgresql_datatype_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    path = clone_root / "src/include/catalog/pg_type.dat"
    if not path.exists():
        return []
    text = path.read_text(encoding="utf-8")
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    for match in re.finditer(r"typname => '([^']+)'", text):
        add_inventory_row(rows, seen, engine, "datatype", "catalog_typname", match.group(1), path, "pg_type catalog typname")
    return rows


def extract_postgresql_index_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    path = clone_root / "src/include/catalog/pg_am.dat"
    if not path.exists():
        return []
    text = path.read_text(encoding="utf-8")
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    for match in re.finditer(r"\{[^}]*amname => '([^']+)'[^}]*amtype => 'i'[^}]*\}", text, re.S):
        add_inventory_row(rows, seen, engine, "index", "index_access_method", match.group(1), path, "pg_am index access method")
    return rows


def extract_mysql_like_datatype_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()

    header = clone_root / ("include/field_types.h" if engine["id"] == "mysql" else "include/mysql_com.h")
    if header.exists():
        text = header.read_text(encoding="utf-8")
        for name in extract_enum_members(text, "enum enum_field_types"):
            if name.startswith("MYSQL_TYPE_"):
                add_inventory_row(rows, seen, engine, "datatype", "protocol_field_type", name, header, "field type enum")

    grammar = clone_root / "sql/sql_yacc.yy"
    if grammar.exists():
        text = grammar.read_text(encoding="utf-8")
        add_pattern_surfaces(
            rows,
            seen,
            engine,
            "datatype",
            "sql_type_token",
            grammar,
            text,
            [
                (r"\bBOOLEAN_SYM\b", "BOOLEAN"),
                (r"\bCHAR_SYM\b", "CHAR"),
                (r"\bNCHAR_SYM\b", "NCHAR"),
                (r"\bVARCHAR_SYM\b", "VARCHAR"),
                (r"\bNVARCHAR_SYM\b", "NVARCHAR"),
                (r"\bBINARY_SYM\b", "BINARY"),
                (r"\bVARBINARY_SYM\b", "VARBINARY"),
                (r"\bVECTOR_SYM\b", "VECTOR"),
                (r"\bYEAR_SYM\b", "YEAR"),
                (r"\bDATE_SYM\b", "DATE"),
                (r"\bTIME_SYM\b", "TIME"),
                (r"\bTIMESTAMP_SYM\b", "TIMESTAMP"),
                (r"\bDATETIME_SYM\b", "DATETIME"),
                (r"\bTINYBLOB_SYM\b", "TINYBLOB"),
                (r"\bBLOB_SYM\b", "BLOB"),
                (r"\bMEDIUMBLOB_SYM\b", "MEDIUMBLOB"),
                (r"\bLONGBLOB_SYM\b", "LONGBLOB"),
                (r"\bTINYTEXT(?:_SYN|_SYM)?\b", "TINYTEXT"),
                (r"\bTEXT_SYM\b", "TEXT"),
                (r"\bMEDIUMTEXT_SYM\b", "MEDIUMTEXT"),
                (r"\bLONGTEXT_SYM\b", "LONGTEXT"),
                (r"\bENUM(?:_SYM)?\b", "ENUM"),
                (r"\bSET(?:_SYM)?\b", "SET"),
                (r"\bSERIAL_SYM\b", "SERIAL"),
                (r"\bJSON_SYM\b", "JSON"),
                (r"\bGEOMETRY_SYM\b", "GEOMETRY"),
                (r"\bGEOMETRYCOLLECTION_SYM\b", "GEOMETRYCOLLECTION"),
                (r"\bPOINT_SYM\b", "POINT"),
                (r"\bMULTIPOINT_SYM\b", "MULTIPOINT"),
                (r"\bLINESTRING_SYM\b", "LINESTRING"),
                (r"\bMULTILINESTRING_SYM\b", "MULTILINESTRING"),
                (r"\bPOLYGON_SYM\b", "POLYGON"),
                (r"\bMULTIPOLYGON_SYM\b", "MULTIPOLYGON"),
                (r"\bINT_SYM\b", "INT"),
                (r"\bTINYINT_SYM\b", "TINYINT"),
                (r"\bSMALLINT_SYM\b", "SMALLINT"),
                (r"\bMEDIUMINT_SYM\b", "MEDIUMINT"),
                (r"\bBIGINT_SYM\b", "BIGINT"),
            ],
            "sql grammar datatype token",
        )
    return rows


def extract_mysql_like_index_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    path = clone_root / "sql/sql_yacc.yy"
    if not path.exists():
        return []
    text = path.read_text(encoding="utf-8")
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    add_pattern_surfaces(
        rows,
        seen,
        engine,
        "index",
        "index_algorithm_token",
        path,
        text,
        [
            (r"\bBTREE_SYM\b", "btree"),
            (r"\bHASH_SYM\b", "hash"),
            (r"\bFULLTEXT_SYM\b", "fulltext"),
            (r"\bSPATIAL_SYM\b", "spatial"),
        ],
        "sql grammar index token",
    )
    if engine["id"] == "mariadb":
        add_pattern_surfaces(
            rows,
            seen,
            engine,
            "index",
            "index_algorithm_token",
            path,
            text,
            [(r"Key::VECTOR|\bVECTOR_SYM\b", "vector")],
            "sql grammar vector index token",
        )
    return rows


def extract_firebird_datatype_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    path = clone_root / "src/dsql/parse.y"
    if not path.exists():
        return []
    text = path.read_text(encoding="utf-8")
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    add_pattern_surfaces(
        rows,
        seen,
        engine,
        "datatype",
        "sql_type_token",
        path,
        text,
        [
            (r"\bSMALLINT\b", "SMALLINT"),
            (r"\bINTEGER\b", "INTEGER"),
            (r"\bINT128\b", "INT128"),
            (r"\bBIGINT\b", "BIGINT"),
            (r"\bBOOLEAN\b", "BOOLEAN"),
            (r"\bDECFLOAT\b", "DECFLOAT"),
            (r"\bNUMERIC\b", "NUMERIC"),
            (r"\bDECIMAL\b", "DECIMAL"),
            (r"\bFLOAT\b", "FLOAT"),
            (r"\bDOUBLE\b", "DOUBLE PRECISION"),
            (r"\bCHAR\b", "CHAR"),
            (r"\bVARCHAR\b", "VARCHAR"),
            (r"\bVARBINARY\b", "VARBINARY"),
            (r"\bBLOB\b", "BLOB"),
            (r"\bDATE\b", "DATE"),
            (r"\bTIME WITH TIME ZONE\b", "TIME WITH TIME ZONE"),
            (r"\bTIME\b", "TIME"),
            (r"\bTIMESTAMP WITH TIME ZONE\b", "TIMESTAMP WITH TIME ZONE"),
            (r"\bTIMESTAMP\b", "TIMESTAMP"),
        ],
        "dsql grammar datatype token",
    )
    return rows


def extract_firebird_index_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    idx_path = clone_root / "src/jrd/idx.cpp"
    if idx_path.exists():
        add_inventory_row(rows, seen, engine, "index", "physical_index_family", "ascending_or_descending_btree", idx_path, "core Firebird index implementation")
    parse_path = clone_root / "src/dsql/parse.y"
    if parse_path.exists():
        text = parse_path.read_text(encoding="utf-8")
        if "COMPUTED BY" in text:
            add_inventory_row(rows, seen, engine, "index", "expression_index", "expression_index", parse_path, "computed-by expression index grammar")
    return rows


def extract_cassandra_datatype_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    type_path = clone_root / "src/java/org/apache/cassandra/cql3/CQL3Type.java"
    if type_path.exists():
        text = type_path.read_text(encoding="utf-8")
        for name in extract_enum_members(text, "enum Native implements CQL3Type"):
            add_inventory_row(rows, seen, engine, "datatype", "native_cql_type", name, type_path, "CQL3Type.Native enum")
        if "ListType.getInstance" in text:
            add_inventory_row(rows, seen, engine, "datatype", "collection_type", "LIST", type_path, "CQL3 collection type")
        if "SetType.getInstance" in text:
            add_inventory_row(rows, seen, engine, "datatype", "collection_type", "SET", type_path, "CQL3 collection type")
        if "MapType.getInstance" in text:
            add_inventory_row(rows, seen, engine, "datatype", "collection_type", "MAP", type_path, "CQL3 collection type")
        if "frozen<" in text:
            add_inventory_row(rows, seen, engine, "datatype", "collection_modifier", "FROZEN", type_path, "CQL3 frozen modifier")
        if "RawUT" in text or "UserType" in text:
            add_inventory_row(rows, seen, engine, "datatype", "user_defined_type", "UDT", type_path, "CQL3 user-defined type")
        if "class Custom" in text:
            add_inventory_row(rows, seen, engine, "datatype", "custom_type", "CUSTOM", type_path, "CQL3 custom type wrapper")
    parser_path = clone_root / "src/antlr/Parser.g"
    if parser_path.exists():
        text = parser_path.read_text(encoding="utf-8")
        if "tuple_type returns" in text:
            add_inventory_row(rows, seen, engine, "datatype", "tuple_type", "TUPLE", parser_path, "CQL tuple grammar")
        if "K_VECTOR" in text or "vector<" in text:
            add_inventory_row(rows, seen, engine, "datatype", "vector_type", "VECTOR", parser_path, "CQL vector grammar")
    return rows


def extract_cassandra_index_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    path = clone_root / "src/java/org/apache/cassandra/schema/IndexMetadata.java"
    if path.exists():
        text = path.read_text(encoding="utf-8")
        add_inventory_row(rows, seen, engine, "index", "index_surface", "secondary_index", path, "IndexMetadata secondary index surface")
        if "StorageAttachedIndex" in text:
            add_inventory_row(rows, seen, engine, "index", "index_surface", "sai", path, "storage attached index alias")
        if "SASIIndex" in text:
            add_inventory_row(rows, seen, engine, "index", "index_surface", "sasi", path, "SASI index alias")
    return rows


def extract_clickhouse_datatype_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    base = clone_root / "src/DataTypes"
    if not base.exists():
        return []
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    pattern = re.compile(r'register(?:Simple)?DataType\("([^"]+)"')
    for path in sorted(base.rglob("*.cpp")):
        text = path.read_text(encoding="utf-8")
        for match in pattern.finditer(text):
            add_inventory_row(rows, seen, engine, "datatype", "datatype_factory_registration", match.group(1), path, "DataTypeFactory registration")
    return rows


def extract_clickhouse_index_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    path = clone_root / "src/Storages/MergeTree/MergeTreeIndices.cpp"
    if not path.exists():
        return []
    text = path.read_text(encoding="utf-8")
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    for match in re.finditer(r'registerCreator\("([^"]+)"', text):
        add_inventory_row(rows, seen, engine, "index", "mergetree_index_type", match.group(1), path, "MergeTree index registration")
    return rows


def extract_duckdb_datatype_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    path = clone_root / "src/include/duckdb/common/types.hpp"
    if not path.exists():
        return []
    text = path.read_text(encoding="utf-8")
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    for name in extract_enum_members(text, "enum class LogicalTypeId : uint8_t"):
        add_inventory_row(rows, seen, engine, "datatype", "logical_type_id", name, path, "LogicalTypeId enum")
    return rows


def extract_duckdb_index_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    art_dir = clone_root / "src/execution/index/art"
    if art_dir.exists():
        add_inventory_row(rows, seen, engine, "index", "index_implementation", "art", art_dir, "ART implementation directory")
    zonemap_probe = clone_root / "src/storage/table_index_list.cpp"
    if zonemap_probe.exists():
        text = zonemap_probe.read_text(encoding="utf-8").lower()
        if "zonemap" in text or "zone map" in text:
            add_inventory_row(rows, seen, engine, "index", "index_implementation", "zonemap", zonemap_probe, "zone map references in storage layer")
    return rows


def extract_influxdb_datatype_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    path = clone_root / "influxdb3_system_tables/src/influxdb_schema.rs"
    if not path.exists():
        return []
    text = path.read_text(encoding="utf-8")
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    for match in re.finditer(r'=> "([^"]+)"', text):
        add_inventory_row(rows, seen, engine, "datatype", "influx_column_type", match.group(1), path, "Influx column type renderer")
    return rows


def extract_mongodb_datatype_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    path = clone_root / "src/mongo/bson/bsontypes.h"
    if not path.exists():
        return []
    text = path.read_text(encoding="utf-8")
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    for name in extract_enum_members(text, "enum class BSONType : int", {"jsTypeMax"}):
        add_inventory_row(rows, seen, engine, "datatype", "bson_type", name, path, "BSONType enum")
    for name in extract_enum_members(text, "enum BinDataType", {"bdtCustom"}):
        add_inventory_row(rows, seen, engine, "datatype", "bson_binary_subtype", f"BINDATA_{name}", path, "BinDataType enum")
    return rows


def extract_mongodb_index_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    path = clone_root / "src/mongo/db/index_names.h"
    if not path.exists():
        return []
    text = path.read_text(encoding="utf-8")
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    for name in extract_enum_members(text, "enum MONGO_MOD_PUBLIC IndexType", {"INDEX_TYPE_COUNT"}):
        donor_surface = name.removeprefix("INDEX_")
        add_inventory_row(rows, seen, engine, "index", "index_type_enum", donor_surface, path, "MongoDB IndexType enum")
    return rows


def extract_neo4j_datatype_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    property_path = clone_root / "community/graphdb-api/src/main/java/org/neo4j/graphdb/schema/PropertyType.java"
    if property_path.exists():
        text = property_path.read_text(encoding="utf-8")
        for name in extract_enum_members(text, "public enum PropertyType"):
            add_inventory_row(rows, seen, engine, "datatype", "property_type", name, property_path, "PropertyType enum")
    codec_path = clone_root / "community/values/src/main/java/org/neo4j/values/storable/ValueByteBufferCodec.java"
    if codec_path.exists():
        text = codec_path.read_text(encoding="utf-8")
        for name in extract_enum_members(text, "public enum ValueType"):
            add_inventory_row(rows, seen, engine, "datatype", "value_codec_type", name, codec_path, "ValueByteBufferCodec.ValueType enum")
    return rows


def extract_neo4j_index_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    path = clone_root / "community/graphdb-api/src/main/java/org/neo4j/graphdb/schema/IndexType.java"
    if not path.exists():
        return []
    text = path.read_text(encoding="utf-8")
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    for name in extract_enum_members(text, "public enum IndexType"):
        add_inventory_row(rows, seen, engine, "index", "index_type_enum", name, path, "Neo4j IndexType enum")
    return rows


def extract_opensearch_datatype_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    search_roots = [
        clone_root / "server/src/main/java/org/opensearch/index/mapper",
        clone_root / "modules/mapper-extras/src/main/java/org/opensearch/index/mapper",
        clone_root / "plugins",
    ]
    pattern = re.compile(r"[A-Z_]*CONTENT_TYPE\s*=\s*\"([^\"]+)\"")
    for base in search_roots:
        if not base.exists():
            continue
        for path in sorted(base.rglob("*.java")):
            text = path.read_text(encoding="utf-8")
            for match in pattern.finditer(text):
                value = match.group(1)
                if value.startswith("_"):
                    continue
                add_inventory_row(rows, seen, engine, "datatype", "field_mapper_type", value, path, "field mapper content type")
    return rows


def extract_redis_datatype_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    path = clone_root / "src/server.h"
    if not path.exists():
        return []
    text = path.read_text(encoding="utf-8")
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    for match in re.finditer(r"#define\s+(OBJ_[A-Z0-9_]+)\s+\d+", text):
        add_inventory_row(rows, seen, engine, "datatype", "redis_object_type", match.group(1).removeprefix("OBJ_"), path, "Redis object type define")
    return rows


def extract_milvus_datatype_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    type_path = clone_root / "internal/core/src/common/Types.h"
    if type_path.exists():
        text = type_path.read_text(encoding="utf-8")
        for name in extract_enum_members(text, "enum class DataType"):
            add_inventory_row(rows, seen, engine, "datatype", "internal_data_type", name, type_path, "internal DataType enum")
    for rel_path in [
        "client/entity/schema.go",
        "client/column/columns.go",
        "internal/parser/planparserv2/plan_parser_v2.go",
    ]:
        path = clone_root / rel_path
        if not path.exists():
            continue
        text = path.read_text(encoding="utf-8")
        for match in re.finditer(r"DataType_([A-Za-z0-9]+)", text):
            add_inventory_row(rows, seen, engine, "datatype", "proto_data_type", match.group(1), path, "schemapb DataType token")
    return rows


def extract_milvus_index_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    for rel_path, kind in [
        ("client/index/common.go", "vector_index_type"),
        ("internal/util/indexparamcheck/index_type.go", "scalar_index_type"),
    ]:
        path = clone_root / rel_path
        if not path.exists():
            continue
        text = path.read_text(encoding="utf-8")
        for match in re.finditer(r'IndexType\s*=\s*"([^"]+)"', text):
            add_inventory_row(rows, seen, engine, "index", kind, match.group(1), path, "Milvus index type constant")
    return rows


def extract_tidb_datatype_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()

    type_path = clone_root / "pkg/parser/mysql/type.go"
    if type_path.exists():
        text = type_path.read_text(encoding="utf-8")
        type_name_map = {
            "TypeTiny": "TINYINT",
            "TypeShort": "SMALLINT",
            "TypeLong": "INT",
            "TypeFloat": "FLOAT",
            "TypeDouble": "DOUBLE",
            "TypeNull": "NULL",
            "TypeTimestamp": "TIMESTAMP",
            "TypeLonglong": "BIGINT",
            "TypeInt24": "MEDIUMINT",
            "TypeDate": "DATE",
            "TypeDuration": "TIME",
            "TypeDatetime": "DATETIME",
            "TypeYear": "YEAR",
            "TypeVarchar": "VARCHAR",
            "TypeBit": "BIT",
            "TypeJSON": "JSON",
            "TypeNewDecimal": "DECIMAL",
            "TypeEnum": "ENUM",
            "TypeSet": "SET",
            "TypeTinyBlob": "TINYBLOB",
            "TypeMediumBlob": "MEDIUMBLOB",
            "TypeLongBlob": "LONGBLOB",
            "TypeBlob": "BLOB",
            "TypeVarString": "VAR_STRING",
            "TypeString": "STRING",
            "TypeGeometry": "GEOMETRY",
            "TypeTiDBVectorFloat32": "VECTOR_FLOAT32",
        }
        for match in re.finditer(r"^\s*(Type[A-Za-z0-9_]+)\s+byte\s*=", text, re.M):
            donor_surface = type_name_map.get(match.group(1), match.group(1))
            add_inventory_row(rows, seen, engine, "datatype", "protocol_field_type", donor_surface, type_path, "TiDB MySQL protocol type constant")

    grammar = clone_root / "pkg/parser/parser.y"
    if grammar.exists():
        text = grammar.read_text(encoding="utf-8")
        add_pattern_surfaces(
            rows,
            seen,
            engine,
            "datatype",
            "sql_type_token",
            grammar,
            text,
            [
                (r"\bBOOLEAN\b", "BOOLEAN"),
                (r"\bTINYINT\b", "TINYINT"),
                (r"\bSMALLINT\b", "SMALLINT"),
                (r"\bMEDIUMINT\b", "MEDIUMINT"),
                (r"\bINT\b", "INT"),
                (r"\bINTEGER\b", "INTEGER"),
                (r"\bBIGINT\b", "BIGINT"),
                (r"\bDECIMAL\b", "DECIMAL"),
                (r"\bNUMERIC\b", "NUMERIC"),
                (r"\bFLOAT\b", "FLOAT"),
                (r"\bDOUBLE\b", "DOUBLE"),
                (r"\bBIT\b", "BIT"),
                (r"\bDATE\b", "DATE"),
                (r"\bTIME\b", "TIME"),
                (r"\bDATETIME\b", "DATETIME"),
                (r"\bTIMESTAMP\b", "TIMESTAMP"),
                (r"\bYEAR\b", "YEAR"),
                (r"\bCHAR\b", "CHAR"),
                (r"\bVARCHAR\b", "VARCHAR"),
                (r"\bBINARY\b", "BINARY"),
                (r"\bVARBINARY\b", "VARBINARY"),
                (r"\bTEXT\b", "TEXT"),
                (r"\bBLOB\b", "BLOB"),
                (r"\bJSON\b", "JSON"),
                (r"\bGEOMETRY\b", "GEOMETRY"),
                (r"\bPOINT\b", "POINT"),
                (r"\bLINESTRING\b", "LINESTRING"),
                (r"\bPOLYGON\b", "POLYGON"),
                (r"\bVECTOR\b", "VECTOR"),
            ],
            "TiDB parser grammar datatype token",
        )
    return rows


def extract_tidb_index_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    path = clone_root / "pkg/parser/parser.y"
    if not path.exists():
        return rows
    text = path.read_text(encoding="utf-8")
    add_pattern_surfaces(
        rows,
        seen,
        engine,
        "index",
        "index_algorithm_token",
        path,
        text,
        [
            (r"\bBTREE\b", "btree"),
            (r"\bHASH\b", "hash"),
            (r"\bFULLTEXT\b", "fulltext"),
            (r"\bSPATIAL\b", "spatial"),
            (r"\bVECTOR\b", "vector"),
        ],
        "TiDB parser grammar index token",
    )
    return rows


def extract_vitess_datatype_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()

    proto_path = clone_root / "go/vt/proto/query/query.pb.go"
    if proto_path.exists():
        text = proto_path.read_text(encoding="utf-8")
        for match in re.finditer(r"^\s*Type_([A-Z0-9_]+)\s+Type\s*=", text, re.M):
            add_inventory_row(rows, seen, engine, "datatype", "querypb_type", match.group(1), proto_path, "Vitess querypb.Type enum")

    grammar = clone_root / "go/vt/sqlparser/sql.y"
    if grammar.exists():
        text = grammar.read_text(encoding="utf-8")
        add_pattern_surfaces(
            rows,
            seen,
            engine,
            "datatype",
            "sql_type_token",
            grammar,
            text,
            [
                (r"\bBOOLEAN\b", "BOOLEAN"),
                (r"\bTINYINT\b", "TINYINT"),
                (r"\bSMALLINT\b", "SMALLINT"),
                (r"\bMEDIUMINT\b", "MEDIUMINT"),
                (r"\bINT\b", "INT"),
                (r"\bINTEGER\b", "INTEGER"),
                (r"\bBIGINT\b", "BIGINT"),
                (r"\bDECIMAL\b", "DECIMAL"),
                (r"\bNUMERIC\b", "NUMERIC"),
                (r"\bFLOAT\b", "FLOAT"),
                (r"\bDOUBLE\b", "DOUBLE"),
                (r"\bBIT\b", "BIT"),
                (r"\bDATE\b", "DATE"),
                (r"\bTIME\b", "TIME"),
                (r"\bDATETIME\b", "DATETIME"),
                (r"\bTIMESTAMP\b", "TIMESTAMP"),
                (r"\bYEAR\b", "YEAR"),
                (r"\bCHAR\b", "CHAR"),
                (r"\bVARCHAR\b", "VARCHAR"),
                (r"\bBINARY\b", "BINARY"),
                (r"\bVARBINARY\b", "VARBINARY"),
                (r"\bTEXT\b", "TEXT"),
                (r"\bBLOB\b", "BLOB"),
                (r"\bJSON\b", "JSON"),
                (r"\bGEOMETRY\b", "GEOMETRY"),
            ],
            "Vitess parser grammar datatype token",
        )
    return rows


def extract_vitess_index_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()

    grammar = clone_root / "go/vt/sqlparser/sql.y"
    if grammar.exists():
        text = grammar.read_text(encoding="utf-8")
        add_pattern_surfaces(
            rows,
            seen,
            engine,
            "index",
            "index_algorithm_token",
            grammar,
            text,
            [
                (r"\bBTREE\b", "btree"),
                (r"\bHASH\b", "hash"),
                (r"\bFULLTEXT\b", "fulltext"),
                (r"\bSPATIAL\b", "spatial"),
            ],
            "Vitess parser grammar index token",
        )

    vindex_dir = clone_root / "go/vt/vtgate/vindexes"
    if vindex_dir.exists():
        add_inventory_row(rows, seen, engine, "index", "routing_index_surface", "vindex", vindex_dir, "Vitess vindex routing directory")
    return rows


def extract_dolt_index_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    path = clone_root / "go/libraries/doltcore/schema/index.go"
    if not path.exists():
        return rows
    text = path.read_text(encoding="utf-8")
    for pattern, donor_surface in [
        (r"\bIsUnique\b", "unique"),
        (r"\bIsSpatial\b", "spatial"),
        (r"\bIsFullText\b", "fulltext"),
        (r"\bIsVector\b", "vector"),
    ]:
        if re.search(pattern, text):
            add_inventory_row(rows, seen, engine, "index", "index_property", donor_surface, path, "Dolt index interface property")
    return rows


def extract_dolt_datatype_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    path = clone_root / "go/libraries/doltcore/schema/typeinfo/typeinfo.go"
    if not path.exists():
        return rows
    text = path.read_text(encoding="utf-8")
    for match in re.finditer(r"case\s+sqltypes\.([A-Za-z0-9_]+)", text):
        add_inventory_row(rows, seen, engine, "datatype", "sqltypes_case", match.group(1), path, "Dolt typeinfo switch over Vitess sqltypes")
    for donor_surface in ["POINT", "LINESTRING", "POLYGON", "MULTIPOINT", "MULTILINESTRING", "MULTIPOLYGON", "GEOMETRY"]:
        if donor_surface in text:
            add_inventory_row(rows, seen, engine, "datatype", "geometry_type", donor_surface, path, "Dolt geometry subtype handling in typeinfo")
    return rows


def extract_foundationdb_datatype_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    for rel_path, donor_surface, note in [
        ("fdbclient/include/fdbclient/FDBTypes.h", "KEY_BYTES", "FoundationDB key material is byte-addressed"),
        ("fdbclient/include/fdbclient/FDBTypes.h", "VALUE_BYTES", "FoundationDB value material is byte-addressed"),
        ("fdbclient/include/fdbclient/Tuple.h", "TUPLE", "Tuple layer surface"),
        ("fdbclient/TupleVersionstamp.cpp", "VERSIONSTAMP", "Tuple versionstamp support"),
    ]:
        path = clone_root / rel_path
        if path.exists():
            add_inventory_row(rows, seen, engine, "datatype", "kv_or_tuple_surface", donor_surface, path, note)
    return rows


def extract_immudb_datatype_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    stmt_path = clone_root / "embedded/sql/stmt.go"
    if stmt_path.exists():
        text = stmt_path.read_text(encoding="utf-8")
        for match in re.finditer(r'^\s*([A-Za-z0-9_]+)\s+SQLValueType\s*=\s*"([^"]+)"', text, re.M):
            add_inventory_row(rows, seen, engine, "datatype", "sql_value_type", match.group(2), stmt_path, "immudb SQLValueType constant")
    grammar = clone_root / "embedded/sql/sql_grammar.y"
    if grammar.exists():
        text = grammar.read_text(encoding="utf-8")
        add_pattern_surfaces(
            rows,
            seen,
            engine,
            "datatype",
            "sql_type_token",
            grammar,
            text,
            [
                (r"\bINTEGER\b", "INTEGER"),
                (r"\bBOOLEAN\b", "BOOLEAN"),
                (r"\bVARCHAR\b", "VARCHAR"),
                (r"\bUUID\b", "UUID"),
                (r"\bBLOB\b", "BLOB"),
                (r"\bFLOAT\b", "FLOAT"),
                (r"\bTIMESTAMP\b", "TIMESTAMP"),
                (r"\bJSON\b", "JSON"),
            ],
            "immudb SQL grammar datatype token",
        )
    return rows


def extract_immudb_index_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    path = clone_root / "embedded/sql/sql_grammar.y"
    if not path.exists():
        return rows
    text = path.read_text(encoding="utf-8")
    add_pattern_surfaces(
        rows,
        seen,
        engine,
        "index",
        "index_surface",
        path,
        text,
        [
            (r"CREATE INDEX", "secondary_index"),
            (r"CREATE UNIQUE INDEX", "unique_index"),
            (r"USE INDEX", "use_index_hint"),
        ],
        "immudb SQL grammar index surface",
    )
    return rows


def extract_xtdb_datatype_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    path = clone_root / "core/src/main/clojure/xtdb/types.clj"
    if not path.exists():
        return rows
    text = path.read_text(encoding="utf-8")
    for match in re.finditer(r"\(derive\s+:([a-z0-9\-_?]+)\s+:", text):
        add_inventory_row(
            rows,
            seen,
            engine,
            "datatype",
            "col_type_hierarchy",
            match.group(1).upper().replace("-", "_").replace("?", "NULLABLE"),
            path,
            "XTDB column type hierarchy derivation",
        )
    for match in re.finditer(r"\(defmethod\s+arrow-type->col-type[^\n]*\n(?:.+\n){0,3}?\s+(\[[^\]]+\]|:[a-z0-9\-_]+)", text):
        token = match.group(1).strip()
        token = token.strip("[]").split()[0].lstrip(":").upper().replace("-", "_")
        if token:
            add_inventory_row(rows, seen, engine, "datatype", "arrow_col_type_method", token, path, "XTDB Arrow-to-column-type multimethod return token")
    return rows


def extract_xtdb_index_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    info_path = clone_root / "core/src/main/clojure/xtdb/information_schema.clj"
    if info_path.exists():
        text = info_path.read_text(encoding="utf-8")
        for match in re.finditer(r':amname\s+"([^"]+)"', text):
            add_inventory_row(rows, seen, engine, "index", "pg_access_method", match.group(1), info_path, "XTDB pg_am emulation entry")
    trie_path = clone_root / "core/src/main/clojure/xtdb/trie_catalog.clj"
    if trie_path.exists():
        add_inventory_row(rows, seen, engine, "index", "physical_trie_surface", "trie", trie_path, "XTDB trie catalog physical surface")
    return rows


def extract_apache_ignite_datatype_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    path = clone_root / "modules/indexing/src/test/java/org/apache/ignite/sqltests/SqlDataTypesCoverageTests.java"
    if not path.exists():
        return rows
    text = path.read_text(encoding="utf-8")
    for match in re.finditer(r"SqlDataType\.([A-Z0-9_]+)", text):
        add_inventory_row(rows, seen, engine, "datatype", "sql_datatype_test_surface", match.group(1), path, "Ignite SQL datatype coverage test")
    return rows


def extract_cockroachdb_datatype_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    path = clone_root / "pkg/sql/types/types.go"
    if not path.exists():
        return rows
    text = path.read_text(encoding="utf-8")
    for line in text.splitlines():
        if not line.startswith("// |"):
            continue
        cells = [cell.strip() for cell in line.removeprefix("// ").split("|")]
        if len(cells) < 2:
            continue
        head = cells[1]
        if not head or head.startswith("SQL type") or set(head) <= {"-"}:
            continue
        for token in [piece.strip() for piece in head.split(",")]:
            normalized = token.strip('"').strip()
            if normalized:
                add_inventory_row(rows, seen, engine, "datatype", "documented_sql_type", normalized, path, "CockroachDB types.go documented SQL type row")
    return rows


def extract_cockroachdb_index_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    proto_path = clone_root / "pkg/sql/sem/idxtype/idxtype.proto"
    if proto_path.exists():
        text = proto_path.read_text(encoding="utf-8")
        for match in re.finditer(r"^\s*([A-Z]+)\s*=\s*\d+;", text, re.M):
            add_inventory_row(rows, seen, engine, "index", "index_type_enum", match.group(1), proto_path, "CockroachDB idxtype enum")
    optbuilder_path = clone_root / "pkg/sql/opt/optbuilder/mutation_builder.go"
    if optbuilder_path.exists() and "hash-shard" in optbuilder_path.read_text(encoding="utf-8").lower():
        add_inventory_row(rows, seen, engine, "index", "index_modifier", "HASH_SHARDED", optbuilder_path, "CockroachDB hash-sharded index support checks")
    return rows


def extract_citus_datatype_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    type_path = clone_root / "src/backend/distributed/commands/type.c"
    if type_path.exists():
        text = type_path.read_text(encoding="utf-8")
        if "CREATE TYPE" in text:
            add_inventory_row(rows, seen, engine, "datatype", "distributed_type_surface", "CREATE_TYPE", type_path, "Citus distributed CREATE TYPE propagation")
        if "AS ENUM" in text:
            add_inventory_row(rows, seen, engine, "datatype", "distributed_type_surface", "ENUM", type_path, "Citus distributed enum propagation")
        if "composite type" in text.lower():
            add_inventory_row(rows, seen, engine, "datatype", "distributed_type_surface", "COMPOSITE", type_path, "Citus composite type recreation")
    header_path = clone_root / "src/include/distributed/type_utils.h"
    if header_path.exists() and "ClusterClock" in header_path.read_text(encoding="utf-8"):
        add_inventory_row(rows, seen, engine, "datatype", "extension_runtime_type", "CLUSTERCLOCK", header_path, "Citus ClusterClock runtime type")
    return rows


def extract_citus_index_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    dep_path = clone_root / "src/backend/distributed/commands/dependencies.c"
    if dep_path.exists():
        text = dep_path.read_text(encoding="utf-8")
        if "RELKIND_INDEX" in text:
            add_inventory_row(rows, seen, engine, "index", "distributed_index_surface", "INDEX", dep_path, "Citus distributed dependency handling for indexes")
        if "RELKIND_PARTITIONED_INDEX" in text:
            add_inventory_row(rows, seen, engine, "index", "distributed_index_surface", "PARTITIONED_INDEX", dep_path, "Citus distributed dependency handling for partitioned indexes")
    planner_path = clone_root / "src/backend/distributed/planner/multi_join_order.c"
    if planner_path.exists() and "HASH_DISTRIBUTED" in planner_path.read_text(encoding="utf-8"):
        add_inventory_row(rows, seen, engine, "index", "distribution_index_surface", "HASH_DISTRIBUTED", planner_path, "Citus hash-distributed table/index planning surface")
    return rows


def extract_apache_ignite_index_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    path = clone_root / "modules/indexing/src/main/java/org/apache/ignite/internal/processors/query/h2/database/H2IndexType.java"
    if not path.exists():
        return rows
    text = path.read_text(encoding="utf-8")
    for name in extract_enum_members(text, "public enum H2IndexType"):
        add_inventory_row(rows, seen, engine, "index", "index_type_enum", name, path, "Ignite H2 index type enum")
    return rows


def extract_yugabytedb_datatype_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    path = clone_root / "src/postgres/src/include/catalog/pg_type.dat"
    if not path.exists():
        return []
    text = path.read_text(encoding="utf-8")
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    for match in re.finditer(r"typname => '([^']+)'", text):
        add_inventory_row(rows, seen, engine, "datatype", "catalog_typname", match.group(1), path, "YugabyteDB embedded PostgreSQL pg_type typname")
    return rows


def extract_yugabytedb_index_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    path = clone_root / "src/postgres/src/include/catalog/pg_am.dat"
    if not path.exists():
        return []
    text = path.read_text(encoding="utf-8")
    rows: list[dict[str, str]] = []
    seen: set[tuple[str, str, str, str]] = set()
    for match in re.finditer(r"\{[^}]*amname => '([^']+)'[^}]*amtype => 'i'[^}]*\}", text, re.S):
        add_inventory_row(rows, seen, engine, "index", "index_access_method", match.group(1), path, "YugabyteDB embedded PostgreSQL pg_am index access method")
    return rows


def extract_donor_datatype_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    if engine["id"] == "postgresql":
        return extract_postgresql_datatype_inventory(engine, clone_root)
    if engine["id"] in {"mysql", "mariadb"}:
        return extract_mysql_like_datatype_inventory(engine, clone_root)
    if engine["id"] == "firebird":
        return extract_firebird_datatype_inventory(engine, clone_root)
    if engine["id"] == "cassandra":
        return extract_cassandra_datatype_inventory(engine, clone_root)
    if engine["id"] == "clickhouse":
        return extract_clickhouse_datatype_inventory(engine, clone_root)
    if engine["id"] == "duckdb":
        return extract_duckdb_datatype_inventory(engine, clone_root)
    if engine["id"] == "influxdb":
        return extract_influxdb_datatype_inventory(engine, clone_root)
    if engine["id"] == "mongodb":
        return extract_mongodb_datatype_inventory(engine, clone_root)
    if engine["id"] == "neo4j":
        return extract_neo4j_datatype_inventory(engine, clone_root)
    if engine["id"] == "opensearch":
        return extract_opensearch_datatype_inventory(engine, clone_root)
    if engine["id"] == "redis":
        return extract_redis_datatype_inventory(engine, clone_root)
    if engine["id"] == "milvus":
        return extract_milvus_datatype_inventory(engine, clone_root)
    if engine["id"] == "dolt":
        return extract_dolt_datatype_inventory(engine, clone_root)
    if engine["id"] == "tidb":
        return extract_tidb_datatype_inventory(engine, clone_root)
    if engine["id"] == "vitess":
        return extract_vitess_datatype_inventory(engine, clone_root)
    if engine["id"] == "foundationdb":
        return extract_foundationdb_datatype_inventory(engine, clone_root)
    if engine["id"] == "immudb":
        return extract_immudb_datatype_inventory(engine, clone_root)
    if engine["id"] == "xtdb":
        return extract_xtdb_datatype_inventory(engine, clone_root)
    if engine["id"] == "apache_ignite":
        return extract_apache_ignite_datatype_inventory(engine, clone_root)
    if engine["id"] == "cockroachdb":
        return extract_cockroachdb_datatype_inventory(engine, clone_root)
    if engine["id"] == "citus":
        return extract_citus_datatype_inventory(engine, clone_root)
    if engine["id"] == "yugabytedb":
        return extract_yugabytedb_datatype_inventory(engine, clone_root)
    return []


def extract_donor_index_inventory(engine: dict, clone_root: Path) -> list[dict[str, str]]:
    if engine["id"] == "postgresql":
        return extract_postgresql_index_inventory(engine, clone_root)
    if engine["id"] in {"mysql", "mariadb"}:
        return extract_mysql_like_index_inventory(engine, clone_root)
    if engine["id"] == "firebird":
        return extract_firebird_index_inventory(engine, clone_root)
    if engine["id"] == "cassandra":
        return extract_cassandra_index_inventory(engine, clone_root)
    if engine["id"] == "clickhouse":
        return extract_clickhouse_index_inventory(engine, clone_root)
    if engine["id"] == "duckdb":
        return extract_duckdb_index_inventory(engine, clone_root)
    if engine["id"] == "mongodb":
        return extract_mongodb_index_inventory(engine, clone_root)
    if engine["id"] == "neo4j":
        return extract_neo4j_index_inventory(engine, clone_root)
    if engine["id"] == "milvus":
        return extract_milvus_index_inventory(engine, clone_root)
    if engine["id"] == "tidb":
        return extract_tidb_index_inventory(engine, clone_root)
    if engine["id"] == "vitess":
        return extract_vitess_index_inventory(engine, clone_root)
    if engine["id"] == "dolt":
        return extract_dolt_index_inventory(engine, clone_root)
    if engine["id"] == "immudb":
        return extract_immudb_index_inventory(engine, clone_root)
    if engine["id"] == "xtdb":
        return extract_xtdb_index_inventory(engine, clone_root)
    if engine["id"] == "apache_ignite":
        return extract_apache_ignite_index_inventory(engine, clone_root)
    if engine["id"] == "cockroachdb":
        return extract_cockroachdb_index_inventory(engine, clone_root)
    if engine["id"] == "citus":
        return extract_citus_index_inventory(engine, clone_root)
    if engine["id"] == "yugabytedb":
        return extract_yugabytedb_index_inventory(engine, clone_root)
    return []


def donor_index_surface_aliases(engine_id: str, donor_surface: str) -> set[str]:
    key = normalize_token(donor_surface)
    aliases = {key}
    overrides = {
        "mongodb": {
            "BTREE": {"DEFAULT_BTREE"},
            "HAYSTACK": {"GEO_HAYSTACK"},
        }
    }
    aliases.update(overrides.get(engine_id, {}).get(key, set()))
    return aliases


def donor_type_support_path(
    row: dict[str, str],
    engine: dict,
    native_type_keys: dict[str, str],
    engine_domain_keys: set[str],
    engine_mapped_compact_keys: set[str],
) -> tuple[str, str]:
    donor_key = row["donor_surface_key"]
    donor_surface = row["donor_surface"]
    surface_kind = row["surface_kind"]
    donor_compact = normalize_compact(donor_surface)

    mysql_like_blob_variants = {"TINYBLOB", "MEDIUMBLOB", "LONGBLOB"}
    mysql_like_text_variants = {"TINYTEXT", "MEDIUMTEXT", "LONGTEXT"}
    if engine["type_key"] == "MYSQL" and donor_key in mysql_like_blob_variants and "BLOB" in engine_domain_keys:
        return ("DOMAIN_PRECEDENT_EXISTS", "MySQL-family BLOB size-class variant can ride the existing domain-backed BLOB path")
    if engine["type_key"] == "MYSQL" and donor_key in mysql_like_text_variants and "TEXT" in engine_domain_keys:
        return ("DOMAIN_PRECEDENT_EXISTS", "MySQL-family TEXT size-class variant can ride the existing domain-backed TEXT path")
    if engine["id"] == "mongodb" and surface_kind == "bson_binary_subtype" and "BINARY" in engine_domain_keys:
        return ("DOMAIN_PRECEDENT_EXISTS", "MongoDB binary subtype can ride the current domain-backed BINARY mapping that preserves subtype metadata")
    if engine["id"] == "influxdb" and surface_kind == "influx_column_type" and donor_key == "TIME":
        return ("ALIAS_OR_CATALOG_NAME_ONLY", "InfluxDB exposes `time` as the terminology label for timestamp columns rather than as a distinct engine-native time-of-day datatype")

    alias_or_catalog_kinds = {
        "catalog_typname",
        "protocol_field_type",
        "bson_type",
        "redis_object_type",
    }
    if surface_kind in alias_or_catalog_kinds:
        return ("ALIAS_OR_CATALOG_NAME_ONLY", "Donor surface was extracted from a catalog/protocol/runtime naming layer, not from an explicit current ScratchBird engine mapping row")
    if donor_compact in engine_mapped_compact_keys:
        return ("ALIAS_OR_CATALOG_NAME_ONLY", "Donor surface differs only at the alias or naming-convention layer from an existing current engine mapping token")

    native_match = native_type_keys.get(donor_key, "")
    if native_match:
        return ("NATIVE_DIRECT", "ScratchBird already exposes a same-name native type and lacks only the donor-engine mapping row")

    return ("NO_CURRENT_PROOF", "Current local ScratchBird code does not prove a direct native, domain-precedent, or alias-only support path")


def build_missing_surface_reports(
    type_rows: list[dict[str, str]],
    index_registry_rows: list[dict[str, str]],
    native_types: list[str],
) -> dict[str, list[dict[str, str]]]:
    datatype_inventory: list[dict[str, str]] = []
    index_inventory: list[dict[str, str]] = []
    missing_datatypes: list[dict[str, str]] = []
    missing_indexes: list[dict[str, str]] = []
    summary_rows: list[dict[str, str]] = []

    engine_type_keys: dict[str, set[str]] = defaultdict(set)
    engine_domain_keys: dict[str, set[str]] = defaultdict(set)
    engine_mapped_compact_keys: dict[str, set[str]] = defaultdict(set)
    for row in type_rows:
        engine_name = row["engine_name"]
        emulated_type = row["emulated_type"]
        engine_type_keys[engine_name].add(normalize_token(emulated_type))
        engine_mapped_compact_keys[engine_name].add(normalize_compact(emulated_type))
        if row["storage_kind"] == "DOMAIN":
            engine_domain_keys[engine_name].add(normalize_token(emulated_type))

    native_type_keys = {normalize_token(name): name for name in native_types}
    sb_index_name_keys: dict[str, str] = {}
    for row in index_registry_rows:
        sb_index_name_keys[normalize_token(row["index_type"])] = row["index_type"]
        sb_index_name_keys[normalize_token(row["canonical_name"])] = row["index_type"]

    for engine in ENGINE_META:
        clone_root = donor_clone_root(engine)
        donor_datatypes = extract_donor_datatype_inventory(engine, clone_root)
        donor_indexes = extract_donor_index_inventory(engine, clone_root)
        datatype_inventory.extend(donor_datatypes)
        index_inventory.extend(donor_indexes)

        mapped_type_keys = engine_type_keys.get(engine["type_key"], set())
        mapped_index_keys = {normalize_token(row[0]) for row in engine["index_rows"]}
        engine_missing_type_count = 0
        engine_missing_index_count = 0

        for row in donor_datatypes:
            donor_key = row["donor_surface_key"]
            if donor_key in mapped_type_keys:
                continue
            engine_missing_type_count += 1
            native_match = native_type_keys.get(donor_key, "")
            support_path, support_path_basis = donor_type_support_path(
                row=row,
                engine=engine,
                native_type_keys=native_type_keys,
                engine_domain_keys=engine_domain_keys.get(engine["type_key"], set()),
                engine_mapped_compact_keys=engine_mapped_compact_keys.get(engine["type_key"], set()),
            )
            missing_datatypes.append(
                {
                    **row,
                    "sb_engine_mapping_present": "no",
                    "sb_native_exact_name_match": "yes" if native_match else "no",
                    "sb_native_exact_name": native_match,
                    "support_path": support_path,
                    "support_path_basis": support_path_basis,
                    "missing_class": "missing_engine_mapping_but_native_name_exists"
                    if native_match
                    else "missing_engine_mapping_and_no_direct_native_name",
                }
            )

        for row in donor_indexes:
            aliases = donor_index_surface_aliases(engine["id"], row["donor_surface"])
            if any(alias in mapped_index_keys for alias in aliases):
                continue
            engine_missing_index_count += 1
            direct_match = ""
            for alias in aliases:
                if alias in sb_index_name_keys:
                    direct_match = sb_index_name_keys[alias]
                    break
            missing_indexes.append(
                {
                    **row,
                    "sb_engine_mapping_present": "no",
                    "sb_direct_index_family_match": "yes" if direct_match else "no",
                    "sb_direct_index_family": direct_match,
                    "missing_class": "missing_engine_mapping_but_direct_family_exists"
                    if direct_match
                    else "missing_engine_mapping_and_no_direct_family",
                }
            )

        summary_rows.append(
            {
                "engine": engine["display"],
                "donor_datatype_surfaces": str(len(donor_datatypes)),
                "missing_datatype_surfaces": str(engine_missing_type_count),
                "datatype_support_native_direct": str(
                    sum(1 for row in missing_datatypes if row["engine"] == engine["display"] and row["support_path"] == "NATIVE_DIRECT")
                ),
                "datatype_support_domain_precedent_exists": str(
                    sum(1 for row in missing_datatypes if row["engine"] == engine["display"] and row["support_path"] == "DOMAIN_PRECEDENT_EXISTS")
                ),
                "datatype_support_alias_or_catalog_name_only": str(
                    sum(1 for row in missing_datatypes if row["engine"] == engine["display"] and row["support_path"] == "ALIAS_OR_CATALOG_NAME_ONLY")
                ),
                "datatype_support_no_current_proof": str(
                    sum(1 for row in missing_datatypes if row["engine"] == engine["display"] and row["support_path"] == "NO_CURRENT_PROOF")
                ),
                "donor_index_surfaces": str(len(donor_indexes)),
                "missing_index_surfaces": str(engine_missing_index_count),
            }
        )

    datatype_groups: dict[tuple[str, str], list[dict[str, str]]] = defaultdict(list)
    for row in missing_datatypes:
        datatype_groups[(row["surface_kind"], row["donor_surface_key"])].append(row)
    datatype_rollup: list[dict[str, str]] = []
    for (surface_kind, donor_key), rows in sorted(datatype_groups.items()):
        engines = sorted({row["engine"] for row in rows})
        datatype_rollup.append(
            {
                "surface_kind": surface_kind,
                "donor_surface_key": donor_key,
                "sample_surface": rows[0]["donor_surface"],
                "engines": "; ".join(engines),
                "engine_count": str(len(engines)),
                "native_exact_name_match_any": "yes"
                if any(row["sb_native_exact_name_match"] == "yes" for row in rows)
                else "no",
                "support_paths": "; ".join(sorted({row["support_path"] for row in rows})),
                "evidence_paths": "; ".join(sorted({row["evidence_path"] for row in rows})),
            }
        )

    index_groups: dict[tuple[str, str], list[dict[str, str]]] = defaultdict(list)
    for row in missing_indexes:
        index_groups[(row["surface_kind"], row["donor_surface_key"])].append(row)
    index_rollup: list[dict[str, str]] = []
    for (surface_kind, donor_key), rows in sorted(index_groups.items()):
        engines = sorted({row["engine"] for row in rows})
        index_rollup.append(
            {
                "surface_kind": surface_kind,
                "donor_surface_key": donor_key,
                "sample_surface": rows[0]["donor_surface"],
                "engines": "; ".join(engines),
                "engine_count": str(len(engines)),
                "direct_family_match_any": "yes"
                if any(row["sb_direct_index_family_match"] == "yes" for row in rows)
                else "no",
                "evidence_paths": "; ".join(sorted({row["evidence_path"] for row in rows})),
            }
        )

    return {
        "datatype_inventory": datatype_inventory,
        "index_inventory": index_inventory,
        "missing_datatypes": missing_datatypes,
        "missing_indexes": missing_indexes,
        "summary_rows": summary_rows,
        "datatype_rollup": datatype_rollup,
        "index_rollup": index_rollup,
    }


def build_engine_support_rows(
    engine: dict,
    type_rows: list[dict[str, str]],
    native_types: list[str],
    index_registry_rows: list[dict[str, str]],
) -> dict[str, list[dict[str, str]]]:
    clone_root = donor_clone_root(engine)
    donor_datatypes = extract_donor_datatype_inventory(engine, clone_root)
    donor_indexes = extract_donor_index_inventory(engine, clone_root)

    engine_type_keys: dict[str, set[str]] = defaultdict(set)
    engine_domain_keys: dict[str, set[str]] = defaultdict(set)
    engine_mapped_compact_keys: dict[str, set[str]] = defaultdict(set)
    for row in type_rows:
        engine_name = row["engine_name"]
        emulated_type = row["emulated_type"]
        engine_type_keys[engine_name].add(normalize_token(emulated_type))
        engine_mapped_compact_keys[engine_name].add(normalize_compact(emulated_type))
        if row["storage_kind"] == "DOMAIN":
            engine_domain_keys[engine_name].add(normalize_token(emulated_type))

    native_type_keys = {normalize_token(name): name for name in native_types}
    sb_index_name_keys: dict[str, str] = {}
    for row in index_registry_rows:
        sb_index_name_keys[normalize_token(row["index_type"])] = row["index_type"]
        sb_index_name_keys[normalize_token(row["canonical_name"])] = row["index_type"]

    mapped_type_keys = engine_type_keys.get(engine["type_key"], set())
    mapped_index_keys = {normalize_token(row[0]) for row in engine["index_rows"]}

    datatype_support_rows: list[dict[str, str]] = []
    index_support_rows: list[dict[str, str]] = []

    for row in donor_datatypes:
        donor_key = row["donor_surface_key"]
        native_match = native_type_keys.get(donor_key, "")
        mapping_present = donor_key in mapped_type_keys
        support_path, support_path_basis = donor_type_support_path(
            row=row,
            engine=engine,
            native_type_keys=native_type_keys,
            engine_domain_keys=engine_domain_keys.get(engine["type_key"], set()),
            engine_mapped_compact_keys=engine_mapped_compact_keys.get(engine["type_key"], set()),
        )
        datatype_support_rows.append(
            {
                **row,
                "sb_engine_mapping_present": "yes" if mapping_present else "no",
                "sb_native_exact_name_match": "yes" if native_match else "no",
                "sb_native_exact_name": native_match,
                "support_path": "ENGINE_MAPPING_PRESENT" if mapping_present else support_path,
                "support_path_basis": "Current ScratchBird comparison-family mapping row already exists"
                if mapping_present
                else support_path_basis,
            }
        )

    for row in donor_indexes:
        aliases = donor_index_surface_aliases(engine["id"], row["donor_surface"])
        engine_mapping_present = any(alias in mapped_index_keys for alias in aliases)
        direct_match = ""
        for alias in aliases:
            if alias in sb_index_name_keys:
                direct_match = sb_index_name_keys[alias]
                break
        index_support_rows.append(
            {
                **row,
                "sb_engine_mapping_present": "yes" if engine_mapping_present else "no",
                "sb_direct_index_family_match": "yes" if direct_match else "no",
                "sb_direct_index_family": direct_match,
            }
        )

    return {
        "donor_datatypes": donor_datatypes,
        "donor_indexes": donor_indexes,
        "datatype_support_rows": datatype_support_rows,
        "index_support_rows": index_support_rows,
    }


def build_section_authority_rows(engine: dict) -> tuple[list[dict[str, str]], list[dict[str, str]]]:
    clone_root = donor_clone_root(engine)
    rows: list[dict[str, str]] = []
    summary_rows: list[dict[str, str]] = []
    for section in engine["donor_sources"].keys():
        donor_rows = matching_paths(clone_root, engine["donor_sources"][section])
        sb_rows = matching_paths(ROOT, sb_section_paths(section, engine))
        for side, section_rows in (("donor", donor_rows), ("scratchbird", sb_rows)):
            for row in section_rows:
                rows.append(
                    {
                        "section": section,
                        "side": side,
                        "path": row["path"],
                        "exists": row["exists"],
                        "kind": row["kind"],
                        "file_count": row["file_count"],
                    }
                )
        summary_rows.append(
            {
                "section": section,
                "donor_paths_present": str(sum(1 for row in donor_rows if row["exists"] == "yes")),
                "donor_paths_total": str(len(donor_rows)),
                "donor_file_total": str(sum(int(row["file_count"]) for row in donor_rows)),
                "scratchbird_paths_present": str(sum(1 for row in sb_rows if row["exists"] == "yes")),
                "scratchbird_paths_total": str(len(sb_rows)),
                "scratchbird_file_total": str(sum(int(row["file_count"]) for row in sb_rows)),
            }
        )
    return rows, summary_rows


def strip_sql_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"--.*?$", "", text, flags=re.M)
    return text


def extract_sql_columns_from_body(body: str) -> list[tuple[str, str]]:
    rows: list[tuple[str, str]] = []
    for raw_line in body.splitlines():
        line = raw_line.strip().rstrip(",")
        if not line:
            continue
        upper = line.upper()
        if upper.startswith(
            (
                "PRIMARY KEY",
                "UNIQUE",
                "KEY ",
                "INDEX ",
                "CONSTRAINT",
                "FAMILY",
                "FOREIGN KEY",
                "CHECK",
                "INTERLEAVE",
                "PARTITION",
                "LOCALITY",
            )
        ):
            continue
        match = re.match(r'[`"]?([A-Za-z0-9_]+(?:\.[A-Za-z0-9_]+)?)["`]?\s+(.+)', line)
        if not match:
            continue
        column_name = match.group(1).split(".")[-1]
        column_type = re.split(
            r"\s+(?:NOT|NULL|DEFAULT|PRIMARY|UNIQUE|REFERENCES|CONSTRAINT|CHECK|COLLATE|FAMILY|VISIBLE|AUTO_INCREMENT|COMMENT|ENCODE|USING|AS|GENERATED|STORED|VIRTUAL)\b",
            match.group(2),
            1,
            flags=re.I,
        )[0].strip()
        rows.append((column_name, column_type))
    return rows


def extract_sql_catalog_objects(
    sql_paths: list[Path],
    *,
    catalog_scope: str,
    default_schema: str = "",
    bootstrap_mode: str,
    note: str,
) -> tuple[list[dict[str, str]], list[dict[str, str]], list[dict[str, str]]]:
    inventory_rows: list[dict[str, str]] = []
    column_rows: list[dict[str, str]] = []
    bootstrap_rows: list[dict[str, str]] = []
    for path in sql_paths:
        if not path.exists():
            continue
        text = strip_sql_comments(path.read_text(encoding="utf-8"))
        for match in re.finditer(
            r"CREATE\s+(TABLE|VIEW)\s+(?:IF NOT EXISTS\s+)?([`\"A-Za-z0-9_.]+)\s*\((.*?)\)\s*(?:ENGINE\b|CHARSET\b|WITH\b|COMMENT\b|ROW_FORMAT\b|PARTITION\b|;|$)",
            text,
            re.S | re.I,
        ):
            object_kind = "system_view" if match.group(1).upper() == "VIEW" else "system_table"
            object_name = match.group(2).strip("`\"")
            if default_schema and "." not in object_name:
                object_name = f"{default_schema}.{object_name}"
            columns = extract_sql_columns_from_body(match.group(3))
            inventory_rows.append(
                {
                    "catalog_scope": catalog_scope,
                    "object_name": object_name,
                    "object_kind": object_kind,
                    "definition_source": rel(path),
                    "field_count": str(len(columns)),
                    "bootstrap_mode": bootstrap_mode,
                    "notes": note,
                }
            )
            for ordinal, (column_name, column_type) in enumerate(columns, start=1):
                column_rows.append(
                    {
                        "object_name": object_name,
                        "ordinal": str(ordinal),
                        "column_name": column_name,
                        "column_type": column_type,
                        "column_source": rel(path),
                    }
                )
            bootstrap_rows.append(
                {
                    "object_name": object_name,
                    "entry_source": rel(path),
                    "evidence_kind": "sql_definition_present",
                    "entry_value": "declared",
                    "note": note,
                }
            )
        for match in re.finditer(
            r"CREATE\s+SEQUENCE\s+(?:IF NOT EXISTS\s+)?([`\"A-Za-z0-9_.]+)",
            text,
            re.I,
        ):
            object_name = match.group(1).strip("`\"")
            if default_schema and "." not in object_name:
                object_name = f"{default_schema}.{object_name}"
            inventory_rows.append(
                {
                    "catalog_scope": catalog_scope,
                    "object_name": object_name,
                    "object_kind": "system_sequence",
                    "definition_source": rel(path),
                    "field_count": "",
                    "bootstrap_mode": bootstrap_mode,
                    "notes": note,
                }
            )
            bootstrap_rows.append(
                {
                    "object_name": object_name,
                    "entry_source": rel(path),
                    "evidence_kind": "sql_definition_present",
                    "entry_value": "declared",
                    "note": note,
                }
            )
    return inventory_rows, column_rows, bootstrap_rows


def extract_mysql_like_catalog(clone_root: Path, definition_files: list[str], data_files: list[str]) -> dict[str, object]:
    inventory_rows: list[dict[str, str]] = []
    column_rows: list[dict[str, str]] = []
    bootstrap_rows: list[dict[str, str]] = []
    bootstrap_counts: Counter[str] = Counter()

    for relpath in [*definition_files, *data_files]:
        text = (clone_root / relpath).read_text(encoding="utf-8")
        for match in re.finditer(r"\b(?:INSERT|REPLACE)\s+INTO\s+(?:[`\"]?[A-Za-z0-9_$]+[`\"]?\.)?[`\"]?([A-Za-z0-9_$]+)[`\"]?", text, re.I):
            bootstrap_counts[match.group(1)] += 1

    for relpath in definition_files:
        text = (clone_root / relpath).read_text(encoding="utf-8")
        for match in re.finditer(
            r"CREATE\s+TABLE\s+IF\s+NOT\s+EXISTS\s+[`\"]?([A-Za-z0-9_$]+)[`\"]?\s*\((.*?)\)\s*engine=",
            text,
            re.S | re.I,
        ):
            name = match.group(1)
            columns = parse_sql_columns(match.group(2))
            inventory_rows.append(
                {
                    "catalog_scope": "mysql",
                    "object_name": name,
                    "object_kind": "system_table",
                    "definition_source": relpath,
                    "field_count": str(len(columns)),
                    "bootstrap_mode": "static_sql_bootstrap",
                    "notes": "Definition extracted from system table bootstrap SQL",
                }
            )
            for ordinal, (column_name, column_type) in enumerate(columns, start=1):
                column_rows.append(
                    {
                        "object_name": name,
                        "ordinal": str(ordinal),
                        "column_name": column_name,
                        "column_type": column_type,
                        "column_source": relpath,
                    }
                )

        for match in re.finditer(
            r"CREATE(?:\s+DEFINER\s*=\s*[^ ]+)?\s+SQL\s+SECURITY\s+\w+\s+VIEW\s+IF\s+NOT\s+EXISTS\s+[`\"]?([A-Za-z0-9_$]+)[`\"]?\s+AS\b",
            text,
            re.S | re.I,
        ):
            inventory_rows.append(
                {
                    "catalog_scope": "mysql",
                    "object_name": match.group(1),
                    "object_kind": "system_view",
                    "definition_source": relpath,
                    "field_count": "",
                    "bootstrap_mode": "view_projection",
                    "notes": "View definition present in bootstrap SQL",
                }
            )

    for row in inventory_rows:
        bootstrap_rows.append(
            {
                "object_name": row["object_name"],
                "entry_source": "; ".join(data_files or definition_files),
                "evidence_kind": "insert_or_replace_statement_count",
                "entry_value": str(bootstrap_counts.get(row["object_name"], 0)),
                "note": "Statement count from bootstrap SQL/data scripts; multi-row VALUES clauses are not expanded",
            }
        )

    return {
        "inventory_rows": inventory_rows,
        "column_rows": column_rows,
        "bootstrap_rows": bootstrap_rows,
        "summary_lines": [
            f"- Parsed `{len(inventory_rows)}` system SQL objects from donor bootstrap scripts.",
            "- Bootstrap evidence is counted from explicit `INSERT INTO` / `REPLACE INTO` statements in the donor SQL files.",
        ],
    }


def extract_postgresql_catalog(clone_root: Path) -> dict[str, object]:
    inventory_rows: list[dict[str, str]] = []
    column_rows: list[dict[str, str]] = []
    bootstrap_rows: list[dict[str, str]] = []
    catalog_dir = clone_root / "src/include/catalog"

    for header in sorted(catalog_dir.glob("pg_*.h")):
        text = header.read_text(encoding="utf-8")
        match = re.search(rf"CATALOG\(({header.stem}),[^\)]*\)\s*([^\{{]*)\{{(.*?)\n\}}", text, re.S)
        if not match:
            continue
        flags = " ".join(match.group(2).split())
        body = match.group(3)
        columns = []
        for field_match in re.finditer(
            r"^\s*([A-Za-z0-9_]+)\s+([A-Za-z0-9_]+)(?:\[[^\]]+\])?(?:\s+BKI_[^;]+)?;",
            body,
            re.M,
        ):
            columns.append((field_match.group(2), field_match.group(1)))
        dat_path = catalog_dir / f"{header.stem}.dat"
        has_dat = dat_path.exists()
        dat_rows = count_pg_dat_rows(dat_path) if has_dat else 0
        inventory_rows.append(
            {
                "catalog_scope": "pg_catalog",
                "object_name": header.stem,
                "object_kind": "system_catalog",
                "definition_source": rel(header),
                "field_count": str(len(columns)),
                "bootstrap_mode": "static_dat_rows" if has_dat else "runtime_or_empty",
                "notes": flags,
            }
        )
        for ordinal, (column_name, column_type) in enumerate(columns, start=1):
            column_rows.append(
                {
                    "object_name": header.stem,
                    "ordinal": str(ordinal),
                    "column_name": column_name,
                    "column_type": column_type,
                    "column_source": rel(header),
                }
            )
        bootstrap_rows.append(
            {
                "object_name": header.stem,
                "entry_source": rel(dat_path) if has_dat else rel(header),
                "evidence_kind": "bootstrap_dat_row_count" if has_dat else "no_static_dat_rows",
                "entry_value": str(dat_rows) if has_dat else "",
                "note": "Counted from top-level `{ ... }` entries in catalog .dat file" if has_dat else "Catalog has no matching .dat bootstrap file in this checkout",
            }
        )

    return {
        "inventory_rows": inventory_rows,
        "column_rows": column_rows,
        "bootstrap_rows": bootstrap_rows,
        "summary_lines": [
            f"- Parsed `{len(inventory_rows)}` PostgreSQL catalog headers from `src/include/catalog`.",
            "- Matching `.dat` files are counted as static bootstrap row sources; catalogs without `.dat` files are runtime-managed or initially empty.",
        ],
    }


def extract_firebird_catalog(clone_root: Path) -> dict[str, object]:
    inventory_rows: list[dict[str, str]] = []
    column_rows: list[dict[str, str]] = []
    bootstrap_rows: list[dict[str, str]] = []
    names_path = clone_root / "src/jrd/names.h"
    relations_path = clone_root / "src/jrd/relations.h"
    init_path = clone_root / "src/jrd/ini.epp"
    names_map = {
        match.group(2): match.group(1)
        for match in re.finditer(r'NAME\("([^"]+)",\s*([A-Za-z0-9_]+)\)', names_path.read_text(encoding="utf-8"))
    }
    init_text = init_path.read_text(encoding="utf-8")
    store_counts = Counter(re.findall(r"\bIN\s+(RDB\$[A-Z0-9_]+)\b", init_text))
    relations_text = relations_path.read_text(encoding="utf-8")
    for match in re.finditer(r"// Relation\s+(\d+)\s+\(([^)]+)\)\s*RELATION\((.*?)\)(.*?)END_RELATION", relations_text, re.S):
        relation_id = match.group(1)
        relation_name = match.group(2)
        body = match.group(4)
        fields: list[tuple[str, str]] = []
        for field_match in re.finditer(r"FIELD\([^,]+,\s*([A-Za-z0-9_]+),\s*([A-Za-z0-9_]+),", body):
            name_token = field_match.group(1)
            descriptor = field_match.group(2)
            fields.append((names_map.get(name_token, name_token), descriptor))
        inventory_rows.append(
            {
                "catalog_scope": "RDB$",
                "object_name": relation_name,
                "object_kind": "system_relation",
                "definition_source": rel(relations_path),
                "field_count": str(len(fields)),
                "bootstrap_mode": "ini_epp_bootstrap",
                "notes": f"Relation id {relation_id}",
            }
        )
        for ordinal, (column_name, column_type) in enumerate(fields, start=1):
            column_rows.append(
                {
                    "object_name": relation_name,
                    "ordinal": str(ordinal),
                    "column_name": column_name,
                    "column_type": column_type,
                    "column_source": rel(relations_path),
                }
            )
        bootstrap_rows.append(
            {
                "object_name": relation_name,
                "entry_source": rel(init_path),
                "evidence_kind": "ini_store_site_count",
                "entry_value": str(store_counts.get(relation_name, 0)),
                "note": "Count of `STORE ... IN <relation>` sites in metadata initialization code; loops may emit multiple rows per site",
            }
        )

    return {
        "inventory_rows": inventory_rows,
        "column_rows": column_rows,
        "bootstrap_rows": bootstrap_rows,
        "summary_lines": [
            f"- Parsed `{len(inventory_rows)}` Firebird system relations from `relations.h`.",
            "- Bootstrap evidence is derived from `ini.epp` store sites because initialization is procedural rather than pure static data-file driven.",
        ],
    }


def extract_cassandra_catalog(clone_root: Path) -> dict[str, object]:
    inventory_rows: list[dict[str, str]] = []
    column_rows: list[dict[str, str]] = []
    bootstrap_rows: list[dict[str, str]] = []

    for relpath, scope in [
        ("src/java/org/apache/cassandra/db/SystemKeyspace.java", "system"),
        ("src/java/org/apache/cassandra/schema/SchemaKeyspace.java", "system_schema"),
    ]:
        path = clone_root / relpath
        text = path.read_text(encoding="utf-8")
        const_map = parse_string_constant_map(path, "java")
        for block in re.finditer(
            r"(?:public|private)\s+static\s+final\s+TableMetadata\s+[A-Za-z0-9_]+\s*=\s*(.*?)\.build\(\);",
            text,
            re.S,
        ):
            segment = block.group(1)
            parse_match = re.search(r"parse\(([^,]+),", segment)
            if not parse_match:
                continue
            object_name = const_map.get(parse_match.group(1).strip(), parse_match.group(1).strip())
            string_parts = re.findall(r'"([^"]*)"', segment)
            cql = "".join(string_parts)
            cql_match = re.search(r"CREATE TABLE(?: IF NOT EXISTS)? %s \((.*)\)", cql, re.S)
            columns = parse_sql_columns(cql_match.group(1)) if cql_match else []
            inventory_rows.append(
                {
                    "catalog_scope": scope,
                    "object_name": object_name,
                    "object_kind": "system_table",
                    "definition_source": rel(path),
                    "field_count": str(len(columns)),
                    "bootstrap_mode": "metadata_declared_runtime_rows",
                    "notes": "Declared through TableMetadata + KeyspaceMetadata bootstrap code",
                }
            )
            for ordinal, (column_name, column_type) in enumerate(columns, start=1):
                column_rows.append(
                    {
                        "object_name": object_name,
                        "ordinal": str(ordinal),
                        "column_name": column_name,
                        "column_type": column_type,
                        "column_source": rel(path),
                    }
                )
            bootstrap_rows.append(
                {
                    "object_name": object_name,
                    "entry_source": rel(path),
                    "evidence_kind": "metadata_declaration",
                    "entry_value": "declared",
                    "note": "Table exists at keyspace bootstrap; persistent row contents are populated by runtime services rather than static insert scripts",
                }
            )

    return {
        "inventory_rows": inventory_rows,
        "column_rows": column_rows,
        "bootstrap_rows": bootstrap_rows,
        "summary_lines": [
            f"- Parsed `{len(inventory_rows)}` Cassandra system tables from `SystemKeyspace.java` and `SchemaKeyspace.java`.",
            "- Static table shapes are available in source; initial row contents are service-populated at runtime and are not inferred beyond the declaring code paths.",
        ],
    }


def extract_clickhouse_catalog(clone_root: Path) -> dict[str, object]:
    inventory_rows: list[dict[str, str]] = []
    bootstrap_rows: list[dict[str, str]] = []
    attach_path = clone_root / "src/Storages/System/attachSystemTables.cpp"
    text = attach_path.read_text(encoding="utf-8")
    for match in re.finditer(
        r'attach(?:NoDescription)?<[^>]+>\(context,\s*system_database,\s*"([^"]+)",\s*"([^"]*)"',
        text,
    ):
        table_name = match.group(1)
        description = " ".join(match.group(2).split())
        inventory_rows.append(
            {
                "catalog_scope": "system",
                "object_name": table_name,
                "object_kind": "system_table",
                "definition_source": rel(attach_path),
                "field_count": "",
                "bootstrap_mode": "attached_at_server_start",
                "notes": description,
            }
        )
        bootstrap_rows.append(
            {
                "object_name": table_name,
                "entry_source": rel(attach_path),
                "evidence_kind": "attach_call",
                "entry_value": "attached",
                "note": "Rows are materialized by the table implementation at query time",
            }
        )

    return {
        "inventory_rows": inventory_rows,
        "column_rows": [],
        "bootstrap_rows": bootstrap_rows,
        "summary_lines": [
            f"- Parsed `{len(inventory_rows)}` ClickHouse `system.*` tables from `attachSystemTables.cpp`.",
            "- ClickHouse system tables are attached explicitly at startup; most rows are generated dynamically by storage implementations instead of static bootstrap scripts.",
        ],
    }


def extract_duckdb_catalog(clone_root: Path) -> dict[str, object]:
    inventory_rows: list[dict[str, str]] = []
    bootstrap_rows: list[dict[str, str]] = []
    schema_path = clone_root / "src/catalog/default/default_schemas.cpp"
    schema_text = schema_path.read_text(encoding="utf-8")
    for schema_name in re.findall(r'\{"([^"]+)"\}', schema_text):
        inventory_rows.append(
            {
                "catalog_scope": "catalog",
                "object_name": schema_name,
                "object_kind": "default_schema",
                "definition_source": rel(schema_path),
                "field_count": "",
                "bootstrap_mode": "created_during_default_catalog_init",
                "notes": "Built-in schema name",
            }
        )
        bootstrap_rows.append(
            {
                "object_name": schema_name,
                "entry_source": rel(schema_path),
                "evidence_kind": "default_schema_name",
                "entry_value": "declared",
                "note": "Schema is created during default catalog setup",
            }
        )

    system_dir = clone_root / "src/function/table/system"
    for path in sorted(system_dir.glob("*.cpp")):
        text = path.read_text(encoding="utf-8")
        for func_name in re.findall(r'TableFunction\("([^"]+)"', text):
            inventory_rows.append(
                {
                    "catalog_scope": "system_table_function",
                    "object_name": func_name,
                    "object_kind": "system_table_function",
                    "definition_source": rel(path),
                    "field_count": "",
                    "bootstrap_mode": "registered_at_startup",
                    "notes": "DuckDB system/pragma table function",
                }
            )
            bootstrap_rows.append(
                {
                    "object_name": func_name,
                    "entry_source": rel(path),
                    "evidence_kind": "table_function_registration",
                    "entry_value": "registered",
                    "note": "Output rows are computed on invocation rather than pre-seeded",
                }
            )

    return {
        "inventory_rows": inventory_rows,
        "column_rows": [],
        "bootstrap_rows": bootstrap_rows,
        "summary_lines": [
            "- DuckDB exposes built-in schemas plus system/pragma table functions instead of a single static relational catalog bootstrap script.",
            f"- Parsed `{len(inventory_rows)}` default schema or system-function surfaces from local source.",
        ],
    }


def extract_influxdb_catalog(clone_root: Path) -> dict[str, object]:
    inventory_rows: list[dict[str, str]] = []
    bootstrap_rows: list[dict[str, str]] = []
    lib_path = clone_root / "influxdb3_system_tables/src/lib.rs"
    lib_text = lib_path.read_text(encoding="utf-8")
    const_map = parse_string_constant_map(lib_path, "rust")
    for const_name, table_name in sorted(const_map.items()):
        if not const_name.endswith("_TABLE_NAME") or const_name == "TABLE_NAME_PREDICATE":
            continue
        inventory_rows.append(
            {
                "catalog_scope": "system",
                "object_name": table_name,
                "object_kind": "system_table",
                "definition_source": rel(lib_path),
                "field_count": "",
                "bootstrap_mode": "provider_registered",
                "notes": const_name,
            }
        )
        bootstrap_rows.append(
            {
                "object_name": table_name,
                "entry_source": rel(lib_path),
                "evidence_kind": "provider_registration",
                "entry_value": "registered",
                "note": "Table provider registered in `AllSystemSchemaTablesProvider`; row contents depend on catalog and runtime state",
            }
        )

    return {
        "inventory_rows": inventory_rows,
        "column_rows": [],
        "bootstrap_rows": bootstrap_rows,
        "summary_lines": [
            f"- Parsed `{len(inventory_rows)}` InfluxDB `system.*` tables from the Rust provider registry.",
            "- The system schema is provider-driven; static table names are source-defined, while row contents are read from the catalog or runtime services.",
        ],
    }


def extract_mongodb_catalog(clone_root: Path) -> dict[str, object]:
    inventory_rows: list[dict[str, str]] = []
    bootstrap_rows: list[dict[str, str]] = []
    reserved_path = clone_root / "src/mongo/db/namespace_string_reserved.h"
    text = reserved_path.read_text(encoding="utf-8")
    for match in re.finditer(r'X\(([A-Za-z0-9_]+),\s*DatabaseName::k([A-Za-z0-9_]+),\s*"([^"]+)"_sd\)', text):
        db_name = match.group(2).lower()
        coll_name = match.group(3)
        inventory_rows.append(
            {
                "catalog_scope": db_name,
                "object_name": coll_name,
                "object_kind": "reserved_namespace",
                "definition_source": rel(reserved_path),
                "field_count": "",
                "bootstrap_mode": "runtime_collection_or_namespace",
                "notes": match.group(1),
            }
        )
        bootstrap_rows.append(
            {
                "object_name": f"{db_name}.{coll_name}",
                "entry_source": rel(reserved_path),
                "evidence_kind": "reserved_namespace_constant",
                "entry_value": "declared",
                "note": "Namespace is reserved in source; collection creation and row contents are topology/runtime dependent",
            }
        )

    return {
        "inventory_rows": inventory_rows,
        "column_rows": [],
        "bootstrap_rows": bootstrap_rows,
        "summary_lines": [
            f"- Parsed `{len(inventory_rows)}` MongoDB reserved system namespaces from `namespace_string_reserved.h`.",
            "- MongoDB catalog state is namespace-driven rather than SQL-table-driven; reserved namespaces are source-defined, but collection contents are runtime managed.",
        ],
    }


def extract_redis_catalog(clone_root: Path) -> dict[str, object]:
    inventory_rows: list[dict[str, str]] = []
    bootstrap_rows: list[dict[str, str]] = []
    config_path = clone_root / "src/config.c"
    server_path = clone_root / "src/server.c"
    config_text = config_path.read_text(encoding="utf-8")
    dbnum_match = re.search(r'createIntConfig\("databases".*?,\s*server\.dbnum,\s*(\d+),', config_text)
    default_db_count = dbnum_match.group(1) if dbnum_match else ""
    inventory_rows.append(
        {
            "catalog_scope": "logical_db_array",
            "object_name": "redis_db_slots",
            "object_kind": "logical_database_pool",
            "definition_source": f"{rel(config_path)}; {rel(server_path)}",
            "field_count": default_db_count,
            "bootstrap_mode": "empty_db_array_on_start",
            "notes": "Redis uses in-memory logical DB slots, not SQL-style system tables",
        }
    )
    bootstrap_rows.append(
        {
            "object_name": "redis_db_slots",
            "entry_source": f"{rel(config_path)}; {rel(server_path)}",
            "evidence_kind": "default_db_count",
            "entry_value": default_db_count,
            "note": "Default configured logical DB count; each slot is initialized empty during server startup",
        }
    )
    return {
        "inventory_rows": inventory_rows,
        "column_rows": [],
        "bootstrap_rows": bootstrap_rows,
        "summary_lines": [
            "- Redis does not expose a relational system-catalog surface in source; metadata is held in server structures and logical DB arrays.",
            f"- The local source default is `{default_db_count or 'unknown'}` logical DB slots, each allocated empty at startup.",
        ],
    }


def extract_opensearch_catalog(clone_root: Path) -> dict[str, object]:
    inventory_rows: list[dict[str, str]] = []
    bootstrap_rows: list[dict[str, str]] = []
    task_results_path = clone_root / "server/src/main/java/org/opensearch/tasks/TaskResultsService.java"
    task_results_constants = parse_string_constant_map(task_results_path, "java") if task_results_path.exists() else {}
    for base in ["server/src/main/java", "plugins"]:
        for path in sorted((clone_root / base).rglob("*.java")):
            lowered = path.as_posix().lower()
            if "/test/" in lowered or "internalclustertest" in lowered or "yamlresttest" in lowered:
                continue
            text = path.read_text(encoding="utf-8")
            const_map = parse_string_constant_map(path, "java")
            for match in re.finditer(r'new\s+SystemIndexDescriptor\((.+?),\s*"([^"]+)"\)', text):
                raw_pattern = " ".join(match.group(1).split())
                object_name = raw_pattern
                literal = re.fullmatch(r'"([^"]+)"', raw_pattern)
                if literal:
                    object_name = literal.group(1)
                elif raw_pattern in const_map:
                    object_name = const_map[raw_pattern]
                else:
                    concat = re.fullmatch(r'([A-Za-z0-9_]+)\s*\+\s*"([^"]+)"', raw_pattern)
                    if concat and concat.group(1) in const_map:
                        object_name = const_map[concat.group(1)] + concat.group(2)
                    elif concat and concat.group(1) in task_results_constants:
                        object_name = task_results_constants[concat.group(1)] + concat.group(2)
                inventory_rows.append(
                    {
                        "catalog_scope": "system_index",
                        "object_name": object_name,
                        "object_kind": "system_index_pattern",
                        "definition_source": rel(path),
                        "field_count": "",
                        "bootstrap_mode": "descriptor_registered",
                        "notes": match.group(2),
                    }
                )
                bootstrap_rows.append(
                    {
                        "object_name": object_name,
                        "entry_source": rel(path),
                        "evidence_kind": "system_index_descriptor",
                        "entry_value": "declared",
                        "note": "System index descriptor is registered in code; actual index creation and documents are runtime managed",
                    }
                )
    return {
        "inventory_rows": inventory_rows,
        "column_rows": [],
        "bootstrap_rows": bootstrap_rows,
        "summary_lines": [
            f"- Parsed `{len(inventory_rows)}` OpenSearch system-index descriptors from server and plugin source.",
            "- OpenSearch uses system-index patterns and cluster metadata rather than SQL-style system tables.",
        ],
    }


def extract_neo4j_catalog(clone_root: Path) -> dict[str, object]:
    config_path = clone_root / "community/configuration/src/main/java/org/neo4j/configuration/GraphDatabaseSettings.java"
    named_db_path = clone_root / "community/common/src/main/java/org/neo4j/kernel/database/NamedDatabaseId.java"
    system_graph_path = clone_root / "community/kernel/src/main/java/org/neo4j/dbms/database/SystemGraphComponent.java"
    components_path = clone_root / "community/kernel/src/main/java/org/neo4j/dbms/database/SystemGraphComponents.java"
    security_path = clone_root / "community/security/src/main/java/org/neo4j/server/security/systemgraph/UserSecurityGraphComponent.java"
    inventory_rows = [
        {
            "catalog_scope": "dbms",
            "object_name": "system",
            "object_kind": "system_database",
            "definition_source": f"{rel(config_path)}; {rel(named_db_path)}",
            "field_count": "",
            "bootstrap_mode": "dbms_bootstrap",
            "notes": "Named system database for DBMS-wide metadata",
        },
        {
            "catalog_scope": "system",
            "object_name": "system_graph_components",
            "object_kind": "system_graph_registry",
            "definition_source": f"{rel(system_graph_path)}; {rel(components_path)}",
            "field_count": "",
            "bootstrap_mode": "component_initializers",
            "notes": "Sub-graphs are initialized and upgraded through versioned component logic",
        },
        {
            "catalog_scope": "system",
            "object_name": "security_graph_component",
            "object_kind": "system_graph_subgraph",
            "definition_source": rel(security_path),
            "field_count": "",
            "bootstrap_mode": "component_initializers",
            "notes": "Security metadata lives in the system graph, not in SQL tables",
        },
    ]
    bootstrap_rows = [
        {
            "object_name": "system",
            "entry_source": f"{rel(config_path)}; {rel(named_db_path)}",
            "evidence_kind": "system_database_name",
            "entry_value": "system",
            "note": "DBMS bootstrap creates/opens the system database",
        },
        {
            "object_name": "system_graph_components",
            "entry_source": f"{rel(system_graph_path)}; {rel(components_path)}",
            "evidence_kind": "component_initializer_contract",
            "entry_value": "declared",
            "note": "Each component owns its system-graph bootstrap and upgrade logic",
        },
    ]
    return {
        "inventory_rows": inventory_rows,
        "column_rows": [],
        "bootstrap_rows": bootstrap_rows,
        "summary_lines": [
            "- Neo4j stores DBMS metadata in the `system` database and versioned system-graph components rather than SQL system tables.",
            "- The packet records the source-backed system graph bootstrap surfaces without inventing node/property rows beyond the checked initializer contracts.",
        ],
    }


def extract_milvus_catalog(clone_root: Path) -> dict[str, object]:
    inventory_rows: list[dict[str, str]] = []
    bootstrap_rows: list[dict[str, str]] = []
    for path in sorted((clone_root / "internal/metastore/model").glob("*.go")):
        if path.name.endswith("_test.go"):
            continue
        text = path.read_text(encoding="utf-8")
        for match in re.finditer(r"type\s+([A-Za-z0-9_]+)\s+struct\s*\{", text):
            if not match.group(1) or not match.group(1)[0].isupper():
                continue
            inventory_rows.append(
                {
                    "catalog_scope": "metastore_model",
                    "object_name": match.group(1),
                    "object_kind": "metadata_struct",
                    "definition_source": rel(path),
                    "field_count": "",
                    "bootstrap_mode": "service_owned_metadata",
                    "notes": "Milvus metadata model struct",
                }
            )
            bootstrap_rows.append(
                {
                    "object_name": match.group(1),
                    "entry_source": rel(path),
                    "evidence_kind": "metadata_struct_definition",
                    "entry_value": "declared",
                    "note": "Materialized in metastore/backing service rather than a SQL system-table bootstrap script",
                }
            )
    return {
        "inventory_rows": inventory_rows,
        "column_rows": [],
        "bootstrap_rows": bootstrap_rows,
        "summary_lines": [
            f"- Parsed `{len(inventory_rows)}` Milvus metastore model structs from local source.",
            "- Milvus catalog state is service/metastore owned; this packet records the metadata object model rather than inventing SQL-style system tables that do not exist in the donor source.",
        ],
    }


def extract_immudb_catalog(clone_root: Path) -> dict[str, object]:
    inventory_rows: list[dict[str, str]] = []
    column_rows: list[dict[str, str]] = []
    bootstrap_rows: list[dict[str, str]] = []
    path = clone_root / "embedded/sql/catalog.go"
    if not path.exists():
        return {
            "inventory_rows": [],
            "column_rows": [],
            "bootstrap_rows": [],
            "summary_lines": ["- immudb catalog bootstrap source file was not present in the local donor clone."],
        }
    text = path.read_text(encoding="utf-8")
    for block in re.finditer(r'name:\s*"([^"]+)",\s*cols:\s*\[\]\*Column\s*\{(.*?)\},\s*\n\s*\}', text, re.S):
        table_name = block.group(1)
        body = block.group(2)
        columns = []
        for col in re.finditer(r'colName:\s*"([^"]+)",\s*colType:\s*([A-Za-z0-9_]+)', body):
            columns.append((col.group(1), col.group(2)))
        inventory_rows.append(
            {
                "catalog_scope": "sql_catalog",
                "object_name": table_name,
                "object_kind": "system_table",
                "definition_source": rel(path),
                "field_count": str(len(columns)),
                "bootstrap_mode": "constructor_bootstrap",
                "notes": "Table declared in embedded SQL catalog constructor",
            }
        )
        for ordinal, (column_name, column_type) in enumerate(columns, start=1):
            column_rows.append(
                {
                    "object_name": table_name,
                    "ordinal": str(ordinal),
                    "column_name": column_name,
                    "column_type": column_type,
                    "column_source": rel(path),
                }
            )
        bootstrap_rows.append(
            {
                "object_name": table_name,
                "entry_source": rel(path),
                "evidence_kind": "constructor_table_registration",
                "entry_value": "declared",
                "note": "Table exists when a new SQL catalog is created; row population is runtime managed",
            }
        )
    return {
        "inventory_rows": inventory_rows,
        "column_rows": column_rows,
        "bootstrap_rows": bootstrap_rows,
        "summary_lines": [
            f"- Parsed `{len(inventory_rows)}` immudb SQL catalog tables from `embedded/sql/catalog.go`.",
            "- Bootstrap rows are constructor-declared; donor source does not provide a separate static insert script for initial contents.",
        ],
    }


def extract_yugabytedb_catalog(clone_root: Path) -> dict[str, object]:
    inventory_rows: list[dict[str, str]] = []
    column_rows: list[dict[str, str]] = []
    bootstrap_rows: list[dict[str, str]] = []
    catalog_dir = clone_root / "src/postgres/src/include/catalog"

    for header in sorted(catalog_dir.glob("pg_*.h")):
        text = header.read_text(encoding="utf-8")
        match = re.search(rf"CATALOG\(({header.stem}),[^\)]*\)\s*([^\{{]*)\{{(.*?)\n\}}", text, re.S)
        if not match:
            continue
        flags = " ".join(match.group(2).split())
        body = match.group(3)
        columns = []
        for field_match in re.finditer(
            r"^\s*([A-Za-z0-9_]+)\s+([A-Za-z0-9_]+)(?:\[[^\]]+\])?(?:\s+BKI_[^;]+)?;",
            body,
            re.M,
        ):
            columns.append((field_match.group(2), field_match.group(1)))
        dat_path = catalog_dir / f"{header.stem}.dat"
        has_dat = dat_path.exists()
        dat_rows = count_pg_dat_rows(dat_path) if has_dat else 0
        inventory_rows.append(
            {
                "catalog_scope": "pg_catalog",
                "object_name": header.stem,
                "object_kind": "system_catalog",
                "definition_source": rel(header),
                "field_count": str(len(columns)),
                "bootstrap_mode": "static_dat_rows" if has_dat else "runtime_or_empty",
                "notes": flags,
            }
        )
        for ordinal, (column_name, column_type) in enumerate(columns, start=1):
            column_rows.append(
                {
                    "object_name": header.stem,
                    "ordinal": str(ordinal),
                    "column_name": column_name,
                    "column_type": column_type,
                    "column_source": rel(header),
                }
            )
        bootstrap_rows.append(
            {
                "object_name": header.stem,
                "entry_source": rel(dat_path) if has_dat else rel(header),
                "evidence_kind": "bootstrap_dat_row_count" if has_dat else "no_static_dat_rows",
                "entry_value": str(dat_rows) if has_dat else "",
                "note": "Counted from embedded PostgreSQL .dat catalog rows" if has_dat else "Catalog has no matching .dat bootstrap file in this checkout",
            }
        )
    return {
        "inventory_rows": inventory_rows,
        "column_rows": column_rows,
        "bootstrap_rows": bootstrap_rows,
        "summary_lines": [
            f"- Parsed `{len(inventory_rows)}` YugabyteDB embedded PostgreSQL catalog headers from `src/postgres/src/include/catalog`.",
            "- Matching `.dat` files are counted as static bootstrap row sources; catalogs without `.dat` files are runtime-managed or initially empty.",
        ],
    }


def extract_dolt_catalog(clone_root: Path) -> dict[str, object]:
    inventory_rows: list[dict[str, str]] = []
    column_rows: list[dict[str, str]] = []
    bootstrap_rows: list[dict[str, str]] = []
    system_table_path = clone_root / "go/libraries/doltcore/doltdb/system_table.go"
    dtables_dir = clone_root / "go/libraries/doltcore/sqle/dtables"
    visible_paths = [
        clone_root / "integration-tests/bats/system-tables.bats",
        clone_root / "integration-tests/bats/ls.bats",
    ]
    if not system_table_path.exists():
        return {
            "inventory_rows": [],
            "column_rows": [],
            "bootstrap_rows": [],
            "summary_lines": ["- Dolt system-table registry source was not present in the local donor clone."],
        }

    system_text = system_table_path.read_text(encoding="utf-8")
    system_names = sorted(
        {
            match.group(2)
            for match in re.finditer(r"([A-Za-z0-9_]+Name)\s*=\s*\"(dolt_[a-z0-9_]+)\"", system_text)
        }
    )
    visible_tokens: set[str] = set()
    for path in visible_paths:
        if path.exists():
            visible_tokens.update(re.findall(r"\bdolt_[a-z0-9_]+\b", path.read_text(encoding="utf-8")))

    dtables_files = list(dtables_dir.glob("*.go")) if dtables_dir.exists() else []
    for name in system_names:
        candidate_files = []
        for path in dtables_files:
            text = path.read_text(encoding="utf-8")
            if name in text:
                candidate_files.append((path, text))
        if candidate_files:
            definition_path, definition_text = candidate_files[0]
        else:
            definition_path, definition_text = system_table_path, system_text

        columns = []
        if candidate_files:
            for column_name in re.findall(r'Name:\s*"([^"]+)"', definition_text):
                if column_name not in {col[0] for col in columns}:
                    columns.append((column_name, "sql.Column"))

        inventory_rows.append(
            {
                "catalog_scope": "dolt_system_tables",
                "object_name": name,
                "object_kind": "system_table",
                "definition_source": rel(definition_path),
                "field_count": str(len(columns)) if columns else "",
                "bootstrap_mode": "generated_system_table_constant",
                "notes": "System table name is declared in Dolt source; visibility and row population are repository-state aware",
            }
        )
        for ordinal, (column_name, column_type) in enumerate(columns, start=1):
            column_rows.append(
                {
                    "object_name": name,
                    "ordinal": str(ordinal),
                    "column_name": column_name,
                    "column_type": column_type,
                    "column_source": rel(definition_path),
                }
            )
        bootstrap_rows.append(
            {
                "object_name": name,
                "entry_source": rel(system_table_path),
                "evidence_kind": "system_table_name_constant",
                "entry_value": "declared",
                "note": "Visible in empty-db system-table tests"
                if name in visible_tokens
                else "Declared system surface; visibility may depend on repository state or table-specific families",
            }
        )

    return {
        "inventory_rows": inventory_rows,
        "column_rows": column_rows,
        "bootstrap_rows": bootstrap_rows,
        "summary_lines": [
            f"- Parsed `{len(inventory_rows)}` Dolt system-table name constants from `go/libraries/doltcore/doltdb/system_table.go`.",
            "- Empty-database visibility is marked only when local integration tests explicitly assert the table name in system-table listings.",
        ],
    }


def extract_foundationdb_catalog(clone_root: Path) -> dict[str, object]:
    inventory_rows: list[dict[str, str]] = []
    bootstrap_rows: list[dict[str, str]] = []
    system_cpp = clone_root / "fdbclient/SystemData.cpp"
    special_header = clone_root / "fdbclient/include/fdbclient/SpecialKeySpace.h"

    range_count = 0
    if system_cpp.exists():
        text = system_cpp.read_text(encoding="utf-8")
        for match in re.finditer(r"const\s+KeyRangeRef\s+([A-Za-z0-9_]+)\s*\(", text):
            name = match.group(1)
            inventory_rows.append(
                {
                    "catalog_scope": "system_keyspace",
                    "object_name": name,
                    "object_kind": "system_key_range",
                    "definition_source": rel(system_cpp),
                    "field_count": "",
                    "bootstrap_mode": "constant_key_range_declaration",
                    "notes": "FoundationDB system or special key range constant",
                }
            )
            bootstrap_rows.append(
                {
                    "object_name": name,
                    "entry_source": rel(system_cpp),
                    "evidence_kind": "key_range_constant",
                    "entry_value": "declared",
                    "note": "Fresh clusters expose this system key range as part of the key-space contract",
                }
            )
            range_count += 1

    module_count = 0
    if special_header.exists():
        text = special_header.read_text(encoding="utf-8")
        for module in extract_enum_members(text, "enum class MODULE", {"INVALID"}):
            inventory_rows.append(
                {
                    "catalog_scope": "special_key_space",
                    "object_name": module,
                    "object_kind": "special_key_module",
                    "definition_source": rel(special_header),
                    "field_count": "",
                    "bootstrap_mode": "special_key_module_enum",
                    "notes": "SpecialKeySpace module enum entry",
                }
            )
            bootstrap_rows.append(
                {
                    "object_name": module,
                    "entry_source": rel(special_header),
                    "evidence_kind": "special_key_module_enum",
                    "entry_value": "declared",
                    "note": "Special-key-space module is registered via client runtime code rather than SQL catalog bootstrap",
                }
            )
            module_count += 1

    return {
        "inventory_rows": inventory_rows,
        "column_rows": [],
        "bootstrap_rows": bootstrap_rows,
        "summary_lines": [
            f"- Parsed `{range_count}` FoundationDB system key-range constants from `fdbclient/SystemData.cpp`.",
            f"- Parsed `{module_count}` special-key-space modules from `fdbclient/include/fdbclient/SpecialKeySpace.h`.",
            "- FoundationDB does not expose a SQL system-catalog bootstrap in this donor clone; this packet records the authoritative key-space and module surfaces instead.",
        ],
    }


def extract_vitess_catalog(clone_root: Path) -> dict[str, object]:
    sidecar_name_path = clone_root / "go/constants/sidecar/name.go"
    default_schema = "_vt"
    if sidecar_name_path.exists():
        text = sidecar_name_path.read_text(encoding="utf-8")
        match = re.search(r'DefaultName\s*=\s*"([^"]+)"', text)
        if match:
            default_schema = match.group(1)

    schema_dir = clone_root / "go/vt/sidecardb/schema"
    sql_paths = sorted(schema_dir.rglob("*.sql")) if schema_dir.exists() else []
    inventory_rows, column_rows, bootstrap_rows = extract_sql_catalog_objects(
        sql_paths,
        catalog_scope="vitess_sidecar",
        default_schema=default_schema,
        bootstrap_mode="sidecar_bootstrap_sql",
        note="Vitess sidecar schema SQL",
    )
    return {
        "inventory_rows": inventory_rows,
        "column_rows": column_rows,
        "bootstrap_rows": bootstrap_rows,
        "summary_lines": [
            f"- Parsed `{len(inventory_rows)}` Vitess sidecar objects from `go/vt/sidecardb/schema/*.sql`.",
            f"- The sidecar schema name defaults to `{default_schema}` in `go/constants/sidecar/name.go`.",
        ],
    }


def extract_xtdb_catalog(clone_root: Path) -> dict[str, object]:
    inventory_rows: list[dict[str, str]] = []
    column_rows: list[dict[str, str]] = []
    bootstrap_rows: list[dict[str, str]] = []
    path = clone_root / "core/src/main/clojure/xtdb/information_schema.clj"
    if not path.exists():
        return {
            "inventory_rows": [],
            "column_rows": [],
            "bootstrap_rows": [],
            "summary_lines": ["- XTDB information-schema source file was not present in the local donor clone."],
        }

    text = path.read_text(encoding="utf-8")
    def_matches = list(re.finditer(r"\(def(?:\s+\^:private)?\s+([A-Za-z0-9_\-]+)", text))
    def_blocks: dict[str, str] = {}
    for idx, match in enumerate(def_matches):
        end = def_matches[idx + 1].start() if idx + 1 < len(def_matches) else len(text)
        def_blocks[match.group(1)] = text[match.start():end]

    for schema_name in ("xt", "pg_catalog", "information_schema", "public"):
        inventory_rows.append(
            {
                "catalog_scope": "default_schema_bootstrap",
                "object_name": schema_name,
                "object_kind": "system_schema",
                "definition_source": rel(path),
                "field_count": "",
                "bootstrap_mode": "default_schema_vector",
                "notes": "Default XTDB database schema name exposed by information_schema.clj",
            }
        )
        bootstrap_rows.append(
            {
                "object_name": schema_name,
                "entry_source": rel(path),
                "evidence_kind": "default_schema_name",
                "entry_value": "declared",
                "note": "Reported by the default schema vector in a newly created database",
            }
        )

    section_scopes = {
        "info-tables": "information_schema",
        "pg-catalog-tables": "pg_catalog",
        "pg-catalog-template-tables": "pg_catalog_template",
        "xt-derived-tables": "xt_runtime",
    }
    table_count = 0
    for section_name, catalog_scope in section_scopes.items():
        block = def_blocks.get(section_name, "")
        for table_match in re.finditer(r"([A-Za-z0-9_]+)/([A-Za-z0-9_]+)\s*\{(.*?)\}", block, re.S):
            object_name = f"{table_match.group(1)}.{table_match.group(2)}"
            body = table_match.group(3)
            columns = re.findall(r"([A-Za-z0-9_\-]+)\s+(\[[^\]]+\]|:[A-Za-z0-9_\-]+)", body)
            inventory_rows.append(
                {
                    "catalog_scope": catalog_scope,
                    "object_name": object_name,
                    "object_kind": "system_table",
                    "definition_source": rel(path),
                    "field_count": str(len(columns)),
                    "bootstrap_mode": "runtime_derived_catalog_map",
                    "notes": f"Declared in XTDB `{section_name}` map",
                }
            )
            for ordinal, (column_name, column_type) in enumerate(columns, start=1):
                column_rows.append(
                    {
                        "object_name": object_name,
                        "ordinal": str(ordinal),
                        "column_name": column_name,
                        "column_type": column_type,
                        "column_source": rel(path),
                    }
                )
            bootstrap_rows.append(
                {
                    "object_name": object_name,
                    "entry_source": rel(path),
                    "evidence_kind": "catalog_map_entry",
                    "entry_value": "declared",
                    "note": "System table or template is exposed through runtime-derived XTDB metadata maps",
                }
            )
            table_count += 1

    return {
        "inventory_rows": inventory_rows,
        "column_rows": column_rows,
        "bootstrap_rows": bootstrap_rows,
        "summary_lines": [
            f"- Parsed `{table_count}` XTDB internal and PostgreSQL-compatibility catalog tables from `core/src/main/clojure/xtdb/information_schema.clj`.",
            "- XTDB catalog contents are derived at runtime from the table and schema-info maps; the packet records the source-declared shapes rather than inventing static bootstrap rows.",
        ],
    }


def extract_tidb_catalog(clone_root: Path) -> dict[str, object]:
    inventory_rows: list[dict[str, str]] = []
    column_rows: list[dict[str, str]] = []
    bootstrap_rows: list[dict[str, str]] = []
    tables_path = clone_root / "pkg/infoschema/tables.go"
    if not tables_path.exists():
        return {
            "inventory_rows": [],
            "column_rows": [],
            "bootstrap_rows": [],
            "summary_lines": ["- TiDB infoschema tables source file was not present in the local donor clone."],
        }

    text = tables_path.read_text(encoding="utf-8")
    constant_values = {
        match.group(1): match.group(2)
        for match in re.finditer(r"([A-Za-z0-9_]+)\s*=\s*\"([^\"]+)\"", text)
    }
    column_sets: dict[str, list[tuple[str, str]]] = {}
    for match in re.finditer(r"var\s+([A-Za-z0-9_]+)\s*=\s*\[\]columnInfo\s*\{(.*?)\n\}", text, re.S):
        column_sets[match.group(1)] = re.findall(r'\{name:\s*"([^"]+)",\s*tp:\s*([A-Za-z0-9_.]+)', match.group(2))

    map_match = re.search(r"var\s+tableNameToColumns\s*=\s*map\[string\]\[\]columnInfo\s*\{(.*?)\n\}", text, re.S)
    if map_match:
        for entry_match in re.finditer(r"([A-Za-z0-9_]+)\s*:\s*([A-Za-z0-9_]+)\s*,", map_match.group(1)):
            table_token = entry_match.group(1)
            cols_token = entry_match.group(2)
            table_name = constant_values.get(table_token, table_token)
            columns = column_sets.get(cols_token, [])
            object_name = f"information_schema.{table_name}"
            inventory_rows.append(
                {
                    "catalog_scope": "infoschema_virtual_tables",
                    "object_name": object_name,
                    "object_kind": "virtual_system_table",
                    "definition_source": rel(tables_path),
                    "field_count": str(len(columns)),
                    "bootstrap_mode": "table_name_to_columns_registry",
                    "notes": "Registered in TiDB information-schema virtual table map",
                }
            )
            for ordinal, (column_name, column_type) in enumerate(columns, start=1):
                column_rows.append(
                    {
                        "object_name": object_name,
                        "ordinal": str(ordinal),
                        "column_name": column_name,
                        "column_type": column_type,
                        "column_source": rel(tables_path),
                    }
                )
            bootstrap_rows.append(
                {
                    "object_name": object_name,
                    "entry_source": rel(tables_path),
                    "evidence_kind": "virtual_table_registry_entry",
                    "entry_value": "declared",
                    "note": "Virtual table exists in a fresh TiDB deployment; row contents are populated at runtime",
                }
            )

    return {
        "inventory_rows": inventory_rows,
        "column_rows": column_rows,
        "bootstrap_rows": bootstrap_rows,
        "summary_lines": [
            f"- Parsed `{len(inventory_rows)}` TiDB information-schema virtual tables from `pkg/infoschema/tables.go`.",
            "- These are source-declared virtual table shapes; TiDB populates contents at runtime rather than via a static bootstrap insert script.",
        ],
    }


def extract_cockroachdb_catalog(clone_root: Path) -> dict[str, object]:
    systemschema_path = clone_root / "pkg/sql/catalog/systemschema/system.go"
    bootstrap_path = clone_root / "pkg/sql/catalog/bootstrap/metadata.go"
    inventory_rows, column_rows, bootstrap_rows = extract_sql_catalog_objects(
        [systemschema_path],
        catalog_scope="system",
        bootstrap_mode="systemschema_ddl_literals",
        note="CockroachDB system schema DDL literal",
    )
    if bootstrap_path.exists():
        descriptor_count = len(re.findall(r"AddDescriptor\(systemschema\.", bootstrap_path.read_text(encoding="utf-8")))
        bootstrap_rows.append(
            {
                "object_name": "system_bootstrap_descriptors",
                "entry_source": rel(bootstrap_path),
                "evidence_kind": "bootstrap_add_descriptor_calls",
                "entry_value": str(descriptor_count),
                "note": "Fresh cluster bootstrap registers system descriptors in metadata.go",
            }
        )
    return {
        "inventory_rows": inventory_rows,
        "column_rows": column_rows,
        "bootstrap_rows": bootstrap_rows,
        "summary_lines": [
            f"- Parsed `{len(inventory_rows)}` CockroachDB system schema objects from `pkg/sql/catalog/systemschema/system.go`.",
            "- Descriptor bootstrap is anchored by `pkg/sql/catalog/bootstrap/metadata.go` and is recorded separately in `catalog_bootstrap_evidence.csv`.",
        ],
    }


def extract_citus_catalog(clone_root: Path) -> dict[str, object]:
    inventory_rows: list[dict[str, str]] = []
    column_rows: list[dict[str, str]] = []
    bootstrap_rows: list[dict[str, str]] = []
    header_paths = sorted((clone_root / "src/include/distributed").glob("pg_dist*.h"))
    metadata_header = clone_root / "src/include/distributed/metadata/pg_dist_object.h"
    if metadata_header.exists():
        header_paths.append(metadata_header)

    for path in header_paths:
        text = path.read_text(encoding="utf-8")
        struct_match = re.search(r"typedef\s+struct\s+FormData_[A-Za-z0-9_]+\s*\{(.*?)\}\s*FormData_", text, re.S)
        if not struct_match:
            continue
        columns = [
            (match.group(2), match.group(1))
            for match in re.finditer(r"^\s*([A-Za-z0-9_]+)\s+([A-Za-z0-9_]+)\s*;", struct_match.group(1), re.M)
        ]
        natts_match = re.search(rf"#define\s+Natts_{re.escape(path.stem)}\s+(\d+)", text)
        inventory_rows.append(
            {
                "catalog_scope": "citus_extension_catalog",
                "object_name": path.stem,
                "object_kind": "extension_catalog_table",
                "definition_source": rel(path),
                "field_count": natts_match.group(1) if natts_match else str(len(columns)),
                "bootstrap_mode": "extension_header_definition",
                "notes": "Citus distributed metadata catalog header",
            }
        )
        for ordinal, (column_name, column_type) in enumerate(columns, start=1):
            column_rows.append(
                {
                    "object_name": path.stem,
                    "ordinal": str(ordinal),
                    "column_name": column_name,
                    "column_type": column_type,
                    "column_source": rel(path),
                }
            )
        bootstrap_rows.append(
            {
                "object_name": path.stem,
                "entry_source": rel(path),
                "evidence_kind": "extension_catalog_header",
                "entry_value": "declared",
                "note": "Citus extension metadata table definition; row population occurs when the extension is installed and used",
            }
        )

    return {
        "inventory_rows": inventory_rows,
        "column_rows": column_rows,
        "bootstrap_rows": bootstrap_rows,
        "summary_lines": [
            f"- Parsed `{len(inventory_rows)}` Citus distributed metadata catalog headers from `src/include/distributed`.",
            "- The local Citus clone is an extension overlay; these catalogs are extension-managed metadata surfaces layered on top of PostgreSQL.",
        ],
    }


def extract_apache_ignite_catalog(clone_root: Path) -> dict[str, object]:
    inventory_rows: list[dict[str, str]] = []
    bootstrap_rows: list[dict[str, str]] = []
    test_path = clone_root / "modules/indexing/src/test/java/org/apache/ignite/internal/processors/query/SqlSystemViewsSelfTest.java"
    if not test_path.exists():
        return {
            "inventory_rows": [],
            "column_rows": [],
            "bootstrap_rows": [],
            "summary_lines": ["- Apache Ignite SQL system-view test source was not present in the local donor clone."],
        }

    text = test_path.read_text(encoding="utf-8")
    view_names = sorted(set(re.findall(r'(?:systemSchemaName\(\)\s*\+\s*"\.|SYS\.)([A-Z0-9_]+)', text)))
    for name in view_names:
        inventory_rows.append(
            {
                "catalog_scope": "SYS",
                "object_name": f"SYS.{name}",
                "object_kind": "system_view",
                "definition_source": rel(test_path),
                "field_count": "",
                "bootstrap_mode": "runtime_registered_system_view",
                "notes": "Observed in Apache Ignite SQL system-view tests",
            }
        )
        bootstrap_rows.append(
            {
                "object_name": f"SYS.{name}",
                "entry_source": rel(test_path),
                "evidence_kind": "system_view_query_reference",
                "entry_value": "referenced",
                "note": "System view is registered by runtime schema managers and queried through the SYS schema",
            }
        )

    return {
        "inventory_rows": inventory_rows,
        "column_rows": [],
        "bootstrap_rows": bootstrap_rows,
        "summary_lines": [
            f"- Parsed `{len(inventory_rows)}` Apache Ignite SYS schema views from `SqlSystemViewsSelfTest.java` query references.",
            "- Ignite system views are registered programmatically; this packet records the SQL-visible view names proven in donor tests rather than inventing a static bootstrap table script.",
        ],
    }


def extract_catalog_reference(engine: dict, clone_root: Path) -> dict[str, object]:
    if engine["id"] == "mysql":
        return extract_mysql_like_catalog(
            clone_root,
            ["scripts/mysql_system_tables.sql"],
            ["scripts/mysql_system_tables.sql", "scripts/mysql_system_tables_data.sql"],
        )
    if engine["id"] == "mariadb":
        return extract_mysql_like_catalog(
            clone_root,
            ["scripts/mariadb_system_tables.sql"],
            ["scripts/mariadb_system_tables.sql", "scripts/mariadb_system_tables_data.sql"],
        )
    if engine["id"] == "postgresql":
        return extract_postgresql_catalog(clone_root)
    if engine["id"] == "firebird":
        return extract_firebird_catalog(clone_root)
    if engine["id"] == "cassandra":
        return extract_cassandra_catalog(clone_root)
    if engine["id"] == "clickhouse":
        return extract_clickhouse_catalog(clone_root)
    if engine["id"] == "duckdb":
        return extract_duckdb_catalog(clone_root)
    if engine["id"] == "influxdb":
        return extract_influxdb_catalog(clone_root)
    if engine["id"] == "mongodb":
        return extract_mongodb_catalog(clone_root)
    if engine["id"] == "redis":
        return extract_redis_catalog(clone_root)
    if engine["id"] == "opensearch":
        return extract_opensearch_catalog(clone_root)
    if engine["id"] == "neo4j":
        return extract_neo4j_catalog(clone_root)
    if engine["id"] == "milvus":
        return extract_milvus_catalog(clone_root)
    if engine["id"] == "dolt":
        return extract_dolt_catalog(clone_root)
    if engine["id"] == "foundationdb":
        return extract_foundationdb_catalog(clone_root)
    if engine["id"] == "vitess":
        return extract_vitess_catalog(clone_root)
    if engine["id"] == "immudb":
        return extract_immudb_catalog(clone_root)
    if engine["id"] == "xtdb":
        return extract_xtdb_catalog(clone_root)
    if engine["id"] == "tidb":
        return extract_tidb_catalog(clone_root)
    if engine["id"] == "cockroachdb":
        return extract_cockroachdb_catalog(clone_root)
    if engine["id"] == "yugabytedb":
        return extract_yugabytedb_catalog(clone_root)
    if engine["id"] == "citus":
        return extract_citus_catalog(clone_root)
    if engine["id"] == "apache_ignite":
        return extract_apache_ignite_catalog(clone_root)
    return {
        "inventory_rows": [
            {
                "catalog_scope": "runtime",
                "object_name": "catalog_surface",
                "object_kind": "authority_root",
                "definition_source": "; ".join(engine["donor_sources"]["catalogs_bootstrap"]),
                "field_count": "",
                "bootstrap_mode": "manual_review_required",
                "notes": "No engine-specific extractor implemented in generator",
            }
        ],
        "column_rows": [],
        "bootstrap_rows": [
            {
                "object_name": "catalog_surface",
                "entry_source": "; ".join(engine["donor_sources"]["catalogs_bootstrap"]),
                "evidence_kind": "authority_paths_only",
                "entry_value": "",
                "note": "Manual catalog deepening still required",
            }
        ],
        "summary_lines": [
            "- No engine-specific static catalog extractor is implemented for this donor yet.",
        ],
    }


def write_csv(path: Path, fieldnames: list[str], rows: list[dict[str, str]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as fh:
        writer = csv.DictWriter(fh, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def count_files(path: Path) -> int:
    if path.is_file():
        return 1
    if not path.exists():
        return 0
    return sum(1 for p in path.rglob("*") if p.is_file())


def matching_paths(base: Path, entries: list[str]) -> list[dict[str, str]]:
    rows = []
    seen: set[str] = set()
    for item in entries:
        if item in seen:
            continue
        seen.add(item)
        target = base / item
        rows.append(
            {
                "path": item,
                "exists": "yes" if target.exists() else "no",
                "kind": "file" if target.is_file() else ("dir" if target.is_dir() else "missing"),
                "file_count": str(count_files(target)),
            }
        )
    return rows


def sb_section_paths(section: str, engine: dict) -> list[str]:
    paths = list(SB_SHARED_FILES[section])
    if section == "parser_ast":
        token = engine["id"]
        extra = {
            "firebird": [
                "src/parser/firebird/firebird_parser.cpp",
                "src/parser/firebird/firebird_lexer.cpp",
                "src/ipc/external_agents/firebird_parser_agent.cpp",
                "src/sblr/firebird_query_compiler.cpp",
            ],
            "postgresql": [
                "src/parser/postgresql/pg_parser.cpp",
                "src/parser/postgresql/pg_parser_dml.cpp",
                "src/parser/postgresql/pg_parser_ddl.cpp",
                "src/parser/postgresql/pg_parser_expr.cpp",
                "src/parser/postgresql/pg_parser_misc.cpp",
                "src/ipc/external_agents/postgresql_parser_agent.cpp",
                "src/sblr/postgresql_query_compiler.cpp",
            ],
            "mysql": [
                "src/parser/mysql/mysql_parser.cpp",
                "src/parser/mysql/mysql_lexer.cpp",
                "src/ipc/external_agents/mysql_parser_agent.cpp",
                "src/sblr/mysql_query_compiler.cpp",
            ],
        }
        paths.extend(extra.get(token, []))
    if section == "wire_protocol":
        extra = {
            "firebird": ["src/protocol/adapters/firebird_adapter.cpp"],
            "postgresql": ["src/protocol/adapters/postgresql_adapter.cpp"],
            "mysql": ["src/protocol/adapters/mysql_adapter.cpp"],
        }
        paths.extend(extra.get(engine["id"], []))
    if section == "client_bridge":
        extra = {
            "firebird": ["src/fdw/firebird_adapter.cpp"],
            "postgresql": ["src/fdw/postgresql_adapter.cpp"],
            "mysql": ["src/fdw/mysql_adapter.cpp"],
        }
        paths.extend(extra.get(engine["id"], []))
    if section == "catalogs_bootstrap":
        catalog_map = {
            "firebird": "src/catalog/firebird_catalog.cpp",
            "cassandra": "src/catalog/cassandra_catalog.cpp",
            "clickhouse": "src/catalog/clickhouse_catalog.cpp",
            "duckdb": "src/catalog/duckdb_catalog.cpp",
            "influxdb": "src/catalog/influxdb_catalog.cpp",
            "mongodb": "src/catalog/mongodb_catalog.cpp",
            "neo4j": "src/catalog/neo4j_catalog.cpp",
            "opensearch": "src/catalog/opensearch_catalog.cpp",
            "redis": "src/catalog/redis_catalog.cpp",
            "milvus": "src/catalog/milvus_catalog.cpp",
            "mariadb": "include/scratchbird/catalog/mariadb_catalog.h",
            "postgresql": "include/scratchbird/catalog/pg_catalog.h",
            "mysql": "include/scratchbird/catalog/mysql_catalog.h",
        }
        if engine["id"] in catalog_map:
            paths.append(catalog_map[engine["id"]])
    return paths


def detect_sb_surfaces(engine: dict, bootstrap_paths: list[str], type_rows: list[dict[str, str]]) -> dict[str, str]:
    def exists(rel_path: str) -> str:
        return "yes" if (ROOT / rel_path).exists() else "no"

    profile = engine["scratchbird_profile"]
    token = engine["token"]
    emitter = (ROOT / "src/parser/v3_emitter.cpp").read_text(encoding="utf-8")
    parser_v3 = (ROOT / "src/parser/parser_v3.cpp").read_text(encoding="utf-8")
    scaffold = (ROOT / "src/core/emulation_package_scaffold.cpp").read_text(encoding="utf-8")
    manifest = (ROOT / "src/core/emulation_package_manifest.cpp").read_text(encoding="utf-8")

    matches = []
    for probe in {
        token,
        profile,
        f"nosql.{token}",
        f"remote.emulation.{token}",
        f"emulation.{token}",
    }:
        if probe and (probe in emitter or probe in parser_v3):
            matches.append(probe)

    type_count = sum(1 for row in type_rows if row["engine_name"] == engine["type_key"])
    bootstrap_hits = [p for p in bootstrap_paths if token in p]

    return {
        "type_rows": str(type_count),
        "protocol_adapter": "yes"
        if engine["id"] in {"firebird", "postgresql", "mysql"}
        and (ROOT / f"src/protocol/adapters/{token}_adapter.cpp").exists()
        else "no",
        "parser_agent": "yes"
        if engine["id"] in {"firebird", "postgresql", "mysql"}
        and (ROOT / f"src/ipc/external_agents/{token}_parser_agent.cpp").exists()
        else "no",
        "query_compiler": "yes"
        if engine["id"] in {"firebird", "postgresql", "mysql"}
        and (ROOT / f"src/sblr/{token}_query_compiler.cpp").exists()
        else "no",
        "fdw_adapter": "yes"
        if engine["id"] in {"firebird", "postgresql", "mysql"}
        and (ROOT / f"src/fdw/{token}_adapter.cpp").exists()
        else "no",
        "catalog_overlay": "yes"
        if any((ROOT / path).exists() for path in sb_section_paths("catalogs_bootstrap", engine))
        else "no",
        "compat_suite": "yes"
        if (ROOT / f"tests/compatibility/{token}").exists()
        else "no",
        "bootstrap_paths": "; ".join(bootstrap_hits),
        "emulation_package_entry": "yes" if profile in scaffold or profile in manifest else "no",
        "v3_markers": "; ".join(sorted(set(matches))) if matches else "",
    }


def format_section_rows(rows: list[dict[str, str]]) -> list[str]:
    lines = []
    for row in rows:
        suffix = f" ({row['kind']}, files={row['file_count']})" if row["exists"] == "yes" else " (missing)"
        lines.append(f"- `{row['path']}`{suffix}")
    return lines


def write_engine_packet(
    engine: dict,
    type_rows: list[dict[str, str]],
    index_registry: dict[str, dict[str, str]],
    bootstrap_paths: list[str],
    gap_rows: list[dict[str, str]],
    manifest_rows: list[dict[str, str]],
) -> None:
    packet_dir = OUT_ROOT / engine["id"]
    packet_dir.mkdir(parents=True, exist_ok=True)
    clone_root = donor_clone_root(engine)
    catalog_reference = extract_catalog_reference(engine, clone_root)
    support_rows = build_engine_support_rows(
        engine=engine,
        type_rows=type_rows,
        native_types=parse_native_data_types(),
        index_registry_rows=list(index_registry.values()),
    )
    authority_rows, authority_summary_rows = build_section_authority_rows(engine)
    authority_summary_map = {row["section"]: row for row in authority_summary_rows}

    engine_type_rows = [row for row in type_rows if row["engine_name"] == engine["type_key"]]
    datatype_csv = packet_dir / "datatype_matrix.csv"
    write_csv(
        datatype_csv,
        ["engine_name", "emulated_type", "storage_kind", "canonical_type", "domain_hint", "parser_rule_hint"],
        engine_type_rows,
    )

    index_rows = []
    for donor_surface, sb_index, compat, note in engine["index_rows"]:
        idx = index_registry.get(sb_index, {})
        index_rows.append(
            {
                "donor_engine": engine["display"],
                "donor_surface": donor_surface,
                "scratchbird_index_type": sb_index,
                "scratchbird_storage_model": idx.get("storage_model", ""),
                "scratchbird_runtime_class": idx.get("runtime_class", ""),
                "compatibility_class": compat,
                "notes": note,
            }
        )
    index_csv = packet_dir / "index_matrix.csv"
    write_csv(
        index_csv,
        [
            "donor_engine",
            "donor_surface",
            "scratchbird_index_type",
            "scratchbird_storage_model",
            "scratchbird_runtime_class",
            "compatibility_class",
            "notes",
        ],
        index_rows,
    )

    write_csv(
        packet_dir / "donor_datatype_inventory.csv",
        ["engine", "engine_id", "surface_class", "surface_kind", "donor_surface", "donor_surface_key", "evidence_path", "evidence_note"],
        support_rows["donor_datatypes"],
    )
    write_csv(
        packet_dir / "donor_index_inventory.csv",
        ["engine", "engine_id", "surface_class", "surface_kind", "donor_surface", "donor_surface_key", "evidence_path", "evidence_note"],
        support_rows["donor_indexes"],
    )
    write_csv(
        packet_dir / "donor_vs_scratchbird_datatype_support.csv",
        [
            "engine",
            "engine_id",
            "surface_class",
            "surface_kind",
            "donor_surface",
            "donor_surface_key",
            "evidence_path",
            "evidence_note",
            "sb_engine_mapping_present",
            "sb_native_exact_name_match",
            "sb_native_exact_name",
            "support_path",
            "support_path_basis",
        ],
        support_rows["datatype_support_rows"],
    )
    write_csv(
        packet_dir / "donor_vs_scratchbird_index_support.csv",
        [
            "engine",
            "engine_id",
            "surface_class",
            "surface_kind",
            "donor_surface",
            "donor_surface_key",
            "evidence_path",
            "evidence_note",
            "sb_engine_mapping_present",
            "sb_direct_index_family_match",
            "sb_direct_index_family",
        ],
        support_rows["index_support_rows"],
    )
    write_csv(
        packet_dir / "source_authority_matrix.csv",
        ["section", "side", "path", "exists", "kind", "file_count"],
        authority_rows,
    )
    write_csv(
        packet_dir / "source_authority_summary.csv",
        [
            "section",
            "donor_paths_present",
            "donor_paths_total",
            "donor_file_total",
            "scratchbird_paths_present",
            "scratchbird_paths_total",
            "scratchbird_file_total",
        ],
        authority_summary_rows,
    )

    catalog_inventory_csv = packet_dir / "catalog_structure_inventory.csv"
    write_csv(
        catalog_inventory_csv,
        [
            "catalog_scope",
            "object_name",
            "object_kind",
            "definition_source",
            "field_count",
            "bootstrap_mode",
            "notes",
        ],
        catalog_reference["inventory_rows"],
    )

    catalog_columns_csv = packet_dir / "catalog_columns.csv"
    write_csv(
        catalog_columns_csv,
        ["object_name", "ordinal", "column_name", "column_type", "column_source"],
        catalog_reference["column_rows"],
    )

    catalog_bootstrap_csv = packet_dir / "catalog_bootstrap_evidence.csv"
    write_csv(
        catalog_bootstrap_csv,
        ["object_name", "entry_source", "evidence_kind", "entry_value", "note"],
        catalog_reference["bootstrap_rows"],
    )

    sb_surface = detect_sb_surfaces(engine, bootstrap_paths, type_rows)
    for row in authority_rows:
        if row["exists"] == "no":
            gap_rows.append(
                {
                    "engine": engine["display"],
                    "section": row["section"],
                    "source_kind": row["side"],
                    "detail": row["path"],
                    "status": "missing_local_donor_path" if row["side"] == "donor" else "missing_local_sb_path",
                }
            )

    if sb_surface["protocol_adapter"] == "no":
        gap_rows.append(
            {
                "engine": engine["display"],
                "section": "wire_protocol",
                "source_kind": "scratchbird",
                "detail": "No dedicated ScratchBird protocol adapter file detected",
                "status": "no_current_local_implementation_evidence",
            }
        )
    if sb_surface["parser_agent"] == "no":
        gap_rows.append(
            {
                "engine": engine["display"],
                "section": "parser_ast",
                "source_kind": "scratchbird",
                "detail": "No dedicated ScratchBird external parser agent detected",
                "status": "no_current_local_implementation_evidence",
            }
        )
    if sb_surface["fdw_adapter"] == "no":
        gap_rows.append(
            {
                "engine": engine["display"],
                "section": "client_bridge",
                "source_kind": "scratchbird",
                "detail": "No dedicated ScratchBird FDW adapter detected",
                "status": "no_current_local_implementation_evidence",
            }
        )
    if not engine["index_rows"]:
        gap_rows.append(
            {
                "engine": engine["display"],
                "section": "indexes",
                "source_kind": "comparison",
                "detail": "No source-backed 1:1 index rows emitted in this packet",
                "status": "comparison_needs_manual_deepening",
            }
        )

    def section_snapshot(section: str) -> str:
        row = authority_summary_map.get(section)
        if not row:
            return "No authority rows were emitted for this section."
        return (
            f"Donor `{row['donor_paths_present']}/{row['donor_paths_total']}` paths "
            f"({row['donor_file_total']} files); ScratchBird `{row['scratchbird_paths_present']}/{row['scratchbird_paths_total']}` paths "
            f"({row['scratchbird_file_total']} files)"
        )

    catalog_reference_md = [
        f"# {engine['display']} Catalog, System Surface, and New-Database Bootstrap Reference",
        "",
        "This document is generated from local donor and ScratchBird source files only.",
        "It records statically extractable catalog/system structures and explicitly separates runtime-populated surfaces from static bootstrap rows.",
        "",
        "## Summary",
        *catalog_reference["summary_lines"],
        "",
        "## Generated assets",
        "- `catalog_structure_inventory.csv`",
        "- `catalog_columns.csv`",
        "- `catalog_bootstrap_evidence.csv`",
        "",
        "## ScratchBird authorities",
    ]
    for row in matching_paths(ROOT, sb_section_paths("catalogs_bootstrap", engine)):
        suffix = f" ({row['kind']}, files={row['file_count']})" if row["exists"] == "yes" else " (missing)"
        catalog_reference_md.append(f"- `{row['path']}`{suffix}")
    catalog_reference_md.extend(
        [
            "",
            "## Donor authorities",
        ]
    )
    for row in matching_paths(clone_root, engine["donor_sources"]["catalogs_bootstrap"]):
        suffix = f" ({row['kind']}, files={row['file_count']})" if row["exists"] == "yes" else " (missing)"
        catalog_reference_md.append(f"- `{row['path']}`{suffix}")
    catalog_reference_md.extend(
        [
            "",
            "## Notes",
            "- `catalog_structure_inventory.csv` records the donor catalog object or metadata surface names visible from source.",
            "- `catalog_columns.csv` is populated only when the donor source encodes the column layout statically enough to extract without execution.",
            "- `catalog_bootstrap_evidence.csv` records either static row sources or the exact runtime/bootstrap code paths when the donor materializes metadata procedurally.",
        ]
    )
    (packet_dir / "catalog_system_bootstrap_reference.md").write_text(
        "\n".join(catalog_reference_md) + "\n",
        encoding="utf-8",
    )

    packet_scope_lines = []
    if engine.get("packet_scope_note"):
        packet_scope_lines.append(f"- {engine['packet_scope_note']}")
    else:
        packet_scope_lines.append(
            "- This packet records only donor and ScratchBird surfaces that could be tied to local source in the current checkout."
        )
    if engine.get("family_reference_packet"):
        packet_scope_lines.append(
            f"- Shared-family baseline packet: `../{engine['family_reference_packet']}/README.md`"
        )

    readme = [
        f"# {engine['display']} 1:1 Emulation Reference Packet",
        "",
        "This packet is a local source-backed evidence map for the donor engine and the current ScratchBird implementation.",
        "It intentionally records only what could be tied to local source files in this checkout.",
        "",
        "## Packet Scope",
        *packet_scope_lines,
        "",
        "## Generated assets",
        f"- `datatype_matrix.csv`: SB emulation datatype rows filtered for `{engine['type_key']}`",
        f"- `index_matrix.csv`: source-backed donor index surfaces mapped to current SB index families",
        "- `donor_datatype_inventory.csv`: donor datatype or value-surface tokens extracted directly from donor source",
        "- `donor_index_inventory.csv`: donor index or access-method tokens extracted directly from donor source",
        "- `donor_vs_scratchbird_datatype_support.csv`: 1:1 donor datatype surfaces with current SB support-path classification",
        "- `donor_vs_scratchbird_index_support.csv`: 1:1 donor index surfaces with current SB native-family comparison",
        "- `source_authority_matrix.csv`: donor and ScratchBird source paths used for sections (a)-(k)",
        "- `source_authority_summary.csv`: per-section authority counts for donor and ScratchBird sides",
        "- `catalog_structure_inventory.csv`: donor catalog or system-surface inventory",
        "- `catalog_columns.csv`: extracted catalog column layouts where statically available",
        "- `catalog_bootstrap_evidence.csv`: bootstrap row or runtime-materialization evidence",
        "- `catalog_system_bootstrap_reference.md`: catalog/bootstrap interpretation notes",
        "",
        "## (a) Datatypes",
        f"- Donor datatype surfaces extracted: `{len(support_rows['donor_datatypes'])}`",
        f"- Current SB explicit family mapping rows: `{len(engine_type_rows)}`",
        "- Review `donor_vs_scratchbird_datatype_support.csv` first for exact donor token, evidence path, and support-path classification.",
        "- `datatype_matrix.csv` remains the direct SB code-derived matrix for the comparison family.",
        "",
        "## (b) Indexes",
        f"- Donor index surfaces extracted: `{len(support_rows['donor_indexes'])}`",
        f"- Current explicit comparison rows emitted in this packet: `{len(index_rows)}`",
        "- Review `donor_vs_scratchbird_index_support.csv` first for exact donor token, evidence path, and direct SB family match state.",
        "",
        "## (c) Parser to SB AST / V3 Dialect",
        f"- {section_snapshot('parser_ast')}",
        f"- ScratchBird dedicated parser agent present: `{sb_surface['parser_agent']}`",
        f"- Native V3 markers: `{sb_surface['v3_markers'] or 'none found'}`",
        "",
        "## (d) Wire Protocol",
        f"- {section_snapshot('wire_protocol')}",
        f"- ScratchBird dedicated donor protocol adapter present: `{sb_surface['protocol_adapter']}`",
        "",
        "## (e) Authentication",
        f"- {section_snapshot('authentication')}",
        "- Review `source_authority_matrix.csv` filtered to `authentication` for the exact donor and SB security source paths.",
        "",
        "## (f) Client Bridge / UDR Target Surface",
        f"- {section_snapshot('client_bridge')}",
        f"- ScratchBird dedicated FDW or bridge adapter present: `{sb_surface['fdw_adapter']}`",
        "",
        "## (g) Plan Layout / Optimizer Output",
        f"- {section_snapshot('plan_output')}",
        "- Plan-shape authorities are tracked as source paths only in this packet; no plan text is invented where local source does not encode a static plan fixture.",
        "",
        "## (h) Error Codes",
        f"- {section_snapshot('error_codes')}",
        "- Error-code mapping authority is path-based in this packet; use the listed donor and SB files to drive exact code-map extraction without inference.",
        "",
        "## (i) Page Types and Storage Optimizations",
        f"- {section_snapshot('page_optimizations')}",
        "- This section identifies donor storage or page-layout authority paths only; optimization conclusions belong in downstream audits, not this packet.",
        "",
        "## (j) Regression Tests and Tooling",
        f"- {section_snapshot('regression_tests')}",
        f"- ScratchBird compatibility suite present: `{sb_surface['compat_suite']}`",
        "",
        "## (k) Catalog / System Tables / New Empty Database",
        f"- {section_snapshot('catalogs_bootstrap')}",
        *catalog_reference["summary_lines"],
        f"- ScratchBird catalog overlay handler present: `{sb_surface['catalog_overlay']}`",
        "",
        "## Current ScratchBird Surface Status",
        f"- Emulation package manifest or scaffold entry present: `{sb_surface['emulation_package_entry']}`",
        f"- Dedicated donor query compiler present: `{sb_surface['query_compiler']}`",
        f"- Type matrix rows in SB code for this family: `{sb_surface['type_rows']}`",
        f"- Bootstrap schema paths referencing this engine: `{sb_surface['bootstrap_paths'] or 'none found'}`",
        "",
        "## Authority Notes",
        "- `source_authority_matrix.csv` is the exact path-level proof ledger for sections (a)-(k).",
        "- If a donor or SB path is absent locally, the packet records that absence instead of inferring support or completeness.",
        "",
        "## Packet notes",
        f"- Local donor clone root: `{rel(clone_root)}`",
        f"- ScratchBird profile token used for packet detection: `{engine['scratchbird_profile']}`",
        "- The datatype matrix is emitted directly from `src/core/type_system.cpp` and therefore reflects explicit code-defined donor-to-SB mapping rows.",
        "- Where no dedicated ScratchBird parser, protocol adapter, FDW bridge, or compatibility suite is present, this packet marks the surface as lacking current local implementation evidence instead of inferring support.",
    ]
    (packet_dir / "README.md").write_text("\n".join(readme) + "\n", encoding="utf-8")

    manifest_rows.append(
        {
            "engine": engine["display"],
            "packet_dir": rel(packet_dir),
            "datatype_rows": str(len(support_rows["datatype_support_rows"])),
            "index_rows": str(len(support_rows["index_support_rows"])),
            "protocol_adapter": sb_surface["protocol_adapter"],
            "parser_agent": sb_surface["parser_agent"],
            "fdw_adapter": sb_surface["fdw_adapter"],
            "catalog_overlay": sb_surface["catalog_overlay"],
            "compat_suite": sb_surface["compat_suite"],
        }
    )


def update_reference_index() -> None:
    index_text = REFERENCE_LIBRARY_INDEX.read_text(encoding="utf-8")
    entry = (
        "| `emulation_1_to_1_engine_reference_packets_2026-04-02/` | "
        "local source-backed donor-engine 1:1 emulation packet set | active |"
    )
    if entry not in index_text:
        index_text = index_text.rstrip() + "\n" + entry + "\n"
        REFERENCE_LIBRARY_INDEX.write_text(index_text, encoding="utf-8")

    readme_text = REFERENCE_LIBRARY_README.read_text(encoding="utf-8")
    needle = "- donor-engine implementation packets"
    addition = (
        "- `emulation_1_to_1_engine_reference_packets_2026-04-02/`: per-engine"
        " 1:1 source-backed emulation packets for the local donor clone set"
    )
    if addition not in readme_text:
        readme_text = readme_text.replace(needle, needle + "\n" + addition)
        REFERENCE_LIBRARY_README.write_text(readme_text, encoding="utf-8")


def main() -> None:
    OUT_ROOT.mkdir(parents=True, exist_ok=True)
    (OUT_ROOT / "shared").mkdir(parents=True, exist_ok=True)

    type_rows = parse_type_rows()
    native_types = parse_native_data_types()
    index_registry_rows = parse_index_registry()
    virtual_catalog_rows = parse_virtual_catalog_registry()
    bootstrap_paths = parse_bootstrap_paths()
    index_registry = {row["index_type"]: row for row in index_registry_rows}

    write_csv(
        OUT_ROOT / "shared/scratchbird_type_matrix.csv",
        ["engine_name", "emulated_type", "storage_kind", "canonical_type", "domain_hint", "parser_rule_hint"],
        type_rows,
    )
    write_csv(
        OUT_ROOT / "shared/scratchbird_index_registry.csv",
        [
            "index_type",
            "canonical_name",
            "storage_model",
            "runtime_class",
            "supports_build",
            "supports_insert",
            "supports_remove",
            "file_based",
            "supports_search",
            "supports_vector",
            "supports_summary",
            "supports_ordering",
        ],
        index_registry_rows,
    )
    write_csv(
        OUT_ROOT / "shared/scratchbird_virtual_catalog_registry.csv",
        ["emulation_engine", "protocol_type", "handler", "comment"],
        virtual_catalog_rows,
    )
    write_csv(
        OUT_ROOT / "shared/scratchbird_bootstrap_schema_paths.csv",
        ["schema_path"],
        [{"schema_path": p} for p in bootstrap_paths],
    )
    write_csv(
        OUT_ROOT / "shared/scratchbird_native_type_registry.csv",
        ["native_type"],
        [{"native_type": name} for name in native_types],
    )

    shared_readme = [
        "# Shared ScratchBird Evidence Assets",
        "",
        "- `scratchbird_type_matrix.csv`: parsed from `src/core/type_system.cpp`",
        "- `scratchbird_native_type_registry.csv`: parsed from `include/scratchbird/core/types.h`",
        "- `scratchbird_index_registry.csv`: parsed from `src/core/index_factory.cpp`",
        "- `scratchbird_virtual_catalog_registry.csv`: parsed from `src/catalog/virtual_catalog.cpp`",
        "- `scratchbird_bootstrap_schema_paths.csv`: parsed from `tests/unit/test_catalog_database_bootstrap.cpp`",
        "",
        "These files are generated from local source in the current checkout.",
    ]
    (OUT_ROOT / "shared/README.md").write_text("\n".join(shared_readme) + "\n", encoding="utf-8")

    gap_rows: list[dict[str, str]] = []
    manifest_rows: list[dict[str, str]] = []
    for engine in ENGINE_META:
        write_engine_packet(
            engine=engine,
            type_rows=type_rows,
            index_registry=index_registry,
            bootstrap_paths=bootstrap_paths,
            gap_rows=gap_rows,
            manifest_rows=manifest_rows,
        )

    missing_surface_reports = build_missing_surface_reports(
        type_rows=type_rows,
        index_registry_rows=index_registry_rows,
        native_types=native_types,
    )

    write_csv(
        OUT_ROOT / "EMULATION_REFERENCE_PACKET_MANIFEST.csv",
        [
            "engine",
            "packet_dir",
            "datatype_rows",
            "index_rows",
            "protocol_adapter",
            "parser_agent",
            "fdw_adapter",
            "catalog_overlay",
            "compat_suite",
        ],
        manifest_rows,
    )
    write_csv(
        OUT_ROOT / "EMULATION_REFERENCE_GAP_MANIFEST.csv",
        ["engine", "section", "source_kind", "detail", "status"],
        gap_rows,
    )
    write_csv(
        OUT_ROOT / "DONOR_DATATYPE_SURFACE_INVENTORY.csv",
        ["engine", "engine_id", "surface_class", "surface_kind", "donor_surface", "donor_surface_key", "evidence_path", "evidence_note"],
        missing_surface_reports["datatype_inventory"],
    )
    write_csv(
        OUT_ROOT / "DONOR_INDEX_SURFACE_INVENTORY.csv",
        ["engine", "engine_id", "surface_class", "surface_kind", "donor_surface", "donor_surface_key", "evidence_path", "evidence_note"],
        missing_surface_reports["index_inventory"],
    )
    write_csv(
        OUT_ROOT / "MISSING_DONOR_DATATYPE_SURFACES.csv",
        [
            "engine",
            "engine_id",
            "surface_class",
            "surface_kind",
            "donor_surface",
            "donor_surface_key",
            "evidence_path",
            "evidence_note",
            "sb_engine_mapping_present",
            "sb_native_exact_name_match",
            "sb_native_exact_name",
            "support_path",
            "support_path_basis",
            "missing_class",
        ],
        missing_surface_reports["missing_datatypes"],
    )
    write_csv(
        OUT_ROOT / "MISSING_DONOR_INDEX_SURFACES.csv",
        [
            "engine",
            "engine_id",
            "surface_class",
            "surface_kind",
            "donor_surface",
            "donor_surface_key",
            "evidence_path",
            "evidence_note",
            "sb_engine_mapping_present",
            "sb_direct_index_family_match",
            "sb_direct_index_family",
            "missing_class",
        ],
        missing_surface_reports["missing_indexes"],
    )
    write_csv(
        OUT_ROOT / "MISSING_DONOR_DATATYPE_TOKEN_ROLLUP.csv",
        ["surface_kind", "donor_surface_key", "sample_surface", "engines", "engine_count", "native_exact_name_match_any", "support_paths", "evidence_paths"],
        missing_surface_reports["datatype_rollup"],
    )
    write_csv(
        OUT_ROOT / "MISSING_DONOR_INDEX_TOKEN_ROLLUP.csv",
        ["surface_kind", "donor_surface_key", "sample_surface", "engines", "engine_count", "direct_family_match_any", "evidence_paths"],
        missing_surface_reports["index_rollup"],
    )
    write_csv(
        OUT_ROOT / "MISSING_DONOR_SURFACE_SUMMARY_BY_ENGINE.csv",
        [
            "engine",
            "donor_datatype_surfaces",
            "missing_datatype_surfaces",
            "datatype_support_native_direct",
            "datatype_support_domain_precedent_exists",
            "datatype_support_alias_or_catalog_name_only",
            "datatype_support_no_current_proof",
            "donor_index_surfaces",
            "missing_index_surfaces",
        ],
        missing_surface_reports["summary_rows"],
    )

    report_lines = [
        "# Missing Donor Datatypes and Index Types vs Current ScratchBird Code",
        "",
        "This report is generated from local donor-engine source and the current ScratchBird checkout only.",
        "A surface is marked missing when the donor token exists in donor source inventory but there is no current explicit ScratchBird engine-mapping row for that token.",
        "The datatype section now assigns a `support_path` classification using current ScratchBird code only: `NATIVE_DIRECT`, `DOMAIN_PRECEDENT_EXISTS`, `ALIAS_OR_CATALOG_NAME_ONLY`, or `NO_CURRENT_PROOF`.",
        "",
        "## Generated detail files",
        "- `DONOR_DATATYPE_SURFACE_INVENTORY.csv`",
        "- `DONOR_INDEX_SURFACE_INVENTORY.csv`",
        "- `MISSING_DONOR_DATATYPE_SURFACES.csv`",
        "- `MISSING_DONOR_INDEX_SURFACES.csv`",
        "- `MISSING_DONOR_DATATYPE_TOKEN_ROLLUP.csv`",
        "- `MISSING_DONOR_INDEX_TOKEN_ROLLUP.csv`",
        "- `MISSING_DONOR_SURFACE_SUMMARY_BY_ENGINE.csv`",
        "",
        "## Summary By Engine",
    ]
    for row in missing_surface_reports["summary_rows"]:
        report_lines.append(
            f"- `{row['engine']}`: donor datatypes `{row['donor_datatype_surfaces']}`, missing datatypes `{row['missing_datatype_surfaces']}`, "
            f"`NATIVE_DIRECT` `{row['datatype_support_native_direct']}`, `DOMAIN_PRECEDENT_EXISTS` `{row['datatype_support_domain_precedent_exists']}`, "
            f"`ALIAS_OR_CATALOG_NAME_ONLY` `{row['datatype_support_alias_or_catalog_name_only']}`, `NO_CURRENT_PROOF` `{row['datatype_support_no_current_proof']}`, "
            f"donor index surfaces `{row['donor_index_surfaces']}`, missing index surfaces `{row['missing_index_surfaces']}`"
        )
    report_lines.extend(
        [
            "",
            "## Missing Datatype Rollup",
        ]
    )
    if missing_surface_reports["datatype_rollup"]:
        for row in missing_surface_reports["datatype_rollup"]:
            report_lines.append(
                f"- `{row['sample_surface']}` [{row['surface_kind']}]: engines `{row['engines']}`; direct native-name match anywhere `{row['native_exact_name_match_any']}`; support paths `{row['support_paths']}`"
            )
    else:
        report_lines.append("- No missing donor datatype surfaces were emitted by the current extractor set.")
    report_lines.extend(
        [
            "",
            "## Missing Index Rollup",
        ]
    )
    if missing_surface_reports["index_rollup"]:
        for row in missing_surface_reports["index_rollup"]:
            report_lines.append(
                f"- `{row['sample_surface']}` [{row['surface_kind']}]: engines `{row['engines']}`; direct SB family-name match anywhere `{row['direct_family_match_any']}`"
            )
    else:
        report_lines.append("- No missing donor index surfaces were emitted by the current extractor set.")
    report_lines.extend(
        [
            "",
            "## Scope Notes",
            "- Datatype inventory mixes SQL type tokens, catalog typnames, protocol field types, field-mapper types, and value-codec enums when those are the donor’s authoritative source-declared surfaces.",
            "- Index inventory is limited to donor surfaces that could be tied to named source declarations in the local clones; engines without an extractable named index token set are intentionally left sparse rather than guessed.",
        ]
    )
    (OUT_ROOT / "MISSING_DATATYPES_AND_INDEX_TYPES_REPORT.md").write_text(
        "\n".join(report_lines) + "\n",
        encoding="utf-8",
    )

    readme = [
        "# Emulation 1:1 Engine Reference Packets",
        "",
        "This packet set covers the local donor-engine clone set under both `docs/reference/project_clones/local_existing/` and `docs/reference/project_clones/remote/`.",
        "It is a source-backed reference library for building 1:1 ScratchBird emulation surfaces.",
        "",
        "## Shared assets",
        "- `shared/README.md`",
        "- `EMULATION_REFERENCE_PACKET_MANIFEST.csv`",
        "- `EMULATION_REFERENCE_GAP_MANIFEST.csv`",
        "- `MISSING_DATATYPES_AND_INDEX_TYPES_REPORT.md`",
        "- `DONOR_DATATYPE_SURFACE_INVENTORY.csv`",
        "- `DONOR_INDEX_SURFACE_INVENTORY.csv`",
        "- `MISSING_DONOR_DATATYPE_SURFACES.csv`",
        "- `MISSING_DONOR_INDEX_SURFACES.csv`",
        "- `MISSING_DONOR_SURFACE_SUMMARY_BY_ENGINE.csv`",
        "- `MISSING_DONOR_DATATYPE_TOKEN_ROLLUP.csv`",
        "- `MISSING_DONOR_INDEX_TOKEN_ROLLUP.csv`",
        "",
        "## Per-engine packets",
    ]
    for engine in ENGINE_META:
        readme.append(f"- `{engine['id']}/README.md`")
    readme.extend(
        [
            "",
            "## Scope rules",
            "- Every packet is driven from local source files only.",
            "- If a local ScratchBird surface or donor authority path is absent, the packet records the gap instead of inferring support.",
            "- The generated datatype matrices come directly from ScratchBird code and therefore reflect explicit emulation rows already encoded in the checkout.",
        ]
    )
    (OUT_ROOT / "README.md").write_text("\n".join(readme) + "\n", encoding="utf-8")

    update_reference_index()


if __name__ == "__main__":
    main()
