# Firebird Wire Protocol Specification

## Protocol Version: 10-17 (Firebird 1.0-5.0)

## Overview

Firebird uses a packet-based protocol with XDR (External Data Representation) encoding for platform independence. The protocol operates over TCP/IP on port 3050 by default. This specification targets Firebird 5.0 behavior for emulation and remote UDR connections.

## XDR Encoding

Firebird uses XDR (RFC 1832) for data serialization:

```c
// XDR Basic Types
int32_t   xdr_long;      // 4 bytes, big-endian
uint32_t  xdr_u_long;    // 4 bytes, big-endian
int16_t   xdr_short;     // 2 bytes, big-endian (padded to 4)
uint16_t  xdr_u_short;   // 2 bytes, big-endian (padded to 4)
char      xdr_char;      // 1 byte (padded to 4)

// XDR String
struct xdr_string {
    uint32_t length;     // String length
    char     data[];     // String data (padded to 4-byte boundary)
    char     padding[];  // 0-3 bytes of padding
};

// XDR Opaque (byte array)
struct xdr_opaque {
    uint32_t length;     // Data length
    uint8_t  data[];     // Raw bytes (padded to 4-byte boundary)
    uint8_t  padding[];  // 0-3 bytes of padding
};
```

Firebird protocol structs use `CSTRING`/`CSTRING_CONST`, which are encoded on the wire as XDR strings:
`uint32 length` followed by that many bytes, padded to a 4-byte boundary (no terminating null).

### XDR Padding Rule
```c
#define XDR_ALIGN(n) (((n) + 3) & ~3)  // Round up to 4-byte boundary

size_t xdr_string_size(const char* str) {
    size_t len = strlen(str);
    return 4 + XDR_ALIGN(len);  // 4 bytes for length + padded string
}
```

## Packet Encoding and Framing

Firebird packets are XDR-encoded `PACKET` unions. The first field is always the
operation code (`P_OP`), encoded as a 32-bit XDR enum. There is **no universal
length field** in the protocol itself; the transport reads/writes the XDR stream
in chunks and the XDR decoder determines message boundaries.

Large logical packets may be sent in partial mode; in that case, the receiver
may observe `op_partial` and must continue reading until a complete packet is
assembled.

## Operation Codes

```c
enum WireOp {
    op_connect           = 1,    // Connect to database
    op_exit              = 2,    // Disconnect
    op_accept            = 3,    // Accept connection
    op_reject            = 4,    // Reject connection
    op_protocol          = 5,    // Protocol handshake
    op_disconnect        = 6,    // Disconnect notification
    op_credit            = 7,    // Credit check
    op_continuation      = 8,    // Continuation packet
    op_response          = 9,    // Generic response
    
    // Database operations
    op_attach            = 19,   // Attach database
    op_create            = 20,   // Create database
    op_detach            = 21,   // Detach database
    op_compile           = 22,   // Compile request
    op_start             = 23,   // Start request
    op_start_and_send    = 24,   // Start and send
    op_send              = 25,   // Send data
    op_receive           = 26,   // Receive data
    op_unwind            = 27,   // Unwind request
    op_release           = 28,   // Release object
    
    // Transaction operations
    op_transaction       = 29,   // Start transaction
    op_commit            = 30,   // Commit transaction
    op_rollback          = 31,   // Rollback transaction
    op_prepare           = 32,   // Prepare transaction
    op_reconnect         = 33,   // Reconnect to transaction
    
    // Blob operations
    op_create_blob       = 34,   // Create blob
    op_open_blob         = 35,   // Open blob
    op_get_segment       = 36,   // Get blob segment
    op_put_segment       = 37,   // Put blob segment
    op_cancel_blob       = 38,   // Cancel blob
    op_close_blob        = 39,   // Close blob
    
    // Info operations
    op_info_database     = 40,   // Database info
    op_info_request      = 41,   // Request info
    op_info_transaction  = 42,   // Transaction info
    op_info_blob         = 43,   // Blob info
    
    // Batch operations
    op_batch_segments    = 44,   // Batch segments
    op_mgr_set_affinity  = 45,   // Manager set affinity
    op_mgr_clear_affinity = 46,  // Manager clear affinity
    op_mgr_report        = 47,   // Manager report
    
    // SQL operations
    op_que_events        = 48,   // Queue events
    op_cancel_events     = 49,   // Cancel events
    op_commit_retaining  = 50,   // Commit retaining
    op_prepare2          = 51,   // Prepare (protocol 11)
    op_event             = 52,   // Event notification
    op_connect_request   = 53,   // Connection request
    op_aux_connect       = 54,   // Auxiliary connection
    op_ddl               = 55,   // DDL statement
    op_open_blob2        = 56,   // Open blob (v2)
    op_create_blob2      = 57,   // Create blob (v2)
    op_get_slice         = 58,   // Get array slice
    op_put_slice         = 59,   // Put array slice
    op_slice             = 60,   // Array slice response
    op_seek_blob         = 61,   // Seek in blob
    
    // Statement operations
    op_allocate_statement = 62,  // Allocate statement
    op_execute           = 63,   // Execute statement
    op_exec_immediate    = 64,   // Execute immediate
    op_fetch             = 65,   // Fetch rows
    op_fetch_response    = 66,   // Fetch response
    op_free_statement    = 67,   // Free statement
    op_prepare_statement = 68,   // Prepare statement
    op_set_cursor        = 69,   // Set cursor name
    op_info_sql          = 70,   // SQL info
    
    // Extended operations
    op_dummy             = 71,   // Dummy packet
    op_response_piggyback = 72,  // Piggyback response
    op_start_and_receive = 73,   // Start and receive
    op_start_send_and_receive = 74, // Start, send and receive
    op_exec_immediate2   = 75,   // Execute immediate (v2)
    op_execute2          = 76,   // Execute (v2)
    op_insert            = 77,   // Insert operation
    op_sql_response      = 78,   // SQL response
    op_transact          = 79,   // Transaction operation
    op_transact_response = 80,   // Transaction response
    op_drop_database     = 81,   // Drop database
    op_service_attach    = 82,   // Attach service
    op_service_detach    = 83,   // Detach service
    op_service_info      = 84,   // Service info
    op_service_start     = 85,   // Start service
    op_rollback_retaining = 86,  // Rollback retaining
    
    // Update operations
    op_update_account_info = 87, // Update account info
    op_authenticate_user = 88,   // Authenticate user
    op_partial           = 89,   // Partial packet
    op_trusted_auth      = 90,   // Trusted authentication
    op_cancel            = 91,   // Cancel operation
    op_cont_auth         = 92,   // Continue authentication
    op_ping              = 93,   // Ping
    op_accept_data       = 94,   // Accept data
    op_abort_aux_connection = 95, // Abort auxiliary connection
    op_crypt             = 96,   // Encryption
    op_crypt_key_callback = 97,  // Encryption key callback
    op_cond_accept       = 98,   // Conditional accept

    // Batch/replication extensions (protocol 17+)
    op_batch_create      = 99,
    op_batch_msg         = 100,
    op_batch_exec        = 101,
    op_batch_rls         = 102,
    op_batch_cs          = 103,
    op_batch_regblob     = 104,
    op_batch_blob_stream = 105,
    op_batch_set_bpb     = 106,
    op_repl_data         = 107,
    op_repl_req          = 108,
    op_batch_cancel      = 109,
    op_batch_sync        = 110,
    op_info_batch        = 111,
    op_fetch_scroll      = 112,
    op_info_cursor       = 113,
    op_inline_blob       = 114
};
```

Notes:
- `op_protocol`, `op_credit`, and `op_continuation` are legacy/unused in modern Firebird.
- Protocol 17+ adds batch/replication opcodes (99-114) described below.

## Batch and Replication Extensions (Protocol 17+)

### Batch DSQL Operations

```c
// Create batch interface for a prepared statement (op_batch_create)
typedef struct p_batch_create
{
    OBJCT         p_batch_statement;   // Statement object id
    CSTRING_CONST p_batch_blr;         // BLR describing input messages
    ULONG         p_batch_msglen;      // Explicit message length
    CSTRING_CONST p_batch_pb;          // Batch parameters block
} P_BATCH_CREATE;

// Add messages to batch (op_batch_msg)
typedef struct p_batch_msg
{
    OBJCT  p_batch_statement;          // Statement object id
    ULONG  p_batch_messages;           // Number of messages in this packet
    CSTRING p_batch_data;              // Packed message stream (xdr_packed_message, no length prefix on wire)
} P_BATCH_MSG;

// Execute batch (op_batch_exec)
typedef struct p_batch_exec
{
    OBJCT p_batch_statement;           // Statement object id
    OBJCT p_batch_transaction;         // Transaction object id
} P_BATCH_EXEC;

// Batch completion state (op_batch_cs)
typedef struct p_batch_cs
{
    OBJCT p_batch_statement;           // Statement object id
    ULONG p_batch_reccount;            // Total records
    ULONG p_batch_updates;             // Update counters count
    ULONG p_batch_vectors;             // Record+status pairs count
    ULONG p_batch_errors;              // Error record numbers count
} P_BATCH_CS;

// BLOB stream portion in batch (op_batch_blob_stream)
typedef struct p_batch_blob
{
    OBJCT   p_batch_statement;         // Statement object id
    CSTRING p_batch_blob_data;         // Blob stream bytes (see below)
} P_BATCH_BLOB;

// Register existing blob in batch (op_batch_regblob)
typedef struct p_batch_regblob
{
    OBJCT p_batch_statement;           // Statement object id
    SQUAD p_batch_exist_id;            // Existing blob id
    SQUAD p_batch_blob_id;             // New blob id
} P_BATCH_REGBLOB;

// Set default BPB for batch blobs (op_batch_set_bpb)
typedef struct p_batch_setbpb
{
    OBJCT         p_batch_statement;   // Statement object id
    CSTRING_CONST p_batch_blob_bpb;    // BPB bytes
} P_BATCH_SETBPB;
```

**Batch opcodes and payloads**
- `op_batch_create` → `P_BATCH_CREATE`
- `op_batch_msg` → `P_BATCH_MSG`
- `op_batch_exec` → `P_BATCH_EXEC`
- `op_batch_cs` → `P_BATCH_CS` + additional data (see below)
- `op_batch_blob_stream` → `P_BATCH_BLOB` (xdr_blob_stream encoding)
- `op_batch_regblob` → `P_BATCH_REGBLOB`
- `op_batch_set_bpb` → `P_BATCH_SETBPB`
- `op_batch_rls` → `P_RLSE` (release batch/statement object)
- `op_batch_cancel` → `P_RLSE` (cancel batch/statement object)
- `op_batch_sync` → no payload
- `op_info_batch` → `P_INFO` (same structure as other info ops)

`op_batch_rls`, `op_batch_cancel`, and `op_release` use `P_RLSE` with the target statement/batch object id.

**Batch parameters block (`p_batch_pb`)**
- `p_batch_pb` is a `WideTagged` clumplet buffer.
- Format: `version:byte` followed by repeated clumplets:
  - `tag:byte`, `length:uint32 (LE)`, `value[length]`
- Note: the outer XDR `CSTRING` length is big-endian; clumplet lengths are little-endian inside the buffer.
- `version` = `IBatch::VERSION1` (1).
- Tags and values (stored as 32-bit integers unless noted):
  - `TAG_MULTIERROR` (1): 0/1, enable multi-error reporting.
  - `TAG_RECORD_COUNTS` (2): 0/1, include record counts in completion state.
  - `TAG_BUFFER_BYTES_SIZE` (3): buffer size in bytes.
  - `TAG_BLOB_POLICY` (4): blob policy enum:
    - `BLOB_NONE` (0)
    - `BLOB_ID_ENGINE` (1)
    - `BLOB_ID_USER` (2)
    - `BLOB_STREAM` (3)
  - `TAG_DETAILED_ERRORS` (5): 0/1, include detailed error vectors.
- `IBatch::BLOB_SEGHDR_ALIGN` = 2 (segment header alignment).

**Batch info items (`op_info_batch`)**
- `INF_BUFFER_BYTES_SIZE` (10)
- `INF_DATA_BYTES_SIZE` (11)
- `INF_BLOBS_BYTES_SIZE` (12)
- `INF_BLOB_ALIGNMENT` (13)
- `INF_BLOB_HEADER` (14)

**Packed batch messages (`op_batch_msg`)**
- `p_batch_data` is a stream of `p_batch_messages` packed messages, back-to-back, with no length prefix.
- Each message uses `xdr_packed_message`: a null bitmap (`(field_count + 7) / 8` bytes, XDR-padded to 4) followed by XDR-encoded values for non-NULL fields.
- The client buffer uses `p_batch_msglen` and aligned size (`FB_ALIGN(fmt_length, FB_ALIGNMENT)`), but the on-wire length per message depends on NULLs and variable-length fields.

**Batch completion state (`op_batch_cs`)**
After the fixed header fields, the payload includes:
1. `p_batch_updates` signed 32-bit update counts.
2. `p_batch_vectors` pairs of: `ULONG record_num` + status vector (xdr_status_vector).
3. `p_batch_errors` `ULONG record_num` entries for errors without a status vector.

**Batch blob stream (`op_batch_blob_stream`)**
`p_batch_blob_data` is encoded via `xdr_blob_stream`:
- Starts with `ULONG stream_len`, then `stream_len` bytes.
- Stream is aligned to `BatchStream.alignment` (from statement).
- Each blob begins with a header aligned to `alignment`:
  - `ISC_QUAD batch_blob_id`
  - `ULONG blob_size`
  - `ULONG bpb_size`
- BPB bytes follow (length `bpb_size`), then blob data.
- If segmented, blob data is split into segments:
  - Each segment starts with `USHORT segment_length` aligned to `IBatch::BLOB_SEGHDR_ALIGN`.
  - `segment_length` is encoded via `xdr_u_short` (4-byte XDR) even though it represents a 16-bit length.
- Stream may end mid-header; the remainder carries to the next `op_batch_blob_stream`.

### Replication Operations

```c
typedef struct p_replicate
{
    OBJCT         p_repl_database;   // Database object id
    CSTRING_CONST p_repl_data;       // Replication data (opaque)
} P_REPLICATE;
```

- `op_repl_data` → `P_REPLICATE` (implemented in protocol.cpp)
- `op_repl_req` → reserved; when used, it follows `P_REPLICATE` layout in protocol.h

### Inline BLOB Transfer

```c
typedef struct p_inline_blob
{
    OBJCT         p_tran_id;     // Transaction id
    SQUAD         p_blob_id;     // Blob id
    CSTRING       p_blob_info;   // Blob info (xdr_response)
    RemBlobBuffer* p_blob_data;  // Blob data (xdr_blobBuffer)
} P_INLINE_BLOB;
```

`op_inline_blob` embeds small blobs in a single packet. The blob data is encoded as:
`Int32 length` + data bytes, padded to 4-byte boundary.

## Connection Phase

### 1. Initial Connect

```c
// Encoded as PACKET with p_operation = op_connect and p_cnct payload.
struct P_CNCT {
    uint16  p_cnct_operation;    // Unused (0)
    uint16  p_cnct_cversion;     // CONNECT_VERSION3
    P_ARCH  p_cnct_client;       // Client architecture
    CSTRING_CONST p_cnct_file;   // Database path
    uint16  p_cnct_count;        // Number of protocol versions
    CSTRING_CONST p_cnct_user_id;// User identification block (TLV)

    struct p_cnct_repeat {
        uint16 p_cnct_version;      // Protocol version (10-13)
        P_ARCH p_cnct_architecture; // Architecture
        uint16 p_cnct_min_type;     // Minimum type (unused)
        uint16 p_cnct_max_type;     // Maximum type
        uint16 p_cnct_weight;       // Preference weight
    } p_cnct_versions[];
};

// Protocol versions
#define PROTOCOL_VERSION10   10
#define PROTOCOL_VERSION11   11
#define PROTOCOL_VERSION12   12
#define PROTOCOL_VERSION13   13

// Architecture types
#define ARCH_GENERIC         1  // Generic
#define ARCH_INTEL_X86       30 // Intel x86
#define ARCH_AMD_X64         31 // AMD x64

// User identification TLV items (p_cnct_user_id)
// Format: <type:byte><length:byte><data...>
#define CNCT_user           1   // User name
#define CNCT_passwd         2   // Password
#define CNCT_host           4   // Host name
#define CNCT_specific_data  7   // Plugin-specific data
#define CNCT_plugin_name    8   // Plugin name
#define CNCT_login          9   // DPB-style user name
#define CNCT_plugin_list    10  // Client-supported auth plugins
#define CNCT_client_crypt   11  // Client encryption plugin
```

### 2. Accept Response

```c
// op_accept -> P_ACPT
struct P_ACPT {
    uint16 p_acpt_version;       // Protocol version
    P_ARCH p_acpt_architecture;  // Architecture
    uint16 p_acpt_type;          // Minimum type
};

// op_accept_data / op_cond_accept -> P_ACPD
struct P_ACPD : public P_ACPT {
    CSTRING p_acpt_data;         // Returned auth data
    CSTRING p_acpt_plugin;       // Plugin to continue with
    uint16  p_acpt_authenticated;// 1 if auth complete
    CSTRING p_acpt_keys;         // Keys known to server
};
```

Servers may respond with:
- `op_accept` (basic accept)
- `op_accept_data` (accept with authentication data)
- `op_cond_accept` (accept and request further authentication before attach)

### 2.1 Example Handshake (Hex)

Client `op_connect` (P_CNCT) with user SYSDBA/masterkey, DB `test.fdb`, protocol 13:
```
00 00 00 01  // op_connect
00 00 00 00  // p_cnct_operation
00 00 00 03  // CONNECT_VERSION3
00 00 00 01  // ARCH_GENERIC
00 00 00 08  // file length
74 65 73 74 2e 66 64 62  // "test.fdb"
00 00 00 01  // protocol count
00 00 00 13  // user_id length
01 06 53 59 53 44 42 41 02 09 6d 61 73 74 65 72 6b 65 79 00  // TLV + pad
00 00 00 0d  // protocol version 13
00 00 00 01  // ARCH_GENERIC
00 00 00 00  // min_type
00 00 00 05  // max_type (ptype_lazy_send)
00 00 00 01  // weight
```

Server `op_accept` (P_ACPT):
```
00 00 00 03  // op_accept
00 00 00 0d  // protocol version 13
00 00 00 01  // ARCH_GENERIC
00 00 00 05  // ptype_lazy_send
```

### 2.2 Worked Flow (Connect + Attach + Exec Immediate)

Client `op_attach` with DPB (SYSDBA/masterkey, dialect 3):
```
00 00 00 13  // op_attach
00 00 00 00  // p_atch_database (0)
00 00 00 08  // p_atch_file length
74 65 73 74 2e 66 64 62  // "test.fdb"
00 00 00 17  // p_atch_dpb length (23)
01 1c 06 53 59 53 44 42 41 1d 09 6d 61 73 74 65 72 6b 65 79 3f 01 03 00
```

Server `op_response` (db handle = 1):
```
00 00 00 09  // op_response
00 00 00 01  // p_resp_object (db handle)
00 00 00 00 00 00 00 00  // p_resp_blob_id
00 00 00 00  // p_resp_data length
00 00 00 00  // status vector (isc_arg_end)
```

Client `op_exec_immediate` (SQL: `SELECT 1 FROM RDB$DATABASE`):
```
00 00 00 40  // op_exec_immediate
00 00 00 01  // p_sqlst_transaction
00 00 00 00  // p_sqlst_statement (0 for immediate)
00 00 00 03  // p_sqlst_SQL_dialect
00 00 00 1a  // SQL length
53 45 4c 45 43 54 20 31 20 46 52 4f 4d 20 52 44 42 24 44 41 54 41 42 41 53 45
00 00        // Padding (XDR)
00 00 00 00  // p_sqlst_items length
00 00 00 00  // p_sqlst_buffer_length
```

Server `op_response` success:
```
00 00 00 09  // op_response
00 00 00 00  // p_resp_object
00 00 00 00 00 00 00 00  // p_resp_blob_id
00 00 00 00  // p_resp_data length
00 00 00 00  // isc_arg_end
```

### 3. Database Attachment

```c
// op_attach / op_create -> P_ATCH
struct P_ATCH {
    OBJCT         p_atch_database;  // Database object id (0 for new)
    CSTRING_CONST p_atch_file;      // Database path
    CSTRING_CONST p_atch_dpb;       // Database Parameter Block (DPB)
};

// Database Parameter Block (DPB) items
#define isc_dpb_version1         1
#define isc_dpb_user_name        28
#define isc_dpb_password         29
#define isc_dpb_password_enc     30
#define isc_dpb_role_name        60
#define isc_dpb_sql_dialect      63
#define isc_dpb_charset          68
#define isc_dpb_session_time_zone 116
#define isc_dpb_auth_plugin_name 119
#define isc_dpb_auth_plugin_list 120
```

DPB format: `byte version` followed by repeated items: `byte item`, `byte length`, `value[length]`.

Example DPB Construction:
```c
uint8_t* build_dpb(const char* user, const char* password) {
    uint8_t* dpb = malloc(256);
    uint8_t* p = dpb;
    
    *p++ = isc_dpb_version1;        // Version
    
    // User name
    *p++ = isc_dpb_user_name;
    *p++ = strlen(user);
    memcpy(p, user, strlen(user));
    p += strlen(user);
    
    // Password
    *p++ = isc_dpb_password;
    *p++ = strlen(password);
    memcpy(p, password, strlen(password));
    p += strlen(password);
    
    // SQL dialect
    *p++ = isc_dpb_sql_dialect;
    *p++ = 1;
    *p++ = 3;  // Dialect 3
    
    return dpb;
}
```

### Character Set and Collation Negotiation

The connection character set is negotiated via the DPB and applies to:
- SQL text in DSQL statements.
- String parameter values when BLR does not specify an explicit charset.
- Metadata strings returned in info items (relation names, field names, etc.).

Rules:
- Prefer `isc_dpb_charset` when provided.
- If `isc_dpb_charset` is not present, fall back to legacy `isc_dpb_lc_ctype`
  (where available) or the database default character set.
- Column-level character set and collation are defined by metadata; BLR uses
  `blr_text2`/`blr_varying2` to encode explicit character set ids.
- If an unknown charset/collation is requested, return the appropriate Firebird
  error (e.g., `isc_charset_not_found`, `isc_collation_not_found`).

### Service Manager (op_service_*)

Service Manager requests use the same wire framing as database attach, but
`op_service_attach`/`op_service_detach`/`op_service_info`/`op_service_start`
operate on the **service handle** instead of a database handle.

Service attach uses `P_ATCH` with `p_atch_file` set to `service_mgr`
(optionally prefixed with host name) and the parameter block encoded as SPB
instead of DPB.

#### Service Parameter Block (SPB)

SPB format matches DPB encoding: `byte version` followed by repeated items
`<item><length><value...>`. ScratchBird uses Firebird-compatible SPB constants
from `ibase.h`.

Common attach items:
- `isc_spb_user_name`
- `isc_spb_password` / `isc_spb_password_enc`
- `isc_spb_trusted_auth`
- `isc_spb_auth_plugin_list` / `isc_spb_auth_plugin_name`

#### Service Actions (op_service_start)

The action block begins with `isc_action_svc_*` followed by action-specific
arguments encoded as TLV items. For Alpha, ScratchBird accepts the common
actions used by Firebird clients and returns a clear error for unsupported
actions:
- `isc_action_svc_db_stats`
- `isc_action_svc_properties`
- `isc_action_svc_backup` / `isc_action_svc_restore`
- `isc_action_svc_repair` / `isc_action_svc_validate`
- `isc_action_svc_trace_start` / `isc_action_svc_trace_stop`

#### Service Info (op_service_info)

`op_service_info` uses `P_INFO` and returns a TLV response in `p_resp_data`
containing `isc_info_svc_*` items. The response buffer ends with
`isc_info_end` or `isc_info_truncated` and is followed by a status vector.

### Protocol Version Quirks (10-13)

ScratchBird accepts versions 10-13 and will choose the highest version offered
by the client. Behavior differences to honor:

| Version | Notes |
| --- | --- |
| 10 | Uses `op_prepare`/`op_exec_immediate`/`op_execute` only; no packed messages; no inline blobs. |
| 11 | Adds `op_prepare2`/`op_execute2`/`op_exec_immediate2` and statement flags. |
| 12 | Adds `op_fetch_scroll`, cursor flags, and broader cancel semantics. |
| 13 | Adds packed message encoding and inline blob size fields. |

When a legacy client uses v10 opcodes, ScratchBird maps them to the v13
internal path (no packed message, no inline blob).

## Authentication

### SRP (Secure Remote Password) Authentication

Firebird 3.0+ uses SRP for secure authentication:

```c
struct srp_client_public {
    xdr_opaque salt;        // Server salt
    xdr_opaque verifier;    // SRP verifier
};

struct srp_server_public {
    xdr_opaque public_key;  // Server public key (B)
    xdr_opaque salt;        // Salt
};

// SRP flow:
// 1. Client sends username
// 2. Server sends salt and B
// 3. Client calculates A and M1, sends both
// 4. Server verifies M1, sends M2
// 5. Client verifies M2
```

Authentication data is delivered during connection setup via `op_accept_data` / `op_cond_accept`
and continued with `op_cont_auth` until the server indicates completion.

### Authentication Continuation (op_cont_auth)

Authentication plugins exchange additional data using `op_cont_auth`:

```c
struct P_AUTH_CONT {
    CSTRING p_data;   // Plugin-specific data
    CSTRING p_name;   // Plugin name
    CSTRING p_list;   // Plugin list
    CSTRING p_keys;   // Keys available on server
};
```

### Authentication Plugin Negotiation Rules

- The client advertises supported plugins in `CNCT_plugin_list` (connect) and
  `isc_dpb_auth_plugin_list` (attach).
- The server selects a plugin and returns it in `P_ACPD.p_acpt_plugin`.
- If authentication is not complete, the server uses `op_accept_data` or
  `op_cond_accept` with `p_acpt_authenticated = 0`, followed by one or more
  `op_cont_auth` exchanges.
- The client must echo the selected plugin name in each `op_cont_auth` packet.
- On completion, the server sets `p_acpt_authenticated = 1` and the attach
  proceeds.
- Protocol 10 clients may only use legacy authentication (no `op_cont_auth`).

### Legacy Authentication

```c
struct legacy_auth {
    xdr_string user_name;
    xdr_string encrypted_password;
};

// Password encryption (legacy)
void encrypt_password(const char* password, const char* key, char* result) {
    // ENC_crypt algorithm (DES-based)
    for (int i = 0; i < strlen(password); i++) {
        result[i] = password[i] ^ key[i % strlen(key)];
    }
}
```

## Statement Execution

### 1. Allocate Statement

```c
// op_allocate_statement uses P_RLSE with p_rlse_object = database handle
typedef struct p_rlse
{
    OBJCT p_rlse_object;  // Database object id
} P_RLSE;

// Response (op_response -> P_RESP)
struct P_RESP {
    OBJCT   p_resp_object;  // Object id
    SQUAD   p_resp_blob_id; // Blob id (if applicable)
    CSTRING p_resp_data;    // Response data (XDR string)
    // Followed by status vector (isc_arg_* ... isc_arg_end)
};
```

### 2. Prepare Statement / Execute Immediate (P_SQLST)

`op_prepare_statement`, `op_exec_immediate`, and `op_exec_immediate2` use `P_SQLST`
(defined below). On-wire field order is:

**op_prepare_statement / op_exec_immediate**
1. `p_sqlst_transaction` (xdr_short)
2. `p_sqlst_statement` (xdr_short) — for `op_exec_immediate`, this is 0
3. `p_sqlst_SQL_dialect` (xdr_short)
4. `p_sqlst_SQL_str` (xdr_cstring_const)
5. `p_sqlst_items` (xdr_cstring_const)
6. `p_sqlst_buffer_length` (xdr_long)
7. `p_sqlst_flags` (xdr_short) — only if protocol >= `PROTOCOL_PREPARE_FLAG`

**op_exec_immediate2**
1. `p_sqlst_blr` (xdr_sql_blr)
2. `p_sqlst_message_number` (xdr_short)
3. `p_sqlst_messages` (xdr_short)
4. If `p_sqlst_messages` > 0, `xdr_sql_message`
5. `p_sqlst_out_blr` (xdr_sql_blr)
6. `p_sqlst_out_message_number` (xdr_short)
7. `p_sqlst_inline_blob_size` (xdr_u_long) — only if protocol >= `PROTOCOL_INLINE_BLOB`
8. Then the same fields as `op_exec_immediate` (transaction, statement, dialect, SQL, items, buffer length, optional flags)

Describe items:
```c
#define isc_info_sql_stmt_type    21
#define isc_info_sql_get_plan     22
#define isc_info_sql_records      23
#define isc_info_sql_batch_fetch  24

#define isc_info_sql_stmt_select         1
#define isc_info_sql_stmt_insert         2
#define isc_info_sql_stmt_update         3
#define isc_info_sql_stmt_delete         4
#define isc_info_sql_stmt_ddl            5
#define isc_info_sql_stmt_exec_procedure 8
```

### 2.1 Protocol-Accurate DSQL Structures (protocol.h)

These are the exact wire-level structures used by the DSQL opcodes.

```c
// Statement flags
#define STMT_NO_BATCH       2
#define STMT_DEFER_EXECUTE  4

enum P_FETCH {
    fetch_next      = 0,
    fetch_prior     = 1,
    fetch_first     = 2,
    fetch_last      = 3,
    fetch_absolute  = 4,
    fetch_relative  = 5
};

typedef struct p_sqlst
{
    OBJCT         p_sqlst_transaction;       // Transaction object
    OBJCT         p_sqlst_statement;         // Statement object
    USHORT        p_sqlst_SQL_dialect;       // SQL dialect
    CSTRING_CONST p_sqlst_SQL_str;           // SQL string
    ULONG         p_sqlst_buffer_length;     // Target buffer length
    CSTRING_CONST p_sqlst_items;             // Describe items
    CSTRING       p_sqlst_blr;               // BLR describing input message
    USHORT        p_sqlst_message_number;
    USHORT        p_sqlst_messages;          // Message count
    CSTRING       p_sqlst_out_blr;           // BLR describing output message
    USHORT        p_sqlst_out_message_number;
    USHORT        p_sqlst_flags;             // Prepare flags
    ULONG         p_sqlst_inline_blob_size;  // Max inline blob size
} P_SQLST;

typedef struct p_sqldata
{
    OBJCT   p_sqldata_statement;        // Statement object
    OBJCT   p_sqldata_transaction;      // Transaction object
    CSTRING p_sqldata_blr;              // BLR describing input message
    USHORT  p_sqldata_message_number;
    USHORT  p_sqldata_messages;         // Message count
    CSTRING p_sqldata_out_blr;          // BLR describing output message
    USHORT  p_sqldata_out_message_number;
    ULONG   p_sqldata_status;           // EOF status
    ULONG   p_sqldata_timeout;          // Statement timeout
    ULONG   p_sqldata_cursor_flags;     // Cursor flags
    P_FETCH p_sqldata_fetch_op;         // Fetch operation
    SLONG   p_sqldata_fetch_pos;        // Fetch position
    ULONG   p_sqldata_inline_blob_size; // Max inline blob size
} P_SQLDATA;

typedef struct p_sqlfree
{
    OBJCT  p_sqlfree_statement;  // Statement object
    USHORT p_sqlfree_option;     // Option
} P_SQLFREE;

typedef struct p_sqlcur
{
    OBJCT         p_sqlcur_statement;    // Statement object
    CSTRING_CONST p_sqlcur_cursor_name;  // Cursor name
    USHORT        p_sqlcur_type;         // Cursor type
} P_SQLCUR;
```

Wire mapping:
- `op_prepare_statement` → `P_SQLST`
- `op_execute`, `op_execute2`, `op_fetch`, `op_fetch_scroll`, `op_fetch_response`, `op_sql_response` → `P_SQLDATA`
- `op_free_statement` → `P_SQLFREE`
- `op_set_cursor` → `P_SQLCUR`

`p_sqlst_inline_blob_size` and `p_sqldata_inline_blob_size` set the maximum size of blobs that can be sent via `op_inline_blob`.

### 3. Execute / Fetch (P_SQLDATA)

`op_execute`, `op_execute2`, `op_fetch`, `op_fetch_scroll`, `op_fetch_response`,
and `op_sql_response` use `P_SQLDATA` (defined below). On-wire field order is:

**op_execute**
1. `p_sqldata_statement` (xdr_short)
2. `p_sqldata_transaction` (xdr_short)
3. `p_sqldata_blr` (xdr_sql_blr)
4. `p_sqldata_message_number` (xdr_short)
5. `p_sqldata_messages` (xdr_short)
6. If `p_sqldata_messages` > 0, `xdr_sql_message`
7. `p_sqldata_timeout` (xdr_u_long) — only if protocol >= `PROTOCOL_STMT_TOUT`
8. `p_sqldata_cursor_flags` (xdr_u_long) — only if protocol >= `PROTOCOL_FETCH_SCROLL`
9. `p_sqldata_inline_blob_size` (xdr_u_long) — only if protocol >= `PROTOCOL_INLINE_BLOB`

**op_execute2**
1. Same as `op_execute`
2. `p_sqldata_out_blr` (xdr_sql_blr)
3. `p_sqldata_out_message_number` (xdr_short)
4. Optional fields (timeout/cursor/inline) as above

**op_fetch**
1. `p_sqldata_statement` (xdr_short)
2. `p_sqldata_blr` (xdr_sql_blr) — output format; may be empty to reuse cached format
3. `p_sqldata_message_number` (xdr_short)
4. `p_sqldata_messages` (xdr_short) — fetch count

**op_fetch_scroll**
1. Same as `op_fetch`
2. `p_sqldata_fetch_op` (xdr_short)
3. `p_sqldata_fetch_pos` (xdr_long)

**op_fetch_response**
1. `p_sqldata_status` (xdr_long) — 0=data, 100=EOF
2. `p_sqldata_messages` (xdr_short)
3. If `p_sqldata_messages` > 0, `xdr_sql_message` (row data)

**op_sql_response**
1. `p_sqldata_messages` (xdr_short)
2. If `p_sqldata_messages` > 0, `xdr_sql_message`

`xdr_sql_blr` is an XDR `CSTRING` (32-bit length + data + 4-byte padding).
`xdr_sql_message` is encoded inline using `xdr_packed_message` (protocol 13+)
or `xdr_message` (older), with no length prefix; XDR alignment applies to each field.

### 5. Info Requests (P_INFO)

Used by `op_info_database`, `op_info_request`, `op_info_transaction`, `op_info_blob`,
`op_info_sql`, `op_info_batch`, and `op_info_cursor`:

```c
typedef struct p_info
{
    OBJCT         p_info_object;        // Object id
    USHORT        p_info_incarnation;   // Object incarnation
    CSTRING_CONST p_info_items;         // Info items (request)
    CSTRING_CONST p_info_recv_items;    // Info items (service only)
    ULONG         p_info_buffer_length; // Target buffer length
} P_INFO;
```

## Data Representation

### SQL Data Type Encoding

```c
// Firebird SQL types
#define SQL_TEXT         452   // CHAR
#define SQL_VARYING      448   // VARCHAR
#define SQL_SHORT        500   // SMALLINT
#define SQL_LONG         496   // INTEGER
#define SQL_FLOAT        482   // FLOAT
#define SQL_DOUBLE       480   // DOUBLE
#define SQL_D_FLOAT      530   // DOUBLE PRECISION
#define SQL_TIMESTAMP    510   // TIMESTAMP
#define SQL_BLOB         520   // BLOB
#define SQL_ARRAY        540   // ARRAY
#define SQL_QUAD         550   // QUAD (internal)
#define SQL_TYPE_TIME    560   // TIME
#define SQL_TYPE_DATE    570   // DATE
#define SQL_INT64        580   // BIGINT
#define SQL_BOOLEAN      32764 // BOOLEAN (FB 3.0+)
#define SQL_NULL         32766 // NULL

// Nullable flag
#define SQL_NULLABLE     1

// Check if nullable
#define IS_NULLABLE(type) ((type) & SQL_NULLABLE)
#define GET_DTYPE(type)   ((type) & ~SQL_NULLABLE)
```

### Message Format Descriptor

```c
struct xsqlda {  // eXtended SQL Descriptor Area
    int16_t version;     // Version (SQLDA_VERSION1)
    int16_t sqln;        // Number of fields allocated
    int16_t sqld;        // Actual number of fields
    
    struct xsqlvar {
        int16_t sqltype;     // Data type + nullable flag
        int16_t sqlscale;    // Scale (for NUMERIC/DECIMAL)
        int16_t sqllen;      // Data length
        uint8_t* sqldata;    // Pointer to data
        int16_t* sqlind;     // Null indicator
        int16_t sqlname_length;
        char sqlname[32];    // Field name
        int16_t relname_length;
        char relname[32];    // Relation name
        int16_t ownname_length;
        char ownname[32];    // Owner name
        int16_t aliasname_length;
        char aliasname[32];  // Alias name
    } sqlvar[];
};
```

### XSQLDA Wire Packing and Alignment

When `isc_info_sql_sqlda_*` items are requested, XSQLDA is returned as a packed
sequence of XDR values:
- All 16-bit fields are encoded as XDR `short` and padded to 4 bytes.
- Strings are encoded as `xdr_short` length followed by bytes and XDR padding.
- Pointer fields (`sqldata`, `sqlind`) are **not** transmitted on the wire.

Field order per `xsqlvar` (wire):
1. `sqltype`
2. `sqlscale`
3. `sqlsubtype`
4. `sqllen`
5. `sqlname_length` + `sqlname`
6. `relname_length` + `relname`
7. `ownname_length` + `ownname`
8. `aliasname_length` + `aliasname`

Array/BLOB specifics:
- `SQL_ARRAY` and `SQL_BLOB` use `sqllen = 8` (quad id size).
- `sqlsubtype` carries the subtype (text/binary) where applicable.
- Nullable fields set `SQL_NULLABLE` in `sqltype`; null indicators appear in
  the message stream, not in XSQLDA.

### Binary Data Encoding

```c
// Integer types (network byte order)
int16_t encode_smallint(int16_t value) {
    return htons(value);
}

int32_t encode_integer(int32_t value) {
    return htonl(value);
}

int64_t encode_bigint(int64_t value) {
    return htobe64(value);
}

// Floating point (IEEE 754, big-endian)
float encode_float(float value) {
    uint32_t* p = (uint32_t*)&value;
    *p = htonl(*p);
    return value;
}

double encode_double(double value) {
    uint64_t* p = (uint64_t*)&value;
    *p = htobe64(*p);
    return value;
}

// Timestamp (8 bytes)
struct fb_timestamp {
    int32_t date;  // Days since November 17, 1858
    uint32_t time; // Time in 100 microseconds since midnight
};

// Date encoding
int32_t encode_date(int year, int month, int day) {
    // Convert to Modified Julian Date
    int a = (14 - month) / 12;
    int y = year + 4800 - a;
    int m = month + 12 * a - 3;
    
    int jd = day + (153 * m + 2) / 5 + 365 * y + 
             y / 4 - y / 100 + y / 400 - 32045;
    
    return jd - 2400001;  // Convert to Firebird epoch
}
```

## BLOB Operations

### Create BLOB

```c
struct op_create_blob_packet {
    uint32_t op_code = op_create_blob;  // 34
    uint32_t op_transaction;  // Transaction handle
};

struct op_create_blob2_packet {
    uint32_t op_code = op_create_blob2;  // 57
    uint32_t op_bpb_length;   // BPB length
    uint8_t op_bpb[];         // Blob Parameter Block
    uint32_t op_transaction;
};

// Response includes blob_id
struct blob_id {
    uint32_t bid_high;
    uint32_t bid_low;
};
```

### Put Segment

```c
struct op_put_segment_packet {
    uint32_t op_code = op_put_segment;  // 37
    uint32_t op_blob;         // Blob handle
    uint32_t op_length;       // Segment length
    uint8_t op_segment[];     // Segment data
};
```

### Get Segment

```c
struct op_get_segment_packet {
    uint32_t op_code = op_get_segment;  // 36
    uint32_t op_blob;         // Blob handle
    uint32_t op_length;       // Max segment length
    uint32_t op_segment;      // Segment number (0 = next)
};
```

### BLOB Segmentation Rules

- Segment length is limited to 16-bit (max 65535). `op_put_segment` must not
  send a segment larger than 65535 bytes.
- `op_get_segment` returns the next segment (or part of it) in `p_resp_data`.
- If the segment is longer than the requested length, return warning
  `isc_segment` and include the partial bytes.
- When the end of the blob is reached, return warning `isc_segstr_eof`.
- Segment lengths are encoded as XDR values; segment payload is padded to a
  4-byte boundary.

## Transaction Management

### Start Transaction

```c
struct op_transaction_packet {
    uint32_t op_code = op_transaction;  // 29
    uint32_t op_database;     // Database handle
    uint32_t op_tpb_length;   // TPB length
    uint8_t op_tpb[];         // Transaction Parameter Block
};

// Transaction Parameter Block items
#define isc_tpb_version1         1
#define isc_tpb_version3         3
#define isc_tpb_consistency      1   // Table-level locking
#define isc_tpb_concurrency      2   // Snapshot isolation
#define isc_tpb_shared           3   // Shared locks
#define isc_tpb_protected        4   // Protected locks
#define isc_tpb_exclusive        5   // Exclusive locks
#define isc_tpb_wait             6   // Wait on locks
#define isc_tpb_nowait           7   // No wait on locks
#define isc_tpb_read             8   // Read-only
#define isc_tpb_write            9   // Read-write
#define isc_tpb_lock_read        10  // Lock for read
#define isc_tpb_lock_write       11  // Lock for write
#define isc_tpb_verb_time        12  // Verb time
#define isc_tpb_commit_time      13  // Commit time
#define isc_tpb_ignore_limbo     14  // Ignore limbo
#define isc_tpb_read_committed   15  // Read committed
#define isc_tpb_autocommit       16  // Auto-commit
#define isc_tpb_rec_version      17  // Record version
#define isc_tpb_no_rec_version   18  // No record version
#define isc_tpb_restart_requests 19  // Restart requests
#define isc_tpb_no_auto_undo     20  // No auto undo
```

TPB format: `byte version` followed by a sequence of item bytes (some items are followed by additional length/value bytes, per Firebird docs).

Example TPB:
```c
uint8_t* build_tpb() {
    uint8_t tpb[] = {
        isc_tpb_version3,
        isc_tpb_write,
        isc_tpb_read_committed,
        isc_tpb_rec_version,
        isc_tpb_nowait
    };
    return tpb;
}
```

### Commit/Rollback

```c
struct op_commit_packet {
    uint32_t op_code = op_commit;  // 30
    uint32_t op_transaction;  // Transaction handle
};

struct op_rollback_packet {
    uint32_t op_code = op_rollback;  // 31
    uint32_t op_transaction;
};

struct op_commit_retaining_packet {
    uint32_t op_code = op_commit_retaining;  // 50
    uint32_t op_transaction;
};
```

## Event Notification

```c
struct op_que_events_packet {
    uint32_t op_code = op_que_events;  // 48
    uint32_t op_database;     // Database handle
    uint32_t op_events_length; // EPB length
    uint8_t op_events[];      // Event Parameter Block
    uint32_t op_ast;          // AST routine address
    uint32_t op_arg;          // AST argument
    uint32_t op_event_id;     // Local event ID
};

struct op_event_packet {
    uint32_t op_code = op_event;  // 52
    uint32_t op_database;
    uint32_t op_events_length;
    uint8_t op_events[];      // Event counts
    uint32_t op_ast;
    uint32_t op_arg;
    uint32_t op_event_id;
};
```

### Event Parameter Block (EPB) Format

The EPB is the buffer built by `isc_event_block` and should be treated as an
opaque Firebird-compatible structure. ScratchBird interprets it as:

```
uint8  event_count;
repeat event_count times:
  uint8  name_length;
  char   name[name_length];
  uint32 event_count;  // XDR, initial value 0
```

`op_event` returns a buffer of the same length with updated event counts. The
client computes deltas using `isc_event_counts` and should re-arm the event
with a new `op_que_events` after each notification.

## Status Vector

```c
// Status vector format
struct status_vector {
    uint32_t isc_arg;     // Argument type
    uint32_t isc_code;    // Error/warning code
    // ... repeated pairs ...
    uint32_t isc_arg_end; // 0 = end of vector
};

// Argument types
#define isc_arg_end          0  // End of arguments
#define isc_arg_gds          1  // Error code
#define isc_arg_string       2  // String argument
#define isc_arg_cstring      3  // C string
#define isc_arg_number       4  // Numeric argument
#define isc_arg_interpreted  5  // Interpreted status
#define isc_arg_vms          6  // VMS status
#define isc_arg_unix         7  // Unix error
#define isc_arg_domain       8  // Domain error
#define isc_arg_dos          9  // DOS error
#define isc_arg_mpexl        10 // MPE/XL error
#define isc_arg_mpexl_ipc    11 // MPE/XL IPC error
#define isc_arg_next_mach    15 // NeXT/Mach error
#define isc_arg_netware      16 // NetWare error
#define isc_arg_win32        17 // Win32 error
#define isc_arg_warning      18 // Warning
#define isc_arg_sql_state    19 // SQLSTATE
```

### Status Vector Mapping and SQLSTATE

ScratchBird returns a Firebird-compatible status vector for all responses:
- The first error is emitted as `isc_arg_gds` with a Firebird GDS code.
- If available, include `isc_arg_sql_state` with a 5-character SQLSTATE.
- Optional message parameters use `isc_arg_string` / `isc_arg_cstring`.
- Warnings are appended with `isc_arg_warning` after the primary error.

Example (SQLSTATE + message parameter):
```
isc_arg_gds, <gds_code>,
isc_arg_sql_state, "42000",
isc_arg_string, "duplicate key value violates unique constraint",
isc_arg_end
```

## Protocol State Machine

```
    ┌────────────┐
    │Disconnected│
    └─────┬──────┘
          │ op_connect
          ▼
    ┌────────────┐
    │ Connecting │
    └─────┬──────┘
          │ op_accept
          ▼
    ┌────────────┐
    │  Connected │
    └─────┬──────┘
          │ op_attach
          ▼
    ┌────────────┐
    │  Attached  │◄─────────┐
    └─────┬──────┘          │
          │                  │
          ▼                  │
    ┌────────────┐          │
    │Transaction │          │
    └─────┬──────┘          │
          │                  │
          ▼                  │
    ┌────────────┐          │
    │ Statement  │──────────┘
    └────────────┘    op_response
```

## Wire Compression

Firebird supports zlib compression:

```c
struct compressed_packet {
    uint32_t uncompressed_length;
    uint32_t compressed_length;
    uint8_t compressed_data[];  // zlib compressed
};

// Enable compression in DPB
#define isc_dpb_wire_compression 126
#define isc_dpb_wire_compression_level 127
```

### Compression Negotiation and Framing

- Clients request compression via `isc_dpb_wire_compression` (and optionally
  `isc_dpb_wire_compression_level`).
- If the server accepts compression, all subsequent packets on that connection
  are framed as `compressed_packet` records.
- If compression is not enabled or the compressed output is larger than the
  input, the server may send the uncompressed payload with
  `compressed_length = 0`.
- Invalid compressed payloads yield a protocol error and the connection is
  closed.

## Example: Complete Query Execution

### 1. Allocate Statement
```
Client → Server:
00 00 00 3E        // op_allocate_statement (62)
00 00 00 01        // p_rlse_object = database handle

Server → Client:
00 00 00 09        // op_response
00 00 00 01        // p_resp_object = statement handle
00 00 00 00 00 00 00 00  // p_resp_blob_id
00 00 00 00        // p_resp_data length
00 00 00 00        // status vector (isc_arg_end)
```

### 2. Prepare Statement
```
Client → Server:
00 00 00 44        // op_prepare_statement (68)
00 00 00 01        // p_sqlst_transaction
00 00 00 01        // p_sqlst_statement
00 00 00 03        // p_sqlst_SQL_dialect
00 00 00 13        // SQL length
53 45 4C 45 43 54 20 2A 20 46 52 4F 4D 20 75 73 65 72 73  // "SELECT * FROM users"
00                 // Padding (XDR to 4-byte boundary)
00 00 00 02        // p_sqlst_items length
15 01              // isc_info_sql_stmt_type, isc_info_end
00 00              // Padding
00 00 00 20        // p_sqlst_buffer_length (32 bytes)
```

### 3. Execute Statement
```
Client → Server:
00 00 00 3F        // op_execute (63)
00 00 00 01        // p_sqldata_statement
00 00 00 01        // p_sqldata_transaction
00 00 00 00        // p_sqldata_blr length (0 = reuse cached input format)
00 00 00 00        // p_sqldata_message_number
00 00 00 00        // p_sqldata_messages (no input messages)
```

### 4. Fetch Rows
```
Client → Server:
00 00 00 41        // op_fetch (65)
00 00 00 01        // p_sqldata_statement
00 00 00 00        // p_sqldata_blr length (0 = reuse cached output format)
00 00 00 00        // p_sqldata_message_number
00 00 00 64        // p_sqldata_messages (fetch 100 rows)

Server → Client:
00 00 00 42        // op_fetch_response (66)
00 00 00 64        // p_sqldata_status = 100 (EOF)
00 00 00 00        // p_sqldata_messages (no rows)
```

### Example: Simple Query (op_exec_immediate)

Client `op_exec_immediate` with SQL `SELECT 1 FROM RDB$DATABASE`:
```
00 00 00 40  // op_exec_immediate
00 00 00 01  // p_sqlst_transaction
00 00 00 00  // p_sqlst_statement (0 for immediate)
00 00 00 03  // p_sqlst_SQL_dialect
00 00 00 1a  // SQL length
53 45 4c 45 43 54 20 31 20 46 52 4f 4d 20 52 44 42 24 44 41 54 41 42 41 53 45  // "SELECT 1 FROM RDB$DATABASE"
00 00        // Padding (XDR)
00 00 00 00  // p_sqlst_items length
00 00 00 00  // p_sqlst_buffer_length
```

Server `op_response` success (empty status vector):
```
00 00 00 09  // op_response
00 00 00 00  // p_resp_object
00 00 00 00 00 00 00 00  // p_resp_blob_id
00 00 00 00  // p_resp_data length
00 00 00 00  // isc_arg_end
```

### Example: Batch Operations (Hex)

Assume protocol 17+, statement id `5`, transaction id `3`, and a statement with
one nullable INTEGER input parameter (message length = 6, aligned length = 8).

Client `op_batch_create`:
```
00 00 00 63  // op_batch_create
00 00 00 05  // p_batch_statement
00 00 00 0c  // p_batch_blr length (12)
05 02 04 00 02 00 08 00 07 00 ff 4c  // BLR: msg 0, count=2 (field + null), blr_long, blr_short
00 00 00 06  // p_batch_msglen (IMessageMetadata::getMessageLength)
00 00 00 1c  // p_batch_pb length
01           // IBatch::VERSION1
02 04 00 00 00 01 00 00 00  // TAG_RECORD_COUNTS = 1
01 04 00 00 00 01 00 00 00  // TAG_MULTIERROR = 1
04 04 00 00 00 03 00 00 00  // TAG_BLOB_POLICY = BLOB_STREAM
```

Client `op_batch_msg`:
```
00 00 00 64  // op_batch_msg
00 00 00 05  // p_batch_statement
00 00 00 03  // p_batch_messages (3 executions)
00 00 00 00  // msg[0] null bitmap (1 byte + XDR padding)
00 00 00 2a  // msg[0] param value (INT32, XDR)
00 00 00 00  // msg[1] null bitmap (1 byte + XDR padding)
00 00 00 07  // msg[1] param value (INT32, XDR)
01 00 00 00  // msg[2] null bitmap (bit0 set => NULL; no value bytes follow)
```

Packed VARCHAR example (separate statement id `6`, one nullable VARCHAR(5) input):
```
BLR (nullable VARCHAR(5)):
05 02 04 00 02 00 25 05 00 07 00 ff 4c

Client `op_batch_msg`:
00 00 00 64  // op_batch_msg
00 00 00 06  // p_batch_statement
00 00 00 02  // p_batch_messages (2 executions)
00 00 00 00  // msg[0] null bitmap (not null)
00 00 00 02  // msg[0] varchar length (xdr_short)
68 69 00 00  // msg[0] "hi" + XDR padding
01 00 00 00  // msg[1] null bitmap (bit0 set => NULL; no length/value follow)
```

Client `op_batch_blob_stream` (alignment = 4, one unsegmented blob, id high=0 low=1, data = DE AD BE EF):
```
00 00 00 69  // op_batch_blob_stream
00 00 00 05  // p_batch_statement
00 00 00 14  // stream_len (20 bytes)
00 00 00 00 00 00 00 01  // batch_blob_id (ISC_QUAD: high=0, low=1)
00 00 00 04  // blob_size
00 00 00 00  // bpb_size
de ad be ef  // blob data (4 bytes)
```

Client `op_batch_blob_stream` (segmented, alignment = 4, BPB sets segmented):
```
00 00 00 69  // op_batch_blob_stream
00 00 00 05  // p_batch_statement
00 00 00 20  // stream_len (32 bytes, internal stream length)
00 00 00 00 00 00 00 02  // batch_blob_id (ISC_QUAD: high=0, low=2)
00 00 00 0c  // blob_size (12 bytes: 2+3+1+2+4)
00 00 00 04  // bpb_size
01 03 01 00  // BPB: version1, isc_bpb_type, len=1, segmented=0
00 00 00 03  // seg[0] length (xdr_u_short)
61 62 63     // seg[0] data "abc"
00           // padding to BLOB_SEGHDR_ALIGN=2
00 00 00 04  // seg[1] length (xdr_u_short)
77 78 79 7a  // seg[1] data "wxyz"
```
Note: `stream_len` counts internal bytes (segment headers are 2 bytes); XDR encodes
segment lengths as 4-byte `xdr_u_short` values on the wire.

Client `op_batch_exec`:
```
00 00 00 65  // op_batch_exec
00 00 00 05  // p_batch_statement
00 00 00 03  // p_batch_transaction
```

Server `op_batch_cs` (3 updates, no errors):
```
00 00 00 67  // op_batch_cs
00 00 00 05  // p_batch_statement
00 00 00 03  // p_batch_reccount
00 00 00 03  // p_batch_updates
00 00 00 00  // p_batch_vectors
00 00 00 00  // p_batch_errors
00 00 00 01  // update count for record 0
00 00 00 01  // update count for record 1
00 00 00 01  // update count for record 2
```

Client `op_info_batch`:
```
00 00 00 6f  // op_info_batch
00 00 00 05  // p_info_object (statement/batch id)
00 00 00 00  // p_info_incarnation
00 00 00 04  // p_info_items length
0a 0b 0d 01  // INF_BUFFER_BYTES_SIZE, INF_DATA_BYTES_SIZE, INF_BLOB_ALIGNMENT, isc_info_end
00 00 00 80  // p_info_buffer_length (128)
```

Server `op_response` (batch info):
```
00 00 00 09  // op_response
00 00 00 05  // p_resp_object (statement/batch id)
00 00 00 00 00 00 00 00  // p_resp_blob_id
00 00 00 16  // p_resp_data length (22)
0a 04 00 00 00 01 00  // INF_BUFFER_BYTES_SIZE = 65536
0b 04 00 18 00 00 00  // INF_DATA_BYTES_SIZE = 24
0d 04 00 04 00 00 00  // INF_BLOB_ALIGNMENT = 4
01                    // isc_info_end
00 00                 // Padding (XDR)
00 00 00 00           // isc_arg_end
```

Server `op_response` (for `op_batch_blob_stream` success):
```
00 00 00 09  // op_response
00 00 00 00  // p_resp_object
00 00 00 00 00 00 00 00  // p_resp_blob_id
00 00 00 00  // p_resp_data length
00 00 00 00  // isc_arg_end
```

## Security Considerations

1. **Use SRP authentication** instead of legacy methods
2. **Enable wire encryption** (Firebird 3.0+)
3. **Use wire compression** to reduce attack surface
4. **Validate packet lengths** to prevent buffer overflows
5. **Implement connection limits** per user
6. **Use prepared statements** to prevent SQL injection
7. **Monitor for protocol violations**
8. **Regular security updates**

## Protocol Versions

- Protocol 10: Firebird 1.0-1.5 (InterBase 6.0)
- Protocol 11: Firebird 2.0-2.5
- Protocol 12: Firebird 3.0
- Protocol 13: Firebird 4.0

## References

- Firebird Source: src/remote/protocol.h
- Firebird Wire Protocol Documentation
- XDR Specification (RFC 1832)
- Wireshark Firebird Dissector
- ScratchBird BLR to SBLR mapping: ScratchBird/docs/specifications/FIREBIRD_BLR_TO_SBLR_MAPPING.md
