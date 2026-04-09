Status: canonical_beta2_immediate_implementation

# ClickHouse Text Index Specification

## Purpose

Define the `CLICKHOUSE_TEXT` family required for ClickHouse text-index
emulation.

## Donor Basis

The donor shape is grounded in:

- `MergeTreeIndices.cpp` registration of `text`
- `MergeTreeIndexText.h`
- `MergeTreeIndexText.cpp`

Those donor files prove:

- the index uses three persisted streams:
  - sparse index stream
  - dictionary stream
  - postings stream
- posting lists may be raw, embedded, block-split, or roaring-compressed
- dictionary blocks may use front coding
- the family is mergeable and does not require full rebuild on every part merge

## Canonical Identity

- admitted named family:
  - `CLICKHOUSE_TEXT`
- donor engines supported:
  - `ClickHouse`
- physical family:
  - `CLICKHOUSE_TEXT`
- planner family:
  - `INVERTED_RANKED`
- metrics type:
  - `TEXT_SEARCH`
- lifecycle model:
  - immutable segment generation with mergeable posting structures

## Required DDL Surface

Canonical donor-compatible form:

```sql
CREATE INDEX idx_name ON t (expr) USING CLICKHOUSE_TEXT(
    tokenizer = ...,
    dictionary_block_size = ...,
    dictionary_block_frontcoding_compression = ...,
    posting_list_block_size = ...,
    posting_list_codec = ...
);
```

Required rules:

- the index must be created on exactly one column or one normalized expression
- `dictionary_block_size > 0`
- `dictionary_block_frontcoding_compression` is `0` or `1`
- `posting_list_block_size > 0`
- the tokenizer must resolve through the family tokenizer registry

## Persisted Layout

`CLICKHOUSE_TEXT` persists three logical structures:

1. sparse dictionary index
   - first token in each dictionary block
   - offset to dictionary block
2. dictionary blocks
   - token encoding mode
   - token list
   - posting-list headers
   - embedded postings for very small lists
3. posting-list blocks
   - raw or roaring-compressed postings
   - optional codec-compressed blocks
   - min/max row-range metadata per posting block

## Search Model

The family supports:

- token containment
- conjunction
- disjunction
- phrase or positional query lowering when the tokenizer and preprocessor prove
  compatibility
- ranked retrieval using posting statistics

The planner must:

- treat this as stronger than Bloom-style token filters
- keep heap visibility and security recheck as final truth
- preserve donor-visible plan labels for ClickHouse overlay modes

## Metrics Contract

The native payload must include:

- `named_family = "CLICKHOUSE_TEXT"`
- `dictionary_block_count`
- `avg_tokens_per_dictionary_block`
- `front_coding_enabled`
- `front_coding_gain_est`
- `posting_block_count`
- `avg_postings_cardinality`
- `embedded_posting_fraction`
- `roaring_posting_fraction`
- `rare_token_cache_hit_ratio`
- `mergeable_segment_count`
- `token_df_skew`
- `posting_scan_cpu_cost_est`
- `ranked_retrieval_gain_est`

## Build and Merge Flow

1. Scan visible rows and tokenize values.
2. Accumulate posting lists per token.
3. Sort tokens and split into dictionary blocks.
4. Persist sparse index entries for the first token in each block.
5. Persist posting lists as embedded, raw, or roaring-compressed blocks.
6. On part or generation merge:
   - merge sparse indexes
   - merge dictionary blocks
   - merge posting lists without full document re-tokenization when legal

## Required Pseudocode

```cpp
for (Token token : tokenize(document_value)) {
    postings[token].add(row_ordinal);
}
write_sparse_dictionary(postings);
write_dictionary_blocks(postings);
write_posting_blocks(postings);
```

## Refusal Rules

Create must fail if:

- more than one key expression is provided
- any required text option is absent or invalid
- the tokenizer or posting codec cannot be resolved
- the key datatype cannot be normalized into the text tokenization path

## First-Class Rule

`CLICKHOUSE_TEXT` is a full text-search family, not a filter hint. It must
participate in ranked candidate formation and publish family-native metrics.
