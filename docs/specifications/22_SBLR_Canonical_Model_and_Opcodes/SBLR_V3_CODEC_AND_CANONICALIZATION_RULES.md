# SBLR V3 Codec and Canonicalization Rules

Status: current_authority

## Purpose

Define the exact binary and canonicalization rules used to encode, decode, and normalize V3 payloads inside SBLR containers.

## Binary Encoding Rules

### Varuint

- varuint uses unsigned LEB128
- decoding fails when:
  - input ends before completion
  - shift exceeds `63`

### Strings and bytes

- strings are encoded as:
  - varuint byte length
  - raw bytes
- byte arrays are encoded as:
  - varuint byte length
  - raw bytes
- decoding fails when length exceeds the remaining buffer

### Instruction header

Every instruction header is encoded as:
1. `opcode` little-endian `uint16`
2. `flags` little-endian `uint16`
3. `payload_len` little-endian `uint32`
4. payload bytes

Decoding fails when the payload length exceeds the remaining buffer.

## Schema-Driven Payload Rule

Payload encode and decode is schema-registry driven.

That means:
- field typing comes from schema definitions
- payload structure must match the schema for the opcode family
- callers may not bypass schema typing with parser-private shortcuts

## Scalar Wrapping Rule

Current codec behavior already wraps scalar values into literal-expression instructions when required.

Supported wrapped literals include:
- null
- boolean
- int64
- uint64 converted to int64 where safe, else string fallback
- double
- string
- binary

## Canonicalization Rules

### Symbol canonicalization

- symbols are sorted lexically
- duplicates collapse to one canonical symbol entry
- remap tables preserve original-to-canonical index mapping

### Constant-pool canonicalization

- constants are sorted by:
  1. tag
  2. canonical byte representation
- duplicates collapse to one canonical constant entry
- remap tables preserve original-to-canonical index mapping

### Payload canonicalization

Current payload canonicalization already sorts:
- `OPTION_KV` items by `key`
- `privileges` lists lexically
- `columns` lists lexically when the schema marks them as identifier lists

## Converter Consequences

The one-way SBLR-to-V3 converter must treat canonicalization as:
- a deterministic normalization aid
- not permission to discard user-significant retained names
- not permission to reorder source-order-sensitive constructs unless the schema explicitly marks them canonicalizable

## Fail-Closed Rules

The codec or converter must fail closed when:
- string length is invalid
- byte length is invalid
- instruction header is truncated
- schema-driven payload decode fails
- canonicalization would discard semantically distinct retained-name entries

## Relationship to Sections `22`, `23`, and `28`

- section `22` owns the codec and canonical payload rules
- section `23` owns runtime use of decoded payloads
- section `28` owns one-way reconstruction into V3 AST and parser-facing structures
