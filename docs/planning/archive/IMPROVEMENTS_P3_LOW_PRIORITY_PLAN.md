# Alpha 1 - Low Priority Issues (P3) Implementation Plan

**Created:** November 23, 2025
**Updated:** November 25, 2025
**Priority:** P3 - LOW / ENHANCEMENTS
**Estimated Effort:** 200+ hours
**Target:** Post-Beta (Future releases)
**Dependencies:** P0, P1, P2 should be substantially complete

---

## STATUS SUMMARY

| Status | Count | Items |
|--------|-------|-------|
| ✅ Complete | 14 | P3-1, P3-5, P3-6, P3-7, P3-8, P3-9, P3-10, P3-11, P3-12, P3-13, P3-16, P3-17, P3-18, P3-19 |
| 🔒 Blocked (Alpha 3) | 3 | P3-2, P3-3, P3-4 |
| 🔒 Blocked (Dependencies) | 3 | P3-14, P3-15, P3-20 |

**Unblocked P3 Progress: 100% (14/14 items)**

---

## OVERVIEW

This plan covers 20+ low-priority enhancements and optimizations. These items improve performance, add advanced features, or enhance observability but are not critical for core functionality.

**Many items require Alpha 3 (network layer) or Beta features and should be deferred until those phases.**

**Execution Strategy:** Implement opportunistically when time permits or when specific features become necessary for higher-priority work.

---

## SECURITY ENHANCEMENTS

### P3-1: Password Expiration (6-8 hours) ✅ COMPLETE
**Feature:** 90-day password rotation policy
**Implementation:**
- Add password_expiry_date to user catalog
- Check on login, force password change if expired
- Configurable expiration period
**Files:** `include/scratchbird/security/password_policy.h`, `src/security/password_policy.cpp`

### P3-2: Multi-Factor Authentication (40-50 hours)
**Blocker:** Requires Alpha 3 (network layer)
**Methods:**
- TOTP (time-based one-time passwords)
- WebAuthn support
- SMS/email verification

### P3-3: IP Whitelisting/Blacklisting (8-10 hours)
**Blocker:** Requires Alpha 3 (network layer)
**Feature:**
- IP-based access control
- CIDR range support
- Per-user and global rules

### P3-4: Certificate-Based Authentication (20-25 hours)
**Blocker:** Requires Alpha 3 (network layer)
**Feature:** X.509 client certificates

### P3-13: View Security Context (6-8 hours) ✅ COMPLETE
**Feature:** SECURITY DEFINER vs SECURITY INVOKER for views
**Implementation:** Similar to procedure security context
**Files:** `include/scratchbird/security/view_security.h`, `src/security/view_security.cpp`

---

## DATA TYPE OPTIMIZATIONS

### P3-5: DECIMAL Fixed-Point Implementation (16-20 hours) ✅ COMPLETE
**Current:** DECIMAL stored as string (slow arithmetic)
**Improvement:** int128_t with scale tracking
**Impact:** 10-100x faster decimal arithmetic
**Files:** `include/scratchbird/core/decimal.h`, `src/core/decimal.cpp`

**Implementation:**
```cpp
struct DecimalValue {
    __int128_t value;     // Unscaled integer value
    uint8_t scale;        // Decimal places
    uint8_t precision;    // Total digits
};

// Example: 123.45 with DECIMAL(5,2)
// value = 12345, scale = 2, precision = 5
```

### P3-6: SIMD Vector Operations (12-16 hours) ✅ COMPLETE
**Current:** Scalar operations for vector dot product, L2 distance
**Improvement:** AVX2/AVX-512 vectorization
**Impact:** 4-8x faster vector queries
**Files:** `include/scratchbird/core/simd_vector.h`, `src/core/simd_vector.cpp`

**Example:**
```cpp
// Dot product with AVX-512
float dotProduct_AVX512(const float* a, const float* b, size_t dim) {
    __m512 sum = _mm512_setzero_ps();
    for (size_t i = 0; i < dim; i += 16) {
        __m512 va = _mm512_loadu_ps(&a[i]);
        __m512 vb = _mm512_loadu_ps(&b[i]);
        sum = _mm512_fmadd_ps(va, vb, sum);
    }
    return _mm512_reduce_add_ps(sum);
}
```

---

## INDEX ENHANCEMENTS

### P3-7: Columnstore Enhancements (30-40 hours) ✅ COMPLETE
**Missing:**
- Dictionary encoding for low-cardinality columns
- Vectorized execution (process columns in batches)
- Additional compression (RLE, bit-packing, delta encoding)
- Zone maps for pruning
**Files:** `include/scratchbird/index/columnstore_enhanced.h`, `src/index/columnstore_enhanced.cpp`

### P3-8: HNSW Quantization (12-16 hours) ✅ COMPLETE
**Feature:**
- Product quantization (PQ) for vectors
- Scalar quantization (SQ)
**Impact:** 4-8x memory reduction for vector indexes
**Files:** `include/scratchbird/index/vector_quantization.h`, `src/index/vector_quantization.cpp`

### P3-9: LSM Compression (4-6 hours) ✅ COMPLETE
**Missing:** SSTable compression using Zstd
**Impact:** 50-70% space savings
**Files:** `include/scratchbird/core/lsm_compression.h`

### P3-10: Bitmap RLE Compression (6-8 hours) ✅ COMPLETE
**Missing:** Run-length encoding for sequential TIDs
**Impact:** 50%+ space savings for bitmap indexes
**Files:** `include/scratchbird/index/bitmap_rle.h`, `src/index/bitmap_rle.cpp`

---

## TRANSACTION & CATALOG IMPROVEMENTS

### P3-11: TIP Compaction (10-12 hours) ✅ COMPLETE
**Issue:** TIP pages grow unbounded
**Fix:** Consolidate old committed/aborted transactions
**Implementation:**
- Periodically scan TIP pages
- Compress ranges of identical states
- Free unused TIP pages
**Files:** `include/scratchbird/core/tip_compaction.h`, `src/core/tip_compaction.cpp`

### P3-12: Catalog B-Tree Indexes (15-20 hours) ✅ COMPLETE
**Current:** Linear scan through catalog pages
**Improvement:** B-tree indexes on catalog tables
**Impact:** 100-1000x faster catalog lookups
**Files:** `include/scratchbird/catalog/catalog_index.h`, `src/catalog/catalog_index.cpp`

**Indexes to add:**
- pg_tables(table_name)
- pg_columns(table_id, column_name)
- pg_indexes(index_name)
- pg_users(username)
- pg_roles(role_name)

---

## QUERY OPTIMIZATION

### P3-14: Partition Pruning (30-40 hours)
**Feature:**
- Table partitioning (RANGE, LIST, HASH)
- Predicate-based partition elimination
- Partition-wise joins
- Dynamic partition pruning

### P3-15: Materialized View Rewriting (40-50 hours)
**Feature:** Automatic MV selection for queries
**Implementation:**
- Query pattern matching
- Cost-based MV selection
- MV substitution in query plan

### P3-19: Common Subexpression Elimination (15-20 hours) ✅ COMPLETE
**Feature:** CSE in WHERE clauses and expressions
**Impact:** Reduce redundant expression evaluations
**Files:** `include/scratchbird/optimizer/cse.h`, `src/optimizer/cse.cpp`

### P3-20: Join Ordering Optimization (25-30 hours)
**Feature:** Cost-based join ordering
**Current:** Left-to-right join order
**Improvement:** Dynamic programming or greedy algorithm

---

## OBSERVABILITY & MONITORING

### P3-16: Telemetry Export (15-20 hours) ✅ COMPLETE
**Feature:** Prometheus/OpenMetrics integration
**Metrics:**
- Query latency histograms
- Transaction throughput
- Cache hit rates
- Lock wait times
- Disk I/O statistics
**Files:** `include/scratchbird/core/telemetry.h`, `src/core/telemetry.cpp`

### P3-17: Structured Logging (10-12 hours) ✅ COMPLETE
**Improvement:** JSON logging format
**Benefits:**
- Machine-parseable logs
- Integration with log aggregation tools (ELK, Splunk)
- Structured error context
**Files:** `include/scratchbird/core/structured_logger.h`, `src/core/structured_logger.cpp`

### P3-18: Query Profiler (20-25 hours) ✅ COMPLETE
**Feature:** EXPLAIN ANALYZE with timing
**Output:**
- Per-operator timing
- Row counts (actual vs estimated)
- Memory usage
- I/O operations
**Files:** `include/scratchbird/optimizer/query_profiler.h`, `src/optimizer/query_profiler.cpp`

---

## SUMMARY BY FEATURE AREA

| Feature Area | Items | Est. Hours |
|--------------|-------|------------|
| **Security** | 5 | 80-101 |
| **Data Types** | 2 | 28-36 |
| **Indexes** | 4 | 52-70 |
| **Transactions** | 2 | 25-32 |
| **Query Optimization** | 4 | 110-130 |
| **Observability** | 3 | 45-57 |
| **Total** | 20 | 340-426 hours |

---

## PRIORITIZATION WITHIN P3

### Tier 1 (Implement First - 100-120 hours)
1. P3-12: Catalog B-Tree Indexes (critical for performance)
2. P3-11: TIP Compaction (prevents unbounded growth)
3. P3-5: DECIMAL Fixed-Point (significant performance win)
4. P3-7: Columnstore Enhancements (analytics performance)
5. P3-18: Query Profiler (essential for optimization)

### Tier 2 (Implement Second - 80-100 hours)
6. P3-14: Partition Pruning (scalability)
7. P3-6: SIMD Vector Operations (ML workloads)
8. P3-8: HNSW Quantization (vector index efficiency)
9. P3-16: Telemetry Export (production monitoring)
10. P3-19: Common Subexpression Elimination

### Tier 3 (Future - 160+ hours)
11. P3-15: Materialized View Rewriting
12. P3-20: Join Ordering Optimization
13. P3-2: Multi-Factor Authentication (requires Alpha 3)
14. P3-3: IP Whitelisting (requires Alpha 3)
15. P3-4: Certificate Auth (requires Alpha 3)
16. All others

---

## DEPENDENCIES & BLOCKERS

**Network Layer Required (Alpha 3):**
- P3-2: MFA
- P3-3: IP Whitelisting
- P3-4: Certificate Auth

**Statistics Required:**
- P3-15: MV Rewriting (needs query cost estimation)
- P3-20: Join Ordering (needs cardinality estimates)

**Partitioning Required:**
- P3-14: Partition Pruning (requires table partitioning implementation)

---

## RECOMMENDED APPROACH

1. **Focus on Tier 1 items first** - These provide the most value with least dependencies
2. **Defer network-dependent items** until Alpha 3
3. **Implement opportunistically** - When working in related code areas
4. **Validate with benchmarks** - All optimizations must show measurable improvement
5. **Consider Beta 4 integration** - Some features may align better with NoSQL/observability phase

---

## COMPLETION CRITERIA

P3 items are enhancements, not requirements. Completion criteria are flexible:

- ✅ Implementation is correct and tested
- ✅ Performance improvement validated (if optimization)
- ✅ No regressions introduced
- ✅ Documentation updated
- ✅ Integration tests passing

**P3 items can be deferred indefinitely if higher-priority work is available.**

---

**Document Status:** MOSTLY COMPLETE
**Last Updated:** November 25, 2025
**Note:** 14 of 20 P3 items complete. Remaining 6 items blocked by Alpha 3 or other dependencies.
