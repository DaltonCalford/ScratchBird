# On-Disk Format Inventory and Version Manifest

Status: current_authority

## 1. Current durable format authority

The current durable format is the combination of:

- canonical `PageHeader`
- legacy compatibility header mapping path
- `PageType` taxonomy
- family-local heap and index base layouts
- bootstrap page assignments from section `06`
- current restore, attach, and format-gate validation paths

## 2. Current top-level format inventory

### Canonical Alpha page family

- header magic: `K_MAGIC_SBRD = 0x53425244`
- canonical header bytes: `106`
- supported page sizes:
  - `8192`
  - `16384`
  - `32768`
  - `65536`
  - `131072`

### Legacy compatibility family

- struct: `LegacyPageHeaderV1`
- size: `80`
- accepted only through canonicalization to `PageHeader`

### vNext reserved family

- header magic: `K_MAGIC_SBPG = 0x53425047`
- page size: `16384`
- base header bytes: `64`
- extension header bytes: `32`
- payload start: `96`
- page-type range: `0x2000..0x20FF`

## 3. Admission algorithm

Attach, restore, or direct page admission must perform:

1. magic validation
2. header-size validation
3. page-size validation
4. page-type validation
5. checksum validation when checksum-required flags demand it
6. page-geometry validation
7. family-local validation after routing

If a page is legacy-format, the engine must canonicalize it first and then apply
canonical legality checks.

## 4. Compatibility rules

The current engine guarantees:

- restore or attach must fail closed when durable format is newer or
  incompatible
- page-size, page-type, and page-header legality are part of format admission
- legacy headers are admitted only through the current canonicalization path
- vNext pages are admitted only if they satisfy the compiled vNext constants

## 5. Non-claims

This file does not claim:

- a universal future format-negotiation framework
- donor-engine page-format compatibility
- automatic promotion of future-lane or vNext pages into current Alpha page
  authority
