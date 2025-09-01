# Phase 4: Heap Storage Implementation

## Objective
Implement heap pages for storing table rows.

## Prerequisites
- Phase 3 complete (page management)

## Technical Specifications
- **Page Management**: See `/references/technical_specifications/STORAGE_ENGINE_PAGE_MANAGEMENT.md`
- **Storage Main**: See `/references/technical_specifications/STORAGE_ENGINE_MAIN.md`

## Tasks

### 4.1 Heap Page Structure
Define in `include/scratchbird/engine/heap.h`:
```cpp
struct HeapPageHeader {
    uint16_t num_slots;
    uint16_t free_start;
    uint16_t dir_start;
    uint16_t flags;
};
```

Layout: `[PageHeader][HeapPageHeader][tuples][free space][slot directory]`

### 4.2 Tuple Format
```cpp
struct TupleHeader {
    uint64_t created_xid;
    uint64_t deleted_xid;
    uint16_t num_attrs;
    uint16_t nullmap_bytes;
    uint16_t flags;
};
```

### 4.3 Row Operations
- `insert_tuple(page_no, data) -> slot_no`
- `fetch_tuple(page_no, slot_no) -> data`
- `delete_tuple(page_no, slot_no)`
- `update_tuple(page_no, slot_no, data)`

### 4.4 Free Space Management
- Track free space per page
- Find page with sufficient space
- Compact page when fragmented

### 4.5 Null Bitmap Handling
- Encode null attributes efficiently
- Support up to 64 attributes initially

## Files to Create/Modify
- `include/scratchbird/engine/heap.h`
- `src/engine/heap.cpp`

## Validation Tests
```cpp
// Insert and fetch tuple
auto slot = insert_tuple(page, {"col1", "col2"});
auto data = fetch_tuple(page, slot);
assert(data == vector{"col1", "col2"});

// Null handling
auto slot2 = insert_tuple(page, {"val", nullptr});
auto data2 = fetch_tuple(page, slot2);
assert(data2[1] == nullptr);
```

## Exit Criteria
- Tuples stored and retrieved correctly
- Null values handled properly
- Free space tracked accurately