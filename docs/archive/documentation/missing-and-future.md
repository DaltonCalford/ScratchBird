### Missing Features and Future Roadmap

**What it is**

This document outlines features that are currently not implemented in ScratchBird but are planned for future releases, as well as known limitations and workarounds. It serves as a roadmap for development priorities and helps users understand the current capabilities and future direction of the database system.

**Why it matters**

- **Planning**: Helps users make informed decisions about adoption
- **Workarounds**: Provides alternatives for missing features
- **Contributions**: Guides community contributions and priorities
- **Expectations**: Sets realistic expectations for capabilities
- **Migration**: Assists in planning migrations from other systems

**How to use it**

Review this document when evaluating ScratchBird for your use case, planning migrations, or encountering limitations. Check the version-specific sections for features added in recent releases. Consult the workarounds section for alternative approaches to missing functionality.

## Currently Missing Features

### SQL Standard Compliance

#### Missing SQL Features

```sql
-- LATERAL joins (not yet supported)
-- Workaround: Use correlated subqueries or CTEs
SELECT * FROM orders o,
    LATERAL (SELECT * FROM order_items WHERE order_id = o.id) oi;
-- Currently use:
SELECT * FROM orders o
JOIN order_items oi ON oi.order_id = o.id;

-- GROUPING SETS (planned for v2.0)
-- Workaround: Use UNION ALL
SELECT region, product, SUM(sales)
FROM sales_data
GROUP BY GROUPING SETS ((region), (product), ());
-- Currently use:
SELECT region, NULL AS product, SUM(sales) FROM sales_data GROUP BY region
UNION ALL
SELECT NULL, product, SUM(sales) FROM sales_data GROUP BY product
UNION ALL
SELECT NULL, NULL, SUM(sales) FROM sales_data;

-- PIVOT/UNPIVOT (planned for v2.1)
-- Workaround: Use conditional aggregation
PIVOT (SUM(amount) FOR month IN ('Jan', 'Feb', 'Mar'));
-- Currently use:
SELECT 
    SUM(CASE WHEN month = 'Jan' THEN amount END) AS jan,
    SUM(CASE WHEN month = 'Feb' THEN amount END) AS feb,
    SUM(CASE WHEN month = 'Mar' THEN amount END) AS mar
FROM sales;
```

#### Window Function Limitations

```sql
-- Missing window functions (planned for v1.5)
- PERCENT_RANK()
- CUME_DIST()
- NTH_VALUE()
- FIRST_VALUE() with IGNORE NULLS
- LAST_VALUE() with IGNORE NULLS

-- Frame clause limitations
- RANGE with offset (only CURRENT ROW supported)
- GROUPS frame type
- EXCLUDE clause

-- Workarounds available using existing functions
```

### Advanced Index Types

```sql
-- Partial implementation status:
✓ B-tree indexes (fully supported)
✓ Hash indexes (fully supported)
✓ Bitmap indexes (basic support)
⚠ GIN indexes (limited to specific types)
⚠ GiST indexes (spatial only)
✗ BRIN indexes (not implemented)
✗ Bloom filters (planned for v2.0)
✗ LSM trees (under research)

-- Expression indexes limitations
-- Cannot use volatile functions
CREATE INDEX idx_random ON table1 (random());  -- Not supported

-- Covering indexes (INCLUDE clause) - planned for v1.6
CREATE INDEX idx_orders ON orders (customer_id) INCLUDE (total, status);
```

### Partitioning

```sql
-- Table partitioning (planned for v2.0)
-- Currently must implement manually with inheritance

-- Desired future syntax:
CREATE TABLE sales (
    id BIGINT,
    sale_date DATE,
    amount DECIMAL
) PARTITION BY RANGE (sale_date);

CREATE TABLE sales_2024_q1 PARTITION OF sales
    FOR VALUES FROM ('2024-01-01') TO ('2024-04-01');

-- Current workaround: Manual partitioning
CREATE TABLE sales_2024_q1 (
    CHECK (sale_date >= '2024-01-01' AND sale_date < '2024-04-01')
) INHERITS (sales);

CREATE OR REPLACE FUNCTION sales_insert_trigger()
RETURNS TRIGGER AS $$
BEGIN
    IF NEW.sale_date >= '2024-01-01' AND NEW.sale_date < '2024-04-01' THEN
        INSERT INTO sales_2024_q1 VALUES (NEW.*);
    -- Add more conditions for other partitions
    END IF;
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;
```

### Replication and High Availability

```sql
-- Streaming replication (planned for v2.0)
-- Logical replication (planned for v2.5)
-- Synchronous replication (planned for v2.0)
-- Cascading replication (planned for v2.1)

-- Current limitations:
- No built-in replication
- Must use external tools for HA
- No automatic failover
- No read replicas

-- Workarounds:
- Use backup/restore for disaster recovery
- Implement application-level replication
- Use external clustering solutions
```

### JSON and Document Features

```sql
-- Limited JSON support
-- Missing operators and functions:
- jsonb_path_query() functions
- JSON Schema validation
- JSON Table functions
- SQL/JSON path language

-- Current support:
✓ JSON and JSONB data types
✓ Basic operators (->>, ->, @>, ?)
✓ json_extract() function
✗ Advanced path expressions
✗ JSON aggregates

-- Workaround for path queries:
-- Instead of: jsonb_path_query(data, '$.items[*].price')
-- Use: json_extract(data, '$.items') and process in application
```

## Performance Limitations

### Query Optimization

```sql
-- Missing optimizer features:
- Join order optimization limited to 8 tables
- No parallel query execution
- Limited statistics on expressions
- No adaptive query execution
- No result caching

-- Workarounds:
- Manually specify join order with CTEs
- Use materialized views for caching
- Maintain summary tables
- Optimize queries manually with EXPLAIN
```

### Concurrency Control

```sql
-- Current limitations:
- Table-level locking only (no row-level)
- No advisory locks
- Limited deadlock detection
- No lock timeout configuration

-- Planned improvements (v1.8):
- Row-level locking
- Lock queuing improvements
- Deadlock prevention algorithms
- Lock monitoring views
```

### Memory Management

```sql
-- Limitations:
- No per-query memory limits
- Basic work_mem allocation
- No memory accounting views
- Limited spill-to-disk for sorts

-- Workarounds:
- Set conservative work_mem
- Monitor system memory externally
- Use temporary tables for large operations
```

## Security Features

### Missing Security Features

```sql
-- Not yet implemented:
✗ Transparent Data Encryption (TDE)
✗ Column-level encryption
✗ Dynamic data masking
✗ Audit logging (basic only)
✗ SQL injection prevention
✗ Password complexity rules
✗ Account lockout policies
✗ Kerberos authentication
✗ LDAP/AD integration

-- Current security features:
✓ Role-based access control
✓ Row-level security
✓ SSL/TLS connections
✓ Password authentication
✓ Basic grants system
```

### Compliance Features

```sql
-- Missing for compliance:
- Audit trail with tamper protection
- Data retention policies
- Right to be forgotten (GDPR)
- Data lineage tracking
- Encryption key management

-- Workarounds:
- Implement audit tables manually
- Use triggers for tracking changes
- External key management systems
```

## Operational Features

### Backup and Recovery

```sql
-- Current limitations:
- No point-in-time recovery
- No incremental backups
- No parallel backup/restore
- No backup compression
- No online backups

-- Planned features (v1.7):
- WAL archiving
- Continuous archiving
- Point-in-time recovery
- Incremental backups
- Backup catalogs
```

### Monitoring and Diagnostics

```sql
-- Missing monitoring features:
- Query performance insights
- Wait event analysis
- Historical performance data
- Automatic workload repository
- Performance recommendations

-- Current capabilities:
✓ Basic statistics views
✓ EXPLAIN ANALYZE
✓ Simple logging
✗ Metrics export
✗ Alerting system
```

## Data Type Limitations

### Specialized Types

```sql
-- Not implemented:
✗ PostGIS geometry types
✗ Full text search types (tsvector, tsquery)
✗ Range types (limited support)
✗ Composite types (basic only)
✗ Enumerated types
✗ Network types (limited)
✗ BitString types

-- Workarounds:
- Store geometry as WKT in TEXT
- Implement full-text search externally
- Use check constraints for enums
```

### Type System Features

```sql
-- Missing features:
- User-defined types (limited)
- Type inheritance
- Polymorphic types
- Type modifiers
- Custom operators

-- Planned additions:
- Enhanced UDT support (v1.9)
- Operator overloading (v2.0)
- Type extensions (v2.1)
```

## Planned Feature Timeline

### Version 1.5 (Q2 2024)
- Additional window functions
- Improved JSON support
- Performance monitoring views
- Backup compression

### Version 1.6 (Q3 2024)
- Covering indexes (INCLUDE)
- Parallel vacuum
- Statement-level triggers
- Extended statistics

### Version 1.7 (Q4 2024)
- Point-in-time recovery
- Incremental backups
- Query result caching
- Connection pooling

### Version 1.8 (Q1 2025)
- Row-level locking
- Advisory locks
- Improved deadlock handling
- Lock monitoring

### Version 2.0 (Q2 2025)
- Table partitioning
- Streaming replication
- Parallel query execution
- GROUPING SETS

### Version 2.5 (2025)
- Logical replication
- Columnar storage
- Time-series optimizations
- Machine learning functions

### Version 3.0 (2026)
- Distributed SQL
- Multi-master replication
- Automatic sharding
- Cloud-native features

## Migration Considerations

### From PostgreSQL

```sql
-- Compatible features:
✓ Basic SQL syntax
✓ Most data types
✓ Simple functions
✓ Views and indexes

-- Incompatible features requiring changes:
✗ Extensions (PostGIS, etc.)
✗ Specialized index types
✗ Advanced partitioning
✗ Foreign data wrappers
✗ Logical replication
```

### From MySQL

```sql
-- Migration challenges:
- Different SQL dialect
- Storage engine concepts
- Replication architecture
- User management model

-- Tools needed:
- Schema converter
- Data migration utility
- Query translator
- Application changes
```

## Workaround Patterns

### Implementing Missing Features

```sql
-- Audit logging workaround
CREATE TABLE audit_log (
    id BIGSERIAL PRIMARY KEY,
    table_name TEXT,
    operation TEXT,
    user_name TEXT,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    old_data JSONB,
    new_data JSONB
);

CREATE OR REPLACE FUNCTION audit_trigger()
RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO audit_log (table_name, operation, user_name, old_data, new_data)
    VALUES (
        TG_TABLE_NAME,
        TG_OP,
        current_user,
        to_jsonb(OLD),
        to_jsonb(NEW)
    );
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- Materialized view refresh scheduling
CREATE TABLE mv_refresh_schedule (
    mv_name TEXT PRIMARY KEY,
    refresh_interval INTERVAL,
    last_refresh TIMESTAMP,
    next_refresh TIMESTAMP
);

CREATE OR REPLACE PROCEDURE refresh_scheduled_mvs()
AS $$
DECLARE
    mv RECORD;
BEGIN
    FOR mv IN 
        SELECT mv_name 
        FROM mv_refresh_schedule 
        WHERE next_refresh <= CURRENT_TIMESTAMP
    LOOP
        EXECUTE format('REFRESH MATERIALIZED VIEW %I', mv.mv_name);
        UPDATE mv_refresh_schedule 
        SET last_refresh = CURRENT_TIMESTAMP,
            next_refresh = CURRENT_TIMESTAMP + refresh_interval
        WHERE mv_name = mv.mv_name;
    END LOOP;
END;
$$ LANGUAGE plpgsql;
```

## Contributing

### How to Contribute

```bash
# Areas needing contribution:
- Query optimizer improvements
- Additional SQL functions
- Performance enhancements
- Documentation
- Testing

# Development setup:
git clone https://github.com/DaltonCalford/ScratchBird.git
cd ScratchBird
./scripts/dev-setup.sh

# Submit improvements:
- Fork repository
- Create feature branch
- Add tests
- Submit pull request
```

### Priority Features for Contributors

1. **High Priority**
   - Row-level locking
   - Parallel query execution
   - Table partitioning
   - Streaming replication

2. **Medium Priority**
   - Additional window functions
   - JSON path support
   - Backup improvements
   - Monitoring views

3. **Nice to Have**
   - Additional index types
   - Query result caching
   - Extended statistics
   - Performance advisor

## Support and Resources

### Getting Help

```sql
-- Check version and capabilities
SELECT version();
SELECT * FROM supported_features;

-- Report issues
-- https://github.com/DaltonCalford/ScratchBird/issues

-- Community forum
-- https://forum.scratchbird.io

-- Commercial support
-- support@scratchbird.io
```

### Staying Updated

- Release notes: Check each release for new features
- Roadmap updates: Quarterly roadmap reviews
- Beta testing: Join beta program for early access
- Community calls: Monthly development discussions

## Implementation Status

**Parser Capabilities** (`src/engine/parser*.cpp`):
- Full SQL-92 support (mostly complete)
- SQL:1999 features (partial)
- SQL:2003 features (limited)
- SQL:2011 features (planned)

**Executor Limitations** (`src/engine/executor*.cpp`):
- Single-threaded execution
- Basic join algorithms
- Simple aggregation
- Limited optimization

**Storage Limitations** (`src/engine/storage*.cpp`):
- Page-based storage only
- No compression
- Basic indexing
- Simple caching

**Code Anchors**:
- Feature flags: `include/scratchbird/engine/features.h`
- Parser limitations: `src/engine/parser.cpp`
- Executor constraints: `src/engine/executor.cpp`
- Storage restrictions: `src/engine/storage_manager.cpp`

## See also

- [SQL Overview](./sql-overview.md) - Current SQL support
- [Configuration](./configuration.md) - Available settings
- [Installation](./installation.md) - Setup instructions
- [Performance](./explain-analyze.md) - Optimization techniques
- [Index](./index.md) - Documentation home