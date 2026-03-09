# Specification: CREATE INDEX Statement

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | parser |
| **Spec Version** | 1.0.0 |
| **Status** | 🟢 Approved |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird v3.0 |
| **Authors** | ScratchBird Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:841`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:3312`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_ddl.cpp`

## Synopsis

The CREATE INDEX statement creates an index on one or more columns of a table. ScratchBird supports 58+ index types spanning traditional B-trees to specialized vector search indexes (HNSW, IVF, DiskANN), full-text search, and database-specific indexes.

## Scope

### In Scope

- 58+ index types (BTREE, HASH, HNSW, GIN, GIST, etc.)
- Unique and partial indexes
- Expression indexes
- Covering indexes (INCLUDE columns)
- Concurrent index creation
- Index options (bloom filters, FPR settings)

### Out of Scope

- Index storage parameters (see storage specs)
- Index maintenance operations (see ALTER INDEX)

## Background

Indexes are critical for query performance. ScratchBird's parser recognizes a comprehensive set of index types from multiple database engines, normalized into a unified IndexType enum for the semantic analyzer.

## Specification

### EBNF Grammar

```ebnf
create_index_stmt ::=
    "CREATE" [ "UNIQUE" ] [ "CONCURRENTLY" ] "INDEX"
    [ "IF" "NOT" "EXISTS" ]
    [ index_name ]
    "ON" schema_path
    [ "USING" index_method ]
    "(" index_column ("," index_column )* ")"
    [ "INCLUDE" "(" identifier_list ")" ]
    [ "WHERE" expression ]
    [ "WITH" "(" index_option ("," index_option )* ")" ]
    [ "TABLESPACE" schema_path ]

index_column ::=
    ( identifier | expression )
    [ "COLLATE" identifier ]
    [ opclass ]
    [ "ASC" | "DESC" ]
    [ "NULLS" "FIRST" | "NULLS" "LAST" ]

index_method ::=
    "BTREE" | "HASH" | "HNSW" | "FULLTEXT" | "GIN" | "GIST" | "BRIN" |
    "RTREE" | "SPGIST" | "BITMAP" | "COLUMNSTORE" | "LSM" | "IVF" |
    "ZONEMAP" | "ART" | "BLOOM" | "VECTOR_FLAT" | "VECTOR_BIN_FLAT" |
    "IVF_FLAT" | "BIN_IVF_FLAT" | "IVF_PQ" | "IVF_SQ8" | "IVF_SQ8_HYBRID" |
    "RHNSW_PQ" | "RHNSW_SQ" | "ANNOY" | "NSG" | "DISKANN" | "SCANN" |
    "GPU_CAGRA" | "MINHASH_LSH" | "SPARSE_INVERTED" | "SPARSE_WAND" |
    "TRIE" | "INVERTED" | "STL_SORT" | "NGRAM" | "MONGODB_2D" |
    "MONGODB_2DSPHERE" | "MONGODB_2DSPHERE_BUCKET" | "MONGODB_GEO_HAYSTACK" |
    "MONGODB_WILDCARD" | "MONGODB_ENCRYPTED_RANGE" | "NEO4J_LOOKUP" |
    "NEO4J_TEXT" | "NEO4J_RANGE" | "NEO4J_POINT" | "NEO4J_VECTOR" |
    "CASSANDRA_SASI" | "CASSANDRA_SAI" | "REDIS_STRING" | "REDIS_HASH" |
    "REDIS_LIST" | "REDIS_SET" | "REDIS_ZSET" | "REDIS_STREAM" |
    "REDIS_BITMAP" | "REDIS_HLL" | "REDIS_GEO"

index_option ::=
    identifier "=" ( identifier | numeric_literal | string_literal )
```

### Index Types Reference

| Category | Types |
|----------|-------|
| **Standard** | BTREE, HASH |
| **Vector Search** | HNSW, IVF, IVF_FLAT, IVF_PQ, IVF_SQ8, IVF_SQ8_HYBRID, RHNSW_PQ, RHNSW_SQ, ANNOY, NSG, DISKANN, SCANN, GPU_CAGRA, VECTOR_FLAT, VECTOR_BIN_FLAT, NEO4J_VECTOR |
| **Full-Text** | FULLTEXT, GIN, GIST, INVERTED, SPARSE_INVERTED, SPARSE_WAND, NGRAM, NEO4J_TEXT, TRIE |
| **PostgreSQL** | BRIN, RTREE, SPGIST, BTREE, HASH, GIN, GIST |
| **Specialized** | BITMAP, COLUMNSTORE, LSM, ZONEMAP, ART, BLOOM, MINHASH_LSH |
| **MongoDB** | MONGODB_2D, MONGODB_2DSPHERE, MONGODB_2DSPHERE_BUCKET, MONGODB_GEO_HAYSTACK, MONGODB_WILDCARD, MONGODB_ENCRYPTED_RANGE |
| **Neo4j** | NEO4J_LOOKUP, NEO4J_TEXT, NEO4J_RANGE, NEO4J_POINT |
| **Cassandra** | CASSANDRA_SASI, CASSANDRA_SAI |
| **Redis** | REDIS_STRING, REDIS_HASH, REDIS_LIST, REDIS_SET, REDIS_ZSET, REDIS_STREAM, REDIS_BITMAP, REDIS_HLL, REDIS_GEO |

### AST Node Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:841
class CreateIndexStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreateIndexStmt; }
    
    bool unique = false;
    bool concurrent = false;
    bool if_not_exists = false;

    StringPool::StringId index_name = StringPool::INVALID_ID;
    SchemaPath table_path;

    IndexType index_type = IndexType::BTREE;
    StringPool::StringId index_method_name = StringPool::INVALID_ID;
    std::vector<IndexColumn> columns;
    IndexOptions options;
    std::vector<IndexOptionAssignment> option_assignments;

    // Partial index
    Expression* where_clause = nullptr;

    // Include columns (covering index)
    std::vector<StringPool::StringId> include_columns;

    // Storage
    SchemaPath tablespace;
    bool has_tablespace = false;
};

// Index column specification
struct IndexColumn {
    StringPool::StringId column = StringPool::INVALID_ID;
    Expression* expr = nullptr;  // For expression indexes
    bool ascending = true;
    bool nulls_first = false;
    bool nulls_last = false;
    StringPool::StringId opclass = StringPool::INVALID_ID;
};

// Index options
struct IndexOptions {
    bool bloom_filter_set = false;
    bool bloom_filter_enabled = false;
    bool bloom_fpr_set = false;
    double bloom_fpr = 0.01;
};

// Index option assignment
struct IndexOptionAssignment {
    StringPool::StringId option_name = StringPool::INVALID_ID;
    StringPool::StringId option_value = StringPool::INVALID_ID;
};

// IndexType enum (58 types)
enum class IndexType : uint8_t {
    BTREE = 0x00, HASH = 0x01, HNSW = 0x02, FULLTEXT = 0x03,
    GIN = 0x04, GIST = 0x05, BRIN = 0x06, RTREE = 0x07,
    SPGIST = 0x08, BITMAP = 0x09, COLUMNSTORE = 0x0A, LSM = 0x0B,
    IVF = 0x0C, ZONEMAP = 0x0D, ART = 0x0E, BLOOM = 0x0F,
    VECTOR_FLAT = 0x10, VECTOR_BIN_FLAT = 0x11, IVF_FLAT = 0x12,
    BIN_IVF_FLAT = 0x13, IVF_PQ = 0x14, IVF_SQ8 = 0x15,
    IVF_SQ8_HYBRID = 0x16, RHNSW_PQ = 0x17, RHNSW_SQ = 0x18,
    ANNOY = 0x19, NSG = 0x1A, DISKANN = 0x1B, SCANN = 0x1C,
    GPU_CAGRA = 0x1D, MINHASH_LSH = 0x1E, SPARSE_INVERTED = 0x1F,
    SPARSE_WAND = 0x20, TRIE = 0x21, INVERTED = 0x22,
    STL_SORT = 0x23, NGRAM = 0x24, MONGODB_2D = 0x25,
    MONGODB_2DSPHERE = 0x26, MONGODB_2DSPHERE_BUCKET = 0x27,
    MONGODB_GEO_HAYSTACK = 0x28, MONGODB_WILDCARD = 0x29,
    MONGODB_ENCRYPTED_RANGE = 0x2A, NEO4J_LOOKUP = 0x2B,
    NEO4J_TEXT = 0x2C, NEO4J_RANGE = 0x2D, NEO4J_POINT = 0x2E,
    NEO4J_VECTOR = 0x2F, CASSANDRA_SASI = 0x30, CASSANDRA_SAI = 0x31,
    REDIS_STRING = 0x32, REDIS_HASH = 0x33, REDIS_LIST = 0x34,
    REDIS_SET = 0x35, REDIS_ZSET = 0x36, REDIS_STREAM = 0x37,
    REDIS_BITMAP = 0x38, REDIS_HLL = 0x39, REDIS_GEO = 0x3A
};
```

### Semantic Binding Rules

1. **Table Resolution**: Target table must exist in catalog
2. **Column Resolution**: Index columns must exist in target table
3. **Expression Validity**: Expression indexes must have valid expressions
4. **Type Compatibility**: Index type must support column data types
5. **Unique Constraint**: UNIQUE indexes enforce uniqueness at DML time

## Examples

```sql
-- Basic B-tree index
CREATE INDEX idx_users_email ON users(email);

-- Unique index
CREATE UNIQUE INDEX idx_users_username ON users(username);

-- Composite index with sort order
CREATE INDEX idx_orders_date_amount ON orders(order_date DESC, amount ASC NULLS LAST);

-- Expression index
CREATE INDEX idx_users_lower_email ON users(LOWER(email));

-- Partial index
CREATE INDEX idx_active_users ON users(email) WHERE status = 'active';

-- Covering index
CREATE INDEX idx_orders_covering ON orders(user_id) INCLUDE (order_date, total);

-- Vector index (HNSW for similarity search)
CREATE INDEX idx_embeddings_hnsw ON items USING HNSW (embedding vector_cosine_ops);

-- Full-text index
CREATE INDEX idx_documents_fts ON documents USING FULLTEXT (content);

-- GIN index for JSONB
CREATE INDEX idx_data_gin ON events USING GIN (payload);
```

## Related Specifications

- [stmt_alter_index.md](./stmt_alter_index.md) - Index modification
- [stmt_drop_index.md](./stmt_drop_index.md) - Index removal
- [stmt_create_table.md](./stmt_create_table.md) - Table creation

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
