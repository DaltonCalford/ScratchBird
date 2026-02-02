# ScratchBird Native Wire Protocol Specification

**Version:** 1.1
**Date:** December 10, 2025
**Status:** Design Complete - Ready for Implementation
**Port:** 3092 (TCP) - IANA Unassigned

---

## Table of Contents

1. [Overview](#1-overview)
2. [Design Goals](#2-design-goals)
3. [Connection Lifecycle](#3-connection-lifecycle)
4. [Message Format](#4-message-format)
5. [Authentication Protocol](#5-authentication-protocol)
6. [Cluster Key Infrastructure](#6-cluster-key-infrastructure)
7. [Query Protocol](#7-query-protocol)
8. [Result Protocol](#8-result-protocol)
9. [Prepared Statements](#9-prepared-statements)
10. [Federation Protocol](#10-federation-protocol)
11. [Transaction Protocol](#11-transaction-protocol)
12. [Streaming Protocol](#12-streaming-protocol)
13. [Notification Protocol](#13-notification-protocol)
14. [Error Handling](#14-error-handling)
15. [Type Serialization](#15-type-serialization)
16. [Compression](#16-compression)
17. [Security Considerations](#17-security-considerations)
18. [Performance Optimizations](#18-performance-optimizations)
19. [Client Implementation Guide](#19-client-implementation-guide)

---

## 1. Overview

The ScratchBird Native Wire Protocol (SBWP) is a binary protocol optimized for ScratchBird's unique features:

- **MGA Transaction Semantics** - Full visibility control, record versioning
- **86 Native Data Types** - Direct serialization without type conversion
- **SBLR Bytecode** - Optional compiled query transmission
- **Cluster Federation** - Cross-database queries with cryptographic trust
- **Streaming Results** - Backpressure-aware result delivery

### Protocol Characteristics

| Property | Value |
|----------|-------|
| Port | 3092 (TCP) |
| Byte Order | Little-endian (native x86-64) |
| Encryption | TLS 1.3 required (no plaintext mode) |
| Compression | zstd (optional, negotiated) |
| Max Message | 1 GB (configurable) |
| Character Encoding | UTF-8 |

### Why a Native Protocol?

While ScratchBird supports PostgreSQL, MySQL, and Firebird protocols for compatibility (TDS is post-gold), the native protocol provides:

1. **Full MGA Semantics** - Other protocols lack visibility epoch, record version concepts
2. **All 86 Types** - No lossy type mapping to foreign type systems
3. **SBLR Transmission** - Skip parsing on subsequent executions
4. **Cluster Trust** - Built-in PKI for inter-database communication
5. **Streaming** - True bidirectional streaming with backpressure

---

## 2. Design Goals

### Primary Goals

1. **Performance** - Minimize serialization overhead, support zero-copy where possible
2. **Type Fidelity** - Exact representation of all ScratchBird types
3. **Security** - TLS 1.3 mandatory, cluster PKI, no security downgrade attacks
4. **Extensibility** - Version negotiation, feature flags, reserved fields
5. **Federation** - Native support for cross-database queries

### Non-Goals

1. Human readability (use psql/mysql for that)
2. Compatibility with other databases (that's what emulated protocols are for)
3. Backward compatibility (not required in Alpha; server accepts v1.1 only)

---

## 3. Connection Lifecycle

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        Connection State Machine                          │
└─────────────────────────────────────────────────────────────────────────┘

    ┌──────────┐
    │  CLOSED  │
    └────┬─────┘
         │ TCP connect
         ▼
    ┌──────────┐
    │   TLS    │ TLS 1.3 handshake
    │HANDSHAKE │ (mandatory)
    └────┬─────┘
         │ TLS established
         ▼
    ┌──────────┐
    │ STARTUP  │ Protocol version negotiation
    │          │ Feature flags exchange
    └────┬─────┘
         │ Version accepted
         ▼
    ┌──────────┐
    │   AUTH   │ Authentication (password, cert, LDAP, etc.)
    │          │ Optional MFA challenge
    └────┬─────┘
         │ Auth success
         ▼
    ┌──────────┐
    │  READY   │◄────────────────────────────────────┐
    │ (TXN ON) │ Ready for queries, always in txn     │
    └────┬─────┘                                      │
         │ Query/Prepare/Execute                      │
         ▼                                            │
    ┌──────────┐                                      │
    │  QUERY   │ Query execution in progress          │
    │          │────────────────────────────────────►│
    └────┬─────┘ Query complete                       │
         │                                            │
         │ COMMIT/ROLLBACK (new txn auto-starts)      │
         ▼                                            │
    ┌──────────┐                                      │
    │ STREAM   │ Streaming results with backpressure  │
    │          │────────────────────────────────────►│
    └────┬─────┘ Stream complete                      │
         │
         │ Terminate
         ▼
    ┌──────────┐
    │  CLOSED  │
    └──────────┘
```

**Always-in-transaction:** After authentication, the server creates a default attachment and immediately starts a transaction. The server returns the attachment_id and txn_id; clients must include them in all subsequent messages.

---

## 4. Message Format

### 4.1 Message Header (40 bytes)

All messages begin with a fixed 40-byte header:

```c
struct MessageHeader {
    uint32_t magic;          // 0x5342_5750 ('SBWP' in ASCII)
    uint8_t  version_major;  // Protocol major version (1)
    uint8_t  version_minor;  // Protocol minor version (1)
    uint8_t  msg_type;       // Message type code
    uint8_t  flags;          // Message flags
    uint32_t length;         // Payload length (excluding header)
    uint32_t sequence;       // Sequence number (for pipelining)
    uint8_t  attachment_id[16]; // Attachment UUID (required after AUTH_OK)
    uint64_t txn_id;            // Transaction ID (required for transactional messages)
};
```

**Version requirement:** Protocol v1.1 (minor=1) is required. v1.0 (16-byte header) is not supported in Alpha builds.
**Header requirements:**
- Before `AUTH_OK`, `attachment_id` and `txn_id` must be zero.
- After `AUTH_OK`, every message **must** include non-zero `attachment_id` and `txn_id`.
- `AUTH_OK` is the first message that includes the assigned non-zero `attachment_id` and `txn_id` in its header.
- `attachment_id`/`txn_id` values are assigned by the server; the client must echo them back on every request.

### 4.2 Message Flags

```c
#define MSG_FLAG_COMPRESSED     0x01  // Payload is zstd compressed
#define MSG_FLAG_CONTINUED      0x02  // More fragments follow
#define MSG_FLAG_FINAL          0x04  // Last fragment
#define MSG_FLAG_URGENT         0x08  // Priority message (cancel, etc.)
#define MSG_FLAG_ENCRYPTED      0x10  // Additional encryption (cluster keys)
#define MSG_FLAG_CHECKSUM       0x20  // CRC32 checksum appended
#define MSG_FLAG_RESERVED1      0x40  // Reserved (must be 0 in v1.1)
#define MSG_FLAG_RESERVED2      0x80  // Reserved (must be 0 in v1.1)
```

### 4.3 Message Types

#### Client → Server Messages (0x01 - 0x3F)

| Code | Name | Description |
|------|------|-------------|
| 0x01 | `STARTUP` | Initial connection setup |
| 0x02 | `AUTH_RESPONSE` | Authentication credential |
| 0x03 | `QUERY` | Simple query (SQL text) |
| 0x04 | `PARSE` | Prepare statement |
| 0x05 | `BIND` | Bind parameters to prepared statement |
| 0x06 | `DESCRIBE` | Get statement/portal metadata |
| 0x07 | `EXECUTE` | Execute prepared statement |
| 0x08 | `CLOSE` | Close statement/portal |
| 0x09 | `SYNC` | Synchronization point |
| 0x0A | `FLUSH` | Flush output buffer |
| 0x0B | `CANCEL` | Cancel current query |
| 0x0C | `TERMINATE` | Close connection |
| 0x0D | `COPY_DATA` | COPY protocol data |
| 0x0E | `COPY_DONE` | COPY complete |
| 0x0F | `COPY_FAIL` | COPY failed |
| 0x10 | `SBLR_EXECUTE` | Execute SBLR bytecode directly |
| 0x11 | `SUBSCRIBE` | Subscribe to notifications |
| 0x12 | `UNSUBSCRIBE` | Unsubscribe from notifications |
| 0x13 | `FEDERATED_QUERY` | Cross-database query |
| 0x14 | `STREAM_CONTROL` | Backpressure control |
| 0x15 | `TXN_BEGIN` | Explicit transaction start |
| 0x16 | `TXN_COMMIT` | Commit transaction |
| 0x17 | `TXN_ROLLBACK` | Rollback transaction |
| 0x18 | `TXN_SAVEPOINT` | Create savepoint |
| 0x19 | `TXN_RELEASE` | Release savepoint |
| 0x1A | `TXN_ROLLBACK_TO` | Rollback to savepoint |
| 0x1B | `PING` | Keepalive ping |
| 0x1C | `SET_OPTION` | Set session option |
| 0x1D | `CLUSTER_AUTH` | Cluster inter-database auth |
| 0x1E | `ATTACH_CREATE` | Create a new attachment on this connection |
| 0x1F | `ATTACH_DETACH` | Detach and destroy an attachment |
| 0x20 | `ATTACH_LIST` | List attachments on this connection |

#### Server → Client Messages (0x40 - 0x7F)

| Code | Name | Description |
|------|------|-------------|
| 0x40 | `AUTH_REQUEST` | Request authentication |
| 0x41 | `AUTH_OK` | Authentication successful |
| 0x42 | `AUTH_CONTINUE` | Continue auth (MFA, SASL) |
| 0x43 | `READY` | Ready for query |
| 0x44 | `ROW_DESCRIPTION` | Column metadata |
| 0x45 | `DATA_ROW` | Result row data |
| 0x46 | `COMMAND_COMPLETE` | Query completed |
| 0x47 | `EMPTY_QUERY` | Empty query string |
| 0x48 | `ERROR` | Error response |
| 0x49 | `NOTICE` | Non-fatal notice |
| 0x4A | `PARSE_COMPLETE` | Statement parsed |
| 0x4B | `BIND_COMPLETE` | Parameters bound |
| 0x4C | `CLOSE_COMPLETE` | Statement/portal closed |
| 0x4D | `PORTAL_SUSPENDED` | Portal execution suspended |
| 0x4E | `NO_DATA` | No rows returned |
| 0x4F | `PARAMETER_STATUS` | Server parameter changed |
| 0x50 | `PARAMETER_DESCRIPTION` | Parameter metadata |
| 0x51 | `COPY_IN_RESPONSE` | Ready for COPY data |
| 0x52 | `COPY_OUT_RESPONSE` | Starting COPY out |
| 0x53 | `COPY_BOTH_RESPONSE` | Bidirectional COPY |
| 0x54 | `NOTIFICATION` | Async notification |
| 0x55 | `FUNCTION_RESULT` | Function call result |
| 0x56 | `NEGOTIATE_VERSION` | Protocol version negotiation |
| 0x57 | `SBLR_COMPILED` | Compiled SBLR bytecode |
| 0x58 | `QUERY_PLAN` | Query execution plan |
| 0x59 | `STREAM_READY` | Ready to stream |
| 0x5A | `STREAM_DATA` | Streaming data chunk |
| 0x5B | `STREAM_END` | Stream complete |
| 0x5C | `TXN_STATUS` | Transaction status |
| 0x5D | `PONG` | Keepalive response |
| 0x5E | `CLUSTER_AUTH_OK` | Cluster auth successful |
| 0x5F | `FEDERATED_RESULT` | Cross-database result |

#### Bidirectional Messages (0x80 - 0xBF)

| Code | Name | Description |
|------|------|-------------|
| 0x80 | `HEARTBEAT` | Connection heartbeat |
| 0x81 | `EXTENSION` | Protocol extension |

---

### 4.4 Payload Conventions

- **Endianness**: all multi-byte integer fields are little-endian.
- **Variable-length fields**: unless explicitly documented as null-terminated, variable-length values are encoded as `uint32 length` followed by `length` bytes (no implicit null).
- **Header length**: `MessageHeader.length` is the payload size **only** (excludes header). It includes any compression header and any appended checksum.
- **Empty payloads**: messages with no payload set `length = 0`.

### 4.5 Fragmentation, Sequencing, and Checksums

**Fragmentation**
- A logical message may be split across multiple frames.
- All fragments carry the same `msg_type` and `sequence`.
- Intermediate fragments set `MSG_FLAG_CONTINUED`; the final fragment sets `MSG_FLAG_FINAL`.
- `length` is the payload length **per fragment**.

**Sequencing**
- Client increments `sequence` per request message (per attachment).
- Server echoes the same `sequence` in all response messages tied to that request.
- Asynchronous messages (NOTIFICATION, NOTICE, PARAMETER_STATUS) use `sequence = 0`.

**Checksum**
- If `MSG_FLAG_CHECKSUM` is set, append a 4-byte CRC32C (Castagnoli, polynomial 0x1EDC6F41, init 0xFFFFFFFF, final XOR 0xFFFFFFFF).
- CRC is computed over the payload bytes **as transmitted**, excluding the checksum itself.

## 5. Authentication Protocol

### 5.1 Authentication Flow

```
Client                                    Server
   │                                         │
   │──────────── STARTUP ───────────────────►│
   │  {version, database, user, options}     │
   │                                         │
   │◄─────────── AUTH_REQUEST ───────────────│
   │  {auth_method, challenge}               │
   │                                         │
   │──────────── AUTH_RESPONSE ─────────────►│
   │  {credentials}                          │
   │                                         │
   │◄─────────── AUTH_CONTINUE ──────────────│ (if MFA)
   │  {mfa_challenge}                        │
   │                                         │
   │──────────── AUTH_RESPONSE ─────────────►│
   │  {mfa_token}                            │
   │                                         │
   │◄─────────── AUTH_OK ────────────────────│
   │  {session_id, server_info}              │
   │                                         │
   │◄─────────── PARAMETER_STATUS ───────────│ (multiple)
   │  {param_name, param_value}              │
   │                                         │
   │◄─────────── READY ──────────────────────│
   │  {txn_status}                           │
```

### 5.2 STARTUP Message

```c
struct StartupMessage {
    MessageHeader header;  // msg_type = 0x01

    // Protocol version requested
    uint8_t  protocol_major;   // 1
    uint8_t  protocol_minor;   // 1
    uint16_t reserved;

    // Feature flags requested
    uint64_t features;

    // Connection parameters (null-terminated strings)
    // Format: key\0value\0key\0value\0...\0\0
    // Required keys: database, user
    // Optional: application_name, client_encoding, search_path, etc.
    char parameters[];
};

// Feature flags
#define FEATURE_COMPRESSION     (1ULL << 0)   // zstd compression
#define FEATURE_STREAMING       (1ULL << 1)   // Streaming results
#define FEATURE_SBLR            (1ULL << 2)   // SBLR bytecode execution
#define FEATURE_FEDERATION      (1ULL << 3)   // Cross-database queries
#define FEATURE_NOTIFICATIONS   (1ULL << 4)   // Async notifications
#define FEATURE_QUERY_PLAN      (1ULL << 5)   // Query plan transmission
#define FEATURE_BATCH           (1ULL << 6)   // Batch execution
#define FEATURE_PIPELINE        (1ULL << 7)   // Pipelined queries
#define FEATURE_BINARY_COPY     (1ULL << 8)   // Binary COPY format
#define FEATURE_SAVEPOINTS      (1ULL << 9)   // Savepoint support
#define FEATURE_2PC             (1ULL << 10)  // Two-phase commit
#define FEATURE_CHECKSUMS       (1ULL << 11)  // Message checksums
```

Note: The native CONNECT_REQUEST/CONNECT_RESPONSE handshake uses 16-bit
capability flags (CONNECT_FLAG_*) to advertise supported features. The server
returns its capability set in CONNECT_RESPONSE; compression is only enabled
when both client and server advertise the compression flag.

### 5.2.1 NEGOTIATE_VERSION Message

Server response when the requested protocol/features differ:

```c
struct NegotiateVersion {
    MessageHeader header;  // msg_type = 0x56

    uint8_t  accepted_major;
    uint8_t  accepted_minor;
    uint16_t reserved;

    uint64_t accepted_features;  // Bitset of features enabled by server
    uint32_t max_message_size;   // Server max payload size in bytes
    uint32_t reserved2;
};
```

### 5.3 AUTH_REQUEST Message

```c
struct AuthRequest {
    MessageHeader header;  // msg_type = 0x40

    uint8_t auth_method;   // Authentication method code
    uint8_t reserved[3];

    // Method-specific data
    union {
        struct {
            uint8_t salt[16];      // For password-based
        } password;

        struct {
            char mechanisms[];     // SASL mechanisms (null-separated)
        } sasl;

        struct {
            // Empty - use TLS client cert
        } certificate;

        struct {
            char realm[];          // Kerberos realm
        } gssapi;

        struct {
            char challenge[32];    // Server challenge
            char cluster_id[36];   // UUID of cluster
        } cluster;
    } data;
};

// Authentication methods
#define AUTH_OK              0   // No auth needed (trusted)
#define AUTH_PASSWORD        1   // Cleartext password (legacy)
#define AUTH_MD5             2   // MD5 hash
#define AUTH_SCRAM_SHA256    3   // SCRAM-SHA-256
#define AUTH_CERTIFICATE     4   // TLS client certificate
#define AUTH_GSSAPI          5   // Kerberos/GSSAPI
#define AUTH_SSPI            6   // Windows SSPI
#define AUTH_LDAP            7   // LDAP bind
#define AUTH_SAML            8   // SAML assertion
#define AUTH_OIDC            9   // OAuth2/OIDC token
#define AUTH_MFA_TOTP        10  // TOTP second factor
#define AUTH_CLUSTER_PKI     11  // Cluster inter-database auth
```

### 5.4 AUTH_RESPONSE Message

```c
struct AuthResponse {
    MessageHeader header;  // msg_type = 0x02

    // Response data depends on auth method
    // SCRAM-SHA-256: client-first-message, client-final-message
    // Password: password string
    // Certificate: empty (cert in TLS handshake)
    // GSSAPI: token bytes
    // MFA: 6-digit TOTP code
    // Cluster: signed challenge response
    uint8_t data[];
};
```

### 5.5 AUTH_CONTINUE Message

```c
struct AuthContinue {
    MessageHeader header;  // msg_type = 0x42

    uint8_t  auth_method;  // Same codes as AUTH_REQUEST
    uint8_t  auth_stage;   // 0=challenge,1=success,2=error (method-specific)
    uint16_t reserved;

    uint32_t data_length;
    uint8_t  data[];       // Method-specific payload
};
```

### 5.6 AUTH_OK Message

```c
struct AuthOk {
    MessageHeader header;  // msg_type = 0x41

    uint8_t  session_id[16];
    uint32_t server_info_length;
    uint8_t  server_info[];   // UTF-8 JSON (server_version, cluster_id, etc.)
};
```

`AUTH_OK` is the first message to carry the non-zero `attachment_id` and `txn_id` assigned by the server in the header.

### 5.7 SCRAM-SHA-256 Implementation

The SCRAM-SHA-256 implementation follows RFC 7677 with ScratchBird extensions:

```
Client                                    Server
   │                                         │
   │◄─────────── AUTH_REQUEST ───────────────│
   │  {method=SCRAM_SHA256, mechanisms}      │
   │                                         │
   │──────────── AUTH_RESPONSE ─────────────►│
   │  client-first-message:                  │
   │  n,,n=<user>,r=<client-nonce>          │
   │                                         │
   │◄─────────── AUTH_CONTINUE ──────────────│
   │  server-first-message:                  │
   │  r=<client-nonce><server-nonce>,        │
   │  s=<salt>,i=<iterations>                │
   │                                         │
   │──────────── AUTH_RESPONSE ─────────────►│
   │  client-final-message:                  │
   │  c=biws,r=<nonce>,p=<proof>            │
   │                                         │
   │◄─────────── AUTH_OK ────────────────────│
   │  server-final-message:                  │
   │  v=<server-signature>                   │
```

---

## 6. Cluster Key Infrastructure

### 6.1 Overview

ScratchBird clusters use a hybrid PKI model:

1. **Cluster CA** - Root of trust for all cluster members
2. **Database Certificates** - Each database has a CA-signed certificate
3. **Session Keys** - Ephemeral keys derived per connection (forward secrecy)

```
┌─────────────────────────────────────────────────────────────────────────┐
│                     Cluster Key Hierarchy                                │
└─────────────────────────────────────────────────────────────────────────┘

                    ┌─────────────────────┐
                    │   Cluster CA        │
                    │   (cluster.crt)     │
                    │   (cluster.key)     │
                    └─────────┬───────────┘
                              │ Signs
            ┌─────────────────┼─────────────────┐
            │                 │                 │
            ▼                 ▼                 ▼
    ┌───────────────┐ ┌───────────────┐ ┌───────────────┐
    │  Database A   │ │  Database B   │ │  Database C   │
    │  (db_a.crt)   │ │  (db_b.crt)   │ │  (db_c.crt)   │
    │  (db_a.key)   │ │  (db_b.key)   │ │  (db_c.key)   │
    │               │ │               │ │               │
    │ Subject:      │ │ Subject:      │ │ Subject:      │
    │ CN=db_a       │ │ CN=db_b       │ │ CN=db_c       │
    │ O=cluster_xyz │ │ O=cluster_xyz │ │ O=cluster_xyz │
    └───────────────┘ └───────────────┘ └───────────────┘
```

### 6.2 Cluster Certificate Format

```
Certificate:
    Data:
        Version: 3 (0x2)
        Serial Number: <unique>
        Signature Algorithm: sha384WithRSAEncryption
        Issuer: CN=ScratchBird Cluster CA, O=<cluster_id>
        Validity:
            Not Before: <creation_time>
            Not After:  <creation_time + 1 year>
        Subject: CN=<database_name>, O=<cluster_id>, OU=database
        Subject Public Key Info:
            Public Key Algorithm: rsaEncryption (4096 bit)
        X509v3 Extensions:
            X509v3 Basic Constraints: critical
                CA:FALSE
            X509v3 Key Usage: critical
                Digital Signature, Key Encipherment
            X509v3 Extended Key Usage:
                TLS Web Server Authentication
                TLS Web Client Authentication
            X509v3 Subject Alternative Name:
                DNS:<database_name>.cluster.<cluster_id>
                URI:scratchbird://cluster/<cluster_id>/db/<database_name>
            ScratchBird Cluster Info (OID 1.3.6.1.4.1.XXXXX.1):
                cluster_id: <uuid>
                database_id: <uuid>
                node_id: <uuid>
                federation_enabled: true
                max_txn_isolation: serializable
```

### 6.3 Inter-Database Connection Flow

When Database A connects to Database B for a federated query:

```
Database A                                Database B
    │                                         │
    │──────────── TCP Connect ───────────────►│
    │                                         │
    │◄─────────── TLS ServerHello ────────────│
    │  {server_cert: db_b.crt}               │
    │                                         │
    │──────────── TLS ClientHello ───────────►│
    │  {client_cert: db_a.crt}               │
    │                                         │
    │  [Both verify certs chain to same       │
    │   Cluster CA with matching cluster_id]  │
    │                                         │
    │◄─────────── TLS Finished ───────────────│
    │                                         │
    │──────────── CLUSTER_AUTH ──────────────►│
    │  {source_db: db_a,                      │
    │   target_db: db_b,                      │
    │   query_context: {...},                 │
    │   signature: <signed_by_db_a.key>}      │
    │                                         │
    │◄─────────── CLUSTER_AUTH_OK ────────────│
    │  {session_key: <ephemeral>,             │
    │   permissions: [...]}                   │
    │                                         │
    │──────────── FEDERATED_QUERY ───────────►│
    │  {encrypted with session_key}           │
```

### 6.3.1 CLUSTER_AUTH Message

```c
struct ClusterAuthMessage {
    MessageHeader header;  // msg_type = 0x1D

    uint8_t  source_db_id[16];
    uint8_t  target_db_id[16];

    uint32_t context_length;
    uint8_t  context[];        // UTF-8 JSON (query_context, roles, etc.)

    uint32_t signature_length;
    uint8_t  signature[];      // Signature over context using source_db key
};
```

### 6.3.2 CLUSTER_AUTH_OK Message

```c
struct ClusterAuthOkMessage {
    MessageHeader header;  // msg_type = 0x5E

    uint8_t  session_key_id[16];
    uint32_t permissions_length;
    uint8_t  permissions[];   // UTF-8 JSON list of permissions
};
```

### 6.4 Session Key Derivation

Session keys provide forward secrecy for inter-database communication:

```c
// HKDF key derivation (RFC 5869)
struct SessionKeyDerivation {
    uint8_t shared_secret[32];  // From TLS key exchange
    uint8_t source_db_id[16];   // UUID of initiating database
    uint8_t target_db_id[16];   // UUID of target database
    uint64_t timestamp;         // Unix timestamp (seconds)
    uint32_t sequence;          // Rotation counter
};

// Derive keys
hkdf_extract(salt="ScratchBird-Cluster-v1",
             ikm=tls_master_secret,
             prk=&prk);

hkdf_expand(prk,
            info="client-to-server" || source_db_id || target_db_id || timestamp,
            okm=client_to_server_key,
            length=32);

hkdf_expand(prk,
            info="server-to-client" || source_db_id || target_db_id || timestamp,
            okm=server_to_client_key,
            length=32);
```

### 6.5 Cluster Management Commands

```sql
-- Initialize cluster (creates CA)
CREATE CLUSTER my_cluster;

-- Add database to cluster
ALTER DATABASE mydb JOIN CLUSTER my_cluster;

-- Generate database certificate
ALTER DATABASE mydb GENERATE CERTIFICATE
    VALID FOR INTERVAL '1 year';

-- Rotate certificates
ALTER DATABASE mydb ROTATE CERTIFICATE;

-- View cluster membership
SHOW CLUSTER MEMBERS;

-- Verify cluster trust
SELECT verify_cluster_trust('db_a', 'db_b');

-- Remove from cluster
ALTER DATABASE mydb LEAVE CLUSTER;
```

---

## 7. Query Protocol

### 7.1 Simple Query

For single-statement queries:

```c
struct QueryMessage {
    MessageHeader header;  // msg_type = 0x03

    uint32_t query_flags;
    uint32_t max_rows;     // 0 = unlimited
    uint32_t timeout_ms;   // 0 = default

    // SQL query text (UTF-8, null-terminated)
    char query[];
};

#define QUERY_FLAG_DESCRIBE_ONLY    0x01  // Don't execute, just describe
#define QUERY_FLAG_NO_PORTAL        0x02  // Don't create portal
#define QUERY_FLAG_BINARY_RESULT    0x04  // Return binary format
#define QUERY_FLAG_INCLUDE_PLAN     0x08  // Include query plan
#define QUERY_FLAG_RETURN_SBLR      0x10  // Return compiled SBLR
#define QUERY_FLAG_NO_CACHE         0x20  // Don't use query cache
```

### 7.1.1 CANCEL Message

```c
struct CancelMessage {
    MessageHeader header;  // msg_type = 0x0B

    uint32_t cancel_type;   // 0=current, 1=by_sequence
    uint32_t target_sequence; // Only if cancel_type==1
};
```

### 7.2 Simple Query Response

```
Server                                    Client
   │                                         │
   │──────────── ROW_DESCRIPTION ───────────►│
   │  {num_columns, column_info[]}           │
   │                                         │
   │──────────── DATA_ROW ──────────────────►│ (repeated)
   │  {column_data[]}                        │
   │                                         │
   │──────────── COMMAND_COMPLETE ──────────►│
   │  {command_tag, rows_affected}           │
   │                                         │
   │──────────── READY ─────────────────────►│
   │  {txn_status}                           │
```

### 7.3 ROW_DESCRIPTION Message

```c
struct RowDescription {
    MessageHeader header;  // msg_type = 0x44

    uint16_t num_columns;
    uint16_t reserved;

    // Column descriptors
    struct ColumnDescriptor {
        uint32_t name_length;
        char     name[];           // Column name (UTF-8)
        uint32_t table_oid;        // Source table OID (0 if expression)
        uint16_t column_index;     // Column index in table (0 if expression)
        uint32_t type_oid;         // ScratchBird type OID
        int16_t  type_size;        // -1 for variable length
        int32_t  type_modifier;    // Type-specific modifier
        uint8_t  format;           // 0=text, 1=binary
        uint8_t  nullable;         // 1=nullable, 0=not null
        uint16_t reserved;
    } columns[];
};
```

### 7.4 DATA_ROW Message

```c
struct DataRow {
    MessageHeader header;  // msg_type = 0x45

    uint16_t num_columns;
    uint16_t null_bitmap_bytes;  // Ceiling(num_columns / 8)

    // Null bitmap (1 bit per column, 1=null)
    uint8_t null_bitmap[];

    // Column values (only for non-null columns)
    // Each value prefixed with 4-byte length
    struct ColumnValue {
        int32_t length;  // -1 for NULL, -2 for streamed, else byte count
        uint8_t data[];  // Column data
    } values[];
};
```

If `length == -2`, the value is streamed out-of-band:

```c
struct StreamedColumnValue {
    int32_t length;        // -2
    uint64_t stream_id;    // Stream identifier
    uint64_t stream_length; // Total byte length (0 if unknown)
};
```

The client must read the referenced value from the subsequent
`STREAM_READY` / `STREAM_DATA` / `STREAM_END` frames for the same
`stream_id`. Streamed values are binary payloads and must not be
re-encoded.

### 7.5 COPY Protocol Messages

```c
struct CopyInResponse {
    MessageHeader header;  // msg_type = 0x51

    uint8_t  format;       // 0=text, 1=binary
    uint8_t  reserved;
    uint16_t num_columns;
    uint16_t column_formats[];  // num_columns entries (0=text,1=binary)
};

struct CopyOutResponse {
    MessageHeader header;  // msg_type = 0x52

    uint8_t  format;
    uint8_t  reserved;
    uint16_t num_columns;
    uint16_t column_formats[];
};

struct CopyBothResponse {
    MessageHeader header;  // msg_type = 0x53

    uint8_t  format;
    uint8_t  reserved;
    uint16_t num_columns;
    uint16_t column_formats[];
};

struct CopyData {
    MessageHeader header;  // msg_type = 0x0D
    uint8_t data[];        // Raw COPY stream (length = header.length)
};

struct CopyDone {
    MessageHeader header;  // msg_type = 0x0E
    // No payload
};

struct CopyFail {
    MessageHeader header;  // msg_type = 0x0F

    uint32_t error_length;
    char     error[];
};
```

Notes:
- For `format = BINARY`, the COPY data stream is a sequence of DATA_ROW payloads
  (row framing without the DATA_ROW header).

---

## 8. Result Protocol

### 8.1 COMMAND_COMPLETE Message

```c
struct CommandComplete {
    MessageHeader header;  // msg_type = 0x46

    uint8_t  command_type;  // See below
    uint8_t  reserved[3];
    uint64_t rows_affected;
    uint64_t last_insert_id;  // For INSERT with identity column

    // Command tag (e.g., "SELECT 100", "INSERT 0 1")
    char tag[];
};

// Command types
#define CMD_SELECT       1
#define CMD_INSERT       2
#define CMD_UPDATE       3
#define CMD_DELETE       4
#define CMD_CREATE       5
#define CMD_DROP         6
#define CMD_ALTER        7
#define CMD_GRANT        8
#define CMD_REVOKE       9
#define CMD_BEGIN        10
#define CMD_COMMIT       11
#define CMD_ROLLBACK     12
#define CMD_COPY         13
#define CMD_SET          14
#define CMD_SHOW         15
#define CMD_EXPLAIN      16
#define CMD_VACUUM       17  // SWEEP/VACUUM (native sweep/GC + PostgreSQL alias)
#define CMD_TRUNCATE     18
#define CMD_MERGE        19
```

### 8.2 READY Message

```c
struct ReadyMessage {
    MessageHeader header;  // msg_type = 0x43

    uint8_t  status;       // 0=idle,1=active,2=error
    uint8_t  reserved[3];
    uint64_t txn_id;
    uint64_t visibility_epoch;
};
```

### 8.3 PARAMETER_STATUS Message

```c
struct ParameterStatus {
    MessageHeader header;  // msg_type = 0x4F

    uint32_t name_length;
    char     name[];
    uint32_t value_length;
    char     value[];
};
```

### 8.4 Header-Only Result Messages

These messages carry no payload (`length = 0`):

- `EMPTY_QUERY` (0x47)
- `NO_DATA` (0x4E)
- `PORTAL_SUSPENDED` (0x4D)
- `PARSE_COMPLETE` (0x4A)
- `BIND_COMPLETE` (0x4B)
- `CLOSE_COMPLETE` (0x4C)

### 8.5 QUERY_PLAN Message (Optional)

When `QUERY_FLAG_INCLUDE_PLAN` is set:

```c
struct QueryPlan {
    MessageHeader header;  // msg_type = 0x58

    uint32_t plan_format;  // 0=text, 1=json, 2=xml, 3=binary
    uint32_t plan_length;

    // Execution statistics
    uint64_t planning_time_us;
    uint64_t estimated_rows;
    uint64_t estimated_cost;

    // Plan data
    uint8_t plan_data[];
};
```

### 8.6 SBLR_COMPILED Message (Optional)

When `QUERY_FLAG_RETURN_SBLR` is set:

```c
struct SblrCompiled {
    MessageHeader header;  // msg_type = 0x57

    uint64_t sblr_hash;      // Hash for caching/matching
    uint32_t sblr_version;   // SBLR bytecode version (current: 2)
    uint32_t sblr_length;

    // Compiled SBLR bytecode
    uint8_t sblr_bytecode[];
};
```

**Version requirement**: Clients must send `sblr_version = 2`. Earlier versions are rejected.

### 8.7 FUNCTION_RESULT Message

```c
struct FunctionResult {
    MessageHeader header;  // msg_type = 0x55

    uint32_t type_oid;
    uint8_t  format;        // 0=text, 1=binary
    uint8_t  reserved[3];
    int32_t  length;        // -1 for NULL
    uint8_t  data[];        // Result value
};
```

---

## 9. Prepared Statements

### 9.1 Extended Query Protocol

For parameterized queries with type binding:

```
Client                                    Server
   │                                         │
   │──────────── PARSE ─────────────────────►│
   │  {stmt_name, query, param_types[]}      │
   │                                         │
   │◄─────────── PARSE_COMPLETE ─────────────│
   │                                         │
   │──────────── DESCRIBE ──────────────────►│
   │  {type='S', name=stmt_name}             │
   │                                         │
   │◄─────────── PARAMETER_DESCRIPTION ──────│
   │  {param_types[]}                        │
   │                                         │
   │◄─────────── ROW_DESCRIPTION ────────────│
   │  {column_info[]}                        │
   │                                         │
   │──────────── BIND ──────────────────────►│
   │  {portal_name, stmt_name, params[]}     │
   │                                         │
   │◄─────────── BIND_COMPLETE ──────────────│
   │                                         │
   │──────────── EXECUTE ───────────────────►│
   │  {portal_name, max_rows}                │
   │                                         │
   │◄─────────── DATA_ROW ───────────────────│ (repeated)
   │                                         │
   │◄─────────── COMMAND_COMPLETE ───────────│
   │                                         │
   │──────────── SYNC ──────────────────────►│
   │                                         │
   │◄─────────── READY ──────────────────────│
```

### 9.2 PARSE Message

```c
struct ParseMessage {
    MessageHeader header;  // msg_type = 0x04

    uint32_t name_length;
    char     statement_name[];  // Empty for unnamed

    uint32_t query_length;
    char     query[];           // SQL with $1, $2, ... placeholders

    uint16_t num_param_types;
    uint16_t reserved;

    // Parameter type OIDs (0 = infer from context)
    uint32_t param_types[];
};
```

### 9.3 BIND Message

```c
struct BindMessage {
    MessageHeader header;  // msg_type = 0x05

    uint32_t portal_name_length;
    char     portal_name[];     // Empty for unnamed

    uint32_t statement_name_length;
    char     statement_name[];  // Reference to parsed statement

    uint16_t num_param_formats;
    uint16_t param_formats[];   // 0=text, 1=binary per param

    uint16_t num_param_values;
    uint16_t reserved;

    // Parameter values
    struct ParamValue {
        int32_t length;   // -1 for NULL
        uint8_t data[];   // Parameter data
    } params[];

    uint16_t num_result_formats;
    uint16_t result_formats[];  // 0=text, 1=binary per column
};
```

### 9.4 DESCRIBE Message

```c
struct DescribeMessage {
    MessageHeader header;  // msg_type = 0x06

    uint8_t  describe_type;    // 'S' = statement, 'P' = portal
    uint8_t  reserved[3];

    uint32_t name_length;
    char     name[];
};
```

### 9.5 EXECUTE Message

```c
struct ExecuteMessage {
    MessageHeader header;  // msg_type = 0x07

    uint32_t portal_name_length;
    char     portal_name[];  // Empty for unnamed

    uint32_t max_rows;        // 0 = unlimited
    uint8_t  fetch_backward; // 0 = forward, 1 = backward
};
```

### 9.6 CLOSE Message

```c
struct CloseMessage {
    MessageHeader header;  // msg_type = 0x08

    uint8_t  close_type;      // 'S' = statement, 'P' = portal
    uint8_t  reserved[3];

    uint32_t name_length;
    char     name[];
};
```

### 9.7 SYNC / FLUSH Messages

```c
struct SyncMessage {
    MessageHeader header;  // msg_type = 0x09
    // No payload
};

struct FlushMessage {
    MessageHeader header;  // msg_type = 0x0A
    // No payload
};
```

### 9.8 PARAMETER_DESCRIPTION Message

```c
struct ParameterDescription {
    MessageHeader header;  // msg_type = 0x50

    uint16_t num_params;
    uint16_t reserved;
    uint32_t param_type_oids[];  // num_params entries
};
```

### 9.9 SBLR_EXECUTE Message

Execute pre-compiled SBLR bytecode (skip parsing):

```c
struct SblrExecuteMessage {
    MessageHeader header;  // msg_type = 0x10

    uint64_t sblr_hash;      // Hash from previous SBLR_COMPILED
    uint32_t sblr_length;    // 0 if using cached bytecode

    uint16_t num_param_values;
    uint16_t reserved;

    // If sblr_length > 0, bytecode follows
    uint8_t sblr_bytecode[];  // Optional

    // Parameter values
    struct ParamValue params[];
};
```

---

## 10. Federation Protocol

### 10.1 Overview

Federation enables queries that span multiple databases in the cluster:

```sql
-- Query spanning three databases
SELECT
    a.name,
    b.balance,
    c.last_login
FROM users@db_a a
JOIN accounts@db_b b ON a.id = b.user_id
JOIN audit@db_c c ON a.id = c.user_id
WHERE a.status = 'active';
```

### 10.2 FEDERATED_QUERY Message

```c
struct FederatedQueryMessage {
    MessageHeader header;  // msg_type = 0x13

    uint32_t query_id;       // Unique query identifier
    uint8_t  coordinator;    // 1 if this node is coordinator
    uint8_t  reserved[3];

    uint16_t num_databases;
    uint16_t reserved2;

    // Target databases for this query fragment
    struct DatabaseTarget {
        uint8_t  database_id[16];  // UUID
        uint32_t name_length;
        char     name[];
    } databases[];

    // Query fragment to execute
    uint32_t query_length;
    char     query[];

    // Execution context
    uint64_t txn_id;           // Distributed transaction ID
    uint64_t visibility_epoch; // MGA visibility epoch
    uint32_t isolation_level;
    uint32_t timeout_ms;
};
```

### 10.3 Federation Execution Model

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    Federated Query Execution                             │
└─────────────────────────────────────────────────────────────────────────┘

Client sends query to Database A (coordinator):
  SELECT u.*, o.total FROM users@db_a u JOIN orders@db_b o ON u.id = o.user_id

Database A (Coordinator):
1. Parse query, identify remote tables
2. Create distributed execution plan
3. Initiate cluster connections to db_b
4. Send query fragments to participants

    ┌────────────────────────────────────────────────────────────────┐
    │ Database A (Coordinator)                                        │
    │                                                                 │
    │  Query Plan:                                                    │
    │  ┌─────────────────────────────────────────────────────────┐  │
    │  │ HashJoin                                                  │  │
    │  │   ├── LocalScan(users)                                   │  │
    │  │   └── RemoteScan(orders@db_b)                           │  │
    │  └─────────────────────────────────────────────────────────┘  │
    │                                                                 │
    │  1. Scan local users table                                     │
    │  2. Send user_ids to db_b                                      │
    │  3. Receive matching orders                                    │
    │  4. Perform hash join locally                                  │
    │  5. Return results to client                                   │
    └──────────────────────────┬─────────────────────────────────────┘
                               │
                               │ FEDERATED_QUERY
                               │ SELECT * FROM orders WHERE user_id IN (...)
                               ▼
    ┌────────────────────────────────────────────────────────────────┐
    │ Database B (Participant)                                        │
    │                                                                 │
    │  1. Verify cluster trust (certificate check)                   │
    │  2. Validate permissions                                       │
    │  3. Execute query fragment                                     │
    │  4. Stream results back to coordinator                         │
    └────────────────────────────────────────────────────────────────┘
```

### 10.4 FEDERATED_RESULT Message

```c
struct FederatedResultMessage {
    MessageHeader header;  // msg_type = 0x5F

    uint32_t query_id;       // Matches request
    uint8_t  source_db[16];  // UUID of responding database
    uint8_t  status;         // 0=success, 1=error, 2=partial
    uint8_t  reserved[3];

    uint64_t rows_returned;
    uint64_t execution_time_us;

    // Result data (same format as DATA_ROW messages)
    // Or error message if status != 0
};
```

### 10.5 Cross-Database Transaction Protocol

For distributed transactions:

```sql
-- Explicit distributed transaction
BEGIN DISTRIBUTED;
UPDATE accounts@db_a SET balance = balance - 100 WHERE id = 1;
UPDATE accounts@db_b SET balance = balance + 100 WHERE id = 2;
COMMIT;
```

Two-phase commit flow:

```
Coordinator                   Participant A                 Participant B
    │                              │                              │
    │── BEGIN DISTRIBUTED ────────►│                              │
    │                              │                              │
    │── BEGIN DISTRIBUTED ─────────────────────────────────────►│
    │                              │                              │
    │── UPDATE @A ────────────────►│                              │
    │◄─ OK ────────────────────────│                              │
    │                              │                              │
    │── UPDATE @B ─────────────────────────────────────────────►│
    │◄─ OK ─────────────────────────────────────────────────────│
    │                              │                              │
    │── PREPARE ──────────────────►│                              │
    │◄─ PREPARED ──────────────────│                              │
    │                              │                              │
    │── PREPARE ───────────────────────────────────────────────►│
    │◄─ PREPARED ───────────────────────────────────────────────│
    │                              │                              │
    │── COMMIT ───────────────────►│                              │
    │◄─ COMMITTED ─────────────────│                              │
    │                              │                              │
    │── COMMIT ────────────────────────────────────────────────►│
    │◄─ COMMITTED ──────────────────────────────────────────────│
    │                              │                              │
    │  [Optional write-after log (post-gold)]                      │
```

### 10.6 Attachment Protocol (Native ScratchBird Only)

ScratchBird supports multiple attachments per TCP connection. An attachment owns its own transaction context and security/session state.

**Routing rules:**
- `attachment_id` and `txn_id` are carried in the message header.
- If either is missing/zero when required, the server returns an error and does not execute the request.
- The server returns the initial `attachment_id` and `txn_id` after authentication; clients must cache and echo them on every request.

**ATTACH_CREATE (0x1E):**
```c
struct AttachCreateMessage {
    MessageHeader header;  // msg_type = 0x1E
    uint32_t emulation_mode_len;
    char     emulation_mode[];  // "scratchbird"|"mysql"|"postgresql"|"firebird"
    uint32_t db_name_len;
    char     db_name[];
};
```
**Request rules:** client supplies a non-zero `attachment_id` and `txn_id` in the header (the caller's attachment/transaction).
**Response:** `READY` with `PARAMETER_STATUS` fields `attachment_id` and `current_txn_id`.

**ATTACH_DETACH (0x1F):**
```c
struct AttachDetachMessage {
    MessageHeader header;  // msg_type = 0x1F
    // attachment_id is in the message header
};
```
Server rolls back active transactions for that attachment and removes it.

**ATTACH_LIST (0x20):**
```c
struct AttachListMessage {
    MessageHeader header;  // msg_type = 0x20
};
```
**Response:** standard result set (`ROW_DESCRIPTION` + `DATA_ROW` + `COMMAND_COMPLETE`) with attachment_id and summary fields.

---

## 11. Transaction Protocol

### 11.1 Transaction Messages

```c
struct TxnBeginMessage {
    MessageHeader header;  // msg_type = 0x15

    uint16_t flags;            // see TXN_FLAG_* below
    uint8_t  conflict_action;  // 0=DEFAULT,1=COMMIT,2=ROLLBACK,3=ERROR,4=KEEP
    uint8_t  autocommit_mode;  // 0=UNCHANGED,1=ON,2=OFF
    uint8_t  isolation_level;  // See below (only if flags HAS_ISOLATION)
    uint8_t  access_mode;      // 0=read-write, 1=read-only (only if flags HAS_ACCESS)
    uint8_t  deferrable;       // 0/1 (only if flags HAS_DEFERRABLE)
    uint8_t  wait_mode;        // 0=NO WAIT, 1=WAIT (only if flags HAS_WAIT)
    uint32_t timeout_ms;       // 0=default (only if flags HAS_TIMEOUT)
};

#define ISOLATION_READ_UNCOMMITTED  0  // Not supported (aliased to READ_COMMITTED)
#define ISOLATION_READ_COMMITTED    1  // Default
#define ISOLATION_REPEATABLE_READ   2  // Snapshot isolation
#define ISOLATION_SERIALIZABLE      3  // Full serializability (MGA)

#define TXN_FLAG_HAS_ISOLATION   0x0001
#define TXN_FLAG_HAS_ACCESS      0x0002
#define TXN_FLAG_HAS_DEFERRABLE  0x0004
#define TXN_FLAG_HAS_WAIT        0x0008
#define TXN_FLAG_HAS_TIMEOUT     0x0010
#define TXN_FLAG_HAS_AUTOCOMMIT  0x0020

struct TxnCommitMessage {
    MessageHeader header;  // msg_type = 0x16
    uint8_t  flags;        // bit0=AND_CHAIN, bit1=AND_NO_CHAIN, bit2=RETAINING
    uint8_t  reserved[3];
};

struct TxnRollbackMessage {
    MessageHeader header;  // msg_type = 0x17
    uint8_t  flags;        // bit0=AND_CHAIN, bit1=AND_NO_CHAIN, bit2=RETAINING
    uint8_t  reserved[3];
};

struct TxnSavepointMessage {
    MessageHeader header;  // msg_type = 0x18
    uint32_t name_length;
    char     name[];
};

struct TxnRollbackToMessage {
    MessageHeader header;  // msg_type = 0x1A
    uint32_t name_length;
    char     name[];
};
```

### 11.2 TXN_STATUS Message

```c
struct TxnStatusMessage {
    MessageHeader header;  // msg_type = 0x5C

    uint8_t  status;           // 'T'=in_txn, 'E'=error (always in txn)
    uint8_t  isolation_level;
    uint8_t  access_mode;
    uint8_t  reserved;

    uint64_t txn_id;           // Transaction ID
    uint64_t visibility_epoch; // MGA visibility epoch
    uint64_t start_timestamp;  // Transaction start time

    uint32_t num_savepoints;
    // Savepoint names follow
};
```

### 11.3 Session Options

```c
struct SetOptionMessage {
    MessageHeader header;  // msg_type = 0x1C

    uint32_t name_length;
    char     name[];
    uint32_t value_length;
    char     value[];
};
```

---

## 12. Streaming Protocol

### 12.1 Overview

For large result sets, streaming provides backpressure control:

```c
struct StreamControlMessage {
    MessageHeader header;  // msg_type = 0x14

    uint8_t  control_type;   // See below
    uint8_t  reserved[3];
    uint32_t window_size;    // Rows to send before waiting
    uint32_t timeout_ms;     // Pause timeout
};

#define STREAM_START    0    // Start streaming
#define STREAM_PAUSE    1    // Pause streaming
#define STREAM_RESUME   2    // Resume streaming
#define STREAM_CANCEL   3    // Cancel stream
#define STREAM_ACK      4    // Acknowledge rows received
```

### 12.2 Streaming Flow

```
Client                                    Server
   │                                         │
   │──────────── QUERY ─────────────────────►│
   │  {flags: STREAMING, window: 1000}       │
   │                                         │
   │◄─────────── STREAM_READY ───────────────│
   │  {total_rows: 1000000}                  │
   │                                         │
   │◄─────────── STREAM_DATA ────────────────│ (rows 0-999)
   │                                         │
   │──────────── STREAM_CONTROL ────────────►│
   │  {type: ACK, window: 1000}              │
   │                                         │
   │◄─────────── STREAM_DATA ────────────────│ (rows 1000-1999)
   │                                         │
   │──────────── STREAM_CONTROL ────────────►│
   │  {type: PAUSE}                          │
   │                                         │
   │  [Client processes buffered data]       │
   │                                         │
   │──────────── STREAM_CONTROL ────────────►│
   │  {type: RESUME, window: 5000}           │
   │                                         │
   │◄─────────── STREAM_DATA ────────────────│ (rows 2000-6999)
   │                                         │
   │  ...                                    │
   │                                         │
   │◄─────────── STREAM_END ─────────────────│
   │  {total_rows: 1000000}                  │
```

### 12.3 STREAM_READY / STREAM_DATA / STREAM_END Messages

```c
struct StreamReady {
    MessageHeader header;  // msg_type = 0x59

    uint64_t stream_id;
    uint64_t total_rows;     // 0 if unknown
    uint64_t estimated_bytes;
};

struct StreamData {
    MessageHeader header;  // msg_type = 0x5A

    uint64_t stream_id;
    uint32_t chunk_rows;
    uint32_t chunk_bytes;
    uint8_t  data[];        // Packed DATA_ROW frames, binary COPY rows, or LOB bytes
};

struct StreamEnd {
    MessageHeader header;  // msg_type = 0x5B

    uint64_t stream_id;
    uint64_t total_rows;
    uint64_t total_bytes;
};
```

### 12.4 QUERY_PROGRESS Message

```c
struct QueryProgress {
    MessageHeader header;  // msg_type = 0x5C

    uint64_t rows_processed;
    uint64_t bytes_processed;
};
```

Server emits QUERY_PROGRESS only when the client advertises
`CONNECT_FLAG_PROGRESS` during CONNECT_REQUEST.

---

## 13. Notification Protocol

### 13.1 SUBSCRIBE Message

```c
struct SubscribeMessage {
    MessageHeader header;  // msg_type = 0x13

    uint8_t  subscribe_type;  // See below
    uint8_t  reserved[3];

    uint32_t channel_length;
    char     channel[];       // Channel name pattern (supports wildcards)

    uint32_t filter_length;
    char     filter[];        // Optional filter expression
};

#define SUB_TYPE_CHANNEL    0   // LISTEN/NOTIFY channel
#define SUB_TYPE_TABLE      1   // Table change notifications
#define SUB_TYPE_QUERY      2   // Query result change (materialized view refresh)
#define SUB_TYPE_EVENT      3   // Firebird-style POST_EVENT notifications
```

### 13.1.1 UNSUBSCRIBE Message

```c
struct UnsubscribeMessage {
    MessageHeader header;  // msg_type = 0x14

    uint32_t channel_length;
    char     channel[];
};
```

Note: The current native implementation uses msg_type 0x13/0x14 for
SUBSCRIBE/UNSUBSCRIBE (see `include/scratchbird/protocol/wire_protocol.h`).

### 13.2 NOTIFICATION Message

```c
struct NotificationMessage {
    MessageHeader header;  // msg_type = 0x54

    uint32_t process_id;      // Backend PID that sent notification

    uint32_t channel_length;
    char     channel[];

    uint32_t payload_length;
    uint8_t  payload[];       // Notification payload (up to 8000 bytes)

    // For table change notifications:
    uint8_t  change_type;     // 'I'=insert, 'U'=update, 'D'=delete
    uint64_t row_id;          // Affected row (if applicable)
};
```

**Payload layout by subscription type:**

- **SUB_TYPE_CHANNEL**: payload is an opaque byte array (client-defined).
- **SUB_TYPE_TABLE**: payload is optional; change_type/row_id fields are present.
- **SUB_TYPE_QUERY**: payload is optional; content is client-defined.
- **SUB_TYPE_EVENT**: payload is structured as:

```c
struct EventNotificationPayload {
    uint8_t  event_uuid[16];   // UUIDv7 for this event instance
    uint8_t  delivery_mode;    // 0=ON_COMMIT, 1=IMMEDIATE
    uint8_t  reserved[3];
    uint32_t message_length;   // 0 if no message
    uint8_t  message[];        // Optional message (native clients only)
};
```

For SUB_TYPE_EVENT, `channel` is the event name (Firebird limit: 127 bytes),
and `payload_length` includes the entire EventNotificationPayload.
`message_length` is limited to 1024 bytes (see DDL_EVENTS).

### 13.3 Keepalive Messages

```c
struct PingMessage {
    MessageHeader header;  // msg_type = 0x1B
    uint64_t client_time_us;
};

struct PongMessage {
    MessageHeader header;  // msg_type = 0x5D
    uint64_t server_time_us;
};

struct HeartbeatMessage {
    MessageHeader header;  // msg_type = 0x80
    uint64_t monotonic_us;
};
```

### 13.4 EXTENSION Message

```c
struct ExtensionMessage {
    MessageHeader header;  // msg_type = 0x81

    uint32_t extension_id;
    uint16_t extension_version;
    uint16_t reserved;
    uint8_t  data[];        // Extension-specific payload
};
```

---

## 14. Error Handling

### 14.1 ERROR Message

```c
struct ErrorMessage {
    MessageHeader header;  // msg_type = 0x48

    // Error fields (key-value pairs)
    // Format: field_type (1 byte) + value (null-terminated string)
    // Terminated by field_type = 0

    // Field types:
    // 'S' = Severity (ERROR, FATAL, PANIC, WARNING, NOTICE, DEBUG, INFO, LOG)
    // 'V' = Severity (non-localized)
    // 'C' = SQLSTATE code (5 chars)
    // 'M' = Message (primary error message)
    // 'D' = Detail (optional secondary message)
    // 'H' = Hint (optional suggestion)
    // 'P' = Position (character position in query)
    // 'p' = Internal position
    // 'q' = Internal query
    // 'W' = Where (call stack/context)
    // 's' = Schema name
    // 't' = Table name
    // 'c' = Column name
    // 'd' = Data type name
    // 'n' = Constraint name
    // 'F' = File (source file)
    // 'L' = Line (source line)
    // 'R' = Routine (source function)

    char fields[];
};
```

### 14.1.1 NOTICE Message

```c
struct NoticeMessage {
    MessageHeader header;  // msg_type = 0x49

    uint8_t  severity;     // 0=INFO,1=NOTICE,2=WARNING
    uint8_t  reserved[3];
    uint32_t message_length;
    char     message[];    // UTF-8 text
};
```

### 14.2 SQLSTATE Codes

ScratchBird uses standard SQLSTATE codes plus ScratchBird-specific extensions:

| Class | Description | Example |
|-------|-------------|---------|
| 00 | Successful completion | 00000 |
| 01 | Warning | 01000 |
| 02 | No data | 02000 |
| 03 | SQL statement not yet complete | 03000 |
| 08 | Connection exception | 08000 |
| 09 | Triggered action exception | 09000 |
| 0A | Feature not supported | 0A000 |
| 0B | Invalid transaction initiation | 0B000 |
| 0F | Locator exception | 0F000 |
| 0L | Invalid grantor | 0L000 |
| 0P | Invalid role specification | 0P000 |
| 0Z | Diagnostics exception | 0Z000 |
| 20 | Case not found | 20000 |
| 21 | Cardinality violation | 21000 |
| 22 | Data exception | 22000 |
| 23 | Integrity constraint violation | 23000 |
| 24 | Invalid cursor state | 24000 |
| 25 | Invalid transaction state | 25000 |
| 26 | Invalid SQL statement name | 26000 |
| 27 | Triggered data change violation | 27000 |
| 28 | Invalid authorization specification | 28000 |
| 2B | Dependent privilege descriptors | 2B000 |
| 2D | Invalid transaction termination | 2D000 |
| 2F | SQL routine exception | 2F000 |
| 34 | Invalid cursor name | 34000 |
| 38 | External routine exception | 38000 |
| 39 | External routine invocation | 39000 |
| 3B | Savepoint exception | 3B000 |
| 3D | Invalid catalog name | 3D000 |
| 3F | Invalid schema name | 3F000 |
| 40 | Transaction rollback | 40000 |
| 42 | Syntax error or access rule violation | 42000 |
| 44 | WITH CHECK OPTION violation | 44000 |
| 53 | Insufficient resources | 53000 |
| 54 | Program limit exceeded | 54000 |
| 55 | Object not in prerequisite state | 55000 |
| 57 | Operator intervention | 57000 |
| 58 | System error | 58000 |
| 72 | Snapshot failure | 72000 |
| F0 | Configuration file error | F0000 |
| HV | FDW error | HV000 |
| P0 | PL/SQL error | P0000 |
| XX | Internal error | XX000 |
| **SB** | **ScratchBird-specific** | **SB000** |

ScratchBird-specific SQLSTATE codes (class SB):

| Code | Description |
|------|-------------|
| SB000 | General ScratchBird error |
| SB001 | MGA visibility conflict |
| SB002 | Record version conflict |
| SB003 | Garbage collection in progress |
| SB004 | Index rebuild required |
| SB005 | SBLR bytecode version mismatch |
| SB006 | Cluster trust failure |
| SB007 | Federation query failed |
| SB008 | Remote database unavailable |
| SB009 | Distributed transaction timeout |
| SB00A | Certificate validation failed |
| SB00B | Session key derivation failed |

---

## 15. Type Serialization

### 15.1 Binary Format

All 86 ScratchBird types have defined binary representations:

```c
// Type format codes
#define FORMAT_TEXT     0
#define FORMAT_BINARY   1

// Binary representations (little-endian unless noted)

// Integers
int16_t  SMALLINT;     // 2 bytes
int32_t  INTEGER;      // 4 bytes
int64_t  BIGINT;       // 8 bytes
__int128 INT128;       // 16 bytes

// Floating point (IEEE 754)
float    REAL;         // 4 bytes
double   DOUBLE;       // 8 bytes

// Fixed-point
struct DECIMAL {
    int16_t precision;
    int16_t scale;
    uint8_t sign;      // 0=positive, 1=negative
    uint8_t reserved[3];
    uint8_t digits[];  // BCD or coefficient bytes
};

// Boolean
uint8_t BOOLEAN;       // 0=false, 1=true

// Strings (UTF-8)
struct VARCHAR {
    int32_t length;    // Byte length (-1 for NULL)
    char    data[];    // UTF-8 bytes
};

struct CHAR {
    int32_t length;
    char    data[];    // Space-padded
};

struct TEXT {
    int32_t length;
    char    data[];
};

// Binary data
struct BYTEA {
    int32_t length;
    uint8_t data[];
};

// Date/Time (all times in microseconds since 2000-01-01 00:00:00 UTC)
int32_t  DATE;         // Days since 2000-01-01
int64_t  TIME;         // Microseconds since midnight
int64_t  TIMESTAMP;    // Microseconds since 2000-01-01 00:00:00
int64_t  TIMESTAMPTZ;  // Same, but interpreted as UTC

struct INTERVAL {
    int64_t microseconds;
    int32_t days;
    int32_t months;
};

// UUID
uint8_t UUID[16];      // RFC 4122 format

// JSON/JSONB
struct JSONB {
    int32_t length;
    uint8_t data[];    // ScratchBird JSONB binary format
};

// Arrays
struct ARRAY {
    int32_t ndim;        // Number of dimensions
    int32_t flags;       // Has nulls, etc.
    uint32_t elem_oid;   // Element type OID

    struct Dimension {
        int32_t length;
        int32_t lower_bound;
    } dims[];

    // Followed by element data
};

// Geometric types
struct POINT {
    double x;
    double y;
};

struct LINE {
    double A, B, C;    // Ax + By + C = 0
};

struct LSEG {
    POINT start;
    POINT end;
};

struct BOX {
    POINT high;
    POINT low;
};

struct PATH {
    uint8_t closed;    // 0=open, 1=closed
    uint8_t reserved[3];
    int32_t npoints;
    POINT points[];
};

struct POLYGON {
    int32_t npoints;
    POINT points[];
};

struct CIRCLE {
    POINT center;
    double radius;
};

// Network types
struct INET {
    uint8_t family;    // AF_INET=2, AF_INET6=10
    uint8_t bits;      // CIDR prefix length
    uint8_t is_cidr;   // 0=inet, 1=cidr
    uint8_t reserved;
    uint8_t addr[];    // 4 or 16 bytes
};

struct MACADDR {
    uint8_t addr[6];
};

struct MACADDR8 {
    uint8_t addr[8];
};

// Bit strings
struct BIT {
    int32_t nbits;
    uint8_t data[];    // Packed bits
};

// Range types
struct RANGE {
    uint8_t flags;     // Empty, inclusive bounds, etc.
    uint8_t reserved[3];
    // Lower bound (if not empty and not infinite)
    // Upper bound (if not empty and not infinite)
};

#define RANGE_EMPTY         0x01
#define RANGE_LB_INC        0x02  // Lower bound inclusive
#define RANGE_UB_INC        0x04  // Upper bound inclusive
#define RANGE_LB_INF        0x08  // Lower bound infinite
#define RANGE_UB_INF        0x10  // Upper bound infinite

// Full text search
struct TSVECTOR {
    int32_t length;
    // Lexemes with positions
};

struct TSQUERY {
    int32_t length;
    // Query tree
};
```

### 15.2 Type OIDs

Each type has a unique OID for identification:

```c
// Built-in type OIDs
#define OID_BOOL          16
#define OID_BYTEA         17
#define OID_CHAR          18
#define OID_NAME          19
#define OID_INT8          20
#define OID_INT2          21
#define OID_INT4          23
#define OID_TEXT          25
#define OID_OID           26
#define OID_TID           27
#define OID_XID           28
#define OID_CID           29
#define OID_JSON          114
#define OID_XML           142
#define OID_POINT         600
#define OID_LSEG          601
#define OID_PATH          602
#define OID_BOX           603
#define OID_POLYGON       604
#define OID_LINE          628
#define OID_FLOAT4        700
#define OID_FLOAT8        701
#define OID_CIRCLE        718
#define OID_MONEY         790
#define OID_MACADDR       829
#define OID_INET          869
#define OID_CIDR          650
#define OID_MACADDR8      774
#define OID_BPCHAR        1042
#define OID_VARCHAR       1043
#define OID_DATE          1082
#define OID_TIME          1083
#define OID_TIMESTAMP     1114
#define OID_TIMESTAMPTZ   1184
#define OID_INTERVAL      1186
#define OID_TIMETZ        1266
#define OID_BIT           1560
#define OID_VARBIT        1562
#define OID_NUMERIC       1700
#define OID_UUID          2950
#define OID_JSONB         3802
#define OID_INT4RANGE     3904
#define OID_NUMRANGE      3906
#define OID_TSRANGE       3908
#define OID_TSTZRANGE     3910
#define OID_DATERANGE     3912
#define OID_INT8RANGE     3926
#define OID_TSVECTOR      3614
#define OID_TSQUERY       3615

// ScratchBird-specific type OIDs (starting at 16384)
#define OID_SB_INT128     16384
#define OID_SB_DECIMAL128 16385
#define OID_SB_VECTOR     16386  // For embeddings
#define OID_SB_RECORD_ID  16387  // MGA record identifier
#define OID_SB_TXN_ID     16388  // Transaction ID
#define OID_SB_EPOCH      16389  // Visibility epoch
```

---

## 16. Compression

### 16.1 zstd Compression

When `FEATURE_COMPRESSION` is negotiated:

```c
// Compression header (prepended to compressed payload)
struct CompressionHeader {
    uint32_t uncompressed_size;
    uint8_t  compression_level;  // 1-22 for zstd
    uint8_t  reserved[3];
};

// Compression is applied to entire message payload
// MSG_FLAG_COMPRESSED indicates compression

// Compression level selection:
// - Level 1-3:  Fast compression, larger output
// - Level 4-9:  Balanced (default: 6)
// - Level 10-15: Better compression, slower
// - Level 16-22: Maximum compression (rarely used)
```

### 16.2 Compression Negotiation

```c
// In STARTUP message
uint64_t features |= FEATURE_COMPRESSION;

// Server responds with accepted features
// If FEATURE_COMPRESSION is set, both sides can compress
//
// Native SBWP implementation note:
// - CONNECT_REQUEST.client_flags includes CONNECT_FLAG_ZSTD_COMPRESSION (0x0001)
// - CONNECT_RESPONSE.server_flags echoes CONNECT_FLAG_ZSTD_COMPRESSION when accepted
// These flags map to FEATURE_COMPRESSION for negotiation.

// Per-message decision:
// - Small messages (<1KB): no compression
// - Result sets: compress if ratio > 1.5x
// - COPY data: always compress if enabled
```

---

## 17. Security Considerations

### 17.1 TLS Requirements

- TLS 1.3 required (no downgrade to 1.2 or earlier)
- Cipher suites: TLS_AES_256_GCM_SHA384, TLS_CHACHA20_POLY1305_SHA256
- Certificate verification required
- SNI (Server Name Indication) supported

### 17.2 Authentication Security

- No cleartext passwords (SCRAM-SHA-256 minimum)
- MFA support for high-security environments
- Certificate-based auth for cluster communication
- Session timeout and idle disconnect

### 17.3 Query Safety

- Parameterized queries prevent SQL injection
- Statement limits prevent resource exhaustion
- Query timeout enforcement
- Result size limits

### 17.4 Cluster Security

- Mutual TLS for all inter-database communication
- Cluster CA certificate pinning
- Session key rotation (hourly default)
- Federation permissions enforced

### 17.5 Audit Logging

All authentication and authorization events logged:

```c
struct AuditEntry {
    uint64_t timestamp;
    uint8_t  event_type;
    uint8_t  result;       // Success/failure
    uint16_t reserved;
    uint32_t user_id;
    uint32_t database_id;
    uint8_t  client_ip[16];
    uint32_t details_length;
    char     details[];
};
```

---

## 18. Performance Optimizations

### 18.1 Pipelining

Multiple queries can be sent without waiting for responses:

```
Client                                    Server
   │                                         │
   │──────────── QUERY 1 ───────────────────►│
   │──────────── QUERY 2 ───────────────────►│
   │──────────── QUERY 3 ───────────────────►│
   │──────────── SYNC ──────────────────────►│
   │                                         │
   │◄─────────── RESULT 1 ───────────────────│
   │◄─────────── RESULT 2 ───────────────────│
   │◄─────────── RESULT 3 ───────────────────│
   │◄─────────── READY ──────────────────────│
```

### 18.2 Statement Caching

Server maintains LRU cache of prepared statements:

```c
// Cache lookup
uint64_t stmt_hash = hash(query_text);
PreparedStatement* cached = stmt_cache.get(stmt_hash);
if (cached) {
    // Skip parse, use cached plan
}
```

### 18.3 Result Caching

For repeated identical queries:

```c
// Query result cache
struct CachedResult {
    uint64_t query_hash;
    uint64_t last_valid_txn;  // Invalidated when tables change
    RowDescription* desc;
    DataRow* rows[];
};
```

### 18.4 Binary Protocol Benefits

- No string parsing for parameters
- No type conversion overhead
- Smaller payload size
- Direct memory copy for simple types

---

## 19. Client Implementation Guide

### 19.1 Minimal Client Requirements

1. TLS 1.3 support (OpenSSL 1.1.1+, LibreSSL 3.4+)
2. zstd library (optional, for compression)
3. Little-endian byte handling

### 19.2 Connection Sequence

```c
// 1. TCP connect
int sock = socket(AF_INET, SOCK_STREAM, 0);
connect(sock, server_addr, sizeof(server_addr));

// 2. TLS handshake
SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
SSL* ssl = SSL_new(ctx);
SSL_set_fd(ssl, sock);
SSL_connect(ssl);

// 3. Send STARTUP
StartupMessage startup = {
    .header = {
        .magic = 0x53425750,
        .version_major = 1,
        .version_minor = 1,
        .msg_type = 0x01,
        .flags = 0,
        .length = sizeof(startup) - sizeof(MessageHeader),
        .sequence = 0,
        .attachment_id = {0},
        .txn_id = 0
    },
    .protocol_major = 1,
    .protocol_minor = 1,
    .features = FEATURE_COMPRESSION | FEATURE_STREAMING
};
// Add parameters: database, user
send_message(ssl, &startup);

// 4. Handle AUTH_REQUEST
AuthRequest* auth_req = recv_message(ssl);
// Perform authentication based on method

// 5. Send AUTH_RESPONSE
// ...

// 6. Wait for AUTH_OK and READY
// ...
```

### 19.3 Query Execution

```c
// Simple query
QueryMessage query = {
    .header = { /* ... */ .msg_type = 0x03 },
    .query_flags = QUERY_FLAG_BINARY_RESULT,
    .max_rows = 0,
    .timeout_ms = 30000
};
strcpy(query.query, "SELECT * FROM users WHERE id = 1");
send_message(ssl, &query);

// Receive results
while (true) {
    Message* msg = recv_message(ssl);
    switch (msg->header.msg_type) {
        case 0x44:  // ROW_DESCRIPTION
            handle_row_description((RowDescription*)msg);
            break;
        case 0x45:  // DATA_ROW
            handle_data_row((DataRow*)msg);
            break;
        case 0x46:  // COMMAND_COMPLETE
            return;
        case 0x48:  // ERROR
            handle_error((ErrorMessage*)msg);
            return;
    }
}
```

### 19.4 Prepared Statement Example

```c
// Parse
ParseMessage parse = { /* ... */ };
strcpy(parse.statement_name, "get_user");
strcpy(parse.query, "SELECT * FROM users WHERE id = $1");
parse.num_param_types = 1;
parse.param_types[0] = OID_INT4;
send_message(ssl, &parse);

// Bind
BindMessage bind = { /* ... */ };
strcpy(bind.portal_name, "");
strcpy(bind.statement_name, "get_user");
bind.num_param_values = 1;
bind.params[0].length = 4;
*(int32_t*)bind.params[0].data = 42;  // User ID
send_message(ssl, &bind);

// Execute
ExecuteMessage exec = { /* ... */ };
strcpy(exec.portal_name, "");
exec.max_rows = 0;
send_message(ssl, &exec);

// Sync
send_message(ssl, &sync);

// Receive results...
```

---

## 20. Hex-Level Examples

### 20.1 Handshake (STARTUP + AUTH)

```
// STARTUP (client -> server), features=COMPRESSION|STREAMING, user=alice, database=testdb
50 57 42 53 01 01 01 00 28 00 00 00 01 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
01 01 00 00 03 00 00 00 00 00 00 00
75 73 65 72 00 61 6c 69 63 65 00 64 61 74 61 62 61 73 65 00 74 65 73 74 64 62 00 00

// AUTH_REQUEST (server -> client), AUTH_PASSWORD, salt=01..10
50 57 42 53 01 01 40 00 14 00 00 00 01 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
01 00 00 00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f 10

// AUTH_RESPONSE (client -> server), password="secret"
50 57 42 53 01 01 02 00 06 00 00 00 01 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
73 65 63 72 65 74

// AUTH_OK (server -> client), attachment_id=01..10, txn_id=1, server_info={"server_version":"1.1"}
50 57 42 53 01 01 41 00 2c 00 00 00 01 00 00 00
01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f 10
01 00 00 00 00 00 00 00
01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f 10
18 00 00 00 7b 22 73 65 72 76 65 72 5f 76 65 72 73 69 6f 6e 22 3a 22 31 2e 31 22 7d

// READY (server -> client), status=idle, txn_id=1, visibility_epoch=1
50 57 42 53 01 01 43 00 14 00 00 00 01 00 00 00
01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f 10
01 00 00 00 00 00 00 00
00 00 00 00 01 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00
```

### 20.2 Simple Query (SELECT 1)

```
// QUERY (client -> server), "SELECT 1"
50 57 42 53 01 01 03 00 15 00 00 00 02 00 00 00
01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f 10
01 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 53 45 4c 45 43 54 20 31 00

// ROW_DESCRIPTION (server -> client), one int4 column named "one"
50 57 42 53 01 01 44 00 1f 00 00 00 02 00 00 00
01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f 10
01 00 00 00 00 00 00 00
01 00 00 00 03 00 00 00 6f 6e 65 00 00 00 00 00 00 17 00 00 00 04 00 ff ff ff ff 01 00 00 00

// DATA_ROW (server -> client), value=1
50 57 42 53 01 01 45 00 0d 00 00 00 02 00 00 00
01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f 10
01 00 00 00 00 00 00 00
01 00 01 00 00 04 00 00 00 01 00 00 00

// COMMAND_COMPLETE (server -> client), SELECT 1
50 57 42 53 01 01 46 00 1d 00 00 00 02 00 00 00
01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f 10
01 00 00 00 00 00 00 00
01 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00 53 45 4c 45 43 54 20 31 00

// READY (server -> client)
50 57 42 53 01 01 43 00 14 00 00 00 02 00 00 00
01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f 10
01 00 00 00 00 00 00 00
00 00 00 00 01 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00
```

## Appendix A: Protocol Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.1 | 2025-12 | Header includes attachment_id/txn_id; ATTACH_* messages; always-in-transaction clarification |
| 1.0 | 2025-12 | Initial specification |

---

## Appendix B: Reference Implementation

A reference client implementation will be provided in:
- C: `libscratchbird_client`
- Python: `scratchbird-python`
- Go: `scratchbird-go`
- Rust: `scratchbird-rs`

---

## Appendix C: Compatibility Notes

### C.1 PostgreSQL Compatibility

The ScratchBird native protocol is NOT compatible with PostgreSQL wire protocol. For PostgreSQL client compatibility, use the PostgreSQL listener (port 5432).

### C.2 Type Mapping

When converting between protocols:

| ScratchBird Type | PostgreSQL Type | Notes |
|------------------|-----------------|-------|
| INT128 | NUMERIC | Precision loss possible |
| DECIMAL128 | NUMERIC | Direct mapping |
| SB_VECTOR | FLOAT8[] | Array representation |
| SB_RECORD_ID | TEXT | String representation |
| SB_TXN_ID | INT8 | Direct mapping |
| SB_EPOCH | INT8 | Direct mapping |

---

## Appendix D: Error Recovery

### D.1 Connection Recovery

If connection is lost:

1. Close existing socket
2. Reconnect with exponential backoff (1s, 2s, 4s, ... max 30s)
3. Re-authenticate
4. Re-prepare cached statements
5. Replay any uncommitted transaction (if idempotent)

### D.2 Transaction Recovery

- Uncommitted transactions are rolled back on disconnect
- Distributed transactions: check with coordinator
- Savepoints preserved within transaction

### D.3 Stream Recovery

If streaming is interrupted:

1. Note last received row sequence
2. Reconnect
3. Re-execute with OFFSET to skip received rows
4. Continue streaming

---

**Document End**
