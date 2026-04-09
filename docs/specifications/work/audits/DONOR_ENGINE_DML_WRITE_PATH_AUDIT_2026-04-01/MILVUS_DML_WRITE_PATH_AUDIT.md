# Milvus DML Write-Path Audit

## Architectural Summary

Milvus is a donor for vector-family write staging. Its core lesson is to keep writes in growing segments with lightweight hot metadata, then seal and synchronize those segments before heavier downstream index work.

## Insert Optimizations

- The write buffer is explicitly a channel-scoped buffer for DML.
- New writes enter growing segments rather than immediately forcing heavy index publication work.
- `write_buffer.go` also makes primary-key bloom filters and L0 delta logic part of the write-buffer abstraction, which is a strong donor pattern for ANN families with duplicate or deletion checks.

## Update/Delete Optimizations

- Delete messages are buffered alongside insert data instead of forcing immediate deep segment rewrite.
- Sync tasks package both insert and delete data, keeping the foreground path focused on staging rather than full publication.

## Index Maintenance Optimizations

- Segment sealing is policy-driven. Segments move from growing to sealed based on idle time, size, or total growing pressure.
- This is the correct donor pattern for expensive ANN families: close a mutable generation, then publish heavier immutable artifacts from that point.
- Sync tasks carry start positions, checkpoints, and per-segment payloads so background workers can continue safely.

## Reliability And Publication Pattern

- Milvus uses explicit checkpoints and sync managers around write-buffer eviction.
- The write buffer keeps the earliest relevant checkpoint candidate, which is the right donor idea for "what must be replayed or re-driven if background publish fails."

## Best Borrow Candidates For ScratchBird

- Growing-versus-sealed generation states for vector and ANN families.
- Per-segment bloom or summary structures in the hot lane.
- Policy-driven sealing and async sync packages instead of foreground full publication.

## Local Source Anchors

- `internal/flushcommon/writebuffer/write_buffer.go`
- `internal/datacoord/segment_manager.go`
