# NoSQL Schema Specification (Beta)

**Status:** Draft (Beta)

## 1. Purpose

Define a proposed system catalog schema that can represent the major NoSQL
models (document, key-value, wide-column, graph, RDF, search, time-series,
vector). This specification is designed to align with ScratchBird's UUID-based
catalog and provide a unified foundation for Beta implementation.

## 2. Design Principles

- **UUID-first**: All objects use UUID identifiers; names are metadata only.
- **Schema-scoped**: NoSQL objects live under a ScratchBird schema.
- **Table-backed by default**: Most models can map to tables; specialized
  storage is permitted via `storage_mode` and `storage_table_id`.
- **Permissioned**: All NoSQL objects are governed by `sys.permissions` and
  tracked in `sys.dependencies`.
- **Options as JSON**: Dialect-specific settings are stored in TOAST JSON.

## 3. Common Columns (Pattern)

Most NoSQL catalog tables use this pattern:

- `*_id` UUID (PK)
- `schema_id` UUID (FK -> sys.schemas)
- `owner_id` UUID
- `name` TEXT
- `storage_mode` TEXT  -- `table` or `specialized`
- `storage_table_id` UUID (nullable)
- `options_oid` OID (nullable, TOAST JSON)
- `created_time` TIMESTAMP

## 4. Document Model (Collections)

### 4.1 sys.nosql_collections

- `collection_id` UUID (PK)
- `schema_id` UUID
- `owner_id` UUID
- `collection_name` TEXT
- `storage_mode` TEXT
- `storage_table_id` UUID (nullable)
- `doc_id_column_id` UUID (nullable)
- `doc_column_id` UUID (nullable) -- JSON/JSONB
- `default_ttl_seconds` INTEGER (nullable)
- `validator_expr_oid` OID (nullable)
- `options_oid` OID (nullable)
- `created_time` TIMESTAMP

**Table-backed mapping (recommended):**
- `_id` UUID PRIMARY KEY
- `doc` JSONB
- `created_at` TIMESTAMP
- `updated_at` TIMESTAMP

### 4.2 sys.nosql_collection_indexes (optional helper)

- `index_id` UUID (FK -> sys.indexes)
- `collection_id` UUID
- `index_type` TEXT  -- `json_path`, `fulltext`, etc
- `options_oid` OID

## 5. Key-Value Model (Buckets)

### 5.1 sys.nosql_buckets

- `bucket_id` UUID (PK)
- `schema_id` UUID
- `owner_id` UUID
- `bucket_name` TEXT
- `storage_mode` TEXT
- `storage_table_id` UUID (nullable)
- `key_column_id` UUID (nullable)
- `value_column_id` UUID (nullable)
- `default_ttl_seconds` INTEGER (nullable)
- `options_oid` OID (nullable)
- `created_time` TIMESTAMP

**Table-backed mapping:**
- `k` VARBINARY/TEXT
- `v` BLOB/JSONB
- `expires_at` TIMESTAMP (optional)

## 6. Wide-Column / Column-Family

### 6.1 sys.nosql_column_families

- `family_id` UUID (PK)
- `schema_id` UUID
- `owner_id` UUID
- `family_name` TEXT
- `storage_mode` TEXT
- `storage_table_id` UUID (nullable)
- `partition_key_expr_oid` OID (nullable)
- `clustering_key_expr_oid` OID (nullable)
- `default_ttl_seconds` INTEGER (nullable)
- `compaction_policy_oid` OID (nullable)
- `options_oid` OID (nullable)
- `created_time` TIMESTAMP

### 6.2 sys.nosql_cf_columns (optional helper)

- `family_id` UUID
- `column_name` TEXT
- `column_type_id` UUID (FK -> sys.domains)
- `is_static` BOOLEAN
- `default_ttl_seconds` INTEGER (nullable)

## 7. Graph Model (Property Graph)

### 7.1 sys.graphs

- `graph_id` UUID (PK)
- `schema_id` UUID
- `owner_id` UUID
- `graph_name` TEXT
- `storage_mode` TEXT
- `vertex_table_id` UUID (nullable)
- `edge_table_id` UUID (nullable)
- `options_oid` OID (nullable)
- `created_time` TIMESTAMP

### 7.2 sys.graph_vertex_labels

- `vertex_label_id` UUID (PK)
- `graph_id` UUID
- `label_name` TEXT
- `vertex_table_id` UUID (nullable)
- `options_oid` OID (nullable)
- `created_time` TIMESTAMP

### 7.3 sys.graph_edge_labels

- `edge_label_id` UUID (PK)
- `graph_id` UUID
- `label_name` TEXT
- `edge_table_id` UUID (nullable)
- `from_label_id` UUID (nullable)
- `to_label_id` UUID (nullable)
- `options_oid` OID (nullable)
- `created_time` TIMESTAMP

### 7.4 sys.graph_properties

- `property_id` UUID (PK)
- `graph_id` UUID
- `owner_type` TEXT  -- `vertex` or `edge`
- `owner_label_id` UUID
- `property_name` TEXT
- `property_type_id` UUID
- `options_oid` OID (nullable)
- `created_time` TIMESTAMP

## 8. RDF Triple Store (SPARQL)

### 8.1 sys.rdf_graphs

- `rdf_graph_id` UUID (PK)
- `schema_id` UUID
- `owner_id` UUID
- `graph_name` TEXT
- `storage_mode` TEXT
- `triple_table_id` UUID (nullable)
- `options_oid` OID (nullable)
- `created_time` TIMESTAMP

### 8.2 sys.rdf_prefixes

- `rdf_graph_id` UUID
- `prefix` TEXT
- `iri` TEXT

**Table-backed mapping (recommended):**
- `subject` TEXT
- `predicate` TEXT
- `object` TEXT
- `graph_id` UUID (optional for named graphs)

## 9. Search Engines (Full-Text)

### 9.1 sys.search_analyzers

- `analyzer_id` UUID (PK)
- `schema_id` UUID
- `name` TEXT
- `tokenizer` TEXT
- `filters` TEXT
- `options_oid` OID (nullable)

### 9.2 sys.search_fields

- `index_id` UUID (FK -> sys.indexes)
- `field_name` TEXT
- `analyzer_id` UUID
- `options_oid` OID (nullable)

## 10. Time-Series Model

### 10.1 sys.timeseries_measurements

- `measurement_id` UUID (PK)
- `schema_id` UUID
- `owner_id` UUID
- `measurement_name` TEXT
- `storage_mode` TEXT
- `storage_table_id` UUID (nullable)
- `time_column_id` UUID
- `retention_policy_id` UUID (nullable)
- `options_oid` OID (nullable)
- `created_time` TIMESTAMP

### 10.2 sys.timeseries_tags

- `measurement_id` UUID
- `tag_name` TEXT
- `tag_column_id` UUID

### 10.3 sys.timeseries_retention_policies

- `retention_policy_id` UUID (PK)
- `policy_name` TEXT
- `duration_seconds` INTEGER
- `downsample_policy_oid` OID (nullable)

## 11. Vector Model

### 11.1 sys.vector_collections

- `vector_collection_id` UUID (PK)
- `schema_id` UUID
- `owner_id` UUID
- `collection_name` TEXT
- `storage_mode` TEXT
- `storage_table_id` UUID (nullable)
- `vector_column_id` UUID
- `dimensions` INTEGER
- `metric_type` TEXT  -- `L2`, `COSINE`, `IP`
- `options_oid` OID (nullable)
- `created_time` TIMESTAMP

### 11.2 sys.vector_indexes (optional helper)

- `index_id` UUID (FK -> sys.indexes)
- `vector_collection_id` UUID
- `index_type` TEXT  -- `hnsw`, `ivf`, etc
- `options_oid` OID (nullable)

## 12. Catalog Root Wiring (Beta)

The catalog root page includes placeholders for:

- `nosql_collections_page`
- `nosql_buckets_page`
- `nosql_column_families_page`
- `graphs_page`
- `graph_vertex_labels_page`
- `graph_edge_labels_page`
- `graph_properties_page`

Additional Beta root entries required by this spec:

- `rdf_graphs_page`
- `rdf_prefixes_page`
- `search_analyzers_page`
- `search_fields_page`
- `timeseries_measurements_page`
- `timeseries_tags_page`
- `timeseries_retention_policies_page`
- `vector_collections_page`
- `vector_indexes_page`

## 13. Compatibility Notes

- All NoSQL objects are compatible with UUID-based permissions and
  dependency tracking.
- Table-backed mappings allow direct SQL access for inspection and
  migration.
- Specialized storage modes are optional and can be introduced incrementally
  per model.

