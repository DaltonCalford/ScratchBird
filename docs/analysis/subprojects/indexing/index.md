### Indexing

This module introduces multiple index families: B-Tree, Hash, Bitmap, GIN, R-Tree, LSM-Tree, Columnstore, and TTL, along with online build support. See the Phase 9 plan for background: `ProjectPlan/Phase 9 — Index Index families and advanced options.md`.

### B-Tree
- Lifecycle: create_empty, open_existing, validate, rebuild_offline
- Capabilities & limits:
  - Point: yes; Range: yes
  - INCLUDE columns: yes; Partial indexes: yes; Expression indexes: yes
  - Unique: supported; Compression: optional leaf prefix compression
- Optimizer: cost via estimate_search_cost/estimate_range_cost; capability flags via `IndexFamilyFactory::supports_*`
- Implementation References:
  - Header type: `include/scratchbird/engine/index_btree.h` class `BTreeIndex`
  - Insert: `src/engine/index_btree.cpp:L294` (BTreeIndex::insert)
  - Search equal: `src/engine/index_btree.cpp:L502` (BTreeIndex::search_equal)
  - Search range: `src/engine/index_btree.cpp:L535` (BTreeIndex::search_range)

### Hash
- Lifecycle: create_empty, open_existing, validate, rebuild_offline
- Capabilities & limits:
  - Point: yes; Range: no
  - INCLUDE columns: yes; Partial indexes: yes; Expression indexes: yes
  - Unique: supported; Compression: n/a
- Optimizer: O(1) average search cost; capability flags via `IndexFamilyFactory::supports_*`
- Implementation References:
  - Header type: `include/scratchbird/engine/index_hash.h` class `HashIndex`
  - Insert: `src/engine/index_hash.cpp:L80` (HashIndex::insert)
  - Search equal: `src/engine/index_hash.cpp:L137` (HashIndex::search_equal)

### Bitmap
- Lifecycle: create_empty, open_existing, validate, rebuild_offline
- Capabilities & limits:
  - Point: yes (equality); Range: no (conceptual only)
  - INCLUDE columns: no; Partial indexes: no; Expression indexes: no
  - Unique: not supported; Compression: RLE/WAH
- Optimizer: cheap equality lookups on low-cardinality data
- Implementation References:
  - Header type: `include/scratchbird/engine/index_bitmap.h` class `BitmapIndex`
  - Insert: `src/engine/index_bitmap.cpp:L20` (BitmapIndex::insert)
  - Search equal: `src/engine/index_bitmap.cpp:L33` (BitmapIndex::search_equal)

### GIN
- Lifecycle: create_empty, open_existing, validate, rebuild_offline
- Capabilities & limits:
  - Point: token/term equality; Range: no
  - INCLUDE columns: no; Partial indexes: no; Expression indexes: no
  - Unique: not supported; Compression: posting list compression
- Optimizer: token-based selectivity; boolean/phrase support at executor layer
- Implementation References:
  - Header type: `include/scratchbird/engine/index_gin.h` class `GinIndex`
  - Insert: `src/engine/index_gin.cpp:L61` (GinIndex::insert)
  - Search equal: `src/engine/index_gin.cpp:L88` (GinIndex::search_equal)

### R-Tree
- Lifecycle: create_empty, open_existing, validate, rebuild_offline
- Capabilities & limits:
  - Point: exact rectangle match; Range: spatial (intersects/contains/within)
  - INCLUDE columns: no; Partial indexes: no; Expression indexes: no
  - Unique: not supported
- Optimizer: spatial range enabled via `supports_range_queries`; cost via estimate_search_cost/estimate_range_cost
- Implementation References:
  - Header type: `include/scratchbird/engine/index_rtree.h` class `RTreeIndex`
  - Insert: `src/engine/index_rtree.cpp:L445` (RTreeIndex::insert)
  - Search equal: `src/engine/index_rtree.cpp:L483` (RTreeIndex::search_equal)
  - Search range: `src/engine/index_rtree.cpp:L531` (RTreeIndex::search_range)

### LSM-Tree
- Lifecycle: create_empty, open_existing, validate, rebuild_offline
- Capabilities & limits:
  - Point: yes; Range: yes
  - INCLUDE columns: yes; Partial indexes: yes; Expression indexes: yes
  - Unique: typically not enforced on write path; Compaction: size-tiered/leveled; Bloom filters optional
- Optimizer: cost reflects levels and Bloom presence; `supports_range_queries` enabled
- Implementation References:
  - Header type: `include/scratchbird/engine/index_lsm.h` class `LSMTreeIndex`
  - Insert: `src/engine/index_lsm.cpp:L540` (LSMTreeIndex::insert)
  - Search equal: `src/engine/index_lsm.cpp:L571` (LSMTreeIndex::search_equal)
  - Search range: `src/engine/index_lsm.cpp:L609` (LSMTreeIndex::search_range)

### Columnstore
- Lifecycle: create_empty, open_existing, validate, rebuild_offline
- Capabilities & limits:
  - Point: yes; Range: yes
  - INCLUDE columns: yes (covering); Partial indexes: no; Expression indexes: no
  - Unique: not supported; Compression: Dictionary, RLE, BitPacking, LZ4/ZSTD/SNAPPY
- Optimizer: cost considers compression/vectorization; `supports_range_queries` enabled
- Implementation References:
  - Header type: `include/scratchbird/engine/index_columnstore.h` class `ColumnstoreIndex`
  - Insert: `src/engine/index_columnstore.cpp:L197` (ColumnstoreIndex::insert)
  - Search equal: `src/engine/index_columnstore.cpp:L244` (ColumnstoreIndex::search_equal)
  - Search range: `src/engine/index_columnstore.cpp:L277` (ColumnstoreIndex::search_range)

### TTL
- Lifecycle: create_empty, open_existing, validate, rebuild_offline
- Capabilities & limits:
  - Point: yes; Range: linear scan only
  - INCLUDE columns: no; Partial indexes: no; Expression indexes: no
  - Unique: optional; TTL-specific expiry/cleanup
- Optimizer: linear scan costs for equality/range; range not supported for planning
- Implementation References:
  - Header type: `include/scratchbird/engine/index_ttl.h` class `TTLIndex`
  - Insert: `src/engine/index_ttl.cpp:L49` (TTLIndex::insert)
  - Search equal: `src/engine/index_ttl.cpp:L96` (TTLIndex::search_equal)

### Optimizer Integration
- Capabilities: `IndexFamilyFactory::supports_range_queries`, `supports_partial_indexes`, `supports_include_columns`, `supports_expression_indexes` (`src/engine/index_family.cpp`)
- Cost model: each family implements `estimate_search_cost`, `estimate_range_cost`, `estimate_maintenance_cost`

## Spec Trace
- [REQ-INDEX-FAMILIES-BTREE](../../traceability/spec/requirements.md#req-index-families-btree)
