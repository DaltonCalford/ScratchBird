# Columnstore Analytical Storage and Segment Model

Status: current_authority

## Purpose

Define the current columnstore metadata-page, segment-page, compression, and MGA-aware analytical storage model.

## Current Use Case Boundary

Columnstore is intended for:
- analytical workloads
- scan-heavy queries
- compression-friendly columns
- vectorized or batch-style processing

It is not the default heap row-store.

## Current Metadata Page Contract

The metadata page currently stores:
- index UUID
- table UUID
- segment size
- default compression type
- column count
- first segment page
- total segment count
- total row count
- creation and deletion transaction identity

## Current Segment Page Contract

A columnstore segment page currently stores:
- index UUID
- table UUID
- column UUID
- flags
- row count
- null count
- compression type
- data type
- compressed size
- uncompressed size
- min and max values
- first and last `TID`
- MGA visibility transaction fields
- sibling segment navigation

This makes the columnstore family:
- durable
- segment oriented
- TID anchored
- MGA visibility aware

## Current Compression Boundary

Current compression types defined in the runtime include:
- `NONE`
- `RLE`
- `DICTIONARY`
- `BITPACK`
- `DELTA`

Current implementation notes identify `RLE` as the proven phase-1 baseline.

Therefore the canon must distinguish:
- defined compression enums
- currently proven compression behavior

## Current In-Memory Segment Contract

Current in-memory `ColumnSegment` state includes:
- column UUID
- data type
- raw or compressed data buffer
- compression type
- row count
- null count
- null bitmap
- first and last `TID`
- `xmin` and `xmax`
- explicit `tid_map`
- visibility bitmap
- min and max value
- page count
- next segment page

This proves that the analytical family already carries:
- TID mapping
- visibility state
- value-range metadata
- page-chain state

## MGA Rule

Columnstore remains MGA subordinate:
- segment visibility is transaction stamped
- stable `TID` identity ties analytical storage back to row truth
- GC and cleanup must respect visibility
- analytical scans do not redefine commit truth

## Predicate and Scan Rule

Current design already exposes:
- min and max value metadata
- null count
- compression-aware segment state

The canonical interpretation is:
- predicate pushdown and segment skipping are legal where current runtime proves them
- segment acceptance remains subordinate to MGA visibility and stable TID mapping

## Relationship to Primary Storage

Columnstore is:
- a specialized analytical access family
- not a replacement for heap primary storage
- not a license to bypass heap-truth validation for mutable row visibility

## Explicit Non-Guarantees

- no claim that columnstore is the default table storage engine
- no claim that every OLTP mutation path is columnstore first
- no claim of full parity with every secondary family
