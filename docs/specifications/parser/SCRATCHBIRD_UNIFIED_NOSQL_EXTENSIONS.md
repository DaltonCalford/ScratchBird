# ScratchBird Unified NoSQL Extensions (SBQL-N)

**Status:** Draft (Beta)

## 1. Purpose

Define the unified ScratchBird NoSQL extension surface. This document is
additive to the ScratchBird SQL core language. The goal is a 1:1 feature set
so emulated NoSQL parsers can map directly into SBLR via the ScratchBird
extension surface.

## 2. Design Goals

- **SQL-first**: Core SQL remains in the SQL core language spec.
- **Additive**: Introduce NoSQL constructs as new statements or clauses.
- **Unified semantics**: Standardize missing vs null, array matching, TTL,
  graph traversal, vector search, and full-text search.
- **SBLR mapping**: Every construct maps to explicit SBLR opcodes and
  canonical plan nodes.
- **Catalog-driven**: All NoSQL objects are catalog-backed with UUIDs.

## 2.1 Alpha vs Beta Cross-Reference

**Alpha (existing SQL core):**
- Standard `SELECT/INSERT/UPDATE/DELETE` and PSQL as defined in
  `ScratchBird/docs/specifications/parser/SCRATCHBIRD_SQL_CORE_LANGUAGE.md`.

**Beta (NoSQL extensions in this doc):**
- `CREATE COLLECTION`, `CREATE BUCKET`, `CREATE COLUMN FAMILY`
- `CREATE GRAPH`, `CREATE RDF GRAPH`
- `CREATE MEASUREMENT`, `CREATE VECTOR COLLECTION`
- `MATCH` graph query clause
- `SEARCH TEXT`, `SEARCH VECTOR`
- `KV` statement family
- `PIPE` statement family
- `FROM COLLECTION/BUCKET/MEASUREMENT/GRAPH/RDF GRAPH/VECTOR COLLECTION/SEARCH INDEX`

## 3. Object Model (Unified)

The unified language operates on these object types (all schema-scoped):

- **Collection** (document)
- **Bucket** (key-value)
- **ColumnFamily** (wide-column)
- **Graph** (property graph)
- **RdfGraph** (RDF triple store)
- **SearchIndex** (full-text)
- **Measurement** (time-series)
- **VectorCollection** (vector similarity)

See `ScratchBird/docs/specifications/beta_requirements/nosql/NOSQL_SCHEMA_SPECIFICATION.md`
for catalog layout.

## 4. Data Semantics

### 4.1 Missing vs Null

- **Missing**: field does not exist.
- **Null**: field exists but is null.

Unified predicates:

- `IS MISSING`, `IS NOT MISSING`
- `IS NULL`, `IS NOT NULL`

### 4.2 Arrays

- Default predicate semantics are **ANY** (matches any element).
- Explicit array semantics:
  - `ALL(expr)`, `ANY(expr)`, `NONE(expr)`
  - `ELEMENTS(expr)` for explicit element scans

### 4.3 TTL and Retention

- TTL is supported for collections, buckets, column families, measurements.
- Retention policies are catalog objects referenced by measurements.

## 5. Top-Level Statements (Extensions)

The unified language adds the following statement families.

### 5.1 NoSQL DDL

```ebnf
create_nosql  = create_collection | create_bucket | create_column_family
              | create_graph | create_rdf_graph
              | create_measurement | create_vector_collection ;

create_collection = "CREATE" "COLLECTION" name
                    [ "WITH" options ] ;

create_bucket = "CREATE" "BUCKET" name
                [ "WITH" options ] ;

create_column_family = "CREATE" "COLUMN" "FAMILY" name
                        "(" cf_key_defs ")"
                        [ "WITH" options ] ;

create_graph  = "CREATE" "GRAPH" name [ "WITH" options ] ;
create_rdf_graph = "CREATE" "RDF" "GRAPH" name [ "WITH" options ] ;

create_measurement = "CREATE" "MEASUREMENT" name
                      "(" time_def "," tag_defs "," field_defs ")"
                      [ "WITH" options ] ;

create_vector_collection = "CREATE" "VECTOR" "COLLECTION" name
                           "(" vector_def "," field_defs ")"
                           [ "WITH" options ] ;
```

### 5.2 Graph Query

```ebnf
match_stmt    = "MATCH" pattern
                [ "WHERE" expr ]
                "RETURN" return_items
                [ order_by ] [ limit ] ;
```

### 5.3 Vector Search

```ebnf
vector_search = "SEARCH" "VECTOR" "FROM" name
                "USING" vector_expr
                "TOPK" number
                [ "WHERE" expr ]
                [ "RETURN" return_items ] ;
```

### 5.4 Full-Text Search

```ebnf
text_search   = "SEARCH" "TEXT" "FROM" name
                "QUERY" text_query
                [ "WHERE" expr ]
                [ "RETURN" return_items ] ;
```

### 5.5 Key-Value Operations

```ebnf
kv_stmt       = "KV" kv_op "IN" name kv_args ;
kv_op         = "GET" | "SET" | "DEL" | "SCAN" ;
```

### 5.6 Pipeline Form (Optional)

The pipeline form mirrors AQL/Flux style but is compiled into SQL plan nodes.

```ebnf
pipe_stmt     = "PIPE" pipe_stage { "|" pipe_stage } ;
pipe_stage    = "FROM" name
              | "FILTER" expr
              | "LET" name "=" expr
              | "GROUP" group_list
              | "RETURN" return_items ;
```

## 6. Unified SELECT Extensions

ScratchBird SQL SELECT gains these optional clauses:

- `FROM COLLECTION name` (document collections)
- `FROM BUCKET name` (key-value)
- `FROM MEASUREMENT name` (time-series)
- `FROM GRAPH name` (property graph, MATCH syntax allowed)
- `FROM RDF GRAPH name` (SPARQL-algebra backend)
- `FROM VECTOR COLLECTION name`
- `FROM SEARCH INDEX name`

This keeps SQL as the primary surface while allowing NoSQL targeting.

## 7. Mapping to SBLR

Each construct maps to a canonical plan node, ensuring 1:1 emulation.

| Unified Construct | Plan Node | SBLR Extension | Notes |
|------------------|-----------|----------------|-------|
| COLLECTION scan | DOC_SCAN | `OP_DOC_SCAN` | JSON path predicates
| BUCKET get/set | KV_OP | `OP_KV_GET/SET/DEL` | TTL handled in storage
| COLUMN FAMILY scan | CF_SCAN | `OP_CF_SCAN` | Partition/clustering rules
| GRAPH MATCH | GRAPH_MATCH | `OP_GRAPH_MATCH` | Pattern -> traversal plan
| RDF GRAPH | RDF_MATCH | `OP_RDF_MATCH` | SPARQL algebra
| TEXT SEARCH | FTS_QUERY | `OP_FTS_QUERY` | Full-text scoring
| VECTOR SEARCH | VECTOR_KNN | `OP_VECTOR_KNN` | HNSW/IVF
| MEASUREMENT scan | TS_SCAN | `OP_TS_SCAN` | Time + tags
| PIPE stages | PIPE_* | `OP_PIPE_STAGE` | Lowered to plan nodes

## 8. Emulation Mapping (Examples)

| Dialect | Emulated Form | Unified Form |
|---------|---------------|--------------|
| MongoDB find | filter + projection | `SELECT ... FROM COLLECTION` + JSON predicates |
| MongoDB aggregate | pipeline | `PIPE` stages or SELECT + GROUP/JSON ops |
| Mango find | selector | `SELECT ... FROM COLLECTION` + JSON predicates |
| N1QL | SQL++ | SQL + `UNNEST` + `USE KEYS` hints |
| AQL | pipeline | `PIPE` stages |
| Cypher | MATCH | `MATCH ... RETURN` |
| Gremlin | traversal | `MATCH` or `PIPE` traversal stages |
| SPARQL | graph pattern | `FROM RDF GRAPH` + graph pattern lowering |
| Elasticsearch DSL | bool query | `SEARCH TEXT` + structured filters |
| Lucene query | query string | `SEARCH TEXT` with query string |
| InfluxQL | time-series SQL | `FROM MEASUREMENT` + `GROUP BY time()` |
| Flux | pipeline | `PIPE` stages |
| PromQL | metric selector | `FROM MEASUREMENT` + range functions |
| Milvus | vector search | `SEARCH VECTOR` + filters |
| Weaviate | GraphQL | `SEARCH VECTOR` + filters + projection |

## 9. Usage Guidance

- Prefer SQL SELECT extensions when possible; they integrate with existing
  optimization and permissions.
- Use `MATCH` for graph semantics that cannot be expressed in SQL joins.
- Use `SEARCH TEXT` for relevance-ranked search; use SQL predicates for strict
  filtering.
- Use `SEARCH VECTOR` for semantic similarity; combine with structured WHERE.

## 10. Compatibility Notes

- This language is **additive**: existing SQL remains valid.
- NoSQL objects map to catalog entries and can be accessed via SQL for
  inspection and migration.
- Emulated parsers should map to these constructs instead of inventing
  separate bytecode paths.
