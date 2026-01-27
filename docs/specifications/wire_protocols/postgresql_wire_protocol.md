# PostgreSQL Wire Protocol v3 Specification

## Protocol Version: 3.0 (PostgreSQL 7.4+)

## Overview

PostgreSQL uses a message-based protocol over TCP/IP. The protocol is asynchronous but typically used synchronously. All communication is through a stream of messages.

## Message Structure

### Basic Message Format

```
Frontend (Client → Server):
+--------+----------+----------+
| 1 byte | 4 bytes  | Variable |
+--------+----------+----------+
| Type   | Length   | Payload  |
+--------+----------+----------+

Backend (Server → Client):
+--------+----------+----------+
| 1 byte | 4 bytes  | Variable |
+--------+----------+----------+
| Type   | Length   | Payload  |
+--------+----------+----------+

Note: Length includes itself (4 bytes) but not the message type byte
```

### Byte Order
All multi-byte integers are in **network byte order** (big-endian).

## Connection Startup

### 0. SSL/GSSENC Requests (Optional, No Type Byte)

Before sending `StartupMessage`, a client can request transport encryption:

```c
struct SSLRequest {
    int32 length = 8;          // Always 8
    int32 ssl_code = 80877103; // 0x04D2162F
};

struct GSSENCRequest {
    int32 length = 8;          // Always 8
    int32 gss_code = 80877104; // 0x04D21630
};
```

Server response is a single byte:
- For SSLRequest: 'S' (0x53) = start TLS, 'N' (0x4E) = refuse.
- For GSSENCRequest: 'G' (0x47) = start GSSAPI encryption, 'N' (0x4E) = refuse.

If the server responds with `S` or `G`, perform the respective handshake, then send the normal `StartupMessage` on the encrypted channel.
If the server responds with `N`, send `StartupMessage` in plaintext (or try the other request type).

Security note (per upstream guidance): read exactly one byte before handing the socket to TLS/GSSAPI; if additional bytes are available, treat as protocol violation. If the server sends an `ErrorResponse` to SSL/GSSENC, do **not** surface it to the user (server identity is not yet authenticated); close and reconnect without SSL/GSSENC if desired.

For GSSENC, the token exchange uses `Int32` length prefixes (network order) followed by token bytes. After the GSSAPI context is established, each wrapped message is sent as `Int32 length` + wrapped bytes; the server will not accept wrapped packets larger than 16kB, so clients must segment accordingly.

### 1. Startup Message (No Type Byte)

The first message from client has no type byte:

```c
struct StartupMessage {
    int32 length;           // Total message length including this field
    int32 protocol_version; // 196608 (0x00030000) for protocol 3.0
    // Parameter pairs (null-terminated strings)
    char parameter_name[];  // e.g., "user"
    char parameter_value[]; // e.g., "postgres"
    // ... more pairs ...
    char terminator = '\0'; // Final null byte
};
```

#### Protocol Version Encoding:
```c
#define PROTOCOL_VERSION(major, minor) (((major) << 16) | (minor))
#define PG_PROTOCOL_LATEST PROTOCOL_VERSION(3, 0)  // 0x00030000 = 196608
```

#### Example Startup Message (Hex):
```
00 00 00 3D           // Length: 61 bytes
00 03 00 00           // Protocol: 3.0
75 73 65 72 00        // "user\0"
70 6F 73 74 67 72 65 73 00  // "postgres\0"
64 61 74 61 62 61 73 65 00  // "database\0"
74 65 73 74 64 62 00  // "testdb\0"
61 70 70 6C 69 63 61 74 69 6F 6E 5F 6E 61 6D 65 00  // "application_name\0"
70 73 71 6C 00        // "psql\0"
00                    // Final terminator
```

#### Startup Parameter Matrix (ScratchBird Alpha)

| Parameter | Required | Behavior |
| --- | --- | --- |
| user | yes | Used as the login username; missing user is a FATAL error. |
| database | no | Defaults to user if omitted. |
| application_name | no | Stored but not enforced in Alpha. |
| client_encoding | no | UTF8/UTF-8 accepted; other values are rejected. |
| replication | no | Any truthy value is rejected (replication not supported). |
| options | no | Accepted but ignored. |
| other keys | no | Accepted and ignored unless used by the adapter. |

Notes:
- GSSENCRequest is answered with 'N' (GSSENC not supported yet); clients must retry in plaintext.
- SSLRequest is answered with 'N' (SSL not supported yet); clients must retry in plaintext.

### 2. Authentication and Startup Response

Server responds with authentication request:

```c
enum AuthType {
    AUTH_OK                = 0,  // Authentication successful
    AUTH_KERBEROS_V5       = 2,  // Kerberos V5 (unused)
    AUTH_CLEARTEXT         = 3,  // Clear-text password
    AUTH_MD5               = 5,  // MD5 password
    AUTH_SCM_CREDS         = 6,  // SCM credentials (unused)
    AUTH_GSS               = 7,  // GSSAPI
    AUTH_GSS_CONTINUE      = 8,  // GSSAPI continue
    AUTH_SSPI              = 9,  // SSPI
    AUTH_SASL              = 10, // SASL
    AUTH_SASL_CONTINUE     = 11, // SASL continue
    AUTH_SASL_FINAL        = 12  // SASL final
};

struct AuthenticationRequest {
    char  type = 'R';      // Message type
    int32 length;          // Message length
    int32 auth_type;       // Authentication type
    char  auth_data[];     // Type-specific data
};
```

After successful authentication (`AuthenticationOk`), the backend typically sends:
- `ParameterStatus` (runtime parameter key/value pairs, multiple messages)
- `BackendKeyData` (PID and secret key for cancellation)
- `ReadyForQuery` (transaction status)

If the server does not understand some startup parameters, it may send `NegotiateProtocolVersion` (type 'v') before proceeding.

### 3. CancelRequest (Out-of-band)

Cancellation is sent on a separate TCP connection (no SSL/GSSENC unless you explicitly negotiate it):

```c
struct CancelRequest {
    int32 length = 16;
    int32 cancel_code = 80877102;  // 0x04D2162E
    int32 process_id;              // From BackendKeyData
    int32 secret_key;              // From BackendKeyData
};
```

#### MD5 Authentication

```c
struct AuthenticationMD5 {
    char  type = 'R';
    int32 length = 12;
    int32 auth_type = 5;   // AUTH_MD5
    char  salt[4];         // Random salt
};

// Client response:
struct PasswordMessage {
    char  type = 'p';
    int32 length;
    char  password[];      // "md5" + md5(md5(password + username) + salt)
};

// MD5 calculation:
char* calculate_md5_password(const char* password, const char* username, const char* salt) {
    // Step 1: MD5(password + username)
    char temp[MD5_DIGEST_LENGTH * 2 + 1];
    MD5_CTX ctx;
    unsigned char digest[MD5_DIGEST_LENGTH];
    
    MD5_Init(&ctx);
    MD5_Update(&ctx, password, strlen(password));
    MD5_Update(&ctx, username, strlen(username));
    MD5_Final(digest, &ctx);
    
    // Convert to hex
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(temp + i * 2, "%02x", digest[i]);
    }
    
    // Step 2: MD5(result + salt)
    MD5_Init(&ctx);
    MD5_Update(&ctx, temp, 32);
    MD5_Update(&ctx, salt, 4);
    MD5_Final(digest, &ctx);
    
    // Format: "md5" + hex
    char* result = malloc(36);
    strcpy(result, "md5");
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(result + 3 + i * 2, "%02x", digest[i]);
    }
    
    return result;
}
```

#### Worked Hex Flow (Startup + MD5 + Simple Query)

Example flow for `user=alice`, `database=testdb`, MD5 auth, and `SELECT 1`:

StartupMessage (no type byte):
```
00 00 00 4F           // Length: 79
00 03 00 00           // Protocol: 3.0
75 73 65 72 00        // "user\0"
61 6C 69 63 65 00     // "alice\0"
64 61 74 61 62 61 73 65 00  // "database\0"
74 65 73 74 64 62 00  // "testdb\0"
61 70 70 6C 69 63 61 74 69 6F 6E 5F 6E 61 6D 65 00  // "application_name\0"
70 73 71 6C 00        // "psql\0"
63 6C 69 65 6E 74 5F 65 6E 63 6F 64 69 6E 67 00  // "client_encoding\0"
55 54 46 38 00        // "UTF8\0"
00                    // Terminator
```

AuthenticationMD5 request:
```
52 00 00 00 0C 00 00 00 05 01 02 03 04
```

PasswordMessage (`md5aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\0`):
```
70 00 00 00 28
6D 64 35 61 61 61 61 61 61 61 61 61 61 61 61 61 61
61 61 61 61 61 61 61 61 61 61 61 61 61 61 61 61 61
00
```

AuthenticationOk + ParameterStatus + BackendKeyData + ReadyForQuery:
```
52 00 00 00 08 00 00 00 00                        // Auth OK
53 00 00 00 19 63 6C 69 65 6E 74 5F 65 6E 63 6F 64 69 6E 67 00 55 54 46 38 00
4B 00 00 00 0C 00 00 04 D2 01 02 03 04            // BackendKeyData
5A 00 00 00 05 49                                  // ReadyForQuery (idle)
```

Simple query (`SELECT 1`) and result:
```
51 00 00 00 0D 53 45 4C 45 43 54 20 31 00          // Query
54 00 00 00 20                                        // RowDescription
00 01                                                 // Field count = 1
63 6F 6C 75 6D 6E 31 00                               // "column1\0"
00 00 00 00                                           // Table OID
00 00                                                 // Column attr
00 00 00 17                                           // Type OID (int4)
00 04                                                 // Type size
FF FF FF FF                                           // Type modifier
00 00                                                 // Format code
44 00 00 00 0B 00 01 00 00 00 01 31                // DataRow ("1")
43 00 00 00 0D 53 45 4C 45 43 54 20 31 00          // CommandComplete
5A 00 00 00 05 49                                  // ReadyForQuery
```

#### SCRAM-SHA-256 Authentication

```c
struct AuthenticationSASL {
    char  type = 'R';
    int32 length;
    int32 auth_type = 10;  // AUTH_SASL
    // List of supported mechanisms (null-terminated strings)
    char mechanisms[];     // e.g., "SCRAM-SHA-256\0SCRAM-SHA-256-PLUS\0\0"
};

// Client initiates SASL:
struct SASLInitialResponse {
    char  type = 'p';
    int32 length;
    char  mechanism[];     // Selected mechanism
    int32 response_length; // -1 if no initial response
    char  response[];      // Initial client response
};

// SCRAM-SHA-256 Flow:
// 1. Client-first: "n,,n=username,r=client-nonce"
// 2. Server-first: "r=combined-nonce,s=salt,i=iterations"
// 3. Client-final: "c=channel-binding,r=combined-nonce,p=client-proof"
// 4. Server-final: "v=server-signature"
```

ScratchBird SASL support in Alpha:
- Server advertises only SCRAM-SHA-256.
- SCRAM-SHA-256-PLUS (channel binding) is not supported and is rejected.
- SCRAM client-first messages with channel binding (GS2 "p=") are rejected.
- If the client selects any other SASL mechanism, the server returns a FATAL error (SQLSTATE 0A000).

#### Worked Hex Flow (SCRAM-SHA-256)

Example flow for `user=alice`, `database=testdb`:

AuthenticationSASL (server → client):
```
52 00 00 00 17 00 00 00 0A 53 43 52 41 4D 2D 53 48 41 2D 32 35 36 00 00
```

SASLInitialResponse (client → server), client-first `n,,n=alice,r=abcdef`:
```
70 00 00 00 29
53 43 52 41 4D 2D 53 48 41 2D 32 35 36 00
00 00 00 13
6E 2C 2C 6E 3D 61 6C 69 63 65 2C 72 3D 61 62 63 64 65 66
```

AuthenticationSASLContinue (server → client), server-first:
`r=abcdefXYZ,s=QSXCR+Q6sek8bf92,i=4096`
```
52 00 00 00 2D 00 00 00 0B
72 3D 61 62 63 64 65 66 58 59 5A 2C 73 3D 51 53 58 43 52 2B 51 36 73 65 6B 38 62 66 39 32 2C 69 3D 34 30 39 36
```

SASLResponse (client → server), client-final:
`c=biws,r=abcdefXYZ,p=xyz`
```
70 00 00 00 1C
63 3D 62 69 77 73 2C 72 3D 61 62 63 64 65 66 58 59 5A 2C 70 3D 78 79 7A
```

AuthenticationSASLFinal (server → client), server-final:
`v=abc123`
```
52 00 00 00 10 00 00 00 0C 76 3D 61 62 63 31 32 33
```

AuthenticationOk + ReadyForQuery:
```
52 00 00 00 08 00 00 00 00
5A 00 00 00 05 49
```

## Message Types

### Frontend Messages (Client → Server)

```c
// Message type codes
#define MSG_BIND                'B'  // Bind portal
#define MSG_CLOSE               'C'  // Close portal/statement
#define MSG_COPY_DATA           'd'  // COPY data
#define MSG_COPY_DONE           'c'  // COPY done
#define MSG_COPY_FAIL           'f'  // COPY failed
#define MSG_DESCRIBE            'D'  // Describe portal/statement
#define MSG_EXECUTE             'E'  // Execute portal
#define MSG_FLUSH               'H'  // Flush output
#define MSG_FUNCTION_CALL       'F'  // Function call
#define MSG_PARSE               'P'  // Parse SQL
#define MSG_PASSWORD            'p'  // Password response
#define MSG_QUERY               'Q'  // Simple query
#define MSG_SYNC                'S'  // Sync (end of messages)
#define MSG_TERMINATE           'X'  // Terminate connection
```

### Backend Messages (Server → Client)

```c
// Message type codes
#define MSG_AUTH_REQUEST        'R'  // Authentication request
#define MSG_BACKEND_KEY_DATA    'K'  // Backend key data
#define MSG_BIND_COMPLETE       '2'  // Bind complete
#define MSG_CLOSE_COMPLETE      '3'  // Close complete
#define MSG_COMMAND_COMPLETE    'C'  // Command complete
#define MSG_COPY_IN_RESPONSE    'G'  // Start COPY IN
#define MSG_COPY_OUT_RESPONSE   'H'  // Start COPY OUT
#define MSG_COPY_BOTH_RESPONSE  'W'  // Start COPY BOTH
#define MSG_DATA_ROW            'D'  // Data row
#define MSG_EMPTY_QUERY         'I'  // Empty query
#define MSG_ERROR               'E'  // Error
#define MSG_FUNCTION_RESULT     'V'  // Function call result
#define MSG_NO_DATA             'n'  // No data
#define MSG_NOTICE              'N'  // Notice
#define MSG_NOTIFICATION        'A'  // Async notification
#define MSG_NEGOTIATE_VERSION   'v'  // NegotiateProtocolVersion
#define MSG_PARAMETER_DESC      't'  // Parameter description
#define MSG_PARAMETER_STATUS    'S'  // Parameter status
#define MSG_PARSE_COMPLETE      '1'  // Parse complete
#define MSG_PORTAL_SUSPENDED    's'  // Portal suspended
#define MSG_READY_FOR_QUERY     'Z'  // Ready for query
#define MSG_ROW_DESCRIPTION     'T'  // Row description
```

## Simple Query Protocol

### Query Message

```c
struct Query {
    char  type = 'Q';
    int32 length;
    char  query[];         // SQL query string (null-terminated)
};
```

### Response Flow

```
Client                          Server
  |                               |
  |-------- Query ('Q') -------->|
  |                               |
  |<----- RowDescription ('T') ---|  (if SELECT)
  |<------- DataRow ('D') --------|  (repeated)
  |<--- CommandComplete ('C') ----|
  |<---- ReadyForQuery ('Z') -----|
  |                               |
```

### Response Messages

#### RowDescription
```c
struct RowDescription {
    char  type = 'T';
    int32 length;
    int16 num_fields;      // Number of fields
    
    struct Field {
        char  name[];      // Field name (null-terminated)
        int32 table_oid;   // Table OID (0 if not a table field)
        int16 column_id;   // Column attribute number (0 if not a table column)
        int32 type_oid;    // Data type OID
        int16 type_size;   // Data type size (-1 for variable)
        int32 type_modifier; // Type modifier
        int16 format_code; // 0 = text, 1 = binary
    } fields[];
};
```

#### DataRow
```c
struct DataRow {
    char  type = 'D';
    int32 length;
    int16 num_columns;     // Number of column values
    
    struct Column {
        int32 length;      // -1 for NULL, otherwise data length
        char  data[];      // Column data (if not NULL)
    } columns[];
};
```

#### CommandComplete
```c
struct CommandComplete {
    char  type = 'C';
    int32 length;
    char  tag[];           // Command tag (e.g., "SELECT 5", "INSERT 0 1")
};
```

#### ReadyForQuery
```c
struct ReadyForQuery {
    char  type = 'Z';
    int32 length = 5;
    char  status;          // 'I' = idle, 'T' = in transaction, 'E' = error
};
```

#### ParameterStatus
```c
struct ParameterStatus {
    char  type = 'S';
    int32 length;
    char  name[];          // Null-terminated
    char  value[];         // Null-terminated
};
```

ScratchBird sends the following ParameterStatus keys on successful startup:
- server_version = "15.0.0 ScratchBird"
- server_encoding = "UTF8"
- client_encoding = "UTF8"
- DateStyle = "ISO, MDY"
- TimeZone = "UTC"
- integer_datetimes = "on"
- standard_conforming_strings = "on"

#### BackendKeyData
```c
struct BackendKeyData {
    char  type = 'K';
    int32 length = 12;
    int32 process_id;
    int32 secret_key;
};
```

#### NegotiateProtocolVersion
```c
struct NegotiateProtocolVersion {
    char  type = 'v';
    int32 length;
    int32 newest_minor;    // Newest minor version supported for major version
    int32 num_options;
    char  option_name[];   // Repeated null-terminated option names
};
```

#### EmptyQueryResponse
```c
struct EmptyQueryResponse {
    char  type = 'I';
    int32 length = 4;
};
```

## Extended Query Protocol

### Parse Message
```c
struct Parse {
    char  type = 'P';
    int32 length;
    char  statement_name[]; // Empty string for unnamed
    char  query[];         // SQL with $1, $2... placeholders
    int16 num_param_types; // Number of parameter types
    int32 param_types[];   // OIDs of parameter types (0 = unspecified)
};
```

### Bind Message
```c
struct Bind {
    char  type = 'B';
    int32 length;
    char  portal_name[];   // Empty string for unnamed
    char  statement_name[]; // Statement to bind
    int16 num_format_codes; // Parameter format codes
    int16 format_codes[];  // 0 = text, 1 = binary
    int16 num_params;      // Number of parameters
    
    struct Parameter {
        int32 length;      // -1 for NULL
        char  value[];     // Parameter value (if not NULL)
    } params[];
    
    int16 num_result_formats; // Result format codes
    int16 result_formats[];   // 0 = text, 1 = binary
};
```

### Execute Message
```c
struct Execute {
    char  type = 'E';
    int32 length;
    char  portal_name[];   // Portal to execute
    int32 max_rows;        // 0 = unlimited
};
```

### Describe Message
```c
struct Describe {
    char  type = 'D';
    int32 length;
    char  describe_type;  // 'S' = statement, 'P' = portal
    char  name[];         // Statement or portal name
};
```

### Close Message
```c
struct Close {
    char  type = 'C';
    int32 length;
    char  close_type;     // 'S' = statement, 'P' = portal
    char  name[];         // Statement or portal name
};
```

### Flush / Sync / Terminate
```c
struct Flush {
    char  type = 'H';
    int32 length = 4;
};

struct Sync {
    char  type = 'S';
    int32 length = 4;
};

struct Terminate {
    char  type = 'X';
    int32 length = 4;
};
```

### Extended-Query Response Messages
```c
struct ParseComplete {
    char  type = '1';
    int32 length = 4;
};

struct BindComplete {
    char  type = '2';
    int32 length = 4;
};

struct CloseComplete {
    char  type = '3';
    int32 length = 4;
};

struct NoData {
    char  type = 'n';
    int32 length = 4;
};

struct PortalSuspended {
    char  type = 's';
    int32 length = 4;
};

struct ParameterDescription {
    char  type = 't';
    int32 length;
    int16 num_params;
    int32 param_type_oids[];  // num_params entries
};
```

### Extended Query Flow
```
Client                          Server
  |                               |
  |--------- Parse ('P') -------->|
  |<---- ParseComplete ('1') ------|
  |                               |
  |---------- Bind ('B') -------->|
  |<---- BindComplete ('2') -------|
  |                               |
  |-------- Describe ('D') ------>|  (optional)
  |<--- ParameterDescription ------|
  |<------ RowDescription ---------|
  |                               |
  |-------- Execute ('E') ------->|
  |<------- DataRow ('D') ---------|  (repeated)
  |<--- CommandComplete ('C') -----|
  |                               |
  |---------- Sync ('S') -------->|
  |<---- ReadyForQuery ('Z') ------|
  |                               |
```

### Worked Hex Flow (Parse/Bind/Describe/Execute)

Example: `SELECT $1::int4 AS v` with parameter `42` (text format).

Client → Server (Parse, statement name `s1`):
```
50 00 00 00 22
73 31 00
53 45 4C 45 43 54 20 24 31 3A 3A 69 6E 74 34 20 41 53 20 76 00
00 01
00 00 00 17
```

Server → Client:
```
31 00 00 00 04  // ParseComplete
```

Client → Server (Bind, unnamed portal):
```
42 00 00 00 14
00
73 31 00
00 00
00 01
00 00 00 02 34 32
00 00
```

Server → Client:
```
32 00 00 00 04  // BindComplete
```

Client → Server (Describe portal):
```
44 00 00 00 06 50 00
```

Server → Client (RowDescription, one int4 column named "v"):
```
54 00 00 00 1A
00 01
76 00
00 00 00 00
00 00
00 00 00 17
00 04
FF FF FF FF
00 00
```

Client → Server (Execute + Sync):
```
45 00 00 00 09 00 00 00 00 00
53 00 00 00 04
```

Server → Client (Row + Complete + Ready):
```
44 00 00 00 0C 00 01 00 00 00 02 34 32
43 00 00 00 0D 53 45 4C 45 43 54 20 31 00
5A 00 00 00 05 49
```

### Extended Query Edge Cases (ScratchBird Alpha)

- Unnamed statement/portal: empty name ("") is allowed; a new Parse/Bind with empty name overwrites the previous unnamed entry.
- Describe before execute: if a statement has not been executed yet, ScratchBird may return NoData because result columns are unknown. After first execute, result columns are cached for subsequent Describe calls.
- Parameter format codes:
  - If num_format_codes is 0, all parameters are treated as text.
  - If num_format_codes is 1, that format applies to all parameters.
  - Only format 0 (text) and 1 (binary) are supported. Other values are treated as text.
- Result format codes:
  - If num_result_formats is 0, all results are text.
  - If num_result_formats is 1, that format applies to all columns.
  - Only format 0 (text) and 1 (binary) are supported. Other values are treated as text.
- Execute with max_rows:
  - ScratchBird buffers up to max_rows and returns PortalSuspended when more rows remain.
  - Subsequent Execute calls on the same portal return remaining buffered rows without re-executing the statement.
  - When exhausted, the portal returns CommandComplete and remains complete until Closed.
- Error handling:
  - On Parse/Bind/Describe/Execute errors, ScratchBird sends ErrorResponse and waits for Sync to return to ReadyForQuery.
  - Flush only sends queued output; it does not change protocol state.

## COPY Protocol

### COPY IN (Client → Server)

```c
struct CopyInResponse {
    char  type = 'G';
    int32 length;
    int8  format;          // 0 = text, 1 = binary
    int16 num_columns;
    int16 column_formats[]; // Format codes per column
};

// Client sends:
struct CopyData {
    char  type = 'd';
    int32 length;
    char  data[];          // Row data
};

struct CopyDone {
    char  type = 'c';
    int32 length = 4;
};

// Or on error:
struct CopyFail {
    char  type = 'f';
    int32 length;
    char  error_message[];
};
```

COPY IN flow:
1) Server sends CopyInResponse.
2) Client streams CopyData messages.
3) Client terminates with CopyDone (success) or CopyFail (error).
4) Server replies with CommandComplete + ReadyForQuery on success, or ErrorResponse + ReadyForQuery on failure.

The COPY payload format (CSV, DELIMITER, NULL, HEADER, etc.) is defined by the SQL COPY statement; the wire protocol only frames CopyData chunks.

### COPY OUT (Server → Client)

```c
struct CopyOutResponse {
    char  type = 'H';
    int32 length;
    int8  format;          // 0 = text, 1 = binary
    int16 num_columns;
    int16 column_formats[];
};
// Server then sends CopyData messages, followed by CopyDone
```

COPY OUT flow:
1) Server sends CopyOutResponse.
2) Server streams CopyData messages.
3) Server terminates with CopyDone, then CommandComplete + ReadyForQuery.

### COPY BOTH (Streaming Replication)

```c
struct CopyBothResponse {
    char  type = 'W';
    int32 length;
    int8  format;          // Always 0
    int16 num_columns;
    int16 column_formats[];
};
```

## Error and Notice Messages

```c
struct ErrorResponse {
    char  type = 'E';      // 'E' for error, 'N' for notice
    int32 length;
    
    // Field type/value pairs
    struct Field {
        char  type;        // Field type code
        char  value[];     // Null-terminated string
    } fields[];
    
    char  terminator = '\0'; // Message terminator
};

// Field type codes:
#define ERR_SEVERITY         'S'  // ERROR, WARNING, etc.
#define ERR_SEVERITY_V       'V'  // Localized severity
#define ERR_CODE             'C'  // SQLSTATE code
#define ERR_MESSAGE          'M'  // Primary message
#define ERR_DETAIL           'D'  // Detail message
#define ERR_HINT             'H'  // Hint message
#define ERR_POSITION         'P'  // Error position
#define ERR_INTERNAL_POS     'p'  // Internal position
#define ERR_INTERNAL_QUERY   'q'  // Internal query
#define ERR_WHERE            'W'  // Context
#define ERR_SCHEMA           's'  // Schema name
#define ERR_TABLE            't'  // Table name
#define ERR_COLUMN           'c'  // Column name
#define ERR_DATATYPE         'd'  // Data type name
#define ERR_CONSTRAINT       'n'  // Constraint name
#define ERR_FILE             'F'  // File name
#define ERR_LINE             'L'  // Line number
#define ERR_ROUTINE          'R'  // Routine name
```

ScratchBird emits the following fields today:
- Required: ERR_SEVERITY, ERR_CODE, ERR_MESSAGE
- Optional: ERR_DETAIL, ERR_HINT

Severity strings used by ScratchBird: ERROR, FATAL, PANIC, WARNING, NOTICE. Other fields may be added as catalog/context mapping expands.

Example Error Message:
```
45 00 00 00 74    // Type 'E', Length 116
53 45 52 52 4F 52 00  // S: "ERROR\0"
56 45 52 52 4F 52 00  // V: "ERROR\0"
43 32 33 35 30 32 00  // C: "23502\0" (not_null_violation)
4D 6E 75 6C 6C 20 76 61 6C 75 65 20 // M: "null value in column..."
...
00                 // Terminator
```

## Data Type Encodings

### Text Format
All values are transmitted as strings in the client encoding (typically UTF-8). The string length is provided by the surrounding `DataRow` field length.

### Binary Format

```c
// Integer types (big-endian)
int16_t  INT2;         // 2 bytes
int32_t  INT4;         // 4 bytes
int64_t  INT8;         // 8 bytes

// Floating point (big-endian)
float    FLOAT4;       // 4 bytes (IEEE 754)
double   FLOAT8;       // 8 bytes (IEEE 754)

// Boolean
uint8_t  BOOL;         // 0 = false, 1 = true

// Variable-length types
// In a DataRow, the length is carried by the column length field.
struct Text {
    char  data[];      // Exactly "column length" bytes
};

// Numeric/Decimal
struct Numeric {
    int16 ndigits;     // Number of digits
    int16 weight;      // Weight of first digit
    int16 sign;        // 0x0000=positive, 0x4000=negative, 0xC000=NaN
    int16 dscale;      // Display scale
    int16 digits[];    // Base-10000 digits
};

// Timestamp (microseconds since 2000-01-01)
int64_t timestamp;

// Date (days since 2000-01-01)
int32_t date;

// Time (microseconds since midnight)
int64_t time;

// UUID
uint8_t uuid[16];

// Arrays
struct Array {
    int32 ndim;        // Number of dimensions
    int32 flags;       // Has null values flag
    int32 element_oid; // Element type OID
    
    struct Dimension {
        int32 size;    // Dimension size
        int32 lower;   // Lower bound (usually 1)
    } dims[];
    
    // Array elements (each with length prefix)
    struct Element {
        int32 length;  // -1 for NULL
        char  data[];
    } elements[];
};
```

## Notification Protocol

```c
struct NotificationResponse {
    char  type = 'A';
    int32 length;
    int32 pid;         // Process ID of notifying backend
    char  channel[];   // Channel name (null-terminated)
    char  payload[];   // Payload string (null-terminated)
};
```

## Cancellation Protocol

Cancellation uses a separate connection:

```c
struct CancelRequest {
    int32 length = 16;
    int32 cancel_code = 80877102;  // Magic number (0x04D2162E)
    int32 process_id;   // From BackendKeyData
    int32 secret_key;   // From BackendKeyData
};

struct BackendKeyData {
    char  type = 'K';
    int32 length = 12;
    int32 process_id;   // Backend process ID
    int32 secret_key;   // Secret key for cancellation
};
```

The CancelRequest has no type byte. It can be sent on a plaintext connection or after SSL/GSSENC negotiation (send SSLRequest/GSSENCRequest, complete encryption handshake, then send CancelRequest directly).

## Streaming Replication Protocol

### Replication Commands (via Simple Query)

```sql
-- Identify system
IDENTIFY_SYSTEM

-- Start replication
START_REPLICATION SLOT slot_name LOGICAL 0/0

-- Create replication slot
CREATE_REPLICATION_SLOT slot_name LOGICAL pgoutput

-- Timeline history
TIMELINE_HISTORY 2
```

### Write-after Log (WAL) Data Messages (optional post-gold)

**Scope Note:** ScratchBird does not implement write-after log (WAL, optional post-gold)-based replication. This section is included as PostgreSQL protocol reference only.

In streaming replication, `CopyBothResponse` is used and **all** replication traffic is carried inside `CopyData` (type 'd') messages. The first byte of the `CopyData` payload is a submessage type:
- 'w' = XLogData (primary → standby)
- 'k' = PrimaryKeepAlive (primary → standby)
- 'r' = StandbyStatusUpdate (standby → primary)

The remaining bytes are the submessage payload as defined below.

```c
struct XLogData {
    char  type = 'w';
    int32 length;
    int64 wal_start;   // WAL (optional post-gold) start position
    int64 wal_end;     // WAL (optional post-gold) end position
    int64 timestamp;   // Send time
    char  wal_data[];  // WAL (optional post-gold) data
};

struct PrimaryKeepAlive {
    char  type = 'k';
    int32 length = 17;
    int64 wal_end;     // Current end of WAL (optional post-gold)
    int64 timestamp;   // Send time
    char  reply_requested; // 1 if reply requested
};

struct StandbyStatusUpdate {
    char  type = 'r';
    int32 length = 34;
    int64 received;    // Last WAL (optional post-gold) byte received
    int64 flushed;     // Last WAL (optional post-gold) byte flushed
    int64 applied;     // Last WAL (optional post-gold) byte applied
    int64 timestamp;   // Send time
    char  reply_requested; // 1 if reply requested
};
```

## Protocol State Machine

```
    ┌────────────┐
    │Disconnected│
    └─────┬──────┘
          │ Connect
          ▼
    ┌────────────┐
    │  SSL Nego  │ (optional)
    └─────┬──────┘
          │
          ▼
    ┌────────────┐
    │   Startup  │
    └─────┬──────┘
          │
          ▼
    ┌────────────┐
    │    Auth    │
    └─────┬──────┘
          │ Auth OK
          ▼
    ┌────────────┐
    │   Ready    │◄─────────┐
    └─────┬──────┘          │
          │                  │
          ▼                  │
    ┌────────────┐          │
    │  Executing │          │
    └─────┬──────┘          │
          │ Complete        │
          └─────────────────┘
```

## Example: Handshake (Trust Auth)

Client StartupMessage (user=bob, database=test):
```
00 00 00 20 00 03 00 00
75 73 65 72 00 62 6f 62 00
64 61 74 61 62 61 73 65 00 74 65 73 74 00
00
```

Server AuthenticationOk + BackendKeyData + ReadyForQuery:
```
52 00 00 00 08 00 00 00 00
4b 00 00 00 0c 00 00 04 d2 00 00 16 2e
5a 00 00 00 05 49
```

## Complete Example: SELECT Query

### 1. Client sends Query
```
51 00 00 00 17    // Type 'Q', Length 23
53 45 4C 45 43 54 20 2A 20 46 52 4F 4D 20 75 73 65 72 73 00
// "SELECT * FROM users\0"
```

### 2. Server sends RowDescription
```
54 00 00 00 6E    // Type 'T', Length 110
00 03              // 3 fields

// Field 1: id
69 64 00           // "id\0"
00 00 40 02        // Table OID 16386
00 01              // Column 1
00 00 00 17        // Type OID 23 (int4)
00 04              // Type size 4
FF FF FF FF        // Type modifier -1
00 00              // Format 0 (text)

// Field 2: name
6E 61 6D 65 00     // "name\0"
00 00 40 02        // Table OID 16386
00 02              // Column 2
00 00 00 19        // Type OID 25 (text)
FF FF              // Type size -1 (variable)
FF FF FF FF        // Type modifier -1
00 00              // Format 0 (text)

// Field 3: email
65 6D 61 69 6C 00  // "email\0"
00 00 40 02        // Table OID 16386
00 03              // Column 3
00 00 00 19        // Type OID 25 (text)
FF FF              // Type size -1
FF FF FF FF        // Type modifier -1
00 00              // Format 0 (text)
```

### 3. Server sends DataRows
```
44 00 00 00 20    // Type 'D', Length 32
00 03              // 3 columns

00 00 00 01        // Column 1: length 1
31                 // "1"

00 00 00 04        // Column 2: length 4
4A 6F 68 6E        // "John"

00 00 00 10        // Column 3: length 16
6A 6F 68 6E 40 65 78 61 6D 70 6C 65 2E 63 6F 6D  // "john@example.com"
```

### 4. Server sends CommandComplete
```
43 00 00 00 0D    // Type 'C', Length 13
53 45 4C 45 43 54 20 31 00  // "SELECT 1\0"
```

### 5. Server sends ReadyForQuery
```
5A 00 00 00 05    // Type 'Z', Length 5
49                 // 'I' (idle)
```

## Implementation Example

### Sending a Message
```c
void send_message(int socket, char type, const void* data, size_t data_len) {
    char header[5];
    header[0] = type;
    
    uint32_t length = htonl(data_len + 4);  // Include length field
    memcpy(header + 1, &length, 4);
    
    send(socket, header, 5, MSG_NOSIGNAL);
    if (data_len > 0) {
        send(socket, data, data_len, MSG_NOSIGNAL);
    }
}
```

### Reading a Message
```c
struct pg_message {
    char type;
    uint32_t length;
    char* data;
};

struct pg_message* read_message(int socket) {
    struct pg_message* msg = malloc(sizeof(struct pg_message));
    
    // Read type
    recv(socket, &msg->type, 1, MSG_WAITALL);
    
    // Read length
    recv(socket, &msg->length, 4, MSG_WAITALL);
    msg->length = ntohl(msg->length) - 4;  // Subtract length field
    
    // Read data
    if (msg->length > 0) {
        msg->data = malloc(msg->length);
        recv(socket, msg->data, msg->length, MSG_WAITALL);
    } else {
        msg->data = NULL;
    }
    
    return msg;
}
```

### Parsing Row Data
```c
void parse_data_row(struct pg_message* msg) {
    char* ptr = msg->data;
    
    // Get number of columns
    int16_t num_columns = ntohs(*(int16_t*)ptr);
    ptr += 2;
    
    for (int i = 0; i < num_columns; i++) {
        // Get column length
        int32_t length = ntohl(*(int32_t*)ptr);
        ptr += 4;
        
        if (length == -1) {
            printf("Column %d: NULL\n", i);
        } else {
            printf("Column %d: %.*s\n", i, length, ptr);
            ptr += length;
        }
    }
}
```

## Common OIDs (Object Identifiers)

```c
// Common type OIDs
#define OID_BOOL         16
#define OID_BYTEA        17
#define OID_CHAR         18
#define OID_NAME         19
#define OID_INT8         20
#define OID_INT2         21
#define OID_INT4         23
#define OID_TEXT         25
#define OID_OID          26
#define OID_FLOAT4       700
#define OID_FLOAT8       701
#define OID_VARCHAR      1043
#define OID_DATE         1082
#define OID_TIME         1083
#define OID_TIMESTAMP    1114
#define OID_TIMESTAMPTZ  1184
#define OID_INTERVAL     1186
#define OID_NUMERIC      1700
#define OID_UUID         2950
#define OID_JSON         114
#define OID_JSONB        3802
```

## Security Considerations

1. **Always use SSL/TLS** for production
2. **Use SCRAM-SHA-256** instead of MD5 authentication
3. **Implement connection pooling** with proper authentication
4. **Validate message lengths** to prevent buffer overflows
5. **Use parameterized queries** (extended protocol)
6. **Implement query timeouts**
7. **Monitor for protocol violations**
8. **Rotate authentication credentials regularly**

## Protocol Versions

- Protocol 1.0: PostgreSQL 6.3 and earlier
- Protocol 2.0: PostgreSQL 6.4 to 7.3
- Protocol 3.0: PostgreSQL 7.4+ (current)

## References

- PostgreSQL Source: src/backend/libpq/
- PostgreSQL Documentation: Protocol Message Formats
- libpq Source Code
- Wireshark PostgreSQL Dissector
