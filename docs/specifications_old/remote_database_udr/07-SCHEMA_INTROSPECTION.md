# Remote Database UDR - Schema Introspection

> Reference-only: Canonical UDR and live-migration behavior now lives in
> `ScratchBird/docs/specifications/Alpha Phase 2/11-Remote-Database-UDR-Specification.md`
> and `ScratchBird/docs/specifications/Alpha Phase 2/11h-Live-Migration-Emulated-Listener.md`.

## 1. Overview

Schema introspection enables automatic discovery and import of remote database schemas, supporting:
- Table and view discovery
- Column metadata extraction
- Index and constraint analysis
- Foreign key relationships
- Automatic foreign table creation

---

## 2. SchemaIntrospector Class

```cpp
class SchemaIntrospector {
public:
    explicit SchemaIntrospector(RemoteConnectionPoolRegistry* pool_registry);

    // Discover all schemas in remote database
    Result<std::vector<SchemaInfo>> discoverSchemas(
        const std::string& server_name,
        const std::string& local_user);

    // Discover tables in a specific schema
    Result<std::vector<TableInfo>> discoverTables(
        const std::string& server_name,
        const std::string& local_user,
        const std::string& remote_schema);

    // Get detailed table information
    Result<TableDetails> describeTable(
        const std::string& server_name,
        const std::string& local_user,
        const std::string& remote_schema,
        const std::string& remote_table);

    // Import remote schema as foreign tables
    Result<std::vector<ForeignTableDefinition>> importSchema(
        const std::string& server_name,
        const std::string& local_user,
        const std::string& remote_schema,
        const std::string& local_schema,
        const ImportOptions& options);

private:
    RemoteConnectionPoolRegistry* pool_registry_;
    std::unordered_map<RemoteDatabaseType, std::unique_ptr<ISchemaQuerier>>
        schema_queriers_;

    // Type mapping helpers
    ScratchBirdType mapRemoteType(
        RemoteDatabaseType db_type,
        uint32_t type_oid,
        int32_t type_modifier);
};
```

---

## 3. Data Structures

### 3.1 Schema Information

```cpp
struct SchemaInfo {
    std::string name;
    std::string owner;
    bool is_system;           // System schema (pg_catalog, sys, etc.)
    uint64_t table_count;     // Number of tables
    uint64_t view_count;      // Number of views
    std::string description;  // Schema comment/description
};

struct TableInfo {
    std::string schema_name;
    std::string table_name;
    TableType type;           // TABLE, VIEW, MATERIALIZED_VIEW, FOREIGN_TABLE
    std::string owner;
    uint64_t estimated_rows;
    uint64_t size_bytes;      // Estimated size (if available)
    std::string description;
    bool is_partitioned;
};

enum class TableType {
    TABLE,
    VIEW,
    MATERIALIZED_VIEW,
    FOREIGN_TABLE,
    TEMPORARY,
    SYSTEM_TABLE,
};
```

### 3.2 Table Details

```cpp
struct TableDetails {
    TableInfo info;
    std::vector<ColumnInfo> columns;
    std::vector<IndexInfo> indexes;
    std::vector<ForeignKeyInfo> foreign_keys;
    std::vector<CheckConstraint> check_constraints;
    std::optional<PrimaryKeyInfo> primary_key;

    // Partitioning info
    std::optional<PartitionInfo> partition_info;

    // For views
    std::optional<std::string> view_definition;
};

struct ColumnInfo {
    std::string name;
    uint32_t position;
    uint32_t remote_type_oid;
    std::string remote_type_name;
    ScratchBirdType mapped_type;
    int32_t type_modifier;      // Length, precision, etc.
    bool nullable;
    std::string default_value;
    bool is_identity;           // Auto-increment
    bool is_generated;          // Computed column
    std::string generated_expression;
    std::string description;
    std::string collation;
};

struct IndexInfo {
    std::string name;
    std::vector<std::string> columns;
    std::vector<bool> descending;  // Per-column sort order
    bool is_unique;
    bool is_primary;
    std::string index_type;        // btree, hash, gist, etc.
    std::string where_clause;      // Partial index predicate
    std::vector<std::string> include_columns;  // Covering index columns
};

struct ForeignKeyInfo {
    std::string name;
    std::vector<std::string> columns;
    std::string referenced_schema;
    std::string referenced_table;
    std::vector<std::string> referenced_columns;
    ForeignKeyAction on_update;
    ForeignKeyAction on_delete;
    bool is_deferrable;
    bool is_deferred;
};

struct CheckConstraint {
    std::string name;
    std::string expression;
    bool is_no_inherit;  // PostgreSQL-specific
};

struct PrimaryKeyInfo {
    std::string name;
    std::vector<std::string> columns;
};

struct PartitionInfo {
    PartitionStrategy strategy;  // RANGE, LIST, HASH
    std::vector<std::string> partition_columns;
    std::vector<PartitionBound> partitions;
};
```

---

## 4. Database-Specific Schema Queries

### 4.1 PostgreSQL Schema Queries

```cpp
class PostgreSQLSchemaQuerier : public ISchemaQuerier {
public:
    std::string listSchemasQuery() const override {
        return R"(
            SELECT
                nspname AS schema_name,
                pg_get_userbyid(nspowner) AS owner,
                nspname LIKE 'pg_%' OR nspname = 'information_schema' AS is_system,
                (SELECT COUNT(*) FROM pg_class c
                 WHERE c.relnamespace = n.oid AND c.relkind = 'r') AS table_count,
                (SELECT COUNT(*) FROM pg_class c
                 WHERE c.relnamespace = n.oid AND c.relkind = 'v') AS view_count,
                obj_description(n.oid, 'pg_namespace') AS description
            FROM pg_namespace n
            ORDER BY schema_name
        )";
    }

    std::string listTablesQuery(const std::string& schema) const override {
        return R"(
            SELECT
                schemaname AS schema_name,
                tablename AS table_name,
                'TABLE' AS table_type,
                tableowner AS owner,
                (SELECT reltuples::bigint FROM pg_class
                 WHERE oid = (schemaname || '.' || tablename)::regclass) AS estimated_rows,
                pg_total_relation_size(
                    (schemaname || '.' || tablename)::regclass) AS size_bytes,
                obj_description((schemaname || '.' || tablename)::regclass) AS description
            FROM pg_tables
            WHERE schemaname = $1
            UNION ALL
            SELECT
                schemaname, viewname, 'VIEW', viewowner, 0, 0,
                obj_description((schemaname || '.' || viewname)::regclass)
            FROM pg_views
            WHERE schemaname = $1
            UNION ALL
            SELECT
                schemaname, matviewname, 'MATERIALIZED_VIEW', matviewowner, 0,
                pg_total_relation_size((schemaname || '.' || matviewname)::regclass),
                obj_description((schemaname || '.' || matviewname)::regclass)
            FROM pg_matviews
            WHERE schemaname = $1
            ORDER BY table_name
        )";
    }

    std::string describeColumnsQuery(const std::string& schema,
                                      const std::string& table) const override {
        return R"(
            SELECT
                a.attname AS column_name,
                a.attnum AS position,
                a.atttypid AS type_oid,
                pg_catalog.format_type(a.atttypid, a.atttypmod) AS type_name,
                a.atttypmod AS type_modifier,
                NOT a.attnotnull AS nullable,
                pg_get_expr(d.adbin, d.adrelid) AS default_value,
                a.attidentity != '' AS is_identity,
                a.attgenerated != '' AS is_generated,
                pg_get_expr(d.adbin, d.adrelid) AS generated_expression,
                col_description(c.oid, a.attnum) AS description,
                CASE WHEN co.collname != 'default' THEN co.collname END AS collation
            FROM pg_attribute a
            JOIN pg_class c ON a.attrelid = c.oid
            JOIN pg_namespace n ON c.relnamespace = n.oid
            LEFT JOIN pg_attrdef d ON a.attrelid = d.adrelid AND a.attnum = d.adnum
            LEFT JOIN pg_collation co ON a.attcollation = co.oid
            WHERE n.nspname = $1
              AND c.relname = $2
              AND a.attnum > 0
              AND NOT a.attisdropped
            ORDER BY a.attnum
        )";
    }

    std::string describeIndexesQuery(const std::string& schema,
                                      const std::string& table) const override {
        return R"(
            SELECT
                i.relname AS index_name,
                array_agg(a.attname ORDER BY k.n) AS columns,
                array_agg(CASE WHEN ix.indoption[k.n-1] & 1 = 1 THEN true ELSE false END
                          ORDER BY k.n) AS descending,
                ix.indisunique AS is_unique,
                ix.indisprimary AS is_primary,
                am.amname AS index_type,
                pg_get_expr(ix.indpred, ix.indrelid) AS where_clause,
                ARRAY(SELECT attname FROM pg_attribute
                      WHERE attrelid = i.oid AND attnum > ix.indnkeyatts) AS include_columns
            FROM pg_index ix
            JOIN pg_class i ON ix.indexrelid = i.oid
            JOIN pg_class t ON ix.indrelid = t.oid
            JOIN pg_namespace n ON t.relnamespace = n.oid
            JOIN pg_am am ON i.relam = am.oid
            CROSS JOIN LATERAL unnest(ix.indkey) WITH ORDINALITY AS k(attnum, n)
            JOIN pg_attribute a ON a.attrelid = t.oid AND a.attnum = k.attnum
            WHERE n.nspname = $1
              AND t.relname = $2
            GROUP BY i.relname, ix.indisunique, ix.indisprimary, am.amname,
                     ix.indpred, ix.indrelid, i.oid, ix.indnkeyatts
            ORDER BY i.relname
        )";
    }

    std::string describeForeignKeysQuery(const std::string& schema,
                                          const std::string& table) const override {
        return R"(
            SELECT
                c.conname AS name,
                array_agg(a.attname ORDER BY array_position(c.conkey, a.attnum)) AS columns,
                nf.nspname AS referenced_schema,
                cf.relname AS referenced_table,
                array_agg(af.attname ORDER BY array_position(c.confkey, af.attnum))
                    AS referenced_columns,
                CASE c.confupdtype
                    WHEN 'a' THEN 'NO ACTION'
                    WHEN 'r' THEN 'RESTRICT'
                    WHEN 'c' THEN 'CASCADE'
                    WHEN 'n' THEN 'SET NULL'
                    WHEN 'd' THEN 'SET DEFAULT'
                END AS on_update,
                CASE c.confdeltype
                    WHEN 'a' THEN 'NO ACTION'
                    WHEN 'r' THEN 'RESTRICT'
                    WHEN 'c' THEN 'CASCADE'
                    WHEN 'n' THEN 'SET NULL'
                    WHEN 'd' THEN 'SET DEFAULT'
                END AS on_delete,
                c.condeferrable AS is_deferrable,
                c.condeferred AS is_deferred
            FROM pg_constraint c
            JOIN pg_class cl ON c.conrelid = cl.oid
            JOIN pg_namespace n ON cl.relnamespace = n.oid
            JOIN pg_class cf ON c.confrelid = cf.oid
            JOIN pg_namespace nf ON cf.relnamespace = nf.oid
            JOIN pg_attribute a ON a.attrelid = cl.oid AND a.attnum = ANY(c.conkey)
            JOIN pg_attribute af ON af.attrelid = cf.oid AND af.attnum = ANY(c.confkey)
            WHERE c.contype = 'f'
              AND n.nspname = $1
              AND cl.relname = $2
            GROUP BY c.conname, nf.nspname, cf.relname, c.confupdtype, c.confdeltype,
                     c.condeferrable, c.condeferred
        )";
    }
};
```

### 4.2 MySQL Schema Queries

```cpp
class MySQLSchemaQuerier : public ISchemaQuerier {
public:
    std::string listSchemasQuery() const override {
        return R"(
            SELECT
                SCHEMA_NAME AS schema_name,
                '' AS owner,
                SCHEMA_NAME IN ('mysql', 'information_schema', 'performance_schema', 'sys')
                    AS is_system,
                (SELECT COUNT(*) FROM information_schema.TABLES t
                 WHERE t.TABLE_SCHEMA = s.SCHEMA_NAME
                   AND t.TABLE_TYPE = 'BASE TABLE') AS table_count,
                (SELECT COUNT(*) FROM information_schema.TABLES t
                 WHERE t.TABLE_SCHEMA = s.SCHEMA_NAME
                   AND t.TABLE_TYPE = 'VIEW') AS view_count,
                '' AS description
            FROM information_schema.SCHEMATA s
            ORDER BY schema_name
        )";
    }

    std::string describeColumnsQuery(const std::string& schema,
                                      const std::string& table) const override {
        return R"(
            SELECT
                COLUMN_NAME AS column_name,
                ORDINAL_POSITION AS position,
                DATA_TYPE AS type_oid,
                COLUMN_TYPE AS type_name,
                CHARACTER_MAXIMUM_LENGTH AS type_modifier,
                IS_NULLABLE = 'YES' AS nullable,
                COLUMN_DEFAULT AS default_value,
                EXTRA LIKE '%auto_increment%' AS is_identity,
                GENERATION_EXPRESSION IS NOT NULL AS is_generated,
                GENERATION_EXPRESSION AS generated_expression,
                COLUMN_COMMENT AS description,
                COLLATION_NAME AS collation
            FROM information_schema.COLUMNS
            WHERE TABLE_SCHEMA = ?
              AND TABLE_NAME = ?
            ORDER BY ORDINAL_POSITION
        )";
    }

    std::string describeIndexesQuery(const std::string& schema,
                                      const std::string& table) const override {
        return R"(
            SELECT
                INDEX_NAME AS index_name,
                GROUP_CONCAT(COLUMN_NAME ORDER BY SEQ_IN_INDEX) AS columns,
                GROUP_CONCAT(CASE WHEN COLLATION = 'D' THEN 'true' ELSE 'false' END
                             ORDER BY SEQ_IN_INDEX) AS descending,
                NON_UNIQUE = 0 AS is_unique,
                INDEX_NAME = 'PRIMARY' AS is_primary,
                INDEX_TYPE AS index_type,
                '' AS where_clause,
                '' AS include_columns
            FROM information_schema.STATISTICS
            WHERE TABLE_SCHEMA = ?
              AND TABLE_NAME = ?
            GROUP BY INDEX_NAME, NON_UNIQUE, INDEX_TYPE
            ORDER BY INDEX_NAME
        )";
    }
};
```

---

## 5. Import Options

```cpp
struct ImportOptions {
    // Table filtering
    std::vector<std::string> include_tables;  // Empty = all
    std::vector<std::string> exclude_tables;
    bool include_views = true;
    bool include_materialized_views = true;

    // Column handling
    bool include_all_columns = true;
    std::unordered_map<std::string, std::vector<std::string>> column_includes;
    std::unordered_map<std::string, std::vector<std::string>> column_excludes;

    // Naming
    std::optional<std::string> table_prefix;
    std::optional<std::string> table_suffix;
    bool lowercase_names = false;
    bool uppercase_names = false;

    // Constraint handling
    bool import_not_null = true;
    bool import_defaults = false;  // Defaults usually don't translate well
    bool import_check_constraints = false;

    // Type mapping overrides
    std::unordered_map<std::string, ScratchBirdType> type_overrides;

    // DML support
    bool allow_insert = true;
    bool allow_update = true;
    bool allow_delete = true;

    // Performance hints
    uint64_t estimated_row_count_default = 1000;
    double startup_cost = 10.0;
    double per_row_cost = 0.01;
};
```

---

## 6. Schema Import Process

```cpp
Result<std::vector<ForeignTableDefinition>> SchemaIntrospector::importSchema(
    const std::string& server_name,
    const std::string& local_user,
    const std::string& remote_schema,
    const std::string& local_schema,
    const ImportOptions& options)
{
    // 1. Get connection
    auto conn = pool_registry_->acquire(server_name, local_user);
    if (!conn) return conn.error();

    auto db_type = conn->getDatabaseType();
    auto querier = schema_queriers_.at(db_type).get();

    // 2. Discover tables
    auto tables = discoverTables(server_name, local_user, remote_schema);
    if (!tables) return tables.error();

    // 3. Filter tables
    std::vector<TableInfo> filtered;
    for (const auto& table : *tables) {
        if (!matchesFilter(table, options)) continue;
        filtered.push_back(table);
    }

    // 4. Create foreign table definitions
    std::vector<ForeignTableDefinition> definitions;
    for (const auto& table : filtered) {
        auto details = describeTable(server_name, local_user,
                                     remote_schema, table.table_name);
        if (!details) {
            // Log warning but continue
            continue;
        }

        ForeignTableDefinition def;
        def.server_id = getServerId(server_name);
        def.local_schema = local_schema;
        def.local_name = transformName(table.table_name, options);
        def.remote_schema = remote_schema;
        def.remote_name = table.table_name;
        def.estimated_row_count = details->info.estimated_rows;
        def.startup_cost = options.startup_cost;
        def.per_row_cost = options.per_row_cost;
        def.updatable = (table.type == TableType::TABLE) &&
                        (options.allow_insert || options.allow_update ||
                         options.allow_delete);

        // Map columns
        for (const auto& col : details->columns) {
            if (!shouldIncludeColumn(table.table_name, col.name, options)) {
                continue;
            }

            ColumnMapping mapping;
            mapping.local_name = transformName(col.name, options);
            mapping.remote_name = col.name;
            mapping.remote_type_oid = col.remote_type_oid;

            // Check for type override
            auto it = options.type_overrides.find(col.remote_type_name);
            if (it != options.type_overrides.end()) {
                mapping.local_type = it->second;
            } else {
                mapping.local_type = mapRemoteType(db_type, col.remote_type_oid,
                                                    col.type_modifier);
            }

            mapping.nullable = col.nullable || !options.import_not_null;
            if (options.import_defaults) {
                mapping.default_value = col.default_value;
            }

            def.columns.push_back(mapping);
        }

        definitions.push_back(def);
    }

    return definitions;
}
```

---

## 7. SQL for Schema Import

### 7.1 IMPORT FOREIGN SCHEMA Statement

```sql
-- Import all tables from a remote schema
IMPORT FOREIGN SCHEMA public
    FROM SERVER legacy_pg
    INTO imported_schema;

-- Import specific tables only
IMPORT FOREIGN SCHEMA public
    LIMIT TO (users, orders, products)
    FROM SERVER legacy_pg
    INTO imported_schema;

-- Exclude certain tables
IMPORT FOREIGN SCHEMA public
    EXCEPT (logs, temp_data)
    FROM SERVER legacy_pg
    INTO imported_schema;

-- With options
IMPORT FOREIGN SCHEMA public
    FROM SERVER legacy_pg
    INTO imported_schema
    OPTIONS (
        import_views 'true',
        import_not_null 'true',
        updatable 'false',
        table_prefix 'legacy_'
    );
```

### 7.2 Generated DDL

The import process generates CREATE FOREIGN TABLE statements:

```sql
-- Generated from IMPORT FOREIGN SCHEMA
CREATE FOREIGN TABLE imported_schema.legacy_users (
    id INTEGER NOT NULL,
    username VARCHAR(255) NOT NULL,
    email VARCHAR(255),
    created_at TIMESTAMP NOT NULL,
    status VARCHAR(20)
)
SERVER legacy_pg
OPTIONS (
    schema_name 'public',
    table_name 'users'
);

CREATE FOREIGN TABLE imported_schema.legacy_orders (
    order_id BIGINT NOT NULL,
    user_id INTEGER NOT NULL,
    total DECIMAL(10,2),
    order_date DATE NOT NULL
)
SERVER legacy_pg
OPTIONS (
    schema_name 'public',
    table_name 'orders'
);
```

---

## 8. Refresh Schema

```cpp
Result<SchemaChanges> refreshSchema(
    const std::string& local_schema,
    const std::string& server_name)
{
    SchemaChanges changes;

    // 1. Get current local definitions
    auto local_tables = catalog_->getForeignTables(local_schema, server_name);

    // 2. Get current remote schema
    auto remote_tables = discoverTables(server_name, current_user_,
                                        getRemoteSchema(local_schema));

    // 3. Compare and identify changes
    for (const auto& remote : *remote_tables) {
        auto local = findLocalTable(local_tables, remote.table_name);

        if (!local) {
            // New table in remote
            changes.new_tables.push_back(remote.table_name);
        } else {
            // Compare columns
            auto remote_details = describeTable(server_name, current_user_,
                                                 remote.schema_name,
                                                 remote.table_name);
            compareColumns(local, *remote_details, changes);
        }
    }

    // Check for dropped tables
    for (const auto& local : local_tables) {
        if (!findRemoteTable(*remote_tables, local.remote_name)) {
            changes.dropped_tables.push_back(local.local_name);
        }
    }

    return changes;
}

struct SchemaChanges {
    std::vector<std::string> new_tables;
    std::vector<std::string> dropped_tables;
    std::vector<ColumnChange> new_columns;
    std::vector<ColumnChange> dropped_columns;
    std::vector<ColumnChange> type_changes;
    std::vector<ConstraintChange> constraint_changes;
};
```
