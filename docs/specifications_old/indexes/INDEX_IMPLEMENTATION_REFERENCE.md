# Index Implementation Reference (Authoritative Algorithm Links)

This document maps each index type to its **authoritative algorithm** section.
If a type is listed without an algorithm, use `INDEX_IMPLEMENTATION_SPEC.md`
for required MGA/UUID rules and consider the implementation incomplete.

## Core Index Types

- BTREE: `docs/specifications/parser/v3/indexes/BTREE_SPEC.md#authoritative-algorithm-normative-2026-02-07`
- HASH: `docs/specifications/parser/v3/indexes/HASH_SPEC.md#authoritative-algorithm-normative-2026-02-07`
- FULLTEXT: `docs/specifications/parser/v3/indexes/InvertedIndex.md#authoritative-algorithm-normative-2026-02-07`
- GIN: `docs/specifications/parser/v3/indexes/GIN_SPEC.md#authoritative-algorithm-normative-2026-02-07`
- GIST: `docs/specifications/parser/v3/indexes/GIST_SPEC.md#authoritative-algorithm-normative-2026-02-07`
- SPGIST: `docs/specifications/parser/v3/indexes/SPGIST_SPEC.md#authoritative-algorithm-normative-2026-02-07`
- BRIN: `docs/specifications/parser/v3/indexes/BRIN_SPEC.md#authoritative-algorithm-normative-2026-02-07`
- BITMAP: `docs/specifications/parser/v3/indexes/BITMAP_SPEC.md#authoritative-algorithm-normative-2026-02-07`
- RTREE: `docs/specifications/parser/v3/indexes/RTREE_SPEC.md#authoritative-algorithm-normative-2026-02-07`
- HNSW: `docs/specifications/parser/v3/indexes/HNSW_SPEC.md#authoritative-algorithm-normative-2026-02-07`
- COLUMNSTORE: `docs/specifications/parser/v3/indexes/COLUMNSTORE_SPEC.md#authoritative-algorithm-normative-2026-02-07`
- LSM: `docs/specifications/parser/v3/indexes/LSM_TREE_SPEC.md#authoritative-algorithm-normative-2026-02-07`

## Remaining Core Index Types

- IVF: `docs/specifications/parser/v3/indexes/IVFIndex.md#authoritative-algorithm-normative-2026-02-07`
- ZONEMAP: `docs/specifications/parser/v3/indexes/ZoneMapsIndex.md#authoritative-algorithm-normative-2026-02-07`
- ZORDER: `docs/specifications/parser/v3/indexes/ZOrderIndex.md#authoritative-algorithm-normative-2026-02-07`
- GEOHASH: `docs/specifications/parser/v3/indexes/GeohashS2Index.md#authoritative-algorithm-normative-2026-02-07`
- S2: `docs/specifications/parser/v3/indexes/GeohashS2Index.md#authoritative-algorithm-normative-2026-02-07`
- QUADTREE: `docs/specifications/parser/v3/indexes/QuadtreeOctreeIndex.md#authoritative-algorithm-normative-2026-02-07`
- OCTREE: `docs/specifications/parser/v3/indexes/QuadtreeOctreeIndex.md#authoritative-algorithm-normative-2026-02-07`
- FST: `docs/specifications/parser/v3/indexes/FSTIndex.md#authoritative-algorithm-normative-2026-02-07`
- SUFFIX_ARRAY: `docs/specifications/parser/v3/indexes/SuffixIndex.md#authoritative-algorithm-normative-2026-02-07`
- SUFFIX_TREE: `docs/specifications/parser/v3/indexes/SuffixIndex.md#authoritative-algorithm-normative-2026-02-07`
- COUNT_MIN_SKETCH: `docs/specifications/parser/v3/indexes/CountMinSketchIndex.md#authoritative-algorithm-normative-2026-02-07`
- HYPERLOGLOG: `docs/specifications/parser/v3/indexes/HyperLogLogIndex.md#authoritative-algorithm-normative-2026-02-07`
- ART: `docs/specifications/parser/v3/indexes/AdaptiveRadixTreeIndex.md#authoritative-algorithm-normative-2026-02-07`
- LEARNED: `docs/specifications/parser/v3/indexes/LearnedIndex.md#authoritative-algorithm-normative-2026-02-07`
- LSM_TTL: `docs/specifications/parser/v3/indexes/LSMTimeSeriesIndex.md#authoritative-algorithm-normative-2026-02-07`
- JSON_PATH: `docs/specifications/parser/v3/indexes/JSONPathIndex.md#authoritative-algorithm-normative-2026-02-07`
- BLOOM_FILTER (auxiliary): `docs/specifications/parser/v3/indexes/BloomFilterIndex.md#authoritative-algorithm-normative-2026-02-07`

## Architecture & GC

- LSM architecture overview: `docs/specifications/parser/v3/indexes/LSM_TREE_ARCHITECTURE.md#authoritative-architecture-normative-2026-02-07`
- Index GC protocol: `docs/specifications/parser/v3/indexes/INDEX_GC_PROTOCOL.md` (protocol only)

## MGA/UUID Rule Anchor

All index algorithms must obey the record‑version metadata + TIP visibility rules
in `docs/specifications/parser/v3/indexes/INDEX_IMPLEMENTATION_SPEC.md`.
