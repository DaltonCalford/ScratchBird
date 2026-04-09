# LOB Page Layouts

Status: current_authority

## Purpose

This file defines the current oversized-value storage layout contract.

The normative storage truth is TOAST chunk-row storage in per-table TOAST tables. Dedicated LOB-named page support and diagnostics may exist, but they do not displace the TOAST chunk-row contract as the current primary storage truth.

## TOAST Table Schema

Each TOAST table is created with three columns.

chunk_id uses DataType::UUID, has max length 16, and is non-null.

chunk_seq uses DataType::INT32, has max length 4, and is non-null.

chunk_data uses DataType::BYTEA, has max length equal to ToastSettings::getMaxChunkSize(db_->page_size()), and is non-null.

Each TOAST table shall have a BTREE index on (chunk_id, chunk_seq).

## Chunk Row Layout

Every stored chunk row uses the following tuple payload order.

TupleHeader

chunk_id as the owning TOAST value UUID

chunk_seq as a zero-based sequence number

chunk_size as the size of the data fragment in the row

chunk_data as the stored bytes for that fragment

## Chunk Lifecycle States

The canonical lifecycle states are owned by ToastChunkLifecycleState.

INVALID

LIVE_VISIBLE

CREATE_INVISIBLE

DELETE_PENDING

RECLAIMABLE_DELETED

CLEAR_DELETE_MARKER

RECLAIMABLE_ABORTED_CREATE

## Lifecycle Interpretation

LIVE_VISIBLE means the chunk is visible to the current transaction.

CREATE_INVISIBLE means the creating transaction is not visible to the current transaction.

DELETE_PENDING means the delete exists but is not reclaimable under current visibility rules.

RECLAIMABLE_DELETED means the delete is visible and reclaim eligibility has been reached.

CLEAR_DELETE_MARKER means the delete marker came from an aborted or invalid deleting transaction and should be treated as a delete-marker cleanup case rather than as a durable delete.

RECLAIMABLE_ABORTED_CREATE means the creating transaction is aborted or invalid and the chunk may be reclaimed.

## Diagnostics Contract

The diagnostics surface accepts PAGE_TYPE_HEAP, PAGE_TYPE_TOAST_CHUNK, and PAGE_TYPE_LOB_CHUNK as the current inspected page families for oversized-value diagnostics.

The canonical issue classes proved by the diagnostics code include:

INVALID_PAGE_HEADER

INVALID_PAGE_TYPE

INVALID_ITEM_POINTER

INVALID_TUPLE_HEADER

INVALID_PAYLOAD_LENGTH

INVALID_TOAST_POINTER

TOAST_FLAG_MISMATCH

LOB_CHUNK_MISSING

The sequence validator requires non-zero chunk_size, rejects non-empty chunk sequences for empty values, and treats missing or non-contiguous chunk indices as LOB_CHUNK_MISSING.

## Current Scope Boundary

This file authorizes TOAST chunk-row layout as the current storage contract.

This file does not authorize a separate standalone LOB_META or operator-visible standalone LOB_CHUNK storage subsystem beyond the currently audited diagnostic and page-family support.
