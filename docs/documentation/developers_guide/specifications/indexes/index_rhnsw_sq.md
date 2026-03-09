# Specification: RHNSW_SQ Index (HNSW with SQ)

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

RHNSW_SQ combines HNSW graph navigation with Scalar Quantization. Uses 8-bit quantized vectors for compact storage in graph nodes, providing a balance between memory efficiency and search accuracy.

## Scope

### In Scope

- HNSW graph with SQ compression
- Per-dimension min/max quantization
- Fast dequantization for distance computation

### Out of Scope

- Product quantization
- Learned quantization schemes

## Specification

### Index Type Identifier

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:682
enum class IndexType : uint8_t {
    RHNSW_SQ = 0x18,          // HNSW with SQ payload variant
    // ... other types
};
```

## Related Specifications

- [index_hnsw.md](./index_hnsw.md) - Base HNSW
- [index_ivf_sq8.md](./index_ivf_sq8.md) - SQ8 details

## Changelog

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-03-08 | Initial specification |
