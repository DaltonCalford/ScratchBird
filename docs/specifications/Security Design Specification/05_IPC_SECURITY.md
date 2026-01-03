# ScratchBird IPC Security Specification

**Document ID**: SBSEC-05  
**Version**: 1.0  
**Status**: Draft for Review  
**Date**: January 2026  
**Scope**: Shared Local, Full Server, and Cluster deployment modes  

---

## 1. Introduction

### 1.1 Purpose

This document defines the security architecture for Inter-Process Communication (IPC) between ScratchBird components. It specifies how parsers communicate with the engine, how the network listener manages parsers, and how these channels are protected.

### 1.2 Scope

This specification covers:
- IPC channel architecture
- Named pipe security
- Parser registration and authentication
- Session binding over IPC
- Message integrity and encryption
- Channel lifecycle management

### 1.3 Applicability

This document applies to:
- Shared Local mode (client ↔ on-demand server)
- Full Server mode (parser ↔ engine)
- Cluster mode (parser ↔ engine, node ↔ node)

This document does NOT apply to:
- Embedded mode (no IPC, direct library calls)

### 1.4 Related Documents

| Document ID | Title |
|-------------|-------|
| SBSEC-01 | Security Architecture |
| SBSEC-02 | Identity and Authentication |
| SBSEC-06 | Cluster Security |

### 1.5 Security Level Applicability

| Feature | L0 | L1 | L2 | L3 | L4 | L5 | L6 |
|---------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| Basic IPC (named pipes) | ● | ● | ● | ● | ● | ● | ● |
| OS-level endpoint protection | ● | ● | ● | ● | ● | ● | ● |
| Parser registration tokens | | | | ● | ● | ● | ● |
| Channel binding | | | | ● | ● | ● | ● |
| Message authentication | | | | ○ | ● | ● | ● |
| IPC encryption | | | | | ○ | ● | ● |
| Mutual authentication | | | | | | ● | ● |

● = Required | ○ = Optional | (blank) = Not applicable

---

## 2. IPC Architecture

### 2.1 Overview

ScratchBird uses IPC to separate the network-facing parser layer from the security-critical engine core:

```
┌─────────────────────────────────────────────────────────────────┐
│                    IPC ARCHITECTURE                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  External Clients                                                │
│       │                                                          │
│       │ Wire Protocols (MySQL, PostgreSQL, Firebird, V2)        │
│       ▼                                                          │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │              Network Listener Process                    │    │
│  │                                                          │    │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐       │    │
│  │  │ Parser  │ │ Parser  │ │ Parser  │ │ Parser  │       │    │
│  │  │ Pool    │ │ Pool    │ │ Pool    │ │ Pool    │       │    │
│  │  │ MySQL   │ │ PgSQL   │ │ FB      │ │ V2      │       │    │
│  │  └────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘       │    │
│  │       │           │           │           │             │    │
│  └───────┼───────────┼───────────┼───────────┼─────────────┘    │
│          │           │           │           │                   │
│          └───────────┴─────┬─────┴───────────┘                  │
│                            │                                     │
│                    Named Pipe IPC                                │
│                            │                                     │
│                            ▼                                     │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                   Engine Process                         │    │
│  │                                                          │    │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐     │    │
│  │  │ IPC Handler │  │ Session Mgr │  │ SBLR Engine │     │    │
│  │  └─────────────┘  └─────────────┘  └─────────────┘     │    │
│  │                                                          │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 IPC Endpoints

| Endpoint | Purpose | Direction |
|----------|---------|-----------|
| Engine Command Pipe | Parser → Engine requests | Bidirectional |
| Engine Control Pipe | Listener ↔ Engine control | Bidirectional |
| Parser Registration Pipe | New parser registration | Parser → Engine |

### 2.3 Communication Model

```
┌─────────────────────────────────────────────────────────────────┐
│                 IPC COMMUNICATION MODEL                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Request-Response Pattern:                                       │
│                                                                  │
│  Parser                              Engine                      │
│    │                                   │                         │
│    │  ┌─────────────────────────────┐ │                         │
│    │──│ REQUEST                      │─►│                         │
│    │  │ • Message type               │ │                         │
│    │  │ • Session UUID (if bound)    │ │                         │
│    │  │ • Channel token (L3+)        │ │                         │
│    │  │ • Payload (SBLR, auth, etc.) │ │                         │
│    │  │ • HMAC (L4+)                 │ │                         │
│    │  └─────────────────────────────┘ │                         │
│    │                                   │                         │
│    │                                   │ Process request         │
│    │                                   │                         │
│    │  ┌─────────────────────────────┐ │                         │
│    │◄─│ RESPONSE                     │─│                         │
│    │  │ • Status code                │ │                         │
│    │  │ • Session UUID               │ │                         │
│    │  │ • Payload (results, error)   │ │                         │
│    │  │ • HMAC (L4+)                 │ │                         │
│    │  └─────────────────────────────┘ │                         │
│    │                                   │                         │
│                                                                  │
│  Asynchronous Notifications (Engine → Parser):                  │
│  • Session termination                                          │
│  • Privilege change notification                                │
│  • Shutdown warning                                             │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 3. Named Pipe Security

### 3.1 Platform-Specific Implementation

#### 3.1.1 Windows Named Pipes

```cpp
// Engine creates named pipe with security descriptor
SECURITY_ATTRIBUTES sa;
SECURITY_DESCRIPTOR sd;
EXPLICIT_ACCESS ea[2];

// Build ACL: SYSTEM and service account only
BuildExplicitAccessWithName(&ea[0], "NT AUTHORITY\\SYSTEM", 
    GENERIC_ALL, SET_ACCESS, NO_INHERITANCE);
BuildExplicitAccessWithName(&ea[1], service_account,
    GENERIC_READ | GENERIC_WRITE, SET_ACCESS, NO_INHERITANCE);

SetEntriesInAcl(2, ea, NULL, &acl);
SetSecurityDescriptorDacl(&sd, TRUE, acl, FALSE);

sa.lpSecurityDescriptor = &sd;

// Create pipe
HANDLE pipe = CreateNamedPipe(
    "\\\\.\\pipe\\scratchbird_engine",
    PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
    PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
    PIPE_UNLIMITED_INSTANCES,
    BUFFER_SIZE,
    BUFFER_SIZE,
    0,
    &sa
);
```

#### 3.1.2 Unix Domain Sockets

```cpp
// Engine creates Unix domain socket
int sock = socket(AF_UNIX, SOCK_STREAM, 0);

struct sockaddr_un addr;
addr.sun_family = AF_UNIX;
strncpy(addr.sun_path, "/var/run/scratchbird/engine.sock", sizeof(addr.sun_path));

// Remove any existing socket
unlink(addr.sun_path);

// Bind and set permissions
bind(sock, (struct sockaddr*)&addr, sizeof(addr));
chmod(addr.sun_path, 0600);  // Owner only
chown(addr.sun_path, service_uid, service_gid);

listen(sock, SOMAXCONN);
```

### 3.2 Endpoint Protection by Security Level

| Security Level | Windows | Unix |
|----------------|---------|------|
| 0-2 | Service account DACL | Mode 0660, service group |
| 3-4 | Above + integrity level | Mode 0600 + SELinux/AppArmor |
| 5-6 | Above + restricted token | Mode 0600 + mandatory access control |

### 3.3 Peer Credential Verification

On Unix systems, the engine verifies connecting process credentials:

```cpp
// Get peer credentials on Unix
struct ucred cred;
socklen_t len = sizeof(cred);
getsockopt(client_fd, SOL_SOCKET, SO_PEERCRED, &cred, &len);

// Verify peer is in allowed group
if (!is_allowed_uid(cred.uid) && !is_allowed_gid(cred.gid)) {
    close(client_fd);
    audit_log(IPC_UNAUTHORIZED_PEER, cred.pid, cred.uid, cred.gid);
    return;
}
```

---

## 4. Parser Registration

### 4.1 Overview

Parsers must register with the engine before handling client connections. This prevents rogue processes from impersonating parsers.

### 4.2 Registration Flow (Levels 3+)

```
┌─────────────────────────────────────────────────────────────────┐
│                  PARSER REGISTRATION FLOW                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Network Listener         Engine              Parser             │
│        │                    │                   │                │
│        │                    │                   │                │
│        │  1. Request parser spawn token         │                │
│        │────────────────────►│                   │                │
│        │                    │                   │                │
│        │  2. Return spawn token (short-lived)   │                │
│        │◄────────────────────│                   │                │
│        │                    │                   │                │
│        │  3. Spawn parser with token            │                │
│        │─────────────────────────────────────────►│                │
│        │                    │                   │                │
│        │                    │  4. Connect to registration pipe  │
│        │                    │◄───────────────────│                │
│        │                    │                   │                │
│        │                    │  5. Present spawn token           │
│        │                    │◄───────────────────│                │
│        │                    │                   │                │
│        │                    │  6. Validate token:               │
│        │                    │     • Not expired                 │
│        │                    │     • Not already used            │
│        │                    │     • Matches expected dialect    │
│        │                    │                   │                │
│        │                    │  7. Issue channel token           │
│        │                    │────────────────────►│                │
│        │                    │                   │                │
│        │                    │  8. Parser registered             │
│        │                    │     • Channel token bound         │
│        │                    │     • Ready to handle requests    │
│        │                    │                   │                │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 4.3 Spawn Token

```c
typedef struct SpawnToken {
    uint8_t     token_data[32];     // Random token
    uint64_t    issued_at;          // Timestamp
    uint64_t    expires_at;         // Short expiry (30 seconds)
    uint32_t    dialect;            // Expected dialect
    uint32_t    listener_id;        // Issuing listener
    uint8_t     hmac[32];           // HMAC-SHA256 by engine secret
} SpawnToken;
```

### 4.4 Channel Token

```c
typedef struct ChannelToken {
    uint8_t     token_id[16];       // Unique identifier
    uint8_t     token_secret[32];   // Shared secret for HMAC
    uint64_t    issued_at;
    uint64_t    expires_at;         // Long-lived (session duration)
    uint32_t    parser_id;          // Bound parser
    uint32_t    dialect;            // Parser dialect
    uint8_t     channel_key[32];    // For IPC encryption (L5+)
} ChannelToken;
```

### 4.5 Registration Validation

```python
def validate_registration(spawn_token, peer_creds):
    """Validate parser registration request."""
    
    # 1. Verify token integrity
    expected_hmac = hmac_sha256(engine_secret, spawn_token.data)
    if not constant_time_compare(spawn_token.hmac, expected_hmac):
        return REGISTRATION_INVALID_TOKEN
    
    # 2. Check expiry
    if current_time() > spawn_token.expires_at:
        return REGISTRATION_TOKEN_EXPIRED
    
    # 3. Check single-use
    if is_token_used(spawn_token.token_data):
        audit_log(REGISTRATION_TOKEN_REUSE, spawn_token, peer_creds)
        return REGISTRATION_TOKEN_REUSED
    
    # 4. Mark token as used
    mark_token_used(spawn_token.token_data)
    
    # 5. Verify peer credentials (Unix)
    if not verify_peer_credentials(peer_creds):
        return REGISTRATION_UNAUTHORIZED_PEER
    
    # 6. Generate channel token
    channel_token = generate_channel_token(spawn_token.dialect)
    
    return REGISTRATION_SUCCESS, channel_token
```

---

## 5. IPC Message Protocol

### 5.1 Message Structure

```
┌─────────────────────────────────────────────────────────────────┐
│                    IPC MESSAGE FORMAT                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Basic Message (Levels 0-2):                                    │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ Length (4B) │ Type (2B) │ Flags (2B) │ Payload (var)    │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  Authenticated Message (Levels 3-4):                            │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ Length │ Type │ Flags │ Session UUID │ Channel Token    │    │
│  │  (4B)  │ (2B) │ (2B)  │    (16B)     │    (16B)        │    │
│  ├─────────────────────────────────────────────────────────┤    │
│  │ Sequence (8B) │ Payload (variable)                      │    │
│  ├─────────────────────────────────────────────────────────┤    │
│  │ HMAC-SHA256 (32B)                                       │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  Encrypted Message (Levels 5-6):                                │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ Length │ Type │ Flags │ Channel Token ID │ Nonce (12B)  │    │
│  │  (4B)  │ (2B) │ (2B)  │      (16B)       │              │    │
│  ├─────────────────────────────────────────────────────────┤    │
│  │                                                          │    │
│  │            AES-256-GCM Encrypted Payload                │    │
│  │                                                          │    │
│  │  Plaintext contains:                                    │    │
│  │  • Session UUID (16B)                                   │    │
│  │  • Sequence number (8B)                                 │    │
│  │  • Actual payload                                       │    │
│  │                                                          │    │
│  ├─────────────────────────────────────────────────────────┤    │
│  │ GCM Auth Tag (16B)                                      │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 5.2 Message Types

| Type Code | Name | Direction | Description |
|-----------|------|-----------|-------------|
| 0x0001 | AUTH_REQUEST | P→E | Client authentication request |
| 0x0002 | AUTH_RESPONSE | E→P | Authentication result |
| 0x0003 | EXECUTE | P→E | Execute SBLR bytecode |
| 0x0004 | EXECUTE_RESULT | E→P | Execution result/rows |
| 0x0005 | TXN_CONTROL | P→E | COMMIT/ROLLBACK |
| 0x0006 | TXN_RESPONSE | E→P | Transaction result |
| 0x0007 | SESSION_END | P→E | Client disconnected |
| 0x0008 | SESSION_TERMINATED | E→P | Session forcibly ended |
| 0x0009 | KEEPALIVE | Both | Connection health check |
| 0x000A | PRIVILEGE_NOTIFY | E→P | Privilege change notification |
| 0x00F0 | REGISTER | P→E | Parser registration |
| 0x00F1 | REGISTER_RESPONSE | E→P | Registration result |
| 0x00FF | CONTROL | L↔E | Listener control commands |

### 5.3 Message Flags

| Bit | Name | Description |
|-----|------|-------------|
| 0 | ENCRYPTED | Message payload is encrypted |
| 1 | COMPRESSED | Payload is compressed |
| 2 | FRAGMENTED | Part of multi-message payload |
| 3 | LAST_FRAGMENT | Last fragment |
| 4 | URGENT | High priority message |
| 5 | RESPONSE_REQUIRED | Sender expects response |
| 6-15 | Reserved | |

### 5.4 Sequence Numbers

Sequence numbers prevent replay attacks:

```python
class SequenceTracker:
    def __init__(self, window_size=1000):
        self.expected_seq = 0
        self.window_size = window_size
        self.received = set()
    
    def validate(self, seq_num):
        # Reject old sequence numbers
        if seq_num < self.expected_seq - self.window_size:
            return SEQUENCE_TOO_OLD
        
        # Reject replays
        if seq_num in self.received:
            return SEQUENCE_REPLAY
        
        # Accept and record
        self.received.add(seq_num)
        
        # Advance window
        if seq_num >= self.expected_seq:
            self.expected_seq = seq_num + 1
            # Clean old entries
            self.received = {s for s in self.received 
                           if s >= self.expected_seq - self.window_size}
        
        return SEQUENCE_VALID
```

---

## 6. Session Binding

### 6.1 Overview

IPC channels are bound to database sessions to prevent session hijacking.

### 6.2 Binding Process

```
┌─────────────────────────────────────────────────────────────────┐
│                    SESSION BINDING                               │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  1. Parser sends AUTH_REQUEST                                    │
│     • Channel token (identifies parser)                         │
│     • Client credentials                                        │
│                                                                  │
│  2. Engine authenticates user                                    │
│     • Validates credentials                                      │
│     • Creates session with UUID                                  │
│     • Binds session to channel token                            │
│                                                                  │
│  3. Engine returns AUTH_RESPONSE                                 │
│     • Session UUID                                               │
│     • Session-specific secret (for request signing)             │
│                                                                  │
│  4. Subsequent requests must include:                           │
│     • Channel token (parser identity)                           │
│     • Session UUID (session identity)                           │
│     • Both must match engine records                            │
│                                                                  │
│  Security Properties:                                            │
│  • Parser cannot use another parser's channel token             │
│  • Parser cannot use another session's UUID                     │
│  • Session is bound to specific parser instance                 │
│  • Session termination invalidates binding                      │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 6.3 Binding Validation

```cpp
ValidationResult validate_request(IPCMessage* msg, ChannelState* channel) {
    // 1. Validate channel token
    ChannelToken* token = lookup_channel_token(msg->channel_token_id);
    if (!token) {
        return VALIDATION_INVALID_CHANNEL;
    }
    
    if (token->parser_id != channel->parser_id) {
        audit_log(IPC_CHANNEL_MISMATCH, msg, channel);
        return VALIDATION_CHANNEL_MISMATCH;
    }
    
    // 2. Validate session binding (if session established)
    if (msg->session_uuid != ZERO_UUID) {
        Session* session = lookup_session(msg->session_uuid);
        if (!session) {
            return VALIDATION_INVALID_SESSION;
        }
        
        if (session->bound_channel_token != token->token_id) {
            audit_log(IPC_SESSION_BINDING_MISMATCH, msg, session);
            return VALIDATION_SESSION_NOT_BOUND;
        }
    }
    
    // 3. Validate sequence number
    SequenceResult seq_result = validate_sequence(channel, msg->sequence);
    if (seq_result != SEQUENCE_VALID) {
        audit_log(IPC_SEQUENCE_ERROR, msg, seq_result);
        return VALIDATION_SEQUENCE_ERROR;
    }
    
    // 4. Validate HMAC (Levels 3+)
    if (security_level >= 3) {
        if (!validate_hmac(msg, token->token_secret)) {
            audit_log(IPC_HMAC_FAILURE, msg);
            return VALIDATION_HMAC_INVALID;
        }
    }
    
    return VALIDATION_SUCCESS;
}
```

---

## 7. IPC Encryption

### 7.1 Key Establishment

For Levels 5-6, IPC channels are encrypted. Key establishment occurs during parser registration:

```
┌─────────────────────────────────────────────────────────────────┐
│                 IPC KEY ESTABLISHMENT                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Parser                              Engine                      │
│    │                                   │                         │
│    │  1. Generate ephemeral ECDH keypair                        │
│    │     parser_private, parser_public                          │
│    │                                   │                         │
│    │  REGISTER + parser_public         │                         │
│    │──────────────────────────────────►│                         │
│    │                                   │                         │
│    │                    2. Generate ephemeral ECDH keypair      │
│    │                       engine_private, engine_public        │
│    │                                   │                         │
│    │                    3. Compute shared secret                │
│    │                       shared = ECDH(engine_priv, parser_pub)│
│    │                                   │                         │
│    │                    4. Derive channel key                   │
│    │                       channel_key = HKDF(shared, context)  │
│    │                                   │                         │
│    │  REGISTER_RESPONSE + engine_public│                         │
│    │◄──────────────────────────────────│                         │
│    │                                   │                         │
│    │  5. Compute shared secret         │                         │
│    │     shared = ECDH(parser_priv, engine_pub)                 │
│    │                                   │                         │
│    │  6. Derive channel key            │                         │
│    │     channel_key = HKDF(shared, context)                    │
│    │                                   │                         │
│    │  Both now have same channel_key   │                         │
│    │  All subsequent messages encrypted│                         │
│    │                                   │                         │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 7.2 Key Derivation

```python
def derive_channel_key(shared_secret, parser_public, engine_public):
    """Derive channel encryption key from ECDH shared secret."""
    
    # Context includes both public keys to bind key to specific exchange
    context = b'scratchbird-ipc-v1' + parser_public + engine_public
    
    # Use HKDF to derive key
    hkdf = HKDF(
        algorithm=hashes.SHA256(),
        length=32,
        salt=None,
        info=context
    )
    
    return hkdf.derive(shared_secret)
```

### 7.3 Message Encryption

```python
def encrypt_ipc_message(plaintext, channel_key, sequence_num):
    """Encrypt an IPC message with AES-256-GCM."""
    
    # Nonce: sequence number + random (prevents reuse across channels)
    nonce = struct.pack('>Q', sequence_num) + secure_random_bytes(4)
    
    # Encrypt
    cipher = AES_GCM(channel_key)
    ciphertext, tag = cipher.encrypt(
        nonce=nonce,
        plaintext=plaintext,
        associated_data=b''  # Could add header as AAD
    )
    
    return EncryptedMessage(
        nonce=nonce,
        ciphertext=ciphertext,
        tag=tag
    )
```

### 7.4 Key Rotation

Channel keys are rotated periodically:

| Trigger | Action |
|---------|--------|
| Time-based | Rotate every 1 hour |
| Volume-based | Rotate after 2^32 messages |
| Session end | Destroy key |
| Security event | Immediate rotation |

---

## 8. Channel Lifecycle

### 8.1 State Machine

```
┌─────────────────────────────────────────────────────────────────┐
│                 CHANNEL STATE MACHINE                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────────┐                                               │
│  │ DISCONNECTED │ Initial state                                 │
│  └──────┬───────┘                                               │
│         │ connect()                                              │
│         ▼                                                        │
│  ┌──────────────┐                                               │
│  │  CONNECTING  │ TCP/pipe connection in progress               │
│  └──────┬───────┘                                               │
│         │ connection established                                 │
│         ▼                                                        │
│  ┌──────────────┐                                               │
│  │ REGISTERING  │ Waiting for registration response             │
│  └──────┬───────┘                                               │
│         │ registration successful                                │
│         ▼                                                        │
│  ┌──────────────┐                                               │
│  │   ACTIVE     │ Ready for requests                            │
│  └──────┬───────┘                                               │
│         │                                                        │
│         ├─── keepalive timeout ───► STALE                       │
│         │                                                        │
│         ├─── error ───► ERROR                                   │
│         │                                                        │
│         ├─── graceful close ───► CLOSING                        │
│         │                                                        │
│         ▼                                                        │
│  ┌──────────────┐                                               │
│  │   CLOSING    │ Draining pending requests                     │
│  └──────┬───────┘                                               │
│         │ all requests complete                                  │
│         ▼                                                        │
│  ┌──────────────┐                                               │
│  │    CLOSED    │ Cleanup complete                              │
│  └──────────────┘                                               │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 8.2 Channel Cleanup

On channel close:

```python
def cleanup_channel(channel):
    """Clean up channel resources on close."""
    
    # 1. Terminate all sessions bound to this channel
    for session_uuid in channel.bound_sessions:
        terminate_session(session_uuid, reason='CHANNEL_CLOSED')
    
    # 2. Clear channel token
    invalidate_channel_token(channel.token_id)
    
    # 3. Secure clear keys
    if channel.channel_key:
        secure_memzero(channel.channel_key)
    
    # 4. Log closure
    audit_log('CHANNEL_CLOSED', channel.channel_id, channel.parser_id)
    
    # 5. Release resources
    close_pipe(channel.pipe_handle)
```

### 8.3 Keepalive

```python
# Parser sends keepalive every 30 seconds
KEEPALIVE_INTERVAL = 30

# Engine expects keepalive within 90 seconds
KEEPALIVE_TIMEOUT = 90

def handle_keepalive(channel):
    """Process keepalive message."""
    channel.last_keepalive = current_time()
    send_keepalive_response(channel)

def check_keepalive_timeout(channel):
    """Check if channel has timed out."""
    if current_time() - channel.last_keepalive > KEEPALIVE_TIMEOUT:
        channel.state = CHANNEL_STALE
        initiate_channel_close(channel, reason='KEEPALIVE_TIMEOUT')
```

---

## 9. Error Handling

### 9.1 Error Categories

| Category | Handling | Recovery |
|----------|----------|----------|
| Connection errors | Close channel | Parser restarts |
| Authentication errors | Return error | Client retries |
| Protocol errors | Close channel | Parser restarts |
| Session errors | Terminate session | Client reconnects |
| Internal errors | Log, return generic error | Depends on error |

### 9.2 Error Response Format

```c
typedef struct ErrorResponse {
    uint32_t    error_code;
    uint32_t    error_category;
    char        error_message[256];     // Generic, no sensitive info
    uint8_t     session_terminated;     // If session was terminated
    uint8_t     channel_closing;        // If channel is closing
} ErrorResponse;
```

### 9.3 Error Codes

| Code | Name | Description |
|------|------|-------------|
| 1001 | INVALID_MESSAGE | Malformed message |
| 1002 | INVALID_TOKEN | Unknown or expired token |
| 1003 | INVALID_SESSION | Session not found |
| 1004 | SESSION_TERMINATED | Session was terminated |
| 1005 | AUTHENTICATION_FAILED | Credentials invalid |
| 1006 | AUTHORIZATION_DENIED | Permission denied |
| 1007 | SEQUENCE_ERROR | Sequence number invalid |
| 1008 | HMAC_INVALID | Message authentication failed |
| 1009 | DECRYPTION_FAILED | Could not decrypt message |
| 2001 | INTERNAL_ERROR | Unspecified internal error |
| 2002 | RESOURCE_EXHAUSTED | No resources available |
| 2003 | SERVICE_UNAVAILABLE | Engine not accepting requests |

---

## 10. Audit Events

### 10.1 IPC Events

| Event | Logged Data |
|-------|-------------|
| CHANNEL_CONNECTED | parser_id, dialect, peer_creds |
| CHANNEL_REGISTERED | parser_id, channel_token_id |
| CHANNEL_CLOSED | parser_id, reason, duration |
| REGISTRATION_FAILED | spawn_token_hash, reason, peer_creds |
| SESSION_BOUND | session_uuid, channel_token_id |
| MESSAGE_HMAC_FAILURE | channel_id, message_type |
| MESSAGE_DECRYPT_FAILURE | channel_id, nonce |
| SEQUENCE_REPLAY | channel_id, sequence_num |
| KEEPALIVE_TIMEOUT | channel_id, last_keepalive |

### 10.2 Security Events

| Event | Severity | Description |
|-------|----------|-------------|
| UNAUTHORIZED_PEER | HIGH | Connection from unauthorized process |
| TOKEN_REUSE | CRITICAL | Spawn token used twice |
| CHANNEL_MISMATCH | HIGH | Token used on wrong channel |
| SESSION_HIJACK_ATTEMPT | CRITICAL | Session UUID from wrong channel |
| REPEATED_AUTH_FAILURES | MEDIUM | Multiple auth failures on channel |

---

## 11. Configuration Reference

### 11.1 IPC Policies

| Policy Key | Type | Default | Description |
|------------|------|---------|-------------|
| `ipc.socket_path` | string | /var/run/scratchbird/engine.sock | Unix socket path |
| `ipc.pipe_name` | string | scratchbird_engine | Windows pipe name |
| `ipc.max_connections` | integer | 1000 | Maximum concurrent channels |
| `ipc.message_max_size` | integer | 16MB | Maximum message size |
| `ipc.keepalive_interval` | integer | 30 | Keepalive interval (seconds) |
| `ipc.keepalive_timeout` | integer | 90 | Keepalive timeout (seconds) |

### 11.2 Security Policies

| Policy Key | Type | Default | Description |
|------------|------|---------|-------------|
| `ipc.require_registration` | boolean | true (L3+) | Require parser registration |
| `ipc.require_hmac` | boolean | true (L4+) | Require message HMAC |
| `ipc.require_encryption` | boolean | true (L5+) | Require channel encryption |
| `ipc.spawn_token_ttl` | integer | 30 | Spawn token lifetime (seconds) |
| `ipc.key_rotation_interval` | integer | 3600 | Key rotation interval (seconds) |

---

*End of Document*


## Appendix A: Normative Wire Format and Examples

See **SBSEC-05.A IPC Wire Format and Examples** for the stable message framing, AAD definition, nonce construction, replay semantics, and worked examples (REGISTER, encrypted message, rotation).
