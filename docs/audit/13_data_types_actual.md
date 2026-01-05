# Data Types (Actual Implementation)

Purpose: Code-verified description of the type system, storage formats, and cast behavior in the current implementation. Scope: DataType enum, TypedValue, heap tuple serialization, and related helpers.

Status: code review snapshot from static inspection; no runtime execution performed.

## If you only remember 5 things
- Heap tuple storage only supports a subset of DataType; many types exist only in TypedValue/encrypted form and are not stored in normal tables.
- DATE/TIME/TIMESTAMP are stored in Firebird formats (MJD + deci-ms) but are returned as strings; TypedValue uses int64 days/micros, so encrypted vs unencrypted representations differ.
- DECIMAL is stored as an 8-byte double in heap tuples; precision/scale metadata is recorded but not enforced, and DECIMAL string conversion is incomplete.
- BINARY/VARBINARY/BLOB are stored as length-prefixed bytes derived from value.toString(), which is not binary-safe; SELECT returns them as VARCHAR strings.
- Encrypted columns store a length-prefixed EncryptedValueRecord; plaintext is produced by TypedValue::serializePlainValue and supports many complex types (spatial, ranges, arrays, network).

## Scope and storage contexts (verified)
- Type definitions: DataType enum and TypeInfo metadata are in `ScratchBird/include/scratchbird/core/types.h`.
- TypeInfo carries precision, scale, array element type, and timezone flags, but enforcement is limited.
- Heap tuples: TupleHeader is 44 bytes, followed by optional null bitmap, then raw column payloads (host-endian, memcpy).
- Unencrypted heap tuple write/read paths are in `ScratchBird/src/sblr/executor.cpp` (INSERT serialization and deserializeTuple).
- UPDATE/MERGE reserialization uses serializeTupleFromValues, which only supports INT32, INT64, FLOAT64, and VARCHAR.
- Materialized view storage uses a separate serializer in the same file and falls back to string storage for unsupported types.
- Encrypted columns: appendEncryptedValue stores a 4-byte length prefix plus an EncryptedValueRecord (13 bytes, packed) and its IV/auth tag/ciphertext; ciphertext is per-type TypedValue::serializePlainValue.

## Casting and conversion rules (actual)
- TypedValue::convertTo only implements conversions among INT32, INT64, FLOAT32, FLOAT64, BOOLEAN, and TEXT/VARCHAR. All other targets throw.
- TypeSystem::isExplicitlyConvertible advertises broader numeric/string/binary/temporal/JSON conversions, but these are not implemented by convertTo.
- INSERT serialization performs additional ad hoc conversions: INT32 from INT32/INT64, INT64 from INT32/INT64/FLOAT64, BOOLEAN from BOOLEAN/INT32/INT64, DATE/TIME/TIMESTAMP parsed from strings or raw integers.
- Numeric expression evaluation uses coerceToDouble for INT32/INT64/FLOAT32/FLOAT64/DECIMAL (DECIMAL via std::stod(toString())).
- toString is incomplete for several types (DECIMAL, MONEY, UUID, XML, INT128, binary, spatial) and returns "<TYPE>" placeholders.

## Common on-disk conventions (unencrypted heap tuples)
- TupleHeader: 44 bytes; null bitmap uses 1 bit per column and no per-value payload for NULL.
- Fixed-width values are stored via memcpy with host endianness (no byte-order normalization).
- Variable-length values use a uint32 length prefix followed by raw bytes.

## Data type breakdown (actual)
### Core/meta
#### UNKNOWN
- Range: n/a (placeholder).
- Heap tuple: not supported.
- Encrypted plaintext: not supported.
- Casts (actual): self only; TEXT/VARCHAR via toString ("<UNKNOWN>").
- Notes: TypeInfo defaults to UNKNOWN.

#### NULL_TYPE
- Range: SQL NULL sentinel.
- Heap tuple: represented only in the null bitmap (no payload).
- Encrypted plaintext: serializePlainValue returns empty buffer.
- Casts (actual): self only; TEXT/VARCHAR yields "NULL".
- Notes: TypedValue::makeNull uses NULL_TYPE by default.

### Numeric types
#### INT8
- Range: -128 to 127.
- Heap tuple: only written by materialized view serializer (1 byte); normal INSERT/deserialize paths do not support INT8.
- Encrypted plaintext: 1 byte (appendUint8 of int8 value).
- Casts (actual): no convertTo support; makeInt8 is an alias of makeInt32 (type becomes INT32).
- Notes: no get/set for INT8; INT8 columns may not round-trip outside MV paths.

#### INT16
- Range: -32768 to 32767.
- Heap tuple: 2 bytes in normal INSERT; deserialize returns INT32 (Value::makeInt32).
- Encrypted plaintext: 2 bytes (int16).
- Casts (actual): no convertTo support; INSERT uses value.getInt32 and truncates to int16 without range checks.
- Notes: makeInt16 is an alias of makeInt32 (type becomes INT32).

#### INT32
- Range: -2147483648 to 2147483647.
- Heap tuple: 4 bytes; INSERT accepts INT32 or INT64 (cast).
- Encrypted plaintext: 4 bytes (int32).
- Casts (actual): to INT64, FLOAT32, FLOAT64, BOOLEAN, TEXT/VARCHAR; from INT64/FLOAT32/FLOAT64/BOOLEAN.
- Notes: comparisons and arithmetic use numeric coercion to double in executor.

#### INT64
- Range: -9223372036854775808 to 9223372036854775807.
- Heap tuple: 8 bytes; INSERT accepts INT64, INT32, or FLOAT64 (cast).
- Encrypted plaintext: 8 bytes (int64).
- Casts (actual): to INT32, FLOAT32, FLOAT64, BOOLEAN, TEXT/VARCHAR; from INT32/FLOAT32/FLOAT64/BOOLEAN.
- Notes: FLOAT64 to INT64 conversion rejects NaN/Inf and values >= 9.223372036854776e18.

#### INT128
- Range: -2^127 to 2^127 - 1 (GCC __int128).
- Heap tuple: not supported.
- Encrypted plaintext: 16 bytes (binary_data_ must be 16 bytes).
- Casts (actual): self only; TEXT/VARCHAR yields "<INT128>".
- Notes: no parser or arithmetic support; toString is placeholder.

#### UINT8
- Range: 0 to 255.
- Heap tuple: not supported.
- Encrypted plaintext: 1 byte.
- Casts (actual): self only; TEXT/VARCHAR via toString (numeric string).
- Notes: no insert/deserialize path for unsigned types.

#### UINT16
- Range: 0 to 65535.
- Heap tuple: not supported.
- Encrypted plaintext: 2 bytes.
- Casts (actual): self only; TEXT/VARCHAR via toString.
- Notes: no insert/deserialize path for unsigned types.

#### UINT32
- Range: 0 to 4294967295.
- Heap tuple: not supported.
- Encrypted plaintext: 4 bytes.
- Casts (actual): self only; TEXT/VARCHAR via toString.
- Notes: no insert/deserialize path for unsigned types.

#### UINT64
- Range: 0 to 18446744073709551615.
- Heap tuple: not supported.
- Encrypted plaintext: 8 bytes.
- Casts (actual): self only; TEXT/VARCHAR via toString.
- Notes: no insert/deserialize path for unsigned types.

#### FLOAT32
- Range: IEEE 754 single precision (approx 1.18e-38 to 3.4e38, plus NaN/Inf).
- Heap tuple: 4 bytes; INSERT uses value.toDouble() then casts to float.
- Deserialization: FLOAT32 columns are returned as FLOAT64 (Value::makeFloat64).
- Encrypted plaintext: 4 bytes (float).
- Casts (actual): to INT32, INT64, FLOAT64, BOOLEAN, TEXT/VARCHAR; from INT32/INT64/FLOAT64.
- Notes: stored via memcpy (host endianness).

#### FLOAT64
- Range: IEEE 754 double precision (approx 2.23e-308 to 1.80e308, plus NaN/Inf).
- Heap tuple: 8 bytes; INSERT uses value.toDouble().
- Deserialization: FLOAT64 columns are returned as FLOAT64.
- Encrypted plaintext: 8 bytes (double).
- Casts (actual): to INT32, INT64, FLOAT32, BOOLEAN, TEXT/VARCHAR; from INT32/INT64/FLOAT32.
- Notes: stored via memcpy (host endianness).

#### DECIMAL
- Range: nominal fixed-precision decimal; actual storage uses double.
- Heap tuple: 8 bytes double; INSERT uses value.toDouble(); deserialize returns FLOAT64.
- Encrypted plaintext: uint32 length + string bytes.
- Casts (actual): no convertTo support; coerceToDouble uses std::stod(toString()) which fails unless toString is overridden by the caller.
- Notes: toString lacks DECIMAL handling (returns "<DECIMAL>"); precision/scale metadata is recorded in catalog but not enforced. StatisticsManager expects a variable-length decimal encoding that does not match heap storage.

#### MONEY
- Range: int64 cents (implicit).
- Heap tuple: not supported.
- Encrypted plaintext: 8 bytes (int64).
- Casts (actual): self only; TEXT/VARCHAR yields "<MONEY>".
- Notes: conversion logic exists only in `type_conversions.cpp.disabled`.

### String types
#### CHAR
- Range: up to precision length in TypeInfo (not enforced).
- Heap tuple: uint32 length prefix + bytes; no padding/truncation; stored like VARCHAR.
- Deserialization: returned as VARCHAR (Value::makeVarchar).
- Encrypted plaintext: uint32 length + bytes.
- Casts (actual): self only; TEXT/VARCHAR via toString (string_data_).
- Notes: char semantics (space padding) are not implemented.

#### VARCHAR
- Range: up to precision length in TypeInfo (not enforced).
- Heap tuple: uint32 length prefix + bytes.
- Deserialization: returned as VARCHAR.
- Encrypted plaintext: uint32 length + bytes.
- Casts (actual): self only; TEXT/VARCHAR via toString.
- Notes: length constraints are not enforced at storage time.

#### TEXT
- Range: unlimited (practically uint32 length prefix).
- Heap tuple: uint32 length prefix + bytes.
- Deserialization: returned as VARCHAR.
- Encrypted plaintext: uint32 length + bytes.
- Casts (actual): self only; TEXT/VARCHAR via toString.

### Binary types
#### BINARY
- Range: fixed length (not enforced).
- Heap tuple: uint32 length prefix + bytes derived from value.toString().
- Deserialization: returned as VARCHAR (string).
- Encrypted plaintext: uint32 length + raw binary bytes (binary_data_).
- Casts (actual): self only; TEXT/VARCHAR via toString (returns "<BINARY>" for BINARY TypedValue).
- Notes: heap storage path is not binary-safe; values are typically passed as strings.

#### VARBINARY
- Range: variable length (not enforced).
- Heap tuple: same as BINARY.
- Deserialization: returned as VARCHAR.
- Encrypted plaintext: uint32 length + raw binary bytes.
- Casts (actual): self only; TEXT/VARCHAR via toString (returns "<VARBINARY>").
- Notes: stored via value.toString() in heap path.

#### BLOB
- Range: large binary (not enforced).
- Heap tuple: same as BINARY.
- Deserialization: returned as VARCHAR.
- Encrypted plaintext: uint32 length + raw binary bytes.
- Casts (actual): self only; TEXT/VARCHAR via toString (returns "<BLOB>").
- Notes: stored via value.toString() in heap path.

#### BYTEA
- Range: variable length.
- Heap tuple: not supported by insert/deserialize.
- Encrypted plaintext: uint32 length + raw binary bytes.
- Casts (actual): self only; TEXT/VARCHAR via toString (returns "<BYTEA>").
- Notes: StatisticsManager expects BYTEA length-prefixed storage, but heap serialization does not implement BYTEA.

### Temporal types
#### DATE
- Range: TypedValue uses int64 days since Unix epoch; heap tuples use int32 Modified Julian Date (days since 1858-11-17).
- Heap tuple: 4 bytes int32 MJD; INSERT accepts INT32 (assumed MJD) or string "YYYY-MM-DD".
- Deserialization: returned as VARCHAR string "YYYY-MM-DD".
- Encrypted plaintext: 8 bytes int64 (days since Unix epoch).
- Casts (actual): self only; TEXT/VARCHAR yields "DATE(<int64>)".
- Notes: storage and encrypted representations differ; statistics_manager assumes int32 days since epoch, not MJD.

#### TIME
- Range: TypedValue uses int64 microseconds since midnight; heap tuples use int32 deci-milliseconds (100 us) since midnight.
- Heap tuple: 4 bytes int32; INSERT accepts INT32 or string "HH:MM:SS" or "HH:MM:SS.nnnn" (4-digit fractional).
- Deserialization: returned as VARCHAR string.
- Encrypted plaintext: 8 bytes int64.
- Casts (actual): self only; TEXT/VARCHAR yields "TIME(<int64>)".
- Notes: no range validation; statistics_manager assumes int64 microseconds, which does not match heap storage.

#### TIMESTAMP
- Range: TypedValue uses int64 microseconds since Unix epoch; heap tuples use two int32 values (MJD date + deci-ms time).
- Heap tuple: 8 bytes total (int32 date + int32 time); INSERT accepts INT64 (microseconds, legacy) or string "YYYY-MM-DD HH:MM:SS(.nnnn)".
- Deserialization: returned as VARCHAR string.
- Encrypted plaintext: 8 bytes int64.
- Casts (actual): self only; TEXT/VARCHAR yields "TIMESTAMP(<int64>)".
- Notes: with_timezone metadata exists but is not applied; statistics_manager assumes int64 microseconds, not MJD/deci-ms.

#### INTERVAL
- Range: months int32, days int32, microseconds int64.
- Heap tuple: not supported.
- Encrypted plaintext: 16 bytes (int32 months + int32 days + int64 microseconds).
- Casts (actual): self only; TEXT/VARCHAR yields "<INTERVAL>".
- Notes: interval arithmetic exists in executor for extract/format, but no storage path.

### Boolean
#### BOOLEAN
- Range: true/false.
- Heap tuple: 1 byte (uint8 0/1); INSERT accepts BOOLEAN, INT32, INT64 (non-zero true).
- Deserialization: returned as BOOLEAN.
- Encrypted plaintext: 1 byte (0/1).
- Casts (actual): to INT32, INT64, TEXT/VARCHAR; from INT32/INT64/FLOAT32/FLOAT64.
- Notes: no BOOLEAN to FLOAT cast; convertTo only supports numeric to BOOLEAN.

### Special types
#### UUID
- Range: 16-byte UUID.
- Heap tuple: not supported.
- Encrypted plaintext: 16 bytes (binary_data_ must be 16 bytes).
- Casts (actual): self only; TEXT/VARCHAR yields "<UUID>".
- Notes: executor includes UUID field extraction helpers, but toString lacks UUID formatting.

#### JSON
- Range: UTF-8 JSON text (not validated on storage).
- Heap tuple: not supported.
- Encrypted plaintext: uint32 length + bytes.
- Casts (actual): self only; TEXT/VARCHAR yields string_data_.
- Notes: executor JSON functions operate on string data using nlohmann::json.

#### JSONB
- Range: JSON text stored as string (not binary).
- Heap tuple: not supported.
- Encrypted plaintext: uint32 length + bytes.
- Casts (actual): self only; TEXT/VARCHAR yields string_data_.
- Notes: JSONB is not stored in a binary format despite the name.

#### XML
- Range: XML text.
- Heap tuple: not supported.
- Encrypted plaintext: uint32 length + bytes.
- Casts (actual): self only; TEXT/VARCHAR yields "<XML>".
- Notes: XML functions use libxml2 when available; no storage path.

#### VECTOR
- Range: vector of float32 values (dimensions recorded in catalog precision).
- Heap tuple: not supported.
- Encrypted plaintext: uint32 length + raw binary_data_ bytes (float32 array as stored by makeVector).
- Casts (actual): self only; TEXT/VARCHAR yields "<VECTOR>".
- Notes: core::Vector::encode uses type byte + dimension count but TypedValue::makeVector stores raw float32 bytes; formats are not unified.

### Spatial types
#### POINT
- Range: two doubles plus SRID.
- Heap tuple: not supported.
- Encrypted plaintext: 4 bytes SRID + 8 bytes X + 8 bytes Y (20 bytes total).
- Casts (actual): self only; TEXT/VARCHAR yields "<POINT>".
- Notes: WKT/WKB parsing exists; storage is only via encryption serialization.

#### LINESTRING
- Range: list of points plus SRID.
- Heap tuple: not supported.
- Encrypted plaintext: 4 bytes SRID + 4 bytes point count + 20 bytes per point.
- Casts (actual): self only; TEXT/VARCHAR yields "<LINESTRING>".

#### POLYGON
- Range: list of rings (each ring is list of points) plus SRID.
- Heap tuple: not supported.
- Encrypted plaintext: 4 bytes SRID + 4 bytes ring count + each ring as point list (4 bytes count + 20 bytes per point).
- Casts (actual): self only; TEXT/VARCHAR yields "<POLYGON>".

#### MULTIPOINT
- Range: list of points plus SRID.
- Heap tuple: not supported.
- Encrypted plaintext: 4 bytes SRID + 4 bytes point count + 20 bytes per point.
- Casts (actual): self only; TEXT/VARCHAR yields "<MULTIPOINT>".

#### MULTILINESTRING
- Range: list of LineString plus SRID.
- Heap tuple: not supported.
- Encrypted plaintext: 4 bytes SRID + 4 bytes line count + for each line: 4 bytes SRID + point list (4 bytes count + 20 bytes per point).
- Casts (actual): self only; TEXT/VARCHAR yields "<MULTILINESTRING>".

#### MULTIPOLYGON
- Range: list of Polygon plus SRID.
- Heap tuple: not supported.
- Encrypted plaintext: 4 bytes SRID + 4 bytes polygon count + each polygon as in POLYGON serialization.
- Casts (actual): self only; TEXT/VARCHAR yields "<MULTIPOLYGON>".

#### GEOMETRYCOLLECTION
- Range: heterogeneous list of geometries plus SRID.
- Heap tuple: not supported.
- Encrypted plaintext: 4 bytes SRID + 4 bytes geometry count + for each geometry: 1 byte null flag, 2 bytes type, 4 bytes length, then nested geometry payload.
- Casts (actual): self only; TEXT/VARCHAR yields "<GEOMETRYCOLLECTION>".

### Array and composite types
#### ARRAY
- Range: list of TypedValue elements.
- Heap tuple: not supported.
- Encrypted plaintext: uint32 element count, then for each element: 1 byte null flag, 2 bytes DataType, 4 bytes payload length, payload bytes.
- Casts (actual): self only; TEXT/VARCHAR yields "<ARRAY>".
- Notes: element type is tracked in TypeInfo but not enforced in serialization.

#### COMPOSITE
- Range: list of field names and values.
- Heap tuple: not supported.
- Encrypted plaintext: uint32 length + field name string (names separated by '\0'), then ARRAY-style value list.
- Casts (actual): self only; TEXT/VARCHAR yields "<COMPOSITE>".

#### VARIANT
- Range: tagged union stored as a vector of TypedValue.
- Heap tuple: not supported.
- Encrypted plaintext: same format as ARRAY.
- Casts (actual): self only; TEXT/VARCHAR yields "<VARIANT>".
- Notes: makeVariant(value) stores a single element; makeVariant(type, value) stores INT32 type id then value.

### Text search types
#### TSVECTOR
- Range: list of lexemes with positions and weights.
- Heap tuple: not supported.
- Encrypted plaintext: TSVector::toBinary format: 4-byte lexeme count, then for each lexeme: 2-byte word length, word bytes, 2-byte position count, then per position: 2-byte position + 1-byte weight.
- Casts (actual): self only; TEXT/VARCHAR yields TSVector::toString() if tsvector present.
- Notes: binary format is PostgreSQL-compatible.

#### TSQUERY
- Range: boolean expression over lexemes.
- Heap tuple: not supported.
- Encrypted plaintext: prefix tree encoding: 1-byte node type; for LEXEME nodes 2-byte term length + term bytes; for PHRASE nodes 2-byte distance; children serialized recursively.
- Casts (actual): self only; TEXT/VARCHAR yields TSQuery::toString() if tsquery present.

### Range types
#### INT4RANGE
- Range: int32 bounds with inclusive/exclusive flags.
- Heap tuple: not supported.
- Encrypted plaintext: 1-byte flags, then optional lower/upper int32 if bounded.
- Casts (actual): self only; TEXT/VARCHAR yields Range<int32>::toString() if present.

#### INT8RANGE
- Range: int64 bounds.
- Heap tuple: not supported.
- Encrypted plaintext: 1-byte flags, then optional lower/upper int64.
- Casts (actual): self only; TEXT/VARCHAR yields Range<int64>::toString() if present.

#### NUMRANGE
- Range: double bounds.
- Heap tuple: not supported.
- Encrypted plaintext: 1-byte flags, then optional lower/upper double.
- Casts (actual): self only; TEXT/VARCHAR yields Range<int64>::toString() (TypedValue::toString uses int64 range).
- Notes: toString path does not format double bounds.

#### TSRANGE
- Range: int64 bounds (timestamp values).
- Heap tuple: not supported.
- Encrypted plaintext: 1-byte flags, then optional lower/upper int64.
- Casts (actual): self only; TEXT/VARCHAR yields Range<int64>::toString() if present.

#### TSTZRANGE
- Range: int64 bounds (timestamp with timezone values).
- Heap tuple: not supported.
- Encrypted plaintext: same as TSRANGE.
- Casts (actual): self only; TEXT/VARCHAR yields Range<int64>::toString() if present.

#### DATERANGE
- Range: int64 bounds (date values).
- Heap tuple: not supported.
- Encrypted plaintext: same as INT8RANGE (flags + int64 bounds).
- Casts (actual): self only; TEXT/VARCHAR yields Range<int64>::toString() if present.

### Network types
#### INET
- Range: IPv4/IPv6 address with netmask.
- Heap tuple: not supported.
- Encrypted plaintext: 1 byte address family + 1 byte netmask + address bytes (4 or 16).
- Casts (actual): self only; TEXT/VARCHAR yields InetAddr::toString().

#### CIDR
- Range: IPv4/IPv6 network (host bits zeroed).
- Heap tuple: not supported.
- Encrypted plaintext: same as INET (family + netmask + address bytes).
- Casts (actual): self only; TEXT/VARCHAR yields Cidr::toString().

#### MACADDR
- Range: 6-byte MAC address.
- Heap tuple: not supported.
- Encrypted plaintext: 6 bytes.
- Casts (actual): self only; TEXT/VARCHAR yields MacAddr::toString().

#### MACADDR8
- Range: 8-byte MAC address.
- Heap tuple: not supported.
- Encrypted plaintext: 8 bytes.
- Casts (actual): self only; TEXT/VARCHAR yields MacAddr8::toString().

## Internal mismatches and risks (observed)
- statistics_manager.cpp assumes DATE/TIME/TIMESTAMP are stored as int32/int64 Unix-based values; actual heap tuples use Firebird MJD and deci-ms formats.
- statistics_manager.cpp assumes DECIMAL uses a variable-length encoding with precision/scale bytes; actual heap tuples store DECIMAL as double.
- statistics_manager.cpp assumes BYTEA is length-prefixed; heap serializer does not support BYTEA.
- FLOAT32 and DECIMAL values are deserialized into FLOAT64, which can mask column type differences.
- UPDATE/MERGE serialization only supports INT32/INT64/FLOAT64/VARCHAR, which can fail for other stored types.

## Evidence map (code)
| Path | Elements | Notes |
| --- | --- | --- |
| ScratchBird/include/scratchbird/core/types.h | DataType, TypeInfo, Interval | Type system definitions |
| ScratchBird/include/scratchbird/core/typed_value.h | TypedValue API | Runtime storage and conversion |
| ScratchBird/src/core/typed_value.cpp | serializePlainValue, convertTo, make* | Plaintext formats and cast rules |
| ScratchBird/src/sblr/executor.cpp | INSERT serialization, deserializeTuple, serializeTupleFromValues, coerceToDouble | Heap tuple formats and conversions |
| ScratchBird/include/scratchbird/core/heap_page.h | TupleHeader, EncryptedValueRecord | On-disk tuple header and encryption record |
| ScratchBird/include/scratchbird/core/network.h | InetAddr, Cidr, MacAddr | Network type storage sizes |
| ScratchBird/include/scratchbird/core/tsvector.h | TSVector | Text search types |
| ScratchBird/src/core/tsvector.cpp | TSVector::toBinary | Binary format |
| ScratchBird/include/scratchbird/core/tsquery.h | TSQuery | Text search query type |
| ScratchBird/src/core/tsquery.cpp | serializeNode, deserializeNode | TSQuery binary format |
| ScratchBird/include/scratchbird/core/vector.h | VectorValue, Vector | Vector utilities |
| ScratchBird/src/core/vector.cpp | Vector::encode | Vector binary encoding (not used by TypedValue) |
| ScratchBird/src/optimizer/statistics_manager.cpp | tuple scanning assumptions | Divergent type formats |
| ScratchBird/src/core/type_system.cpp | TypeSystem::isExplicitlyConvertible | Declared conversion matrix |

## Verification gaps
- No runtime validation; actual storage/conversion behavior may differ if other execution paths (e.g., compiler V2) override these serializers.
