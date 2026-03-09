# Specification: IVF_SQ8 Index (Scalar Quantization)

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | storage/indexes |
| **Spec Version** | 1.0.0 |
| **Status** | 🟡 Review |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird v3.0 |
| **Authors** | ScratchBird Development Team |

## Synopsis

IVF_SQ8 uses 8-bit scalar quantization to compress float32 vectors to 1/4 size. Each dimension is independently quantized using min/max scaling, providing 4x memory reduction with minimal accuracy loss.

## Scope

### In Scope

- Per-dimension scalar quantization
- Min/max range calculation
- 8-bit uniform quantization
- Dequantization for distance computation

### Out of Scope

- Non-uniform quantization
- Learned quantizers
- Product quantization (see IVF_PQ)

## Background

Scalar Quantization (SQ8):
- **Range**: Compute min/max per dimension
- **Quantize**: (value - min) / range * 255
- **Storage**: 1 byte per dimension
- **Dequantize**: code / 255 * range + min

Advantages:
- Simple and fast
- 4x memory reduction
- SIMD-friendly dequantization

## Specification

### Index Type Identifier

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:679
enum class IndexType : uint8_t {
    IVF_SQ8 = 0x15,           // IVF scalar quantization variant
    // ... other types
};
```

### SQ8 Structure

```cpp
struct IVFSQ8Index {
    uint32_t dimension;
    uint32_t nlist;
    
    // Scalar quantizer parameters (per dimension)
    std::vector<float> min_values;  // dimension floats
    std::vector<float> max_values;  // dimension floats
    std::vector<float> ranges;      // max - min per dimension
    
    // Compressed vectors: 1 byte per dimension
    struct InvertedList {
        uint64_t code_count;
        std::vector<uint8_t> codes;  // dimension * count bytes
    };
    std::vector<InvertedList> lists;
};
```

## Algorithms

### Algorithm: Compute Ranges

```
Input:  Training vectors
Output: min/max per dimension

For each dimension d:
  min[d] = minimum value across all vectors
  max[d] = maximum value across all vectors
  range[d] = max[d] - min[d]
```

### Algorithm: Quantize

```
Input:  vector x, ranges
Output: quantized codes

For each dimension i:
  If range[i] == 0:
    code[i] = 0
  Else:
    code[i] = round((x[i] - min[i]) / range[i] * 255)
```

### Algorithm: Dequantize

```
Input:  codes
Output: approximate vector

For each dimension i:
  x_approx[i] = codes[i] / 255.0 * range[i] + min[i]
```

## Related Specifications

- [index_ivf_pq.md](./index_ivf_pq.md) - Higher compression PQ
- [index_ivf_sq8_hybrid.md](./index_ivf_sq8_hybrid.md) - Hybrid routing

## Changelog

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-03-08 | Initial specification |
