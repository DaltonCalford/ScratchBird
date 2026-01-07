# ScratchBird Compression Framework

## Overview

ScratchBird implements a pluggable compression framework that allows transparent page-level compression to reduce storage requirements and I/O. The framework is designed to be extensible, allowing multiple compression algorithms while maintaining backward compatibility.

## Architecture

### Key Components

1. **CompressionCodec** - Abstract interface for compression algorithms
2. **CompressionFactory** - Factory for creating codec instances
3. **CompressedPageManager** - Extends PageManager with compression support
4. **CompressedPageHeader** - Metadata for compressed pages

### Design Principles

- **Transparency**: Applications don't need to know if pages are compressed
- **Pluggability**: Easy to add new compression algorithms
- **Performance**: Minimal overhead, compression only when beneficial
- **Compatibility**: Compressed and uncompressed pages can coexist

## Implementation Details

### Compression Interface

```cpp
class CompressionCodec {
public:
    virtual CompressionType type() const = 0;
    virtual const char* name() const = 0;
    
    virtual Status compress(const uint8_t* src, uint32_t src_size,
                          uint8_t* dst, uint32_t dst_capacity,
                          uint32_t* compressed_size,
                          CompressionLevel level = CompressionLevel::DEFAULT) = 0;
    
    virtual Status decompress(const uint8_t* src, uint32_t src_size,
                            uint8_t* dst, uint32_t dst_capacity,
                            uint32_t* decompressed_size) = 0;
};
```

### Supported Algorithms

| Algorithm | Status | Compression Ratio | Speed | Use Case |
|-----------|--------|------------------|-------|----------|
| None | ✅ Implemented | 1.0 | N/A | No compression |
| LZ4 | ✅ Implemented | ~2-4x | Very Fast | Default, general purpose |
| Zstandard | 🔜 Planned | ~3-5x | Fast | Better ratio when needed |
| Snappy | 🔜 Planned | ~2-3x | Very Fast | Alternative to LZ4 |

### Compression Levels

- **FASTEST**: Optimize for speed over compression ratio
- **DEFAULT**: Balance between speed and compression
- **BEST**: Optimize for compression ratio over speed

## Page Compression Process

### Write Path

1. Check if page should be compressed (type, size, settings)
2. Compress page data using selected algorithm
3. Check if compression saved at least 10% space
4. If beneficial, write compressed page with headers
5. If not beneficial, write uncompressed page

### Read Path

1. Read page header and check PAGE_FLAG_COMPRESSED
2. If compressed, read compression header
3. Verify compression type matches expected
4. Decompress data into buffer
5. Validate decompressed page checksum

### Compression Rules

1. **System Pages**: Pages 0-2 are never compressed
2. **Minimum Benefit**: Only compress if saves ≥10% space
3. **Page Types**: Different page types have different policies
   - Heap pages: Compress if >50% full
   - B-tree leaves: Always try compression
   - B-tree internal: Usually not compressed
4. **Size Threshold**: Don't compress pages with <256 bytes of data

## Performance Characteristics

### LZ4 Compression (Baseline)

| Page Size | Compression Time | Decompression Time | Typical Ratio |
|-----------|-----------------|-------------------|---------------|
| 8 KB | ~5-10 μs | ~2-5 μs | 2-3x |
| 16 KB | ~10-20 μs | ~5-10 μs | 2-3x |
| 32 KB | ~20-40 μs | ~10-20 μs | 2-3x |
| 64 KB | ~40-80 μs | ~20-40 μs | 2-3x |
| 128 KB | ~80-160 μs | ~40-80 μs | 2-3x |

*Note: Actual performance depends on data compressibility and CPU*

### When Compression Helps

✅ **Good Cases**:
- Text-heavy data
- Repeated patterns
- Sparse data with lots of zeros
- Large page sizes with moderate fill factor

❌ **Poor Cases**:
- Already compressed data (images, etc.)
- High-entropy data (encrypted, random)
- Very small pages or low fill factor
- CPU-bound workloads

## Configuration

### Database Level

```cpp
// Create database with compression
CompressedPageManager pm(&db, page_size, CompressionType::LZ4);
```

### Future: Table Level

```sql
-- Future enhancement
CREATE TABLE users (...) WITH (compression = 'lz4');
CREATE TABLE logs (...) WITH (compression = 'zstd:3');  -- Level 3
CREATE TABLE images (...) WITH (compression = 'none');  -- Disable
```

## Statistics and Monitoring

The framework tracks compression statistics:

```cpp
struct CompressionStats {
    uint64_t bytes_in;           // Total uncompressed bytes
    uint64_t bytes_out;          // Total compressed bytes  
    uint64_t compress_time_us;   // Total compression time
    uint64_t decompress_time_us; // Total decompression time
    uint64_t compress_calls;     // Number of compressions
    uint64_t decompress_calls;   // Number of decompressions
};
```

### Key Metrics

- **Compression Ratio**: `bytes_out / bytes_in`
- **Space Saved**: `1 - compression_ratio`
- **Avg Compress Time**: `compress_time_us / compress_calls`
- **Avg Decompress Time**: `decompress_time_us / decompress_calls`

## Testing

### Unit Tests
- Codec functionality tests
- Compression/decompression correctness
- Performance benchmarks
- Edge cases and error handling

### Integration Tests
- Page manager integration
- All page sizes (8KB - 128KB)
- Mixed compressed/uncompressed pages
- Crash recovery with compressed pages

### Interoperability Tests
- Verify all page sizes work with compression
- Test page size migration with compressed pages
- Ensure backward compatibility

## Future Enhancements

1. **Additional Algorithms**
   - Zstandard for better compression ratios
   - Snappy as LZ4 alternative
   - Brotli for read-mostly data

2. **Adaptive Compression**
   - Monitor compression effectiveness
   - Automatically disable for incompressible data
   - Adjust compression level based on CPU availability

3. **Column-Level Compression**
   - Compress individual columns
   - Different algorithms per column
   - Better compression for columnar data

4. **Compression Dictionaries**
   - Shared dictionaries for better small-page compression
   - Table-specific dictionaries
   - Automatic dictionary training

## Best Practices

1. **Development**
   - Always handle both compressed and uncompressed pages
   - Test with compression disabled and enabled
   - Monitor compression statistics in production

2. **Deployment**
   - Start with LZ4 default level
   - Monitor CPU and I/O impact
   - Adjust based on workload characteristics

3. **Troubleshooting**
   - Check compression statistics for effectiveness
   - Verify CPU usage doesn't spike
   - Ensure enough memory for compression buffers

## Building with Compression

### Requirements
- C++17 compatible compiler
- LZ4 library (optional but recommended)
- CMake 3.20+

### Ubuntu/Debian
```bash
sudo apt-get install liblz4-dev
```

### macOS
```bash
brew install lz4
```

### Build Options
```bash
cmake -DBUILD_TESTING=ON ..  # Will auto-detect LZ4
```

If LZ4 is not available, compression support will be disabled but the code will still compile and run.