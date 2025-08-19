Title: Heap on-disk layout (Phase 1)
Status: draft
Version: 1

Overview
This document specifies the Phase 1 heap storage format: page types, headers, tuple encoding, overflow layout, RowId, and invariants. Parser/executor integration, MVCC enforcement, and GC are out-of-scope for this phase.

Page types
- HeapRoot: per-relation metadata (first/last heap page, tuple format id, flags)
- HeapData: tuple-bearing heap page using a slot-directory layout
- HeapOverflow: chained varlena payload pages

Common page header (ods::PageHeader)
- checksum: CRC32C of full page with checksum field zeroed before compute
- page_no: logical page number
- space_id: page space (1 = main DB)
- type: PageType
- flags: reserved
- scn: per-page SCN/LSN (future)
- prev/next: optional links (unused for heap)
- page_size, header_version

HeapRoot payload (ods::HeapRootPayload)
- version, flags
- first_heap_page, last_heap_page
- free_space_hint_page (reserved)
- tuple_format_id: FNV-1a hash of column layout (Phase 1)

HeapData header (ods::HeapPageHeader)
- num_slots: number of slot entries
- free_start: first free byte after headers/tuples
- dir_start: first byte of slot directory from page end (grows backward)
- flags: HasFreeSpace, HasOverflow (reserved)

HeapData physical layout
[PageHeader][HeapPageHeader][tuples...][free space][slot directory]
- Slot directory: array of 2-byte offsets (HEAP_SLOT_SIZE_BYTES). 0 = dead/unused.
- Alignment: tuples written at 2-byte boundaries by default; if all attributes are 8-byte fixed and by-value, tuples are 8-byte aligned.

Row identity (ods::RowId)
- 64-bit RID = [space_id:16][page_no:32][slot_no:16]
- Helpers: pack_rowid/unpack_rowid

Tuple header (ods::TupleHeader)
- created_xid, deleted_xid, backptr_rid (placeholders for MVCC in later phases)
- num_attrs, nullmap_bytes, varlena_bytes (aggregate, optional), flags
- Follows: [null-bitmap][attribute directory][attribute data]

Attribute encodings
- Fixed-width Int64: little-endian 8 bytes
- VarBytes short: 2-byte length + bytes inline
- VarBytes large: sentinel 0xFFFF + OverflowRef
- Attribute directory: 2 bytes per attribute, relative offset from start of attribute area
- Null handling: null bit set ⇒ attribute directory entry may be 0 and data absent

Overflow
- OverflowRef: {space_id, page_no, slot_or_off=0, length}
- HeapOverflow page body: [u32 length][u32 next_page][bytes...]
- Chains continue until length bytes are delivered; last page has next_page = 0

Checksums
- On write, page checksum set via ods::crc32c (checksum field zeroed during compute)
- Validator (heap_check) optionally verifies checksums when config policy = VerifyOnRead

Scan and access
- fetch(RowId): read slot offset; decode tuple; follow overflow chain as needed
- HeapScan: sequential pages from first_heap_page..last_heap_page; iterates non-zero slots
- Prefetch: advisory prefetch of current and next page via FileManager::prefetch_willneed

Invariants and validation
- free_start >= sizeof(PageHeader)+sizeof(HeapPageHeader)
- dir_start <= page_size; free_start <= dir_start
- Each non-zero slot offset ∈ [tuples_region_start, dir_start)
- Attribute-directory vs. nullmap:
  - non-null ⇒ directory offset non-zero and base+offset < dir_start
  - null ⇒ directory may be zero; payload must not be dereferenced
- Overflow: each referenced page type = HeapOverflow; length bounds within page

Limits and constants (Phase 1)
- HEAP_SLOT_SIZE_BYTES = 2
- HEAP_ALIGN_BYTES = 2; HEAP_ALIGN8_BYTES = 8 (optional path)
- HEAP_OVERFLOW_THRESHOLD_PCT = 80 (effective threshold considers page overhead)
- HEAP_MAX_SLOTS_PER_PAGE: guard value; constrained in practice by free space

Compatibility notes
- Tuple format id is a hash in Phase 1; will bind to SDB$ schema in Phase 4
- MGA headers present but not enforced until Phase 3

Future phases
- Free space maps, allocation policy, HOT updates/GC, visibility checks, WAL integration
