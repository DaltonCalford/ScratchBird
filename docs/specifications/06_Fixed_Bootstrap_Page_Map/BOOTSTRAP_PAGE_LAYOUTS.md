# Bootstrap Page Layouts

Status: current_authority

## 1. Scope and authority

This document defines the fixed bootstrap page map for the primary ScratchBird database file.
The controlling binary authority is the current bootstrap-page implementation in the engine together with the
current page-type authority in `ScratchBird/include/scratchbird/core/ondisk.h`.

This section applies to the primary database file only.
It does not authorize duplicating the same fixed bootstrap map inside secondary tablespace files.

## 2. Fixed primary-file bootstrap map

The current fixed bootstrap map is:

| Physical page number | Required page type | Meaning |
| --- | --- | --- |
| `0` | `PAGE_TYPE_DATABASE_HEADER` | Main database header page |
| `1` | `PAGE_TYPE_SYSTEM_STATE` | Bootstrap system-state page |
| `2` | `PAGE_TYPE_CATALOG_ROOT` | Catalog-root bootstrap page |
| `3` | `PAGE_TYPE_FSM_ROOT` | Free-space-map root bootstrap page |
| `4` | `PAGE_TYPE_TRANSACTION_MAP` | Transaction-map bootstrap root |
| `5` | `PAGE_TYPE_BOOTSTRAP_RESERVED` | Reserved bootstrap slot |

This map is fixed and positional.
The engine must not infer these pages from catalog state at open time.
Open-time bootstrap validation starts from these physical locations.

## 3. Open-path validation contract

The open path must validate the fixed bootstrap pages as part of database bring-up.
At minimum, pages `1` through `5` are treated as required bootstrap pages and must match their required
page types and bootstrap roles.

Open-path failure rules:

1. if a required bootstrap page is missing, fail closed
2. if a bootstrap page has the wrong `PageType`, fail closed
3. if a bootstrap page is present but structurally malformed for its required role, fail closed
4. bootstrap recovery may classify incidents, but it must not silently remap the fixed page numbers to new
   roles

## 4. Role of each bootstrap page

### 4.1 Page `0`: database header

Page `0` is the primary database header page.
It anchors database-level identity and primary-file startup metadata.
It is not interchangeable with the tablespace header used by secondary files.

### 4.2 Page `1`: system state

Page `1` is the bootstrap system-state page.
It carries the fixed system-state role required before normal catalog-driven discovery can proceed.

### 4.3 Page `2`: catalog root

Page `2` is the fixed catalog-root bootstrap page.
It is the anchor for catalog-root discovery during startup.
The catalog root is not relocatable by routine runtime policy unless a future explicit migration protocol is
specified.

### 4.4 Page `3`: FSM root

Page `3` is the fixed free-space-map root bootstrap page.
It is the bootstrap entry point for free-space discovery in the primary file.

### 4.5 Page `4`: transaction map root

Page `4` is the fixed transaction-map bootstrap root.
This is real current transaction infrastructure and must not be described as a placeholder or as an
uncommitted future slot.
The transaction layer remains MGA-based; page `4` does not authorize `WAL`, redo-log, or LSN authority.

### 4.6 Page `5`: bootstrap reserved

Page `5` is the reserved bootstrap slot.
It is positionally reserved and must remain available for explicitly defined bootstrap uses only.
Implementations must not repurpose it ad hoc.

## 5. Main-file versus tablespace boundary

The fixed bootstrap map belongs to the main database file.
Secondary tablespace files are governed by their own header contract and are not allowed to clone pages
`0` through `5` as if they were miniature primary databases.

Rules:

1. the primary file owns the fixed bootstrap map
2. secondary tablespaces own tablespace-local header state, not the primary bootstrap map
3. global bootstrap authority comes from the primary file first, then from structures reachable from that
   bootstrap state

## 6. Negative requirements

The following are explicitly prohibited as current truth:

1. treating page `4` as an undefined future page
2. moving the catalog root off page `2` without an explicit migration protocol
3. describing the fixed bootstrap map as optional startup guidance instead of mandatory primary-file truth
4. inferring a `WAL` or log-replay bootstrap dependency from any bootstrap page

## 7. Implementation and test contract

Any implementation or audit against this file must prove:

1. the primary database file has the fixed page map `0` through `5`
2. the required page types match the map exactly
3. startup validation checks the bootstrap pages before normal runtime access
4. secondary tablespaces do not masquerade as primary bootstrap files
5. page `4` is treated as the live transaction-map bootstrap root
