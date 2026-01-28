# Quadtree / Octree Index Specification for ScratchBird

**Version:** 1.0
**Date:** January 22, 2026
**Status:** Implementation Ready
**Author:** ScratchBird Architecture Team
**Target:** ScratchBird Beta (Optional Index Type)
**Features:** Page-size agnostic, MGA compliant, hierarchical spatial tree

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture Decision](#architecture-decision)
3. [Data Model](#data-model)
4. [On-Disk Structures](#on-disk-structures)
5. [Page Size Considerations](#page-size-considerations)
6. [MGA Compliance](#mga-compliance)
7. [Core API](#core-api)
8. [DML Integration](#dml-integration)
9. [Garbage Collection](#garbage-collection)
10. [Query Planner Integration](#query-planner-integration)
11. [DDL and Catalog](#ddl-and-catalog)
12. [Implementation Steps](#implementation-steps)
13. [Testing Requirements](#testing-requirements)
14. [Performance Targets](#performance-targets)
15. [Future Enhancements](#future-enhancements)

---

## Overview

### Purpose

Quadtree and Octree indexes partition space into hierarchical cells. Quadtrees are 2D; octrees are 3D. They are well suited for sparse spatial data, collision detection, and bounding-box intersection queries.

### Primary Use Cases

- Spatial intersection searches (points, boxes, polygons)
- Collision detection or tile-based lookup
- 3D object indexing (octree)

---

## Architecture Decision

### Design Choice

Implement a **region quadtree/octree** where nodes store a bounding box and either:

- child pointers (internal node)
- a list of entries (leaf node)

A leaf splits when it exceeds capacity or max depth is reached.

### Supported Geometries

- POINT, LINESTRING, POLYGON, GEOMETRY
- Bounding boxes are derived from geometry for index placement

---

## Data Model

### Node Types

- **Internal node:** has 4 (quadtree) or 8 (octree) children
- **Leaf node:** stores entries with bounding boxes and TIDs

### Split Policy

- Default: split when leaf count > node_capacity
- Stop at max_depth; store overflow list if needed

---

## On-Disk Structures

### Meta Page

```cpp
#pragma pack(push, 1)

enum SBTreeDim : uint8_t {
    TREE_DIM_2D = 2,
    TREE_DIM_3D = 3
};

struct SBQuadTreeMetaPage {
    PageHeader qt_header;

    uint8_t qt_index_uuid[16];
    uint8_t qt_table_uuid[16];

    uint8_t qt_dimensions;        // 2 or 3
    uint8_t qt_max_depth;         // 1..32
    uint16_t qt_node_capacity;    // leaf capacity

    double qt_min_bounds[3];      // min XYZ
    double qt_max_bounds[3];      // max XYZ

    uint32_t qt_root_page;        // root node page
    uint32_t qt_reserved1;

    uint64_t qt_total_nodes;
    uint64_t qt_total_entries;
    uint64_t qt_total_splits;
    uint64_t qt_reserved2;

    uint8_t qt_padding[];
} __attribute__((packed));

#pragma pack(pop)
```

### Node Page (Internal)

```cpp
#pragma pack(push, 1)

struct SBQuadNodeInternal {
    PageHeader qn_header;

    uint8_t qn_level;             // depth
    uint8_t qn_is_leaf;           // 0
    uint16_t qn_child_count;      // 4 or 8

    double qn_min_bounds[3];
    double qn_max_bounds[3];

    uint32_t qn_children[8];      // child page IDs (unused set to 0)

    uint8_t qn_padding[];
} __attribute__((packed));

#pragma pack(pop)
```

### Node Page (Leaf)

```cpp
#pragma pack(push, 1)

struct SBQuadLeafEntry {
    double min_bounds[3];
    double max_bounds[3];
    TID tid;
};

struct SBQuadNodeLeaf {
    PageHeader ql_header;

    uint8_t ql_level;
    uint8_t ql_is_leaf;           // 1
    uint16_t ql_entry_count;

    double ql_min_bounds[3];
    double ql_max_bounds[3];

    SBQuadLeafEntry ql_entries[];
} __attribute__((packed));

#pragma pack(pop)
```

---

## Page Size Considerations

- Leaf entry size: 6 doubles + tid = 56 bytes (2D uses only 4 doubles)
- 8K pages fit about 140 entries (2D) or 110 entries (3D)
- Larger pages improve fan-out and reduce splits

---

## MGA Compliance

- Entries store TID and use standard visibility rules
- Deletes create tombstones or removal entries per MGA rules
- Split operations are transactional; new nodes use transaction stamps

---

## Core API

```cpp
Status qt_insert(UUID index_uuid, const BoundingBox* box, TID tid);
Status qt_delete(UUID index_uuid, const BoundingBox* box, TID tid);
IndexScan qt_intersect_scan(UUID index_uuid, const BoundingBox* query);
```

---

## DML Integration

- INSERT: compute bounding box from geometry and insert
- UPDATE: if geometry changes, delete and reinsert
- DELETE: remove or tombstone entry

---

## Garbage Collection

Quadtree/Octree indexes implement `IndexGCInterface`:

TID references use `TID { gpid, slot }`. Legacy packed TID encodings are
not permitted in v2 on-disk formats.

- `removeDeadEntries()` scans leaf node entry lists and removes any entry
  whose TID is dead.
- After removal, leaf nodes that drop below the merge threshold are merged
  with siblings when possible.
- Internal node bounding boxes are recalculated as children shrink.

Concurrency:

- GC holds write locks on modified nodes; readers use shared locks.
- No movement of live TIDs; only removal and optional node merge.

See `INDEX_GC_PROTOCOL.md` for the shared GC contract.

---

## Query Planner Integration

- Supports `ST_Intersects`, `ST_Contains`, `ST_Within`, bbox overlaps
- Planner chooses quadtree when geometry predicate exists
- Exact predicate applied post-filter for precise geometry

---

## DDL and Catalog

### Syntax

```sql
CREATE INDEX idx_shapes_qt
ON shapes(geom)
USING quadtree
WITH (max_depth = 12, node_capacity = 128);

CREATE INDEX idx_cubes_ot
ON cubes(geom)
USING octree
WITH (max_depth = 10, node_capacity = 64);
```

### Catalog Additions

- `sys.indexes.index_type = 'QUADTREE'` or `'OCTREE'`
- `sys.indexes.index_options` stores bounds, depth, capacity

---

## Implementation Steps

1. Implement node allocation and bounding-box splitting.
2. Add insert/delete logic with split/merge rules.
3. Add intersection scan with pruning.
4. Add planner integration and geometry predicate mapping.
5. Add catalog persistence and stats.

---

## Testing Requirements

- Insert/update/delete tests for points and polygons
- Range intersection correctness vs brute force
- Split/merge behavior under heavy load
- MGA visibility with concurrent updates

---

## Performance Targets

- 5-20x faster intersection scans vs heap on sparse data
- Split rate under 2% of inserts for tuned node capacity

---

## Future Enhancements

- R-tree hybrid for dense datasets
- Bulk-load mode for large imports
- Adaptive split policy based on density
