### Task: Storage documentation authoring

Goal: Author storage docs with full Implementation References and Spec Trace.

Input:
- Heap: `include/scratchbird/engine/heap.h`, `src/engine/heap.cpp`
- ODS: `include/scratchbird/engine/ods.h`, `src/engine/ods.cpp`
- Allocator/Space: `include/scratchbird/engine/alloc.h`, `src/engine/alloc.cpp`
- ProjectPlan Phase 1/2

Steps:
1. Create pages under `subprojects/storage/` using templates.
2. Link REQ-CORE-HEAP-* and REQ-CORE-SPACE-* items in Spec Trace.
3. Insert Implementation References via anchors for key structs/functions.
4. Add a lifecycle page for heap relation and pages.

Output:
- Updated `subprojects/storage/index.md` and lifecycle docs.

Validation:
- Each subsection contains at least two precise code anchors (header + source).
