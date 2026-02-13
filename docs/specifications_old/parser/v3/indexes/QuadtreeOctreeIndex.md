# Quadtree / Octree Index Specification for ScratchBird


**Authoritative MGA/Lock/GC References:**
- [TRANSACTION_MGA_CORE.md](../transaction/TRANSACTION_MGA_CORE.md)
- [TRANSACTION_LOCK_MANAGER.md](../transaction/TRANSACTION_LOCK_MANAGER.md)
- [MGA_IMPLEMENTATION.md](../storage/MGA_IMPLEMENTATION.md)
- [FIREBIRD_GC_SWEEP_GLOSSARY.md](../transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md)
- [FIREBIRD_CONSTANTS_REFERENCE.md](../transaction/FIREBIRD_CONSTANTS_REFERENCE.md)


Status: Authoritative (V3)
Last Updated: 2026-02-08
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
15. [Reserved Enhancements (Not Supported in V3)](#reserved-enhancements-not-supported-in-v3)

---

## Overview

### Purpose

Quadtree and Octree indexes partition space into hierarchical cells. Quadtrees are 2D; octrees are 3D. They are well suited for sparse spatial data, collision detection, and bounding-box intersection queries.

### Primary Use Cases

- Spatial intersection searches (points, boxes, polygons)
- Collision detection or tile-based lookup
- 3D object indexing (octree)

---

## Authoritative Algorithm (Normative, 2026-02-07)

This section is the implementation source of truth. If any other section in
this document conflicts with the steps below, this section wins.

### Node Layout

- Each node stores a bounding box `(min, max)` and either:
  - child pointers (internal), or  
  - a list of `(bbox, tid)` entries (leaf).  

### Insert

1. Compute the geometry bounding box.  
2. Descend from root:
   - If current node is leaf and capacity not exceeded, insert entry.  
   - If capacity exceeded and depth < max, split into 4 (quadtree) or 8 (octree).  
3. On split:
   - Partition entries into child nodes based on bbox overlap.  
   - If an entry overlaps multiple children, store at current node or duplicate (choose policy and enforce).  

### Query (Range/Intersection)

1. Given query bbox `Q`, start at root.  
2. If node bbox does not intersect `Q`, prune.  
3. If leaf, test all entries for intersection.  
4. If internal, recurse into children that intersect `Q`.

### Delete

1. Locate entry by bbox and tid.  
2. Remove from leaf.  
3. If total entries across children fall below merge threshold, collapse children into parent.

### MGA / Versioning

- Entries are versioned like any other index.  
- Visibility filtered on read; GC removes obsolete versions.

### Complexity Targets

- Insert/query: `O(log N)` average, worst‑case `O(N)` for pathological distributions.  

### References (for algorithmic definitions)

- Quadtree and Octree algorithm definitions (standard spatial indexing).  

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
- **Leaf node:** stores entries with bounding boxes and record UUIDs

### Split Policy

- Default: split when leaf count > node_capacity
- Stop at max_depth; store overflow list if needed

---

## On-Disk Structures

**Storage Layout Authority:** On-disk page headers, slot arrays, free-space rules, and page-type layouts are authoritative in `../storage/PAGE_TYPES_AND_LAYOUTS.md`. Any structs here are logical field groupings; do not infer byte-accurate layout from this file.

### Meta Page


**Logical Fields:**

- `qt_header` (PageHeader)
- `qt_index_uuid[16]` (uint8_t)
- `qt_table_uuid[16]` (uint8_t)
- `qt_dimensions` (uint8_t): 2 or 3
- `qt_max_depth` (uint8_t): 1..32
- `qt_node_capacity` (uint16_t): leaf capacity
- `qt_min_bounds[3]` (double): min XYZ
- `qt_max_bounds[3]` (double): max XYZ
- `qt_root_page` (uint32_t): root node page
- `qt_reserved1` (uint32_t)
- `qt_total_nodes` (uint64_t)
- `qt_total_entries` (uint64_t)
- `qt_total_splits` (uint64_t)
- `qt_reserved2` (uint64_t)
- `qt_padding[]` (uint8_t)


### Node Page (Internal)


**Logical Fields:**

- `qn_header` (PageHeader)
- `qn_level` (uint8_t): depth
- `qn_is_leaf` (uint8_t): 0
- `qn_child_count` (uint16_t): 4 or 8
- `qn_min_bounds[3]` (double)
- `qn_max_bounds[3]` (double)
- `qn_children[8]` (uint32_t): child page IDs (unused set to 0)
- `qn_padding[]` (uint8_t)


### Node Page (Leaf)


**Logical Fields:**

- `min_bounds[3]` (double)
- `max_bounds[3]` (double)
- `record_uuid` (UUID)
- `ql_header` (PageHeader)
- `ql_level` (uint8_t)
- `ql_is_leaf` (uint8_t): 1
- `ql_entry_count` (uint16_t)
- `ql_min_bounds[3]` (double)
- `ql_max_bounds[3]` (double)
- `ql_entries[]` (SBQuadLeafEntry)


---

## Page Size Considerations

- Leaf entry size: 6 doubles + tid = 56 bytes (2D uses only 4 doubles)
- 8K pages fit about 140 entries (2D) or 110 entries (3D)
- Larger pages improve fan-out and reduce splits

---

## MGA Compliance

- Entries store record UUIDs and use MGA visibility rules
- Deletes create tombstones or removal entries per MGA rules
- Split operations are transactional; new nodes use transaction stamps

---

## Core API

```cpp
Status qt_insert(UUID index_uuid, const BoundingBox* box, UUID record_uuid);
Status qt_delete(UUID index_uuid, const BoundingBox* box, UUID record_uuid);
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

Record locators use `record_uuid` with optional `SBRecordPtr` cache hints. Legacy packed TIDs are
not permitted in v2 on-disk formats.

- `removeDeadEntries()` scans leaf node entry lists and removes any entry
  whose `record_uuid` is dead.
- After removal, leaf nodes that drop below the merge threshold are merged
  with siblings when possible.
- Internal node bounding boxes are recalculated as children shrink.

Concurrency:

- GC holds write locks on modified nodes; readers use shared locks.
- No movement of live entries; only removal and optional node merge.

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

## Reserved Enhancements (Not Supported in V3)

- R-tree hybrid for dense datasets
- Bulk-load mode for large imports
- Adaptive split policy based on density

## See Also

- `INDEX_IMPLEMENTATION_REFERENCE.md` — authoritative algorithm map (includes per-index specs).
- `INDEX_IMPLEMENTATION_SPEC.md` — global MGA/UUID requirements.
- `INDEX_GC_PROTOCOL.md` — index GC contract.
