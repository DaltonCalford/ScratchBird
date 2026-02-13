# Appendix: Firebird 5 ODS Invariants (Authoritative)

This appendix defines the on‑disk invariants that are required for 1:1 emulation of Firebird 5 ODS behavior. The values and layouts below are authoritative and must be implemented exactly.

## ODS Version (Authoritative)
- `ODS_MAJOR = 13`
- `ODS_MINOR = 1`
- `ODS_CURRENT_VERSION = ENCODE_ODS(13, 1)`
- `ODS_FIREBIRD_FLAG = 0x8000`
- `ENCODE_ODS(major, minor) = (major << 4) | minor`
- `DECODE_ODS_MAJOR(ods) = (ods & 0x7FF0) >> 4`
- `DECODE_ODS_MINOR(ods) = ods & 0x000F`

## Page Size and Alignment (Authoritative)
- `PAGE_SIZE_BASE = 1024`
- `MIN_PAGE_SIZE = 8192`
- `MAX_PAGE_SIZE = 32768`
- `DEFAULT_PAGE_SIZE = 8192`
- `ODS_ALIGNMENT = 4`

Page size rules:
- `hdr_page_size` MUST be a multiple of `PAGE_SIZE_BASE`.
- `MIN_PAGE_SIZE <= hdr_page_size <= MAX_PAGE_SIZE`.
- `hdr_page_size % MIN_PAGE_SIZE == 0`.

## Page Types (Authoritative)
- `pag_undefined = 0`
- `pag_header = 1` (database header page)
- `pag_pages = 2` (page inventory / PIP)
- `pag_transactions = 3` (transaction inventory / TIP)
- `pag_pointer = 4` (pointer page)
- `pag_data = 5` (data page)
- `pag_root = 6` (index root page)
- `pag_index = 7` (B‑tree index page)
- `pag_blob = 8` (blob page)
- `pag_ids = 9` (generator page)
- `pag_scns = 10` (SCN inventory)

## Basic Page Header Layout (Authoritative)
All page types begin with this header. Offsets are byte offsets from page start.

| Field | Offset | Size | Notes |
| --- | --- | --- | --- |
| `pag_type` | 0 | 1 | Page type (see above). |
| `pag_flags` | 1 | 1 | Bit flags (see below). |
| `pag_reserved` | 2 | 2 | Reserved, must be present. |
| `pag_generation` | 4 | 4 | Page generation. |
| `pag_scn` | 8 | 4 | Page SCN. |
| `pag_pageno` | 12 | 4 | Page number (for validation). |

`sizeof(pag) == 16`.

`pag_flags`:
- `crypted_page = 0x80` (page is encrypted on disk).

## Header Page Layout (Authoritative)
The header page is `pag_header` at page number 0. Offsets are byte offsets from page start.

| Field | Offset | Size |
| --- | --- | --- |
| `hdr_header` | 0 | 16 |
| `hdr_page_size` | 16 | 2 |
| `hdr_ods_version` | 18 | 2 |
| `hdr_ods_minor` | 20 | 2 |
| `hdr_flags` | 22 | 2 |
| `hdr_backup_mode` | 24 | 1 |
| `hdr_shutdown_mode` | 25 | 1 |
| `hdr_replica_mode` | 26 | 1 |
| `hdr_PAGES` | 28 | 4 |
| `hdr_page_buffers` | 32 | 4 |
| `hdr_end` | 36 | 2 |
| `hdr_next_transaction` | 40 | 8 |
| `hdr_oldest_transaction` | 48 | 8 |
| `hdr_oldest_active` | 56 | 8 |
| `hdr_oldest_snapshot` | 64 | 8 |
| `hdr_attachment_id` | 72 | 8 |
| `hdr_db_impl` | 80 | 4 |
| `hdr_guid` | 84 | 16 |
| `hdr_creation_date` | 100 | 8 |
| `hdr_shadow_count` | 108 | 4 |
| `hdr_crypt_page` | 112 | 4 |
| `hdr_crypt_plugin` | 116 | 32 |
| `hdr_data` | 148 | 1+ |

`sizeof(header_page) == 152`. `HDR_SIZE = 148` (offset of `hdr_data`).

`hdr_flags`:
- `hdr_active_shadow = 0x1`
- `hdr_force_write = 0x2`
- `hdr_crypt_process = 0x4`
- `hdr_no_reserve = 0x8`
- `hdr_SQL_dialect_3 = 0x10`
- `hdr_read_only = 0x20`
- `hdr_encrypted = 0x40`

`hdr_backup_mode`:
- `hdr_nbak_normal = 0`
- `hdr_nbak_stalled = 1`
- `hdr_nbak_merge = 2`
- `hdr_nbak_unknown = 255`

`hdr_shutdown_mode`:
- `hdr_shutdown_none = 0`
- `hdr_shutdown_multi = 1`
- `hdr_shutdown_single = 2`
- `hdr_shutdown_full = 3`

`hdr_replica_mode`:
- `hdr_replica_none = 0`
- `hdr_replica_read_only = 1`
- `hdr_replica_read_write = 2`

Header clumplets in `hdr_data` use the format `<type_byte> <length_byte> <data...>` and are terminated by `HDR_end = 0`.

## Generator Page Layout (Authoritative)
Generator pages use `pag_ids` and the `generator_page` structure.

| Field | Offset | Size | Notes |
| --- | --- | --- | --- |
| `gpg_header` | 0 | 16 | Base page header. |
| `gpg_sequence` | 16 | 4 | Generator page sequence number. |
| `gpg_dummy1` | 20 | 4 | Alignment padding. |
| `gpg_values` | 24 | 8 * N | `SINT64` generator values. |

`sizeof(generator_page) == 32`. `gpg_values` begins at offset 24.

`gens_per_page = (page_size - 24) / 8`.

## Index Root Page Layout (Authoritative)
Index root pages use `pag_root` and the `index_root_page` structure.

| Field | Offset | Size |
| --- | --- | --- |
| `irt_header` | 0 | 16 |
| `irt_relation` | 16 | 2 |
| `irt_count` | 18 | 2 |
| `irt_dummy` | 20 | 4 |
| `irt_rpt` | 24 | 24 * N |

`sizeof(index_root_page) == 48`.

`irt_repeat` layout (24 bytes each):
- `irt_transaction` (offset 0, 8 bytes)
- `irt_page_num` (offset 8, 4 bytes)
- `irt_page_space_id` (offset 12, 4 bytes)
- `irt_desc` (offset 16, 2 bytes)
- `irt_flags` (offset 18, 2 bytes)
- `irt_state` (offset 20, 1 byte)
- `irt_keys` (offset 21, 1 byte)
- `irt_dummy` (offset 22, 2 bytes)

`irt_flags` (must match index flags):
- `irt_unique = 1`
- `irt_descending = 2`
- `irt_foreign = 4`
- `irt_primary = 8`
- `irt_expression = 16`
- `irt_condition = 32`

`irt_state`:
- `irt_unused = 0`
- `irt_in_progress = 1`
- `irt_rollback = 2`
- `irt_normal = 3`
- `irt_kill = 4`
- `irt_commit = 5`
- `irt_drop = 6`

Key descriptors (`irtd`) are 8‑byte records: `irtd_field` (2), `irtd_itype` (2), `irtd_selectivity` (4).

## Record Headers and Format Constraints (Authoritative)
Record headers are stored in data pages. All record fragments and header+data lengths are rounded up to `ODS_ALIGNMENT`.

Record header structures and sizes:
- `rhd` (standard) header size `RHD_SIZE = 13` bytes (offset of `rhd_data`).
- `rhde` (extended transaction id) header size `RHDE_SIZE = 16` bytes.
- `rhdf` (fragment) header size `RHDF_SIZE = 24` bytes.

`rhd_flags` (also `rhdf_flags` and blob header flags):
- `rhd_deleted = 1`
- `rhd_chain = 2`
- `rhd_fragment = 4`
- `rhd_incomplete = 8`
- `rhd_blob = 16`
- `rhd_stream_blob = 32`
- `rhd_delta = 32` (same bit as `rhd_stream_blob`)
- `rhd_large = 64`
- `rhd_damaged = 128`
- `rhd_gc_active = 256`
- `rhd_uk_modified = 512`
- `rhd_long_tranum = 1024`
- `rhd_not_packed = 2048`

Record format constraints:
- `rhd_format` is an unsigned byte. `MAX_TABLE_VERSIONS = 255`.
- For any record fragment stored on a data page, the stored length must be `ROUNDUP(header_size + data_size + fill, ODS_ALIGNMENT)`.
- `rhd_format` must match the format version in `RDB$RELATIONS.RDB$FORMAT` for the relation.

## ODS Calculated Limits (Authoritative)
The following are derived from page size and must be computed exactly. All offsets are byte offsets of the referenced struct fields.

Constants:
- `BITS_PER_LONG = 32`
- `PPG_DP_BITS_NUM = 8`

Formulas:
- `bytesBitPIP(page_size) = page_size - offsetof(page_inv_page, pip_bits[0])`
- `pagesPerPIP(page_size) = bytesBitPIP(page_size) * 8`
- `pagesPerSCN(page_size) = pagesPerPIP(page_size) / BITS_PER_LONG`
- `maxPagesPerSCN(page_size) = (page_size - offsetof(scns_page, scn_pages[0])) / sizeof(ULONG)`
- `transPerTIP(page_size) = (page_size - offsetof(tx_inv_page, tip_transactions[0])) * 4`
- `gensPerPage(page_size) = (page_size - offsetof(generator_page, gpg_values[0])) / sizeof(SINT64)`
- `dataPagesPerPP(page_size) = ((page_size - offsetof(pointer_page, ppg_page[0])) * 8 / (BITS_PER_LONG + PPG_DP_BITS_NUM)) & (~7)`
- `maxRecsPerDP(page_size) = (page_size - sizeof(data_page)) / (sizeof(data_page::dpg_repeat) + offsetof(rhd, rhd_data[0]))`
- `maxIndices(page_size) = (page_size - offsetof(index_root_page, irt_rpt[0])) / (sizeof(index_root_page::irt_repeat) + sizeof(irtd))`

These functions must produce identical values to Firebird for every valid page size.
