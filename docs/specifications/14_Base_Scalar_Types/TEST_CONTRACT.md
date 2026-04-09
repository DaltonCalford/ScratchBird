# Section 14 Test Contract

## Purpose

This file defines the proof surface required for section `14`.

## Direct proof artifacts

`tests/unit/test_type_conversions.cpp` proves current scalar conversion behavior, money conversion behavior, and a subset of conversion error semantics.

`tests/unit/test_type_mapping.cpp` proves audited PostgreSQL, MySQL, Firebird, and SBWP type mappings plus type-family classification helpers.

`tests/unit/test_type_serialization.cpp` proves `TypeSerializer` round-trip behavior for scalar, temporal, network, range, geometry, and current VNext scalar families.

`tests/unit/test_network_types.cpp`, `tests/unit/test_range_types.cpp`, `tests/unit/test_temporal_range_types.cpp`, and `tests/unit/test_text_search_types.cpp` prove the audited native value families that section `14` currently claims as code-backed scalar or scalar-adjacent runtime surfaces.

`tests/unit/test_python_psql_parity.cpp` is a SQL-facing proof surface for a subset of cast and scalar behavior.

## Required behavioral assertions

The proof surface shall show that `TypeSerializer` entry points are active and route through `TypedValue`.

The proof surface shall show that conversion failures surface named runtime errors rather than silent coercion.

The proof surface shall show that the emulated type matrix is backed by `TypeSystem` and not by parser-only prose.

The proof surface shall show that `EXTRACT(...)` is a real runtime feature and not a documentation-only claim.

The proof surface shall show that composite-field extraction is a separate `COMPOSITE` path and does not prove universal scalar member access.

## Explicit negative requirements

This section shall not be treated as proving universal scalar dot-member syntax.

This section shall not be treated as proving full semantic parity with every donor engine named in older prose.

This section shall not be treated as proving every `DataType` enum member as a fully audited base scalar family.

## Beta 2 required proof additions

The Beta 2 datatype expansion is not certified unless evidence covers:

- `BFLOAT16`, `BIGNUM`, and `VERSIONSTAMP` serializer round-trip
- translated alias resolution for the new fixed-width numeric and temporal rows
- system-domain-backed compact decimal, scaled-float, bitstring, keyword, and
  identifier rows
- deterministic text rendering and parse-back for every new scalar family
- selector coverage for temporal, interval, network, versionstamp, and
  reference wrappers
