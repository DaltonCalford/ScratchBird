# Production Database Index Implementation Guide

**Four advanced index types with complete specifications for database developers building high-performance data systems.** This guide synthesizes production implementations from Lucene, RocksDB, Cassandra, ClickHouse, Parquet, and Faiss, providing code-level details for immediate use.

## Overview and selection criteria

Modern database systems require specialized indexes beyond B-trees to handle full-text search, write-heavy workloads, analytical queries, and vector similarity. **Inverted indexes** power sub-100ms full-text search across terabytes. **Bloom filters** reduce LSM-tree read amplification by 10-100x with minimal memory. **Zone maps** enable data skipping that accelerates analytical queries 10-100x. **IVF indexes** make billion-vector searches tractable at ~1ms latency. Each addresses fundamental performance bottlenecks where traditional indexes fall short.

The specifications below provide production-ready implementation details: exact data structures with field-level layouts, compression algorithms with pseudocode, integration patterns with query optimizers, and configuration parameters from battle-tested systems.

---

## 1. Inverted Index for Full-Text Search

### Core data structures

The inverted index consists of a **term dictionary** mapping terms to metadata, and **posting lists** containing document occurrences.

```python
InvertedIndex {
    dictionary: HashMap<Term, TermInfo>,
    postings: CompressedIntegerArray
}

TermInfo {
    doc_frequency: u32,           # Documents containing term
    posting_offset: u64,          # File pointer to posting list
    term_frequency_offset: u64    # Pointer to TF data
}

PostingList {
    doc_ids: [u32],               # Sorted document IDs
    frequencies: [u32],           # Term frequency per doc
    positions: [[u32]]            # Word positions (optional)
}
```

**Dictionary implementation:** HashMap (fast O(1) lookups) or FST/Finite State Transducer (5-10% overhead, compressed tries). Lucene uses FST for terms with separate metadata in .tim files.

### Token processing pipeline

```
Raw Text → Character Filters → Tokenization → Token Filters → Indexed Terms

Token Filters:
• Lowercase normalization
• Stop word removal (optional)
• Stemming (Porter/Snowball)
• Synonym expansion
```

**Implementation example:**
```rust
pub trait Analyzer {
    fn token_stream(&self, text: &str) -> TokenStream;
}

pub struct StandardAnalyzer {
    stop_words: HashSet<String>,
}

impl Analyzer for StandardAnalyzer {
    fn token_stream(&self, text: &str) -> TokenStream {
        TokenStream::new(text)
            .lowercase()
            .filter_stop_words(&self.stop_words)
            .stem(StemmerType::Porter)
    }
}
```

### Posting list compression

**VByte (Variable Byte) encoding:**
- 7 bits per byte for data, 1 bit continuation flag
- MSB=1: more bytes follow, MSB=0: final byte
- Optimal for small integers (0-127 use 1 byte)

```python
def compress_vbyte(integers):
    result = bytearray()
    for num in integers:
        while num >= 128:
            result.append((num & 0x7F) | 0x80)
            num >>= 7
        result.append(num & 0x7F)
    return bytes(result)
```

**PForDelta (Patched Frame of Reference):**
- Block size: 128 integers
- Bit-pack 90% of values with uniform width
- Store 10% exceptions separately
- 2-4x better compression than VByte
- SIMD-accelerated decompression (2-8 GB/s)

**Delta encoding** (applied before compression):
```
Doc IDs: [7, 12, 15, 17, 25] → Deltas: [7, 5, 3, 2, 8]
```

### Scoring mechanisms

**BM25 (Industry Standard):**
```
BM25(D,Q) = Σ IDF(qi) × [f(qi,D) × (k1 + 1)] / 
                        [f(qi,D) + k1 × (1 - b + b × |D|/avgdl)]

Where:
  qi = query terms
  f(qi,D) = term frequency in document D
  |D| = document length (number of terms)
  avgdl = average document length
  k1 = 1.2 (default, range 0.5-2.0, controls TF saturation)
  b = 0.75 (default, range 0-1, controls length normalization)
  
IDF(qi) = log[(N - n(qi) + 0.5) / (n(qi) + 0.5)]
```

**Parameter tuning:** k1=1.2, b=0.75 (standard), k1=2.0, b=0 (short docs), k1=1.0, b=0.5 (mixed lengths).

### Query processing

**Boolean AND (Intersection):**
```python
def intersect_sorted_lists(list1, list2):
    result = []
    i, j = 0, 0
    while i < len(list1) and j < len(list2):
        if list1[i] == list2[j]:
            result.append(list1[i])
            i += 1; j += 1
        elif list1[i] < list2[j]:
            i += 1
        else:
            j += 1
    return result
```

**Optimization:** Skip lists add pointers every √n positions, reducing intersection from O(n) to O(√n) skips.

### Apache Lucene storage format

**File structure per segment:**
```
_0.tim     # Term dictionary (terms + stats)
_0.tip     # Term index (random access)
_0.doc     # Doc IDs + frequencies + skip data
_0.pos     # Position data (word positions)
_0.pay     # Payloads and offsets
```

**Postings (.doc) format:**
```
PackedBlock (128 docs) {
    DocDelta[128]      # Bit-packed deltas
    Frequency[128]     # Bit-packed frequencies
}

SkipData (every 128 docs) {
    SkipDocFP         # File pointer to next block
    LastDoc           # Largest doc ID in block
}
```

### API interface

**Tantivy (Rust) example:**
```rust
// Schema definition
let mut schema_builder = Schema::builder();
let title = schema_builder.add_text_field("title", TEXT | STORED);
let body = schema_builder.add_text_field("body", TEXT);
let schema = schema_builder.build();

// Index creation
let index = Index::create_in_dir("index_path", schema.clone())?;
let mut writer = index.writer(50_000_000)?; // 50MB buffer

// Document indexing
writer.add_document(doc!(
    title => "Document title",
    body => "Full text content..."
))?;
writer.commit()?;

// Search
let reader = index.reader()?;
let searcher = reader.searcher();
let query_parser = QueryParser::for_index(&index, vec![title, body]);
let query = query_parser.parse_query("search terms")?;
let top_docs = searcher.search(&query, &TopDocs::with_limit(10))?;
```

### Segment-based architecture

**Write-once, read-many (WORM):**
- Segments are immutable after creation
- New documents → new segment
- Background merging combines segments (10→1, merge factor=10)
- Deletions via bitmap, cleaned during merge

**Query processing:** Search all segments in parallel, merge results with score normalization.

### Performance characteristics

**Complexity:**
- Index construction: O(n log n)
- Single term query: O(log t) + O(d) where d = matching docs
- Space: 20-40% of original text with compression

**Benchmarks:**
- Indexing: 10-50 MB/s
- Query latency: 1-50ms (p95)
- Compression: 4-10x for posting lists

### Configuration

```rust
IndexSettings {
    commit_interval: Duration,
    merge_policy: TieredMergePolicy {
        max_merge_at_once: 10,
        max_merged_segment_mb: 5000,
        segments_per_tier: 10
    },
    ram_buffer_mb: 256,
}
```

---

## 2. Bloom Filter Index for LSM-Tree Optimization

### Mathematical specifications

**Core formulas:**
```
Optimal bit array size:
m = -n × ln(p) / (ln(2)²)

Optimal hash functions:
k = (m/n) × ln(2) ≈ 0.693 × (m/n)

False positive rate:
FPR = (1 - e^(-kn/m))^k ≈ 2^(-m/n)
```

**Parameter guidelines:**

| Bits/Key | Hash Functions | FPR | Use Case |
|----------|---------------|-----|----------|
| 10 | 7 | 1% | Standard (recommended) |
| 14.4 | 10 | 0.1% | High-accuracy |
| 20 | 14 | 0.01% | Critical queries |

### Hash function selection

**Production choices:**
- **xxHash3 (XXH3)**: Fastest (10+ GB/s), modern choice
- **MurmurHash3**: 128-bit, widely compatible
- **CityHash**: Google's fast hash

**Double hashing** (generate k functions from 2 base hashes):
```python
h1 = murmur3_hash(key)
h2 = xxhash(key)
for i in range(k):
    index = (h1 + i * h2) % m
    set_bit(index)
```

### Bit array implementation

```cpp
struct BloomFilter {
    uint8_t* bits;          // Byte array
    size_t num_bits;        // m
    uint8_t num_hashes;     // k
    uint32_t num_keys;      // n inserted
};

void SetBit(uint8_t* bits, size_t index) {
    bits[index / 8] |= (1 << (index % 8));
}

bool CheckBit(const uint8_t* bits, size_t index) {
    return (bits[index / 8] & (1 << (index % 8))) != 0;
}
```

**Atomic operations for concurrent access:**
```cpp
class ThreadSafeBloomFilter {
    std::atomic<uint64_t>* words_;
    
    void AtomicSetBit(size_t bit_index) {
        size_t word_index = bit_index / 64;
        size_t bit_offset = bit_index % 64;
        uint64_t mask = 1ULL << bit_offset;
        
        uint64_t old_val = words_[word_index].load(std::memory_order_relaxed);
        while (true) {
            uint64_t new_val = old_val | mask;
            if (words_[word_index].compare_exchange_weak(
                    old_val, new_val, std::memory_order_relaxed)) {
                break;
            }
        }
    }
};
```

**Cache-efficient blocked filters:**
```cpp
// RocksDB: 64-byte blocks (512 bits per cache line)
struct BlockedBloomFilter {
    static constexpr size_t kBlockSize = 64;
    uint8_t* blocks_;
    
    void Add(const Slice& key) {
        uint32_t h = Hash(key);
        uint32_t block_idx = h % num_blocks_;
        uint8_t* block = blocks_ + (block_idx * kBlockSize);
        
        // All k probes within single cache line
        for (int i = 0; i < num_hashes_; i++) {
            uint32_t bit = (h + i * Delta(h)) % 512;
            SetBit(block, bit);
        }
    }
};
```

### Persistence format

```cpp
struct BloomFilterHeader {
    uint32_t magic_number;      // 0xB100F117
    uint16_t version;           // Format version
    uint16_t hash_type;         // 1=Murmur3, 2=xxHash
    uint32_t num_bits;          // m
    uint16_t num_hashes;        // k
    uint32_t num_keys_inserted; // n
    uint32_t checksum;          // CRC32
};

// File layout
[Header - 48 bytes]
[Bitset - (num_bits/8) bytes]
[Footer checksum - 4 bytes]
```

### LSM-tree integration

**Per-SSTable architecture:**
```
SSTable:
├── Data Blocks
├── Meta Blocks
│   ├── Index Block
│   └── Filter Block (Bloom Filter)
└── Footer
```

**Read path:**
```cpp
bool SSTable::Get(const Slice& key, std::string* value) {
    // 1. Check Bloom filter
    if (filter_ && !filter_->MayContain(key)) {
        return false;  // Skip this SSTable
    }
    
    // 2. Check index, read data block
    return ReadFromDisk(key, value);
}
```

**Multi-level LSM:**
```
Query: Get(key)
1. Check MemTable
2. For each level (L0 → L6):
   For each SSTable:
     a. Check Bloom filter → Skip if "definitely not"
     b. Check index → Read if "might exist"
```

**Performance impact:** Without filters: 10-50 SSD reads. With filters (1% FPR): 0.1-0.5 reads (10-100x reduction).

### RocksDB implementation

```cpp
rocksdb::BlockBasedTableOptions table_options;
table_options.filter_policy.reset(
    rocksdb::NewBloomFilterPolicy(
        10,     // bits_per_key → ~1% FPR
        false   // use_block_based (false = full filter)
    )
);

// Cache filters
table_options.cache_index_and_filter_blocks = true;
table_options.pin_l0_filter_and_index_blocks_in_cache = true;

options.table_factory.reset(
    rocksdb::NewBlockBasedTableFactory(table_options)
);
```

**Monitoring:**
```cpp
uint64_t useful = stats->getTickerCount(BLOOM_FILTER_USEFUL);
uint64_t checked = stats->getTickerCount(BLOOM_FILTER_CHECKED);
double skip_rate = (double)useful / checked;  // Target: >99%
```

### Cassandra implementation

```cql
CREATE TABLE users (
    user_id uuid PRIMARY KEY,
    name text
) WITH bloom_filter_fp_chance = 0.01;  -- 1% FPR

-- Rebuild after config change
ALTER TABLE users WITH bloom_filter_fp_chance = 0.001;
nodetool upgradesstables -a
```

**Characteristics:** Off-heap storage, per-SSTable filters, ~1-2 GB per billion keys.

### ClickHouse implementation

```sql
CREATE TABLE events (
    event_id UInt64,
    user_id UInt64,
    payload String,
    INDEX bf_user user_id TYPE bloom_filter() GRANULARITY 3,
    INDEX token_payload payload TYPE tokenbf_v1(8192, 3, 0) GRANULARITY 1
) ENGINE = MergeTree()
ORDER BY (event_id);
```

**Filter types:** Standard bloom, token bloom (substring search), n-gram bloom (LIKE queries).

### Dynamic resizing

**Scalable Bloom filters:**
```cpp
class ScalableBloomFilter {
    std::vector<BloomFilter*> filters_;
    double growth_factor_ = 2.0;
    double fpr_tightening_ = 0.85;
    
    void Add(const Slice& key) {
        if (filters_.empty() || filters_.back()->IsFull()) {
            size_t capacity = initial_capacity_ * pow(growth_factor_, filters_.size());
            double fpr = initial_fpr_ * pow(fpr_tightening_, filters_.size());
            filters_.push_back(new BloomFilter(capacity, fpr));
        }
        filters_.back()->Add(key);
    }
};
```

### Configuration checklist

**Phase 1:** xxHash3 + MurmurHash3, 10 bits/key, versioned format  
**Phase 2:** Atomic operations, blocked filters, LRU caching  
**Phase 3:** Partitioned filters, SIMD acceleration, scalable filters

---

## 3. Zone Maps (Min-Max Indexes) for Analytical Queries

### Storage structure

```c
struct ZoneMapEntry {
    DataType min_value;
    DataType max_value;
    uint64_t null_count;
    uint64_t row_count;
    uint64_t zone_id;
    uint8_t zone_state;         // FRESH, STALE, INVALID
    uint64_t distinct_count;    // HyperLogLog estimate
};
```

**ClickHouse granules:** 8,192 rows or ~10MB (adaptive)  
**Parquet:** 128MB row groups, 1MB pages  
**DuckDB:** 122,880 rows (fixed)

**Hierarchical levels:**
```
File Level: Global min/max
  ↓
Row Group Level: 128MB-1GB partitions
  ↓
Page Level: 1MB-8MB blocks
```

### Zone size determination

**Optimal ranges:**
- Minimum: 8KB (avoid overhead)
- Maximum: 1GB (maintain effectiveness)
- Recommended: 128MB-512MB

**Trade-off analysis:**
```python
# 1TB dataset, 256MB zones → 4k zones, 0.12MB metadata (0.00001%)
# 1TB dataset, 8MB zones → 131k zones, 4MB metadata (0.0004%)
```

**ClickHouse adaptive:**
```sql
CREATE TABLE events (...) ENGINE = MergeTree
SETTINGS 
    index_granularity = 8192,              -- Row limit
    index_granularity_bytes = 10485760;    -- 10MB limit
-- Finalizes granule when EITHER threshold reached
```

### Statistics collection

**Incremental computation:**
```cpp
class IncrementalStatistics {
    std::optional<DataType> min_value_;
    std::optional<DataType> max_value_;
    uint64_t null_count_ = 0;
    
    void Update(const std::optional<DataType>& value) {
        if (!value.has_value()) {
            null_count_++;
            return;
        }
        if (!min_value_ || value < min_value_) min_value_ = value;
        if (!max_value_ || value > max_value_) max_value_ = value;
    }
};
```

**Parquet writer pipeline:**
```python
def write_row(self, row):
    for col_idx, value in enumerate(row):
        if value is None:
            col_writer.null_count += 1
        else:
            col_writer.min_value = min(col_writer.min_value, value)
            col_writer.max_value = max(col_writer.max_value, value)
        
        if buffer_full:
            self.flush_page(col_idx)
```

**ClickHouse skip indexes:**
```sql
INDEX idx_minmax col TYPE minmax GRANULARITY 4;          -- Min/max
INDEX idx_set col TYPE set(100) GRANULARITY 4;           -- Exact membership
INDEX idx_bloom col TYPE bloom_filter(0.01) GRANULARITY 4; -- Bloom
INDEX idx_token text TYPE tokenbf_v1(8192, 3, 0) GRANULARITY 4; -- Substring
```

### Predicate pushdown

**Evaluation algorithm:**
```cpp
enum class SkipDecision { EXCLUDE, INCLUDE, MAYBE };

SkipDecision Evaluate(const ZoneMapEntry& zone, const Predicate& pred) {
    switch (pred.type) {
        case EQUALS:
            if (pred.value < zone.min || pred.value > zone.max)
                return EXCLUDE;
            if (zone.min == zone.max && zone.min == pred.value)
                return INCLUDE;
            return MAYBE;
        
        case LESS_THAN:
            if (zone.max < pred.value) return INCLUDE;
            if (zone.min >= pred.value) return EXCLUDE;
            return MAYBE;
        
        case BETWEEN:
            if (zone.min >= pred.low && zone.max <= pred.high)
                return INCLUDE;
            if (zone.max < pred.low || zone.min > pred.high)
                return EXCLUDE;
            return MAYBE;
        
        case IS_NULL:
            if (zone.null_count == 0) return EXCLUDE;
            if (zone.null_count == zone.row_count) return INCLUDE;
            return MAYBE;
    }
}
```

**Combining predicates:**
```cpp
// AND: Any EXCLUDE → zone excluded
SkipDecision EvaluateAND(zone, predicates) {
    if (any predicate returns EXCLUDE) return EXCLUDE;
    if (all predicates return INCLUDE) return INCLUDE;
    return MAYBE;
}

// OR: All EXCLUDE → zone excluded
SkipDecision EvaluateOR(zone, predicates) {
    if (all predicates return EXCLUDE) return EXCLUDE;
    if (any predicate returns INCLUDE) return INCLUDE;
    return MAYBE;
}
```

**ClickHouse query flow:**
```
1. Binary search granule range in primary.idx
2. Apply skip indexes → prune granules
3. Load marks for selected granules
4. Read only selected granules (10-100x less I/O)
```

**Performance:** 99.95% skip rate on ordered data, 10-100x speedup typical.

### Columnar storage integration

**Parquet structure:**
```
PAR1 (magic)
<Row Group 1>
  <Column 1 Chunk>
    [Data Pages with statistics]
  <Column 2 Chunk>
<Row Group N>
File Metadata (schema, row group stats)
Footer Length
PAR1 (magic)
```

**ClickHouse part:**
```
part_directory/
├── primary.idx        # Sparse index
├── column.bin         # Compressed data
├── column.mrk2        # Marks (offsets)
└── skp_idx_*.idx      # Skip indexes
```

**Best practice:** Hybrid metadata placement—coarse stats in footer, fine stats inline.

### Automatic maintenance

**ClickHouse:**
```sql
-- Automatic during merges
OPTIMIZE TABLE t FINAL;  -- Manual trigger

-- Materialize skip index
ALTER TABLE t MATERIALIZE INDEX idx;
```

**Oracle:**
```sql
CREATE MATERIALIZED ZONEMAP zm ON table(cols)
  REFRESH FAST ON COMMIT;

EXEC DBMS_MVIEW.REFRESH('ZM', 'C');  -- Complete
EXEC DBMS_MVIEW.REFRESH('ZM', 'F');  -- Fast (incremental)
```

**Parquet:** Immutable; use table formats (Iceberg, Delta) for compaction.

### Query optimizer integration

**Cost-based pruning:**
```python
def should_use_zone_maps(zones, predicate):
    cost_full_scan = len(zones) * ZONE_SCAN_COST
    estimated_zones = estimate_filtered_zones(zones, predicate)
    cost_with_pruning = METADATA_LOAD + estimated_zones * ZONE_SCAN_COST
    return cost_with_pruning < cost_full_scan
```

**Cardinality estimation:**
```python
def estimate_cardinality(zones, predicate):
    if predicate.type == "EQUALS":
        matching_zones = [z for z in zones if z.min <= val <= z.max]
        distinct = estimate_distinct(zones)
        return total_rows / distinct
    
    elif predicate.type == "RANGE":
        data_range = max(z.max) - min(z.min)
        pred_range = predicate.high - predicate.low
        selectivity = pred_range / data_range
        return total_rows * selectivity
```

### Data type support

**Numeric types:** Exact min/max, highly effective for IDs, timestamps, counts.

**Strings:** Lexicographic comparison, prefix optimization. ClickHouse stores first N bytes of min/max.

**Dates/times:** 32/64-bit integer comparison, excellent for time-series. Always include in ORDER BY.

**Complex types:**
- **Arrays:** Parquet statistics on leaf nodes after flattening
- **Nested structures:** Full support with Dremel encoding, statistics on each leaf
- **Maps:** Limited effectiveness, convert to arrays for better statistics

### Implementation guidance

**Schema design checklist:**
- Identify frequently filtered columns
- Order data by these columns (ORDER BY / sort)
- Use appropriate data types (INT for dates, not STRING)
- Partition by date/time for time-series

**Performance tuning:**
```sql
-- ClickHouse: Check skip effectiveness
SELECT 
    query,
    read_rows,
    result_rows,
    100 * (1 - result_rows/read_rows) AS skip_rate
FROM system.query_log
WHERE type = 'QueryFinish';
```

**Common pitfalls:**
1. Wrong column order: High cardinality first spreads data
2. Over-indexing: Too many skip indexes (>1% overhead)
3. Unsorted data: Random inserts make zone maps ineffective
4. Function on filtered column: WHERE YEAR(date)=2023 prevents zone map use

**Optimal patterns:**
```sql
-- Good: Pre-sorted by filter columns
INSERT INTO table SELECT * FROM source ORDER BY date, user_id;

-- Good: Rewrite to use column directly
WHERE date >= '2023-01-01' AND date < '2024-01-01'
-- Instead of: WHERE YEAR(date) = 2023
```

---

## 4. IVF (Inverted File Index) for Vector Search

### K-means clustering

**Core concept:** Partition vector space into Voronoi cells using k-means.

**nlist selection** (number of clusters):
```
Formula: 4*sqrt(N) to 16*sqrt(N) where N = dataset size
- 1M-10M vectors: nlist = 65,536 (2^16)
- 10M-100M vectors: nlist = 262,144 (2^18)
- 100M-1B vectors: nlist = 1,048,576 (2^20)
```

**Training requirements:**
- Need 30K to 256K training vectors
- For nlist=4096, need 122K to 1M training vectors
- Training vectors should represent dataset distribution

### Inverted list structure

**Data structure:** `cluster_id → list of (vector_id, encoded_vector)`

```cpp
struct IVFIndex {
    std::vector<float> centroids;  // nlist × d centroids
    std::vector<InvertedList> lists;
};

struct InvertedList {
    std::vector<uint64_t> vector_ids;
    std::vector<uint8_t> encoded_vectors;  // PQ codes or full vectors
};
```

**Storage:**
- **IndexIVFFlat:** Stores full vectors (4×d + 8 bytes per vector)
- **IndexIVFPQ:** Stores PQ codes (m + 8 bytes per vector)

### Product Quantization (PQ)

**Algorithm:**
1. **Split:** Divide d-dimensional vector into m subvectors (d/m each)
2. **Train:** Run k-means on each subspace → k=256 centroids per subspace
3. **Encode:** Replace each subvector with nearest centroid ID (1 byte)
4. **Store:** Vector becomes m-byte code

**Memory reduction:**
```
Original: 128-dim × 4 bytes = 512 bytes
PQ (m=8): 8 bytes (64x reduction, 98.4% compression)
```

**Parameters:**
- **m:** Number of subquantizers (8, 16, 32, 64)
- **nbits:** Bits per subquantizer (typically 8 → 256 centroids)
- **Constraint:** d must be divisible by m

**Distance computation (Asymmetric Distance Computation):**
```python
# Pre-compute distance table
distance_table = np.zeros((m, 256))
for i in range(m):
    for j in range(256):
        distance_table[i][j] = dist(query_subvector[i], codebook[i][j])

# Compute distance to encoded vector
def compute_distance(pq_code):
    total_dist = 0
    for i in range(m):
        total_dist += distance_table[i][pq_code[i]]
    return total_dist
```

**Residual encoding (IVFPQ):**
- Encode residual = original_vector - partition_centroid
- Reduces variance → smaller quantization errors

### Search algorithm

**Process:**
```python
def search(query, k, nprobe):
    # 1. Find nprobe nearest centroids
    nearest_centroids = coarse_quantizer.search(query, nprobe)
    
    # 2. Scan inverted lists
    candidates = []
    for centroid_id in nearest_centroids:
        for vector in inverted_lists[centroid_id]:
            dist = compute_distance(query, vector)
            candidates.append((dist, vector.id))
    
    # 3. Return top-k
    candidates.sort()
    return candidates[:k]
```

**nprobe parameter:**
- Controls speed-accuracy tradeoff
- Higher nprobe = more accurate, slower
- Typical values: 1-256 (default 1-10)
- Search fraction ≈ nprobe/nlist
- Example: nlist=4096, nprobe=20 → searches ~0.5% of dataset

**Time complexity:**
- Coarse quantization: O(nlist × d) for flat quantizer
- List scanning: O(nprobe × vectors_per_list × d*)
- With PQ: Much faster via table lookup

### Training phase

**Two-stage process:**

```python
# Stage 1: Coarse quantizer training
quantizer = faiss.IndexFlatL2(d)
index = faiss.IndexIVFPQ(quantizer, d, nlist, m, nbits)

# Stage 2: Train on representative sample
training_vectors = sample_dataset(30000 * nlist)  # 30K-256K per cluster
index.train(training_vectors)
# - Runs k-means to create centroids
# - Trains PQ codebooks on residuals

# Add vectors
index.add(vectors)
```

**Training time:** Can be slow for large nlist. Use GPU acceleration or HNSW quantizer.

### Faiss implementation

**IndexIVFFlat:**
```python
import faiss

d = 128  # dimension
nlist = 4096
quantizer = faiss.IndexFlatL2(d)
index = faiss.IndexIVFFlat(quantizer, d, nlist)

index.train(train_data)
index.add(vectors)
index.nprobe = 20  # Set at search time
D, I = index.search(queries, k=10)
```

**Memory:** (4×d + 8) bytes per vector

**IndexIVFPQ:**
```python
d = 128
nlist = 4096
m = 16      # subquantizers
nbits = 8   # bits per code (256 centroids)

quantizer = faiss.IndexFlatL2(d)
index = faiss.IndexIVFPQ(quantizer, d, nlist, m, nbits)

index.train(train_data)
index.add(vectors)
index.nprobe = 48
D, I = index.search(queries, k=10)
```

**Memory:** (m + 8) bytes per vector (16+8 = 24 bytes for m=16)

**Key C++ structure:**
```cpp
struct IndexIVF {
    Index* quantizer;              // Coarse quantizer
    size_t nlist;                  // Number of lists
    size_t nprobe;                 // Search parameter
    InvertedLists* invlists;       // Storage
    
    virtual void train(idx_t n, const float* x);
    virtual void add(idx_t n, const float* x);
    virtual void search(idx_t n, const float* x, idx_t k,
                       float* distances, idx_t* labels);
};
```

### HNSW integration

**Purpose:** Use HNSW as coarse quantizer for faster centroid assignment.

**Benefits:**
- Standard assignment: O(nlist × d) per query
- HNSW assignment: O(log(nlist) × M × d) → 8-10x faster
- More balanced clusters
- Faster training

**Implementation:**
```python
# Method 1: Index factory
index = faiss.index_factory(d, "IVF65536_HNSW32,PQ32")

# Method 2: Manual
quantizer = faiss.IndexHNSWFlat(d, M=32)
index = faiss.IndexIVFPQ(quantizer, d, nlist=65536, m=16, nbits=8)
index.train(train_data)
index.add(vectors)
index.nprobe = 48
```

**Factory syntax:**
- `IVF4096_HNSW32,Flat` - HNSW quantizer, flat storage
- `IVF65536_HNSW32,PQ32` - HNSW quantizer, 32-byte PQ
- Number after HNSW = M parameter (graph connectivity)

**Performance:** IVFPQ+HNSW best for billion-scale, ~15x smaller than pure HNSW.

### Storage format

**Serialization:**
```python
# Write to disk
faiss.write_index(index, "index.faiss")

# Read from disk
index = faiss.read_index("index.faiss")
```

**Memory layout:**
```
Header: metadata (d, nlist, metric, m, nbits)
Quantizer: coarse quantizer index
PQ Codebooks: m × 256 × (d/m) centroids
Inverted Lists:
  - List 0: [id0, pq_code0], [id1, pq_code1], ...
  - List 1: ...
  - List nlist-1: ...
```

**Storage estimates:**
- **IndexIVFFlat:** ~(4×d + overhead) bytes per vector
- **IndexIVFPQ:** ~(m + 8) bytes per vector + codebook overhead

### Performance characteristics

**Typical benchmarks:**
- **Flat index:** 100% recall, slowest
- **IndexPQ:** 50% recall, 1.5ms search
- **IndexIVFPQ:** 50% recall, 0.09ms search (90x faster than flat)

**Parameter tuning:**
- Larger nlist: faster search, needs more training data
- Larger nprobe: better recall, slower search
- Larger m: better compression (d must divide m)
- Larger nbits: better accuracy, larger codebooks

**Accuracy-speed tradeoff:**
```
Configuration examples (1M vectors, d=128):
- nlist=1024, nprobe=1: 0.01ms, 30% recall
- nlist=4096, nprobe=20: 0.09ms, 50% recall
- nlist=4096, nprobe=100: 0.3ms, 80% recall
- nlist=16384, nprobe=256: 1.2ms, 95% recall
```

### Implementation pipeline

**Complete example:**
```python
import faiss
import numpy as np

# Parameters
d = 128
nlist = 4096
m = 16
nbits = 8
nprobe = 20

# Create index
quantizer = faiss.IndexFlatL2(d)
index = faiss.IndexIVFPQ(quantizer, d, nlist, m, nbits)

# Train (sample 30K-256K vectors)
train_data = sample_dataset(100000)
index.train(train_data)

# Add vectors
index.add(vectors)

# Search
index.nprobe = nprobe
D, I = index.search(queries, k=10)

# Save
faiss.write_index(index, "ivfpq.index")
```

**Using index factory (recommended):**
```python
# Simple specification
index = faiss.index_factory(d, "IVF4096,PQ32")

# With HNSW quantizer
index = faiss.index_factory(d, "IVF65536_HNSW32,PQ32")

# With preprocessing (OPQ = Optimized Product Quantization)
index = faiss.index_factory(d, "OPQ32_128,IVF4096,PQ32")
```

### Critical constraints

1. **Dimension:** d must be divisible by m for PQ
2. **Training data:** 30K-256K vectors recommended
3. **nbits:** Only 8, 12, 16 supported in IndexIVFPQ
4. **Training:** MUST call train() before add() for IVF indexes
5. **nprobe:** Set at search time, not construction time

---

## Cross-Index Integration Patterns

### Query optimizer integration

**Cost-based selection:**
```cpp
class IndexSelector {
    Index* SelectBestIndex(const Query& query) {
        if (query.type == FULL_TEXT) {
            return inverted_index_;
        } else if (query.type == POINT_LOOKUP && query.selectivity < 0.001) {
            if (bloom_filter_->MayContain(query.key)) {
                return primary_index_;
            }
            return nullptr;  // Skip scan
        } else if (query.type == RANGE && query.is_analytical) {
            // Use zone maps for data skipping
            auto selected_zones = zone_map_->PruneZones(query.predicate);
            return CreateScanPlan(selected_zones);
        } else if (query.type == VECTOR_SIMILARITY) {
            return ivf_index_;
        }
        return default_index_;
    }
};
```

### Hybrid index strategies

**LSM-tree with multiple indexes:**
```
SSTable structure:
├── Data blocks (4KB-256KB)
├── Bloom filter (point lookup optimization)
├── Zone maps (range query optimization)
├── Inverted index (full-text search)
└── Footer with pointers
```

**Columnar database with composite indexes:**
```
Column storage:
├── Primary data (compressed column chunks)
├── Zone maps (min/max per zone)
├── Bloom filters (high-cardinality columns)
├── Inverted indexes (text columns)
└── Vector indexes (embedding columns)
```

### Performance monitoring

**Key metrics across all index types:**

```cpp
struct IndexMetrics {
    // Effectiveness
    double hit_rate;           // Bloom: skip rate, Zone: prune rate
    double false_positive_rate; // Bloom filters
    double skip_percentage;     // Zone maps
    double recall_at_k;         // Vector indexes
    
    // Performance
    double avg_query_latency_ms;
    double p95_query_latency_ms;
    double p99_query_latency_ms;
    
    // Resource usage
    size_t memory_bytes;
    double cpu_utilization;
    size_t disk_bytes;
    
    // Maintenance
    double index_build_time_sec;
    double merge_time_sec;
    uint64_t rebuild_count;
};
```

### Configuration best practices

**Memory budgeting:**
```python
def allocate_index_memory(total_memory_gb):
    """Distribute memory across index types"""
    return {
        'bloom_filters': total_memory_gb * 0.15,    # 15% - highest ROI
        'zone_maps': total_memory_gb * 0.05,        # 5% - lightweight
        'inverted_index': total_memory_gb * 0.30,   # 30% - dictionaries
        'vector_index': total_memory_gb * 0.40,     # 40% - if using vectors
        'buffer_cache': total_memory_gb * 0.10      # 10% - operations
    }
```

**Tuning priority:**
1. **Bloom filters:** Start with 10 bits/key, adjust based on read/write ratio
2. **Zone maps:** Ensure data is sorted by filter columns
3. **Inverted index:** Tune merge policy to balance query speed and write throughput
4. **IVF index:** Balance nprobe for target recall (80-95% typical)

---

## Benchmark Methodologies

### Inverted index benchmarks

**Dataset:** Wikipedia corpus (6M documents, 3B tokens)

**Metrics:**
- Indexing throughput (MB/s)
- Index size (% of corpus)
- Query latency (p50, p95, p99)
- Recall@10 for ranked queries

**Test queries:**
```
Single term: "database"
Phrase: "machine learning"
Boolean: "vector AND search NOT legacy"
Fuzzy: "algorithm~2" (edit distance 2)
```

**Expected results:**
- Indexing: 20-50 MB/s
- Index size: 30-40% of corpus
- Query latency: 5-50ms (p95)
- Compression: 5-8x for posting lists

### Bloom filter benchmarks

**Dataset:** LSM-tree with 100 SSTables, 1M keys each

**Metrics:**
- False positive rate (actual vs theoretical)
- Query skip rate (% SSTables avoided)
- Memory overhead (bytes per key)
- Query latency reduction

**Test workload:**
```
90% negative queries (key doesn't exist)
10% positive queries (key exists)
```

**Expected results:**
- 10 bits/key → 1% FPR, 99% skip rate
- Memory: 1.25 bytes per key
- Latency reduction: 10-50x for negative queries

### Zone map benchmarks

**Dataset:** 1TB TPC-H lineitem table, 6B rows

**Metrics:**
- Data skip percentage
- Query speedup vs full scan
- Metadata overhead
- Predicate pushdown effectiveness

**Test queries:**
```sql
SELECT * FROM lineitem WHERE l_shipdate = '1995-03-15';
SELECT * FROM lineitem WHERE l_quantity > 45;
SELECT * FROM lineitem WHERE l_orderkey BETWEEN 1000000 AND 2000000;
```

**Expected results:**
- Skip rate: 95-99.9% for selective predicates
- Speedup: 20-100x for filtered queries
- Metadata: <0.1% of data size

### Vector index benchmarks

**Dataset:** 1M SIFT vectors (128-dim)

**Metrics:**
- Recall@10, Recall@100
- Query latency (QPS)
- Index size (compression ratio)
- Training time

**Test configurations:**
```
IVF4096,Flat: High accuracy baseline
IVF4096,PQ32: Balanced
IVF16384,PQ32: Speed-optimized
IVF65536_HNSW32,PQ32: Large-scale
```

**Expected results (recall@10):**
- nprobe=1: 30% recall, 0.01ms
- nprobe=10: 60% recall, 0.05ms
- nprobe=50: 85% recall, 0.2ms
- nprobe=256: 95% recall, 1.0ms

---

## Conclusion

These four index types address distinct performance bottlenecks in modern database systems. **Inverted indexes** enable millisecond full-text search through compressed posting lists and BM25 scoring. **Bloom filters** reduce read amplification in LSM-trees by 10-100x with just 10 bits per key. **Zone maps** accelerate analytical queries through data skipping, achieving 10-100x speedups with <1% overhead. **IVF indexes** make billion-scale vector search tractable through clustering and product quantization, compressing vectors 50-100x while maintaining 80-95% recall.

**Implementation priorities:**
1. Start with production-proven defaults (10 bits/key for Bloom, 256MB zones, k1=1.2/b=0.75 for BM25, nlist=4√N for IVF)
2. Instrument extensively to measure actual effectiveness
3. Tune based on workload characteristics (read-heavy vs write-heavy, point vs range queries)
4. Monitor and adapt as data distributions change

**Key success factors:**
- Data organization matters: Sort by filter columns, partition by time
- Memory management: Budget appropriately across index types
- Maintenance: Regular compaction, rebuild, and statistics refresh
- Integration: Tight coupling with query optimizer for automatic usage

These specifications provide production-ready building blocks for high-performance database systems handling modern workloads at scale.

---

## ScratchBird GC Integration

All advanced indexes must comply with `INDEX_GC_PROTOCOL.md`:

- Indexes that store per-row TIDs (inverted, zone map, IVF) implement
  `IndexGCInterface::removeDeadEntries()` and filter dead TIDs during sweep.
- Immutable segment indexes (inverted, FST) perform GC during segment merges.
- Probabilistic auxiliary indexes (Bloom, HLL, CMS) trigger rebuilds rather
  than per-row deletions.

Each ScratchBird-specific index spec defines its GC behavior in detail.
