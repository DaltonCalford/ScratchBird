# DateTime and UUID Storage Format in ScratchBird

Date: 2025-12-31
Updated: 2026-01-09
Focus: On-disk storage format for temporal and UUID data types

---

## Overview

ScratchBird stores date/time and UUID values as binary payloads in record storage. The
current on-disk layout is defined by `TypedValue::serializePlainValue` and
`TypedValue::deserializePlainValue`.

Primary source files:
- `ScratchBird/src/core/typed_value.cpp`
- `ScratchBird/include/scratchbird/core/types.h`

---

## Date/Time Storage (On-Disk)

### 1. DATE

Storage format:
- `int32` MJD (Modified Julian Day)
- `int32` timezone_offset_seconds
- Total size: 8 bytes

Representation:
- MJD is days since 1858-11-17.
- `Stored_MJD = days_since_unix_epoch + FirebirdDateTime::UNIX_EPOCH_MJD`.

Example:
- `1970-01-01` -> `Stored_MJD = FirebirdDateTime::UNIX_EPOCH_MJD`.

Source:
- `ScratchBird/src/core/typed_value.cpp:3550`
- `ScratchBird/src/core/typed_value.cpp:4226`

---

### 2. TIME

Storage format:
- `int64` microseconds since midnight (UTC-normalized)
- `int32` timezone_offset_seconds
- Total size: 12 bytes

Notes:
- `timezone_offset_seconds` preserves the original input offset.

Source:
- `ScratchBird/src/core/typed_value.cpp:3564`
- `ScratchBird/src/core/typed_value.cpp:4240`

---

### 3. TIMESTAMP

Storage format:
- `int64` microseconds since Unix epoch (UTC)
- `int32` timezone_offset_seconds
- Total size: 12 bytes

Notes:
- `timezone_offset_seconds` preserves the original input offset.
- TypedValue serialization does not persist a timezone name or hint.

Source:
- `ScratchBird/src/core/typed_value.cpp:3571`
- `ScratchBird/src/core/typed_value.cpp:4254`

---

## UUID Storage

### UUID (RFC 4122 format)

Storage format:
- Raw 16-byte payload
- No textual conversion

Source:
- `ScratchBird/src/core/typed_value.cpp:3578`
- `ScratchBird/src/core/typed_value.cpp:4280`
- `ScratchBird/include/scratchbird/core/uuidv7.h`

---

## Storage Size Summary

| Type | Disk Size | Format |
|------|-----------|--------|
| DATE | 8 bytes | int32 MJD + int32 offset_seconds |
| TIME | 12 bytes | int64 micros + int32 offset_seconds |
| TIMESTAMP | 12 bytes | int64 micros + int32 offset_seconds |
| UUID | 16 bytes | raw 16-byte payload |

---

## Wire Protocol Format (ScratchBird Native)

Wire formats are defined separately from on-disk storage.

Source:
- `ScratchBird/include/scratchbird/protocol/wire_protocol.h`

Wire type layouts:
- DATE: int32 days since 2000-01-01
- TIME: int64 microseconds since midnight
- TIMESTAMP: int64 microseconds since Unix epoch
- TIMESTAMPTZ: int64 microseconds since epoch + int16 timezone offset
- UUID: 16 bytes

Note: Wire formats are not identical to on-disk formats. Use the wire protocol
spec when implementing client/server encoding.

---

## References

- `ScratchBird/include/scratchbird/core/types.h`
- `ScratchBird/include/scratchbird/core/typed_value.h`
- `ScratchBird/src/core/typed_value.cpp`
- `ScratchBird/include/scratchbird/core/uuidv7.h`
- `ScratchBird/src/core/uuidv7.cpp`
- `ScratchBird/include/scratchbird/protocol/wire_protocol.h`
- `ScratchBird/docs/specifications/types/DATA_TYPE_PERSISTENCE_AND_CASTS.md`
- `ScratchBird/docs/specifications/types/TIMEZONE_SYSTEM_CATALOG.md`
- `ScratchBird/docs/specifications/types/UUID_IDENTITY_COLUMNS.md`
- `ScratchBird/docs/specifications/api/CLIENT_LIBRARY_API_SPECIFICATION.md`

---

END OF DOCUMENT
