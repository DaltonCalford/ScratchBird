# ScratchBird Performance Benchmarks Specification

**Version:** 2.0
**Last Updated:** 2026-01-07
**Status:** ✅ Complete

**WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in Alpha; any WAL support is optional post-gold (replication/PITR).
Any WAL references in this document describe an optional post-gold stream for
replication/PITR only.
**Table Footnote:** In comparison tables below, ScratchBird WAL references are optional post-gold (replication/PITR).

---

## Table of Contents

1. [Overview](#1-overview)
2. [Hardware Configurations](#2-hardware-configurations)
3. [Benchmark Suites](#3-benchmark-suites)
4. [Workload Definitions](#4-workload-definitions)
5. [Performance Targets](#5-performance-targets)
6. [Measurement Methodology](#6-measurement-methodology)
7. [Tuning Guide](#7-tuning-guide)
8. [Profiling and Analysis](#8-profiling-and-analysis)
9. [Comparative Benchmarks](#9-comparative-benchmarks)
10. [Regression Testing](#10-regression-testing)
11. [Cluster Benchmarks](#11-cluster-benchmarks)
12. [Best Practices](#12-best-practices)
13. [Reporting and Visualization](#13-reporting-and-visualization)

---

## 1. Overview

### 1.1. Purpose

This document specifies performance benchmarking for ScratchBird Database Engine, including:

- Standard benchmark suites (TPC-like, YCSB, custom)
- Performance targets and SLAs
- Measurement methodology and tools
- Tuning guide for optimization
- Regression testing framework

### 1.2. Goals

1. **Establish Baselines** - Define expected performance on reference hardware
2. **Prevent Regressions** - Automated testing to catch performance degradations
3. **Guide Optimization** - Identify bottlenecks and optimization opportunities
4. **Enable Comparisons** - Fair comparisons with PostgreSQL, Firebird, MySQL
5. **Inform Users** - Help users set realistic performance expectations

### 1.3. Key Metrics

| Metric | Unit | Description |
|--------|------|-------------|
| **Throughput** | TPS (transactions/sec) | Sustained transaction rate |
| **Latency** | ms | Response time (avg, p50, p95, p99) |
| **Scalability** | TPS/core | How throughput scales with cores |
| **Resource Usage** | % or MB | CPU, memory, I/O utilization |
| **Cache Hit Rate** | % | Buffer pool cache hits |
| **Index Performance** | ops/sec | Index lookup/insert speed |

### 1.4. MGA-Specific Considerations

**ScratchBird uses Firebird MGA, not PostgreSQL write-after log (WAL):**

- **No write-after log write overhead** - Faster commit in core MGA (no write-after log (WAL) flush)
- **Multi-Version In-Page** - Page updates may be larger than PostgreSQL
- **Sweep Overhead** - Garbage collection runs periodically
- **Transaction Marker Reads** - Must check OIT, OAT, OST, NEXT

**Performance Implications:**

| Aspect | ScratchBird (MGA) | PostgreSQL (write-after log (WAL)) |
|--------|-------------------|------------------|
| **Write Latency** | Lower (no write-after log (WAL) sync in core MGA) | Higher (write-after log (WAL) sync) |
| **Read Latency** | Comparable | Comparable |
| **Throughput** | Higher (no write-after log (WAL) in core MGA) | Lower (write-after log (WAL) overhead) |
| **GC Overhead** | Sweep/GC (periodic) | VACUUM (periodic) |
| **Hot Standby** | Not yet (Beta) | Yes |

Note: Optional write-after log (WAL) for replication/PITR may reintroduce write-after log (WAL)-style overhead.

---

## 2. Hardware Configurations

### 2.1. Baseline (Alpha Testing)

**Purpose:** Minimum viable hardware for development and testing.

```
CPU:     4 vCPU (x86_64, 2.5+ GHz)
RAM:     8 GB
Storage: 128 GB SSD (SATA or NVMe)
Network: 1 Gbps (cluster tests)
OS:      Ubuntu 22.04 LTS, kernel ≥ 5.15
FS:      ext4 or xfs
Build:   Release, no sanitizers
```

**Expected Performance:**
- Single-row INSERT: 1,000+ TPS
- Point SELECT: < 1 ms p95
- Sequential scan: 100+ MB/sec

### 2.2. Recommended (Production)

**Purpose:** Standard production deployment.

```
CPU:     16 cores (x86_64, 3.0+ GHz)
RAM:     64 GB
Storage: 1 TB NVMe SSD (RAID 10 optional)
Network: 10 Gbps (cluster deployments)
OS:      Ubuntu 22.04/24.04 LTS or RHEL 8/9
FS:      xfs (recommended for large files)
Build:   Release, optimized
```

**Expected Performance:**
- Single-row INSERT: 10,000+ TPS
- Point SELECT: < 0.5 ms p95
- Sequential scan: 500+ MB/sec
- Concurrent users: 100+

### 2.3. High-Performance (Enterprise)

**Purpose:** Large-scale production deployments.

```
CPU:     64 cores (x86_64, 3.5+ GHz, dual-socket)
RAM:     512 GB (NUMA-aware allocation)
Storage: 4 TB NVMe SSD (RAID 10, hardware RAID)
Network: 25/100 Gbps (cluster deployments)
OS:      RHEL 8/9 with real-time kernel
FS:      xfs with optimized mount options
Build:   Release, PGO (Profile-Guided Optimization)
```

**Expected Performance:**
- Single-row INSERT: 50,000+ TPS
- Point SELECT: < 0.2 ms p95
- Sequential scan: 2+ GB/sec
- Concurrent users: 1,000+

---

## 3. Benchmark Suites

### 3.1. Core Suite (Required for Alpha)

**Purpose:** Essential benchmarks for CI/CD regression testing.

| Benchmark | Workload | Duration | Acceptance |
|-----------|----------|----------|------------|
| **insert_single** | Single-row INSERT | 60 sec | 1,000+ TPS |
| **select_point** | Primary key lookup | 60 sec | < 1 ms p95 |
| **select_scan** | Sequential scan | 30 sec | 100+ MB/sec |
| **update_single** | Single-row UPDATE | 60 sec | 1,000+ TPS |
| **mixed_oltp** | 50% read, 50% write | 60 sec | 500+ TPS |

### 3.2. TPC-C (OLTP Benchmark)

**Purpose:** Standard OLTP benchmark simulating order-entry workload.

**Transactions:**
1. **NewOrder** - Create new order (45%)
2. **Payment** - Process payment (43%)
3. **OrderStatus** - Check order status (4%)
4. **Delivery** - Deliver orders (4%)
5. **StockLevel** - Check stock levels (4%)

**Metrics:**
- **tpmC** - Transactions per minute (NewOrder)
- **Latency** - Per-transaction latency (p95, p99)

**Target (Baseline):** 1,000+ tpmC

### 3.3. YCSB (Yahoo! Cloud Serving Benchmark)

**Purpose:** Key-value workload testing.

**Workloads:**
- **Workload A** - 50% read, 50% update
- **Workload B** - 95% read, 5% update
- **Workload C** - 100% read
- **Workload D** - 95% read, 5% insert
- **Workload E** - 95% scan, 5% insert
- **Workload F** - 50% read, 50% read-modify-write

**Target (Baseline, Workload B):** 10,000+ ops/sec

### 3.4. Index Benchmark Suite

**Purpose:** Test performance of all 11 index types.

| Index Type | Workload | Metric |
|------------|----------|--------|
| **B-Tree** | Point lookup, range scan | ops/sec |
| **Hash** | Equality lookup | ops/sec |
| **GIN** | Array/JSON containment | ops/sec |
| **GiST** | Spatial queries | ops/sec |
| **SP-GiST** | Tree structures | ops/sec |
| **BRIN** | Large table scan | MB/sec |
| **R-Tree** | Spatial range queries | ops/sec |
| **HNSW** | Vector similarity search | queries/sec |
| **Bitmap** | Low-cardinality columns | scans/sec |
| **Columnstore** | Analytical queries | GB/sec |
| **LSM Tree** | Write-heavy workload | inserts/sec |

### 3.5. Security Overhead Benchmark

**Purpose:** Measure performance impact of security features.

| Feature | Overhead Target |
|---------|-----------------|
| **RLS (Row-Level Security)** | < 10% |
| **CLS (Column-Level Security)** | < 15% |
| **Encryption (at-rest)** | < 5% |
| **Encryption (in-transit)** | < 10% |
| **Domain Validation** | < 5% |
| **Audit Logging** | < 10% |

### 3.6. Domain Performance Benchmark

**Purpose:** Test domain validation, masking, encryption performance.

**Scenarios:**
1. **Simple CHECK constraint** - Baseline
2. **Complex VALIDATION** - Multi-predicate rules
3. **MASKING** - Dynamic data masking
4. **ENCRYPTION** - Domain-level encryption
5. **QUALITY** - Statistical quality checks

---

## 4. Workload Definitions

### 4.1. OLTP Workloads

#### 4.1.1. Write-Heavy OLTP

**Characteristics:**
- 80% writes (INSERT/UPDATE/DELETE)
- 20% reads (SELECT)
- Single-row operations
- Autocommit or small transactions (1-5 operations)

**Schema Example:**

```sql
CREATE TABLE orders (
    order_id BIGSERIAL PRIMARY KEY,
    customer_id INT NOT NULL,
    order_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    total_amount DECIMAL(10,2),
    status VARCHAR(20)
);

CREATE INDEX idx_orders_customer ON orders(customer_id);
CREATE INDEX idx_orders_date ON orders(order_date);
```

**Workload:**

```python
def write_heavy_oltp(conn, duration_sec):
    start = time.time()
    ops = 0

    while time.time() - start < duration_sec:
        # 80% writes
        if random.random() < 0.8:
            if random.random() < 0.7:
                # 70% of writes are INSERTs
                conn.execute("""
                    INSERT INTO orders (customer_id, total_amount, status)
                    VALUES ($1, $2, $3)
                """, random.randint(1, 1000000), random.uniform(10, 1000), 'pending')
            else:
                # 30% of writes are UPDATEs
                conn.execute("""
                    UPDATE orders SET status = $1
                    WHERE order_id = $2
                """, 'completed', random.randint(1, ops))
        else:
            # 20% reads (point SELECT)
            conn.execute("""
                SELECT * FROM orders WHERE order_id = $1
            """, random.randint(1, ops))

        ops += 1

    return ops
```

**Target (Baseline):** 1,000+ TPS

#### 4.1.2. Read-Heavy OLTP

**Characteristics:**
- 20% writes
- 80% reads
- Mix of point lookups and small range scans

**Target (Baseline):** 5,000+ TPS

#### 4.1.3. Balanced OLTP

**Characteristics:**
- 50% writes
- 50% reads
- Realistic transaction patterns

**Target (Baseline):** 2,000+ TPS

### 4.2. OLAP Workloads

#### 4.2.1. Sequential Scan

**Workload:**

```sql
SELECT * FROM large_table WHERE date_col > '2025-01-01';
```

**Target (Baseline):** 100+ MB/sec

#### 4.2.2. Aggregate Queries

**Workload:**

```sql
SELECT
    customer_id,
    COUNT(*) AS order_count,
    SUM(total_amount) AS total_spent,
    AVG(total_amount) AS avg_order
FROM orders
GROUP BY customer_id
HAVING COUNT(*) > 10;
```

**Target (Baseline):** 10+ queries/sec (1M rows)

#### 4.2.3. Join Queries

**Workload:**

```sql
SELECT
    o.order_id,
    c.customer_name,
    p.product_name,
    oi.quantity
FROM orders o
JOIN customers c ON o.customer_id = c.customer_id
JOIN order_items oi ON o.order_id = oi.order_id
JOIN products p ON oi.product_id = p.product_id
WHERE o.order_date > '2025-01-01';
```

**Target (Baseline):** 5+ queries/sec (1M rows)

### 4.3. Mixed Workloads

#### 4.3.1. HTAP (Hybrid Transactional/Analytical)

**Characteristics:**
- 60% OLTP (short transactions)
- 40% OLAP (analytical queries)
- Concurrent execution

**Target (Baseline):** 500+ TPS (OLTP), 2+ queries/sec (OLAP)

---

## 5. Performance Targets

### 5.1. Latency Targets (Baseline Hardware)

| Operation | Dataset | p50 | p95 | p99 |
|-----------|---------|-----|-----|-----|
| **Single-row INSERT** | 10K | < 0.5 ms | < 1 ms | < 2 ms |
| **Single-row UPDATE** | 10K | < 0.5 ms | < 1 ms | < 2 ms |
| **Single-row DELETE** | 10K | < 0.5 ms | < 1 ms | < 2 ms |
| **Point SELECT** | 10K | < 0.3 ms | < 0.5 ms | < 1 ms |
| **Range SELECT (10 rows)** | 10K | < 1 ms | < 2 ms | < 5 ms |
| **Aggregate (COUNT)** | 1M | < 100 ms | < 200 ms | < 500 ms |
| **Simple JOIN (2 tables)** | 1M | < 50 ms | < 100 ms | < 200 ms |

### 5.2. Throughput Targets (Baseline Hardware)

| Workload | Dataset | Target TPS |
|----------|---------|------------|
| **Write-heavy OLTP** | 10K | 1,000+ |
| **Read-heavy OLTP** | 10K | 5,000+ |
| **Balanced OLTP** | 10K | 2,000+ |
| **Sequential scan** | 1M | 100+ MB/sec |

### 5.3. Scalability Targets

| Metric | 4 cores | 16 cores | 64 cores |
|--------|---------|----------|----------|
| **Write-heavy OLTP** | 1,000 TPS | 3,500 TPS | 10,000 TPS |
| **Read-heavy OLTP** | 5,000 TPS | 18,000 TPS | 50,000 TPS |
| **Scaling Efficiency** | 100% | 87% | 62% |

**Scaling Efficiency:** `(TPS_n / TPS_4) / (n / 4) * 100%`

### 5.4. Resource Usage Targets

| Metric | Target | Notes |
|--------|--------|-------|
| **CPU Usage** | < 80% | Sustained load |
| **Memory Usage** | < 75% | Of configured buffer pool + overhead |
| **Cache Hit Rate** | > 95% | Buffer pool |
| **I/O Util** | < 70% | Disk utilization |

---

## 6. Measurement Methodology

### 6.1. Benchmark Protocol

**Standard Protocol:**

```
1. Setup Phase
   - Create database
   - Load schema
   - Load initial data
   - Create indexes
   - Run SWEEP/GC + ANALYZE (VACUUM is a PostgreSQL alias)
   - Restart database

2. Warmup Phase (10-30 seconds)
   - Execute workload
   - Discard metrics

3. Measurement Phase (60-300 seconds)
   - Execute workload
   - Record metrics every second

4. Cooldown Phase
   - Flush pending writes
   - Wait for background tasks

5. Teardown Phase
   - Stop database
   - Collect logs
   - Archive results
```

### 6.2. Metrics Collection

**Client-Side Metrics:**

```python
class BenchmarkMetrics:
    def __init__(self):
        self.latencies = []  # List of operation latencies
        self.errors = 0
        self.start_time = None
        self.end_time = None

    def record_op(self, latency_ms):
        self.latencies.append(latency_ms)

    def record_error(self):
        self.errors += 1

    def report(self):
        if not self.latencies:
            return None

        latencies = sorted(self.latencies)
        duration = self.end_time - self.start_time

        return {
            'ops': len(latencies),
            'duration_sec': duration,
            'throughput_tps': len(latencies) / duration,
            'latency_avg_ms': sum(latencies) / len(latencies),
            'latency_p50_ms': latencies[int(len(latencies) * 0.50)],
            'latency_p95_ms': latencies[int(len(latencies) * 0.95)],
            'latency_p99_ms': latencies[int(len(latencies) * 0.99)],
            'latency_max_ms': latencies[-1],
            'errors': self.errors,
            'error_rate': self.errors / len(latencies) if latencies else 0
        }
```

**Server-Side Metrics:**

```sql
-- Enable statistics collection
SELECT
    pg_stat_get_db_numbackends(oid) AS connections,
    pg_stat_get_db_xact_commit(oid) AS commits,
    pg_stat_get_db_xact_rollback(oid) AS rollbacks,
    pg_stat_get_db_blks_read(oid) AS disk_reads,
    pg_stat_get_db_blks_hit(oid) AS cache_hits,
    pg_stat_get_db_tup_returned(oid) AS tuples_read,
    pg_stat_get_db_tup_fetched(oid) AS tuples_fetched,
    pg_stat_get_db_tup_inserted(oid) AS tuples_inserted,
    pg_stat_get_db_tup_updated(oid) AS tuples_updated,
    pg_stat_get_db_tup_deleted(oid) AS tuples_deleted
FROM pg_database
WHERE datname = current_database();
```

### 6.3. System Monitoring

**CPU:**

```bash
# Collect CPU usage
mpstat 1 | tee cpu_stats.log
```

**Memory:**

```bash
# Collect memory usage
free -m | tee memory_stats.log
```

**I/O:**

```bash
# Collect I/O statistics
iostat -x 1 | tee io_stats.log
```

**Network (cluster benchmarks):**

```bash
# Collect network statistics
iftop -t -s 1 | tee network_stats.log
```

### 6.4. Result Validation

**Acceptance Criteria:**

```python
def validate_benchmark(results, targets):
    failures = []

    if results['throughput_tps'] < targets['min_tps']:
        failures.append(f"Throughput {results['throughput_tps']} < {targets['min_tps']} TPS")

    if results['latency_p95_ms'] > targets['max_p95_latency_ms']:
        failures.append(f"p95 latency {results['latency_p95_ms']} > {targets['max_p95_latency_ms']} ms")

    if results['error_rate'] > targets['max_error_rate']:
        failures.append(f"Error rate {results['error_rate']} > {targets['max_error_rate']}")

    return len(failures) == 0, failures
```

---

## 7. Tuning Guide

### 7.1. Database Configuration

**Baseline Configuration:**

```ini
# Memory
buffer_pool_size = 2GB              # 25% of RAM (8 GB)
shared_memory_size = 256MB          # Cluster mode

# Connections
max_connections = 100

# Write-after log (WAL, optional; not used for recovery in MGA)
wal_level = minimal                 # MGA doesn't need write-after log (WAL, optional post-gold) for recovery

# Checkpointing
checkpoint_interval_sec = 300       # 5 minutes

# Query
work_mem = 64MB                     # Sort/hash work memory
maintenance_work_mem = 512MB        # Sweep/GC, index builds

# Logging
log_min_duration_statement_ms = 1000  # Log slow queries > 1 sec
```

**Optimized Configuration (Recommended Hardware):**

```ini
# Memory
buffer_pool_size = 16GB             # 25% of RAM (64 GB)
shared_memory_size = 1GB

# Connections
max_connections = 500

# Query
work_mem = 256MB
maintenance_work_mem = 2GB

# Performance
effective_cache_size = 48GB         # OS page cache hint
random_page_cost = 1.1              # SSD (default 4.0 for HDD)
```

### 7.2. OS Tuning

**Linux Kernel Parameters:**

```bash
# /etc/sysctl.conf

# Virtual Memory
vm.swappiness = 10                  # Reduce swap usage
vm.dirty_ratio = 10                 # Dirty pages threshold
vm.dirty_background_ratio = 5       # Background flush threshold

# Huge Pages (2 MB)
vm.nr_hugepages = 8192              # 16 GB huge pages

# Network (for cluster)
net.core.rmem_max = 134217728       # 128 MB receive buffer
net.core.wmem_max = 134217728       # 128 MB send buffer
net.ipv4.tcp_rmem = 4096 87380 134217728
net.ipv4.tcp_wmem = 4096 65536 134217728
```

**File System Mount Options:**

```bash
# /etc/fstab
/dev/nvme0n1 /data xfs noatime,nodiratime,nobarrier 0 0
```

### 7.3. Index Optimization

**Index Selection Guide:**

| Query Pattern | Recommended Index |
|---------------|-------------------|
| Equality (=) | B-Tree or Hash |
| Range (<, >, BETWEEN) | B-Tree |
| Array/JSON containment | GIN |
| Spatial queries | GiST or R-Tree |
| Low-cardinality columns | Bitmap or BRIN |
| Vector similarity | HNSW |
| Write-heavy workload | LSM Tree |
| Analytical (columnar) | Columnstore |

**Index Maintenance:**

```sql
-- Rebuild fragmented indexes
REINDEX INDEX idx_orders_customer;

-- Update statistics
ANALYZE orders;

-- Check index usage
SELECT schemaname, tablename, indexname, idx_scan, idx_tup_read, idx_tup_fetch
FROM pg_stat_user_indexes
WHERE idx_scan = 0;  -- Unused indexes
```

### 7.4. Query Optimization

**EXPLAIN Analysis:**

```sql
EXPLAIN (ANALYZE, BUFFERS, VERBOSE)
SELECT * FROM orders WHERE customer_id = 12345;
```

**Common Optimizations:**

1. **Use Covering Indexes**
   ```sql
   CREATE INDEX idx_orders_covering ON orders(customer_id) INCLUDE (total_amount, status);
   ```

2. **Partition Large Tables**
   ```sql
   CREATE TABLE orders_2025_01 PARTITION OF orders
   FOR VALUES FROM ('2025-01-01') TO ('2025-02-01');
   ```

3. **Use Materialized Views**
   ```sql
   CREATE MATERIALIZED VIEW customer_summary AS
   SELECT customer_id, COUNT(*) AS order_count, SUM(total_amount) AS total
   FROM orders
   GROUP BY customer_id;
   ```

---

## 8. Profiling and Analysis

### 8.1. CPU Profiling

**perf (Linux):**

```bash
# Record CPU profile
perf record -g -p $(pgrep scratchbird-server)

# Generate flamegraph
perf script | stackcollapse-perf.pl | flamegraph.pl > flamegraph.svg
```

**Valgrind (Callgrind):**

```bash
valgrind --tool=callgrind --callgrind-out-file=callgrind.out ./scratchbird-server

# Visualize with KCachegrind
kcachegrind callgrind.out
```

### 8.2. Memory Profiling

**Valgrind (Massif):**

```bash
valgrind --tool=massif --massif-out-file=massif.out ./scratchbird-server

# Generate report
ms_print massif.out
```

**jemalloc Profiling:**

```bash
export MALLOC_CONF="prof:true,prof_prefix:jeprof.out"
./scratchbird-server

# Generate heap profile
jeprof --pdf ./scratchbird-server jeprof.out.*.heap > heap.pdf
```

### 8.3. I/O Profiling

**iotop:**

```bash
# Monitor I/O by process
iotop -p $(pgrep scratchbird-server)
```

**blktrace:**

```bash
# Trace block I/O
blktrace -d /dev/nvme0n1 -o - | blkparse -i -
```

### 8.4. Query Profiling

**pg_stat_statements:**

```sql
-- Top 10 slowest queries
SELECT
    queryid,
    calls,
    mean_exec_time,
    total_exec_time,
    query
FROM pg_stat_statements
ORDER BY mean_exec_time DESC
LIMIT 10;
```

---

## 9. Comparative Benchmarks

### 9.1. ScratchBird vs. PostgreSQL

**Benchmark:** Write-heavy OLTP (1,000 concurrent clients)

| Metric | ScratchBird (MGA) | PostgreSQL 16 |
|--------|-------------------|---------------|
| **Throughput** | 15,200 TPS | 12,800 TPS |
| **p95 Latency** | 0.8 ms | 1.2 ms |
| **CPU Usage** | 65% | 72% |
| **I/O Util** | 42% | 58% |
| **Reason** | No write-after log (WAL) sync overhead | Write-after log (WAL) fsync required |

**Benchmark:** Read-heavy OLTP (100 concurrent clients)

| Metric | ScratchBird | PostgreSQL 16 |
|--------|-------------|---------------|
| **Throughput** | 52,000 TPS | 48,000 TPS |
| **p95 Latency** | 0.3 ms | 0.4 ms |
| **CPU Usage** | 55% | 58% |
| **Cache Hit Rate** | 97% | 96% |
| **Reason** | Comparable (both MVCC) | Comparable |

### 9.2. ScratchBird vs. Firebird

**Benchmark:** Single-connection OLTP

| Metric | ScratchBird | Firebird 5.0 |
|--------|-------------|--------------|
| **Throughput** | 3,200 TPS | 2,800 TPS |
| **p95 Latency** | 0.6 ms | 0.8 ms |
| **Reason** | Optimized C++17/20 | Older codebase |

### 9.3. ScratchBird vs. MySQL

**Benchmark:** Write-heavy OLTP (InnoDB)

| Metric | ScratchBird (MGA) | MySQL 8.0 (InnoDB) |
|--------|-------------------|--------------------|
| **Throughput** | 15,200 TPS | 14,500 TPS |
| **p95 Latency** | 0.8 ms | 0.9 ms |
| **CPU Usage** | 65% | 68% |
| **Reason** | No write-after log (WAL) overhead | Redo log overhead |

**Note:** Comparisons are illustrative. Actual performance depends on workload, configuration, and hardware.

---

## 10. Regression Testing

### 10.1. Automated Benchmark CI

**GitHub Actions Workflow:**

```yaml
name: Performance Regression Tests

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]

jobs:
  benchmark:
    runs-on: ubuntu-latest-8core  # Consistent hardware

    steps:
      - uses: actions/checkout@v3

      - name: Build ScratchBird
        run: |
          cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
          cmake --build build

      - name: Run Core Benchmark Suite
        run: |
          ./build/bin/scratchbird-benchmark --suite=core --output=results.json

      - name: Compare with Baseline
        run: |
          python scripts/compare_benchmarks.py results.json baseline.json

      - name: Upload Results
        uses: actions/upload-artifact@v3
        with:
          name: benchmark-results
          path: results.json
```

### 10.2. Performance Regression Detection

**Comparison Script:**

```python
def detect_regression(current, baseline, threshold=0.10):
    """Detect performance regressions."""
    regressions = []

    for benchmark_name in current.keys():
        if benchmark_name not in baseline:
            continue  # New benchmark, skip

        curr_tps = current[benchmark_name]['throughput_tps']
        base_tps = baseline[benchmark_name]['throughput_tps']

        degradation = (base_tps - curr_tps) / base_tps

        if degradation > threshold:
            regressions.append({
                'benchmark': benchmark_name,
                'baseline_tps': base_tps,
                'current_tps': curr_tps,
                'degradation_pct': degradation * 100
            })

    return regressions

# Usage
regressions = detect_regression(current_results, baseline_results, threshold=0.10)

if regressions:
    print("❌ Performance regressions detected:")
    for reg in regressions:
        print(f"  {reg['benchmark']}: {reg['degradation_pct']:.1f}% slower")
    sys.exit(1)
else:
    print("✅ No performance regressions detected")
```

### 10.3. Baseline Updates

**Update Baseline (Manual Approval):**

```bash
# Run benchmarks
./scripts/run-benchmarks.sh --suite=core --output=new_baseline.json

# Review results
./scripts/compare_benchmarks.py new_baseline.json baseline.json

# Update baseline (if approved)
cp new_baseline.json baseline.json
git add baseline.json
git commit -m "Update performance baseline"
```

---

## 11. Cluster Benchmarks

### 11.1. Distributed Query Benchmark

**Cluster Configuration:**
- 3 nodes
- 16 shards (RF=2)
- 10 Gbps network

**Benchmark:**

```sql
-- Distributed JOIN across shards
SELECT
    o.order_id,
    c.customer_name,
    SUM(oi.quantity * p.price) AS total
FROM orders o
JOIN customers c ON o.customer_id = c.customer_id
JOIN order_items oi ON o.order_id = oi.order_id
JOIN products p ON oi.product_id = p.product_id
GROUP BY o.order_id, c.customer_name;
```

**Metrics:**
- Query latency (p95)
- Network utilization
- Cross-shard communication overhead

**Target:** < 20% overhead vs. single-node

### 11.2. Shard Rebalancing

**Benchmark:** Add node, trigger shard rebalancing, measure impact.

**Metrics:**
- Rebalancing duration
- Foreground query latency impact
- Network bandwidth usage

**Target:** < 10% latency increase during rebalancing

### 11.3. Failover Performance

**Benchmark:** Kill primary shard replica, measure failover time.

**Metrics:**
- Failover detection time
- New leader election time
- Query availability downtime

**Target:** < 5 sec total failover time

---

## 12. Best Practices

### 12.1. Benchmark Design

1. **Use Realistic Data**
   - Representative data distributions
   - Appropriate data sizes
   - Include NULLs, duplicates

2. **Warm Up Properly**
   - Load data into buffer pool
   - Populate caches
   - Stabilize resource usage

3. **Measure Steady State**
   - Avoid startup transients
   - Run long enough (60+ seconds)
   - Monitor for stability

4. **Control Variables**
   - Consistent hardware
   - Isolated environment
   - Repeatable setup

5. **Report Context**
   - Hardware specs
   - Software versions
   - Configuration parameters

### 12.2. Optimization Process

1. **Measure Baseline**
   - Establish current performance
   - Identify bottlenecks

2. **Hypothesize**
   - Form theory about bottleneck
   - Predict impact of change

3. **Apply Change**
   - Make single change
   - Document modification

4. **Measure Impact**
   - Re-run benchmarks
   - Compare results

5. **Iterate**
   - Keep improvements
   - Revert regressions
   - Repeat process

### 12.3. Common Pitfalls

1. **❌ Insufficient warmup** → Inaccurate results
2. **❌ Too short measurement window** → High variance
3. **❌ Multiple concurrent changes** → Cannot isolate impact
4. **❌ Ignoring resource limits** → Unrealistic results
5. **❌ Using debug builds** → Not representative of production

---

## 13. Reporting and Visualization

### 13.1. Benchmark Report Template

```markdown
# ScratchBird Performance Benchmark Report

**Date:** 2026-01-07
**Version:** v1.0.0-alpha
**Commit:** a1b2c3d4

## Environment

- **Hardware:** 16 cores, 64 GB RAM, 1 TB NVMe SSD
- **OS:** Ubuntu 22.04 LTS, kernel 5.15
- **Configuration:** Recommended (see config.ini)

## Results

### Write-Heavy OLTP

| Metric | Value |
|--------|-------|
| Throughput | 15,200 TPS |
| p50 Latency | 0.4 ms |
| p95 Latency | 0.8 ms |
| p99 Latency | 1.2 ms |
| CPU Usage | 65% |
| Cache Hit Rate | 96% |

**Status:** ✅ PASS (target: 10,000+ TPS)

### Read-Heavy OLTP

| Metric | Value |
|--------|-------|
| Throughput | 52,000 TPS |
| p95 Latency | 0.3 ms |
| CPU Usage | 55% |

**Status:** ✅ PASS (target: 18,000+ TPS)

## Conclusion

All benchmarks passed. Performance meets or exceeds targets.
```

### 13.2. Visualization

**Throughput Over Time:**

```python
import matplotlib.pyplot as plt

plt.plot(timestamps, throughput_tps)
plt.xlabel('Time (seconds)')
plt.ylabel('Throughput (TPS)')
plt.title('ScratchBird Throughput - Write-Heavy OLTP')
plt.savefig('throughput.png')
```

**Latency Percentiles:**

```python
percentiles = [50, 75, 90, 95, 99, 99.9]
latencies = [0.4, 0.6, 0.7, 0.8, 1.2, 2.1]

plt.bar(percentiles, latencies)
plt.xlabel('Percentile')
plt.ylabel('Latency (ms)')
plt.title('Latency Distribution - Write-Heavy OLTP')
plt.savefig('latency_percentiles.png')
```

**Resource Utilization:**

```python
fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(10, 8))

ax1.plot(timestamps, cpu_pct, label='CPU %')
ax1.set_ylabel('CPU %')

ax2.plot(timestamps, memory_mb, label='Memory MB')
ax2.set_ylabel('Memory (MB)')

ax3.plot(timestamps, io_util_pct, label='I/O Util %')
ax3.set_ylabel('I/O Util %')
ax3.set_xlabel('Time (seconds)')

plt.tight_layout()
plt.savefig('resource_utilization.png')
```

---

## Appendix A: Dataset Definitions

### A.1. Tiny (Smoke Tests)

```sql
-- 100 rows
INSERT INTO orders (customer_id, total_amount, status)
SELECT
    (random() * 100)::INT,
    (random() * 1000)::DECIMAL(10,2),
    CASE (random() * 3)::INT
        WHEN 0 THEN 'pending'
        WHEN 1 THEN 'processing'
        ELSE 'completed'
    END
FROM generate_series(1, 100);
```

### A.2. Small (Unit Tests)

```sql
-- 10,000 rows
INSERT INTO orders (customer_id, total_amount, status)
SELECT
    (random() * 1000)::INT,
    (random() * 1000)::DECIMAL(10,2),
    CASE (random() * 3)::INT
        WHEN 0 THEN 'pending'
        WHEN 1 THEN 'processing'
        ELSE 'completed'
    END
FROM generate_series(1, 10000);
```

### A.3. Medium (Integration Tests)

```sql
-- 1,000,000 rows
INSERT INTO orders (customer_id, total_amount, status)
SELECT
    (random() * 100000)::INT,
    (random() * 1000)::DECIMAL(10,2),
    CASE (random() * 3)::INT
        WHEN 0 THEN 'pending'
        WHEN 1 THEN 'processing'
        ELSE 'completed'
    END
FROM generate_series(1, 1000000);
```

---

## Appendix B: Benchmark Tools

### B.1. pgbench (PostgreSQL-compatible)

```bash
# Initialize pgbench schema
pgbench -i -s 100 scratchbird_bench

# Run TPC-B-like workload
pgbench -c 10 -j 4 -T 60 scratchbird_bench
```

### B.2. sysbench

```bash
# Prepare database
sysbench oltp_read_write \
    --db-driver=pgsql \
    --pgsql-host=localhost \
    --pgsql-user=scratchbird \
    --pgsql-db=bench \
    --table-size=1000000 \
    prepare

# Run benchmark
sysbench oltp_read_write \
    --db-driver=pgsql \
    --pgsql-host=localhost \
    --pgsql-user=scratchbird \
    --pgsql-db=bench \
    --threads=16 \
    --time=60 \
    run
```

### B.3. YCSB

```bash
# Load data
./bin/ycsb load jdbc -P workloads/workloada \
    -p db.driver=org.scratchbird.jdbc.Driver \
    -p db.url=jdbc:scratchbird://localhost:3092/bench

# Run workload
./bin/ycsb run jdbc -P workloads/workloada \
    -p db.driver=org.scratchbird.jdbc.Driver \
    -p db.url=jdbc:scratchbird://localhost:3092/bench \
    -threads 16
```

### B.4. Custom Benchmark Harness

```python
#!/usr/bin/env python3

import argparse
import psycopg2
import time
import json

def run_benchmark(args):
    conn = psycopg2.connect(
        host=args.host,
        port=args.port,
        database=args.database,
        user=args.user
    )

    metrics = BenchmarkMetrics()
    metrics.start_time = time.time()

    # Warmup
    for _ in range(args.warmup_ops):
        execute_operation(conn, args.workload)

    # Measurement
    while time.time() - metrics.start_time < args.duration_sec:
        start = time.time()
        execute_operation(conn, args.workload)
        latency_ms = (time.time() - start) * 1000
        metrics.record_op(latency_ms)

    metrics.end_time = time.time()

    # Report
    results = metrics.report()
    print(json.dumps(results, indent=2))

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--host', default='localhost')
    parser.add_argument('--port', type=int, default=3092)
    parser.add_argument('--database', default='bench')
    parser.add_argument('--user', default='scratchbird')
    parser.add_argument('--workload', required=True)
    parser.add_argument('--duration-sec', type=int, default=60)
    parser.add_argument('--warmup-ops', type=int, default=1000)

    args = parser.parse_args()
    run_benchmark(args)
```

---

## Appendix C: Glossary

| Term | Definition |
|------|------------|
| **TPS** | Transactions per second |
| **OLTP** | Online Transaction Processing (short, frequent transactions) |
| **OLAP** | Online Analytical Processing (complex queries, aggregations) |
| **HTAP** | Hybrid Transactional/Analytical Processing |
| **p50/p95/p99** | 50th/95th/99th percentile latency |
| **tpmC** | Transactions per minute (NewOrder) - TPC-C metric |
| **YCSB** | Yahoo! Cloud Serving Benchmark |
| **MGA** | Multi-Generational Architecture (Firebird-style MVCC) |

---

## Appendix D: References

- [TPC-C Specification](http://www.tpc.org/tpcc/)
- [YCSB Benchmark](https://github.com/brianfrankcooper/YCSB)
- [PostgreSQL pgbench](https://www.postgresql.org/docs/current/pgbench.html)
- [sysbench Documentation](https://github.com/akopytov/sysbench)
- [Firebird Performance Guide](https://firebirdsql.org/file/documentation/html/en/firebirddocs/fbperfguide/firebird-performance-guide.html)

---

**Document Status:** ✅ Complete
**Last Review:** 2026-01-07
**Next Review:** 2026-04-07 (Quarterly)
**Owner:** ScratchBird Core Team
