Status: canonical_beta2_immediate_implementation

# ClickHouse TokenBF and Sparse Grams Specification

## Purpose

Define the `CLICKHOUSE_TOKENBF_V1` and `CLICKHOUSE_SPARSE_GRAMS` families
required for ClickHouse text-filter emulation.

## Donor Basis

The donor shape is grounded in:

- `MergeTreeIndices.cpp` registration of `tokenbf_v1` and `sparse_grams`
- `MergeTreeIndexBloomFilterText.h`
- `MergeTreeIndexBloomFilterText.cpp`

Those donor files prove:

- one Bloom filter is stored per indexed column per granule
- token extraction mode is part of the family contract
- the family is used for filter pruning, not ranked retrieval
- supported predicates include token and string containment style probes

## Canonical Identity

### `CLICKHOUSE_TOKENBF_V1`

- donor engines supported:
  - `ClickHouse`
- physical family:
  - `TOKEN_BLOOM_TEXT`
- planner family:
  - `FILTER_ONLY`
- metrics type:
  - `TEXT_SEARCH`
- family mode:
  - `CLICKHOUSE_TOKENBF_V1`

### `CLICKHOUSE_SPARSE_GRAMS`

- donor engines supported:
  - `ClickHouse`
- physical family:
  - `TOKEN_BLOOM_TEXT`
- planner family:
  - `FILTER_ONLY`
- metrics type:
  - `TEXT_SEARCH`
- family mode:
  - `CLICKHOUSE_SPARSE_GRAMS`

## Required DDL Surface

Canonical donor-compatible forms:

```sql
CREATE INDEX idx_name ON t (expr) USING CLICKHOUSE_TOKENBF_V1(bytes, hashes, seed);
CREATE INDEX idx_name ON t (expr) USING CLICKHOUSE_SPARSE_GRAMS(
    gram_size,
    window_size,
    bytes,
    hashes,
    seed
);
```

Required option rules:

- `CLICKHOUSE_TOKENBF_V1` requires exactly 3 unsigned integer arguments
- `CLICKHOUSE_SPARSE_GRAMS` requires 5 or 6 unsigned integer arguments
- only `String`, `FixedString`, low-cardinality string, IPv6, or arrays of
  those values may be indexed

## Runtime Model

For each index granule:

1. evaluate the indexed expression
2. normalize to supported string-like inputs
3. run the configured token extractor
4. add extracted tokens or sparse grams into the per-column Bloom filter
5. persist the Bloom filter payload

The family is filter-only:

- it may eliminate granules
- it never provides ordered output
- it never provides exact result truth
- heap or row recheck remains required

## Metrics Contract

The native payload must include:

- `named_family`
- `tokenizer_mode`
- `filter_size_bytes`
- `hash_function_count`
- `hash_seed`
- `avg_tokens_per_row`
- `avg_tokens_per_granule`
- `false_positive_ratio_est`
- `granule_elimination_gain_est`
- `extractor_cpu_cost_est`
- `overflow_or_saturation_fraction`

Additional required fields for `CLICKHOUSE_SPARSE_GRAMS`:

- `gram_size`
- `window_size`
- `avg_sparse_grams_per_value`

## Optimizer Rules

1. These families must be enumerated for token or string-filter predicates they
   can legally serve.
2. Candidate ranking must prefer them over heap-only scans when:
   - granule elimination gain is material
   - false-positive ratio remains bounded
3. They must lose to ranked or posting-list text indexes when the query demands:
   - ranking
   - phrase proximity
   - exact token frequency scoring

## Required Pseudocode

```cpp
bool may_granule_match(const BloomGranule& g, const TextPredicate& p) {
    BloomProbe probe = tokenize_for_probe(p, g.tokenizer_mode);
    if (!g.filter.may_contain(probe)) {
        return false;
    }
    return true;
}
```

## Required Refusal Rules

Create must fail if:

- the argument count does not match the admitted donor family
- any argument is not an unsigned integer
- the key type is not one of the admitted string-like forms
- the tokenizer mode is not the one required by the named family

## First-Class Rule

These families are not advisory Bloom hints. They are primary optimizer
candidates for filter pruning and must publish planner packets like every other
supported family.
