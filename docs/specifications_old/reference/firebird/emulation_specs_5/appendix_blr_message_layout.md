# Appendix: BLR Message Layout and rem_fmt (Authoritative)

This appendix defines exactly how BLR message formats are parsed into `rem_fmt` structures and how message buffers are laid out in memory for wire encoding.

## 1) BLR Message Envelope
A BLR message definition used on the wire has the form:
- `blr_begin`
- `blr_message`
- `message_number` (1 byte)
- `count` (2 bytes, little-endian)
- `count` field descriptors, each starting with a BLR type code and type-specific parameters.

If `blr_begin` or `blr_message` is missing, parsing fails.

## 2) Descriptor Parsing (Authoritative)
Descriptors are parsed in order. For each descriptor:

- `desc->dsc_dtype` and `desc->dsc_length` are assigned based on BLR type.
- `desc->dsc_scale` and `desc->dsc_sub_type` are assigned if present in the BLR.
- The field is aligned to `type_alignments[dsc_dtype]` before assigning `desc->dsc_address`.
- `desc->dsc_address` is the byte offset within the message buffer.

### 2.1 BLR Type Mapping
The following BLR types are accepted in BLR message definitions:

- `blr_text`:
  - length: 2 bytes little-endian
  - dtype: `dtype_text`
  - dsc_length = length
- `blr_varying`:
  - length: 2 bytes little-endian
  - dtype: `dtype_varying`
  - dsc_length = length + 2 (includes 2-byte vary length)
- `blr_cstring`:
  - length: 2 bytes little-endian
  - dtype: `dtype_cstring`
  - dsc_length = length

- `blr_text2`:
  - scale: 2 bytes little-endian
  - length: 2 bytes little-endian
  - dtype: `dtype_text`
  - dsc_length = length
- `blr_varying2`:
  - scale: 2 bytes little-endian
  - length: 2 bytes little-endian
  - dtype: `dtype_varying`
  - dsc_length = length + 2
- `blr_cstring2`:
  - scale: 2 bytes little-endian
  - length: 2 bytes little-endian
  - dtype: `dtype_cstring`
  - dsc_length = length

- `blr_short`:
  - scale: 1 byte
  - dtype: `dtype_short`
  - dsc_length = 2
- `blr_long`:
  - scale: 1 byte
  - dtype: `dtype_long`
  - dsc_length = 4
- `blr_int64`:
  - scale: 1 byte
  - dtype: `dtype_int64`
  - dsc_length = 8
- `blr_quad`:
  - scale: 1 byte
  - dtype: `dtype_quad`
  - dsc_length = 8
  - this is treated as a blob id; index added to `fmt_blob_idx`
- `blr_float`:
  - dtype: `dtype_real`
  - dsc_length = 4
- `blr_double`, `blr_d_float`:
  - dtype: `dtype_double`
  - dsc_length = 8
- `blr_dec64`:
  - dtype: `dtype_dec64`
  - dsc_length = 8
- `blr_dec128`:
  - dtype: `dtype_dec128`
  - dsc_length = 16
- `blr_int128`:
  - scale: 1 byte
  - dtype: `dtype_int128`
  - dsc_length = 16

- `blr_blob2`:
  - sub_type: 2 bytes little-endian
  - text type: 2 bytes little-endian
  - dtype: `dtype_blob`
  - dsc_length = 8
  - index added to `fmt_blob_idx`

- `blr_timestamp`:
  - dtype: `dtype_timestamp`
  - dsc_length = 8
- `blr_timestamp_tz`:
  - dtype: `dtype_timestamp_tz`
  - dsc_length = 12
- `blr_ex_timestamp_tz`:
  - dtype: `dtype_ex_timestamp_tz`
  - dsc_length = 12

- `blr_sql_date`:
  - dtype: `dtype_sql_date`
  - dsc_length = 4
- `blr_sql_time`:
  - dtype: `dtype_sql_time`
  - dsc_length = 4
- `blr_sql_time_tz`:
  - dtype: `dtype_sql_time_tz`
  - dsc_length = 8
- `blr_ex_time_tz`:
  - dtype: `dtype_ex_time_tz`
  - dsc_length = 8

- `blr_bool`:
  - dtype: `dtype_boolean`
  - dsc_length = 1

Any unknown BLR type in a message definition is a parsing error.

### 2.2 Alignment Rules (Authoritative)
Before placing a field, the offset is aligned to the type’s alignment (bytes). After alignment, `desc->dsc_address` is set to that offset and the offset is incremented by `dsc_length`.

Alignment table (bytes):
- `dtype_text` = 0
- `dtype_cstring` = 0
- `dtype_varying` = 2
- `dtype_packed` = 1
- `dtype_byte` = 1
- `dtype_short` = 2
- `dtype_long` = 4
- `dtype_quad` = 4
- `dtype_real` = 4
- `dtype_double` = 8
- `dtype_d_float` = 8
- `dtype_sql_date` = 4
- `dtype_sql_time` = 4
- `dtype_timestamp` = 4
- `dtype_blob` = 4
- `dtype_array` = 4
- `dtype_int64` = 8
- `dtype_dbkey` = 4
- `dtype_boolean` = 1
- `dtype_dec64` = 8
- `dtype_dec128` = 16
- `dtype_int128` = 8
- `dtype_sql_time_tz` = 4
- `dtype_timestamp_tz` = 4
- `dtype_ex_time_tz` = 4
- `dtype_ex_timestamp_tz` = 4

### 2.3 `fmt_length` and `fmt_net_length`
- `fmt_length` is the final aligned offset after placing all descriptors.
- `fmt_net_length` is the encoded network length:
  - if `dtype_varying`: `4 + ((dsc_length - 2 + 3) & ~3)`
  - otherwise: `(dsc_length + 3) & ~3`
  - `fmt_net_length` is the sum over all descriptors in order.

## 3) Wire Encoding Impact
The resulting `rem_fmt` drives:
- `xdr_message` (unpacked) encoding order and offsets
- `xdr_packed_message` null bitmap and packed values
- alignment and buffer size on both client and server

These rules are authoritative and must be used when interpreting BLR message definitions on the wire.
