# Page Header Layout

Status: current_authority

## 1. Scope and authority

This document defines the current shared page-header binary contract for
ScratchBird page families.

The controlling binary authority is:

- `ScratchBird/include/scratchbird/core/ondisk.h`
- the compiled `PageHeader`
- the compiled `LegacyPageHeaderV1`
- the current `PageType` enum

Any older prose that invents a different universal page header is non-authority.

## 2. Magic and header size contract

### Canonical page header

- struct: `PageHeader`
- exact size: `106` bytes
- magic constant: `K_MAGIC_SBRD = 0x53425244`
- required `header_bytes`: `CANONICAL_PAGE_HEADER_BYTES = 106`

### Legacy compatibility header

- struct: `LegacyPageHeaderV1`
- exact size: `80` bytes
- accepted only through the current legacy compatibility path

Canonical code must publish and validate the 106-byte `PageHeader`.

## 3. Canonical field layout

The current `PageHeader` field set is:

| Offset | Field | Type | Meaning |
| --- | --- | --- | --- |
| `0x00` | `magic` | `uint32_t` | must equal `K_MAGIC_SBRD` |
| `0x04` | `version` | `uint16_t` | on-disk format version |
| `0x06` | `header_bytes` | `uint16_t` | canonical header length, currently `106` |
| `0x08` | `page_type` | `uint16_t` | current `PageType` enum value |
| `0x0A` | `flags` | `uint16_t` | page flags |
| `0x0C` | `page_size` | `uint32_t` | legal page size |
| `0x10` | `header_checksum` | `uint32_t` | checksum over header bytes excluding payload-checksum slot |
| `0x14` | `payload_checksum` | `uint32_t` | payload checksum compatibility alias `checksum` |
| `0x18` | `page_id` | `uint64_t` | 0-based page number in file |
| `0x20` | `database_uuid` | `uint8_t[16]` | database UUID |
| `0x30` | `object_uuid` | `uint8_t[16]` | owning object UUID |
| `0x40` | `page_generation` | `uint64_t` | canonical generation, compatibility alias `generation` |
| `0x48` | `flush_generation` | `uint64_t` | last durably flushed generation |
| `0x50` | `checkpoint_generation` | `uint64_t` | last completed checkpoint covering the page |
| `0x58` | `repair_epoch` | `uint32_t` | repair publication epoch |
| `0x5C` | `repair_state_raw` | `uint16_t` | repair-state marker |
| `0x5E` | `free_space` | `uint32_t` | free bytes in payload band |
| `0x62` | `item_count` | `uint16_t` | number of items on page |
| `0x64` | `free_offset` | `uint16_t` | scaled lower bound for free-space band |
| `0x66` | `special_size` | `uint16_t` | scaled size of end-anchored special area |
| `0x68` | `reserved` | `uint16_t` | must remain zero |

## 4. Legal page sizes

The current validator accepts only:

- `8192`
- `16384`
- `32768`
- `65536`
- `131072`

Any other `page_size` is page corruption.

## 5. Page-space geometry

ScratchBird computes page geometry from the header through the current helper
contract:

- `pageLower(header)` = `free_offset * unit`
- `pageUpper(header)` = `pageLower(header) + free_space`
- `pageSpecial(header)` = `page_size - (special_size * unit)`

where:

- `unit = 1` when `page_size <= 0xFFFF`
- `unit = 2` when `page_size > 0xFFFF`

### Required legality

The shared validator requires:

- `lower >= header_bytes`
- `lower <= upper`
- `upper <= special`
- `special <= page_size`

Any violation is page corruption.

## 6. Page flags

Current shared page flags are:

- `PAGE_FLAG_DIRTY = 0x0001`
- `PAGE_FLAG_PINNED = 0x0002`
- `PAGE_FLAG_COMPRESSED = 0x0004`
- `PAGE_FLAG_ENCRYPTED = 0x0008`
- `PAGE_FLAG_SPECIAL = 0x0010`
- `PAGE_FLAG_CHECKSUM_VALID = 0x0020`
- `PAGE_FLAG_TEMPORARY_WORK = 0x0040`

`PAGE_FLAG_TEMPORARY_WORK` and `PAGE_TYPE_TEMP_HEAP` are the current shared
temporary-work indicators.

## 7. Page type authority

`PageType` is the only current shared page-family classifier.
Routing, validation, repair classification, and dump tooling must dispatch from
`page_type` alone before any family-local parsing.

Current families include:

- core bootstrap and storage pages
- heap, TOAST, and LOB pages
- B-tree, hash, GIN, GiST, SP-GiST, BRIN, bitmap, inverted, sparse, trie, ART,
  spatial, bloom, SAI, SASI families
- columnstore, LSM, and sort families
- vector and ANN families
- emulation and Redis families
- reserved vNext range `0x2000..0x20FF`

Unknown page types fail closed unless an explicit compatibility gate owns them.

## 8. Legacy-to-canonical mapping

Current code accepts `LegacyPageHeaderV1` only through explicit
canonicalization.

The canonicalization rules are:

1. `header_bytes = 106`
2. `flags = legacy.flags & 0xFFFF`
3. `payload_checksum = legacy.checksum`
4. `page_id = legacy.page_id`
5. `page_generation = legacy.generation`
6. `flush_generation = legacy.generation`
7. `checkpoint_generation = legacy.generation`
8. `repair_epoch = 0`
9. `repair_state_raw = REPAIR_NONE`
10. `free_space` is scaled by unit `2` when `page_size > 0xFFFF`, otherwise
    unit `1`
11. `reserved = 0`

This mapping is the only legal way to interpret the legacy header as canonical
header state.

## 9. Shared validation algorithm

The shared page-header validation algorithm is:

1. validate `magic == K_MAGIC_SBRD`
2. validate `page_size` is one of the legal page sizes
3. validate expected page size if caller provides one
4. validate expected `page_type` if caller provides one
5. validate `header_bytes == 106`
6. validate `repair_state_raw` is inside the legal repair-state enum range
7. validate `flush_generation <= page_generation`
8. validate `checkpoint_generation <= flush_generation`
9. validate expected `database_uuid` and `object_uuid` when supplied
10. validate `lower`, `upper`, and `special` bounds

Failure at any step is page corruption.

## 10. vNext overlay boundary

The vNext page family is separate from the canonical Alpha page header.

Current vNext constants are:

- `VNEXT_PAGE_SIZE_BYTES = 16384`
- `VNEXT_PAGE_ALIGNMENT_BYTES = 8`
- `VNEXT_BASE_HEADER_BYTES = 64`
- `VNEXT_EXT_HEADER_BYTES = 32`
- `VNEXT_PAYLOAD_REGION_START = 96`
- `K_MAGIC_SBPG = 0x53425047`

The vNext range `0x2000..0x20FF` is reserved for those page types only.
This does not authorize inventing a second current universal header for Alpha
pages.

## 11. Negative requirements

The following are prohibited as current truth:

1. inventing a different universal shared page header
2. treating `lsn`-style compatibility aliases as WAL authority
3. bypassing `PageType` routing with donor-engine family assumptions
4. reintroducing deprecated prose fields as if they outrank the compiled struct

## 12. Implementation contract

Any implementation or audit against this file must prove:

1. every current page begins with the compiled `PageHeader`
2. legal page geometry is derived from the current helper formulas
3. `PageType` drives family routing
4. legacy headers are interpreted only through the explicit canonicalization path
5. unsupported types and illegal geometry fail closed
