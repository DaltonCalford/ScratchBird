# Index Page Base Layout

Status: current_authority

## 1. Scope and authority

This document defines the current shared and family-local base contract for
index pages.

The controlling binary authorities are:

- `ScratchBird/include/scratchbird/core/ondisk.h`
- current concrete family headers such as
  `ScratchBird/include/scratchbird/core/btree.h`

There is no current universal `IndexPageHeader` beyond the shared `PageHeader`.

## 2. Shared cross-family contract

The only current engine-wide shared index-page requirements are:

1. the page begins with `PageHeader`
2. `page_type` is a legal index-family `PageType`
3. shared routing dispatches by `PageType`
4. post-dispatch parsing is family-local

No implementation may infer one family's opaque fields into another family.

## 3. Concrete family authority: `SBBTreePage`

The concrete B-tree page layout is currently the clearest compiled example of a
real ScratchBird index-family header.

### Binary layout

| Field | Type | Meaning |
| --- | --- | --- |
| `btr_header` | `PageHeader` | shared page header |
| `btr_index_uuid` | `ID` | owning index UUID |
| `btr_table_uuid` | `ID` | owning table UUID |
| `btr_level` | `uint16_t` | tree level, `0` = leaf |
| `btr_flags` | `uint16_t` | `BTreeFlags` bitset |
| `btr_count` | `uint16_t` | number of entries |
| `btr_free_space` | `uint16_t` | free bytes |
| `btr_left_sibling` | `uint64_t` | left sibling page |
| `btr_right_sibling` | `uint64_t` | right sibling page |
| `btr_parent_page` | `uint64_t` | parent page |
| `btr_rightmost_child` | `uint64_t` | rightmost child page for internal nodes |
| `btr_prefix_total` | `uint16_t` | total prefix-compression bytes saved |
| `btr_suffix_total` | `uint16_t` | total suffix-truncation bytes saved |
| `btr_compression` | `uint8_t` | `BTreeCompressionType` |
| `btr_min_prefix_len` | `uint8_t` | minimum prefix length on page |
| `btr_xmin` | `uint64_t` | page creation transaction |
| `btr_xmax` | `uint64_t` | page deletion transaction or `0` |
| `btr_lsn` | `uint64_t` | legacy compatibility slot, not WAL authority |
| `btr_high_water` | `uint16_t` | highest used offset in page |

### Hard compiled size rule

`sizeof(SBBTreePage) == sizeof(PageHeader) + 104`

That size rule is B-tree truth only.
It is not cross-family truth.

## 4. Concrete family authority: `SBBTreeNode`

The compiled B-tree node header is:

| Field | Type | Meaning |
| --- | --- | --- |
| `btn_flags` | `uint16_t` | `BTreeNodeFlags` |
| `btn_prefix_len` | `uint16_t` | prefix-compression length |
| `btn_suffix_trunc` | `uint16_t` | suffix-truncation length |
| `btn_key_len` | `uint16_t` | key length after compression |
| `btn_tuple_count` | `uint32_t` | tuple count for duplicates |
| `btn_child_page` | `uint64_t` | child page for internal-node semantics |
| `btn_xmin` | `uint64_t` | node creation transaction |
| `btn_xmax` | `uint64_t` | node deletion transaction |

Compiled size rule:

- `sizeof(SBBTreeNode) == 36`

## 5. B-tree family vocabularies

### `BTreeFlags`

- `LEAF = 0x0001`
- `ROOT = 0x0002`
- `RIGHTMOST = 0x0004`
- `LEFTMOST = 0x0008`
- `COMPRESSED = 0x0010`
- `ENCRYPTED = 0x0020`
- `HAS_GARBAGE = 0x0040`
- `INCOMPLETE = 0x0080`

### `BTreeCompressionType`

- `NONE = 0`
- `PREFIX = 1`
- `SUFFIX = 2`
- `BOTH = 3`
- `ZSTD = 4`
- `ADAPTIVE = 5`

### `BTreeNodeFlags`

- `DELETED = 0x0001`
- `HAS_DUPLICATES = 0x0002`
- `FIRST_ON_PAGE = 0x0004`
- `LAST_ON_PAGE = 0x0008`
- `NULL_KEY = 0x0010`
- `INFINITY_KEY = 0x0020`

## 6. Family-local ownership boundary

The following remain family-local unless a compiled family struct proves
otherwise:

1. root-page markers
2. sibling links
3. tree level or depth markers
4. opaque trailer layout
5. split, merge, and maintenance metadata
6. posting-list, prefix-compression, or summary-region structure

That means a limited implementer must not project the B-tree header into hash,
GIN, GiST, BRIN, bitmap, inverted, vector, or other families.

## 7. Validation algorithm

Index-page validation is a two-stage process:

1. shared routing
   - validate `PageHeader`
   - validate `page_type` belongs to an index family
   - dispatch to the concrete family parser
2. family-local validation
   - validate the concrete family struct
   - validate family-local flags and fields
   - validate family-local relationships such as siblings, levels, child
     pointers, or compression metadata

It is always an error to parse one family as another.

## 8. WAL boundary

Some family structs still contain legacy names such as `btr_lsn`.
Those fields are compatibility slots in the current compiled layout.
They are not authorization to introduce WAL-owned correctness or replay
semantics into ScratchBird Alpha.

## 9. Negative requirements

The following are prohibited as current truth:

1. a universal 32-byte or fixed-size `IndexPageHeader` for all families
2. assuming `level`, `left_sibling`, `right_sibling`, or `opaque_offset` exist
   for every index page
3. treating B-tree field presence as cross-family proof
4. using donor-engine folklore where current ScratchBird code has a different
   family layout

## 10. Implementation contract

Any implementation or audit against this file must prove:

1. every index family begins with `PageHeader`
2. family dispatch occurs from `PageType`
3. family-local parsing is used after dispatch
4. shared tooling does not assume a synthetic universal index header
5. B-tree handling matches the compiled `SBBTreePage` and `SBBTreeNode`
   contracts exactly
