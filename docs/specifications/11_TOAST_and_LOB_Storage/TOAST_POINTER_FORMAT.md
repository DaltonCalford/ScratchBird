# TOAST Pointer Format

## Purpose

This file defines the authoritative `ToastPointer` contract.

## Binary Layout

`ToastPointer` is a packed 32-byte structure.

Field `lob_uuid` is an `ID` and occupies 16 bytes.

Field `total_len` is `uint64_t` and records the uncompressed byte length.

Field `chunk_size` is `uint32_t`.

Field `compression` is `uint16_t`.

Field `flags` is `uint16_t`.

The packed size shall remain exactly 32 bytes.

## Flag Vocabulary

`TOAST_COMPRESSED = 0x0001`

`TOAST_ENCRYPTED = 0x0002`

`TOAST_INLINE_REF = 0x0004`

`TOAST_FLAG_MASK = TOAST_COMPRESSED | TOAST_ENCRYPTED | TOAST_INLINE_REF`

## Pointer Validation Rules

A valid TOAST pointer payload shall be exactly `sizeof(ToastPointer)`.

`lob_uuid` shall be non-zero and shall pass the current UUID-v7 plausibility check used by `ToastManager::isToastPointer(...)`.

`total_len` shall be non-zero.

`chunk_size` shall be non-zero.

`flags` shall not contain bits outside `TOAST_FLAG_MASK`.

If `TOAST_COMPRESSED` is not set, `compression` shall equal `CompressionType::NONE`.

If `TOAST_COMPRESSED` is set, `compression` shall not equal `CompressionType::NONE`.

## Tuple Integration Rules

Heap tuples surface TOAST indirection through `TupleHeader::RHD_TOAST_PTR`.

`ToastManager::extractReferencedToastValueId(...)` may also accept a payload as a TOAST pointer when the payload passes `ToastManager::isToastPointer(...)`, even if the tuple-header flag is absent.

## Materialization Rules

`toastValue(...)` initializes `lob_uuid` with a UUID-v7 value.

`toastValue(...)` initializes `total_len` with the logical input size before compression.

`toastValue(...)` initializes `chunk_size` from `ToastSettings::getMaxChunkSize(db_->page_size())`.

`toastValue(...)` initializes `compression` to `CompressionType::NONE` and `flags` to `0` before strategy-specific updates.

If `ToastStrategy::EXTERNAL` compression succeeds, `compression` is set to `CompressionType::LZ4` and `TOAST_COMPRESSED` is set.

If `ToastStrategy::EXTERNAL` compression fails, the manager falls back to uncompressed chunk storage and clears the compressed flag.

## Current Scope Boundary

`TOAST_ENCRYPTED` is an encoded pointer bit. This file authorizes the bit and its validation rules only. It does not authorize a standalone encryption policy or operator contract.

`TOAST_INLINE_REF` is part of the flag vocabulary. This file does not widen that bit into a separate standalone LOB subsystem.
