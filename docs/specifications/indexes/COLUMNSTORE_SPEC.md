# Columnstore Index - Technical Specification
**Version:** 1.0
**Date:** November 3, 2025
**Status:** Specification for Full Implementation
**MGA Rules:** MUST follow `/MGA_RULES.md` - Pure Firebird MGA, NO PostgreSQL MVCC

---

## 1. Overview

### 1.1 Purpose

Columnstore index provides column-oriented storage optimized for analytical (OLAP) workloads. Data is stored by column rather than by row, enabling:
- High compression ratios (5-100x depending on data characteristics)
- Fast analytical queries (only read needed columns)
- Predicate pushdown (filter before decompression)
- Vectorized processing (batch operations on arrays)

### 1.2 Use Cases

- **Data Warehousing**: Large fact tables, star schema analytics
- **Time-Series Data**: Logs, metrics, events with temporal queries
- **Analytics Dashboards**: GROUP BY, aggregations, OLAP cubes
- **Reporting**: Business intelligence, data mining
- **Machine Learning**: Feature extraction from large datasets

### 1.3 Architecture

```
Traditional Row Store (Heap):
┌──────────────────────────────────┐
│ Row 1: [id:1, name:"Alice", age:25, sal:50000] │
│ Row 2: [id:2, name:"Bob",   age:30, sal:60000] │
│ Row 3: [id:3, name:"Carol", age:28, sal:55000] │
└──────────────────────────────────┘

Columnstore:
┌─────────────────────┐
│ Column: id          │
│ [1,2,3,4,5,...]    │
│ Compressed (RLE)    │
│ Min:1, Max:1000    │
└─────────────────────┘
┌─────────────────────┐
│ Column: name        │
│ ["Alice","Bob",...] │
│ Compressed (Dict)   │
└─────────────────────┘
┌─────────────────────┐
│ Column: age         │
│ [25,30,28,35,...]  │
│ Compressed (RLE)    │
│ Min:25, Max:65     │
└─────────────────────┘
┌─────────────────────┐
│ Column: salary      │
│ [50000,60000,...]  │
│ Compressed (BitPack)│
│ Min:40000, Max:120k│
└─────────────────────┘

Query: SELECT AVG(salary) WHERE age > 30
Step 1: Scan "age" column only
Step 2: Check segment min/max (skip if max ≤ 30)
Step 3: Decompress and filter (age > 30)
Step 4: Get matching TIDs: [1025, 1026, ...]
Step 5: Scan "salary" column for those TIDs only
Step 6: Compute AVG (no row reconstruction!)

Result: 10-100x faster than full table scan
```

---

## 2. Compression Algorithms

### 2.1 Run-Length Encoding (RLE)

**Algorithm:**
```
Input:  [1, 1, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4]
Output: [(1, 3), (2, 2), (3, 4), (4, 3)]
Format: (value, count) pairs
```

**Implementation:**
```cpp
Status compressRLE(const std::vector<int64_t>& values,
                   std::vector<uint8_t>* compressed) {
    if (values.empty()) return Status::OK;

    int64_t current_value = values[0];
    uint32_t run_length = 1;

    for (size_t i = 1; i < values.size(); i++) {
        if (values[i] == current_value && run_length < UINT32_MAX) {
            run_length++;
        } else {
            // Write (value, count) pair
            writeInt64(compressed, current_value);
            writeUInt32(compressed, run_length);

            current_value = values[i];
            run_length = 1;
        }
    }

    // Write final run
    writeInt64(compressed, current_value);
    writeUInt32(compressed, run_length);

    return Status::OK;
}
```

**Best For:**
- Sorted columns (timestamps, IDs)
- Low-cardinality columns (status codes, boolean flags)
- Repeated values (NULL-heavy columns)

**Compression Ratio:**
- Best case: 1000x (all same value)
- Typical: 5-10x (sorted low-cardinality)
- Worst case: 1.2x overhead (all unique values)

### 2.2 Dictionary Encoding

**Algorithm:**
```
Input:  ["California", "Texas", "California", "New York", "Texas", "California"]
Dictionary: {"California": 0, "Texas": 1, "New York": 2}
Codes:  [0, 1, 0, 2, 1, 0]
Format: Dictionary (string -> code) + Code array
```

**Implementation:**
```cpp
Status compressDictionary(const std::vector<std::string>& values,
                          std::vector<uint8_t>* compressed) {
    // Build dictionary
    std::unordered_map<std::string, uint32_t> dict;
    std::vector<uint32_t> codes;
    uint32_t next_code = 0;

    for (const auto& value : values) {
        auto [it, inserted] = dict.try_emplace(value, next_code);
        if (inserted) {
            next_code++;
        }
        codes.push_back(it->second);
    }

    // Check if worthwhile (cardinality < 70% of values)
    if (dict.size() > values.size() * 0.7) {
        return Status::InvalidArgument("Dictionary not effective");
    }

    // Write dictionary
    writeUInt32(compressed, dict.size());
    for (const auto& [str, code] : dict) {
        writeUInt32(compressed, code);
        writeString(compressed, str);
    }

    // Write codes (bit-pack based on dictionary size)
    uint8_t bits_per_code = std::ceil(std::log2(dict.size()));
    bitPack(codes, bits_per_code, compressed);

    return Status::OK;
}
```

**Best For:**
- String columns with low cardinality
- Categorical data (country codes, product types)
- Enum-like columns

**Compression Ratio:**
- Best case: 90% (few unique strings, many repetitions)
- Typical: 50-70% (moderate cardinality)
- Not used: When cardinality > 70%

### 2.3 Bit-Packing

**Algorithm:**
```
Input:  [25, 30, 28, 35, 27] (ages 25-35)
Range:  Min=25, Max=35, Span=10
Bits:   4 bits sufficient (2^4=16 > 10)
Deltas: [0, 5, 3, 10, 2] (subtract min)
Packed: 0101 0011 1010 0010 (4 bits each)
Format: (min_value, bits_per_value, packed_data)
```

**Implementation:**
```cpp
Status compressBitPack(const std::vector<int64_t>& values,
                       std::vector<uint8_t>* compressed) {
    if (values.empty()) return Status::OK;

    // Find min/max
    int64_t min_val = *std::min_element(values.begin(), values.end());
    int64_t max_val = *std::max_element(values.begin(), values.end());
    int64_t span = max_val - min_val;

    // Calculate bits needed
    uint8_t bits = span == 0 ? 0 : std::ceil(std::log2(span + 1));

    // Write header
    writeInt64(compressed, min_val);
    writeUInt8(compressed, bits);

    if (bits == 0) {
        // All same value, just store count
        writeUInt32(compressed, values.size());
        return Status::OK;
    }

    // Pack deltas
    BitWriter writer(compressed);
    for (int64_t value : values) {
        uint64_t delta = value - min_val;
        writer.writeBits(delta, bits);
    }
    writer.flush();

    return Status::OK;
}
```

**Best For:**
- Integer columns with limited range
- IDs in a narrow range
- Age, year, month columns

**Compression Ratio:**
- Best case: 75-90% (small range like ages 0-127)
- Typical: 50-70% (moderate range)
- Not used: When range spans full integer width

### 2.4 Delta Encoding

**Algorithm:**
```
Input:     [1000, 1001, 1003, 1006, 1010] (timestamps)
Deltas:    [1000, 1, 2, 3, 4] (first + differences)
Bit-Pack:  Store deltas with minimal bits
Format:    (base_value, deltas_compressed)
```

**Implementation:**
```cpp
Status compressDelta(const std::vector<int64_t>& values,
                     std::vector<uint8_t>* compressed) {
    if (values.empty()) return Status::OK;

    // Write base value
    int64_t base = values[0];
    writeInt64(compressed, base);

    // Compute deltas
    std::vector<int64_t> deltas;
    deltas.reserve(values.size() - 1);
    for (size_t i = 1; i < values.size(); i++) {
        deltas.push_back(values[i] - values[i-1]);
    }

    // Bit-pack deltas
    return compressBitPack(deltas, compressed);
}
```

**Best For:**
- Sequential data (timestamps, auto-increment IDs)
- Time-series data
- Monotonic columns

**Compression Ratio:**
- Best case: 80-95% (sequential timestamps)
- Typical: 60-80% (mostly sequential)

---

## 3. Predicate Pushdown

### 3.1 Min/Max Pruning

**Algorithm:**
```cpp
bool canSkipSegment(const ColumnPredicate& pred,
                   int64_t segment_min,
                   int64_t segment_max) {
    switch (pred.op) {
    case Op::EQUAL:
        // Skip if value outside [min, max]
        return pred.value < segment_min || pred.value > segment_max;

    case Op::LESS_THAN:
        // Skip if all values >= predicate
        return segment_min >= pred.value;

    case Op::GREATER_THAN:
        // Skip if all values <= predicate
        return segment_max <= pred.value;

    case Op::LESS_EQUAL:
        return segment_min > pred.value;

    case Op::GREATER_EQUAL:
        return segment_max < pred.value;

    default:
        return false;  // Can't skip
    }
}
```

**Example:**
```
Query: SELECT * FROM logs WHERE timestamp > '2025-11-01'

Segment 1: [2025-10-01 to 2025-10-31] → SKIP (max < '2025-11-01')
Segment 2: [2025-11-15 to 2025-11-20] → SKIP (max < '2025-11-01')
Segment 3: [2025-11-01 to 2025-11-05] → SCAN (overlaps range)
Segment 4: [2025-11-10 to 2025-11-15] → SCAN (all values match)

Result: Only scan segments 3 and 4 (skip 1 and 2)
```

### 3.2 Compressed Predicate Evaluation

**For RLE:**
```cpp
std::vector<uint32_t> evaluateRLE(const RLERun* runs, size_t num_runs,
                                  const ColumnPredicate& pred) {
    std::vector<uint32_t> matching_offsets;

    uint32_t offset = 0;
    for (size_t i = 0; i < num_runs; i++) {
        bool matches = evaluatePredicate(runs[i].value, pred);

        if (matches) {
            // Add all offsets in this run
            for (uint32_t j = 0; j < runs[i].count; j++) {
                matching_offsets.push_back(offset + j);
            }
        }

        offset += runs[i].count;
    }

    return matching_offsets;
}
```

**Benefit:** Evaluate predicate on compressed runs, not individual values!

---

## 4. Firebird MGA Compliance

### 4.1 MGA Rules Reference

**CRITICAL**: See `/MGA_RULES.md` for complete Firebird MGA rules.

**Key Rules:**
- Use TransactionId (uint64_t), NOT Snapshot
- TIP-based visibility (Transaction Inventory Pages)
- xmin/xmax on all versions
- Soft delete (set xmax, physical removal via sweep/GC)
- No PostgreSQL MVCC contamination

### 4.2 Segment-Level Versioning

**Structure:**
```cpp
struct SBColumnstorePage {
    // ... other fields ...

    // MGA compliance
    uint64_t cs_xmin;  // Transaction that created segment
    uint64_t cs_xmax;  // Transaction that deleted segment (0 if active)
    uint64_t cs_lsn;   // Last LSN that modified segment
};
```

**Visibility Check:**
```cpp
bool isSegmentVisible(const SBColumnstorePage* segment,
                     uint64_t current_xid,
                     TransactionManager* txn_mgr) {
    // Firebird MGA rules (NOT PostgreSQL MVCC!)

    // Created after current transaction
    if (segment->cs_xmin > current_xid) {
        return false;
    }

    // Deleted before current transaction
    if (segment->cs_xmax != 0 && segment->cs_xmax <= current_xid) {
        return false;
    }

    // TIP-based visibility check
    if (!txn_mgr->isVersionVisible(segment->cs_xmin, current_xid)) {
        return false;
    }

    if (segment->cs_xmax != 0 &&
        txn_mgr->isVersionVisible(segment->cs_xmax, current_xid)) {
        return false;
    }

    return true;
}
```

### 4.3 Row-Level Versioning

**Challenge:** Columnstore stores values by column, but MGA needs per-row versioning.

**Solution:** Version vector per segment
```cpp
struct ColumnSegment {
    std::vector<uint64_t> row_xmin;  // xmin for each row
    std::vector<uint64_t> row_xmax;  // xmax for each row (0 if not deleted)
    std::vector<uint8_t> data;       // Compressed column data
};
```

**Scan with Visibility:**
```cpp
Status scan(const ColumnSegment& segment,
           const ColumnPredicate* pred,
           uint64_t current_xid,
           ColumnScanBatch* batch) {
    // Decompress segment
    std::vector<int64_t> values;
    decompress(segment.data, &values);

    // Apply predicate and visibility
    for (uint32_t i = 0; i < values.size(); i++) {
        // Check visibility (Firebird MGA)
        if (!isRowVisible(segment.row_xmin[i],
                         segment.row_xmax[i],
                         current_xid)) {
            continue;  // Skip invisible row
        }

        // Check predicate
        if (pred && !evaluatePredicate(values[i], *pred)) {
            continue;  // Skip non-matching row
        }

        // Add to batch
        batch->tids.push_back(segment.first_tid + i);
        batch->values.append(&values[i], sizeof(int64_t));
    }

    return Status::OK;
}
```

---

## 5. Batch Processing

### 5.1 Vectorized Scans

**Concept:** Process 1024 values at a time for CPU cache efficiency

```cpp
constexpr size_t BATCH_SIZE = 1024;

Status scanVectorized(const ColumnSegment& segment,
                     const ColumnPredicate* pred,
                     uint64_t current_xid,
                     std::vector<ColumnScanBatch>* batches) {
    std::vector<int64_t> values;
    decompress(segment.data, &values);

    ColumnScanBatch current_batch;
    current_batch.data_type = segment.data_type;

    for (size_t offset = 0; offset < values.size(); offset += BATCH_SIZE) {
        size_t batch_end = std::min(offset + BATCH_SIZE, values.size());

        for (size_t i = offset; i < batch_end; i++) {
            if (!isRowVisible(segment.row_xmin[i],
                            segment.row_xmax[i],
                            current_xid)) {
                continue;
            }

            if (pred && !evaluatePredicate(values[i], *pred)) {
                continue;
            }

            current_batch.tids.push_back(segment.first_tid + i);
            // ... add value ...

            if (current_batch.count >= BATCH_SIZE) {
                batches->push_back(std::move(current_batch));
                current_batch = ColumnScanBatch();
                current_batch.data_type = segment.data_type;
            }
        }
    }

    if (current_batch.count > 0) {
        batches->push_back(std::move(current_batch));
    }

    return Status::OK;
}
```

### 5.2 SIMD Optimization (Future)

**Concept:** Use SIMD instructions for predicate evaluation

```cpp
// Example: AVX2 comparison (8 int32 at once)
__m256i evaluatePredicateSIMD(__m256i values, int32_t threshold) {
    __m256i thresh = _mm256_set1_epi32(threshold);
    return _mm256_cmpgt_epi32(values, thresh);  // values > threshold
}
```

---

## 6. Segment Management

### 6.1 Segment Structure

**Segment Size:** 1024 rows per segment (configurable)

**Segment Chain:**
```
Root Segment (id_column)
  → cs_next_segment → Segment 2 (rows 1025-2048)
    → cs_next_segment → Segment 3 (rows 2049-3072)
      → cs_next_segment → NULL
```

### 6.2 Segment Creation

**Algorithm:**
1. Buffer incoming values until segment full (1024 rows)
2. Compress buffered values
3. Allocate new page
4. Write compressed data
5. Update min/max values
6. Set xmin (current transaction)
7. Link to previous segment

### 6.3 Segment Compaction

**Trigger:** Too many deleted rows (>50% deleted)

**Process:**
1. Scan segment and skip deleted rows (xmax != 0 and visible)
2. Collect live rows
3. Compress live rows
4. Write to new segment
5. Update segment chain
6. Mark old segment as deleted (set xmax)
7. Sweep/GC will reclaim old segment later

---

## 7. Performance Characteristics

### 7.1 Compression Ratios

| Column Type | Algorithm | Typical Ratio |
|-------------|-----------|---------------|
| Sorted integers | RLE | 10x |
| Timestamps | Delta + RLE | 15x |
| Low-cardinality strings | Dictionary | 20x |
| Boolean flags | RLE | 30x |
| Small-range integers | Bit-pack | 5x |
| Random integers | None | 1x |

**Example (1M row table):**
- id (INT): 4MB → 400KB (10x, RLE)
- timestamp (TIMESTAMP): 8MB → 500KB (16x, delta+RLE)
- status (VARCHAR): 10MB → 500KB (20x, dictionary)
- amount (DECIMAL): 8MB → 4MB (2x, bit-pack)

**Total:** 30MB → 5.4MB (5.5x overall)

### 7.2 Query Performance

**Query:** `SELECT AVG(amount), COUNT(*) FROM transactions WHERE timestamp > '2025-01-01' AND status = 'COMPLETED'`

| Storage | I/O | Decompress | Filter | Aggregate | Total |
|---------|-----|------------|--------|-----------|-------|
| Row Store | 30MB | 0ms | 250ms | 50ms | 300ms |
| Columnstore | 1MB (2 cols only) | 20ms | 30ms | 10ms | 60ms |

**Speedup:** 5x faster

---

## 8. Implementation Requirements

### 8.1 Core Compression (70-110 hours)

**RLE (20-30 hours):**
- Implement compressRLE() for all integer types
- Implement decompressRLE()
- Optimize for sorted columns
- Handle NULL values in runs

**Dictionary (30-40 hours):**
- Build dictionary during segment creation
- Implement variable-width code bit-packing
- Handle string data efficiently
- Cardinality check (skip if >70%)

**Bit-Packing (20-30 hours):**
- Implement BitWriter/BitReader
- Calculate optimal bits per value
- Handle variable bit widths (1-64 bits)
- SIMD optimization for unpacking

### 8.2 Predicate Pushdown (30-40 hours)

- Min/max pruning
- Compressed predicate evaluation
- RLE-aware filtering
- Dictionary-aware filtering
- Bloom filters for existence checks (future)

### 8.3 Segment Management (30-40 hours)

- Segment creation with buffering
- Segment chain traversal
- Segment compaction
- Garbage collection integration
- TID mapping

### 8.4 MGA Integration (20-30 hours)

- Row-level xmin/xmax vectors
- TIP-based visibility checks
- Soft delete handling
- Sweep/GC integration
- Multi-version segment handling

### 8.5 Batch Processing (20-30 hours)

- Vectorized scan implementation
- Batch assembly (1024 values)
- Late materialization
- SIMD predicate evaluation (future)

**TOTAL:** 140-180 hours

---

## 9. Testing Requirements

### 9.1 Compression Tests

- Verify RLE correctness
- Verify dictionary encoding
- Verify bit-packing
- Test compression ratios
- Test NULL handling
- Test extreme values

### 9.2 Predicate Tests

- Test all 8 operators (=, !=, <, <=, >, >=, IS NULL, IS NOT NULL)
- Test min/max pruning
- Test compressed evaluation
- Test predicate combinations

### 9.3 MGA Tests

- Test visibility across transactions
- Test concurrent reads
- Test soft deletes
- Test sweep/GC integration
- Test xmin/xmax correctness

### 9.4 Performance Tests

- Measure compression ratios on real data
- Measure scan performance vs row store
- Measure aggregate performance
- Measure memory usage
- Measure compaction overhead

---

## 10. References

- [The Design and Implementation of Modern Column-Oriented Database Systems](https://blog.acolyer.org/2018/09/26/the-design-and-implementation-of-modern-column-oriented-database-systems/)
- [DuckDB Lightweight Compression](https://duckdb.org/2022/10/28/lightweight-compression)
- [ClickHouse Compression Techniques](https://chistadata.com/compression-techniques-column-oriented-databases/)
- [Parquet Encoding Algorithms](https://dataninjago.com/2021/12/07/databricks-deep-dive-3-parquet-encoding/)
- `/MGA_RULES.md` - Firebird MGA rules (MUST READ before implementation)

---

**Document Version:** 1.0
**Last Updated:** November 3, 2025
**Status:** Ready for implementation
**Estimated Effort:** 140-180 hours
