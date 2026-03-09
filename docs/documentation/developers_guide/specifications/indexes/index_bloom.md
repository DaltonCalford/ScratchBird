# Specification: Bloom Filter Index

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

Bloom filter index provides space-efficient probabilistic membership testing. Used for range filtering and negative lookup optimization with zero false negatives and configurable false positive rate.

## Scope

### In Scope

- Standard Bloom filter structure
- Multi-hash functions (MurmurHash variants)
- Bit array storage
- FPP (False Positive Probability) calculation
- Range bloom filters (for range queries)

### Out of Scope

- Counting bloom filters (deletion support)
- Cuckoo filters
- XOR filters

## Background

Bloom filters:
- **Space**: ~10 bits per element for 1% FPP
- **False positives**: Can claim element exists when it doesn't
- **False negatives**: Never (if element was inserted)
- **No deletion**: Standard bloom filter (counting variant adds overhead)

Use cases:
- SSTable filtering in LSM-trees
- Block pruning in columnstores
- Join optimization (bloom join)

## Specification

### Index Type Identifier

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:673
enum class IndexType : uint8_t {
    BLOOM = 0x0F,     // Bloom filter range index
    // ... other types
};
```

### Bloom Filter Structure

```cpp
// Source: scratchbird/core/bloom_filter.h
struct BloomFilter {
    uint32_t bit_size;           // Size of bit array
    uint32_t num_hashes;         // Number of hash functions (k)
    uint32_t element_count;      // Elements added
    uint64_t seed;               // Hash seed
    
    // Bit array (bit_size bits)
    std::vector<uint8_t> bits;
};

// Configuration
struct BloomFilterConfig {
    uint64_t expected_elements;  // n
    double target_fpp;           // Target false positive rate
};
```

## Algorithms

### Algorithm: Optimal Parameters

```
Input:  expected_elements (n), target_fpp (p)
Output: bit_size (m), num_hashes (k)

m = -n * ln(p) / (ln(2)^2)
  = ceil(-n * ln(p) / 0.480453)

k = (m/n) * ln(2)
  = round(m / n * 0.693147)

Example: n=1M, p=0.01 (1% FPP)
  m = 9.6M bits = 1.2 MB
  k = 7 hash functions
```

### Algorithm: Add Element

```
Input:  bloom_filter, key
Output: Status

1. For i = 0 to k-1:
   a. hash = murmurhash3_64(key, seed=i)
   b. index = hash % m
   c. Set bit at index

2. element_count++
```

### Algorithm: May Contain

```
Input:  bloom_filter, key
Output: true (maybe) / false (definitely not)

1. For i = 0 to k-1:
   a. hash = murmurhash3_64(key, seed=i)
   b. index = hash % m
   c. If bit at index is 0:
      - Return false (definitely not present)

2. Return true (may be present)
```

## FPP Formulas

| Bits/Element | Hash Functions | FPP |
|--------------|----------------|-----|
| 6 | 4 | 5.6% |
| 8 | 5 | 2.2% |
| 10 | 7 | 1.0% |
| 12 | 8 | 0.5% |
| 16 | 11 | 0.1% |

## Related Specifications

- [index_lsm.md](./index_lsm.md) - SSTable bloom filters
- [index_hash.md](./index_hash.md) - Exact lookups

## References

- Bloom, B. H. (1970). Space/Time Trade-offs in Hash Coding.

## Changelog

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-03-08 | Initial specification |
