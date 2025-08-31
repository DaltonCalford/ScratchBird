# ScratchBird Index Selection Developer Guide

## Quick Reference Decision Tree

```
📊 WORKLOAD TYPE?
├─ 🔍 Point Lookups (exact matches)
│  ├─ High frequency? → Hash Index
│  └─ General purpose? → B-Tree Index
├─ 📈 Range Queries (between, >, <)
│  ├─ Spatial data? → R-Tree Index
│  ├─ Write-heavy? → LSM-Tree Index
│  └─ General purpose? → B-Tree Index
├─ 📝 Write-Heavy Workloads
│  ├─ Time-series data? → LSM-Tree Index
│  └─ Simple inserts? → Hash Index
├─ 📊 Analytics/Aggregations
│  └─ Large datasets? → Columnstore Index
├─ 🔎 Full-Text Search
│  └─ Text/Arrays? → GIN Index
└─ 📋 Low-Cardinality Data
   └─ Boolean/Categories? → Bitmap Index
```

## Detailed Index Selection Guide

### 1. Hash Index - Lightning Fast Point Lookups

**✅ Choose Hash Index When:**
- Primary key lookups (`SELECT * WHERE id = ?`)
- Foreign key constraints
- Session management (`user_id → session_data`)
- Cache layer indexing
- High-frequency exact match queries
- Single-column keys only

**❌ Avoid Hash Index When:**
- Need range queries (`WHERE date BETWEEN ? AND ?`)
- Multi-column composite keys
- Need ordered results
- Low-frequency queries (B-Tree is fine)

**📈 Performance Profile:**
```
Search:     O(1) - Single page access
Insert:     O(1) - Direct hash placement
Space:      Low overhead
Concurrency: Excellent (bucket-level locking)
```

**💡 Example Usage:**
```sql
-- Perfect for user lookups
CREATE INDEX idx_user_hash ON users USING HASH (user_id);

-- Great for session management
CREATE INDEX idx_session_hash ON sessions USING HASH (session_token)
INCLUDE (user_id, expires_at);
```

### 2. B-Tree Index - The Reliable Workhorse

**✅ Choose B-Tree Index When:**
- General-purpose indexing needs
- Range queries required
- Need ordered results
- Multi-column composite keys
- Mixed read/write workloads
- Don't know the specific access pattern yet

**❌ Consider Alternatives When:**
- Pure point lookups (Hash is faster)
- Write-heavy workloads (LSM-Tree better)
- Analytics workloads (Columnstore better)
- Full-text search needs (GIN better)

**📈 Performance Profile:**
```
Search:     O(log n) - Balanced tree traversal
Range:      O(log n + k) - k = result size
Insert:     O(log n) - Tree rebalancing
Space:      Moderate overhead
```

**💡 Example Usage:**
```sql
-- Classic range queries
CREATE INDEX idx_order_date ON orders (order_date);

-- Composite keys
CREATE INDEX idx_user_category ON products (user_id, category_id)
INCLUDE (name, price);
```

### 3. LSM-Tree Index - Write-Optimized Powerhouse

**✅ Choose LSM-Tree Index When:**
- High write throughput required (>1000 writes/sec)
- Time-series data (sensor readings, logs)
- Event streaming applications
- Audit trails and logging
- IoT data ingestion
- Write-to-read ratio > 10:1

**❌ Avoid LSM-Tree When:**
- Read-heavy workloads (B-Tree better)
- Need immediate consistency
- Storage space is constrained
- Simple point lookups (Hash better)

**📈 Performance Profile:**
```
Write:      O(1) - Append to MemTable
Read:       O(log n) - Multiple level checks
Range:      O(log n + k) - Sequential scans
Compaction: Background process
```

**🔧 Compaction Strategies:**
```sql
-- Size-Tiered: Better write performance
CREATE INDEX idx_sensor_data ON sensor_readings USING LSMTREE (timestamp)
WITH (compaction_strategy = 'SIZE_TIERED');

-- Leveled: Better read performance
CREATE INDEX idx_audit_log ON audit_log USING LSMTREE (event_time)
WITH (compaction_strategy = 'LEVELED');
```

**💡 Example Usage:**
```sql
-- Time-series data
CREATE INDEX idx_metrics_time ON metrics USING LSMTREE (timestamp)
INCLUDE (metric_name, value);

-- High-frequency logging
CREATE INDEX idx_access_log ON access_log USING LSMTREE (request_time)
WITH (compaction_strategy = 'SIZE_TIERED');
```

### 4. Columnstore Index - Analytics Powerhouse

**✅ Choose Columnstore Index When:**
- Analytical queries (GROUP BY, SUM, AVG)
- Data warehouse workloads
- Large dataset aggregations
- Reporting and business intelligence
- Read-heavy analytical workloads
- Need high compression ratios

**❌ Avoid Columnstore When:**
- OLTP workloads (frequent updates)
- Point lookups (Hash/B-Tree better)
- Small datasets (<1M rows)
- Need unique constraints

**📈 Performance Profile:**
```
Analytics:  Excellent - Vectorized operations
Point Lookup: Moderate - Column scanning
Compression: 60-90% size reduction
Updates:    Expensive - Column reorganization
```

**🗜️ Compression Options:**
```sql
-- LZ4: Balanced compression/speed
CREATE INDEX idx_sales_analytics ON sales USING COLUMNSTORE (date, product_id, amount)
WITH (compression = 'LZ4');

-- ZSTD: Maximum compression
CREATE INDEX idx_archive_data ON archive USING COLUMNSTORE (year, month, data)
WITH (compression = 'ZSTD');
```

**💡 Example Usage:**
```sql
-- Sales analytics
CREATE INDEX idx_sales_analysis ON sales USING COLUMNSTORE
(sale_date, product_category, customer_segment, amount);

-- Data warehouse dimensions
CREATE INDEX idx_fact_table ON fact_sales USING COLUMNSTORE
(date_key, product_key, customer_key, amount, quantity, discount);
```

### 5. GIN Index - Full-Text Search Expert

**✅ Choose GIN Index When:**
- Full-text search requirements
- Searching within arrays or JSON
- Multi-token queries
- Document management systems
- Content search engines
- Tag-based systems

**❌ Avoid GIN When:**
- Exact string matching (Hash better)
- Simple equality checks
- Numeric range queries
- Storage space is constrained

**📈 Performance Profile:**
```
Text Search: Excellent - Inverted index lookup
Index Size:  Large - Stores all tokens
Insert:      Expensive - Tokenization overhead
Maintenance: High - Posting list updates
```

**💡 Example Usage:**
```sql
-- Document search
CREATE INDEX idx_document_search ON documents USING GIN (content);

-- Product catalog
CREATE INDEX idx_product_tags ON products USING GIN (tags);

-- JSON array search
CREATE INDEX idx_json_search ON events USING GIN (metadata);
```

### 6. R-Tree Index - Spatial Data Specialist

**✅ Choose R-Tree Index When:**
- Geographic Information Systems (GIS)
- Location-based services
- Spatial range queries (within, contains, intersects)
- Mapping applications
- Geometric data indexing
- Computer graphics applications

**❌ Avoid R-Tree When:**
- Non-spatial data
- High-dimensional data (>3D)
- Simple point coordinates (B-Tree sufficient)
- Text-based location queries

**📈 Performance Profile:**
```
Spatial Search: Excellent - MBR optimization
Point Search:   Good - Geometric indexing
Index Size:     Moderate - Bounding rectangles
Updates:        Moderate - Tree rebalancing
```

**💡 Example Usage:**
```sql
-- Geographic locations
CREATE INDEX idx_store_location ON stores USING RTREE (location);

-- Geometric shapes
CREATE INDEX idx_polygon_data ON regions USING RTREE (boundary);

-- Spatial analytics
CREATE INDEX idx_delivery_zones ON zones USING RTREE (coverage_area);
```

### 7. Bitmap Index - Low-Cardinality Champion

**✅ Choose Bitmap Index When:**
- Low-cardinality data (< 1000 distinct values)
- Boolean flags and status fields
- Data warehouse dimensions
- Analytical queries with multiple conditions
- Space efficiency is critical

**❌ Avoid Bitmap When:**
- High-cardinality data (> 1000 distinct values)
- Frequent updates (bitmap maintenance overhead)
- Unique constraints needed
- Single-condition queries only

**📈 Performance Profile:**
```
Low-Cardinality: Excellent - Compressed bitmaps
Space Usage:     Very Low - RLE compression
Multi-Condition: Excellent - Bitmap operations
Updates:         Moderate - Bitmap maintenance
```

**💡 Example Usage:**
```sql
-- Status fields
CREATE INDEX idx_order_status ON orders USING BITMAP (status);

-- Boolean flags
CREATE INDEX idx_user_flags ON users USING BITMAP (is_active);

-- Category data
CREATE INDEX idx_product_category ON products USING BITMAP (category);
```

## Index Combination Strategies

### Multi-Index Approach

For complex queries, multiple indexes can work together:

```sql
-- Point lookups + Analytics
CREATE INDEX idx_user_hash ON users USING HASH (user_id);
CREATE INDEX idx_user_analytics ON user_actions USING COLUMNSTORE
(user_id, action_date, action_type);

-- Range queries + Full-text
CREATE INDEX idx_doc_date ON documents USING BTREE (created_date);
CREATE INDEX idx_doc_content ON documents USING GIN (content);
```

### Covering Indexes with INCLUDE

Reduce I/O by including frequently accessed columns:

```sql
-- Hash index with payload
CREATE INDEX idx_user_profile ON users USING HASH (user_id)
INCLUDE (name, email, last_login);

-- B-Tree with analytical columns
CREATE INDEX idx_order_summary ON orders USING BTREE (order_date)
INCLUDE (customer_id, total_amount, status);
```

## Performance Testing Guide

### Benchmark Your Workload

Always test with your actual data patterns:

```sql
-- Create test indexes
CREATE INDEX test_btree ON large_table USING BTREE (lookup_column);
CREATE INDEX test_hash ON large_table USING HASH (lookup_column);
CREATE INDEX test_lsm ON large_table USING LSMTREE (lookup_column);

-- Compare query performance
EXPLAIN ANALYZE SELECT * FROM large_table WHERE lookup_column = 'test_value';
```

### Monitor Index Statistics

```sql
-- Check index usage and performance
SELECT collect_statistics FROM index_statistics
WHERE index_name = 'your_index_name';
```

### Index Maintenance

```sql
-- LSM-Tree compaction monitoring
SELECT write_amplification, read_amplification, space_amplification
FROM lsm_statistics WHERE index_name = 'lsm_index_name';

-- Columnstore compression effectiveness
SELECT compression_ratio, vectorized_operations
FROM columnstore_statistics WHERE index_name = 'cs_index_name';
```

## Common Anti-Patterns to Avoid

### ❌ Wrong Index Type
```sql
-- DON'T: Hash index for range queries
CREATE INDEX bad_idx ON events USING HASH (event_date);
SELECT * FROM events WHERE event_date BETWEEN '2024-01-01' AND '2024-12-31';

-- DO: B-Tree or LSM-Tree for ranges
CREATE INDEX good_idx ON events USING BTREE (event_date);
```

### ❌ Over-Indexing
```sql
-- DON'T: Create indexes for every column
CREATE INDEX idx1 ON table (col1);
CREATE INDEX idx2 ON table (col2);
CREATE INDEX idx3 ON table (col3);

-- DO: Create composite indexes for query patterns
CREATE INDEX idx_composite ON table (col1, col2, col3);
```

### ❌ Ignoring Workload Patterns
```sql
-- DON'T: Use Columnstore for OLTP
CREATE INDEX bad_oltp ON orders USING COLUMNSTORE (order_id, customer_id);

-- DO: Use Hash/B-Tree for OLTP
CREATE INDEX good_oltp ON orders USING HASH (order_id);
```

## Summary Decision Matrix

| Use Case | Index Type | Rationale |
|----------|------------|-----------|
| **Primary Key Lookups** | Hash | O(1) performance |
| **Foreign Key Constraints** | Hash | Fast joins |
| **Date Range Queries** | B-Tree | Ordered traversal |
| **Write-Heavy Logs** | LSM-Tree | Write optimization |
| **Time-Series Analytics** | Columnstore | Compression + aggregation |
| **Document Search** | GIN | Full-text capabilities |
| **Geographic Queries** | R-Tree | Spatial optimization |
| **Status/Category Fields** | Bitmap | Space efficiency |
| **General Purpose** | B-Tree | Balanced performance |

## Getting Started

1. **Analyze Your Queries**: Identify the most frequent query patterns
2. **Measure Cardinality**: Count distinct values in indexed columns
3. **Assess Workload**: Determine read/write ratio and access patterns
4. **Start Simple**: Begin with B-Tree, then specialize based on performance needs
5. **Monitor Performance**: Use index statistics to validate choices
6. **Iterate**: Adjust index strategy based on production metrics

Remember: The best index is the one that fits your specific workload pattern. Start with general-purpose B-Tree indexes, then optimize with specialized index types based on measured performance characteristics.
