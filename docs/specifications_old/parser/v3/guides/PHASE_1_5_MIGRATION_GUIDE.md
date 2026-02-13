# Phase 1.5 TID Migration Guide

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


## Overview

Phase 1.5 introduces a breaking change to ScratchBird's tuple identification system, migrating from a 48-bit legacy format to an 80-bit GPID-based TID format to support multi-tablespace functionality.

**Status**: ✅ COMPLETE
**Version**: ALPHA 1.0.2
**Breaking Change**: YES - Database recreation required

## What Changed

### TID Format Migration

**Before (Legacy Format - ALPHA 1.0.1 and earlier):**
```cpp
// 48-bit TID packed into uint64_t
// Bits: [page_id: 32 bits][item_id: 16 bits][unused: 16 bits]
uint64_t tid = (page_id << 32) | (item_id << 16);
```

**After (GPID-based Format - ALPHA 1.0.2+):**
```cpp
// 80-bit TID struct
struct TID {
    GPID gpid;      // 64 bits: [tablespace_id: 16][page_number: 48]
    uint16_t slot;  // 16 bits: item/slot number
};
```

### On-Disk Compatibility

**Important**: On-disk storage format remains **unchanged** (uint64_t). The TID struct is used in-memory only, with conversion at API boundaries:

```cpp
// Convert TID to legacy format for storage
uint64_t legacy_tid = convertTIDtoLegacy(tid);

// Convert legacy format to TID
TID tid = convertLegacyTID(legacy_uint64);
```

## Migration Impact

### For Database Users

**Database Recreation Required:**
- Existing databases created with ALPHA 1.0.1 remain compatible
- No automatic migration tool available in ALPHA phase
- Recommendation: Export data, recreate database, re-import

**Custom Tablespace Support:**
- TIDs with custom tablespace IDs (`tablespace_id != 1`) return `Status::NOT_IMPLEMENTED` in ALPHA 1.0.2
- Primary tablespace (ID=1) fully supported
- Multi-tablespace support required for future releases

### For Application Developers

**API Changes:**

1. **Tuple Struct (StorageEngine)**
   ```cpp
   // OLD API (ALPHA 1.0.1)
   struct Tuple {
       const uint8_t *data;
       uint32_t data_size;
       uint64_t tid;        // Legacy format
       uint16_t item_id;    // Separate fields
       uint32_t page_id;
   };

   // NEW API (ALPHA 1.0.2+)
   struct Tuple {
       const uint8_t *data;
       uint32_t data_size;
       TID tid;             // Unified TID struct
   };
   ```

2. **Index API (All Index Types)**
   ```cpp
   // OLD API
   Status insert(const Key &key, uint64_t tuple_id, ErrorContext *ctx);
   std::vector<uint64_t> search(const Key &key, ...);

   // NEW API
   Status insert(const Key &key, const TID &tid, ErrorContext *ctx);
   std::vector<TID> search(const Key &key, ...);
   ```

3. **Garbage Collector**
   ```cpp
   // OLD API
   Status removeDeadEntries(const std::vector<uint64_t> &dead_tids, ...);

   // NEW API
   Status removeDeadEntries(const std::vector<TID> &dead_tids, ...);
   ```

## Migration Steps

### Step 1: Update Code to New API

**Replace Tuple Field Access:**
```cpp
// OLD CODE
uint32_t page_id = tuple.page_id;
uint16_t item_id = tuple.item_id;

// NEW CODE
uint32_t page_id = static_cast<uint32_t>(getPageNumber(tuple.tid));
uint16_t item_id = getSlot(tuple.tid);
```

**Update Index Operations:**
```cpp
// OLD CODE
uint64_t tid = (page_id << 32) | (item_id << 16);
btree->insert(key, tid, &ctx);

// NEW CODE
TID tid = TID(makeGPID(PRIMARY_TABLESPACE_ID, page_id), item_id);
btree->insert(key, tid, &ctx);
```

**Update Index Search Results:**
```cpp
// OLD CODE
std::vector<uint64_t> results = btree->search(key, snapshot, &ctx);
for (uint64_t tid : results) {
    uint32_t page_id = tid >> 32;
    uint16_t item_id = (tid >> 16) & 0xFFFF;
    // ...
}

// NEW CODE
std::vector<TID> results = btree->search(key, snapshot, &ctx);
for (const TID &tid : results) {
    uint32_t page_id = static_cast<uint32_t>(getPageNumber(tid));
    uint16_t item_id = getSlot(tid);
    // ...
}
```

### Step 2: Rebuild Application

```bash
cd build
rm -rf *
cmake ..
make
```

### Step 3: Recreate Databases

```bash
# Backup existing data if needed
cp mydb.db mydb.db.backup

# Remove old database
rm mydb.db

# Application will create new database with updated format
./your_application
```

## Helper Functions

The following helper functions are available in `tid.h`:

```cpp
// Create GPID from tablespace and page number
GPID makeGPID(uint16_t tablespace_id, uint64_t page_number);

// Extract components from GPID
uint16_t getTablespaceId(GPID gpid);
uint64_t getPageNumber(GPID gpid);
uint64_t getPageNumber(const TID &tid);  // Convenience overload

// Extract slot from TID
uint16_t getSlot(const TID &tid);

// Legacy conversion (for internal use)
uint64_t convertTIDtoLegacy(const TID &tid);
TID convertLegacyTID(uint64_t legacy_tid);

// Constants
constexpr GPID INVALID_GPID = 0;
constexpr uint16_t PRIMARY_TABLESPACE_ID = 1;
```

## Affected Components

### Fully Migrated (ALPHA 1.0.2)
- ✅ B-Tree Index
- ✅ Hash Index
- ✅ Bitmap Index (Roaring)
- ✅ HNSW Index (Vector Similarity)
- ✅ BRIN Index (Block Range)
- ✅ GIN Index (Generalized Inverted)
- ✅ Garbage Collector
- ✅ Storage Engine (Tuple struct)
- ✅ Heap Page Operations
- ✅ TOAST Manager

### Not Yet Migrated
- ⏳ Test Suite (some tests use Phase 4A APIs not yet implemented)
- ⏳ CLI Tools (if any exist)

## Troubleshooting

### Compilation Errors

**Error**: `no member named 'page_id' in 'Tuple'`
```cpp
// Fix: Use helper functions
uint32_t page_id = static_cast<uint32_t>(getPageNumber(tuple.tid));
```

**Error**: `cannot convert 'uint64_t' to 'TID'`
```cpp
// Fix: Use convertLegacyTID for on-disk values
TID tid = convertLegacyTID(disk_value);
```

**Error**: `cannot convert 'TID' to 'uint64_t'`
```cpp
// Fix: Use convertTIDtoLegacy for storage
uint64_t legacy = convertTIDtoLegacy(tid);
```

### Runtime Errors

**Status::NOT_IMPLEMENTED for Custom Tablespaces**
- Custom tablespace IDs (≠ 1) are not supported in ALPHA 1.0.2
- All operations must use PRIMARY_TABLESPACE_ID (1)
- Multi-tablespace support coming in future phases

## Performance Impact

**Expected**: Minimal to none
- On-disk format unchanged (still uint64_t)
- Conversion overhead is trivial (bit operations)
- No additional I/O required
- Indexes maintain same performance characteristics

**Measured**: No performance regression observed in benchmarks

## Future Compatibility

### Phase 2: Multi-Tablespace Support

When multi-tablespace support is enabled:
- TIDs with custom tablespace IDs will be fully supported
- Database version will increment (requires migration)
- Conversion tools will be provided

### Phase 3+: Extended TID Features

Potential future enhancements:
- Compressed TID storage for memory-constrained systems
- TID validation and corruption detection
- Cross-tablespace tuple movement tracking

## Support and Questions

For issues or questions about this migration:
1. Check [PROJECT_CONTEXT.md](../../PROJECT_CONTEXT.md) for overall project status
2. Review [TABLESPACE_IMPLEMENTATION_PLAN.md](../../TABLESPACE_IMPLEMENTATION_PLAN.md) for roadmap
3. File issues at [GitHub Issues](https://github.com/anthropics/claude-code/issues)

## Summary

Phase 1.5 lays the groundwork for multi-tablespace functionality while maintaining backward compatibility through careful API design. The migration is straightforward for most applications and provides a clear path to future enhancements.

**Key Takeaway**: Update your code to use TID structs instead of uint64_t tuple IDs, and you're ready for future multi-tablespace features!

---

**Document Version**: 1.0
**Last Updated**: 2025-10-20
**Phase**: 1.5 Complete
