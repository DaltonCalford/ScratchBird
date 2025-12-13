# TODO: 128-bit Numeric Support (UINT128, FLOAT128)

## Goal
Add unsigned 128-bit integers and quad-precision floats to the ScratchBird type system and execution stack, with consistent storage, wire, and index semantics.
**Scope constraint:** ScratchBird-only. Emulated engines must not surface capabilities beyond their native support; adapters may only map to compatible fallback types (e.g., DECIMAL/text) without extending emulated behavior.

## Scope
- Types: `UINT128`, `FLOAT128` (quad/long double or libquadmath-backed).
- Affects: type system, parser/AST, evaluator, storage/serialization, indexes, wire adapters, catalogs, tests.

## Work Items
1) **Type System & Catalog**
   - Add `UINT128`, `FLOAT128` to `core::DataType`; propagate through `TypeInfo`, catalog persistence, column definitions, SBLR opcodes if needed.
   - Decide canonical on-disk format: fixed 16-byte little-endian for UINT128; for FLOAT128, choose binary quad (if stable) or text/decimal encoding with versioning.

2) **Parser/AST**
   - Literal parsing/validation for uint128 range; quad-float literals with high precision.
   - Update casts/resolution to include new types.

3) **Evaluator/Operators**
   - Arithmetic/comparison/aggregates for UINT128 using `unsigned __int128` (feature-detect in CMake; provide fallback/error if unsupported).
   - Quad-float arithmetic/comparison/aggregates; require `long double` (128) or libquadmath; add CMake feature flag.
   - Casting matrix to/from existing numerics, text, decimal, and JSON.
   - Hashing and equality for both types.

4) **Storage/Serialization**
   - Tuple storage/read, TOAST handling, and network/wire serialization.
   - SBLR bytecode literal encoding/decoding for new types.
   - Endianness handling for 16-byte numerics.

5) **Indexing**
   - B-tree/hash comparator key extraction for UINT128 and FLOAT128; ensure deterministic ordering (define NaN/signaling rules for FLOAT128).
   - Update any SIMD/SIMD-less compare paths.

6) **Wire/Adapters**
   - Map to client-visible types per dialect: ScratchBird wire can expose native types; emulated adapters must use compatible fallbacks (e.g., DECIMAL/NUMERIC/text) and must not add non-native capabilities.

7) **Tests**
   - Unit: arithmetic, comparison, casts, hashing, serialization, index ordering.
   - Integration: DDL with columns of new types, insert/select/compare, index scans.
   - Cross-platform guardrails: skip/feature-flag when compiler/lib support is absent.

8) **Build/Config**
   - CMake feature detection: `unsigned __int128` availability; quad support via `long double` width or libquadmath; add optional dependency toggles.
   - Document requirements in BUILD_ENVIRONMENT/README.

## Risks/Decisions
- Quad-float ABI differences across platforms; prefer libquadmath where available.
- Wire compatibility: choose stable text/decimal mapping for clients lacking native support.
