# PostgreSQL Parser Implementation Guide

---

## IMPLEMENTATION STATUS: 🔴 NOT IMPLEMENTED - DESIGN SPECIFICATION ONLY

**Current Alpha Implementation:**
- Basic SQL parser exists (see `src/parser/parser_v2.cpp`)
- Parses SQL syntax only (no wire protocol)
- **No PostgreSQL wire protocol support**
- No system catalog emulation
- Listener/pool integration (no standalone Y-Valve router)
- Direct API only

**This specification describes a future Phase 2+ feature.**

---

## Overview

This document provides the complete implementation specification for the PostgreSQL
parser module (PLANNED) that interfaces with the listener/pool control plane.
The parser handles ALL PostgreSQL-specific logic, keeping the listener layer lean.

## Parser Responsibilities (PLANNED)

The PostgreSQL parser will be responsible for:
1. PostgreSQL wire protocol handling (v3.0)
2. System catalog emulation (pg_catalog, information_schema)
3. SQL dialect translation
4. Type mapping
5. Protocol-specific features (COPY, LISTEN/NOTIFY, etc.)
6. Version compatibility (initially 15.x, later 12.x-14.x)

## Implementation Structure

```c++
class PostgreSQLParser : public IParser {
private:
    // Core components
    int socket_fd;
    PGProtocolHandler protocol;
    PGSystemCatalog system_catalog;
    PGInformationSchema info_schema;
    PGSQLTranslator translator;
    PGTypeMapper type_mapper;
    PGStateMachine state;
    
    // Feature handlers
    PGCopyHandler copy_handler;
    PGNotifyHandler notify_handler;
    PGExtendedQueryHandler extended_query;
    PGPreparedStatementCache prepared_cache;
    
    // Version-specific adapter
    PGVersionAdapter version_adapter;
    
public:
    void takeOwnership(int socket, const uint8_t* data, size_t len) override;
    BLRResult parseToBLR(const std::string& sql) override;
    ResponsePacket handleCommand(const CommandPacket& cmd) override;
    DataPacket formatResults(const ResultSet& results) override;
};
```

## System Catalog Emulation

### Core System Tables

The parser maintains virtual versions of PostgreSQL system catalogs:

```c++
class PGSystemCatalog {
private:
    // Cached metadata from ScratchBird
    struct CachedMetadata {
        std::chrono::steady_clock::time_point last_refresh;
        std::vector<TableInfo> tables;
        std::vector<ColumnInfo> columns;
        std::vector<IndexInfo> indexes;
        std::vector<ConstraintInfo> constraints;
        std::vector<SchemaInfo> schemas;
        std::vector<TypeInfo> types;
    } cache;
    
    // Virtual table definitions
    std::map<std::string, VirtualTable> virtual_tables = {
        {"pg_class", createPgClass()},
        {"pg_attribute", createPgAttribute()},
        {"pg_namespace", createPgNamespace()},
        {"pg_type", createPgType()},
        {"pg_index", createPgIndex()},
        {"pg_constraint", createPgConstraint()},
        {"pg_database", createPgDatabase()},
        {"pg_tablespace", createPgTablespace()},
        {"pg_settings", createPgSettings()},
        {"pg_stat_activity", createPgStatActivity()},
        {"pg_stat_user_tables", createPgStatUserTables()},
        {"pg_stat_user_indexes", createPgStatUserIndexes()}
    };
    
public:
    ResultSet query(const std::string& sql) {
        // Parse the query
        auto parsed = parseSystemQuery(sql);
        
        // Check if we can handle it internally
        if (canHandleInternally(parsed)) {
            return executeInternally(parsed);
        }
        
        // Otherwise translate to ScratchBird system query
        return translateToScratchBird(parsed);
    }
    
private:
    VirtualTable createPgClass() {
        // pg_class columns (PostgreSQL 15 compatible)
        return VirtualTable{
            .columns = {
                {"oid", PG_OID},
                {"relname", PG_NAME},
                {"relnamespace", PG_OID},
                {"reltype", PG_OID},
                {"reloftype", PG_OID},
                {"relowner", PG_OID},
                {"relam", PG_OID},
                {"relfilenode", PG_OID},
                {"reltablespace", PG_OID},
                {"relpages", PG_INT4},
                {"reltuples", PG_FLOAT4},
                {"relallvisible", PG_INT4},
                {"reltoastrelid", PG_OID},
                {"relhasindex", PG_BOOL},
                {"relisshared", PG_BOOL},
                {"relpersistence", PG_CHAR},
                {"relkind", PG_CHAR},
                {"relnatts", PG_INT2},
                {"relchecks", PG_INT2},
                {"relhasrules", PG_BOOL},
                {"relhastriggers", PG_BOOL},
                {"relhassubclass", PG_BOOL},
                {"relrowsecurity", PG_BOOL},
                {"relforcerowsecurity", PG_BOOL},
                {"relispopulated", PG_BOOL},
                {"relreplident", PG_CHAR},
                {"relispartition", PG_BOOL},
                {"relrewrite", PG_OID},
                {"relfrozenxid", PG_XID},
                {"relminmxid", PG_XID},
                {"relacl", PG_ACLITEM_ARRAY},
                {"reloptions", PG_TEXT_ARRAY},
                {"relpartbound", PG_PG_NODE_TREE}
            },
            .generator = [this](const QueryPlan& plan) {
                return generatePgClassRows(plan);
            }
        };
    }
    
    ResultSet generatePgClassRows(const QueryPlan& plan) {
        ResultSet result;
        
        // Map ScratchBird tables to pg_class rows
        for (const auto& table : cache.tables) {
            Row row;
            row.add(generateOID(table.uuid));           // oid
            row.add(table.name);                        // relname
            row.add(generateOID(table.schema_uuid));    // relnamespace
            row.add(generateOID(table.type_uuid));      // reltype
            row.add(0);                                  // reloftype
            row.add(generateOID(table.owner_uuid));     // relowner
            row.add(getAccessMethod(table));            // relam
            row.add(table.file_node);                   // relfilenode
            row.add(generateOID(table.tablespace_uuid)); // reltablespace
            row.add(table.page_count);                  // relpages
            row.add(table.row_estimate);                // reltuples
            // ... map remaining fields
            
            if (matchesFilter(row, plan.filter)) {
                result.addRow(row);
            }
        }
        
        return result;
    }
};
```

### Information Schema

```c++
class PGInformationSchema {
private:
    std::map<std::string, VirtualTable> virtual_tables = {
        {"tables", createTableView()},
        {"columns", createColumnView()},
        {"views", createViewView()},
        {"schemata", createSchemaView()},
        {"routines", createRoutineView()},
        {"triggers", createTriggerView()},
        {"key_column_usage", createKeyColumnUsage()},
        {"referential_constraints", createReferentialConstraints()},
        {"table_constraints", createTableConstraints()},
        {"check_constraints", createCheckConstraints()}
    };
    
    VirtualTable createTableView() {
        return VirtualTable{
            .columns = {
                {"table_catalog", PG_NAME},
                {"table_schema", PG_NAME},
                {"table_name", PG_NAME},
                {"table_type", PG_VARCHAR},
                {"self_referencing_column_name", PG_NAME},
                {"reference_generation", PG_VARCHAR},
                {"user_defined_type_catalog", PG_NAME},
                {"user_defined_type_schema", PG_NAME},
                {"user_defined_type_name", PG_NAME},
                {"is_insertable_into", PG_YES_OR_NO},
                {"is_typed", PG_YES_OR_NO},
                {"commit_action", PG_VARCHAR}
            },
            .generator = [this](const QueryPlan& plan) {
                return generateInformationSchemaTables(plan);
            }
        };
    }
};
```

## SQL Translation

### PostgreSQL-Specific SQL Features

```c++
class PGSQLTranslator {
public:
    TranslationResult translate(const std::string& pg_sql) {
        TranslationResult result;
        
        // Handle PostgreSQL-specific syntax
        std::string sql = pg_sql;
        
        // 1. Dollar quoting
        sql = translateDollarQuoting(sql);
        
        // 2. :: casting
        sql = translateDoubleColonCast(sql);
        
        // 3. RETURNING clause
        sql = handleReturningClause(sql);
        
        // 4. Array syntax
        sql = translateArraySyntax(sql);
        
        // 5. LATERAL joins
        sql = translateLateralJoins(sql);
        
        // 6. ON CONFLICT
        sql = translateOnConflict(sql);
        
        // 7. System functions
        sql = translateSystemFunctions(sql);
        
        result.scratchbird_sql = sql;
        return result;
    }
    
private:
    std::string translateDollarQuoting(const std::string& sql) {
        // Convert $$ quotes to standard quotes
        // $$text$$ -> 'text'
        // $tag$text$tag$ -> 'text'
        std::regex dollar_quote(R"(\$([^$]*)\$(.*?)\$\1\$)");
        return std::regex_replace(sql, dollar_quote, "'$2'");
    }
    
    std::string translateDoubleColonCast(const std::string& sql) {
        // Convert :: casts to CAST()
        // column::type -> CAST(column AS type)
        std::regex cast_pattern(R"((\w+)::(\w+))");
        return std::regex_replace(sql, cast_pattern, "CAST($1 AS $2)");
    }
    
    std::string translateArraySyntax(const std::string& sql) {
        // ARRAY[1,2,3] -> ScratchBird array syntax
        // '{1,2,3}'::int[] -> ScratchBird array syntax
        // ANY(array) -> ScratchBird ANY syntax
        // array[1] -> array subscript
    }
};
```

## Protocol Handling

### Extended Query Protocol

```c++
class PGExtendedQueryHandler {
private:
    struct Portal {
        std::string name;
        PreparedStatement statement;
        std::vector<Value> parameters;
        int row_limit;
    };
    
    struct PreparedStatement {
        std::string name;
        std::string sql;
        BLRData blr;
        std::vector<DataType> param_types;
        std::vector<DataType> result_types;
    };
    
    std::map<std::string, PreparedStatement> prepared_statements;
    std::map<std::string, Portal> portals;
    
public:
    void handleParse(const ParseMessage& msg) {
        // Parse SQL and generate BLR
        auto blr = parseToBLR(msg.query);
        
        // Store prepared statement
        PreparedStatement stmt{
            .name = msg.statement_name,
            .sql = msg.query,
            .blr = blr,
            .param_types = msg.param_types
        };
        
        prepared_statements[msg.statement_name] = stmt;
        
        // Send ParseComplete
        sendParseComplete();
    }
    
    void handleBind(const BindMessage& msg) {
        // Get prepared statement
        auto& stmt = prepared_statements[msg.statement_name];
        
        // Create portal with bound parameters
        Portal portal{
            .name = msg.portal_name,
            .statement = stmt,
            .parameters = msg.parameters
        };
        
        portals[msg.portal_name] = portal;
        
        // Send BindComplete
        sendBindComplete();
    }
    
    void handleExecute(const ExecuteMessage& msg) {
        // Get portal
        auto& portal = portals[msg.portal_name];
        
        // Execute with parameters
        auto result = engine->execute(
            portal.statement.blr,
            portal.parameters,
            msg.row_limit
        );
        
        // Send results
        sendDataRows(result);
        sendCommandComplete(result.row_count);
    }
};
```

### COPY Protocol

```c++
class PGCopyHandler {
public:
    void handleCopyIn(const CopyInMessage& msg) {
        // Handle COPY FROM
        if (msg.format == COPY_FORMAT_BINARY) {
            handleBinaryCopy(msg);
        } else {
            handleTextCopy(msg);
        }
    }
    
    void handleCopyOut(const CopyOutMessage& msg) {
        // Handle COPY TO
        auto result = executeQuery(msg.query);
        
        if (msg.format == COPY_FORMAT_BINARY) {
            sendBinaryData(result);
        } else {
            sendTextData(result);
        }
    }
    
private:
    void handleBinaryCopy(const CopyInMessage& msg) {
        // PostgreSQL binary format
        // Header: PGCOPY\n\377\r\n\0
        // Flags: 4 bytes
        // Header extension: 4 bytes
        // Tuples: count + data
        
        BinaryReader reader(msg.data);
        
        // Verify header
        if (!reader.checkHeader("PGCOPY\n\377\r\n\0")) {
            throw ProtocolError("Invalid COPY binary header");
        }
        
        // Read flags
        uint32_t flags = reader.readUInt32();
        bool has_oids = flags & 0x00010000;
        
        // Read tuples
        while (!reader.eof()) {
            int16_t field_count = reader.readInt16();
            if (field_count == -1) break; // End marker
            
            Row row;
            for (int i = 0; i < field_count; i++) {
                int32_t field_length = reader.readInt32();
                if (field_length == -1) {
                    row.addNull();
                } else {
                    row.addBytes(reader.readBytes(field_length));
                }
            }
            
            insertRow(row);
        }
    }
};
```

## Type Mapping

```c++
class PGTypeMapper {
private:
    // PostgreSQL OID to ScratchBird type mapping
    std::map<uint32_t, SBType> oid_to_sb = {
        {16, SB_BOOLEAN},      // bool
        {17, SB_BLOB},         // bytea
        {18, SB_CHAR},         // char
        {19, SB_VARCHAR},      // name
        {20, SB_BIGINT},       // int8
        {21, SB_SMALLINT},     // int2
        {23, SB_INTEGER},      // int4
        {25, SB_TEXT},         // text
        {26, SB_INTEGER},      // oid
        {114, SB_JSON},        // json
        {700, SB_FLOAT},       // float4
        {701, SB_DOUBLE},      // float8
        {1082, SB_DATE},       // date
        {1083, SB_TIME},       // time
        {1114, SB_TIMESTAMP},  // timestamp
        {1184, SB_TIMESTAMPTZ},// timestamptz
        {1700, SB_DECIMAL},    // numeric
        {2950, SB_UUID},       // uuid
        {3802, SB_JSON}        // jsonb
    };
    
public:
    SBType mapFromPostgreSQL(uint32_t oid) {
        auto it = oid_to_sb.find(oid);
        if (it != oid_to_sb.end()) {
            return it->second;
        }
        
        // Check for array types (OID > 1000 usually)
        if (oid > 1000) {
            uint32_t base_oid = getBaseType(oid);
            if (oid_to_sb.count(base_oid)) {
                return SB_ARRAY(oid_to_sb[base_oid]);
            }
        }
        
        return SB_TEXT; // Default fallback
    }
    
    uint32_t mapToPostgreSQL(SBType type) {
        // Reverse mapping
        switch (type) {
            case SB_BOOLEAN: return 16;
            case SB_SMALLINT: return 21;
            case SB_INTEGER: return 23;
            case SB_BIGINT: return 20;
            // ... etc
        }
    }
};
```

## Version Compatibility

```c++
class PGVersionAdapter {
private:
    int major_version;
    int minor_version;
    
public:
    void initialize(const std::string& version_string) {
        // Parse "15.2" -> major=15, minor=2
        sscanf(version_string.c_str(), "%d.%d", &major_version, &minor_version);
    }
    
    bool supportsFeature(const std::string& feature) {
        if (feature == "MERGE") {
            return major_version >= 15;
        }
        if (feature == "SQL_JSON") {
            return major_version >= 14;
        }
        if (feature == "GENERATED_COLUMNS") {
            return major_version >= 12;
        }
        if (feature == "PROCEDURES") {
            return major_version >= 11;
        }
        return true; // Assume supported
    }
    
    std::string adaptSQL(const std::string& sql) {
        // Adapt SQL for older versions
        if (major_version < 15 && sql.find("MERGE") != std::string::npos) {
            return convertMergeToUpsert(sql);
        }
        return sql;
    }
};
```

## Testing

```c++
// Test system catalog emulation
TEST(PostgreSQLParser, SystemCatalogQueries) {
    PostgreSQLParser parser;
    
    // Test pg_class query
    auto result = parser.execute(R"(
        SELECT relname, relkind 
        FROM pg_catalog.pg_class 
        WHERE relnamespace = 'public'::regnamespace
    )");
    
    EXPECT_GT(result.row_count, 0);
    EXPECT_EQ(result.columns[0].name, "relname");
    EXPECT_EQ(result.columns[1].name, "relkind");
}

// Test COPY binary format
TEST(PostgreSQLParser, CopyBinary) {
    PostgreSQLParser parser;
    
    // Create test binary data
    std::vector<uint8_t> copy_data = createPGCopyBinary({
        {1, "test", 42},
        {2, "test2", 43}
    });
    
    parser.handleCopyIn("test_table", copy_data, COPY_FORMAT_BINARY);
    
    auto result = parser.execute("SELECT * FROM test_table");
    EXPECT_EQ(result.row_count, 2);
}

// Test version compatibility
TEST(PostgreSQLParser, VersionCompatibility) {
    // Test with PostgreSQL 14
    PostgreSQLParser parser14("14.5");
    EXPECT_FALSE(parser14.supportsFeature("MERGE"));
    
    // Test with PostgreSQL 15
    PostgreSQLParser parser15("15.2");
    EXPECT_TRUE(parser15.supportsFeature("MERGE"));
}
```

## Performance Considerations

1. **System Catalog Caching**: Cache system catalog data for 60 seconds
2. **Prepared Statement Cache**: LRU cache of 1000 statements
3. **Type Mapping Cache**: Pre-compute all standard type mappings
4. **Fast Path for Simple Queries**: Skip full parsing for simple SELECT/INSERT
5. **Batch Insert Optimization**: Use COPY protocol for bulk inserts

## Character Set and Timezone Handling

### Character Sets
External parsers must handle character set conversion between client and database:

**References**:
- [character_sets_and_collations.md](../types/character_sets_and_collations.md) - Character set and collation system
- Character set specification at connection, table, and column levels
- UTF-8 as default for all system objects
- Collation-aware string operations

**Client Responsibilities**:
```cpp
// Query connection charset
SELECT current_setting('client_encoding');

// Convert client data to database charset
auto converted = charset_manager.convert(
    client_data, client_charset,
    output, db_charset, &ctx
);
```

### Timezone Support
External parsers must handle timezone-aware timestamp operations:

**References**:
- [TIMEZONE_SYSTEM_CATALOG.md](../types/TIMEZONE_SYSTEM_CATALOG.md) - Complete timezone specification
- `pg_timezone` system catalog table
- `TIMESTAMP WITH TIME ZONE` type support
- `AT TIME ZONE` operator

**Key Implementation Points**:
1. **Storage**: All timestamps stored as GMT (int64_t microseconds since epoch)
2. **Display**: Use `timezone_hint` from column metadata or connection default
3. **Parsing**: Support ISO 8601 with timezone offsets (e.g., "2025-10-04 15:30:00-05:00")
4. **Formatting**: Apply timezone conversion only at presentation layer

**Client Responsibilities**:
```cpp
// Load timezone definitions
std::vector<CatalogManager::TimezoneInfo> timezones;
catalog->listTimezones(timezones, &ctx);

// Parse timestamp with timezone
auto gmt_ts = tz_manager.parseTimestamp(
    "2025-10-04 10:30:00-05:00",  // EST input
    conn_default_tz,               // Fallback
    &ctx
);

// Format for display
std::string display = tz_manager.formatTimestamp(
    gmt_ts,
    column_tz_hint != 0 ? column_tz_hint : conn_tz,
    true  // show offset
);
```

**SQL Commands for Timezone Management** (to be implemented):
```sql
-- Create timezone
CREATE TIMEZONE 'Custom/Zone' ABBREVIATION 'CZ' OFFSET +05:30;

-- Query timezones
SELECT * FROM pg_timezone WHERE observes_dst = true;

-- Use in queries
SELECT event_time AT TIME ZONE 'America/New_York' FROM events;
```

## Summary

This PostgreSQL parser implementation:
- Handles ALL PostgreSQL-specific logic
- Keeps listener/pool control plane minimal
- Provides full system catalog emulation
- Supports current version (15.x) with framework for older versions
- Enables transparent migration from PostgreSQL
- Maintains high performance through caching and optimization
- **Includes character set and timezone handling for internationalization**
- **References complete specifications for charset/collation and timezone systems**
