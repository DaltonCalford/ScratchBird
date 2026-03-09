# Specification: IVF_SQ8_HYBRID Index

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

IVF_SQ8_HYBRID combines IVF_SQ8 with a deterministic routing strategy that sends queries to specific inverted lists based on learned thresholds. Provides predictable performance with quality guarantees.

## Scope

### In Scope

- Hybrid routing strategy
- Query-dependent nprobe selection
- Performance-quality trade-off control

### Out of Scope

- Adaptive routing during search
- Machine learning-based routing

## Background

Hybrid routing:
- **Coarse distance threshold**: Pre-compute distance ranges
- **Query routing**: Based on query-to-centroid distances
- **Deterministic**: Same query always uses same lists

## Specification

### Index Type Identifier

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:680
enum class IndexType : uint8_t {
    IVF_SQ8_HYBRID = 0x16,    // IVF SQ8 with deterministic hybrid routing
    // ... other types
};
```

## Related Specifications

- [index_ivf_sq8.md](./index_ivf_sq8.md) - Base SQ8

## Changelog

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-03-08 | Initial specification |
