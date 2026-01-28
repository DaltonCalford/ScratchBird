# Index Spec Gap Tracker

This checklist tracks spec-driven gaps for index implementations, with a focus on tablespace (GPID/TID) correctness and advanced index types.

## Spec Updates (Advanced Indexes)

- [x] Add tablespace + TID/GPID requirements to Bloom filter spec (`docs/specifications/indexes/BloomFilterIndex.md`).
- [x] Add tablespace + TID/GPID requirements to Inverted index spec (`docs/specifications/indexes/InvertedIndex.md`).
- [x] Add tablespace + TID/GPID requirements to IVF spec (`docs/specifications/indexes/IVFIndex.md`).
- [x] Add tablespace + TID/GPID requirements to Zone Maps spec (`docs/specifications/indexes/ZoneMapsIndex.md`).

## Implementation Gaps (By Index Type)

### Core Indexes

- [x] **B-Tree:** GC path drops non-primary tablespace TIDs; must accept full GPID/TID (`src/core/btree.cpp:2417`).
- [x] **B-Tree:** `updateTIDsAfterMigration` uses legacy 32-bit page IDs; must support GPID rewrite (`src/core/btree.cpp:2760`).
- [x] **Hash:** audit migration/GC paths for GPID-only, legacy-free TID handling.
- [x] **GIN:** compressed posting list handling + TID rewrite not implemented (`src/core/gin_index.cpp:930`, `src/core/gin_index.cpp:4794`).
- [x] **GiST:** legacy `create(..., root_page_out)` uses tablespace id 0; route via index tablespace (`src/core/gist_index.cpp:95`).
- [x] **BRIN:** legacy `create/open` assume PRIMARY tablespace; route via index tablespace (`src/core/brin_index.cpp:118`, `src/core/brin_index.cpp:174`).
- [x] **SP-GiST:** verify root allocation and all page pins use tablespace id derived from `root_gpid`.
- [x] **R-Tree:** verify wrapper/metadata uses tablespace id consistently for root/meta pages.
- [x] **HNSW:** `updateTIDsAfterMigration` still uses legacy TID conversions (`src/core/hnsw_index.cpp:1868`).
- [x] **Bitmap:** audit for any legacy TID conversions or non-GPID page pins.
- [x] **FULLTEXT:** V2 parser/semantic/bytecode supports `USING FULLTEXT` and `USING INVERTED`.
- [x] **Columnstore:** delta + bitpack compression implemented alongside RLE + dictionary; no remaining spec gap.
  - [x] GC interface wired and dead-segment removal implemented (`docs/specifications/indexes/INDEX_GC_PROTOCOL.md`).
  - [x] Full GC design (TID map + visibility bitmap + segment rewrite thresholds) implemented.
- [x] **LSM Tree:** file-based storage restricted to PRIMARY tablespace (explicit constraint).
  - [x] GC interface wired to drop dead TIDs from memtables/SSTables (`docs/specifications/indexes/INDEX_GC_PROTOCOL.md`).
  - [x] GC metadata (tid bloom/min/max + count) stored in SSTable footers for targeted compactions.

### Advanced Indexes (Not Implemented)

- [x] **Bloom Filter Index:** implemented per spec as an auxiliary per-index Bloom filter (spec explicitly rejects standalone index type).
  - [x] Core Bloom filter pages + BTree/Hash/GIN attach, query gating, and GC rebuild hooks.
  - [x] Persist Bloom filter meta pointer/config in index params + SQL options (CREATE INDEX).
  - [x] ALTER INDEX SET bloom_filter options.
- [x] **Inverted Index:** core structures + DML + search implemented per spec (`docs/specifications/indexes/InvertedIndex.md`).
  - [x] Implement GC purge via `InvertedIndex::removeDeadEntries` (`src/core/inverted_index.cpp:3795`).
- [x] **IVF Index:** implement IVF (training, PQ, inverted lists, query path) per spec (`docs/specifications/indexes/IVFIndex.md`).
  - [x] DDL surface (CREATE INDEX USING IVF) in V2 parser/semantic/bytecode.
- [x] **Zone Maps Index:** implement zone map index per spec (`docs/specifications/indexes/ZoneMapsIndex.md`).
  - [x] DDL surface (CREATE INDEX USING ZONEMAP) in V2 parser/semantic/bytecode.

## Cross-Cutting Requirements

- [x] Ensure all index entry formats store `TID` with full `GPID + slot`.
- [x] Ensure all index page allocations and pin/unpin operations are tablespace-aware (`root_gpid` + `tablespace_id`).
- [x] Ensure migration paths update TIDs for both uncompressed and compressed storage formats.
- [x] Ensure V2 parser DDL supports all index types required by specs (remaining: IVF, ZONEMAP).
