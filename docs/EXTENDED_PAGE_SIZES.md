# Extended Page Sizes - Stage 1.1 Implementation

## Overview
As part of Alpha Stage 1.1, ScratchBird now supports extended page sizes of 64KB and 128KB in addition to the original 8KB, 16KB, and 32KB sizes. This enhancement allows for more efficient storage of large tuples and better I/O performance for certain workloads.

## Changes Made

### 1. Page Size Validation
Updated `is_valid_alpha_page_size()` in `ondisk.h` to accept 65536 (64KB) and 131072 (128KB) as valid page sizes.

### 2. Structure Updates for Large Page Support
To support pages larger than 64KB, the following structures were updated to use 32-bit offsets:

#### ItemPointer Structure
```c++
// Before (16-bit, max 64KB addressable)
struct ItemPointer {
    uint16_t offset;     // Offset from start of page
    uint16_t length : 15; // Length of tuple (max ~32KB)
    uint16_t flags : 1;   // 0 = normal, 1 = deleted
};

// After (32-bit, max 4GB addressable)
struct ItemPointer {
    uint32_t offset;     // Offset from start of page (supports up to 4GB pages)
    uint32_t length : 31; // Length of tuple (max ~2GB)
    uint32_t flags : 1;   // 0 = normal, 1 = deleted
};
```

#### HeapPageSpecial Structure
```c++
// Before (16-bit offsets)
struct HeapPageSpecial {
    uint16_t pd_flags;      // Page flags
    uint16_t pd_lower;      // Offset to start of free space
    uint16_t pd_upper;      // Offset to end of free space
    uint16_t pd_special;    // Offset to start of special area
    uint64_t pd_prune_xid;  // Oldest XID for pruning
};

// After (32-bit offsets)
struct HeapPageSpecial {
    uint16_t pd_flags;      // Page flags
    uint16_t reserved;      // Reserved for alignment
    uint32_t pd_lower;      // Offset to start of free space (supports up to 4GB pages)
    uint32_t pd_upper;      // Offset to end of free space
    uint32_t pd_special;    // Offset to start of special area
    uint64_t pd_prune_xid;  // Oldest XID for pruning
};
```

### 3. User Interface Updates
- Updated command-line validation in `main.cpp` to accept new page sizes
- Error messages now include all valid page sizes

### 4. Testing
Created comprehensive test suite (`test_extended_page_sizes.cpp`) that covers:
- Database creation with all 5 page sizes
- Buffer pool operations with large pages
- Heap page operations with 64KB and 128KB pages
- Maximum tuple size validation for large pages
- Free Space Map (FSM) functionality with large pages
- Performance comparison across page sizes

## Performance Considerations

### Advantages of Larger Page Sizes
1. **Reduced overhead**: Fewer page headers for the same amount of data
2. **Better I/O efficiency**: Larger sequential reads/writes
3. **Larger tuples**: Can store tuples up to ~64KB (in 64KB pages) or ~128KB (in 128KB pages)
4. **Fewer FSM entries**: Less metadata to track free space

### Disadvantages of Larger Page Sizes
1. **Memory usage**: Buffer pool requires more memory per page
2. **Write amplification**: Modifying small tuples requires writing larger pages
3. **Internal fragmentation**: More wasted space for small tuples
4. **Lock granularity**: Page-level locks affect more data

## Recommended Use Cases

- **8KB pages**: General purpose, OLTP workloads with small tuples
- **16KB pages**: Mixed workloads, moderate tuple sizes
- **32KB pages**: Analytical workloads, larger tuples
- **64KB pages**: Data warehousing, large text/binary data
- **128KB pages**: Specialized use cases with very large tuples

## Backward Compatibility
The changes maintain full backward compatibility with existing databases using 8KB, 16KB, or 32KB page sizes. The structure size increases only affect newly created databases with 64KB or 128KB pages.

## Future Enhancements
1. Dynamic page size selection based on table characteristics
2. Page compression to reduce I/O for large pages
3. TOAST support for tuples larger than a single page
4. Adaptive buffer pool management for mixed page sizes

## Testing the Feature
```bash
# Create databases with different page sizes
scratchbird create database test_8k.db --page-size=8192
scratchbird create database test_16k.db --page-size=16384
scratchbird create database test_32k.db --page-size=32768
scratchbird create database test_64k.db --page-size=65536
scratchbird create database test_128k.db --page-size=131072

# Run tests
./tests/scratchbird_tests --gtest_filter="ExtendedPageSizesTest.*"
```