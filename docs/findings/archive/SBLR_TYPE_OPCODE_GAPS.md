# SBLR Type/Literal Opcode Coverage Gaps (Alpha)

Status: Complete (implementation); tests pending
Last Updated: 2026-02-02

## Purpose
Document SBLR type-marker and literal-opcode gaps that prevent full datatype
coverage in the executor, with emphasis on emulated-engine requirements.

## Inputs
- `include/scratchbird/core/types.h` (canonical DataType enum)
- `include/scratchbird/sblr/opcodes.h` (SBLR opcode registry)
- `src/sblr/executor.cpp` (type decode + literal parsing)

## Summary
- **Resolved:** SBLR type markers now cover all DataTypes, including emulated
  engine parity types such as `DECFLOAT(16/34)`.
- **Resolved:** Typed literal opcodes are assigned for boolean/date/time/uuid/
  decimal/binary/JSON/XML/network families.
- **Resolved:** Timezone-aware temporal markers are encoded via extended type
  opcodes for TIME/TIMESTAMP WITH TIME ZONE.

## Findings

### F-SBLR-001 - Missing SBLR type markers for defined DataTypes
**Status:** Resolved
**Evidence**
- DataType enum includes: `UINT8/UINT16/UINT32/UINT64`, `MONEY`, `INTERVAL`,
  `JSONB`, `XML`, `MULTIPOINT/MULTILINESTRING/MULTIPOLYGON/GEOMETRYCOLLECTION`,
  `COMPOSITE`, `VARIANT`, `INET/CIDR/MACADDR/MACADDR8`:
  `include/scratchbird/core/types.h:35-105`.
- SBLR base type markers only cover signed ints, float, boolean, string, basic
  date/time, binary, UUID, DECIMAL, JSON, ARRAY, DOMAIN:
  `include/scratchbird/sblr/opcodes.h:57-187`.
- Extended type markers only cover INT128/UINT128, VECTOR, POINT/LINESTRING/
  POLYGON, TSVECTOR/TSQUERY, and range types:
  `include/scratchbird/sblr/opcodes.h:354-505`, `include/scratchbird/sblr/opcodes.h:1062-1074`.
- Executor decode only maps those base/extended markers:
  `src/sblr/executor.cpp:194-295`.

**Impact**
SBLR cannot represent these DataTypes, so they cannot be created, cast,
or used in schema/PSQL with full bytecode execution.

**Resolution**
- Implemented extended type markers for all listed DataTypes in
  `include/scratchbird/sblr/opcodes.h`, with executor decode coverage in
  `src/sblr/executor.cpp`.

### F-SBLR-002 - Missing typed literal opcodes
**Status:** Resolved
**Evidence**
- Literal opcodes exist only for NULL, INT32, INT64, DOUBLE, STRING, CHARSET,
  COLLATION: `include/scratchbird/sblr/opcodes.h:75-82`.
- Executor literal parsing only handles these forms:
  `src/sblr/executor.cpp:3877-3899`.

**Impact**
SBLR cannot encode literals for most SQL types (date/time/uuid/decimal/json/etc.),
forcing string + CAST or preventing literal emission entirely.

**Resolution**
- Added base and extended literal opcodes in `include/scratchbird/sblr/opcodes.h`
  with executor support in `src/sblr/executor.cpp`.

### F-SBLR-003 - Time zone modifiers not encoded in SBLR type stream
**Status:** Resolved
**Evidence**
- TypeInfo exposes `with_timezone`/`timezone_hint`:
  `include/scratchbird/core/types.h:124-129`.
- SBLR type decoding ignores any timezone modifier and only uses opcodes +
  precision/scale: `src/sblr/executor.cpp:3384-3518`.

**Impact**
`TIME/TIMESTAMP WITH TIME ZONE` cannot be expressed in SBLR, blocking
Firebird and PostgreSQL parity for timezone-aware temporal types.

### F-SBLR-004 - Executor supports JSONB/Multi-geometry operations without type markers
**Status:** Resolved
**Evidence**
- Executor contains JSONB and multi-geometry runtime handling:
  `src/sblr/executor.cpp:1401`, `src/sblr/executor.cpp:28054-28301`.
- No SBLR type marker exists for JSONB or multi-geometry types:
  `include/scratchbird/sblr/opcodes.h:57-187`, `include/scratchbird/sblr/opcodes.h:354-505`.

**Impact**
These capabilities are unreachable from normal schema definitions and
typed expression emission because the bytecode cannot declare them.

## Emulated-Engine Coverage Pressure
These coverage blockers are addressed by the SBLR opcode additions above.
