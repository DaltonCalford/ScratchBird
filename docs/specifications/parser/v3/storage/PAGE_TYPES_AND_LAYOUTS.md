# V3 Storage: Page Types and Size-Dependent Layouts (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

This document is the authoritative definition of page types and page layouts for
ScratchBird V3. All pages MUST conform to these layouts for all supported page sizes.

## 1) Page Sizes

Supported page sizes (bytes): 8192, 16384, 32768, 65536, 131072.

Symbols used:
- `P` = page size in bytes
- `H` = page header size in bytes (fixed)
- `S` = special area size in bytes (type-specific)
- `L` = slot (line pointer) size in bytes
- `N` = number of slots
- `pd_lower` = start of free space (grows upward)
- `pd_upper` = end of free space (grows downward)
- `pd_special` = start of special area

## 2) Universal Page Header (Fixed Layout)

All pages begin with the same fixed-size header. All fields are little-endian.

Fields (in order):
- `magic:u32` = 0x53425244 ('SBRD')
- `version:u16` = page format version
- `page_type:u16` = PageType enum
- `page_size:u32` = P
- `page_id:u64` = absolute page id within file
- `generation:u64` = monotonic generation counter
- `checksum:u32` = CRC32C over entire page
- `flags:u16` = page flags
- `reserved0:u16` = 0
- `lsn:u64` = MUST be 0 in V3 (WAL forbidden)
- `prev_page_id:u64` = linked list prev (0 if none)
- `next_page_id:u64` = linked list next (0 if none)
- `pd_lower:u32`
- `pd_upper:u32`
- `pd_special:u32`
- `reserved1:u32` = 0

Header size: `H = 80` bytes.

Invariants:
- `magic` must match
- `page_size == P`
- `lsn == 0`
- `pd_lower <= pd_upper <= pd_special <= P`
- `pd_lower >= H`

## 3) Page Type Catalog (Separate Type Per Index)

### 3.1 System and Control Pages
- `PAGE_DATABASE_HEADER = 0x0001`
- `PAGE_CATALOG_ROOT = 0x0002`
- `PAGE_FSM = 0x0003`
- `PAGE_TIP = 0x0004`
- `PAGE_VM = 0x0005`
- `PAGE_SEQUENCE = 0x0006`
- `PAGE_SLRU = 0x0007`
- `PAGE_FREE_LIST = 0x0008`

### 3.2 Heap / TOAST
- `PAGE_HEAP = 0x0101`
- `PAGE_TOAST = 0x0102`

### 3.3 Index Page Types (One Per Index)
- `PAGE_INDEX_BTREE = 0x1001`
- `PAGE_INDEX_HASH = 0x1002`
- `PAGE_INDEX_GIN = 0x1003`
- `PAGE_INDEX_GIST = 0x1004`
- `PAGE_INDEX_SPGIST = 0x1005`
- `PAGE_INDEX_BRIN = 0x1006`
- `PAGE_INDEX_BITMAP = 0x1007`
- `PAGE_INDEX_RTREE = 0x1008`
- `PAGE_INDEX_HNSW = 0x1009`
- `PAGE_INDEX_LSM_MEMTABLE = 0x100A`
- `PAGE_INDEX_LSM_SSTABLE = 0x100B`
- `PAGE_INDEX_COLUMNSTORE = 0x100C`
- `PAGE_INDEX_FULLTEXT = 0x100D`
- `PAGE_INDEX_ZORDER = 0x100E`
- `PAGE_INDEX_GEOHASH_S2 = 0x100F`
- `PAGE_INDEX_QUADTREE = 0x1010`
- `PAGE_INDEX_OCTREE = 0x1011`
- `PAGE_INDEX_FST = 0x1012`
- `PAGE_INDEX_SUFFIX_ARRAY = 0x1013`
- `PAGE_INDEX_SUFFIX_TREE = 0x1014`
- `PAGE_INDEX_CMS = 0x1015`
- `PAGE_INDEX_HLL = 0x1016`
- `PAGE_INDEX_ART = 0x1017`
- `PAGE_INDEX_LEARNED = 0x1018`
- `PAGE_INDEX_JSON_PATH = 0x1019`
- `PAGE_INDEX_IVF = 0x101A`
- `PAGE_INDEX_ZONEMAP = 0x101B`

### 3.4 Overflow and Misc
- `PAGE_INDEX_OVERFLOW = 0x1F01`
- `PAGE_SPECIAL = 0x1FFF`

## 4) Common Layout Formulas

### 4.1 Slot-Based Pages (Heap + Many Indexes)

- `pd_lower = H + (N * L)`
- `pd_special = P - S`
- `pd_upper = pd_special - payload_bytes`
- `N_max = floor((pd_special - H) / L)`
- `free = pd_upper - pd_lower`

### 4.2 Fixed-Region Pages (FSM/TIP/VM)

- `pd_lower = H`
- `pd_upper = pd_special`
- `pd_special = P - S` (often `S = 0`)

## 5) Page Layout Templates

### 5.1 Heap Page (`PAGE_HEAP`)

Layout:
```
[PageHeader H]
[LinePointers N * L]
[Free Space]
[Tuple Payloads]
[HeapSpecial S]
```

Constants:
- `L = 8` bytes (slot: offset + length + flags)
- `S = HeapSpecial`

### 5.2 TOAST Page (`PAGE_TOAST`)

Same as heap page with TOAST chunks as payloads.

### 5.3 B-Tree Page (`PAGE_INDEX_BTREE`)

Layout:
```
[PageHeader H]
[BtreePageHeader IH]
[LinePointers N * L]
[Free Space]
[IndexTuples]
[BtreeSpecial S]
```

Constants:
- `IH = BtreePageHeader`
- `L = 8`

### 5.4 Hash Page (`PAGE_INDEX_HASH`)

Layout:
```
[PageHeader H]
[HashPageHeader IH]
[LinePointers N * L]
[Free Space]
[IndexTuples]
[HashSpecial S]
```

### 5.5 GIN/GIST/SPGiST/BRIN/Bitmap

All use the slot-based template with their own special area and tuple formats.
The special area definitions are authoritative in their respective index specs
under `docs/specifications/parser/v3/indexes/`.

## 6) Validation Rules

- Any page with a mismatched `page_type` MUST be rejected.
- Any page failing header invariants MUST be rejected.
- Index pages MUST match their index-specific page type.

## Related Specs

- `docs/specifications/parser/v3/storage/ON_DISK_FORMAT.md`
- `docs/specifications/parser/v3/indexes/INDEX_ARCHITECTURE.md`
