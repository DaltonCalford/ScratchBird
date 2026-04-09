# Milvus Audit

## Architectural Summary

Milvus is a donor for ANN-native planning and execution, not for general SQL optimization. Its strength is coordinator-driven segment routing, asynchronous index publication, and disciplined vector-search execution over sealed and growing segments.

## Query and Index Flow

1. Query/filter expressions are parsed into a serialized plan representation.
2. `QueryCoord` manages which collections and partitions are loaded and which segments are queryable.
3. `IndexCoord` and `IndexNode` build indexes asynchronously per segment.
4. Query nodes receive search or retrieve requests together with serialized expression plans and timestamps.
5. Search execution routes between:
   - sealed/index-backed segments
   - growing/in-memory segments
6. `SearchOnIndex.cpp` executes ANN search against the chosen vector index, applies bitset filtering, handles offset mappings, and returns top-k candidates.
7. Coordinator layers merge and rank results across segments.

## How Milvus Uses Indexes

Milvus treats indexes as segment-native artifacts:

- built asynchronously after data publication
- loaded or released with segment lifecycle
- used differently for sealed versus growing data
- vector search may use iterators or exact/top-k specific paths depending on search mode

Scalar and text filters are parsed and rewritten, but ANN/vector execution is the center of gravity.

## Consistency Model

Milvus is not MGA. Its important consistency ideas are:

- guarantee timestamps
- travel timestamps
- segment load state
- offset mapping between index-local and segment-local identifiers

This is valuable to ScratchBird for approximate or hybrid search families where publication and visibility may lag ingestion.

## What ScratchBird Should Borrow

- Asynchronous index build/publication contracts
- Clear distinction between sealed, indexed, and growing paths
- Bitset-filter plus ANN execution composition
- Coordinator-level merging of per-segment top-k results

## ScratchBird Comparison Hooks

- Compare ScratchBird vector and ANN family lifecycle to Milvus segment-native index publication.
- Compare any future hybrid scalar+vector execution path to Milvus bitset-plus-index execution rather than to SQL B-tree assumptions.
