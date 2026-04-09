#!/usr/bin/env python3
from __future__ import annotations

import csv
import re
from collections import defaultdict
from pathlib import Path


ROOT = Path("/home/dcalford/CliWork/ScratchBird")
PACKET_ROOT = (
    ROOT
    / "docs/reference/reference_library/emulation_1_to_1_engine_reference_packets_2026-04-02"
)
TYPE_SYSTEM = ROOT / "src/core/type_system.cpp"
CORE_TYPES = ROOT / "include/scratchbird/core/types.h"
INDEX_FACTORY = ROOT / "src/core/index_factory.cpp"


ENGINE_META = [
    {"id": "firebird", "display": "FirebirdSQL", "family": "FIREBIRD"},
    {"id": "postgresql", "display": "PostgreSQL", "family": "POSTGRESQL"},
    {"id": "mysql", "display": "MySQL", "family": "MYSQL"},
    {"id": "mariadb", "display": "MariaDB", "family": "MYSQL"},
    {"id": "cassandra", "display": "Cassandra", "family": "CASSANDRA"},
    {"id": "clickhouse", "display": "ClickHouse", "family": "CLICKHOUSE"},
    {"id": "duckdb", "display": "DuckDB", "family": "DUCKDB"},
    {"id": "influxdb", "display": "InfluxDB", "family": "INFLUXDB"},
    {"id": "mongodb", "display": "MongoDB", "family": "MONGODB"},
    {"id": "neo4j", "display": "Neo4j", "family": "NEO4J"},
    {"id": "opensearch", "display": "OpenSearch", "family": "OPENSEARCH"},
    {"id": "redis", "display": "Redis", "family": "REDIS"},
    {"id": "milvus", "display": "Milvus", "family": "MILVUS"},
    {"id": "sqlite", "display": "SQLite", "family": "SQLITE"},
    {"id": "dolt", "display": "Dolt", "family": "MYSQL"},
    {"id": "foundationdb", "display": "FoundationDB", "family": "FOUNDATIONDB"},
    {"id": "vitess", "display": "Vitess", "family": "MYSQL"},
    {"id": "immudb", "display": "immudb", "family": "IMMUDB"},
    {"id": "xtdb", "display": "XTDB", "family": "XTDB"},
    {"id": "tidb", "display": "TiDB", "family": "MYSQL"},
    {"id": "cockroachdb", "display": "CockroachDB", "family": "POSTGRESQL"},
    {"id": "yugabytedb", "display": "YugabyteDB", "family": "POSTGRESQL"},
    {"id": "citus", "display": "Citus", "family": "POSTGRESQL"},
    {"id": "apache_ignite", "display": "Apache Ignite", "family": "IGNITE"},
]

ENGINE_BY_ID = {engine["id"]: engine for engine in ENGINE_META}
PRIMARY_DISPLAY_BY_FAMILY = {
    "FIREBIRD": "FirebirdSQL",
    "POSTGRESQL": "PostgreSQL",
    "MYSQL": "MySQL",
    "CASSANDRA": "Cassandra",
    "CLICKHOUSE": "ClickHouse",
    "DUCKDB": "DuckDB",
    "INFLUXDB": "InfluxDB",
    "MONGODB": "MongoDB",
    "NEO4J": "Neo4j",
    "OPENSEARCH": "OpenSearch",
    "REDIS": "Redis",
    "MILVUS": "Milvus",
}
DATABASE_COLUMNS = ["ScratchBird"] + [engine["display"] for engine in ENGINE_META]

DATATYPE_MATRIX = PACKET_ROOT / "CROSS_ENGINE_DATATYPE_SUPPORT_MATRIX.csv"
INDEX_MATRIX = PACKET_ROOT / "CROSS_ENGINE_INDEX_SUPPORT_MATRIX.csv"
COMBINED_MATRIX = PACKET_ROOT / "CROSS_ENGINE_DATATYPE_AND_INDEX_SUPPORT_MATRIX.csv"


TYPE_ROW_PATTERN = re.compile(
    r'\{"([^"]+)",\s*"([^"]+)",\s*EmulatedStorageKind::([A-Z_]+),\s*DataType::([A-Z0-9_]+),\s*"([^"]*)",\s*"([^"]*)"\}'
)
INDEX_ROW_PATTERN = re.compile(
    r'\{IndexType::([A-Z0-9_]+),\s*"([^"]+)",\s*IndexStorageModel::([A-Z_]+),\s*IndexRuntimeClass::([A-Z_]+),\s*'
    r'(true|false),\s*(true|false),\s*(true|false),\s*(true|false),\s*(true|false),\s*(true|false),\s*(true|false),\s*(true|false)\}'
)
ENGINE_BLOCK_PATTERN = re.compile(
    r'\{\s*"id":\s*"([^"]+)"(.*?)"index_rows":\s*\[(.*?)\]\s*,\s*\}',
    re.S,
)
INDEX_TUPLE_PATTERN = re.compile(
    r'\("([^"]+)",\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)"\)'
)

STANDARD_DATATYPE_LABELS = {
    "INT8": "INT8 / TINYINT",
    "UINT8": "UINT8 / TINYINT UNSIGNED",
    "INT16": "INT16 / SMALLINT",
    "UINT16": "UINT16 / SMALLINT UNSIGNED",
    "INT32": "INT32 / INTEGER / INT",
    "UINT32": "UINT32 / INTEGER UNSIGNED / INT UNSIGNED",
    "INT64": "INT64 / BIGINT",
    "UINT64": "UINT64 / BIGINT UNSIGNED",
    "INT128": "INT128",
    "UINT128": "UINT128",
    "INT256": "INT256",
    "UINT256": "UINT256",
    "DECIMAL": "DECIMAL / NUMERIC",
    "DECIMAL256": "DECIMAL256",
    "DECFLOAT": "DECFLOAT",
    "FLOAT16": "FLOAT16",
    "FLOAT32": "FLOAT32 / REAL / FLOAT",
    "FLOAT64": "FLOAT64 / DOUBLE",
    "BOOLEAN": "BOOLEAN",
    "CHAR": "CHAR",
    "VARCHAR": "VARCHAR",
    "TEXT": "TEXT",
    "BINARY": "BINARY",
    "VARBINARY": "VARBINARY",
    "BYTEA": "BYTEA",
    "BLOB": "BLOB",
    "BIT": "BIT",
    "UUID": "UUID",
    "DATE": "DATE",
    "TIME": "TIME",
    "TIME_WITH_ZONE": "TIME WITH TIME ZONE",
    "TIMESTAMP": "TIMESTAMP / DATETIME",
    "TIMESTAMP_WITH_ZONE": "TIMESTAMP WITH TIME ZONE",
    "TIMESTAMP_NS": "TIMESTAMP_NS",
    "INTERVAL": "INTERVAL / DURATION",
    "MONEY": "MONEY",
    "YEAR": "YEAR",
    "JSON": "JSON",
    "JSONB": "JSONB",
    "JSONPATH": "JSONPATH",
    "XML": "XML",
    "INET": "INET",
    "CIDR": "CIDR",
    "MACADDR": "MACADDR",
    "MACADDR8": "MACADDR8",
    "TSVECTOR": "TSVECTOR",
    "TSQUERY": "TSQUERY",
    "POINT": "POINT",
    "POLYGON": "POLYGON",
    "GEOMETRY": "GEOMETRY",
    "GEOMETRYCOLLECTION": "GEOMETRYCOLLECTION",
    "LINESTRING": "LINESTRING",
    "MULTIPOINT": "MULTIPOINT",
    "MULTILINESTRING": "MULTILINESTRING",
    "MULTIPOLYGON": "MULTIPOLYGON",
    "ARRAY": "ARRAY",
    "ENUM": "ENUM",
    "COMPOSITE": "COMPOSITE",
    "REGCLASS": "REGCLASS",
    "REGPROC": "REGPROC",
    "OID": "OID",
    "OIDVECTOR": "OIDVECTOR",
    "PG_LSN": "PG_LSN",
    "XID": "XID",
    "XID8": "XID8",
    "INT4RANGE": "INT4RANGE",
    "INT8RANGE": "INT8RANGE",
    "NUMRANGE": "NUMRANGE",
    "TSRANGE": "TSRANGE",
    "TSTZRANGE": "TSTZRANGE",
    "DATERANGE": "DATERANGE",
    "LOWCARDINALITY": "LOWCARDINALITY",
}

FAMILY_TYPE_ALIASES = {
    "POSTGRESQL": {
        "BOOL": "BOOLEAN",
        "INT2": "SMALLINT",
        "INT4": "INTEGER",
        "INT8": "BIGINT",
        "FLOAT4": "REAL",
        "FLOAT8": "DOUBLE PRECISION",
        "BPCHAR": "CHAR",
        "TIMESTAMPTZ": "TIMESTAMP WITH TIME ZONE",
        "TIMETZ": "TIME WITH TIME ZONE",
    },
    "MYSQL": {
        "BOOLEAN": "BOOLEAN",
        "BOOL": "BOOL",
        "INT": "INT",
        "INTEGER": "INTEGER",
        "TINYTEXT": "TEXT",
        "MEDIUMTEXT": "TEXT",
        "LONGTEXT": "TEXT",
        "TINYBLOB": "BLOB",
        "MEDIUMBLOB": "BLOB",
        "LONGBLOB": "BLOB",
    },
    "MILVUS": {
        "BOOL": "BOOL",
        "DOUBLE": "DOUBLE",
        "FLOAT": "FLOAT",
        "STRING": "STRING",
    },
    "MONGODB": {
        "STRING": "STRING",
        "INT32": "INT32",
        "INT64": "INT64",
        "DOUBLE": "DOUBLE",
        "DATE": "DATE",
        "TIMESTAMP": "TIMESTAMP",
        "BINARY": "BINARY",
        "ARRAY": "ARRAY",
    },
    "NEO4J": {
        "INTEGER": "INTEGER",
        "FLOAT": "FLOAT",
        "STRING": "STRING",
        "DATETIME": "DATETIME",
        "TIME": "TIME",
        "POINT": "POINT",
    },
    "REDIS": {
        "STRING": "STRING",
        "SET": "SET",
        "LIST": "LIST",
    },
    "CASSANDRA": {
        "VARCHAR": "VARCHAR",
        "TEXT": "TEXT",
        "TIME": "TIME",
        "SET": "SET",
        "VECTOR": "VECTOR",
    },
    "INFLUXDB": {
        "TIMESTAMP": "TIMESTAMP",
    },
}

SPECIAL_DATATYPE_LABELS = {
    ("MYSQL", "BOOLEAN"): "BOOLEAN (MySQL 0/1 alias)",
    ("MYSQL", "BOOL"): "BOOLEAN (MySQL 0/1 alias)",
    ("MYSQL", "TIME"): "TIME (MySQL duration)",
    ("MYSQL", "MEDIUMINT"): "INT24 / MEDIUMINT",
    ("MYSQL", "SET"): "SET (MySQL labels)",
    ("MYSQL", "VECTOR"): "VECTOR (MySQL family)",
    ("CASSANDRA", "TIME"): "TIME (Cassandra nanoseconds)",
    ("CASSANDRA", "SET"): "SET (Cassandra collection)",
    ("CASSANDRA", "TUPLE"): "COMPOSITE",
    ("CASSANDRA", "UDT"): "COMPOSITE",
    ("CASSANDRA", "VECTOR"): "VECTOR (Cassandra fixed-dim)",
    ("MONGODB", "DATE"): "DATE (MongoDB BSON datetime)",
    ("MONGODB", "TIMESTAMP"): "TIMESTAMP (MongoDB oplog)",
    ("MONGODB", "ARRAY"): "ARRAY (MongoDB BSON)",
    ("MONGODB", "BINARY"): "BINARY (MongoDB subtype)",
    ("MONGODB", "BINDATA"): "BINARY (MongoDB subtype)",
    ("MONGODB", "OID"): "OBJECTID",
    ("REDIS", "STRING"): "STRING (Redis binary-safe)",
    ("REDIS", "SET"): "SET (Redis collection)",
    ("REDIS", "LIST"): "LIST (Redis collection)",
    ("NEO4J", "POINT"): "POINT (Neo4j CRS)",
    ("FOUNDATIONDB", "KEY_BYTES"): "KEY_BYTES (FoundationDB)",
    ("FOUNDATIONDB", "VALUE_BYTES"): "VALUE_BYTES (FoundationDB)",
    ("FOUNDATIONDB", "TUPLE"): "TUPLE (FoundationDB layer)",
    ("FOUNDATIONDB", "VERSIONSTAMP"): "VERSIONSTAMP (FoundationDB)",
    ("XTDB", "KEYWORD"): "KEYWORD (XTDB)",
    ("COCKROACHDB", "BYTES"): "BLOB",
}

STANDARD_TOKEN_LABELS = {
    "SMALLINT": "INT16 / SMALLINT",
    "INT2": "INT16 / SMALLINT",
    "TINYINT": "INT8 / TINYINT",
    "INT": "INT32 / INTEGER / INT",
    "INTEGER": "INT32 / INTEGER / INT",
    "INT4": "INT32 / INTEGER / INT",
    "BIGINT": "INT64 / BIGINT",
    "REAL": "FLOAT32 / REAL / FLOAT",
    "FLOAT4": "FLOAT32 / REAL / FLOAT",
    "FLOAT": "FLOAT32 / REAL / FLOAT",
    "DOUBLE": "FLOAT64 / DOUBLE",
    "DOUBLE_PRECISION": "FLOAT64 / DOUBLE",
    "FLOAT8": "FLOAT64 / DOUBLE",
    "BOOLEAN": "BOOLEAN",
    "BOOL": "BOOLEAN",
    "CHAR": "CHAR",
    "BPCHAR": "CHAR",
    "VARCHAR": "VARCHAR",
    "TIMESTAMPTZ": "TIMESTAMP WITH TIME ZONE",
    "TIMETZ": "TIME WITH TIME ZONE",
    "DATE": "DATE",
    "TIME": "TIME",
    "TIMESTAMP": "TIMESTAMP / DATETIME",
    "DATETIME": "TIMESTAMP / DATETIME",
    "UUID": "UUID",
    "JSON": "JSON",
    "JSONB": "JSONB",
    "XML": "XML",
    "BYTEA": "BYTEA",
    "BYTES": "BLOB",
    "BINARY": "BINARY",
    "VARBINARY": "VARBINARY",
    "BLOB": "BLOB",
    "BIT": "BIT",
    "YEAR": "YEAR",
    "INET": "INET",
    "CIDR": "CIDR",
    "MACADDR": "MACADDR",
    "MACADDR8": "MACADDR8",
    "PG_LSN": "PG_LSN",
    "XID": "XID",
    "XID8": "XID8",
    "REGCLASS": "REGCLASS",
    "REGPROC": "REGPROC",
    "OIDVECTOR": "OIDVECTOR",
    "OID": "OID",
    "TSVECTOR": "TSVECTOR",
    "TSQUERY": "TSQUERY",
    "POINT": "POINT",
    "POLYGON": "POLYGON",
    "GEOMETRY": "GEOMETRY",
    "LINESTRING": "LINESTRING",
    "MULTIPOINT": "MULTIPOINT",
    "MULTILINESTRING": "MULTILINESTRING",
    "MULTIPOLYGON": "MULTIPOLYGON",
    "GEOMETRYCOLLECTION": "GEOMETRYCOLLECTION",
    "ARRAY": "ARRAY",
    "ENUM": "ENUM",
    "COMPOSITE": "COMPOSITE",
    "INT4RANGE": "INT4RANGE",
    "INT8RANGE": "INT8RANGE",
    "NUMRANGE": "NUMRANGE",
    "TSRANGE": "TSRANGE",
    "TSTZRANGE": "TSTZRANGE",
    "DATERANGE": "DATERANGE",
    "LOWCARDINALITY": "LOWCARDINALITY",
    "STRING": "TEXT",
    "TEXT": "TEXT",
}

TRANSLATED_HINT_FRAGMENTS = (
    "epoch",
    "timezone",
    "datetime",
    "offset",
    "crs",
    "srid",
    "format",
    "subtype",
    "wkb",
    "ewkb",
    "ms->us",
    "nanosecond",
    "hex split",
)

UNSIGNED_MYSQL_ROWS = [
    ("TINYINT UNSIGNED", "UINT8 / TINYINT UNSIGNED", "native"),
    ("SMALLINT UNSIGNED", "UINT16 / SMALLINT UNSIGNED", "native"),
    ("INT UNSIGNED", "UINT32 / INTEGER UNSIGNED / INT UNSIGNED", "native"),
    ("INTEGER UNSIGNED", "UINT32 / INTEGER UNSIGNED / INT UNSIGNED", "native"),
    ("BIGINT UNSIGNED", "UINT64 / BIGINT UNSIGNED", "native"),
    ("MEDIUMINT UNSIGNED", "UINT24 / MEDIUMINT UNSIGNED", "domain"),
]

INDEX_STATUS_PRIORITY = {"Missing": 0, "Partial": 1, "Supported": 2}
DONOR_DATATYPE_PRIORITY = {"missing": 0, "translated": 1, "domain": 2, "native": 3}
SCRATCHBIRD_DATATYPE_PRIORITY = {"missing": 0, "translated": 1, "domain": 2, "native": 3}
NON_STORAGE_SURFACE_KINDS = {
    "protocol_field_type",
    "querypb_type",
    "sqltypes_case",
    "collection_modifier",
    "arrow_col_type_method",
    "sql_value_type",
    "extension_runtime_type",
}
XTDB_ABSTRACT_HIERARCHY_TOKENS = {
    "ANY",
    "NULL",
    "NUM",
    "INT",
    "UINT",
    "FLOAT",
    "DATE_TIME",
}
DUCKDB_NON_STORAGE_LOGICAL_TYPES = {"ANY", "UNKNOWN", "SQLNULL", "INVALID"}
CLICKHOUSE_NON_STORAGE_FACTORY_TYPES = {"NULLABLE", "NOTHING"}
MILVUS_NON_STORAGE_INTERNAL_TYPES = {"NONE"}
SUPPORT_ROW_SKIP_TOKENS = {
    "firebird": {
        "DECFLOAT",
    },
    "duckdb": {
        "AGGREGATE_STATE",
        "INTEGER_LITERAL",
        "LAMBDA",
        "LEGACY_AGGREGATE_STATE",
        "POINTER",
        "STRING_LITERAL",
        "TABLE",
        "TEMPLATE",
        "TYPE",
        "UNBOUND",
        "VALIDITY",
    },
    "cassandra": {
        "BUFFER",
        "CUSTOM",
        "ELEMENT",
        "ELEMENTS",
        "FIRST",
        "KEYSPACE",
        "TYPE",
    },
    "cockroachdb": {
        "ARRAYCONTENTS",
        "BIT_0",
        "BIT_N",
        "CHAR_COL",
        "CHAR_N",
        "CHAR_N_COL",
        "DECIMAL_N",
        "FAMILY",
        "FIELD",
        "M",
        "NULL_UNKNOWN",
        "STRING_COLLATE_EN",
        "STRING_N",
        "STRING_N_COL",
        "TUPLECONTENTS",
        "TUPLELABELS",
        "VARBIT_N",
        "VARCHAR_COL",
        "VARCHAR_N",
        "VARCHAR_N_COL",
    },
    "mongodb": {
        "EOO",
    },
    "mysql": {
        "NCHAR",
        "NVARCHAR",
    },
    "mariadb": {
        "NCHAR",
        "NVARCHAR",
    },
    "opensearch": {
        "ALIAS",
        "CONSTANT_KEYWORD",
        "CONTEXT_AWARE_GROUPING",
        "DERIVED",
        "NESTED",
        "OBJECT",
        "STAR_TREE",
    },
    "redis": {
        "HASH_KEY",
        "HASH_VALUE",
        "SHARED_BULKHDR_LEN",
        "SHARED_INTEGERS",
        "TYPE_BASIC_MAX",
        "TYPE_MAX",
    },
}
ROW_ORDER_RANK = {
    "INT8 / TINYINT": 10,
    "UINT8 / TINYINT UNSIGNED": 11,
    "INT16 / SMALLINT": 12,
    "UINT16 / SMALLINT UNSIGNED": 13,
    "INT24 / MEDIUMINT": 14,
    "UINT24 / MEDIUMINT UNSIGNED": 15,
    "INT32 / INTEGER / INT": 16,
    "UINT32 / INTEGER UNSIGNED / INT UNSIGNED": 17,
    "INT64 / BIGINT": 18,
    "UINT64 / BIGINT UNSIGNED": 19,
    "INT128": 20,
    "UINT128": 21,
    "DECIMAL / NUMERIC": 30,
    "DECFLOAT": 31,
    "FLOAT32 / REAL / FLOAT": 32,
    "FLOAT64 / DOUBLE": 33,
    "MONEY": 34,
    "BOOLEAN": 40,
    "BOOLEAN (MySQL 0/1 alias)": 41,
    "CHAR": 50,
    "VARCHAR": 51,
    "TEXT": 52,
    "BINARY": 53,
    "VARBINARY": 54,
    "BYTEA": 55,
    "BLOB": 56,
    "BIT": 57,
    "UUID": 58,
    "DATE": 60,
    "DATE (MongoDB BSON datetime)": 61,
    "TIME": 62,
    "TIME WITH TIME ZONE": 63,
    "TIME (MySQL duration)": 64,
    "TIME (Cassandra nanoseconds)": 65,
    "TIMESTAMP / DATETIME": 66,
    "TIMESTAMP WITH TIME ZONE": 67,
    "TIMESTAMP_NS": 68,
    "TIMESTAMP (MongoDB oplog)": 69,
    "INTERVAL / DURATION": 70,
    "YEAR": 71,
    "JSON": 80,
    "JSONB": 81,
    "XML": 82,
    "ARRAY": 90,
    "ARRAY (MongoDB BSON)": 91,
    "SET (MySQL labels)": 92,
    "SET (Cassandra collection)": 93,
    "SET (Redis collection)": 94,
    "POINT": 95,
    "POINT (Neo4j CRS)": 96,
    "GEOMETRY": 97,
}
POSTGRES_TYPE_METADATA_CACHE: dict[str, dict[str, dict[str, str]]] = {}


def normalize_token(value: str) -> str:
    return re.sub(r"[^A-Z0-9]+", "_", value.upper()).strip("_")


def write_csv(path: Path, fieldnames: list[str], rows: list[dict[str, str]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        for row in rows:
            writer.writerow(row)


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle))


def parse_postgres_catalog_type_metadata(relative_path: str) -> dict[str, dict[str, str]]:
    cached = POSTGRES_TYPE_METADATA_CACHE.get(relative_path)
    if cached is not None:
        return cached

    path = ROOT / relative_path
    metadata: dict[str, dict[str, str]] = {}
    if not path.exists():
        POSTGRES_TYPE_METADATA_CACHE[relative_path] = metadata
        return metadata

    text = path.read_text(encoding="utf-8")
    for match in re.finditer(r"\{[^{}]*typname => '([^']+)'[^{}]*\}", text, re.S):
        block = match.group(0)
        typname = normalize_token(match.group(1))
        typtype_match = re.search(r"typtype => '([^']+)'", block)
        typrelid_match = re.search(r"typrelid => '([^']+)'", block)
        typcategory_match = re.search(r"typcategory => '([^']+)'", block)
        metadata[typname] = {
            "typtype": typtype_match.group(1) if typtype_match else "",
            "typrelid": typrelid_match.group(1) if typrelid_match else "",
            "typcategory": typcategory_match.group(1) if typcategory_match else "",
        }

    POSTGRES_TYPE_METADATA_CACHE[relative_path] = metadata
    return metadata


def parse_type_rows() -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    text = TYPE_SYSTEM.read_text(encoding="utf-8")
    for match in TYPE_ROW_PATTERN.finditer(text):
        rows.append(
            {
                "engine_name": match.group(1),
                "emulated_type": match.group(2),
                "emulated_type_key": normalize_token(match.group(2)),
                "storage_kind": match.group(3),
                "canonical_type": match.group(4),
                "domain_hint": match.group(5),
                "parser_rule_hint": match.group(6),
            }
        )
    return rows


def parse_native_types() -> set[str]:
    text = CORE_TYPES.read_text(encoding="utf-8")
    block = text.split("enum class DataType : uint16_t", 1)[1].split("};", 1)[0]
    return {normalize_token(match.group(1)) for match in re.finditer(r"^\s*([A-Z][A-Z0-9_]+)\s*=", block, re.M)}


def parse_index_registry() -> dict[str, dict[str, str]]:
    rows: dict[str, dict[str, str]] = {}
    text = INDEX_FACTORY.read_text(encoding="utf-8")
    for match in INDEX_ROW_PATTERN.finditer(text):
        index_type = match.group(1)
        rows[index_type] = {
            "index_type": index_type,
            "canonical_name": match.group(2),
            "storage_model": match.group(3),
            "runtime_class": match.group(4),
        }
    return rows


def parse_engine_index_rows() -> dict[str, list[dict[str, str]]]:
    text = (ROOT / "scripts/reference/generate_emulation_reference_packets.py").read_text(encoding="utf-8")
    rows_by_engine: dict[str, list[dict[str, str]]] = defaultdict(list)
    for engine_match in ENGINE_BLOCK_PATTERN.finditer(text):
        engine_id = engine_match.group(1)
        block = engine_match.group(3)
        for tuple_match in INDEX_TUPLE_PATTERN.finditer(block):
            rows_by_engine[engine_id].append(
                {
                    "donor_surface": tuple_match.group(1),
                    "donor_surface_key": normalize_token(tuple_match.group(1)),
                    "sb_index": tuple_match.group(2),
                    "compatibility_class": tuple_match.group(3),
                    "note": tuple_match.group(4),
                }
            )
    return rows_by_engine


def datatype_row_label_from_type_row(row: dict[str, str]) -> str:
    special = SPECIAL_DATATYPE_LABELS.get((row["engine_name"], row["emulated_type_key"]))
    if special:
        return special
    if row["storage_kind"] == "DOMAIN":
        if row["emulated_type_key"] in STANDARD_TOKEN_LABELS:
            return STANDARD_TOKEN_LABELS[row["emulated_type_key"]]
        if row["emulated_type_key"] in STANDARD_DATATYPE_LABELS:
            return STANDARD_DATATYPE_LABELS[row["emulated_type_key"]]
        return row["emulated_type"].upper()
    canonical = STANDARD_DATATYPE_LABELS.get(row["canonical_type"])
    if canonical:
        return canonical
    return row["emulated_type"].upper()


def donor_status_from_type_row(row: dict[str, str]) -> str:
    if row["storage_kind"] == "DOMAIN":
        return "domain"
    hint = row["parser_rule_hint"].lower()
    if any(fragment in hint for fragment in TRANSLATED_HINT_FRAGMENTS):
        return "translated"
    return "native"


def scratchbird_status_from_type_row(row: dict[str, str]) -> str:
    if row["storage_kind"] == "DOMAIN":
        return "domain"
    special_label = SPECIAL_DATATYPE_LABELS.get((row["engine_name"], row["emulated_type_key"]))
    if special_label in {
        "DATE (MongoDB BSON datetime)",
        "POINT (Neo4j CRS)",
    }:
        return "translated"
    return "native"


def support_path_to_status(row: dict[str, str]) -> str:
    support_path = row["support_path"]
    if support_path == "ENGINE_MAPPING_PRESENT":
        return "native"
    if support_path == "NATIVE_DIRECT":
        return "native"
    if support_path == "DOMAIN_PRECEDENT_EXISTS":
        return "domain"
    if support_path == "ALIAS_OR_CATALOG_NAME_ONLY" and row["sb_native_exact_name_match"] == "yes":
        return "native"
    return "missing"


def donor_support_row_status(row: dict[str, str]) -> str:
    del row
    return "native"


def build_explicit_type_maps(
    type_rows: list[dict[str, str]]
) -> tuple[
    dict[tuple[str, str], dict[str, str]],
    dict[str, dict[str, str]],
    dict[str, dict[str, str]],
]:
    family_lookup: dict[tuple[str, str], dict[str, str]] = {}
    unique_token_lookup: dict[str, dict[str, str]] = {}
    label_registry: dict[str, dict[str, str]] = {}
    rows_by_token: dict[str, list[dict[str, str]]] = defaultdict(list)

    for row in type_rows:
        family_lookup[(row["engine_name"], row["emulated_type_key"])] = row
        rows_by_token[row["emulated_type_key"]].append(row)
        label = datatype_row_label_from_type_row(row)
        scratchbird_status = scratchbird_status_from_type_row(row)
        existing = label_registry.get(label)
        if not existing or SCRATCHBIRD_DATATYPE_PRIORITY[scratchbird_status] > SCRATCHBIRD_DATATYPE_PRIORITY[existing["scratchbird_status"]]:
            label_registry[label] = {
                "scratchbird_status": scratchbird_status,
                "order_rank": str(ROW_ORDER_RANK.get(label, 1000)),
            }

    for token, rows in rows_by_token.items():
        labels = {datatype_row_label_from_type_row(row) for row in rows}
        if len(labels) == 1:
            unique_token_lookup[token] = rows[0]

    return family_lookup, unique_token_lookup, label_registry


def ensure_datatype_row(
    rows: dict[str, dict[str, str]],
    order_map: dict[str, int],
    label_registry: dict[str, dict[str, str]],
    label: str,
) -> dict[str, str]:
    if label not in rows:
        row = {"datatype": label}
        for column in DATABASE_COLUMNS:
            row[column] = "missing"
        row["ScratchBird"] = label_registry.get(label, {}).get("scratchbird_status", "missing")
        rows[label] = row
        order_map[label] = ROW_ORDER_RANK.get(label, 1000)
    return rows[label]


def maybe_set_datatype_status(row: dict[str, str], column: str, status: str) -> None:
    if DONOR_DATATYPE_PRIORITY[status] > DONOR_DATATYPE_PRIORITY[row[column]]:
        row[column] = status


def resolve_support_row(
    engine: dict[str, str],
    row: dict[str, str],
    family_lookup: dict[tuple[str, str], dict[str, str]],
    unique_token_lookup: dict[str, dict[str, str]],
    label_registry: dict[str, dict[str, str]],
    native_types: set[str],
) -> tuple[str, str, str]:
    token = row["donor_surface_key"]
    family = engine["family"]
    alias_token = FAMILY_TYPE_ALIASES.get(family, {}).get(token, token)
    family_match = family_lookup.get((family, alias_token))
    if family_match:
        return (
            datatype_row_label_from_type_row(family_match),
            donor_status_from_type_row(family_match),
            scratchbird_status_from_type_row(family_match),
        )

    special = SPECIAL_DATATYPE_LABELS.get((family, token)) or SPECIAL_DATATYPE_LABELS.get((engine["id"].upper(), token))
    if special:
        scratchbird = label_registry.get(special, {}).get("scratchbird_status", support_path_to_status(row))
        return special, donor_support_row_status(row), scratchbird

    unique_match = unique_token_lookup.get(token)
    if unique_match:
        label = datatype_row_label_from_type_row(unique_match)
        scratchbird = label_registry.get(label, {}).get("scratchbird_status", support_path_to_status(row))
        status = donor_support_row_status(row)
        return label, status, scratchbird

    if token in STANDARD_TOKEN_LABELS:
        label = STANDARD_TOKEN_LABELS[token]
        scratchbird = label_registry.get(label, {}).get("scratchbird_status", support_path_to_status(row))
        return label, donor_support_row_status(row), scratchbird

    if token in native_types and token in STANDARD_DATATYPE_LABELS:
        label = STANDARD_DATATYPE_LABELS[token]
        scratchbird = label_registry.get(label, {}).get("scratchbird_status", support_path_to_status(row))
        return label, donor_support_row_status(row), scratchbird

    label = SPECIAL_DATATYPE_LABELS.get((engine["family"], token)) or SPECIAL_DATATYPE_LABELS.get((engine["id"].upper(), token))
    if label:
        scratchbird = label_registry.get(label, {}).get("scratchbird_status", support_path_to_status(row))
        return label, donor_support_row_status(row), scratchbird

    return row["donor_surface"].upper(), donor_support_row_status(row), support_path_to_status(row)


def build_datatype_matrix(
    type_rows: list[dict[str, str]],
    native_types: set[str],
) -> list[dict[str, str]]:
    family_lookup, unique_token_lookup, label_registry = build_explicit_type_maps(type_rows)
    rows: dict[str, dict[str, str]] = {}
    order_map: dict[str, int] = {}

    for type_row in type_rows:
        label = datatype_row_label_from_type_row(type_row)
        row = ensure_datatype_row(rows, order_map, label_registry, label)
        primary_display = PRIMARY_DISPLAY_BY_FAMILY.get(type_row["engine_name"])
        if primary_display:
            maybe_set_datatype_status(row, primary_display, donor_status_from_type_row(type_row))
        maybe_set_datatype_status(row, "ScratchBird", scratchbird_status_from_type_row(type_row))

    mysql_like_columns = {"MySQL", "MariaDB", "Dolt", "Vitess", "TiDB"}
    for _, label, scratchbird_status in UNSIGNED_MYSQL_ROWS:
        row = ensure_datatype_row(rows, order_map, label_registry, label)
        maybe_set_datatype_status(row, "ScratchBird", scratchbird_status)
        for column in mysql_like_columns:
            maybe_set_datatype_status(row, column, scratchbird_status)

    for engine in ENGINE_META:
        support_path = PACKET_ROOT / engine["id"] / "donor_vs_scratchbird_datatype_support.csv"
        if not support_path.exists():
            continue
        for support_row in read_csv(support_path):
            surface_kind = support_row["surface_kind"]
            token = support_row["donor_surface_key"]
            if surface_kind in NON_STORAGE_SURFACE_KINDS:
                continue
            if token in SUPPORT_ROW_SKIP_TOKENS.get(engine["id"], set()):
                continue
            if surface_kind == "distributed_type_surface" and token == "CREATE_TYPE":
                continue
            if engine["id"] == "xtdb" and surface_kind == "col_type_hierarchy" and token in XTDB_ABSTRACT_HIERARCHY_TOKENS:
                continue
            if engine["id"] == "duckdb" and surface_kind == "logical_type_id" and token in DUCKDB_NON_STORAGE_LOGICAL_TYPES:
                continue
            if engine["id"] == "clickhouse" and surface_kind == "datatype_factory_registration" and token in CLICKHOUSE_NON_STORAGE_FACTORY_TYPES:
                continue
            if engine["id"] == "milvus" and surface_kind == "internal_data_type" and token in MILVUS_NON_STORAGE_INTERNAL_TYPES:
                continue
            if surface_kind == "catalog_typname":
                metadata = parse_postgres_catalog_type_metadata(support_row["evidence_path"])
                token_meta = metadata.get(token, {})
                if token_meta.get("typtype") == "p":
                    continue
                if token_meta.get("typtype") == "c" and token_meta.get("typrelid", "").startswith("pg_"):
                    continue
            if token in {"TYPEUNSPECIFIED", "NULL"}:
                continue
            if (
                surface_kind == "protocol_field_type"
                and token in {"VAR_STRING", "STRING"}
                and engine["family"] == "MYSQL"
            ):
                continue
            label, donor_status, scratchbird_status = resolve_support_row(
                engine=engine,
                row=support_row,
                family_lookup=family_lookup,
                unique_token_lookup=unique_token_lookup,
                label_registry=label_registry,
                native_types=native_types,
            )
            row = ensure_datatype_row(rows, order_map, label_registry, label)
            if row[engine["display"]] == "missing":
                maybe_set_datatype_status(row, engine["display"], donor_status)
            if row["ScratchBird"] == "missing":
                maybe_set_datatype_status(row, "ScratchBird", scratchbird_status)

    def sort_key(item: dict[str, str]) -> tuple[int, str]:
        label = item["datatype"]
        return (order_map.get(label, 1000), label)

    filtered_rows = [
        row
        for row in rows.values()
        if any(row[column] != "missing" for column in DATABASE_COLUMNS)
    ]
    return sorted(filtered_rows, key=sort_key)


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


def build_index_matrix(index_registry: dict[str, dict[str, str]]) -> list[dict[str, str]]:
    engine_index_rows = parse_engine_index_rows()
    rows: dict[str, dict[str, str]] = {}
    order_map: dict[str, int] = {}
    explicit_lookup: dict[tuple[str, str], dict[str, str]] = {}

    def ensure_index_row(label: str, scratchbird_status: str = "Missing") -> dict[str, str]:
        if label not in rows:
            row = {"indextype": label}
            for column in DATABASE_COLUMNS:
                row[column] = "Missing"
            row["ScratchBird"] = scratchbird_status
            rows[label] = row
            order_map[label] = 1000
        return rows[label]

    def maybe_set_index_status(row: dict[str, str], column: str, status: str) -> None:
        if INDEX_STATUS_PRIORITY[status] > INDEX_STATUS_PRIORITY[row[column]]:
            row[column] = status

    def label_for_index(sb_index: str, donor_surface_key: str) -> str:
        if sb_index in index_registry:
            return sb_index
        return donor_surface_key

    for engine in ENGINE_META:
        for index_row in engine_index_rows.get(engine["id"], []):
            label = label_for_index(index_row["sb_index"], index_row["donor_surface_key"])
            explicit_lookup[(engine["id"], index_row["donor_surface_key"])] = {
                "label": label,
                "compatibility_class": index_row["compatibility_class"],
                "sb_index": index_row["sb_index"],
            }
            for alias in donor_index_surface_aliases(engine["id"], index_row["donor_surface"]):
                explicit_lookup[(engine["id"], alias)] = {
                    "label": label,
                    "compatibility_class": index_row["compatibility_class"],
                    "sb_index": index_row["sb_index"],
                }
            scratchbird_status = "Supported" if index_row["sb_index"] in index_registry else "Missing"
            row = ensure_index_row(label, scratchbird_status=scratchbird_status)
            donor_status = "Supported" if index_row["compatibility_class"] == "SB_NATIVE_INDEX_PRESENT" else "Partial"
            maybe_set_index_status(row, "ScratchBird", scratchbird_status)
            maybe_set_index_status(row, engine["display"], donor_status)
            order_map[label] = min(order_map.get(label, 1000), 100)

    for engine in ENGINE_META:
        support_path = PACKET_ROOT / engine["id"] / "donor_vs_scratchbird_index_support.csv"
        if not support_path.exists():
            continue
        for support_row in read_csv(support_path):
            explicit = explicit_lookup.get((engine["id"], support_row["donor_surface_key"]))
            if explicit:
                label = explicit["label"]
                donor_status = "Supported" if explicit["compatibility_class"] == "SB_NATIVE_INDEX_PRESENT" else "Partial"
                scratchbird_status = "Supported" if explicit["sb_index"] in index_registry else "Missing"
            elif support_row["sb_direct_index_family_match"] == "yes":
                label = support_row["sb_direct_index_family"]
                donor_status = "Partial"
                scratchbird_status = "Supported" if label in index_registry else "Missing"
            else:
                label = support_row["donor_surface_key"]
                donor_status = "Missing"
                scratchbird_status = "Missing"
            row = ensure_index_row(label, scratchbird_status=scratchbird_status)
            maybe_set_index_status(row, "ScratchBird", scratchbird_status)
            maybe_set_index_status(row, engine["display"], donor_status)

    return sorted(rows.values(), key=lambda row: (order_map.get(row["indextype"], 1000), row["indextype"]))


def write_combined_matrix(
    datatype_rows: list[dict[str, str]],
    index_rows: list[dict[str, str]],
) -> None:
    COMBINED_MATRIX.parent.mkdir(parents=True, exist_ok=True)
    with COMBINED_MATRIX.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(["datatype", *DATABASE_COLUMNS])
        for row in datatype_rows:
            writer.writerow([row["datatype"], *[row[column] for column in DATABASE_COLUMNS]])
        writer.writerow([])
        writer.writerow(["indextype", *DATABASE_COLUMNS])
        for row in index_rows:
            writer.writerow([row["indextype"], *[row[column] for column in DATABASE_COLUMNS]])


def main() -> None:
    type_rows = parse_type_rows()
    native_types = parse_native_types()
    index_registry = parse_index_registry()

    datatype_rows = build_datatype_matrix(type_rows=type_rows, native_types=native_types)
    index_rows = build_index_matrix(index_registry=index_registry)

    write_csv(DATATYPE_MATRIX, ["datatype", *DATABASE_COLUMNS], datatype_rows)
    write_csv(INDEX_MATRIX, ["indextype", *DATABASE_COLUMNS], index_rows)
    write_combined_matrix(datatype_rows=datatype_rows, index_rows=index_rows)


if __name__ == "__main__":
    main()
