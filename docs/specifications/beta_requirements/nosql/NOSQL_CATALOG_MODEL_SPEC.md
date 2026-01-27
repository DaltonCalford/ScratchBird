# NoSQL Catalog Model Specification (Beta)

**Status:** Draft (Beta)

## Purpose

Define the canonical catalog objects required to represent NoSQL models inside
ScratchBird: document collections, key-value buckets, column families, and
graphs. These catalog objects provide stable UUIDs for security, dependencies,
and schema resolution while remaining compatible with the existing system
catalog design.

This spec covers metadata and catalog wiring only. Storage engines and query
languages are specified elsewhere.

## Scope and Assumptions

- All objects use UUIDs as primary identifiers.
- Objects are schema-scoped (except where explicitly noted).
- Permissions are enforced via `sys.permissions` like all other SQL objects.
- Catalog entries are owned by users and tracked in `sys.dependencies`.
- Physical storage mapping (table-backed vs specialized) is declared in the
  catalog for each object.

## Object Types

- **Collection**: Document-oriented container (JSON/JSONB). Comparable to a
  document table but modeled as a first-class NoSQL object.
- **Bucket**: Key-value namespace with optional TTL and value log settings.
- **Column Family**: Wide-column / column-family container with partition and
  clustering definitions.
- **Graph**: Logical graph object with vertex labels and edge labels.

## System Catalog Additions

### 1) sys.nosql_collections

Stores document collection metadata.

**Key Columns**
- `collection_id` UUID (PK)
- `schema_id` UUID (FK -> sys.schemas)
- `owner_id` UUID
- `collection_name` TEXT
- `storage_mode` TEXT  -- `table`, `specialized`
- `storage_table_id` UUID (nullable, if table-backed)
- `doc_id_column_id` UUID (nullable, if table-backed)
- `doc_column_id` UUID (nullable, if table-backed)
- `default_ttl_seconds` INTEGER (nullable)
- `validator_expr_oid` OID (nullable, TOAST)
- `options_oid` OID (nullable, TOAST JSON)
- `created_time` TIMESTAMP

**Notes**
- `options_oid` holds collection-level settings (sharding key, compression,
  change stream policy, etc).
- `doc_id_column_id` points at the stable document ID column (UUID).

### 2) sys.nosql_buckets

Stores key-value bucket metadata.

**Key Columns**
- `bucket_id` UUID (PK)
- `schema_id` UUID
- `owner_id` UUID
- `bucket_name` TEXT
- `storage_mode` TEXT  -- `table`, `specialized`
- `storage_table_id` UUID (nullable)
- `key_column_id` UUID (nullable)
- `value_column_id` UUID (nullable)
- `default_ttl_seconds` INTEGER (nullable)
- `options_oid` OID (nullable, TOAST JSON)
- `created_time` TIMESTAMP

**Notes**
- Buckets map naturally to a two-column table (key, value), but a specialized
  LSM-backed storage mode is permitted.

### 3) sys.nosql_column_families

Stores wide-column / column-family metadata.

**Key Columns**
- `family_id` UUID (PK)
- `schema_id` UUID
- `owner_id` UUID
- `family_name` TEXT
- `storage_mode` TEXT  -- `table`, `specialized`
- `storage_table_id` UUID (nullable)
- `partition_key_expr_oid` OID (nullable, TOAST)
- `clustering_key_expr_oid` OID (nullable, TOAST)
- `default_ttl_seconds` INTEGER (nullable)
- `compaction_policy_oid` OID (nullable, TOAST JSON)
- `options_oid` OID (nullable, TOAST JSON)
- `created_time` TIMESTAMP

**Notes**
- Partition and clustering definitions are stored as expressions, not just
  column lists, to allow computed keys.

### 4) sys.graphs

Stores graph metadata.

**Key Columns**
- `graph_id` UUID (PK)
- `schema_id` UUID
- `owner_id` UUID
- `graph_name` TEXT
- `storage_mode` TEXT  -- `table`, `specialized`
- `vertex_table_id` UUID (nullable)
- `edge_table_id` UUID (nullable)
- `options_oid` OID (nullable, TOAST JSON)
- `created_time` TIMESTAMP

### 5) sys.graph_vertex_labels

**Key Columns**
- `vertex_label_id` UUID (PK)
- `graph_id` UUID (FK -> sys.graphs)
- `label_name` TEXT
- `vertex_table_id` UUID (nullable)
- `options_oid` OID (nullable, TOAST JSON)
- `created_time` TIMESTAMP

### 6) sys.graph_edge_labels

**Key Columns**
- `edge_label_id` UUID (PK)
- `graph_id` UUID (FK -> sys.graphs)
- `label_name` TEXT
- `edge_table_id` UUID (nullable)
- `from_label_id` UUID (nullable)
- `to_label_id` UUID (nullable)
- `options_oid` OID (nullable, TOAST JSON)
- `created_time` TIMESTAMP

### 7) sys.graph_properties

Stores property metadata for vertices/edges.

**Key Columns**
- `property_id` UUID (PK)
- `graph_id` UUID
- `owner_type` TEXT  -- `vertex`, `edge`
- `owner_label_id` UUID
- `property_name` TEXT
- `property_type_id` UUID (FK -> sys.domains or sys.columns)
- `options_oid` OID (nullable, TOAST JSON)
- `created_time` TIMESTAMP

## Catalog Root Wiring (Planned)

Add catalog root page pointers for the new tables:

- `nosql_collections_page`
- `nosql_buckets_page`
- `nosql_column_families_page`
- `graphs_page`
- `graph_vertex_labels_page`
- `graph_edge_labels_page`
- `graph_properties_page`

These are Beta additions and must not affect Alpha bootstrapping.

## DDL Stubs (Syntax Placeholders)

The following statements are planned for Beta (syntax to be finalized):

- `CREATE COLLECTION <name> [WITH (...)]`
- `ALTER COLLECTION <name> SET (...)`
- `DROP COLLECTION <name>`

- `CREATE BUCKET <name> [WITH (...)]`
- `ALTER BUCKET <name> SET (...)`
- `DROP BUCKET <name>`

- `CREATE COLUMN FAMILY <name> (partition_key, clustering_key, ...)`
- `ALTER COLUMN FAMILY <name> SET (...)`
- `DROP COLUMN FAMILY <name>`

- `CREATE GRAPH <name> [WITH (...)]`
- `ALTER GRAPH <name> SET (...)`
- `DROP GRAPH <name>`

DDL operations must emit dependencies and permissions in the same way as
core SQL objects.

## Security and Permissions

All NoSQL objects are governed by standard ACLs:

- `USAGE` on collections/buckets/families/graphs
- `READ` / `WRITE` permissions for data access
- `ADMIN` for DDL and configuration changes

The permission model is identical to SQL objects and uses UUID references.

## Observability and Monitoring

Each object type should be visible via `sys.*` monitoring views (planned):

- `sys.nosql_collections`
- `sys.nosql_buckets`
- `sys.nosql_column_families`
- `sys.graphs` / `sys.graph_vertex_labels` / `sys.graph_edge_labels`

Additional runtime metrics (counts, TTL expirations, compaction) should be
surfaced through the monitoring metrics spec when implemented.

