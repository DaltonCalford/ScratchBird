# IPC and Native Wire Frame Specification

## Scope

This document defines the current active frame headers for:
- the native ScratchBird wire protocol (`SBDB`)
- the parser-agent to engine IPC contract (`SBIP`)

It does not define listener control-plane framing. That belongs to section
`29`.

## Native wire header (`SBDB`)

Current native wire header is `MessageHeader` and is exactly 12 bytes.

Fields in order:
- `magic:u32`
- `version:u16`
- `type:u8`
- `flags:u8`
- `payload_length:u32`

Current constants:
- `PROTOCOL_MAGIC = 0x42444253`
- `PROTOCOL_VERSION_MAJOR = 1`
- `PROTOCOL_VERSION_MINOR = 1`
- `PROTOCOL_VERSION = 0x0101`
- `MAX_MESSAGE_SIZE = 16 MiB`

Native header validation must fail closed on:
- magic mismatch
- unsupported version
- oversized payload length

## Native capability flags

Current connection capability flags include:
- `CONNECT_FLAG_ZSTD_COMPRESSION`
- `CONNECT_FLAG_COPY`
- `CONNECT_FLAG_LOB_STREAM`
- `CONNECT_FLAG_PORTAL_PAGING`
- `CONNECT_FLAG_NOTIFICATIONS`
- `CONNECT_FLAG_PROGRESS`
- `CONNECT_FLAG_MANAGER_DBBT`
- `CONNECT_FLAG_BOUND_DB_UUID`
- `CONNECT_FLAG_AUTH_PLUGIN_REGISTRY`
- `CONNECT_FLAG_AUTOCOMMIT_OFF`
- `CONNECT_FLAG_NO_DORMANT_DETACH`
- `CONNECT_FLAG_DORMANT_REATTACH`

These are current native-wire capability bits. Their deeper session semantics
are owned by adjacent sections.

## IPC header (`SBIP`)

Current IPC header is `IPCHeader` and is exactly 40 bytes.

Fields in order:
- `magic:u32`
- `version:u16`
- `type:u16`
- `length:u32`
- `request_id:u32`
- `session_id:u32`
- `timestamp:u64`
- `flags:u32`
- `reserved:u32`

Current constants:
- `IPCHeader::MAGIC = 0x53424950`
- `IPC_CURRENT_VERSION = 0x0101`
- `IPC_MAX_MESSAGE_SIZE = 1 MiB`
- `IPC_MAX_PAYLOAD_SIZE = ~1020 KiB`

IPC header validation must fail closed on:
- magic mismatch
- version mismatch
- oversized payload

## Header ownership boundary

- `SBDB` is the current external native wire header
- `SBIP` is the current internal parser-agent to engine header
- `DBBT` and `LPREFACE` are current listener control-plane headers owned
  outside this document

## Negative requirements

- do not invent CRC, fragment, or multi-profile fixed-header fields that are
  not present in current source
- do not describe native and IPC headers as already unified
