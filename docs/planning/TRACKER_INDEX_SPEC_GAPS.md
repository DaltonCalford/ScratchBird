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
- [x] **Columnstore:** spec calls for delta/bitpack; implementation currently RLE + dictionary only.
- [x] **LSM Tree:** file-based storage not integrated with tablespace IDs; define mapping or constraints.

### Advanced Indexes (Not Implemented)

- [ ] **Bloom Filter Index:** implement standalone index per spec (`docs/specifications/indexes/BloomFilterIndex.md`).
  - [x] Core Bloom filter pages + BTree/Hash/GIN attach, query gating, and GC rebuild hooks.
  - [x] Persist Bloom filter meta pointer/config in index params + SQL options (CREATE INDEX).
  - [x] ALTER INDEX SET bloom_filter options.
- [ ] **Inverted Index:** implement full inverted index (positions, compression, BM25) per spec (`docs/specifications/indexes/InvertedIndex.md`).
- [ ] **IVF Index:** implement IVF (training, PQ, inverted lists, query path) per spec (`docs/specifications/indexes/IVFIndex.md`).
- [ ] **Zone Maps Index:** implement zone map index per spec (`docs/specifications/indexes/ZoneMapsIndex.md`).

## Cross-Cutting Requirements

- [ ] Ensure all index entry formats store `TID` with full `GPID + slot`.
- [ ] Ensure all index page allocations and pin/unpin operations are tablespace-aware (`root_gpid` + `tablespace_id`).
- [ ] Ensure migration paths update TIDs for both uncompressed and compressed storage formats.
