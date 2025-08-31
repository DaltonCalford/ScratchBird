# Phase 5: Space Allocation and Management

## Objective
Implement page allocation with PIP (Page Inventory Pages).

## Prerequisites
- Phase 4 complete (heap storage)

## Tasks

### 5.1 PIP Structure
Page Inventory Page tracks allocated/free pages:
- Bitmap: 1 bit per page (0=free, 1=allocated)
- Coverage: Calculate pages per PIP based on page size
- Chain: Link multiple PIPs for large databases

### 5.2 Allocator Implementation
```cpp
class Allocator {
    uint32_t allocate_page();
    void free_page(uint32_t page_no);
    vector<uint32_t> allocate_extent(size_t pages);
    void free_extent(uint32_t first_page);
};
```

### 5.3 Multi-Segment Support
- Segment files: `database.seg0`, `database.seg1`, etc.
- Growth: Create new segment when current full
- Mapping: Page number to segment/offset

### 5.4 Space Reclamation
- Mark pages free on DELETE/DROP
- Reuse freed pages before growing file
- Defragmentation not required in this phase

## Files to Create/Modify
- `include/scratchbird/engine/alloc.h`
- `src/engine/alloc.cpp`

## Validation Tests
```cpp
// Allocate and free pages
auto page1 = allocator.allocate_page();
auto page2 = allocator.allocate_page();
assert(page2 > page1);

allocator.free_page(page1);
auto page3 = allocator.allocate_page();
assert(page3 == page1);  // Reused

// Extent allocation
auto extent = allocator.allocate_extent(8);
assert(extent.size() == 8);
for(size_t i = 1; i < 8; i++) {
    assert(extent[i] == extent[i-1] + 1);
}
```

## Exit Criteria
- Pages allocated deterministically
- Freed pages reused
- Multi-segment files created as needed