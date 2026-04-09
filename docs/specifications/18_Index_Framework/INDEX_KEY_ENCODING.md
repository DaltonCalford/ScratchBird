# Index Key Encoding

## Purpose
Define canonical key encoding used by all index types.

## Dependencies
- 14_Base_Scalar_Types
- 13_Operator_Model_and_Coercion

## Key Byte Format
Key bytes are a concatenation of encoded segments, one per indexed key column.

## Descending Sort Mode
- `desc_sort_mode` is an index-level setting stored in `index.desc_sort_mode`.
- Modes:
  - `invert_compare` (default): store bytes as-is and invert comparison results.
  - `bytewise_complement`: store complemented sort bytes for DESC segments.

### Segment Encoding
Each segment is encoded as:
- `null_sort_byte` (u8)
- `seg_flags` (u8)
- `seg_len` (u16)
- `seg_bytes` (seg_len bytes)

### Segment Flags
- `SEG_NULL` = 0x01
- `SEG_TRUNCATED` = 0x02
- `SEG_COLLATION_KEY` = 0x04
- `SEG_DESC` = 0x08

## Null Ordering
- If the value is NULL:
- `null_sort_byte` = 0x00 for NULLS FIRST.
- `null_sort_byte` = 0xFF for NULLS LAST.
- `seg_flags` includes `SEG_NULL`.
- `seg_len` = 0 and `seg_bytes` is empty.
- If the value is NOT NULL:
- `null_sort_byte` = 0x80.
- `seg_flags` does not include `SEG_NULL`.

## Value Encoding
- Non-NULL values are encoded using canonical byte encoding from section 14.
- For text types, encode using the deterministic collation key for the specified collation.
- Nondeterministic collations are not indexable in Alpha; parser must reject index creation.
- If `seg_len` exceeds 65535 bytes, index creation fails with explicit error.

## Prefix Indexes (MySQL-style)
- If `prefix_length` is specified:
- Encode the value bytes.
- Truncate to prefix length in bytes without splitting multibyte characters.
- Set `SEG_TRUNCATED`.
- `seg_len` is the truncated byte length.

## Descending Order
If `sort_order=DESC`:
1. Set `SEG_DESC`.
2. Apply behavior based on `desc_sort_mode`:
   - `invert_compare`:
     - Store bytes unchanged.
     - Comparator reverses the comparison result for `seg_bytes` and `seg_len`.
     - `null_sort_byte` ordering is preserved to honor explicit `null_order`.
   - `bytewise_complement`:
     - For NULL values:
       - Use `null_sort_byte=0xFF` for NULLS FIRST, `null_sort_byte=0x00` for NULLS LAST.
     - For non-NULL values:
       - Use `null_sort_byte=0x80`.
     - Complement `null_sort_byte` and every byte of `seg_bytes` (bitwise NOT).
     - `seg_flags` and `seg_len` are stored unmodified.
     - Comparator uses ascending byte comparison of `null_sort_byte` and `seg_bytes` (no inversion).

## Composite Key Comparison
- Keys are compared segment-by-segment using the encoded segment boundaries.
- For each segment:
  - Compare `null_sort_byte`.
  - If equal, compare `seg_bytes` lexicographically.
  - If all bytes equal, shorter `seg_len` sorts before longer `seg_len`.
- If `SEG_DESC` is set:
  - `invert_compare` mode: invert the comparison result for `seg_bytes` and `seg_len`.
  - `bytewise_complement` mode: no inversion; bytes are already complemented.
- Segment ordering is significant; each segment encodes its own null ordering and sort order.

## Collation and Encoding Notes
- `SEG_COLLATION_KEY` is set when the collation key is used instead of raw text bytes.
- Collation keys must be stable across database versions; changing collation requires index rebuild.

## Open Decisions
- None. If segment encoding changes, all index types and catalogs must be updated.
