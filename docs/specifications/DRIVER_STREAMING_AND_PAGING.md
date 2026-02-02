# Driver Streaming and Paging

Status: Draft
Last Updated: 2026-01-09

## Purpose

Define how drivers stream result sets and page large responses without
loading all rows in memory.

## Binary-Only Requirement

Streaming and paging must operate on binary result formats only.
Drivers must request binary format for all columns when streaming.

## Requirements

- Provide a streaming API (iterator/reader) for large result sets.
- Do not buffer the entire result set by default.
- Expose a configurable fetch size where the language allows.

## SBWP Paging

- Use EXECUTE max_rows to page through a portal.
- Drivers should support repeated EXECUTE calls to fetch subsequent pages.
- Where possible, drivers should expose a `fetch_size` (rows per page) option.

## SBWP LOB Streaming

- Large values may be returned as streamed columns (length = -2 in DATA_ROW).
- Drivers must consume the corresponding `STREAM_READY` / `STREAM_DATA` /
  `STREAM_END` frames and assemble the byte payload before exposing the value.
- Streamed payloads are binary and must not be re-encoded.

## SQLSTATE Codes (Streaming/Paging)

- 57014: query canceled
- 08006: connection failure during streaming

## Per-Language Streaming Mapping

### Go

- Use database/sql Rows (Rows.Next) for streaming.
- Optional: expose fetch size via driver-specific options.

### Node.js/TypeScript

- Use queryStream() returning AsyncGenerator.
- Support backpressure by yielding per row.

### Python

- Cursor is iterable; arraysize controls fetchmany().
- Avoid loading full result into memory.

### Ruby

- Statement#stream and Connection#stream return ResultStream.
- ResultStream#each should yield rows incrementally.

### Rust

- Client.query_stream returns QueryStream.
- Stream rows asynchronously.

### PHP

- ResultStream::readRow reads rows incrementally.

### R

- No streaming API today; must add incremental fetch support.

### Pascal/Delphi

- Provide row-by-row fetch in dataset components.

### .NET

- Use DbDataReader with sequential access.
- Expose CommandBehavior.SequentialAccess when streaming.

### JDBC

- Use ResultSet streaming with fetchSize and forward-only cursors.
