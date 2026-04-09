#!/usr/bin/env python3
"""Generate the April 2026 cross-engine comparison report batch.

The canonical output format is the narrow current-state vs target-state matrix
defined by the section 31 comparison-report specification. This generator is
ScratchBird-centric: it models ScratchBird as a database environment with a
separate parser/front-door layer, virtual catalog overlays, UUID identity, and
cluster-aware lineage semantics rather than flattening it into a generic SQL
engine profile.
"""

from __future__ import annotations

import csv
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


REPO_ROOT = Path("/home/dcalford/CliWork/ScratchBird")
APRIL_DIR = REPO_ROOT / "docs/audit/comparisons/April_2026"
REFERENCE_DOC = REPO_ROOT / "docs/specifications/Reference_Documentation_specification.md"
REFERENCE_ROOT = REPO_ROOT / "docs/reference/formal_reference_set"
MANIFEST_PATH = APRIL_DIR / "comparison_report_manifest.csv"
README_PATH = APRIL_DIR / "README.md"
REPORT_DATE = "2026-04-01"
REPORT_FAMILY_VERSION = "april_2026_v6"
GENERATOR = "Codex"

DIRECT_DIALECTS = {"firebirdsql", "postgresql", "mysql"}
MYSQL_FAMILY = {"mysql", "mariadb", "tidb"}
POSTGRES_FAMILY = {"postgresql", "cockroachdb", "yugabytedb", "citus"}
TARGETED_SPECIALIZED = {"cassandra", "mongodb", "neo4j", "redis", "milvus"}
DIRECT_FAMILY_SUBORDINATE_KINDS = {
    "catalog_overlay",
    "system_views",
    "type_mapping",
    "schema_mapping",
    "donor_query_emulation",
    "front_door",
    "prepared_error",
}
PLANNED_EMULATION_SUBORDINATE_KINDS = DIRECT_FAMILY_SUBORDINATE_KINDS | {"tool_harness"}

SQL_DONORS = {
    "firebirdsql",
    "postgresql",
    "mysql",
    "mariadb",
    "tidb",
    "cockroachdb",
    "yugabytedb",
    "citus",
    "sqlite",
    "dolt",
    "duckdb",
    "clickhouse",
    "apache_ignite",
    "xtdb",
}
DOCUMENT_DONORS = {"mongodb", "xtdb"}
GRAPH_DONORS = {"neo4j"}
VECTOR_DONORS = {"milvus"}
SEARCH_DONORS = {"opensearch", "clickhouse"}
KV_DONORS = {"redis", "cassandra", "foundationdb", "etcd", "tikv", "immudb"}
TIME_SERIES_DONORS = {"influxdb", "cassandra", "clickhouse"}
HISTORY_DONORS = {"dolt", "xtdb", "immudb"}
PROCEDURAL_DONORS = {"firebirdsql", "postgresql", "mysql", "mariadb", "tidb", "citus"}
DISTRIBUTED_DONORS = {
    "foundationdb",
    "etcd",
    "tikv",
    "mongodb",
    "opensearch",
    "cassandra",
    "redis",
    "neo4j",
    "milvus",
    "tidb",
    "cockroachdb",
    "yugabytedb",
    "citus",
    "apache_ignite",
}
REPLICATION_DONORS = {
    "postgresql",
    "mysql",
    "mariadb",
    "tidb",
    "cockroachdb",
    "yugabytedb",
    "citus",
    "mongodb",
    "opensearch",
    "cassandra",
    "redis",
    "neo4j",
    "milvus",
    "foundationdb",
    "etcd",
    "tikv",
}

NON_DATABASE_DONORS = {
    "SQLancer",
    "sqllogictest",
    "SQLsmith",
    "Apache Calcite",
    "Debezium",
    "WiredTiger",
    "Substrait",
    "Vitess",
    "RocksDB",
    "LMDB",
    "Jepsen",
    "TLA+",
    "Elle",
    "Maelstrom",
    "Arrow Flight SQL",
    "Raft paper",
    "Spanner paper",
    "Calvin paper",
    "Calcite paper",
    "Differential Query Execution",
    "SQLancer / TLP paper",
}

HARD_BLOCKING_CLASSES = {"CURRENT_GAP", "TARGET_GAP", "UNKNOWN"}
MAPPING_BLOCKING_CLASS = "MAPPING_REQUIRED"


@dataclass(frozen=True)
class State:
    code: str
    coverage: str
    evidence: str


@dataclass(frozen=True)
class Feature:
    section_number: int
    section_title: str
    family_key: str
    feature_key: str
    feature_name: str
    kind: str
    comparison_mode: str
    summary: str
    refs: tuple[str, ...]


@dataclass
class ReportSeed:
    donor: str
    rank: int
    phase: str
    role: str
    slug: str
    report_file: str
    why_included: str
    what_to_study: str
    what_not_to_copy: str
    ref_dir: Path
    ref_readme: str
    ref_source: str
    source_url: str


FEATURES: tuple[Feature, ...] = (
    Feature(
        5,
        "Architecture, Storage, and Transaction Core",
        "architecture",
        "architecture.product_model_and_execution_boundary",
        "Product model and execution boundary",
        "product_model",
        "structural",
        "Compare a donor database product against ScratchBird's database-environment model rather than flattening both subjects into one generic engine label.",
        (
            "docs/specifications/29_Listener_and_Server_Orchestration/PARSER_AGENT_EXECUTABLE_COMPOSITION_AND_RUNTIME_STACK_MODEL.md",
            "docs/specifications/28_Parser_Implementations/README.md",
            "docs/specifications/25_Runtime_Modes/EMBEDDED_DIRECT_ENGINE_AND_LOCAL_SHARED_SERVER_RUNTIME_SELECTION_MODEL.md",
        ),
    ),
    Feature(
        5,
        "Architecture, Storage, and Transaction Core",
        "architecture",
        "architecture.storage_engine_toast_and_oversized_value_model",
        "Storage engine, TOAST, and oversized-value model",
        "oversized_value",
        "capability",
        "Compare large-value storage, off-row or chunked storage, and ScratchBird's TOAST-first storage contract.",
        (
            "docs/specifications/11_TOAST_and_LOB_Storage/README.md",
            "docs/specifications/11_TOAST_and_LOB_Storage/LOB_PAGE_LAYOUTS.md",
        ),
    ),
    Feature(
        5,
        "Architecture, Storage, and Transaction Core",
        "architecture",
        "architecture.transaction_context_lifecycle",
        "Transaction-context lifecycle",
        "txn_context",
        "mga",
        "Compare whether the platform is always in a transaction and how commit or rollback transitions the next execution boundary.",
        (
            "docs/specifications/29_Listener_and_Server_Orchestration/CONNECTION_AND_SESSION_LIFECYCLE.md",
            "docs/specifications/42_Failure_Model_and_Fault_Tolerance/FAILURE_MODEL_AND_FAULT_CLASSIFICATION.md",
        ),
    ),
    Feature(
        5,
        "Architecture, Storage, and Transaction Core",
        "architecture",
        "architecture.non_destructive_version_lineage",
        "Non-destructive version lineage",
        "version_lineage",
        "mga",
        "Compare update or delete lineage semantics, retained back versions, and whether mutation is destructive or version-preserving.",
        (
            "docs/specifications/42_Failure_Model_and_Fault_Tolerance/FAILURE_MODEL_AND_FAULT_CLASSIFICATION.md",
            "docs/specifications/20_Diagnostics_Audit_and_Observability/LOGICAL_ROW_UUID_CLUSTER_TRACKING_AND_VERSION_LINEAGE_OBSERVABILITY_MODEL.md",
        ),
    ),
    Feature(
        5,
        "Architecture, Storage, and Transaction Core",
        "architecture",
        "architecture.recovery_truth_and_derivative_lanes",
        "Recovery truth and derivative lanes",
        "recovery_model",
        "mga",
        "Compare whether recovery truth comes from durable MGA state or from log-centric replay and how derivative lanes are treated.",
        (
            "docs/specifications/42_Failure_Model_and_Fault_Tolerance/FAILURE_MODEL_AND_FAULT_CLASSIFICATION.md",
            "docs/specifications/20_Diagnostics_Audit_and_Observability/MGA_OBSERVABILITY_AND_OPERATOR_DIAGNOSTICS.md",
        ),
    ),
    Feature(
        5,
        "Architecture, Storage, and Transaction Core",
        "architecture",
        "architecture.cluster_ready_identity_and_retained_replication_model",
        "Cluster-ready identity and retained replication model",
        "cluster_ready",
        "capability",
        "Compare whether identity, retained lineage, and replication-friendly record history are part of the core platform model.",
        (
            "docs/specifications/07_Catalog_Bootstrap_and_UUID_Mapping/UUID_IDENTITY_AND_COLLISION_RULES.md",
            "docs/specifications/20_Diagnostics_Audit_and_Observability/CLUSTER_IDENTITY_SECURITY_QUORUM_AND_WRITE_FENCE_OBSERVABILITY_MODEL.md",
            "docs/specifications/20_Diagnostics_Audit_and_Observability/CLUSTER_ROUTING_FENCING_AND_SHARD_COMMIT_LOG_OBSERVABILITY.md",
        ),
    ),
    Feature(
        6,
        "Catalog, Metadata, and Object Identity",
        "catalog",
        "catalog.uuid_identity_and_row_lineage_keys",
        "UUID identity and row-lineage keys",
        "uuid_identity",
        "capability",
        "Compare durable database, object, and row identity rules and how they participate in lineage and cluster safety.",
        (
            "docs/specifications/07_Catalog_Bootstrap_and_UUID_Mapping/UUID_IDENTITY_AND_COLLISION_RULES.md",
            "docs/specifications/20_Diagnostics_Audit_and_Observability/LOGICAL_ROW_UUID_CLUSTER_TRACKING_AND_VERSION_LINEAGE_OBSERVABILITY_MODEL.md",
        ),
    ),
    Feature(
        6,
        "Catalog, Metadata, and Object Identity",
        "catalog",
        "catalog.recursive_schema_tree_and_schema_root_sandboxing",
        "Recursive schema tree and schema-root sandboxing",
        "schema_tree",
        "structural",
        "Compare namespace structure, schema-root scoping, and whether sessions can be sandboxed into a recursive schema tree.",
        (
            "docs/specifications/24_Catalog_Model_and_Virtual_Overlays/SCHEMA_TREE_CANONICAL.md",
            "docs/specifications/19_Security_Model/SECURITY_DEFINER_RLS_AND_EMULATED_SCHEMA_SANDBOX_MODEL.md",
        ),
    ),
    Feature(
        6,
        "Catalog, Metadata, and Object Identity",
        "catalog",
        "catalog.donor_catalog_overlay_and_system_table_emulation",
        "Donor catalog overlay and system-table emulation",
        "catalog_overlay",
        "emulation",
        "Compare donor system catalogs or metadata surfaces against ScratchBird's virtual-overlay and emulated-schema model.",
        (
            "docs/specifications/24_Catalog_Model_and_Virtual_Overlays/README.md",
            "docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_INVENTORY.md",
        ),
    ),
    Feature(
        6,
        "Catalog, Metadata, and Object Identity",
        "catalog",
        "catalog.metadata_publication_and_schema_epoch_rules",
        "Metadata publication and schema-epoch rules",
        "metadata_publication",
        "capability",
        "Compare commit-bound metadata publication, invalidation, and schema-epoch boundaries.",
        (
            "docs/specifications/24_Catalog_Model_and_Virtual_Overlays/README.md",
            "docs/specifications/37_Statistics_Metadata_and_Schema_DDL/SCHEMA_DDL_STATE_MACHINE.md",
        ),
    ),
    Feature(
        6,
        "Catalog, Metadata, and Object Identity",
        "catalog",
        "catalog.system_views_and_statistics_surface",
        "System views and statistics surface",
        "system_views",
        "emulation",
        "Compare monitoring, metadata, and statistics views that users or donor tools expect to query directly.",
        (
            "docs/specifications/24_Catalog_Model_and_Virtual_Overlays/EMULATED_STATS_MAPPING.md",
            "docs/specifications/37_Statistics_Metadata_and_Schema_DDL/README.md",
        ),
    ),
    Feature(
        7,
        "Types and Value Semantics",
        "types",
        "types.native_scalar_and_temporal_surface",
        "Native scalar and temporal type surface",
        "native_types",
        "capability",
        "Compare the breadth of native scalar, numeric, binary, UUID, and temporal type support.",
        (
            "docs/specifications/14_Base_Scalar_Types/README.md",
            "docs/specifications/14_Base_Scalar_Types/EMULATED_SCALAR_TYPE_MATRIX.md",
        ),
    ),
    Feature(
        7,
        "Types and Value Semantics",
        "types",
        "types.complex_type_and_specialized_value_surface",
        "Complex-type and specialized-value surface",
        "complex_types",
        "capability",
        "Compare arrays, composites, variants, JSON-like, BSON-like, vector-like, and other non-scalar value carriers.",
        (
            "docs/specifications/15_Complex_Types/README.md",
            "docs/specifications/15_Complex_Types/TYPE_IO_AND_ERROR_SEMANTICS.md",
        ),
    ),
    Feature(
        7,
        "Types and Value Semantics",
        "types",
        "types.custom_domains_and_user_defined_type_control_plane",
        "Custom domains and user-defined type control plane",
        "domains",
        "capability",
        "Compare whether the platform exposes a first-class control plane for custom datatypes or domain-like user-defined value contracts.",
        (
            "docs/specifications/15_Complex_Types/DOMAIN_DDL_AND_CATALOG.md",
            "docs/specifications/15_Complex_Types/SYSTEM_DOMAIN_UUID_REGISTRY.md",
        ),
    ),
    Feature(
        7,
        "Types and Value Semantics",
        "types",
        "types.domain_security_masking_and_encryption_metadata",
        "Domain security, masking, and encryption metadata",
        "domain_security",
        "capability",
        "Compare whether datatype or domain definitions can carry masking, audit, and encryption metadata as part of the type contract.",
        (
            "docs/specifications/19_Security_Model/DOMAIN_SECURITY_PAYLOAD_AND_PERMISSION_MASK_MODEL.md",
            "docs/specifications/19_Security_Model/DOMAIN_SECURITY_MASKING_ENCRYPTION_AND_AUDIT_MODEL.md",
        ),
    ),
    Feature(
        7,
        "Types and Value Semantics",
        "types",
        "types.donor_type_mapping_and_emulation_surface",
        "Donor type-mapping and emulation surface",
        "type_mapping",
        "emulation",
        "Compare donor-facing type compatibility against ScratchBird's emulated type and mapping surfaces.",
        (
            "docs/specifications/15_Complex_Types/EMULATED_COMPLEX_TYPE_MATRIX.md",
            "docs/specifications/14_Base_Scalar_Types/EMULATED_SCALAR_TYPE_MATRIX.md",
            "docs/specifications/28_Parser_Implementations/SCHEMA_VISIBILITY_AND_TRANSLATION_MATRIX.md",
        ),
    ),
    Feature(
        8,
        "DDL and Schema Lifecycle",
        "ddl",
        "ddl.object_lifecycle_and_dependency_publication",
        "Object lifecycle and dependency publication",
        "object_lifecycle",
        "capability",
        "Compare create, alter, drop, rename, dependency replacement, and publication boundaries.",
        (
            "docs/specifications/21_V3_Dialect_Surface/README.md",
            "docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_INVALIDATION_DEPENDENCY_GRAPH_AND_SELF_CHECK.md",
        ),
    ),
    Feature(
        8,
        "DDL and Schema Lifecycle",
        "ddl",
        "ddl.donor_schema_mapping_and_search_path_behavior",
        "Donor schema mapping and search-path behavior",
        "schema_mapping",
        "emulation",
        "Compare donor database, schema, and search-path semantics against ScratchBird's emulated-schema-root model.",
        (
            "docs/specifications/24_Catalog_Model_and_Virtual_Overlays/SCHEMA_HOME_AND_SEARCH_PATH_SEMANTICS.md",
            "docs/specifications/29_Listener_and_Server_Orchestration/CONNECTION_AND_SESSION_LIFECYCLE.md",
        ),
    ),
    Feature(
        9,
        "DML, Query, and Planner Surface",
        "dml",
        "dml.query_and_mutation_surface",
        "Query and mutation surface",
        "query_mutation",
        "capability",
        "Compare selection, mutation, returning behavior, and the breadth of the primary relational or query surface.",
        (
            "docs/specifications/21_V3_Dialect_Surface/README.md",
            "docs/specifications/36_Query_Rewrite_and_Planner/README.md",
        ),
    ),
    Feature(
        9,
        "DML, Query, and Planner Surface",
        "dml",
        "dml.donor_query_semantics_emulation_surface",
        "Donor query-semantics emulation surface",
        "donor_query_emulation",
        "emulation",
        "Compare donor-facing SQL or query semantics against the shipped or specified ScratchBird emulation surface.",
        (
            "docs/specifications/28_Parser_Implementations/README.md",
            "docs/specifications/29_Listener_and_Server_Orchestration/README.md",
        ),
    ),
    Feature(
        9,
        "DML, Query, and Planner Surface",
        "planner",
        "planner.path_taxonomy_plan_control_and_explain_surface",
        "Planner-path taxonomy, plan control, and explain surface",
        "planner",
        "capability",
        "Compare path taxonomy, explainability, typed planner metrics, and plan-control visibility.",
        (
            "docs/specifications/18_Index_Framework/INDEX_PLANNER_PATH_TAXONOMY_AND_EXACTNESS.md",
            "docs/specifications/31_Conformance_Performance_and_Reliability_Gates/OPTIMIZER_PARITY_TRACE_AND_PLAN_EXPLANATION_CERTIFICATION_MODEL.md",
        ),
    ),
    Feature(
        10,
        "Indexing and Access Paths",
        "indexes",
        "indexes.runtime_family_surface",
        "Index runtime-family surface",
        "index_runtime",
        "capability",
        "Compare the breadth of admitted index runtime families rather than treating indexing as one generic feature.",
        (
            "docs/specifications/18_Index_Framework/README.md",
            "docs/specifications/18_Index_Framework/INDEX_RUNTIME_TAXONOMY_AND_ALIAS_LOWERING.md",
        ),
    ),
    Feature(
        10,
        "Indexing and Access Paths",
        "indexes",
        "indexes.donor_index_mapping_and_alias_lowering",
        "Donor index mapping and alias lowering",
        "index_mapping",
        "emulation",
        "Compare donor index-name and index-behavior mapping against ScratchBird's canonical index-family lowering model.",
        (
            "docs/specifications/18_Index_Framework/INDEX_RUNTIME_TAXONOMY_AND_ALIAS_LOWERING.md",
            "docs/specifications/18_Index_Framework/DIALECT_COMPATIBILITY_MATRIX.md",
        ),
    ),
    Feature(
        10,
        "Indexing and Access Paths",
        "indexes",
        "indexes.specialized_index_families",
        "Specialized index families",
        "specialized_indexes",
        "capability",
        "Compare text, spatial, vector, graph, summary, and donor-specific specialized index surfaces.",
        (
            "docs/specifications/18_Index_Framework/README.md",
            "docs/specifications/18_Index_Framework/GENERALIZED_SEARCH_AND_SPATIAL_PLANNER_SPEC.md",
            "docs/specifications/18_Index_Framework/VECTOR_ANN_PLANNER_SPEC.md",
        ),
    ),
    Feature(
        10,
        "Indexing and Access Paths",
        "indexes",
        "indexes.mga_safe_publication_reclaim_and_metrics",
        "MGA-safe publication, reclaim, and metrics",
        "index_mga",
        "capability",
        "Compare whether index publication, reclaim, and observability remain subordinate to MGA-visible row truth.",
        (
            "docs/specifications/18_Index_Framework/INDEX_MGA_PUBLICATION_AND_RECLAIM.md",
            "docs/specifications/18_Index_Framework/INDEX_FAMILY_NATIVE_METRICS_PACKET_CONTRACT.md",
        ),
    ),
    Feature(
        11,
        "Callable, Procedural, and Trigger Surface",
        "functions",
        "functions.builtin_function_and_operator_surface",
        "Built-in function and operator surface",
        "functions",
        "capability",
        "Compare built-in callable surface, scalar operators, casts, aggregates, and donor-facing function families.",
        (
            "docs/specifications/17_Functions_and_Procedures/README.md",
            "docs/specifications/13_Operator_Model_and_Coercion/README.md",
        ),
    ),
    Feature(
        11,
        "Callable, Procedural, and Trigger Surface",
        "procedural",
        "procedural.server_side_code_trigger_and_routine_surface",
        "Server-side code, trigger, and routine surface",
        "server_side_code",
        "capability",
        "Compare procedures, triggers, stored routines, and the surrounding execution model.",
        (
            "docs/specifications/17_Functions_and_Procedures/README.md",
            "docs/specifications/21_V3_Dialect_Surface/NATIVE_PSQL_TSQL_LANGUAGE_DEFINITION.md",
        ),
    ),
    Feature(
        11,
        "Callable, Procedural, and Trigger Surface",
        "procedural",
        "procedural.sblr_bytecode_runtime_and_compilation_lane",
        "SBLR bytecode runtime and compilation lane",
        "sblr",
        "structural",
        "Compare donor routine execution against ScratchBird's native SBLR bytecode runtime with LLVM-backed compilation lanes.",
        (
            "docs/specifications/22_SBLR_Canonical_Model_and_Opcodes/README.md",
            "docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_SCHEMA_SBLR_EXECUTION_ARTIFACTS.md",
        ),
    ),
    Feature(
        12,
        "Security, Identity, and Policy Surface",
        "security",
        "security.identity_authentication_and_provider_chain",
        "Identity, authentication, and provider chain",
        "authn",
        "capability",
        "Compare user, role, authentication, and provider-chain behavior exposed to clients and operators.",
        (
            "docs/specifications/19_Security_Model/README.md",
            "docs/specifications/19_Security_Model/ENGINE_AUTHENTICATION_HARDENING_AND_MANAGER_OPTION_SPEC.md",
        ),
    ),
    Feature(
        12,
        "Security, Identity, and Policy Surface",
        "security",
        "security.row_column_domain_masking_and_policy_pipeline",
        "Row, column, domain, masking, and policy pipeline",
        "policy_pipeline",
        "capability",
        "Compare whether disclosure control spans row policy, column policy, domain security, masking, and audit in one deterministic pipeline.",
        (
            "docs/specifications/19_Security_Model/ROW_COLUMN_DOMAIN_SECURITY_MASKING_EVALUATION_ORDER_MODEL.md",
            "docs/specifications/19_Security_Model/PRIVILEGE_GRAPH_ROW_COLUMN_DOMAIN_SECURITY_AND_MASKING_MODEL.md",
        ),
    ),
    Feature(
        12,
        "Security, Identity, and Policy Surface",
        "security",
        "security.definer_invoker_and_schema_sandbox_security",
        "Definer/invoker and schema-sandbox security",
        "sandbox_security",
        "capability",
        "Compare definer or invoker behavior, schema sandboxing, and policy resolution at the execution boundary.",
        (
            "docs/specifications/19_Security_Model/SECURITY_DEFINER_RLS_AND_EMULATED_SCHEMA_SANDBOX_MODEL.md",
            "docs/specifications/19_Security_Model/ROW_COLUMN_DOMAIN_MASKING_AND_SANDBOX_SECURITY_MODEL.md",
        ),
    ),
    Feature(
        13,
        "Parser, Protocol, and Emulation Front Door",
        "parser",
        "parser.engine_parser_separation_and_sblr_lowering",
        "Engine/parser separation and SBLR lowering",
        "parser_separation",
        "structural",
        "Compare integrated donor parser models against ScratchBird's separate parser families and dialect-local lowering to SBLR.",
        (
            "docs/specifications/28_Parser_Implementations/README.md",
            "docs/specifications/29_Listener_and_Server_Orchestration/PARSER_AGENT_EXECUTABLE_COMPOSITION_AND_RUNTIME_STACK_MODEL.md",
        ),
    ),
    Feature(
        13,
        "Parser, Protocol, and Emulation Front Door",
        "protocol",
        "protocol.dedicated_donor_front_door",
        "Dedicated donor front door",
        "front_door",
        "emulation",
        "Compare donor wire protocol or API front doors against the shipped or specified ScratchBird parser-adapter entry points.",
        (
            "docs/specifications/28_Parser_Implementations/README.md",
            "docs/specifications/29_Listener_and_Server_Orchestration/README.md",
        ),
    ),
    Feature(
        13,
        "Parser, Protocol, and Emulation Front Door",
        "protocol",
        "protocol.prepared_parameter_error_and_result_mapping",
        "Prepared, parameter, error, and result mapping",
        "prepared_error",
        "emulation",
        "Compare prepared statements, parameter binding, error tuples, and donor-facing result framing.",
        (
            "docs/specifications/28_Parser_Implementations/NORMATIVE_WIRE_PROTOCOL_MYSQL_CHECKLIST.md",
            "docs/specifications/28_Parser_Implementations/NORMATIVE_WIRE_PROTOCOL_POSTGRESQL_CHECKLIST.md",
            "docs/specifications/28_Parser_Implementations/ERROR_MAPPING_AND_DIAGNOSTICS.md",
        ),
    ),
    Feature(
        13,
        "Parser, Protocol, and Emulation Front Door",
        "protocol",
        "protocol.donor_tool_and_upstream_harness_compatibility",
        "Donor-tool and upstream-harness compatibility",
        "tool_harness",
        "emulation",
        "Compare donor-native client and upstream harness execution against ScratchBird's current bounded proof or specified target.",
        (
            "tests/compatibility/README.md",
            "tests/compatibility/postgresql/README.md",
            "tests/compatibility/mysql/README.md",
            "tests/compatibility/firebird/README.md",
        ),
    ),
    Feature(
        14,
        "Operations, Introspection, and Data Movement",
        "operations",
        "operations.runtime_modes_operator_views_and_utilities",
        "Runtime modes, operator views, and utilities",
        "runtime_modes",
        "capability",
        "Compare embedded, server, listener, and operator-visible runtime modes and utilities.",
        (
            "docs/specifications/25_Runtime_Modes/README.md",
            "docs/specifications/30_Client_Tooling/README.md",
        ),
    ),
    Feature(
        14,
        "Operations, Introspection, and Data Movement",
        "operations",
        "operations.mga_diagnostics_and_introspection",
        "MGA diagnostics and introspection",
        "mga_diag",
        "capability",
        "Compare introspection surfaces that expose transaction, sweep, lineage, and derivative-lane health.",
        (
            "docs/specifications/20_Diagnostics_Audit_and_Observability/MGA_OBSERVABILITY_AND_OPERATOR_DIAGNOSTICS.md",
            "docs/specifications/20_Diagnostics_Audit_and_Observability/PAGE_WALKER_AND_REPAIR.md",
        ),
    ),
    Feature(
        14,
        "Operations, Introspection, and Data Movement",
        "data_movement",
        "data_movement.backup_restore_bulk_import_export",
        "Backup, restore, bulk, import, and export",
        "backup_bulk",
        "capability",
        "Compare backup, restore, dump, load, import, export, and bulk data paths.",
        (
            "docs/specifications/39_Backup_Restore_and_Bulk_Data_Paths/README.md",
            "docs/specifications/25_Runtime_Modes/README.md",
        ),
    ),
    Feature(
        14,
        "Operations, Introspection, and Data Movement",
        "data_movement",
        "data_movement.cdc_migration_replay_and_change_capture",
        "CDC, migration, replay, and change capture",
        "cdc",
        "capability",
        "Compare change capture, migration, replay, and commit-ordered export surfaces.",
        (
            "docs/specifications/29_Listener_and_Server_Orchestration/PROXY_MIGRATION_FOR_NON_REPLICATING_DONORS_MODEL.md",
            "docs/specifications/39_Backup_Restore_and_Bulk_Data_Paths/README.md",
        ),
    ),
    Feature(
        15,
        "Replication, Distribution, and Availability",
        "replication",
        "replication.cluster_identity_fencing_and_routing_epoch",
        "Cluster identity, fencing, and routing epoch",
        "cluster_fencing",
        "capability",
        "Compare cluster identity, routing epoch, write fencing, and refusal visibility.",
        (
            "docs/specifications/20_Diagnostics_Audit_and_Observability/CLUSTER_IDENTITY_SECURITY_QUORUM_AND_WRITE_FENCE_OBSERVABILITY_MODEL.md",
            "docs/specifications/20_Diagnostics_Audit_and_Observability/CLUSTER_ROUTING_FENCING_AND_SHARD_COMMIT_LOG_OBSERVABILITY.md",
        ),
    ),
    Feature(
        15,
        "Replication, Distribution, and Availability",
        "replication",
        "replication.sharding_partitioning_and_topology_control",
        "Sharding, partitioning, and topology control",
        "topology",
        "capability",
        "Compare sharding, partitioning, placement, and topology-management surfaces.",
        (
            "docs/specifications/24_Catalog_Model_and_Virtual_Overlays/SHARDING_CATALOG_SCHEMA.md",
            "docs/specifications/41_Platform_Interface_and_Lifecycle_Management/ENGINE_PROFILE_ENUM_FREEZE_AND_RUNTIME_CAPABILITY_CONTRACT.md",
        ),
    ),
    Feature(
        15,
        "Replication, Distribution, and Availability",
        "replication",
        "replication.apply_lag_failover_and_observability",
        "Apply lag, failover, and observability",
        "apply_lag",
        "capability",
        "Compare replication apply, failover, lag, and operator-visible availability diagnostics.",
        (
            "docs/specifications/20_Diagnostics_Audit_and_Observability/CLUSTER_ROUTING_FENCING_AND_SHARD_COMMIT_LOG_OBSERVABILITY.md",
            "docs/specifications/29_Listener_and_Server_Orchestration/NORMATIVE_LISTENER_ONE_WAY_AND_BIDIRECTIONAL_REPLICATION_CHECKLIST.md",
        ),
    ),
    Feature(
        16,
        "Specialized and Non-Relational Surface Areas",
        "specialized",
        "specialized.document_surface",
        "Document surface",
        "document",
        "specialized",
        "Compare document-native storage, nested-path behavior, and document-query or aggregation surfaces.",
        (
            "docs/specifications/24_Catalog_Model_and_Virtual_Overlays/EMULATED_CATALOG_ANALYSIS_MONGODB.md",
            "docs/specifications/15_Complex_Types/EMULATED_COMPLEX_TYPE_MATRIX.md",
        ),
    ),
    Feature(
        16,
        "Specialized and Non-Relational Surface Areas",
        "specialized",
        "specialized.graph_surface",
        "Graph surface",
        "graph",
        "specialized",
        "Compare graph-native modeling, traversal, path, and graph-query surfaces.",
        (
            "docs/specifications/18_Index_Framework/NEO4J_TEXT_SPEC.md",
            "docs/specifications/18_Index_Framework/NEO4J_RANGE_SPEC.md",
            "docs/specifications/18_Index_Framework/NEO4J_VECTOR_SPEC.md",
        ),
    ),
    Feature(
        16,
        "Specialized and Non-Relational Surface Areas",
        "specialized",
        "specialized.vector_ann_surface",
        "Vector and ANN surface",
        "vector",
        "specialized",
        "Compare vector storage, ANN indexing, and similarity-search surfaces.",
        (
            "docs/specifications/18_Index_Framework/VECTOR_ANN_PLANNER_SPEC.md",
            "docs/specifications/18_Index_Framework/IVF_VARIANTS_SPEC.md",
            "docs/specifications/18_Index_Framework/HNSW_SPEC.md",
        ),
    ),
    Feature(
        16,
        "Specialized and Non-Relational Surface Areas",
        "specialized",
        "specialized.key_value_and_data_structure_surface",
        "Key-value and data-structure surface",
        "kv",
        "specialized",
        "Compare key-value commands, collection-like structures, and non-relational atomic mutation surfaces.",
        (
            "docs/specifications/18_Index_Framework/REDIS_DATA_STRUCTURES_SPEC.md",
            "docs/specifications/28_Parser_Implementations/NORMATIVE_WIRE_PROTOCOL_REDIS_RESP_CHECKLIST.md",
        ),
    ),
    Feature(
        16,
        "Specialized and Non-Relational Surface Areas",
        "specialized",
        "specialized.time_series_surface",
        "Time-series surface",
        "time_series",
        "specialized",
        "Compare time-series writes, retention, windows, and downsampling-visible behavior.",
        (
            "docs/specifications/36_Query_Rewrite_and_Planner/README.md",
            "docs/specifications/15_Complex_Types/README.md",
        ),
    ),
    Feature(
        16,
        "Specialized and Non-Relational Surface Areas",
        "specialized",
        "specialized.history_temporal_and_lineage_query_surface",
        "History, temporal, and lineage-query surface",
        "history",
        "specialized",
        "Compare temporal queryability, retained history, lineage inspection, and immutable-history style behavior.",
        (
            "docs/specifications/20_Diagnostics_Audit_and_Observability/LOGICAL_ROW_UUID_CLUSTER_TRACKING_AND_VERSION_LINEAGE_OBSERVABILITY_MODEL.md",
            "docs/specifications/42_Failure_Model_and_Fault_Tolerance/FAILURE_MODEL_AND_FAULT_CLASSIFICATION.md",
        ),
    ),
)


def load_reference_matrix() -> dict[str, dict[str, str]]:
    rows: dict[str, dict[str, str]] = {}
    for line in REFERENCE_DOC.read_text(encoding="utf-8").splitlines():
        if not line.startswith("|"):
            continue
        cells = [cell.strip() for cell in line.strip().split("|")[1:-1]]
        if len(cells) != 7 or cells[0] == "Rank" or not cells[0].isdigit():
            continue
        rows[cells[1]] = {
            "rank": cells[0],
            "phase": cells[2],
            "role": cells[3],
            "why_included": cells[4],
            "what_to_study": cells[5],
            "what_not_to_copy": cells[6],
        }
    return rows


def discover_reference_dir(rank: int) -> Path:
    pattern = f"{rank:02d}_*"
    matches = sorted(REFERENCE_ROOT.glob(f"*/{pattern}"))
    if not matches:
        raise FileNotFoundError(f"Could not locate formal reference directory for rank {rank}")
    return matches[0]


def read_source_url(readme_path: Path) -> str:
    for line in readme_path.read_text(encoding="utf-8").splitlines():
        if line.startswith("- source:"):
            parts = line.split("`")
            if len(parts) >= 3:
                return parts[1]
            return line.split(":", 1)[1].strip()
    return ""


def load_existing_manifest() -> list[ReportSeed]:
    matrix = load_reference_matrix()
    seeds: list[ReportSeed] = []
    with MANIFEST_PATH.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            donor = row["donor"]
            if donor in NON_DATABASE_DONORS:
                continue
            rank = int(row["rank"])
            report_file = row["report_file"]
            slug = report_file.split("_vs_scratchbird_")[0]
            ref_dir = discover_reference_dir(rank)
            readme_path = ref_dir / "README.md"
            seeds.append(
                ReportSeed(
                    donor=donor,
                    rank=rank,
                    phase=matrix[donor]["phase"],
                    role=matrix[donor]["role"],
                    slug=slug,
                    report_file=report_file,
                    why_included=matrix[donor]["why_included"],
                    what_to_study=matrix[donor]["what_to_study"],
                    what_not_to_copy=matrix[donor]["what_not_to_copy"],
                    ref_dir=ref_dir,
                    ref_readme=ref_dir.relative_to(REPO_ROOT).as_posix() + "/README.md",
                    ref_source=ref_dir.relative_to(REPO_ROOT).as_posix() + "/SOURCE.txt",
                    source_url=read_source_url(readme_path),
                )
            )
    return seeds


def donor_has_sql(seed: ReportSeed) -> bool:
    return seed.slug in SQL_DONORS


def donor_has_document(seed: ReportSeed) -> bool:
    return seed.slug in DOCUMENT_DONORS


def donor_has_graph(seed: ReportSeed) -> bool:
    return seed.slug in GRAPH_DONORS


def donor_has_vector(seed: ReportSeed) -> bool:
    return seed.slug in VECTOR_DONORS


def donor_has_search(seed: ReportSeed) -> bool:
    return seed.slug in SEARCH_DONORS


def donor_has_kv(seed: ReportSeed) -> bool:
    return seed.slug in KV_DONORS


def donor_has_time_series(seed: ReportSeed) -> bool:
    return seed.slug in TIME_SERIES_DONORS


def donor_has_history(seed: ReportSeed) -> bool:
    return seed.slug in HISTORY_DONORS


def donor_is_distributed(seed: ReportSeed) -> bool:
    return seed.slug in DISTRIBUTED_DONORS


def donor_has_replication(seed: ReportSeed) -> bool:
    return seed.slug in REPLICATION_DONORS


def donor_has_procedural(seed: ReportSeed) -> bool:
    return seed.slug in PROCEDURAL_DONORS


def is_family_adjacent(seed: ReportSeed) -> bool:
    return seed.slug in (MYSQL_FAMILY | POSTGRES_FAMILY) and seed.slug not in DIRECT_DIALECTS


def donor_state(seed: ReportSeed, feature: Feature) -> State:
    kind = feature.kind

    if kind == "product_model":
        return State("DATABASE_ENGINE", "full", "REFERENCE_PACK_BACKED")
    if kind == "oversized_value":
        if donor_has_sql(seed) or donor_has_document(seed) or donor_has_search(seed) or donor_has_time_series(seed):
            return State("DONOR_LARGE_VALUE_SURFACE", "full", "REFERENCE_PACK_BACKED")
        if donor_has_kv(seed) or donor_has_graph(seed) or donor_has_vector(seed):
            return State("DONOR_SPECIFIC_VALUE_MODEL", "partial", "REFERENCE_PACK_BACKED")
        return State("ABSENT", "absent", "REFERENCE_PACK_BACKED")
    if kind == "txn_context":
        if seed.slug == "firebirdsql":
            return State("ALWAYS_IN_TRANSACTION", "full", "REFERENCE_PACK_BACKED")
        if donor_has_sql(seed):
            return State("EXPLICIT_OR_AUTOCOMMIT_TRANSACTION_MODEL", "full", "REFERENCE_PACK_BACKED")
        if donor_has_document(seed) or donor_has_kv(seed) or donor_has_graph(seed):
            return State("COMMAND_OR_SESSION_SCOPED_TRANSACTION_MODEL", "partial", "REFERENCE_PACK_BACKED")
        return State("OUT_OF_SCOPE", "out", "REFERENCE_PACK_BACKED")
    if kind == "version_lineage":
        if seed.slug == "firebirdsql":
            return State("MGA_BACK_VERSIONED", "full", "REFERENCE_PACK_BACKED")
        if donor_has_sql(seed):
            return State("MVCC_OR_UNDO_VARIANT", "full", "REFERENCE_PACK_BACKED")
        if donor_has_document(seed) or donor_has_search(seed) or donor_has_time_series(seed):
            return State("ENGINE_SPECIFIC_VERSIONING", "partial", "REFERENCE_PACK_BACKED")
        return State("ABSENT", "absent", "REFERENCE_PACK_BACKED")
    if kind == "recovery_model":
        if seed.slug == "firebirdsql":
            return State("PRIMARY_STATE_TRUTH_WITHOUT_WAL_AUTHORITY", "full", "REFERENCE_PACK_BACKED")
        if donor_has_sql(seed) or donor_has_document(seed) or donor_has_search(seed) or donor_has_time_series(seed):
            return State("LOG_OR_WAL_PRIMARY_RECOVERY_MODEL", "full", "REFERENCE_PACK_BACKED")
        return State("ENGINE_SPECIFIC_RECOVERY_MODEL", "partial", "REFERENCE_PACK_BACKED")
    if kind == "cluster_ready":
        if donor_is_distributed(seed) or donor_has_replication(seed):
            return State("DISTRIBUTED_OR_REPLICATED_PLATFORM_MODEL", "full", "REFERENCE_PACK_BACKED")
        return State("SINGLE_NODE_PRIMARY_PLATFORM_MODEL", "partial", "REFERENCE_PACK_BACKED")
    if kind == "uuid_identity":
        if donor_has_sql(seed) or donor_has_document(seed) or donor_has_graph(seed) or donor_has_vector(seed) or donor_has_search(seed) or donor_has_time_series(seed):
            return State("ENGINE_OBJECT_IDENTITY_MODEL", "full", "REFERENCE_PACK_BACKED")
        if donor_has_kv(seed):
            return State("KEYSPACE_OR_RECORD_IDENTITY_MODEL", "partial", "REFERENCE_PACK_BACKED")
        return State("ABSENT", "absent", "REFERENCE_PACK_BACKED")
    if kind == "schema_tree":
        if donor_has_sql(seed):
            return State("FLAT_SCHEMA_OR_DATABASE_NAMESPACE_MODEL", "full", "REFERENCE_PACK_BACKED")
        if donor_has_document(seed) or donor_has_graph(seed) or donor_has_kv(seed):
            return State("DATABASE_OR_NAMESPACE_SCOPING_MODEL", "partial", "REFERENCE_PACK_BACKED")
        return State("OUT_OF_SCOPE", "out", "REFERENCE_PACK_BACKED")
    if kind == "catalog_overlay":
        if donor_has_sql(seed) or donor_has_document(seed) or donor_has_graph(seed) or donor_has_search(seed) or donor_has_time_series(seed):
            return State("NATIVE_SYSTEM_CATALOG_OR_METADATA_SURFACE", "full", "REFERENCE_PACK_BACKED")
        if donor_has_kv(seed) or donor_has_vector(seed):
            return State("DONOR_METADATA_SURFACE", "partial", "REFERENCE_PACK_BACKED")
        return State("OUT_OF_SCOPE", "out", "REFERENCE_PACK_BACKED")
    if kind == "metadata_publication":
        if donor_has_sql(seed) or donor_has_document(seed) or donor_has_graph(seed) or donor_has_search(seed) or donor_has_time_series(seed):
            return State("DONOR_METADATA_PUBLICATION_MODEL", "full", "REFERENCE_PACK_BACKED")
        return State("OUT_OF_SCOPE", "out", "REFERENCE_PACK_BACKED")
    if kind == "system_views":
        if donor_has_sql(seed) or donor_has_document(seed) or donor_has_search(seed):
            return State("NATIVE_SYSTEM_VIEW_SURFACE", "full", "REFERENCE_PACK_BACKED")
        if donor_has_graph(seed) or donor_has_vector(seed) or donor_has_kv(seed):
            return State("DONOR_INTROSPECTION_SURFACE", "partial", "REFERENCE_PACK_BACKED")
        return State("OUT_OF_SCOPE", "out", "REFERENCE_PACK_BACKED")
    if kind == "native_types":
        if donor_has_sql(seed) or donor_has_document(seed) or donor_has_graph(seed) or donor_has_vector(seed) or donor_has_search(seed) or donor_has_time_series(seed):
            return State("NATIVE_TYPE_SURFACE", "full", "REFERENCE_PACK_BACKED")
        if donor_has_kv(seed):
            return State("LIMITED_NATIVE_TYPE_SURFACE", "partial", "REFERENCE_PACK_BACKED")
        return State("OUT_OF_SCOPE", "out", "REFERENCE_PACK_BACKED")
    if kind == "complex_types":
        if donor_has_sql(seed) or donor_has_document(seed) or donor_has_graph(seed) or donor_has_vector(seed) or donor_has_search(seed):
            return State("COMPLEX_OR_SPECIALIZED_VALUE_SURFACE", "full", "REFERENCE_PACK_BACKED")
        if donor_has_time_series(seed) or donor_has_kv(seed):
            return State("PARTIAL_SPECIALIZED_VALUE_SURFACE", "partial", "REFERENCE_PACK_BACKED")
        return State("OUT_OF_SCOPE", "out", "REFERENCE_PACK_BACKED")
    if kind == "domains":
        if donor_has_sql(seed):
            return State("DOMAIN_OR_USER_DEFINED_TYPE_SURFACE", "full", "REFERENCE_PACK_BACKED")
        return State("ABSENT", "absent", "REFERENCE_PACK_BACKED")
    if kind == "domain_security":
        if donor_has_sql(seed):
            return State("TYPE_OR_DOMAIN_SECURITY_SURFACE", "partial", "REFERENCE_PACK_BACKED")
        return State("ABSENT", "absent", "REFERENCE_PACK_BACKED")
    if kind == "type_mapping":
        if donor_has_sql(seed) or seed.slug in TARGETED_SPECIALIZED:
            return State("DONOR_TYPE_SURFACE", "full", "REFERENCE_PACK_BACKED")
        return State("OUT_OF_SCOPE", "out", "REFERENCE_PACK_BACKED")
    if kind == "object_lifecycle":
        if donor_has_sql(seed) or donor_has_document(seed) or donor_has_graph(seed) or donor_has_search(seed) or donor_has_time_series(seed):
            return State("NATIVE_OBJECT_LIFECYCLE_SURFACE", "full", "REFERENCE_PACK_BACKED")
        return State("PARTIAL_DDL_OR_CONTROL_PLANE_SURFACE", "partial", "REFERENCE_PACK_BACKED")
    if kind == "schema_mapping":
        if donor_has_sql(seed):
            return State("DONOR_SCHEMA_AND_SEARCH_PATH_MODEL", "full", "REFERENCE_PACK_BACKED")
        if donor_has_document(seed) or donor_has_graph(seed) or donor_has_kv(seed):
            return State("DONOR_DATABASE_OR_NAMESPACE_BINDING_MODEL", "partial", "REFERENCE_PACK_BACKED")
        return State("OUT_OF_SCOPE", "out", "REFERENCE_PACK_BACKED")
    if kind == "query_mutation":
        if donor_has_sql(seed) or donor_has_document(seed) or donor_has_graph(seed) or donor_has_search(seed) or donor_has_time_series(seed):
            return State("DONOR_QUERY_AND_MUTATION_SURFACE", "full", "REFERENCE_PACK_BACKED")
        if donor_has_kv(seed):
            return State("DONOR_COMMAND_MUTATION_SURFACE", "partial", "REFERENCE_PACK_BACKED")
        return State("OUT_OF_SCOPE", "out", "REFERENCE_PACK_BACKED")
    if kind == "donor_query_emulation":
        if donor_has_sql(seed) or seed.slug in TARGETED_SPECIALIZED:
            return State("DONOR_QUERY_SEMANTICS", "full", "REFERENCE_PACK_BACKED")
        return State("OUT_OF_SCOPE", "out", "REFERENCE_PACK_BACKED")
    if kind == "planner":
        if donor_has_sql(seed) or donor_has_search(seed) or donor_has_graph(seed) or donor_has_vector(seed):
            return State("DONOR_PLAN_AND_EXPLAIN_SURFACE", "full", "REFERENCE_PACK_BACKED")
        return State("PARTIAL_QUERY_PLANNING_SURFACE", "partial", "REFERENCE_PACK_BACKED")
    if kind == "index_runtime":
        if donor_has_sql(seed) or donor_has_document(seed) or donor_has_graph(seed) or donor_has_search(seed) or donor_has_vector(seed) or donor_has_time_series(seed):
            return State("DONOR_INDEX_FAMILY_SURFACE", "full", "REFERENCE_PACK_BACKED")
        return State("PARTIAL_INDEX_OR_LOOKUP_SURFACE", "partial", "REFERENCE_PACK_BACKED")
    if kind == "index_mapping":
        if donor_has_sql(seed) or seed.slug in TARGETED_SPECIALIZED:
            return State("DONOR_INDEX_NAME_AND_BEHAVIOR_SURFACE", "full", "REFERENCE_PACK_BACKED")
        return State("OUT_OF_SCOPE", "out", "REFERENCE_PACK_BACKED")
    if kind == "specialized_indexes":
        if donor_has_search(seed) or donor_has_graph(seed) or donor_has_vector(seed) or donor_has_document(seed) or donor_has_time_series(seed) or donor_has_sql(seed):
            return State("DONOR_SPECIALIZED_INDEX_SURFACE", "full", "REFERENCE_PACK_BACKED")
        return State("ABSENT", "absent", "REFERENCE_PACK_BACKED")
    if kind == "index_mga":
        if donor_has_sql(seed) or donor_has_document(seed) or donor_has_search(seed) or donor_has_graph(seed) or donor_has_vector(seed):
            return State("DONOR_INDEX_PUBLICATION_AND_METRICS_MODEL", "full", "REFERENCE_PACK_BACKED")
        return State("PARTIAL_INDEX_MAINTENANCE_MODEL", "partial", "REFERENCE_PACK_BACKED")
    if kind == "functions":
        if donor_has_sql(seed) or donor_has_document(seed) or donor_has_search(seed) or donor_has_time_series(seed):
            return State("DONOR_BUILTIN_FUNCTION_SURFACE", "full", "REFERENCE_PACK_BACKED")
        if donor_has_graph(seed) or donor_has_kv(seed) or donor_has_vector(seed):
            return State("DONOR_COMMAND_OR_SPECIALIZED_FUNCTION_SURFACE", "partial", "REFERENCE_PACK_BACKED")
        return State("OUT_OF_SCOPE", "out", "REFERENCE_PACK_BACKED")
    if kind == "server_side_code":
        if donor_has_procedural(seed):
            return State("DONOR_SERVER_SIDE_CODE_SURFACE", "full", "REFERENCE_PACK_BACKED")
        return State("ABSENT", "absent", "REFERENCE_PACK_BACKED")
    if kind == "sblr":
        return State("NO_SBLR_EQUIVALENT", "absent", "REFERENCE_PACK_BACKED")
    if kind == "authn":
        return State("DONOR_AUTHENTICATION_SURFACE", "full", "REFERENCE_PACK_BACKED")
    if kind == "policy_pipeline":
        if donor_has_sql(seed):
            return State("DONOR_PRIVILEGE_AND_POLICY_SURFACE", "full", "REFERENCE_PACK_BACKED")
        if donor_has_document(seed) or donor_has_graph(seed) or donor_has_search(seed) or donor_has_kv(seed):
            return State("DONOR_AUTHZ_SURFACE", "partial", "REFERENCE_PACK_BACKED")
        return State("OUT_OF_SCOPE", "out", "REFERENCE_PACK_BACKED")
    if kind == "sandbox_security":
        if donor_has_sql(seed):
            return State("DONOR_DEFINER_OR_SCHEMA_SECURITY_MODEL", "partial", "REFERENCE_PACK_BACKED")
        return State("ABSENT", "absent", "REFERENCE_PACK_BACKED")
    if kind == "parser_separation":
        return State("INTEGRATED_PARSER_AND_ENGINE_SURFACE", "full", "REFERENCE_PACK_BACKED")
    if kind == "front_door":
        if seed.slug in DIRECT_DIALECTS or is_family_adjacent(seed) or seed.slug in TARGETED_SPECIALIZED:
            return State("DONOR_NATIVE_FRONT_DOOR", "full", "REFERENCE_PACK_BACKED")
        return State("OUT_OF_SCOPE", "out", "REFERENCE_PACK_BACKED")
    if kind == "prepared_error":
        if seed.slug in MYSQL_FAMILY or seed.slug in POSTGRES_FAMILY or seed.slug == "firebirdsql":
            return State("DONOR_PREPARED_AND_ERROR_SURFACE", "full", "REFERENCE_PACK_BACKED")
        if donor_has_sql(seed) or seed.slug in TARGETED_SPECIALIZED:
            return State("DONOR_PARAMETER_AND_RESULT_SURFACE", "partial", "REFERENCE_PACK_BACKED")
        return State("OUT_OF_SCOPE", "out", "REFERENCE_PACK_BACKED")
    if kind == "tool_harness":
        if seed.slug in DIRECT_DIALECTS:
            return State("DONOR_TOOLS_AND_UPSTREAM_HARNESSES_EXIST", "full", "REFERENCE_PACK_BACKED")
        if is_family_adjacent(seed):
            return State("DONOR_TOOLS_EXIST", "partial", "REFERENCE_PACK_BACKED")
        return State("OUT_OF_SCOPE", "out", "REFERENCE_PACK_BACKED")
    if kind == "runtime_modes":
        return State("DONOR_RUNTIME_AND_OPERATOR_SURFACE", "full", "REFERENCE_PACK_BACKED")
    if kind == "mga_diag":
        if donor_has_sql(seed) or donor_has_document(seed) or donor_has_search(seed) or donor_has_graph(seed):
            return State("DONOR_INTROSPECTION_AND_DIAGNOSTICS_SURFACE", "full", "REFERENCE_PACK_BACKED")
        return State("PARTIAL_DIAGNOSTICS_SURFACE", "partial", "REFERENCE_PACK_BACKED")
    if kind == "backup_bulk":
        return State("DONOR_BACKUP_AND_BULK_SURFACE", "full", "REFERENCE_PACK_BACKED")
    if kind == "cdc":
        if donor_has_replication(seed) or donor_has_document(seed) or donor_has_search(seed):
            return State("DONOR_CHANGE_CAPTURE_OR_MIGRATION_SURFACE", "full", "REFERENCE_PACK_BACKED")
        if donor_has_sql(seed):
            return State("PARTIAL_MIGRATION_SURFACE", "partial", "REFERENCE_PACK_BACKED")
        return State("OUT_OF_SCOPE", "out", "REFERENCE_PACK_BACKED")
    if kind == "cluster_fencing":
        if donor_is_distributed(seed) or donor_has_replication(seed):
            return State("DONOR_CLUSTER_OR_ROUTING_SURFACE", "full", "REFERENCE_PACK_BACKED")
        return State("ABSENT", "absent", "REFERENCE_PACK_BACKED")
    if kind == "topology":
        if donor_is_distributed(seed):
            return State("DONOR_TOPOLOGY_CONTROL_SURFACE", "full", "REFERENCE_PACK_BACKED")
        if donor_has_replication(seed):
            return State("PARTIAL_TOPOLOGY_SURFACE", "partial", "REFERENCE_PACK_BACKED")
        return State("ABSENT", "absent", "REFERENCE_PACK_BACKED")
    if kind == "apply_lag":
        if donor_has_replication(seed) or donor_is_distributed(seed):
            return State("DONOR_REPLICATION_AND_FAILOVER_SURFACE", "full", "REFERENCE_PACK_BACKED")
        return State("ABSENT", "absent", "REFERENCE_PACK_BACKED")
    if kind == "document":
        if donor_has_document(seed):
            return State("DONOR_DOCUMENT_SURFACE", "full", "REFERENCE_PACK_BACKED")
        return State("ABSENT", "absent", "REFERENCE_PACK_BACKED")
    if kind == "graph":
        if donor_has_graph(seed):
            return State("DONOR_GRAPH_SURFACE", "full", "REFERENCE_PACK_BACKED")
        return State("ABSENT", "absent", "REFERENCE_PACK_BACKED")
    if kind == "vector":
        if donor_has_vector(seed):
            return State("DONOR_VECTOR_SURFACE", "full", "REFERENCE_PACK_BACKED")
        return State("ABSENT", "absent", "REFERENCE_PACK_BACKED")
    if kind == "kv":
        if donor_has_kv(seed):
            return State("DONOR_KEY_VALUE_OR_DATA_STRUCTURE_SURFACE", "full", "REFERENCE_PACK_BACKED")
        return State("ABSENT", "absent", "REFERENCE_PACK_BACKED")
    if kind == "time_series":
        if donor_has_time_series(seed):
            return State("DONOR_TIME_SERIES_SURFACE", "full", "REFERENCE_PACK_BACKED")
        return State("ABSENT", "absent", "REFERENCE_PACK_BACKED")
    if kind == "history":
        if donor_has_history(seed):
            return State("DONOR_HISTORY_AND_LINEAGE_SURFACE", "full", "REFERENCE_PACK_BACKED")
        return State("ABSENT", "absent", "REFERENCE_PACK_BACKED")

    raise ValueError(f"Unhandled donor-state kind: {kind}")


def scratchbird_state(seed: ReportSeed, feature: Feature, lane: str) -> State:
    kind = feature.kind
    is_target = lane == "SPECIFIED_TARGET_STATE"

    def current(code: str, coverage: str, evidence: str) -> State:
        return State(code, coverage, evidence)

    def target(code: str, coverage: str, evidence: str = "SPEC_BACKED") -> State:
        return State(code, coverage, evidence)

    if is_target and kind == "catalog_overlay":
        if donor_has_sql(seed) or seed.slug in TARGETED_SPECIALIZED or donor_has_document(seed) or donor_has_graph(seed) or donor_has_search(seed) or donor_has_time_series(seed) or donor_has_kv(seed) or donor_has_vector(seed):
            return target("PLANNED_DONOR_CATALOG_OVERLAY_PARITY", "full")
    if is_target and kind == "system_views":
        if donor_has_sql(seed) or seed.slug in TARGETED_SPECIALIZED or donor_has_document(seed) or donor_has_graph(seed) or donor_has_search(seed) or donor_has_time_series(seed) or donor_has_kv(seed) or donor_has_vector(seed):
            return target("PLANNED_DONOR_SYSTEM_VIEW_PARITY", "full")
    if is_target and kind == "type_mapping":
        if donor_has_sql(seed) or seed.slug in TARGETED_SPECIALIZED:
            return target("PLANNED_DONOR_TYPE_MAPPING_PARITY", "full")
    if is_target and kind == "schema_mapping":
        if donor_has_sql(seed) or donor_has_document(seed) or donor_has_graph(seed) or donor_has_kv(seed) or donor_has_search(seed) or donor_has_time_series(seed) or donor_has_vector(seed):
            return target("PLANNED_DONOR_SCHEMA_OR_NAMESPACE_MAPPING_PARITY", "full")
    if is_target and kind == "donor_query_emulation":
        if donor_has_sql(seed) or seed.slug in TARGETED_SPECIALIZED:
            return target("PLANNED_DONOR_QUERY_EMULATION_PARITY", "full")
    if is_target and kind == "front_door":
        return target("PLANNED_DONOR_FRONT_DOOR_PARITY", "full")
    if is_target and kind == "prepared_error":
        return target("PLANNED_DONOR_RESULT_AND_ERROR_PARITY", "full")
    if is_target and kind == "tool_harness":
        return target("PLANNED_DONOR_TOOL_AND_HARNESS_PARITY", "full")

    if kind == "product_model":
        return current("DATABASE_ENVIRONMENT", "full", "CODE_AND_SPEC_BACKED") if not is_target else target("DATABASE_ENVIRONMENT", "full", "CODE_AND_SPEC_BACKED")
    if kind == "oversized_value":
        return current("TOAST_FIRST_NATIVE", "full", "CODE_AND_SPEC_BACKED") if not is_target else target("TOAST_FIRST_NATIVE", "full", "CODE_AND_SPEC_BACKED")
    if kind == "txn_context":
        return current("ALWAYS_IN_TRANSACTION", "full", "CODE_AND_SPEC_BACKED") if not is_target else target("ALWAYS_IN_TRANSACTION", "full", "CODE_AND_SPEC_BACKED")
    if kind == "version_lineage":
        return current("MGA_BACK_VERSIONED", "full", "CODE_AND_SPEC_BACKED") if not is_target else target("MGA_BACK_VERSIONED", "full", "CODE_AND_SPEC_BACKED")
    if kind == "recovery_model":
        return current("MGA_PRIMARY_DERIVATIVE_SECONDARY", "full", "CODE_AND_SPEC_BACKED") if not is_target else target("MGA_PRIMARY_DERIVATIVE_SECONDARY", "full", "CODE_AND_SPEC_BACKED")
    if kind == "cluster_ready":
        return current("UUID_CLUSTER_SUBSTRATE_WITH_RETAINED_LINEAGE", "full", "CODE_AND_SPEC_BACKED") if not is_target else target("UUID_CLUSTER_SUBSTRATE_WITH_RETAINED_LINEAGE", "full", "CODE_AND_SPEC_BACKED")
    if kind == "uuid_identity":
        return current("UUIDV7_DATABASE_OBJECT_AND_ROW_IDENTITY", "full", "CODE_AND_SPEC_BACKED") if not is_target else target("UUIDV7_DATABASE_OBJECT_AND_ROW_IDENTITY", "full", "CODE_AND_SPEC_BACKED")
    if kind == "schema_tree":
        return current("RECURSIVE_SCHEMA_TREE_WITH_SANDBOX_ROOTS", "full", "CODE_AND_SPEC_BACKED") if not is_target else target("RECURSIVE_SCHEMA_TREE_WITH_SANDBOX_ROOTS", "full", "CODE_AND_SPEC_BACKED")
    if kind == "catalog_overlay":
        if seed.slug in DIRECT_DIALECTS:
            return current("VIRTUAL_OVERLAY_CATALOG_SHIPPED", "full", "CODE_AND_SPEC_BACKED") if not is_target else target("VIRTUAL_OVERLAY_CATALOG_SHIPPED", "full", "CODE_AND_SPEC_BACKED")
        if is_family_adjacent(seed):
            return current("FAMILY_ADJACENT_OVERLAY_MAPPING", "partial", "SPEC_BACKED") if not is_target else target("SPECIFIED_FAMILY_OVERLAY_MAPPING", "full")
        if seed.slug in TARGETED_SPECIALIZED:
            return current("TARGETED_OVERLAY_AND_METADATA_SURFACE", "partial", "SPEC_BACKED") if not is_target else target("SPECIFIED_TARGETED_OVERLAY_SURFACE", "partial")
        return current("REFERENCE_ONLY", "absent", "SPEC_BACKED") if not is_target else target("REFERENCE_ONLY", "absent")
    if kind == "metadata_publication":
        return current("COMMIT_BOUND_SCHEMA_EPOCH_PUBLICATION", "full", "CODE_AND_SPEC_BACKED") if not is_target else target("COMMIT_BOUND_SCHEMA_EPOCH_PUBLICATION", "full", "CODE_AND_SPEC_BACKED")
    if kind == "system_views":
        if seed.slug in DIRECT_DIALECTS:
            return current("EMULATED_SYSTEM_VIEW_SURFACE", "full", "CODE_AND_SPEC_BACKED") if not is_target else target("EMULATED_SYSTEM_VIEW_SURFACE", "full", "CODE_AND_SPEC_BACKED")
        if is_family_adjacent(seed) or seed.slug in TARGETED_SPECIALIZED:
            return current("PARTIAL_EMULATED_VIEW_SURFACE", "partial", "SPEC_BACKED") if not is_target else target("SPECIFIED_EMULATED_VIEW_SURFACE", "partial")
        return current("SCRATCHBIRD_NATIVE_INTROSPECTION", "partial", "CODE_AND_SPEC_BACKED") if not is_target else target("SCRATCHBIRD_NATIVE_INTROSPECTION", "partial", "CODE_AND_SPEC_BACKED")
    if kind == "native_types":
        return current("BROAD_NATIVE_TYPE_SYSTEM", "full", "CODE_AND_SPEC_BACKED") if not is_target else target("BROAD_NATIVE_TYPE_SYSTEM", "full", "CODE_AND_SPEC_BACKED")
    if kind == "complex_types":
        return current("BROAD_COMPLEX_TYPE_SYSTEM", "full", "CODE_AND_SPEC_BACKED") if not is_target else target("BROAD_COMPLEX_TYPE_SYSTEM", "full", "CODE_AND_SPEC_BACKED")
    if kind == "domains":
        return current("DOMAIN_CONTROL_PLANE_NATIVE", "full", "CODE_AND_SPEC_BACKED") if not is_target else target("DOMAIN_CONTROL_PLANE_NATIVE", "full", "CODE_AND_SPEC_BACKED")
    if kind == "domain_security":
        return current("DOMAIN_MASKING_AND_SECURITY_PAYLOAD", "full", "CODE_AND_SPEC_BACKED") if not is_target else target("DOMAIN_MASKING_AND_SECURITY_PAYLOAD", "full", "CODE_AND_SPEC_BACKED")
    if kind == "type_mapping":
        if seed.slug in DIRECT_DIALECTS:
            return current("DIRECT_DONOR_TYPE_MAPPING_SHIPPED", "partial", "CODE_AND_SPEC_BACKED") if not is_target else target("DIRECT_DONOR_TYPE_MAPPING_SPECIFIED", "full")
        if is_family_adjacent(seed):
            return current("FAMILY_TYPE_MAPPING_SPECIFIED", "partial", "SPEC_BACKED") if not is_target else target("FAMILY_TYPE_MAPPING_SPECIFIED", "full")
        if seed.slug in TARGETED_SPECIALIZED:
            return current("TARGETED_TYPE_MAPPING_SPECIFIED", "partial", "SPEC_BACKED") if not is_target else target("TARGETED_TYPE_MAPPING_SPECIFIED", "partial")
        return current("OUT_OF_SCOPE", "out", "SPEC_BACKED") if not is_target else target("OUT_OF_SCOPE", "out")
    if kind == "object_lifecycle":
        return current("NATIVE_OBJECT_LIFECYCLE_AND_PUBLICATION", "full", "CODE_AND_SPEC_BACKED") if not is_target else target("NATIVE_OBJECT_LIFECYCLE_AND_PUBLICATION", "full", "CODE_AND_SPEC_BACKED")
    if kind == "schema_mapping":
        if seed.slug in DIRECT_DIALECTS:
            return current("DIRECT_DONOR_SCHEMA_ROOT_MAPPING", "full", "CODE_AND_SPEC_BACKED") if not is_target else target("DIRECT_DONOR_SCHEMA_ROOT_MAPPING", "full", "CODE_AND_SPEC_BACKED")
        if is_family_adjacent(seed):
            return current("FAMILY_SCHEMA_MAPPING", "partial", "SPEC_BACKED") if not is_target else target("SPECIFIED_FAMILY_SCHEMA_MAPPING", "full")
        if seed.slug in TARGETED_SPECIALIZED:
            return current("TARGETED_SCHEMA_OR_NAMESPACE_MAPPING", "partial", "SPEC_BACKED") if not is_target else target("TARGETED_SCHEMA_OR_NAMESPACE_MAPPING", "partial")
        return current("OUT_OF_SCOPE", "out", "SPEC_BACKED") if not is_target else target("OUT_OF_SCOPE", "out")
    if kind == "query_mutation":
        if donor_has_sql(seed):
            return current("NATIVE_SQL_CORE_WITH_EMULATION_BOUNDARIES", "full", "CODE_AND_SPEC_BACKED") if not is_target else target("NATIVE_SQL_CORE_WITH_EMULATION_BOUNDARIES", "full", "CODE_AND_SPEC_BACKED")
        if seed.slug in TARGETED_SPECIALIZED:
            return current("SPECIALIZED_QUERY_SURFACE_PRESENT", "partial", "SPEC_BACKED") if not is_target else target("SPECIALIZED_QUERY_SURFACE_PRESENT", "partial")
        return current("OUT_OF_SCOPE", "out", "SPEC_BACKED") if not is_target else target("OUT_OF_SCOPE", "out")
    if kind == "donor_query_emulation":
        if seed.slug in DIRECT_DIALECTS:
            return current("DIRECT_DONOR_QUERY_EMULATION_BOUNDED", "partial", "RUNTIME_VERIFIED") if not is_target else target("DIRECT_DONOR_QUERY_EMULATION_SPECIFIED", "full")
        if is_family_adjacent(seed):
            return current("FAMILY_QUERY_MAPPING", "partial", "SPEC_BACKED") if not is_target else target("FAMILY_QUERY_MAPPING", "full")
        if seed.slug in TARGETED_SPECIALIZED:
            return current("TARGETED_QUERY_MAPPING_OR_CHECKLIST", "partial", "SPEC_BACKED") if not is_target else target("TARGETED_QUERY_MAPPING_OR_CHECKLIST", "partial")
        return current("OUT_OF_SCOPE", "out", "SPEC_BACKED") if not is_target else target("OUT_OF_SCOPE", "out")
    if kind == "planner":
        return current("PARTIAL_CODE_BACKED_PLANNER_TAXONOMY", "partial", "CODE_AND_SPEC_BACKED") if not is_target else target("CANONICAL_PLANNER_TAXONOMY", "full")
    if kind == "index_runtime":
        return current("MULTI_FAMILY_INDEX_RUNTIME", "full", "CODE_AND_SPEC_BACKED") if not is_target else target("MULTI_FAMILY_INDEX_RUNTIME", "full", "CODE_AND_SPEC_BACKED")
    if kind == "index_mapping":
        return current("CANONICAL_INDEX_ALIAS_LOWERING", "full", "CODE_AND_SPEC_BACKED") if not is_target else target("CANONICAL_INDEX_ALIAS_LOWERING", "full", "CODE_AND_SPEC_BACKED")
    if kind == "specialized_indexes":
        return current("BROAD_SPECIALIZED_INDEX_SURFACE", "full", "CODE_AND_SPEC_BACKED") if not is_target else target("BROAD_SPECIALIZED_INDEX_SURFACE", "full", "CODE_AND_SPEC_BACKED")
    if kind == "index_mga":
        return current("MGA_SAFE_INDEX_PUBLICATION_AND_METRICS", "full", "CODE_AND_SPEC_BACKED") if not is_target else target("MGA_SAFE_INDEX_PUBLICATION_AND_METRICS", "full", "CODE_AND_SPEC_BACKED")
    if kind == "functions":
        return current("BROAD_BUILTIN_FUNCTION_AND_OPERATOR_SURFACE", "full", "CODE_AND_SPEC_BACKED") if not is_target else target("BROAD_BUILTIN_FUNCTION_AND_OPERATOR_SURFACE", "full", "CODE_AND_SPEC_BACKED")
    if kind == "server_side_code":
        if donor_has_sql(seed):
            return current("NATIVE_ROUTINE_AND_TRIGGER_SURFACE", "full", "CODE_AND_SPEC_BACKED") if not is_target else target("NATIVE_ROUTINE_AND_TRIGGER_SURFACE", "full", "CODE_AND_SPEC_BACKED")
        return current("ABSENT", "absent", "CODE_AND_SPEC_BACKED") if not is_target else target("ABSENT", "absent", "CODE_AND_SPEC_BACKED")
    if kind == "sblr":
        return current("SBLR_BYTECODE_RUNTIME_WITH_LLVM", "full", "CODE_AND_SPEC_BACKED") if not is_target else target("SBLR_BYTECODE_RUNTIME_WITH_LLVM", "full", "CODE_AND_SPEC_BACKED")
    if kind == "authn":
        return current("AUTH_PROVIDER_CHAIN_AND_SESSION_IDENTITY", "full", "CODE_AND_SPEC_BACKED") if not is_target else target("AUTH_PROVIDER_CHAIN_AND_SESSION_IDENTITY", "full", "CODE_AND_SPEC_BACKED")
    if kind == "policy_pipeline":
        return current("ROW_COLUMN_DOMAIN_MASKING_PIPELINE", "full", "CODE_AND_SPEC_BACKED") if not is_target else target("ROW_COLUMN_DOMAIN_MASKING_PIPELINE", "full", "CODE_AND_SPEC_BACKED")
    if kind == "sandbox_security":
        return current("DEFINER_INVOKER_AND_SCHEMA_SANDBOX_SECURITY", "full", "CODE_AND_SPEC_BACKED") if not is_target else target("DEFINER_INVOKER_AND_SCHEMA_SANDBOX_SECURITY", "full", "CODE_AND_SPEC_BACKED")
    if kind == "parser_separation":
        return current("SEPARATE_PARSER_FAMILIES_LOWER_TO_SBLR", "full", "CODE_AND_SPEC_BACKED") if not is_target else target("SEPARATE_PARSER_FAMILIES_LOWER_TO_SBLR", "full", "CODE_AND_SPEC_BACKED")
    if kind == "front_door":
        if seed.slug in DIRECT_DIALECTS:
            return current("SHIPPED_DIRECT_FRONT_DOOR_BOUNDED", "partial", "RUNTIME_VERIFIED") if not is_target else target("DIRECT_FRONT_DOOR_SPECIFIED", "full")
        if is_family_adjacent(seed):
            return current("FAMILY_ADJACENT_FRONT_DOOR", "partial", "SPEC_BACKED") if not is_target else target("FAMILY_ADJACENT_FRONT_DOOR", "full")
        if seed.slug in TARGETED_SPECIALIZED:
            return current("TARGETED_FRONT_DOOR_CHECKLIST_OR_SCAFFOLD", "partial", "SPEC_BACKED") if not is_target else target("TARGETED_FRONT_DOOR_CHECKLIST_OR_SCAFFOLD", "partial")
        return current("OUT_OF_SCOPE", "out", "SPEC_BACKED") if not is_target else target("OUT_OF_SCOPE", "out")
    if kind == "prepared_error":
        if seed.slug in DIRECT_DIALECTS:
            return current("BOUNDED_PREPARED_AND_ERROR_MAPPING", "partial", "RUNTIME_VERIFIED") if not is_target else target("PREPARED_AND_ERROR_MAPPING_SPECIFIED", "full")
        if is_family_adjacent(seed):
            return current("FAMILY_PREPARED_AND_ERROR_MAPPING", "partial", "SPEC_BACKED") if not is_target else target("FAMILY_PREPARED_AND_ERROR_MAPPING", "full")
        if seed.slug in TARGETED_SPECIALIZED:
            return current("TARGETED_PARAMETER_AND_RESULT_MAPPING", "partial", "SPEC_BACKED") if not is_target else target("TARGETED_PARAMETER_AND_RESULT_MAPPING", "partial")
        return current("OUT_OF_SCOPE", "out", "SPEC_BACKED") if not is_target else target("OUT_OF_SCOPE", "out")
    if kind == "tool_harness":
        if seed.slug == "postgresql":
            return current("BOUNDED_PG_REGRESS_AND_DONOR_CLIENT_PROOF", "partial", "RUNTIME_HARNESS_VERIFIED") if not is_target else target("BOUNDED_PG_REGRESS_AND_DONOR_CLIENT_PROOF", "partial", "RUNTIME_HARNESS_VERIFIED")
        if seed.slug == "mysql":
            return current("DONOR_CLIENT_AND_MTR_SMOKE_PROOF", "partial", "RUNTIME_HARNESS_VERIFIED") if not is_target else target("DONOR_CLIENT_AND_MTR_SMOKE_PROOF", "partial", "RUNTIME_HARNESS_VERIFIED")
        if seed.slug == "firebirdsql":
            return current("DONOR_CLIENT_AND_FIREBIRD_QA_SMOKE_PROOF", "partial", "RUNTIME_HARNESS_VERIFIED") if not is_target else target("DONOR_CLIENT_AND_FIREBIRD_QA_SMOKE_PROOF", "partial", "RUNTIME_HARNESS_VERIFIED")
        if is_family_adjacent(seed):
            return current("FAMILY_TOOLING_ONLY", "partial", "SPEC_BACKED") if not is_target else target("FAMILY_TOOLING_ONLY", "partial", "SPEC_BACKED")
        return current("OUT_OF_SCOPE", "out", "SPEC_BACKED") if not is_target else target("OUT_OF_SCOPE", "out")
    if kind == "runtime_modes":
        return current("EMBEDDED_AND_LISTENER_RUNTIME_MODES", "full", "CODE_AND_SPEC_BACKED") if not is_target else target("EMBEDDED_AND_LISTENER_RUNTIME_MODES", "full", "CODE_AND_SPEC_BACKED")
    if kind == "mga_diag":
        return current("MGA_DIAGNOSTICS_AND_OPERATOR_INTROSPECTION", "full", "CODE_AND_SPEC_BACKED") if not is_target else target("MGA_DIAGNOSTICS_AND_OPERATOR_INTROSPECTION", "full", "CODE_AND_SPEC_BACKED")
    if kind == "backup_bulk":
        return current("BACKUP_RESTORE_AND_BULK_PATHS", "full", "CODE_AND_SPEC_BACKED") if not is_target else target("BACKUP_RESTORE_AND_BULK_PATHS", "full", "CODE_AND_SPEC_BACKED")
    if kind == "cdc":
        return current("CDC_AND_MIGRATION_SUBSTRATE", "partial", "SPEC_BACKED") if not is_target else target("CDC_AND_MIGRATION_SUBSTRATE", "full")
    if kind == "cluster_fencing":
        return current("CLUSTER_IDENTITY_FENCING_AND_ROUTING_EPOCH", "full", "CODE_AND_SPEC_BACKED") if not is_target else target("CLUSTER_IDENTITY_FENCING_AND_ROUTING_EPOCH", "full", "CODE_AND_SPEC_BACKED")
    if kind == "topology":
        return current("PARTIAL_TOPOLOGY_AND_SHARDING_MODEL", "partial", "SPEC_BACKED") if not is_target else target("CANONICAL_TOPOLOGY_AND_SHARDING_MODEL", "full")
    if kind == "apply_lag":
        return current("PARTIAL_APPLY_AND_FAILOVER_OBSERVABILITY", "partial", "CODE_AND_SPEC_BACKED") if not is_target else target("CANONICAL_APPLY_AND_FAILOVER_MODEL", "partial")
    if kind == "document":
        if donor_has_document(seed):
            return current("TARGETED_DOCUMENT_SURFACE", "partial", "SPEC_BACKED") if not is_target else target("TARGETED_DOCUMENT_SURFACE", "partial")
        return current("PARTIAL_DOCUMENT_SUBSTRATE", "partial", "SPEC_BACKED") if not is_target else target("PARTIAL_DOCUMENT_SUBSTRATE", "partial")
    if kind == "graph":
        if donor_has_graph(seed):
            return current("TARGETED_GRAPH_SURFACE", "partial", "SPEC_BACKED") if not is_target else target("TARGETED_GRAPH_SURFACE", "partial")
        return current("PARTIAL_GRAPH_SUBSTRATE", "partial", "SPEC_BACKED") if not is_target else target("PARTIAL_GRAPH_SUBSTRATE", "partial")
    if kind == "vector":
        if donor_has_vector(seed):
            return current("TARGETED_VECTOR_SURFACE", "partial", "SPEC_BACKED") if not is_target else target("TARGETED_VECTOR_SURFACE", "partial")
        return current("PARTIAL_VECTOR_AND_ANN_SUBSTRATE", "partial", "CODE_AND_SPEC_BACKED") if not is_target else target("PARTIAL_VECTOR_AND_ANN_SUBSTRATE", "partial", "CODE_AND_SPEC_BACKED")
    if kind == "kv":
        if donor_has_kv(seed):
            return current("TARGETED_KEY_VALUE_SUBSTRATE", "partial", "SPEC_BACKED") if not is_target else target("TARGETED_KEY_VALUE_SUBSTRATE", "partial")
        return current("PARTIAL_KEY_VALUE_SUBSTRATE", "partial", "SPEC_BACKED") if not is_target else target("PARTIAL_KEY_VALUE_SUBSTRATE", "partial")
    if kind == "time_series":
        if donor_has_time_series(seed):
            return current("TARGETED_TIME_SERIES_SUBSTRATE", "partial", "SPEC_BACKED") if not is_target else target("TARGETED_TIME_SERIES_SUBSTRATE", "partial")
        return current("PARTIAL_TIME_SERIES_SUBSTRATE", "partial", "SPEC_BACKED") if not is_target else target("PARTIAL_TIME_SERIES_SUBSTRATE", "partial")
    if kind == "history":
        return current("RETAINED_LINEAGE_AND_TEMPORAL_SUBSTRATE", "partial", "CODE_AND_SPEC_BACKED") if not is_target else target("RETAINED_LINEAGE_AND_TEMPORAL_SUBSTRATE", "partial", "CODE_AND_SPEC_BACKED")

    raise ValueError(f"Unhandled ScratchBird-state kind: {kind}")


def compare_states(seed: ReportSeed, feature: Feature, donor: State, scratchbird: State, lane: str) -> tuple[str, str, str]:
    direct_family_subordinate = seed.slug in DIRECT_DIALECTS and feature.kind in DIRECT_FAMILY_SUBORDINATE_KINDS
    planned_emulation_subordinate = lane == "SPECIFIED_TARGET_STATE" and feature.kind in PLANNED_EMULATION_SUBORDINATE_KINDS

    if donor.coverage == "out" and scratchbird.coverage == "out":
        return "NOT_COMPARABLE", "SCOPE_EXCLUDED", "NONE"
    if donor.coverage == "out":
        return "SUBJECT_B_ONLY", "SUBJECT_B_SUPERSET", "NONE"
    if scratchbird.coverage == "out":
        return "SUBJECT_A_ONLY", "SUBJECT_A_SUPERSET", "NON_GOAL"

    if feature.comparison_mode == "structural":
        if donor.code == scratchbird.code:
            return "FUNCTIONALLY_EQUIVALENT", "NO_DIRECTIONAL_DELTA", "NONE"
        return "NOT_COMPARABLE", "ARCHITECTURALLY_DIFFERENT", "NONE"

    if feature.comparison_mode == "mga":
        if donor.code == scratchbird.code:
            return "FUNCTIONALLY_EQUIVALENT", "NO_DIRECTIONAL_DELTA", "NONE"
        if donor.coverage == "full" and scratchbird.coverage == "full":
            return "NOT_COMPARABLE", "ARCHITECTURALLY_DIFFERENT", "NONE"

    if donor.coverage == "full" and scratchbird.coverage == "full":
        if donor.code == scratchbird.code:
            return "FUNCTIONALLY_EQUIVALENT", "NO_DIRECTIONAL_DELTA", "NONE"
        if feature.comparison_mode == "emulation" and lane == "SPECIFIED_TARGET_STATE" and scratchbird.evidence == "SPEC_BACKED":
            return "EMULATABLE_WITHOUT_MATERIAL_LOSS", "NO_DIRECTIONAL_DELTA", "NONE"
        return "COMPATIBLE_WITH_MAPPING", "NO_DIRECTIONAL_DELTA", "MAPPING_REQUIRED"

    if direct_family_subordinate and donor.coverage == "full" and scratchbird.coverage == "partial":
        return "COMPATIBLE_WITH_MAPPING", "NO_DIRECTIONAL_DELTA", "MAPPING_REQUIRED"

    if planned_emulation_subordinate and donor.coverage == "full" and scratchbird.coverage == "partial":
        return "EMULATABLE_WITHOUT_MATERIAL_LOSS", "NO_DIRECTIONAL_DELTA", "NONE"

    if donor.coverage == "full" and scratchbird.coverage == "partial":
        blocker = "TARGET_GAP" if lane == "SPECIFIED_TARGET_STATE" else "CURRENT_GAP"
        return "PARTIAL_PARITY", "NO_DIRECTIONAL_DELTA", blocker

    if donor.coverage == "full" and scratchbird.coverage == "absent":
        blocker = "TARGET_GAP" if lane == "SPECIFIED_TARGET_STATE" else "CURRENT_GAP"
        return "SUBJECT_A_ONLY", "SUBJECT_A_SUPERSET", blocker

    if donor.coverage == "partial" and scratchbird.coverage == "full":
        return "SUBJECT_B_ONLY", "SUBJECT_B_SUPERSET", "NONE"

    if donor.coverage == "partial" and scratchbird.coverage == "partial":
        return "PARTIAL_PARITY", "NO_DIRECTIONAL_DELTA", "NONE"

    if donor.coverage == "partial" and scratchbird.coverage == "absent":
        blocker = "TARGET_GAP" if lane == "SPECIFIED_TARGET_STATE" else "CURRENT_GAP"
        return "SUBJECT_A_ONLY", "SUBJECT_A_SUPERSET", blocker

    if donor.coverage == "absent" and scratchbird.coverage == "full":
        return "SUBJECT_B_ONLY", "SUBJECT_B_SUPERSET", "NONE"

    if donor.coverage == "absent" and scratchbird.coverage == "partial":
        return "SUBJECT_B_ONLY", "SUBJECT_B_SUPERSET", "NONE"

    if donor.coverage == "absent" and scratchbird.coverage == "absent":
        return "IDENTICAL", "NO_DIRECTIONAL_DELTA", "NONE"

    return "NOT_COMPARABLE", "ARCHITECTURALLY_DIFFERENT", "UNKNOWN"


def is_hard_blocker(blocking: str) -> bool:
    return blocking in HARD_BLOCKING_CLASSES


def is_mapping_dependency(blocking: str) -> bool:
    return blocking == MAPPING_BLOCKING_CLASS


def donor_result_state(seed: ReportSeed, feature: Feature, lane: str) -> State:
    # Donor surface does not change between current and target lanes.
    return donor_state(seed, feature)


def emulated_catalog_analysis_ref(seed: ReportSeed) -> str | None:
    mapping = {
        "firebirdsql": "docs/specifications/24_Catalog_Model_and_Virtual_Overlays/EMULATED_CATALOG_ANALYSIS_FIREBIRD.md",
        "postgresql": "docs/specifications/24_Catalog_Model_and_Virtual_Overlays/EMULATED_CATALOG_ANALYSIS_POSTGRESQL.md",
        "mysql": "docs/specifications/24_Catalog_Model_and_Virtual_Overlays/EMULATED_CATALOG_ANALYSIS_MYSQL.md",
        "cassandra": "docs/specifications/24_Catalog_Model_and_Virtual_Overlays/EMULATED_CATALOG_ANALYSIS_CASSANDRA.md",
        "mongodb": "docs/specifications/24_Catalog_Model_and_Virtual_Overlays/EMULATED_CATALOG_ANALYSIS_MONGODB.md",
        "neo4j": "docs/specifications/24_Catalog_Model_and_Virtual_Overlays/EMULATED_CATALOG_ANALYSIS_NEO4J.md",
        "redis": "docs/specifications/24_Catalog_Model_and_Virtual_Overlays/EMULATED_CATALOG_ANALYSIS_REDIS.md",
        "milvus": "docs/specifications/24_Catalog_Model_and_Virtual_Overlays/EMULATED_CATALOG_ANALYSIS_MILVUS.md",
    }
    return mapping.get(seed.slug)


def front_door_refs(seed: ReportSeed) -> tuple[str, ...]:
    base = (
        "docs/specifications/28_Parser_Implementations/README.md",
        "docs/specifications/29_Listener_and_Server_Orchestration/README.md",
    )
    if seed.slug == "postgresql":
        return base + (
            "docs/specifications/28_Parser_Implementations/NORMATIVE_EMULATED_PARSER_LAYER_POSTGRESQL.md",
            "tests/compatibility/postgresql/README.md",
            "tests/compatibility/results/emulation/epfc-026-postgresql-upstream-harness-evidence-20260304T024216Z.md",
        )
    if seed.slug == "mysql":
        return base + (
            "docs/specifications/28_Parser_Implementations/NORMATIVE_WIRE_PROTOCOL_MYSQL_CHECKLIST.md",
            "tests/compatibility/mysql/README.md",
            "tests/compatibility/results/emulation/mysql/p5s2w2/my-emu-041-mtr-report.md",
        )
    if seed.slug == "firebirdsql":
        return base + (
            "docs/specifications/28_Parser_Implementations/NORMATIVE_EMULATED_PARSER_LAYER_FIREBIRD.md",
            "tests/compatibility/firebird/README.md",
            "tests/compatibility/results/emulation/firebird/p5s2w2/fb-emu-041-firebird-qa-report.md",
        )
    if seed.slug in TARGETED_SPECIALIZED:
        return base + ("docs/specifications/28_Parser_Implementations/NORMATIVE_EMULATED_PARSER_LAYER_BASELINE.md",)
    return base


def source_refs(seed: ReportSeed, feature: Feature, lane: str) -> tuple[str, ...]:
    refs: list[str] = [seed.ref_readme, seed.ref_source]
    refs.extend(feature.refs)
    if feature.kind == "catalog_overlay":
        ref = emulated_catalog_analysis_ref(seed)
        if ref:
            refs.append(ref)
    if feature.kind in {"donor_query_emulation", "front_door", "prepared_error", "tool_harness"}:
        refs.extend(front_door_refs(seed))
    # Preserve ordering while removing duplicates.
    seen: set[str] = set()
    ordered: list[str] = []
    for ref in refs:
        if ref not in seen:
            ordered.append(ref)
            seen.add(ref)
    return tuple(ordered)


def render_refs(refs: Iterable[str]) -> str:
    return "<br>".join(f"`{ref}`" for ref in refs)


def render_cell(value: str) -> str:
    return value.replace("\n", "<br>")


def note_text(seed: ReportSeed, feature: Feature, donor: State, scratchbird: State, lane: str, outcome: str, blocking: str) -> str:
    lane_label = "current lane" if lane == "CURRENT_STATE" else "specified target lane"
    parts = [
        feature.summary,
        f"Donor baseline is `{donor.code}`.",
        f"ScratchBird {lane_label} is `{scratchbird.code}`.",
    ]
    if outcome == "COMPATIBLE_WITH_MAPPING":
        parts.append("This row depends on explicit mapping or overlay behavior rather than one-for-one architecture.")
    if blocking == "CURRENT_GAP":
        parts.append("The donor surface is broader than the currently proven ScratchBird lane for this row.")
    if blocking == "TARGET_GAP":
        parts.append("The donor surface remains broader than the currently specified ScratchBird target lane for this row.")
    return " ".join(parts)


def build_feature_row(seed: ReportSeed, feature: Feature) -> dict[str, dict[str, str]]:
    result: dict[str, dict[str, str]] = {}
    for lane in ("CURRENT_STATE", "SPECIFIED_TARGET_STATE"):
        donor = donor_result_state(seed, feature, lane)
        scratchbird = scratchbird_state(seed, feature, lane)
        outcome, delta, blocking = compare_states(seed, feature, donor, scratchbird, lane)
        result[lane] = {
            "Assessment Lane": f"`{lane}`",
            "Feature Key": f"`{feature.feature_key}`",
            "Feature Name": feature.feature_name,
            f"{seed.donor} State": f"`{donor.code}`",
            "ScratchBird State": f"`{scratchbird.code}`",
            "Outcome": f"`{outcome}`",
            "Delta Class": f"`{delta}`",
            "Blocking Class": f"`{blocking}`",
            "Evidence": f"`{scratchbird.evidence}`",
            "Notes or Mapping": note_text(seed, feature, donor, scratchbird, lane, outcome, blocking),
            "Source References": render_refs(source_refs(seed, feature, lane)),
        }
    return result


def score_rows(rows: list[dict[str, dict[str, str]]], lane: str) -> dict[str, int]:
    metrics = {
        "in_scope": 0,
        "IDENTICAL": 0,
        "FUNCTIONALLY_EQUIVALENT": 0,
        "COMPATIBLE_WITH_MAPPING": 0,
        "EMULATABLE_WITHOUT_MATERIAL_LOSS": 0,
        "PARTIAL_PARITY": 0,
        "directional": 0,
        "blockers": 0,
        "roadmap": 0,
    }
    for row in rows:
        donor_state_value = next(
            value.strip("`")
            for key, value in row[lane].items()
            if key.endswith(" State") and key != "ScratchBird State"
        )
        scratchbird_state_value = row[lane]["ScratchBird State"].strip("`")
        outcome = row[lane]["Outcome"].strip("`")
        blocking = row[lane]["Blocking Class"].strip("`")
        evidence = row[lane]["Evidence"].strip("`")
        if donor_state_value == "OUT_OF_SCOPE" or scratchbird_state_value == "OUT_OF_SCOPE":
            continue
        metrics["in_scope"] += 1
        if outcome in metrics:
            metrics[outcome] += 1
        if outcome in {"SUBJECT_A_ONLY", "SUBJECT_B_ONLY", "NOT_COMPARABLE"}:
            metrics["directional"] += 1
        if is_hard_blocker(blocking):
            metrics["blockers"] += 1
        if evidence == "PLANNED_ONLY":
            metrics["roadmap"] += 1
    return metrics


def verdict(rows: list[dict[str, dict[str, str]]], lane: str) -> str:
    mapping = 0
    hard = 0
    for row in rows:
        blocking = row[lane]["Blocking Class"].strip("`")
        if blocking == "NONE":
            continue
        if is_mapping_dependency(blocking):
            mapping += 1
        elif is_hard_blocker(blocking):
            hard += 1
    if hard == 0:
        return "FEASIBLE_WITH_MAPPING" if mapping else "NO_BLOCKERS"
    if hard <= 6 and mapping >= hard:
        return "PARTIAL_WITH_GAPS"
    return "NOT_CURRENTLY_FEASIBLE"


def blocker_feature_keys(rows: list[dict[str, dict[str, str]]], lane: str) -> list[str]:
    blockers: list[str] = []
    for row in rows:
        if is_hard_blocker(row[lane]["Blocking Class"].strip("`")):
            blockers.append(row[lane]["Feature Key"].strip("`"))
    return blockers


def mapping_feature_keys(rows: list[dict[str, dict[str, str]]], lane: str) -> list[str]:
    mappings: list[str] = []
    for row in rows:
        if is_mapping_dependency(row[lane]["Blocking Class"].strip("`")):
            mappings.append(row[lane]["Feature Key"].strip("`"))
    return mappings


def render_feature_table(seed: ReportSeed, feature: Feature, row: dict[str, dict[str, str]]) -> str:
    ordered_headings = [
        "Assessment Lane",
        "Feature Key",
        "Feature Name",
        f"{seed.donor} State",
        "ScratchBird State",
        "Outcome",
        "Delta Class",
        "Blocking Class",
        "Evidence",
        "Notes or Mapping",
        "Source References",
    ]
    lines = [
        f"### `{feature.feature_key}` {feature.feature_name}",
        "",
        "| Row Heading | Current State | Target State |",
        "| --- | --- | --- |",
    ]
    for heading in ordered_headings:
        lines.append(
            f"| {heading} | {render_cell(row['CURRENT_STATE'][heading])} | {render_cell(row['SPECIFIED_TARGET_STATE'][heading])} |"
        )
    return "\n".join(lines)


def render_scorecard_table(metrics: dict[str, int], lane: str) -> str:
    return "\n".join(
        [
            f"### {lane}",
            "",
            "| Metric | Count |",
            "| --- | ---: |",
            f"| in-scope feature rows | {metrics['in_scope']} |",
            f"| IDENTICAL | {metrics['IDENTICAL']} |",
            f"| FUNCTIONALLY_EQUIVALENT | {metrics['FUNCTIONALLY_EQUIVALENT']} |",
            f"| COMPATIBLE_WITH_MAPPING | {metrics['COMPATIBLE_WITH_MAPPING']} |",
            f"| EMULATABLE_WITHOUT_MATERIAL_LOSS | {metrics['EMULATABLE_WITHOUT_MATERIAL_LOSS']} |",
            f"| PARTIAL_PARITY | {metrics['PARTIAL_PARITY']} |",
            f"| directional-only rows | {metrics['directional']} |",
            f"| blocker rows | {metrics['blockers']} |",
            f"| roadmap-only rows | {metrics['roadmap']} |",
        ]
    )


def source_pack(seed: ReportSeed) -> str:
    return (
        "docs/specifications/Reference_Documentation_specification.md + "
        f"{seed.ref_dir.relative_to(REPO_ROOT).as_posix()} + "
        "ScratchBird canonical specs + compatibility evidence where applicable"
    )


def exclusions(seed: ReportSeed) -> str:
    return (
        "Benchmark-only claims, byte-for-byte on-disk parity, ecosystem-only tooling, and "
        "unproven full upstream-harness closure remain excluded unless a row cites bounded evidence directly."
    )


def render_report(seed: ReportSeed) -> tuple[str, str, str]:
    rows = [build_feature_row(seed, feature) for feature in FEATURES]
    current_metrics = score_rows(rows, "CURRENT_STATE")
    target_metrics = score_rows(rows, "SPECIFIED_TARGET_STATE")
    current_verdict = verdict(rows, "CURRENT_STATE")
    target_verdict = verdict(rows, "SPECIFIED_TARGET_STATE")
    current_blockers = blocker_feature_keys(rows, "CURRENT_STATE")
    target_blockers = blocker_feature_keys(rows, "SPECIFIED_TARGET_STATE")
    current_mappings = mapping_feature_keys(rows, "CURRENT_STATE")
    target_mappings = mapping_feature_keys(rows, "SPECIFIED_TARGET_STATE")

    sections: dict[int, list[str]] = {}
    for feature, row in zip(FEATURES, rows):
        sections.setdefault(feature.section_number, []).append(render_feature_table(seed, feature, row))

    report_lines = [
        f"# {seed.donor} vs ScratchBird: Cross-Engine Feature Comparison",
        "",
        f"Report ID: `april-2026-{seed.slug}-vs-scratchbird`",
        "",
        f"- report_date: `{REPORT_DATE}`",
        f"- report_family_version: `{REPORT_FAMILY_VERSION}`",
        "- comparison_intent: `EMULATION_FEASIBILITY`",
        "- assessment_lanes: `CURRENT_STATE`, `SPECIFIED_TARGET_STATE`",
        "- scope_profile: `full feature-to-feature comparison of donor surface against ScratchBird database-environment surface; includes engine core, parser boundary, catalog overlays, donor front door, and cluster/lineage behavior; current-state plus specified-target-state lanes`",
        f"- subject_a.engine_key: `{seed.slug}`",
        f"- subject_a.display_name: `{seed.donor}`",
        f"- subject_a.version_or_release: `formal reference pack capture as of {REPORT_DATE}`",
        "- subject_a.edition_or_variant: `reference donor baseline`",
        "- subject_a.deployment_mode: `donor-defined deployment surface`",
        "- subject_b.engine_key: `scratchbird`",
        "- subject_b.display_name: `ScratchBird`",
        f"- subject_b.version_or_release: `canonical spec tree and current code evidence as of {REPORT_DATE}`",
        "- subject_b.edition_or_variant: `database environment with engine, parser families, overlays, and cluster substrate`",
        "- subject_b.deployment_mode: `embedded-first environment with listener and parser front doors`",
        "- scratchbird_role: `IMPLEMENTATION_CANDIDATE`",
        f"- evidence_cutoff_date: `{REPORT_DATE}`",
        f"- generator_or_author: `{GENERATOR}`",
        f"- excluded_domains: `{exclusions(seed)}`",
        f"- source_pack: `{source_pack(seed)}`",
        "",
        "## 1. Purpose and Scope",
        "",
        f"This report compares `{seed.donor}` against the full ScratchBird platform surface rather than against a flattened generic SQL-engine profile. The comparison keeps environment-level ScratchBird features visible, including parser separation, virtual catalog overlays, SBLR, UUID lineage, and cluster-facing identity or retention behavior.",
        "",
        "## 2. Executive Summary",
        "",
        f"- current_state_verdict: `{current_verdict}`",
        f"- specified_target_state_verdict: `{target_verdict}`",
        f"- feature_matrix_rows: `{len(FEATURES)}`",
        f"- current_blocker_feature_keys: `{', '.join(current_blockers[:10]) if current_blockers else 'none'}`",
        f"- target_blocker_feature_keys: `{', '.join(target_blockers[:10]) if target_blockers else 'none'}`",
        f"- current_mapping_feature_keys: `{', '.join(current_mappings[:10]) if current_mappings else 'none'}`",
        f"- target_mapping_feature_keys: `{', '.join(target_mappings[:10]) if target_mappings else 'none'}`",
        f"- donor_role: `{seed.role}`",
        "",
        "## 3. Subject Identity and Scope Assumptions",
        "",
        f"- donor_rank_and_phase: `rank {seed.rank}`, `{seed.phase}`",
        f"- why_included: {seed.why_included}",
        f"- authoritative_topics: {seed.what_to_study}",
        f"- non_authoritative_topics: {seed.what_not_to_copy}",
        "- ScratchBird interpretation rule: compare ScratchBird as a database environment with a separate parser/front-door layer, canonical catalogs, SBLR execution, and cluster-aware lineage semantics.",
        "- Donor-tool evidence rule: direct client or upstream harness closure is cited only where the repo contains bounded execution evidence; it is not inferred from parser presence alone.",
        "",
        "## 4. Overall Category Scorecard",
        "",
        render_scorecard_table(current_metrics, "CURRENT_STATE"),
        "",
        render_scorecard_table(target_metrics, "SPECIFIED_TARGET_STATE"),
        "",
        "## 5. Architecture, Storage, and Transaction Core",
        "",
        "\n\n".join(sections[5]),
        "",
        "## 6. Catalog, Metadata, and Object Identity",
        "",
        "\n\n".join(sections[6]),
        "",
        "## 7. Types and Value Semantics",
        "",
        "\n\n".join(sections[7]),
        "",
        "## 8. DDL and Schema Lifecycle",
        "",
        "\n\n".join(sections[8]),
        "",
        "## 9. DML, Query, and Planner Surface",
        "",
        "\n\n".join(sections[9]),
        "",
        "## 10. Indexing and Access Paths",
        "",
        "\n\n".join(sections[10]),
        "",
        "## 11. Callable, Procedural, and Trigger Surface",
        "",
        "\n\n".join(sections[11]),
        "",
        "## 12. Security, Identity, and Policy Surface",
        "",
        "\n\n".join(sections[12]),
        "",
        "## 13. Parser, Protocol, and Emulation Front Door",
        "",
        "\n\n".join(sections[13]),
        "",
        "## 14. Operations, Introspection, and Data Movement",
        "",
        "\n\n".join(sections[14]),
        "",
        "## 15. Replication, Distribution, and Availability",
        "",
        "\n\n".join(sections[15]),
        "",
        "## 16. Specialized and Non-Relational Surface Areas",
        "",
        "\n\n".join(sections[16]),
        "",
        "## 17. Limitations, Exclusions, and Non-Comparable Areas",
        "",
        "- This report keeps environment-level ScratchBird rows visible even when the donor has no equivalent surface.",
        "- Direct donor-tool and upstream harness rows are intentionally conservative. Current bounded evidence exists for direct donor lanes only where cited in the row references.",
        "- `SPECIFIED_TARGET_STATE` counts only behavior already defined in authoritative ScratchBird specifications. Desirable but unspec'ed work is not scored.",
        "",
        "## 18. Optional Mapping or Emulation Strategy",
        "",
        f"- Primary follow-on hard-gap set for this donor: `{', '.join(target_blockers[:12]) if target_blockers else 'none'}`.",
        f"- Mapping-intensive rows for this donor: `{', '.join(target_mappings[:12]) if target_mappings else 'none'}`.",
        "- For direct donor families, prioritize catalog-overlay, schema-root, type-mapping, and front-door rows before widening ecosystem tooling claims.",
        "- For non-direct donor families, treat environment-boundary rows as design inputs rather than as evidence of existing donor parity.",
        "",
        "## 19. Final Assessment",
        "",
        "### CURRENT_STATE",
        "",
        f"Verdict: `{current_verdict}`. Current hard gaps are driven by `{', '.join(current_blockers[:12]) if current_blockers else 'none'}`. Mapping-required rows are `{', '.join(current_mappings[:12]) if current_mappings else 'none'}`.",
        "",
        "### SPECIFIED_TARGET_STATE",
        "",
        f"Verdict: `{target_verdict}`. Target-state hard gaps are driven by `{', '.join(target_blockers[:12]) if target_blockers else 'none'}`. Mapping-required rows are `{', '.join(target_mappings[:12]) if target_mappings else 'none'}`.",
        "",
        "## 20. Evidence and Sources",
        "",
        f"- donor control matrix: `docs/specifications/Reference_Documentation_specification.md`",
        f"- donor formal reference pack: `{seed.ref_readme}`, `{seed.ref_source}`",
        "- comparison schema authority: `docs/specifications/31_Conformance_Performance_and_Reliability_Gates/CROSS_ENGINE_FEATURE_COMPARISON_REPORT_SCHEMA_AND_PAIRWISE_COMPARABILITY_MODEL.md`",
        "- ScratchBird canonical anchors: `docs/specifications/07_Catalog_Bootstrap_and_UUID_Mapping/UUID_IDENTITY_AND_COLLISION_RULES.md`; `docs/specifications/11_TOAST_and_LOB_Storage/README.md`; `docs/specifications/15_Complex_Types/README.md`; `docs/specifications/18_Index_Framework/README.md`; `docs/specifications/19_Security_Model/README.md`; `docs/specifications/20_Diagnostics_Audit_and_Observability/MGA_OBSERVABILITY_AND_OPERATOR_DIAGNOSTICS.md`; `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/README.md`; `docs/specifications/28_Parser_Implementations/README.md`; `docs/specifications/29_Listener_and_Server_Orchestration/README.md`; `docs/specifications/42_Failure_Model_and_Fault_Tolerance/FAILURE_MODEL_AND_FAULT_CLASSIFICATION.md`",
    ]
    return "\n".join(report_lines).strip() + "\n", current_verdict, target_verdict


def write_manifest(seeds: list[ReportSeed], verdicts: dict[str, tuple[str, str]]) -> None:
    with MANIFEST_PATH.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(
            [
                "donor",
                "rank",
                "phase",
                "role",
                "report_family_version",
                "current_verdict",
                "target_verdict",
                "feature_rows",
                "report_file",
            ]
        )
        for seed in seeds:
            current_verdict, target_verdict = verdicts[seed.slug]
            writer.writerow(
                [
                    seed.donor,
                    seed.rank,
                    seed.phase,
                    seed.role,
                    REPORT_FAMILY_VERSION,
                    current_verdict,
                    target_verdict,
                    len(FEATURES),
                    seed.report_file,
                ]
            )


def write_readme(seeds: list[ReportSeed], verdicts: dict[str, tuple[str, str]]) -> None:
    lines = [
        "# April 2026 Cross-Engine Comparison Report Set",
        "",
        "This directory contains the April 2026 comparison batch generated from the canonical section 31 report schema and the donor list in `docs/specifications/Reference_Documentation_specification.md`.",
        "",
        "The current batch is environment-aware. Each report compares a donor product against ScratchBird's engine core, parser boundary, catalog overlays, donor front doors, and cluster or lineage surface instead of reducing the comparison to one generic SQL-engine matrix.",
        "",
        "## Included Database and Database-Platform Donors",
        "",
        "| Rank | Donor | Phase | Role | Current Verdict | Target Verdict | Feature Rows | Report |",
        "| ---: | --- | --- | --- | --- | --- | ---: | --- |",
    ]
    for seed in seeds:
        current_verdict, target_verdict = verdicts[seed.slug]
        lines.append(
            f"| {seed.rank} | {seed.donor} | {seed.phase} | {seed.role} | `{current_verdict}` | `{target_verdict}` | {len(FEATURES)} | [{seed.report_file}]({seed.report_file}) |"
        )
    lines.extend(
        [
            "",
            "## Excluded Non-Database Donors",
            "",
            "| Donor | Reason Excluded From This Per-Database Report Batch |",
            "| --- | --- |",
            "| SQLancer | Testing tool, not a database product. |",
            "| sqllogictest | Result-oracle corpus, not a database product. |",
            "| SQLsmith | Fuzzing tool, not a database product. |",
            "| Apache Calcite | Planner and federation framework, not a database product. |",
            "| Debezium | CDC and migration platform, not a database product. |",
            "| WiredTiger | Storage-engine library, not a standalone database product. |",
            "| Substrait | Interop specification, not a database product. |",
            "| Vitess | Sharding and control-plane layer rather than a standalone database product. |",
            "| RocksDB | Storage-engine library, not a standalone database product. |",
            "| LMDB | Embedded storage library, not a full database product in this batch. |",
            "| Jepsen | Fault-testing methodology and tooling, not a database product. |",
            "| TLA+ | Formal method, not a database product. |",
            "| Elle | Anomaly detector, not a database product. |",
            "| Maelstrom | Fault-simulation harness, not a database product. |",
            "| Arrow Flight SQL | Transport protocol, not a database product. |",
            "| Raft paper | Paper, not a database product. |",
            "| Spanner paper | Paper, not a database product. |",
            "| Calvin paper | Paper, not a database product. |",
            "| Calcite paper | Paper, not a database product. |",
            "| Differential Query Execution | Paper/topic donor, not a database product. |",
            "| SQLancer / TLP paper | Paper, not a database product. |",
            "",
            "## Artifacts",
            "",
            "- manifest: [comparison_report_manifest.csv](comparison_report_manifest.csv)",
            "- report schema authority: `docs/specifications/31_Conformance_Performance_and_Reliability_Gates/CROSS_ENGINE_FEATURE_COMPARISON_REPORT_SCHEMA_AND_PAIRWISE_COMPARABILITY_MODEL.md`",
            "- donor control source: `docs/specifications/Reference_Documentation_specification.md`",
        ]
    )
    README_PATH.write_text("\n".join(lines).strip() + "\n", encoding="utf-8")


def main() -> None:
    seeds = load_existing_manifest()
    verdicts: dict[str, tuple[str, str]] = {}
    for seed in seeds:
        report_text, current_verdict, target_verdict = render_report(seed)
        (APRIL_DIR / seed.report_file).write_text(report_text, encoding="utf-8")
        verdicts[seed.slug] = (current_verdict, target_verdict)
    write_manifest(seeds, verdicts)
    write_readme(seeds, verdicts)


if __name__ == "__main__":
    main()
