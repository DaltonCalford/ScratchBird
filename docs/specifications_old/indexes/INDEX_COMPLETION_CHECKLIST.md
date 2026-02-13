# Index Completion Checklist (Per Index Type)

**Date:** 2026-02-07
**Scope:** Identify missing details required for fully implementable specs.

Legend:
- **Missing** means not present or not concrete enough.
- **Partial** means mentioned but lacks algorithmic detail.

---

## Core Index Types

### BTREE
- Missing: full on-disk page layout (header, slot array, prefix compression layout, overflow handling)
- Missing: split/merge algorithms, rebalancing policies, and invariant proofs
- Missing: lock/latch order and concurrency model for readers/writers
- Missing: GC integration details for leaf pruning and internal node cleanup
- Missing: recovery/consistency checks on open

### HASH
- Missing: directory/bucket page formats (extendible hashing)
- Missing: split/rehash algorithm detail
- Missing: overflow chain structure and cleanup
- Missing: lock/latch order for directory + buckets
- Missing: GC removal algorithm for dead record versions

### GIN
- Missing: posting list on-disk format + compression
- Missing: posting tree node layout and split/merge
- Missing: pending list flush algorithm
- Missing: MGA‑compliant delete/tombstone handling (no physical delete)
- Missing: concurrency rules for pending list and posting merges

### GIST
- Missing: node layout and entry format
- Missing: split policy and picksplit details
- Missing: lock/latch order and predicate consistency rules
- Missing: GC semantics for dead entries

### SPGIST
- Missing: node/page layout and partitioning schema
- Missing: insertion/traversal algorithms for partitions
- Missing: lock/latch rules
- Missing: GC rules

### BRIN
- Missing: range summary storage layout
- Missing: update/merge algorithm on inserts
- Missing: GC/rebuild thresholds
- Missing: concurrency rules

### BITMAP
- Missing: bitmap page layout + compression format
- Missing: insert/delete logic (currently stubbed)
- Missing: MGA visibility and GC semantics
- Missing: lock/latch order

### RTREE
- Missing: node/entry format
- Missing: split algorithm (quadratic/linear) details
- Missing: deletion/condense tree algorithm
- Missing: GC rules for dead record versions

### HNSW
- Missing: node layout and adjacency list storage
- Missing: insertion/search parameters + neighbor selection rules
- Missing: deletion/cleanup rules under MGA
- Missing: lock/latch model for graph updates

### COLUMNSTORE
- Partial: row group/segment layouts; missing exact on-disk format
- Missing: compression block format and dictionary encoding details
- Missing: GC compaction algorithm and thresholds
- Missing: lock/latch order for segment append/merge

### LSM
- Partial: compaction policy; missing concrete level sizing rules
- Missing: SSTable on-disk format + index blocks
- Missing: manifest format + atomic update protocol
- Missing: concurrency/lock model for memtable + flush

---

## Remaining Core Index Types

### IVF
- Partial: inverted list entry format; missing exact on-disk layout
- Missing: PQ encoding storage format and update logic
- Missing: GC compaction rules for dead record versions
- Missing: lock/latch model for training vs insert

### ZONEMAP
- Partial: segment mapping; missing exact storage layout
- Missing: rebuild algorithm and thresholds

### ZORDER
- Partial: node layout and split/merge rules
- Missing: on-disk page format and key encoding

### GEOHASH/S2
- Partial: posting list format + split/merge
- Missing: on-disk layout and range query algorithm detail

### QUADTREE/OCTREE
- Partial: node format and split/merge criteria
- Missing: deletion/condense algorithm

### FST
- Partial: segment file format and merge rules
- Missing: FST node encoding layout

### SUFFIX_ARRAY/SUFFIX_TREE
- Partial: suffix construction algorithm and storage
- Missing: on-disk format and incremental update algorithm

### COUNT_MIN_SKETCH
- Partial: rebuild thresholds + persistence format

### HYPERLOGLOG
- Partial: rebuild thresholds + persistence format

### ART
- Partial: snapshot persistence format + rebuild procedure
- Missing: lock/latch rules for node update

### LEARNED
- Partial: model storage format and retraining schedule
- Missing: delta index merge algorithm detail

### LSM_TTL
- Partial: TTL compaction thresholds + tombstone aging
- Missing: on-disk format for TTL metadata

### JSON_PATH
- Partial: posting list format + path dictionary layout
- Missing: update/delete algorithm detail

### BLOOM_FILTER
- Partial: bitset layout + rebuild triggers

---

## Global Gaps (Across Many Indexes)

- Concrete lock/latch modes and order per index
- On-disk page formats with exact fields
- Error handling/consistency checks during open
- Explicit GC/sweep interaction steps for each index type

