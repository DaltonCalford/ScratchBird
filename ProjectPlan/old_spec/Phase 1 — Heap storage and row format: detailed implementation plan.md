# Phase 1 — Heap Storage and Row Format ✅ **COMPLETED**

## Implementation Status: FULLY IMPLEMENTED

All planned features have been successfully implemented:

- ✅ **Complete heap page structures** with slot-directory model (`[PageHeader][HeapPageHeader][tuples...][free space][line pointer directory]`)
- ✅ **Tuple encoding/decoding** with null bitmap and attribute directory
- ✅ **RowID format** and relation root structures
- ✅ **Varlena and overflow support** with off-page storage (`HeapOverflow` pages)
- ✅ **Heap relation API** with `HeapRelation` and `HeapScan` classes
- ✅ **Comprehensive test suite** (heap_*.cpp files: heap_tuple_tests.cpp, heap_page_tests.cpp, etc.)
- ✅ **Integration** with pager, allocator, and transaction systems
- ✅ **Free space tracking** per page with sophisticated page management
- ✅ **Heap tuple codec** with encoding/decoding, null handling, overflow support

**Exit Criteria Met:**
- ✅ Create/insert/select rows via internal harness
- ✅ Comprehensive page validation tools
- ✅ All planned APIs and structures implemented

---

## Original Implementation Plan (Completed)

### 1. On-disk structures (ODS) and constants

- ✅ **Page types and metadata implemented**: `ods::PageType::HeapRoot`, `ods::PageType::HeapData`, `ods::PageType::HeapOverflow`, `ods::PageType::Blob` in `include/scratchbird/engine/ods.h`
- ✅ **Fixed constants implemented**: maximum slots per page, alignment, and varlena thresholds

- Heap data page layout (slot-directory model)

  - Physical layout: `[PageHeader][HeapPageHeader][tuples...][free space][line pointer directory]`
  - `HeapPageHeader`:
    - `std::uint16_t num_slots`
    - `std::uint16_t free_start` (first free byte after header)
    - `std::uint16_t dir_start` (start offset of slot directory from the end)
    - `std::uint16_t flags` (e.g., has_free_space, has_overflow)
  - Slot directory: array of 2-byte offsets; offset 0 means dead/unused slot.

- Tuple record header (per row)

  - `std::uint64_t created_xid` (placeholder for Phase 3 MGA)
  - `std::uint64_t deleted_xid` (0 if live; placeholder)
  - `std::uint64_t backptr_rid` (RID of previous version, 0 none; placeholder)
  - `std::uint16_t num_attrs`
  - `std::uint16_t nullmap_bytes`
  - `std::uint16_t varlena_bytes` (optional; may be implicit)
  - `std::uint16_t flags` (e.g., has_overflow)
  - `[null-bitmap][attribute directory][attribute data]`

- Varlen and overflow

  - Attribute cell encodings:
    - Fixed-width: bytes inline.
    - Varlen small: 2-byte length + bytes inline.
    - Varlen large: 0xFFFF + 4-byte length + bytes (if fits) or off-page marker.
    - Off-page: marker tag + `OverflowRef { space_id, page_no, offset/slot, length }`.
  - Overflow pages: `ods::PageType::HeapOverflow` payload is a sequence of chunks; tuple stores one or more refs (simple: single ref, Phase 1).

- Heap root page (per relation)

  - `ods::PageType::HeapRoot` payload:
    - `std::uint16_t version`
    - `std::uint16_t flags`
    - `std::uint32_t first_heap_page`
    - `std::uint32_t last_heap_page`
    - `std::uint32_t free_space_hint_page` (optional)
    - `std::uint32_t tuple_format_id` (references relation’s column layout id)
  - Relation “table root structures”:
    - Add a `RelRoot` metadata struct held in memory with cached page_size, root_page_no, format, etc.

- RowID format (RID)

  - 64-bit logical RID:
    - `std::uint16_t space_id` (tablespace)
    - `std::uint32_t page_no` (heap data page)
    - `std::uint16_t slot_no` (line pointer index)
  - Provide helpers to pack/unpack RID and to compute file offsets via `FileMap`.

#### 2. Core APIs and modules

- New header `include/scratchbird/engine/heap.h`

  - `struct TupleLayout { vector<AttrMeta> attrs; }` where `AttrMeta { TypeId, fixed_len, by_val, nullable }`
  - `struct InsertResult { RowId rid; std::uint32_t bytes_written; bool overflow; };`
  - `class HeapRelation`:
    - `static HeapRelation create(FileMap&, const TupleLayout&, const HeapOptions&)`
    - `static HeapRelation open(FileMap&, std::uint32_t heap_root_page)`
    - `InsertResult insert(const std::vector<Value>& attrs)`
    - `bool fetch(RowId, std::vector<Value>& out) const`
    - `bool update(RowId, const vector<Value>& newvals, RowId* new_rid)` (stubbed or omitted in Phase 1)
    - `bool remove(RowId)` (stubbed or omitted in Phase 1)
    - `HeapScan open_scan()` returning an iterator over rows in page order
    - Internal helpers: `find_page_with_freespace(size_t need)`, `insert_into_page(page_no, ...)`
  - `class HeapScan`:
    - `bool next(std::vector<Value>& out, RowId* rid_out)`

- Free space tracking (page-local, Phase 1)

  - Maintain `free_start` and `dir_start` per page; `free_bytes = dir_start - free_start`
  - Append a “has free space” bit in header flags if `free_bytes > threshold`
  - Simple allocation policy:
    - For small relations, linear probing from `last_heap_page` backward until page with free space or allocate a new page.
    - Optionally add a lightweight in-memory heap free-space heap (min-heap keyed by free bytes) per opened relation; recomputed lazily.

- Integration with existing subsystems

  - Use `Pager` and `BufferCache` for reads/writes; `ChecksumPolicy` honored.
  - Use `Allocator` for new page allocation (Phase E already introduced allocator; wire if available; otherwise fallback to `next_alloc_page_` for Phase 1).
  - WAL: define logical record types `HeapInsert` and `HeapOverflowWrite` stubs to register later; for Phase 1 allow “no WAL” (behind config) or optionally log record metadata without replay.

#### 3. Encoding/decoding and null/varlen handling

- Null map
  - Compact bitset of `num_attrs` bits; 1=Null.
  - Must be byte-aligned; size = `(num_attrs + 7) / 8`.
- Attribute directory (optional)
  - If mixing fixed and varlen, maintain a per-attribute offset table after the nullmap to allow random access without full linear scan.
- Varlen encoding
  - Length prefix as described under ODS; for JSON/text/blob use varlen path; overflow threshold = `page_size - safety_margin - hdr_overheads`.
- Overflow management
  - Phase 1: single-chunk overflow; allocate `HeapOverflow` page and write contiguous bytes; store one `OverflowRef` in tuple.
  - Deallocation policy out-of-scope for Phase 1; test only inserts/reads.

#### 4. Table creation and bootstrap

- `HeapRelation::create`:
  - Allocate `HeapRoot` page; initialize payload and pointers; allocate the first `HeapData` page; mark `first_heap_page=last_heap_page=new_page`.
  - Persist tuple format id (for now, a hash or sequential id); Phase 4 will tie this to `SDB$COLUMN` layout.

#### 5. Scanners and basic selection

- `HeapScan` iterator:
  - Sequential page traversal from `first_heap_page` to `last_heap_page`.
  - For each page, iterate slots, skip `0` offsets and tombstones (not used in Phase 1), decode tuple and return as `Value[]`.
- `fetch(RowId)`:
  - Read page, read offset by slot, decode tuple, follow overflow if needed.

#### 6. Validation and page tools

- Page validator CLI `heap_check`:
  - Usage: `heap_check <seg0_base_path> <heap_root_page>`
  - Validates:
    - Page header types, checksums
    - For each `HeapData` page: slot directory bounds and monotonicity, no overlaps, `free_start <= dir_start`
    - Tuple decoded lengths aligned with stored lengths; nullmap matches attribute directory; overflow references point to `HeapOverflow` pages with sufficient data
  - Emits “bloat/advice” similar to `fast_check`: very low average tuples/page suggests `VACUUM/REBUILD` (future).

#### 7. Internal harness and tests

- Unit tests (GoogleTest or simple mains in `tests/heap_*`):
  - Create/open relation; insert rows with:
    - All-null row
    - All fixed-width row
    - Mixed null/non-null row
    - Varlen short rows
    - Varlen large rows triggering overflow
  - `fetch(rid)` roundtrips to original values
  - `HeapScan` iterates all inserted rows in order of insertion or page order
  - Corruptions: tamper a slot; validator detects and reports
- Fuzz tests (lightweight):
  - Random schemas (fixed/varlen), random row generation with nulls; insert N rows; scan and verify checksum of logical values.

#### 8. Performance and safety considerations (Phase 1 scope)

- Alignment: write records at 2-byte boundaries to retain compact slot directory; consider 8-byte align only for fixed 8-byte types.
- Short-key fast paths: not needed; heap payloads are opaque.
- Prefetch: optional prefetch of next heap page in `HeapScan` using `FileManager::prefetch_willneed` with a small horizon.
- Checksums: set per-page header checksum via `ods::crc32c`; validator checks on read if config says `VerifyOnRead`.

#### 9. File and code touchpoints

- New:
  - `include/scratchbird/engine/heap.h`
  - `src/engine/heap.cpp`
  - `tools/heap_check/heap_check.cpp`
  - `tests/heap_basic_tests.cpp`, `tests/heap_overflow_tests.cpp`, `tests/heap_scan_tests.cpp`, `tests/heap_validator_tests.cpp`
- Updated:
  - `include/scratchbird/engine/ods.h` (page types, small helpers)
  - `CMakeLists.txt` (add sources and tool)
  - Optionally `specs/engine/heap-format.md` to document on-disk layouts in detail.

#### 10. Milestones and exit criteria

- M1: Data structures and page read/write
  - Heap page encode/decode; record header/nullmap/varlen encode/decode; unit tests for single page.
- M2: Relation create/open and multi-page insert
  - Allocate pages; free space accounting; simple page selection policy; unit tests insert 10K rows.
- M3: Fetch and scan
  - `fetch(RowId)` and `HeapScan`; verify round-trip; test varied schemas and overflow.
- M4: Validator and tool
  - Implement `heap_check`; inject corruptions in tests; validator catches issues; advice emitted when avg tuples/page < threshold.
- Exit (Phase 1 complete):
  - Create/insert/select via internal harness works across fixed/varlen/overflow cases.
  - Page validator passes on healthy data and flags intentional corruptions.
  - No checksum mismatches; no buffer overflows (ASAN clean if enabled).

#### 11. Deferred to later phases (scaffold now where cheap)

- MGA fields are present in tuple header but not enforced yet (Phase 3).

- WAL records are stubbed/types reserved; full WAL/recovery integration comes with later phases.

- Free space map global structure (FSM) and vacuum are Phase 2/15.

- Update/delete, HOT chains, and tombstones are Phase 3/7.

  ### Phase 1 — Heap storage and row format: detailed implementation plan

#### 1. On-disk structures (ODS) and constants

- Add page types and metadata

- Add ods::PageType::HeapRoot, ods::PageType::HeapData, ods::PageType::HeapOverflow, ods::PageType::Blob (if not already present) to include/scratchbird/engine/ods.h.

- Add fixed constants for maximum slots per page, alignment, and varlena thresholds.

- Heap data page layout (slot-directory model)

- Physical layout: [PageHeader][HeapPageHeader][tuples...][free space][line pointer directory]

- HeapPageHeader:

- std::uint16_t num_slots

- std::uint16_t free_start (first free byte after header)

- std::uint16_t dir_start (start offset of slot directory from the end)

- std::uint16_t flags (e.g., has_free_space, has_overflow)

- Slot directory: array of 2-byte offsets; offset 0 means dead/unused slot.

- Tuple record header (per row)

- std::uint64_t created_xid (placeholder for Phase 3 MGA)

- std::uint64_t deleted_xid (0 if live; placeholder)

- std::uint64_t backptr_rid (RID of previous version, 0 none; placeholder)

- std::uint16_t num_attrs

- std::uint16_t nullmap_bytes

- std::uint16_t varlena_bytes (optional; may be implicit)

- std::uint16_t flags (e.g., has_overflow)

- [null-bitmap][attribute directory][attribute data]

- Varlen and overflow

- Attribute cell encodings:

- Fixed-width: bytes inline.

- Varlen small: 2-byte length + bytes inline.

- Varlen large: 0xFFFF + 4-byte length + bytes (if fits) or off-page marker.

- Off-page: marker tag + OverflowRef { space_id, page_no, offset/slot, length }.

- Overflow pages: ods::PageType::HeapOverflow payload is a sequence of chunks; tuple stores one or more refs (simple: single ref, Phase 1).

- Heap root page (per relation)

- ods::PageType::HeapRoot payload:

- std::uint16_t version

- std::uint16_t flags

- std::uint32_t first_heap_page

- std::uint32_t last_heap_page

- std::uint32_t free_space_hint_page (optional)

- std::uint32_t tuple_format_id (references relation’s column layout id)

- Relation “table root structures”:

- Add a RelRoot metadata struct held in memory with cached page_size, root_page_no, format, etc.

- RowID format (RID)

- 64-bit logical RID:

- std::uint16_t space_id (tablespace)

- std::uint32_t page_no (heap data page)

- std::uint16_t slot_no (line pointer index)

- Provide helpers to pack/unpack RID and to compute file offsets via FileMap.

#### 2. Core APIs and modules

- New header include/scratchbird/engine/heap.h

- struct TupleLayout { vector<AttrMeta> attrs; } where AttrMeta { TypeId, fixed_len, by_val, nullable }

- struct InsertResult { RowId rid; std::uint32_t bytes_written; bool overflow; };

- class HeapRelation:

- static HeapRelation create(FileMap&, const TupleLayout&, const HeapOptions&)

- static HeapRelation open(FileMap&, std::uint32_t heap_root_page)

- InsertResult insert(const std::vector<Value>& attrs)

- bool fetch(RowId, std::vector<Value>& out) const

- bool update(RowId, const vector<Value>& newvals, RowId* new_rid) (stubbed or omitted in Phase 1)

- bool remove(RowId) (stubbed or omitted in Phase 1)

- HeapScan open_scan() returning an iterator over rows in page order

- Internal helpers: find_page_with_freespace(size_t need), insert_into_page(page_no, ...)

- class HeapScan:

- bool next(std::vector<Value>& out, RowId* rid_out)

- Free space tracking (page-local, Phase 1)

- Maintain free_start and dir_start per page; free_bytes = dir_start - free_start

- Append a “has free space” bit in header flags if free_bytes > threshold

- Simple allocation policy:

- For small relations, linear probing from last_heap_page backward until page with free space or allocate a new page.

- Optionally add a lightweight in-memory heap free-space heap (min-heap keyed by free bytes) per opened relation; recomputed lazily.

- Integration with existing subsystems

- Use Pager and BufferCache for reads/writes; ChecksumPolicy honored.

- Use Allocator for new page allocation (Phase E already introduced allocator; wire if available; otherwise fallback to next_alloc_page_ for Phase 1).

- WAL: define logical record types HeapInsert and HeapOverflowWrite stubs to register later; for Phase 1 allow “no WAL” (behind config) or optionally log record metadata without replay.

#### 3. Encoding/decoding and null/varlen handling

- Null map

- Compact bitset of num_attrs bits; 1=Null.

- Must be byte-aligned; size = (num_attrs + 7) / 8.

- Attribute directory (optional)

- If mixing fixed and varlen, maintain a per-attribute offset table after the nullmap to allow random access without full linear scan.

- Varlen encoding

- Length prefix as described under ODS; for JSON/text/blob use varlen path; overflow threshold = page_size - safety_margin - hdr_overheads.

- Overflow management

- Phase 1: single-chunk overflow; allocate HeapOverflow page and write contiguous bytes; store one OverflowRef in tuple.

- Deallocation policy out-of-scope for Phase 1; test only inserts/reads.

#### 4. Table creation and bootstrap

- HeapRelation::create:

- Allocate HeapRoot page; initialize payload and pointers; allocate the first HeapData page; mark first_heap_page=last_heap_page=new_page.

- Persist tuple format id (for now, a hash or sequential id); Phase 4 will tie this to SDB$COLUMN layout.

#### 5. Scanners and basic selection

- HeapScan iterator:

- Sequential page traversal from first_heap_page to last_heap_page.

- For each page, iterate slots, skip 0 offsets and tombstones (not used in Phase 1), decode tuple and return as Value[].

- fetch(RowId):

- Read page, read offset by slot, decode tuple, follow overflow if needed.

#### 6. Validation and page tools

- Page validator CLI heap_check:

- Usage: heap_check <seg0_base_path> <heap_root_page>

- Validates:

- Page header types, checksums

- For each HeapData page: slot directory bounds and monotonicity, no overlaps, free_start <= dir_start

- Tuple decoded lengths aligned with stored lengths; nullmap matches attribute directory; overflow references point to HeapOverflow pages with sufficient data

- Emits “bloat/advice” similar to fast_check: very low average tuples/page suggests VACUUM/REBUILD (future).

#### 7. Internal harness and tests

- Unit tests (GoogleTest or simple mains in tests/heap_*):

- Create/open relation; insert rows with:

- All-null row

- All fixed-width row

- Mixed null/non-null row

- Varlen short rows

- Varlen large rows triggering overflow

- fetch(rid) roundtrips to original values

- HeapScan iterates all inserted rows in order of insertion or page order

- Corruptions: tamper a slot; validator detects and reports

- Fuzz tests (lightweight):

- Random schemas (fixed/varlen), random row generation with nulls; insert N rows; scan and verify checksum of logical values.

#### 8. Performance and safety considerations (Phase 1 scope)

- Alignment: write records at 2-byte boundaries to retain compact slot directory; consider 8-byte align only for fixed 8-byte types.

- Short-key fast paths: not needed; heap payloads are opaque.

- Prefetch: optional prefetch of next heap page in HeapScan using FileManager::prefetch_willneed with a small horizon.

- Checksums: set per-page header checksum via ods::crc32c; validator checks on read if config says VerifyOnRead.

#### 9. File and code touchpoints

- New:

- include/scratchbird/engine/heap.h

- src/engine/heap.cpp

- tools/heap_check/heap_check.cpp

- tests/heap_basic_tests.cpp, tests/heap_overflow_tests.cpp, tests/heap_scan_tests.cpp, tests/heap_validator_tests.cpp

- Updated:

- include/scratchbird/engine/ods.h (page types, small helpers)

- CMakeLists.txt (add sources and tool)

- Optionally specs/engine/heap-format.md to document on-disk layouts in detail.

#### 10. Milestones and exit criteria

- M1: Data structures and page read/write

- Heap page encode/decode; record header/nullmap/varlen encode/decode; unit tests for single page.

- M2: Relation create/open and multi-page insert

- Allocate pages; free space accounting; simple page selection policy; unit tests insert 10K rows.

- M3: Fetch and scan

- fetch(RowId) and HeapScan; verify round-trip; test varied schemas and overflow.

- M4: Validator and tool

- Implement heap_check; inject corruptions in tests; validator catches issues; advice emitted when avg tuples/page < threshold.

- Exit (Phase 1 complete):

- Create/insert/select via internal harness works across fixed/varlen/overflow cases.

- Page validator passes on healthy data and flags intentional corruptions.

- No checksum mismatches; no buffer overflows (ASAN clean if enabled).

#### 11. Deferred to later phases (scaffold now where cheap)

- MGA fields are present in tuple header but not enforced yet (Phase 3).

- WAL records are stubbed/types reserved; full WAL/recovery integration comes with later phases.

- Free space map global structure (FSM) and vacuum are Phase 2/15.

- Update/delete, HOT chains, and tombstones are Phase 3/7.

- Summary

- Designed slot-directory heap page format with tuple header, nullmap, varlena/overflow.

- Defined RowID, relation root structures, APIs (HeapRelation, HeapScan).

- Planned validator CLI and comprehensive tests.

- Integration points aligned with Pager/Allocator and future MGA/WAL.
