# Scalar Storage Format

## Purpose

This file defines the current serializer contract for scalar and scalar-adjacent plain values.

## Serializer API Contract

`TypeSerializer::serialize(const TypedValue&)` calls `TypedValue::serializePlainValue(...)`. On failure it returns an empty vector.

`TypeSerializer::deserialize(DataType, const uint8_t*, uint32_t, ErrorContext*)` rejects null input, applies the minimum-size gate for the requested `DataType`, constructs a `TypedValue(type)`, and calls `TypedValue::deserializePlainValue(...)`. If deserialization fails, it returns `std::nullopt`.

`TypeSerializer::getSerializedSize(const TypedValue&)` serializes through `TypedValue::serializePlainValue(...)` and returns the resulting byte length as `uint32_t`. If serialization fails or the result exceeds `uint32_t`, it returns `0`.

## Minimum-Size Admission Gates

The current minimum serialized sizes are:

`1` byte: `INT8`, `UINT8`, `BOOLEAN`

`2` bytes: `INT16`, `UINT16`

`4` bytes: `INT32`, `UINT32`, `FLOAT32`, `BIT`, `COMPLETION_FIELD`, `FLAT_OBJECT`

`6` bytes: `MACADDR`

`8` bytes: `INT64`, `UINT64`, `FLOAT64`, `MONEY`, `TIMESTAMP_NS`, `DATE`, `MACADDR8`, `DICT_ENCODED`

`12` bytes: `TIME`, `TIMESTAMP`, `TIME_WITH_ZONE`, `TIMESTAMP_WITH_ZONE`, `PREFIX_SEARCH_FIELD`

`16` bytes: `UUID`, `INT128`, `UINT128`, `INTERVAL`

`32` bytes: `INT256`, `UINT256`

`36` bytes: `DECIMAL256`

`3` bytes: `TAGGED_UNION`

All other types currently use a minimum-size gate of `0`, meaning this serializer layer does not reject them by a fixed minimum-size rule before handing them to `TypedValue`.

## Current Storage Rules

This file owns the serializer API contract and the admission gates above.

Exact payload encoding and decoding are currently owned by the `TypedValue` plain-value branches consumed by `TypeSerializer`.

## Boundary

This file does not widen section `14` into a complete storage contract for every complex family present in the shared `DataType` enum.
