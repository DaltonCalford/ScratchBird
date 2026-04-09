# Catalog: Large Object Tables

## Purpose
Define canonical catalog tables for large object storage metadata.

## Conventions
- All columns use catalog domains defined in `CATALOG_SYSTEM_DOMAINS.md`.
- `*_uuid` columns use `[sb_dom]cat_<name>_uuid`.

## Table: `lob`
Columns:
- `lob_uuid` `[sb_dom]cat_lob_uuid` PK
- `database_uuid` `[sb_dom]cat_database_uuid`
- `owner_uuid` `[sb_dom]cat_owner_uuid`
- `data_length` `[sb_dom]cat_bytes_u64`
- `page_count` `[sb_dom]cat_count_u64`
- `checksum` `[sb_dom]cat_hash32` nullable
- `is_encrypted` `[sb_dom]cat_bool`
- `encryption_key_uuid` `[sb_dom]cat_encryption_key_uuid` nullable
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- `encryption_key_uuid` MUST be non-null when `is_encrypted=true`.

## Table: `lob_page`
Columns:
- `lob_page_uuid` `[sb_dom]cat_lob_page_uuid` PK
- `lob_uuid` `[sb_dom]cat_lob_uuid`
- `page_index` `[sb_dom]cat_uint32`
- `page_gpid` `[sb_dom]cat_uint64`
- `chunk_bytes` `[sb_dom]cat_uint32`
- `checksum` `[sb_dom]cat_hash32` nullable
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`lob_uuid`, `page_index`)

Notes:
- `page_gpid` stores the global page identifier as a 64-bit integer.
- `chunk_bytes` MUST be <= (page_size - page_header_bytes).
