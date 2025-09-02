# Complete Database Protocols and Data Types Technical Reference

## Table of Contents
1. [FirebirdSQL Complete Protocol Specification](#firebirdsql-complete-protocol-specification)
2. [Microsoft SQL Server TDS Protocol Specification](#microsoft-sql-server-tds-protocol-specification)
3. [PostgreSQL Wire Protocol Specification](#postgresql-wire-protocol-specification)
4. [MySQL Protocol Specification](#mysql-protocol-specification)
5. [MariaDB Extended Protocol](#mariadb-extended-protocol)
6. [JDBC Type System and Implementation](#jdbc-type-system-and-implementation)
7. [ODBC API and Protocol Details](#odbc-api-and-protocol-details)

---

# FirebirdSQL Complete Protocol Specification

## Protocol Overview

FirebirdSQL uses a proprietary wire protocol with the following characteristics:
- **Default Port**: 3050
- **Byte Order**: Network (Big-Endian)
- **Protocol Version**: 10, 11, 12, 13 (depending on Firebird version)
- **Packet Structure**: Header + Operation + Data

## Wire Protocol Structure

### Packet Header
```c
typedef struct P_OP_HEADER {
    ULONG   op_code;        // Operation code (4 bytes)
    ULONG   op_length;      // Packet length (4 bytes)
} P_OP_HEADER;

// Operation codes
#define op_connect              1
#define op_exit                 2
#define op_accept               3
#define op_reject               4
#define op_protocol             5
#define op_disconnect           6
#define op_response             9
#define op_attach               19
#define op_create               20
#define op_detach               21
#define op_compile              22
#define op_start                23
#define op_start_and_send       24
#define op_send                 25
#define op_receive              26
#define op_unwind               27
#define op_release              28
#define op_transaction          29
#define op_commit               30
#define op_rollback             31
#define op_prepare              32
#define op_reconnect            33
#define op_create_blob          34
#define op_open_blob            35
#define op_get_segment          36
#define op_put_segment          37
#define op_cancel_blob          38
#define op_close_blob           39
#define op_info_database        40
#define op_info_request         41
#define op_info_transaction     42
#define op_info_blob            43
#define op_batch_segments       44
#define op_execute              63
#define op_execute_immediate    64
#define op_fetch                65
#define op_fetch_response       66
#define op_free_statement       67
#define op_prepare_statement    68
#define op_allocate_statement   70
#define op_ping                 93
```

### Connection Protocol
```c
// Initial connection packet
typedef struct P_CNCT {
    P_OP_HEADER header;
    ULONG   p_cnct_operation;       // op_connect
    ULONG   p_cnct_cversion;        // Client version
    ULONG   p_cnct_client;          // Client architecture
    CSTRING p_cnct_file;            // Database path
    ULONG   p_cnct_user_id_length;  // User ID length
    UCHAR   p_cnct_user_id[n];      // User ID
    ULONG   p_cnct_protocols;       // Number of protocols
    struct {
        ULONG   p_cnct_protocol_version;
        ULONG   p_cnct_architecture;
        ULONG   p_cnct_min_type;
        ULONG   p_cnct_max_type;
        ULONG   p_cnct_weight;
    } protocols[p_cnct_protocols];
} P_CNCT;

// Authentication block
typedef struct P_AUTH_BLOCK {
    CSTRING plugin_name;        // "Srp256", "Srp", "Legacy_Auth"
    ULONG   plugin_data_length;
    UCHAR   plugin_data[n];     // Plugin-specific data
    CSTRING plugin_list;        // Available plugins
    ULONG   keys_length;
    UCHAR   keys[n];            // Wire crypt keys
} P_AUTH_BLOCK;
```

## Complete Data Type Implementations

### SMALLINT (SQL_SHORT)
```c
// Type code: 500 (SQL_SHORT)
// Size: 2 bytes
// Range: -32,768 to 32,767

// Encoding function
void encode_smallint(UCHAR* buffer, SSHORT value) {
    // Convert to network byte order
    buffer[0] = (value >> 8) & 0xFF;
    buffer[1] = value & 0xFF;
}

// Decoding function
SSHORT decode_smallint(const UCHAR* buffer) {
    return (SSHORT)((buffer[0] << 8) | buffer[1]);
}

// XSQLVAR structure
typedef struct {
    short   sqltype;     // 500 or 501 (nullable)
    short   sqlscale;    // 0 for integer
    short   sqlsubtype;  // 0
    short   sqllen;      // 2
    char*   sqldata;     // Pointer to 2-byte value
    short*  sqlind;      // NULL indicator
    short   sqlname_length;
    char    sqlname[32];
    short   relname_length;
    char    relname[32];
    short   ownname_length;
    char    ownname[32];
    short   aliasname_length;
    char    aliasname[32];
} XSQLVAR_SMALLINT;
```

### INTEGER (SQL_LONG)
```c
// Type code: 496 (SQL_LONG)
// Size: 4 bytes
// Range: -2,147,483,648 to 2,147,483,647

void encode_integer(UCHAR* buffer, SLONG value) {
    buffer[0] = (value >> 24) & 0xFF;
    buffer[1] = (value >> 16) & 0xFF;
    buffer[2] = (value >> 8) & 0xFF;
    buffer[3] = value & 0xFF;
}

SLONG decode_integer(const UCHAR* buffer) {
    return ((SLONG)buffer[0] << 24) |
           ((SLONG)buffer[1] << 16) |
           ((SLONG)buffer[2] << 8) |
           (SLONG)buffer[3];
}
```

### BIGINT (SQL_INT64)
```c
// Type code: 580 (SQL_INT64)
// Size: 8 bytes
// Range: -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807

void encode_bigint(UCHAR* buffer, ISC_INT64 value) {
    for (int i = 7; i >= 0; i--) {
        buffer[i] = value & 0xFF;
        value >>= 8;
    }
}

ISC_INT64 decode_bigint(const UCHAR* buffer) {
    ISC_INT64 value = 0;
    for (int i = 0; i < 8; i++) {
        value = (value << 8) | buffer[i];
    }
    return value;
}
```

### NUMERIC/DECIMAL
```c
// Type codes: 580 (SQL_INT64) with scale
// Storage: Scaled integer

typedef struct {
    short   sqltype;     // 580 or 581 (nullable)
    short   sqlscale;    // Negative scale (e.g., -2 for 2 decimal places)
    short   sqlsubtype;  // 1 for NUMERIC, 2 for DECIMAL
    short   sqllen;      // 8 for INT64 storage
    ISC_INT64* sqldata;  // Scaled value
    short*  sqlind;      // NULL indicator
} XSQLVAR_NUMERIC;

// Encoding
void encode_numeric(UCHAR* buffer, double value, short scale) {
    ISC_INT64 scaled = (ISC_INT64)(value * pow(10, -scale));
    encode_bigint(buffer, scaled);
}

// Decoding
double decode_numeric(const UCHAR* buffer, short scale) {
    ISC_INT64 scaled = decode_bigint(buffer);
    return scaled / pow(10, -scale);
}
```

### FLOAT
```c
// Type code: 482 (SQL_FLOAT)
// Size: 4 bytes
// IEEE 754 single precision

void encode_float(UCHAR* buffer, float value) {
    union {
        float f;
        ULONG l;
    } converter;
    converter.f = value;
    
    // Convert to network byte order
    buffer[0] = (converter.l >> 24) & 0xFF;
    buffer[1] = (converter.l >> 16) & 0xFF;
    buffer[2] = (converter.l >> 8) & 0xFF;
    buffer[3] = converter.l & 0xFF;
}

float decode_float(const UCHAR* buffer) {
    union {
        float f;
        ULONG l;
    } converter;
    
    converter.l = ((ULONG)buffer[0] << 24) |
                  ((ULONG)buffer[1] << 16) |
                  ((ULONG)buffer[2] << 8) |
                  (ULONG)buffer[3];
    return converter.f;
}
```

### DOUBLE PRECISION
```c
// Type code: 480 (SQL_DOUBLE)
// Size: 8 bytes
// IEEE 754 double precision

void encode_double(UCHAR* buffer, double value) {
    union {
        double d;
        ISC_UINT64 l;
    } converter;
    converter.d = value;
    
    for (int i = 7; i >= 0; i--) {
        buffer[i] = converter.l & 0xFF;
        converter.l >>= 8;
    }
}

double decode_double(const UCHAR* buffer) {
    union {
        double d;
        ISC_UINT64 l;
    } converter;
    
    converter.l = 0;
    for (int i = 0; i < 8; i++) {
        converter.l = (converter.l << 8) | buffer[i];
    }
    return converter.d;
}
```

### DECFLOAT(16)
```c
// Type code: 32764 (SQL_DEC16)
// Size: 8 bytes
// IEEE 754-2008 decimal64 (BID encoding)

typedef struct {
    UCHAR bytes[8];
} DECFLOAT16;

void encode_decfloat16(UCHAR* buffer, DECFLOAT16 value) {
    // BID (Binary Integer Decimal) format
    // Bit 63: Sign
    // Bits 62-53: Combination field (exponent + leading digit)
    // Bits 52-0: Trailing significand
    memcpy(buffer, value.bytes, 8);
}

DECFLOAT16 decode_decfloat16(const UCHAR* buffer) {
    DECFLOAT16 result;
    memcpy(result.bytes, buffer, 8);
    return result;
}
```

### DECFLOAT(34)
```c
// Type code: 32765 (SQL_DEC34)
// Size: 16 bytes
// IEEE 754-2008 decimal128 (BID encoding)

typedef struct {
    UCHAR bytes[16];
} DECFLOAT34;

void encode_decfloat34(UCHAR* buffer, DECFLOAT34 value) {
    // BID format for decimal128
    // Bit 127: Sign
    // Bits 126-113: Combination field
    // Bits 112-0: Trailing significand
    memcpy(buffer, value.bytes, 16);
}
```

### CHAR(n)
```c
// Type code: 452 (SQL_TEXT)
// Fixed-length, space-padded

typedef struct {
    short   sqltype;     // 452 or 453 (nullable)
    short   sqlscale;    // Character set ID
    short   sqlsubtype;  // Collation ID
    short   sqllen;      // Fixed length n
    char*   sqldata;     // Space-padded data
    short*  sqlind;      // NULL indicator
} XSQLVAR_CHAR;

void encode_char(UCHAR* buffer, const char* value, short length, short charset) {
    // Copy value
    size_t value_len = strlen(value);
    size_t copy_len = (value_len < length) ? value_len : length;
    memcpy(buffer, value, copy_len);
    
    // Pad with spaces
    if (copy_len < length) {
        memset(buffer + copy_len, ' ', length - copy_len);
    }
}

// Character set IDs
#define CS_NONE         0   // No character set
#define CS_BINARY       1   // Binary data
#define CS_ASCII        2   // ASCII
#define CS_UNICODE_FSS  3   // Unicode (deprecated)
#define CS_UTF8         4   // UTF-8
#define CS_ISO8859_1    21  // Latin-1
#define CS_ISO8859_2    22  // Latin-2
#define CS_WIN1250      51  // Windows Central European
#define CS_WIN1251      52  // Windows Cyrillic
#define CS_WIN1252      53  // Windows Latin-1
```

### VARCHAR(n)
```c
// Type code: 448 (SQL_VARYING)
// Variable-length with 2-byte length prefix

typedef struct {
    short   sqltype;     // 448 or 449 (nullable)
    short   sqlscale;    // Character set ID
    short   sqlsubtype;  // Collation ID
    short   sqllen;      // Maximum length n
    struct {
        short length;
        char data[1];    // Variable length
    }*      sqldata;
    short*  sqlind;      // NULL indicator
} XSQLVAR_VARCHAR;

void encode_varchar(UCHAR* buffer, const char* value, short max_length) {
    short actual_length = strlen(value);
    if (actual_length > max_length) {
        actual_length = max_length;
    }
    
    // Encode length (network byte order)
    buffer[0] = (actual_length >> 8) & 0xFF;
    buffer[1] = actual_length & 0xFF;
    
    // Copy data
    memcpy(buffer + 2, value, actual_length);
}

void decode_varchar(const UCHAR* buffer, char* output, short* length) {
    *length = (buffer[0] << 8) | buffer[1];
    memcpy(output, buffer + 2, *length);
    output[*length] = '\0';
}
```

### BLOB
```c
// Type code: 520 (SQL_BLOB)
// Blob ID: 8 bytes
// Data accessed via separate API

typedef struct {
    SLONG   blob_id_low;
    SLONG   blob_id_high;
} ISC_QUAD;

typedef struct {
    short   sqltype;     // 520 or 521 (nullable)
    short   sqlscale;    // Blob subtype
    short   sqlsubtype;  // 0=binary, 1=text, 2=BLR
    short   sqllen;      // 8 (size of ISC_QUAD)
    ISC_QUAD* sqldata;   // Blob ID
    short*  sqlind;      // NULL indicator
} XSQLVAR_BLOB;

// Blob operations
typedef struct {
    P_OP_HEADER header;
    ULONG   p_blob_transaction;  // Transaction handle
    ISC_QUAD p_blob_id;          // Blob ID
} P_OPEN_BLOB;

typedef struct {
    P_OP_HEADER header;
    ULONG   p_blob_handle;       // Blob handle
    USHORT  p_segment_length;   // Requested segment size
    USHORT  p_segment_pad;      // Padding
} P_GET_SEGMENT;

typedef struct {
    P_OP_HEADER header;
    ULONG   p_blob_handle;       // Blob handle
    USHORT  p_segment_length;   // Segment size
    USHORT  p_segment_pad;      // Padding
    UCHAR   p_segment_data[n];  // Segment data
} P_PUT_SEGMENT;

// Blob access functions
ISC_STATUS open_blob(ISC_QUAD* blob_id, isc_blob_handle* handle) {
    // Send op_open_blob packet
    // Receive op_response with handle
}

ISC_STATUS get_segment(isc_blob_handle handle, 
                       USHORT* length, 
                       UCHAR* buffer) {
    // Send op_get_segment packet
    // Receive op_response with data
}

ISC_STATUS create_blob(ISC_QUAD* blob_id, isc_blob_handle* handle) {
    // Send op_create_blob packet
    // Receive op_response with blob_id
}
```

### DATE
```c
// Type code: 510 (SQL_TYPE_DATE)
// Size: 4 bytes
// Days since November 17, 1858 (Modified Julian Date)

typedef SLONG ISC_DATE;

void encode_date(UCHAR* buffer, int year, int month, int day) {
    // Calculate Modified Julian Date
    int a = (14 - month) / 12;
    int y = year + 4800 - a;
    int m = month + 12 * a - 3;
    
    ISC_DATE mjd = day + (153 * m + 2) / 5 + 365 * y + 
                   y / 4 - y / 100 + y / 400 - 32045 - 2400001;
    
    encode_integer(buffer, mjd);
}

void decode_date(const UCHAR* buffer, int* year, int* month, int* day) {
    ISC_DATE mjd = decode_integer(buffer);
    
    // Convert MJD to calendar date
    int jd = mjd + 2400001;
    int a = jd + 32044;
    int b = (4 * a + 3) / 146097;
    int c = a - (146097 * b) / 4;
    int d = (4 * c + 3) / 1461;
    int e = c - (1461 * d) / 4;
    int m = (5 * e + 2) / 153;
    
    *day = e - (153 * m + 2) / 5 + 1;
    *month = m + 3 - 12 * (m / 10);
    *year = 100 * b + d - 4800 + m / 10;
}
```

### TIME
```c
// Type code: 560 (SQL_TYPE_TIME)
// Size: 4 bytes
// Ten-thousandths of seconds since midnight

typedef ULONG ISC_TIME;

void encode_time(UCHAR* buffer, int hour, int minute, int second, int fractions) {
    ISC_TIME time = hour * 36000000 + minute * 600000 + 
                    second * 10000 + fractions;
    
    buffer[0] = (time >> 24) & 0xFF;
    buffer[1] = (time >> 16) & 0xFF;
    buffer[2] = (time >> 8) & 0xFF;
    buffer[3] = time & 0xFF;
}

void decode_time(const UCHAR* buffer, int* hour, int* minute, 
                 int* second, int* fractions) {
    ISC_TIME time = ((ULONG)buffer[0] << 24) |
                    ((ULONG)buffer[1] << 16) |
                    ((ULONG)buffer[2] << 8) |
                    (ULONG)buffer[3];
    
    *hour = time / 36000000;
    time %= 36000000;
    *minute = time / 600000;
    time %= 600000;
    *second = time / 10000;
    *fractions = time % 10000;
}
```

### TIMESTAMP
```c
// Type code: 510 (SQL_TIMESTAMP)
// Size: 8 bytes
// Combination of DATE and TIME

typedef struct {
    ISC_DATE date;
    ISC_TIME time;
} ISC_TIMESTAMP;

void encode_timestamp(UCHAR* buffer, ISC_TIMESTAMP* ts) {
    encode_integer(buffer, ts->date);
    encode_integer(buffer + 4, ts->time);
}

void decode_timestamp(const UCHAR* buffer, ISC_TIMESTAMP* ts) {
    ts->date = decode_integer(buffer);
    ts->time = decode_integer(buffer + 4);
}
```

### TIME WITH TIME ZONE
```c
// Type code: 32756 (SQL_TIME_TZ)
// Size: 8 bytes

typedef struct {
    ISC_TIME time;       // 4 bytes: time in UTC
    USHORT   zone_id;    // 2 bytes: time zone ID
    SHORT    zone_offset; // 2 bytes: offset in minutes
} ISC_TIME_TZ;

void encode_time_tz(UCHAR* buffer, ISC_TIME_TZ* time_tz) {
    encode_integer(buffer, time_tz->time);
    buffer[4] = (time_tz->zone_id >> 8) & 0xFF;
    buffer[5] = time_tz->zone_id & 0xFF;
    buffer[6] = (time_tz->zone_offset >> 8) & 0xFF;
    buffer[7] = time_tz->zone_offset & 0xFF;
}
```

### TIMESTAMP WITH TIME ZONE
```c
// Type code: 32754 (SQL_TIMESTAMP_TZ)
// Size: 12 bytes

typedef struct {
    ISC_TIMESTAMP timestamp;  // 8 bytes
    USHORT        zone_id;    // 2 bytes
    SHORT         zone_offset; // 2 bytes
} ISC_TIMESTAMP_TZ;

void encode_timestamp_tz(UCHAR* buffer, ISC_TIMESTAMP_TZ* ts_tz) {
    encode_timestamp(buffer, &ts_tz->timestamp);
    buffer[8] = (ts_tz->zone_id >> 8) & 0xFF;
    buffer[9] = ts_tz->zone_id & 0xFF;
    buffer[10] = (ts_tz->zone_offset >> 8) & 0xFF;
    buffer[11] = ts_tz->zone_offset & 0xFF;
}
```

### BOOLEAN
```c
// Type code: 32764 (SQL_BOOLEAN)
// Size: 1 byte
// Values: 0 (false), 1 (true), NULL

typedef SCHAR ISC_BOOLEAN;

void encode_boolean(UCHAR* buffer, ISC_BOOLEAN value) {
    buffer[0] = value;
}

ISC_BOOLEAN decode_boolean(const UCHAR* buffer) {
    return buffer[0];
}

// XSQLVAR for BOOLEAN
typedef struct {
    short   sqltype;     // 32764 or 32765 (nullable)
    short   sqlscale;    // 0
    short   sqlsubtype;  // 0
    short   sqllen;      // 1
    ISC_BOOLEAN* sqldata;
    short*  sqlind;      // NULL indicator
} XSQLVAR_BOOLEAN;
```

### RDB$DB_KEY
```c
// Type code: 536 (SQL_DB_KEY)
// Size: 8 bytes
// Physical row identifier

typedef struct {
    UCHAR bytes[8];
} RDB_DB_KEY;

void encode_db_key(UCHAR* buffer, RDB_DB_KEY* key) {
    memcpy(buffer, key->bytes, 8);
}

void decode_db_key(const UCHAR* buffer, RDB_DB_KEY* key) {
    memcpy(key->bytes, buffer, 8);
}
```

## Statement Execution Protocol

### Prepare Statement
```c
typedef struct {
    P_OP_HEADER header;
    ULONG   p_statement_handle;    // Statement handle
    ULONG   p_transaction_handle;  // Transaction handle
    USHORT  p_statement_length;    // SQL statement length
    UCHAR   p_statement[n];        // SQL statement
    USHORT  p_dialect;             // SQL dialect (1, 2, or 3)
    USHORT  p_buffer_length;       // Info buffer length
} P_PREPARE_STATEMENT;

// Response includes XSQLDA structure
typedef struct {
    short   version;        // SQLDA version (1 or 2)
    char    sqldaid[8];    // "SQLDA   " or "SQLDA2  "
    SLONG   sqldabc;       // Length of SQLDA
    short   sqln;          // Number of allocated XSQLVAR
    short   sqld;          // Actual number of XSQLVAR
    XSQLVAR sqlvar[1];     // Array of XSQLVAR structures
} XSQLDA;
```

### Execute Statement
```c
typedef struct {
    P_OP_HEADER header;
    ULONG   p_statement_handle;
    ULONG   p_transaction_handle;
    USHORT  p_message_number;      // Message type (0=input, 1=output)
    USHORT  p_message_length;      // Parameter buffer length
    UCHAR   p_parameters[n];       // Parameter data
} P_EXECUTE;

// Parameter buffer format
typedef struct {
    UCHAR   null_indicators[(param_count + 7) / 8];  // Null bitmap
    // Followed by non-null parameter values in order
    // Each value encoded according to its type
} PARAMETER_BUFFER;
```

### Fetch Results
```c
typedef struct {
    P_OP_HEADER header;
    ULONG   p_statement_handle;
    USHORT  p_message_number;      // Output message number
    USHORT  p_fetch_count;         // Number of rows to fetch
} P_FETCH;

typedef struct {
    P_OP_HEADER header;
    ULONG   p_status;              // Fetch status
    USHORT  p_row_count;           // Number of rows returned
    UCHAR   p_row_data[n];         // Row data
} P_FETCH_RESPONSE;

// Row data format
typedef struct {
    UCHAR   null_indicators[(column_count + 7) / 8];  // Null bitmap
    // Followed by non-null column values
    // Each value encoded according to its type
} ROW_DATA;
```

## Transaction Protocol

### Start Transaction
```c
typedef struct {
    P_OP_HEADER header;
    ULONG   p_database_handle;
    USHORT  p_tpb_length;          // Transaction Parameter Block length
    UCHAR   p_tpb[n];              // TPB data
} P_TRANSACTION;

// Transaction Parameter Block (TPB)
#define isc_tpb_version1         1
#define isc_tpb_version3         3
#define isc_tpb_consistency      1   // Consistency isolation
#define isc_tpb_concurrency      2   // Snapshot isolation
#define isc_tpb_read_committed   8   // Read committed
#define isc_tpb_wait            6    // Wait on lock conflict
#define isc_tpb_nowait          7    // Error on lock conflict
#define isc_tpb_read            8    // Read-only transaction
#define isc_tpb_write           9    // Read-write transaction
#define isc_tpb_lock_read       14   // Lock for read
#define isc_tpb_lock_write      15   // Lock for write
#define isc_tpb_rec_version     17   // Record version
#define isc_tpb_no_rec_version  18   // Latest version only

// Example TPB construction
UCHAR tpb[] = {
    isc_tpb_version3,
    isc_tpb_write,
    isc_tpb_read_committed,
    isc_tpb_rec_version,
    isc_tpb_wait
};
```

### Commit/Rollback
```c
typedef struct {
    P_OP_HEADER header;
    ULONG   p_transaction_handle;
} P_COMMIT, P_ROLLBACK;

typedef struct {
    P_OP_HEADER header;
    ULONG   p_transaction_handle;
    USHORT  p_message_length;      // Message buffer length
    UCHAR   p_message[n];          // Prepare message
} P_PREPARE;
```

## Error Handling

### Status Vector
```c
typedef ISC_STATUS ISC_STATUS_ARRAY[20];

// Status vector structure
// [0] - isc_arg_gds or isc_arg_end
// [1] - Error code
// [2] - isc_arg_* (argument type)
// [3] - Argument value
// ... repeating pairs
// [n] - isc_arg_end

#define isc_arg_end          0  // End of arguments
#define isc_arg_gds          1  // Firebird error code
#define isc_arg_string       2  // String argument
#define isc_arg_cstring      3  // C string argument
#define isc_arg_number       4  // Numeric argument
#define isc_arg_interpreted  5  // Interpreted status code
#define isc_arg_vms          6  // VMS error code
#define isc_arg_unix         7  // Unix error code
#define isc_arg_domain       8  // Domain error
#define isc_arg_dos          9  // DOS error code
#define isc_arg_mpexl        10 // MPE/XL error code
#define isc_arg_sql_state    19 // SQL STATE

// Common error codes
#define isc_sqlerr           -303  // SQL error
#define isc_deadlock         335544336  // Deadlock
#define isc_lock_conflict    335544345  // Lock conflict
#define isc_no_dup          335544349  // Duplicate key
#define isc_foreign_key     335544466  // Foreign key violation
#define isc_not_null_col    335544347  // NOT NULL violation
```

---

# Microsoft SQL Server TDS Protocol Specification

## TDS (Tabular Data Stream) Protocol Overview

- **Default Port**: 1433
- **Byte Order**: Little-Endian
- **Protocol Versions**: TDS 7.0, 7.1, 7.2, 7.3, 7.4, 8.0
- **Packet Size**: Default 4096, negotiable up to 32767

## TDS Packet Structure

### Packet Header
```c
typedef struct {
    BYTE    type;       // Packet type
    BYTE    status;     // Status flags
    USHORT  length;     // Packet length (big-endian!)
    USHORT  spid;       // Server process ID
    BYTE    packet_id;  // Packet number in message
    BYTE    window;     // Currently unused (0)
} TDS_PACKET_HEADER;

// Packet types
#define TDS_SQL_BATCH       1   // SQL batch
#define TDS_PRE_LOGIN       2   // Pre-login
#define TDS_RPC            3   // Remote procedure call
#define TDS_TABULAR_RESULT 4   // Tabular result
#define TDS_ATTENTION      6   // Attention signal
#define TDS_BULK_LOAD      7   // Bulk load data
#define TDS_TRANS_MGR      14  // Transaction manager request
#define TDS_LOGIN7         16  // Login
#define TDS_SSPI           17  // SSPI token
#define TDS_PRELOGIN       18  // Pre-login

// Status flags
#define TDS_STATUS_NORMAL           0x00
#define TDS_STATUS_EOM             0x01  // End of message
#define TDS_STATUS_IGNORE          0x02  // Ignore this event
#define TDS_STATUS_RESET_CONN      0x08  // Reset connection
#define TDS_STATUS_RESET_CONN_SKIP 0x10  // Reset connection keeping state
```

### Pre-Login Packet
```c
typedef struct {
    BYTE    token;      // Option token
    USHORT  offset;     // Offset to data
    USHORT  length;     // Length of data
} PRELOGIN_OPTION;

// Pre-login option tokens
#define PL_OPTION_VERSION       0x00
#define PL_OPTION_ENCRYPTION    0x01
#define PL_OPTION_INSTOPT       0x02
#define PL_OPTION_THREADID      0x03
#define PL_OPTION_MARS          0x04
#define PL_OPTION_TRACEID       0x05
#define PL_OPTION_FEDAUTHREQUIRED 0x06
#define PL_OPTION_NONCEOPT      0x07
#define PL_OPTION_TERMINATOR    0xFF

// Version structure
typedef struct {
    BYTE    major;
    BYTE    minor;
    USHORT  build;
    USHORT  sub_build;
} TDS_VERSION;
```

### Login7 Packet
```c
typedef struct {
    DWORD   length;         // Total length
    DWORD   tds_version;    // TDS version
    DWORD   packet_size;    // Packet size
    DWORD   client_prog_ver;// Client program version
    DWORD   client_pid;     // Client process ID
    DWORD   connection_id;  // Connection ID
    BYTE    option_flags1;  // Option flags 1
    BYTE    option_flags2;  // Option flags 2
    BYTE    type_flags;     // SQL type flags
    BYTE    option_flags3;  // Option flags 3
    DWORD   client_timezone;// Client time zone
    DWORD   client_lcid;    // Client LCID
    
    // Variable length data offsets and lengths
    USHORT  hostname_offset;
    USHORT  hostname_length;
    USHORT  username_offset;
    USHORT  username_length;
    USHORT  password_offset;
    USHORT  password_length;
    USHORT  appname_offset;
    USHORT  appname_length;
    USHORT  servername_offset;
    USHORT  servername_length;
    // ... more fields
} LOGIN7_HEADER;

// Password encryption
void encrypt_password(WCHAR* password, int length) {
    for (int i = 0; i < length; i++) {
        WCHAR ch = password[i];
        ch = ((ch & 0x0F) << 4) | ((ch & 0xF0) >> 4);
        ch ^= 0xA5;
        password[i] = ch;
    }
}
```

## Complete TDS Data Type Implementations

### BIT
```c
// Type: TYPE_BIT (0x32)
// Size: 1 bit (packed into bytes)

typedef struct {
    BYTE    type;       // 0x32
    // No length or scale info for BIT
} TDS_BIT_META;

// In row data
void encode_bit(BYTE* buffer, BOOL value) {
    buffer[0] = value ? 1 : 0;
}

BOOL decode_bit(BYTE* buffer) {
    return buffer[0] != 0;
}
```

### TINYINT
```c
// Type: TYPE_INT1 (0x30)
// Size: 1 byte

typedef struct {
    BYTE    type;       // 0x30
} TDS_TINYINT_META;

void encode_tinyint(BYTE* buffer, BYTE value) {
    buffer[0] = value;
}

BYTE decode_tinyint(BYTE* buffer) {
    return buffer[0];
}
```

### SMALLINT
```c
// Type: TYPE_INT2 (0x34)
// Size: 2 bytes

typedef struct {
    BYTE    type;       // 0x34
} TDS_SMALLINT_META;

void encode_smallint(BYTE* buffer, SHORT value) {
    buffer[0] = value & 0xFF;
    buffer[1] = (value >> 8) & 0xFF;
}

SHORT decode_smallint(BYTE* buffer) {
    return buffer[0] | (buffer[1] << 8);
}
```

### INT
```c
// Type: TYPE_INT4 (0x38)
// Size: 4 bytes

typedef struct {
    BYTE    type;       // 0x38
} TDS_INT_META;

void encode_int(BYTE* buffer, LONG value) {
    buffer[0] = value & 0xFF;
    buffer[1] = (value >> 8) & 0xFF;
    buffer[2] = (value >> 16) & 0xFF;
    buffer[3] = (value >> 24) & 0xFF;
}

LONG decode_int(BYTE* buffer) {
    return buffer[0] | 
           (buffer[1] << 8) |
           (buffer[2] << 16) |
           (buffer[3] << 24);
}
```

### BIGINT
```c
// Type: TYPE_INT8 (0x7F)
// Size: 8 bytes

typedef struct {
    BYTE    type;       // 0x7F
} TDS_BIGINT_META;

void encode_bigint(BYTE* buffer, LONGLONG value) {
    for (int i = 0; i < 8; i++) {
        buffer[i] = (value >> (i * 8)) & 0xFF;
    }
}

LONGLONG decode_bigint(BYTE* buffer) {
    LONGLONG value = 0;
    for (int i = 0; i < 8; i++) {
        value |= ((LONGLONG)buffer[i]) << (i * 8);
    }
    return value;
}
```

### DECIMAL/NUMERIC
```c
// Type: TYPE_NUMERIC (0x6C) / TYPE_DECIMAL (0x6A)
// Size: 5-17 bytes depending on precision

typedef struct {
    BYTE    type;       // 0x6C or 0x6A
    BYTE    length;     // Total length (5-17)
    BYTE    precision;  // 1-38
    BYTE    scale;      // 0-precision
} TDS_NUMERIC_META;

typedef struct {
    BYTE    sign;       // 1=positive, 0=negative
    BYTE    data[16];   // Little-endian 128-bit integer
} TDS_NUMERIC_DATA;

void encode_numeric(BYTE* buffer, DECIMAL* dec, BYTE precision, BYTE scale) {
    buffer[0] = dec->sign;
    
    // Convert decimal to scaled 128-bit integer
    // Store in little-endian format
    memcpy(buffer + 1, dec->data, (precision <= 9) ? 4 :
                                  (precision <= 19) ? 8 :
                                  (precision <= 28) ? 12 : 16);
}
```

### MONEY
```c
// Type: TYPE_MONEY (0x3C)
// Size: 8 bytes

typedef struct {
    BYTE    type;       // 0x3C
} TDS_MONEY_META;

typedef struct {
    LONG    high;       // High 32 bits
    ULONG   low;        // Low 32 bits
} TDS_MONEY_DATA;

void encode_money(BYTE* buffer, LONGLONG value) {
    // Value is in 10,000ths of currency unit
    LONG high = (LONG)(value >> 32);
    ULONG low = (ULONG)(value & 0xFFFFFFFF);
    
    // Little-endian encoding
    encode_int(buffer, high);
    encode_int(buffer + 4, low);
}

LONGLONG decode_money(BYTE* buffer) {
    LONG high = decode_int(buffer);
    ULONG low = decode_int(buffer + 4);
    return ((LONGLONG)high << 32) | low;
}
```

### SMALLMONEY
```c
// Type: TYPE_MONEY4 (0x7A)
// Size: 4 bytes

typedef struct {
    BYTE    type;       // 0x7A
} TDS_SMALLMONEY_META;

void encode_smallmoney(BYTE* buffer, LONG value) {
    // Value is in 10,000ths of currency unit
    encode_int(buffer, value);
}
```

### FLOAT
```c
// Type: TYPE_FLT4 (0x3B)
// Size: 4 bytes

typedef struct {
    BYTE    type;       // 0x3B
} TDS_FLOAT_META;

void encode_float(BYTE* buffer, float value) {
    union {
        float f;
        ULONG l;
    } converter;
    converter.f = value;
    encode_int(buffer, converter.l);
}

float decode_float(BYTE* buffer) {
    union {
        float f;
        ULONG l;
    } converter;
    converter.l = decode_int(buffer);
    return converter.f;
}
```

### REAL
```c
// Type: TYPE_FLT4 (0x3B) - same as FLOAT
// Size: 4 bytes
// Implementation identical to FLOAT
```

### DOUBLE (FLOAT(53))
```c
// Type: TYPE_FLT8 (0x3E)
// Size: 8 bytes

typedef struct {
    BYTE    type;       // 0x3E
} TDS_DOUBLE_META;

void encode_double(BYTE* buffer, double value) {
    union {
        double d;
        ULONGLONG l;
    } converter;
    converter.d = value;
    encode_bigint(buffer, converter.l);
}

double decode_double(BYTE* buffer) {
    union {
        double d;
        ULONGLONG l;
    } converter;
    converter.l = decode_bigint(buffer);
    return converter.d;
}
```

### CHAR
```c
// Type: TYPE_CHAR (0x2F)
// Fixed-length character data

typedef struct {
    BYTE    type;       // 0x2F
    USHORT  length;     // Fixed length
    COLLATION collation;// 5 bytes: LCID + flags
} TDS_CHAR_META;

typedef struct {
    DWORD   lcid;       // Locale ID
    BYTE    flags;      // Comparison flags
} COLLATION;

void encode_char(BYTE* buffer, const char* value, USHORT length) {
    memcpy(buffer, value, length);
    // Pad with spaces if necessary
    size_t value_len = strlen(value);
    if (value_len < length) {
        memset(buffer + value_len, ' ', length - value_len);
    }
}
```

### VARCHAR
```c
// Type: TYPE_VARCHAR (0x27)
// Variable-length character data

typedef struct {
    BYTE    type;       // 0x27
    USHORT  max_length; // Maximum length
    COLLATION collation;// 5 bytes
} TDS_VARCHAR_META;

// In row data
void encode_varchar(BYTE* buffer, const char* value, USHORT* written) {
    USHORT length = strlen(value);
    buffer[0] = length & 0xFF;
    buffer[1] = (length >> 8) & 0xFF;
    memcpy(buffer + 2, value, length);
    *written = length + 2;
}

void decode_varchar(BYTE* buffer, char* output, USHORT* length) {
    *length = buffer[0] | (buffer[1] << 8);
    memcpy(output, buffer + 2, *length);
    output[*length] = '\0';
}
```

### VARCHAR(MAX)
```c
// Type: TYPE_BIGVARCHAR (0xA7)
// Large variable-length character data

typedef struct {
    BYTE    type;       // 0xA7
    USHORT  max_length; // 0xFFFF for MAX
    COLLATION collation;
} TDS_VARCHAR_MAX_META;

// Partially Length Prefixed (PLP) format
typedef struct {
    ULONGLONG total_length;  // 0xFFFFFFFFFFFFFFFE for NULL
                            // 0xFFFFFFFFFFFFFFFF for unknown
} PLP_HEADER;

void encode_varchar_max(BYTE* buffer, const char* value, size_t length) {
    ULONGLONG* header = (ULONGLONG*)buffer;
    *header = length;
    buffer += 8;
    
    // Write chunks
    while (length > 0) {
        ULONG chunk_size = (length > 0x8000) ? 0x8000 : length;
        *(ULONG*)buffer = chunk_size;
        buffer += 4;
        memcpy(buffer, value, chunk_size);
        buffer += chunk_size;
        value += chunk_size;
        length -= chunk_size;
    }
    
    // Terminator
    *(ULONG*)buffer = 0;
}
```

### NCHAR
```c
// Type: TYPE_NCHAR (0xEF)
// Fixed-length Unicode (UTF-16LE)

typedef struct {
    BYTE    type;       // 0xEF
    USHORT  length;     // Length in bytes (characters * 2)
    COLLATION collation;
} TDS_NCHAR_META;

void encode_nchar(BYTE* buffer, const WCHAR* value, USHORT char_count) {
    USHORT byte_count = char_count * 2;
    memcpy(buffer, value, byte_count);
    // Pad with space (0x0020) if necessary
    size_t value_len = wcslen(value);
    if (value_len < char_count) {
        WCHAR* pad_start = (WCHAR*)(buffer + value_len * 2);
        for (size_t i = value_len; i < char_count; i++) {
            *pad_start++ = 0x0020;
        }
    }
}
```

### NVARCHAR
```c
// Type: TYPE_NVARCHAR (0xE7)
// Variable-length Unicode (UTF-16LE)

typedef struct {
    BYTE    type;       // 0xE7
    USHORT  max_length; // Maximum length in bytes
    COLLATION collation;
} TDS_NVARCHAR_META;

void encode_nvarchar(BYTE* buffer, const WCHAR* value, USHORT* written) {
    USHORT char_count = wcslen(value);
    USHORT byte_count = char_count * 2;
    
    buffer[0] = byte_count & 0xFF;
    buffer[1] = (byte_count >> 8) & 0xFF;
    memcpy(buffer + 2, value, byte_count);
    *written = byte_count + 2;
}
```

### NVARCHAR(MAX)
```c
// Type: TYPE_NVARCHAR (0xE7)
// Large Unicode text
// Uses PLP format like VARCHAR(MAX)
```

### TEXT (deprecated)
```c
// Type: TYPE_TEXT (0x23)
// Legacy large text type

typedef struct {
    BYTE    type;       // 0x23
    ULONG   length;     // Maximum length
    COLLATION collation;
} TDS_TEXT_META;

typedef struct {
    BYTE    textptr[16];    // Text pointer
    BYTE    timestamp[8];   // Timestamp
    ULONG   length;         // Actual length
    // Followed by data
} TDS_TEXT_DATA;
```

### BINARY
```c
// Type: TYPE_BINARY (0x2D)
// Fixed-length binary

typedef struct {
    BYTE    type;       // 0x2D
    USHORT  length;     // Fixed length
} TDS_BINARY_META;

void encode_binary(BYTE* buffer, const BYTE* data, USHORT length) {
    memcpy(buffer, data, length);
}
```

### VARBINARY
```c
// Type: TYPE_VARBINARY (0x25)
// Variable-length binary

typedef struct {
    BYTE    type;       // 0x25
    USHORT  max_length; // Maximum length
} TDS_VARBINARY_META;

void encode_varbinary(BYTE* buffer, const BYTE* data, USHORT length) {
    buffer[0] = length & 0xFF;
    buffer[1] = (length >> 8) & 0xFF;
    memcpy(buffer + 2, data, length);
}
```

### VARBINARY(MAX)
```c
// Type: TYPE_BIGVARBINARY (0xA5)
// Large binary data
// Uses PLP format
```

### IMAGE (deprecated)
```c
// Type: TYPE_IMAGE (0x22)
// Legacy large binary type

typedef struct {
    BYTE    type;       // 0x22
    ULONG   length;     // Maximum length
} TDS_IMAGE_META;

// Same data format as TEXT
```

### DATE
```c
// Type: TYPE_DATE (0x28)
// Size: 3 bytes
// Days since 0001-01-01

typedef struct {
    BYTE    type;       // 0x28
} TDS_DATE_META;

void encode_date(BYTE* buffer, int year, int month, int day) {
    // Calculate days since 0001-01-01
    int days = calculate_days_since_0001(year, month, day);
    
    buffer[0] = days & 0xFF;
    buffer[1] = (days >> 8) & 0xFF;
    buffer[2] = (days >> 16) & 0xFF;
}

void decode_date(BYTE* buffer, int* year, int* month, int* day) {
    int days = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16);
    calculate_date_from_days(days, year, month, day);
}
```

### TIME
```c
// Type: TYPE_TIME (0x29)
// Size: 3-5 bytes depending on scale

typedef struct {
    BYTE    type;       // 0x29
    BYTE    scale;      // 0-7 (fractional seconds precision)
} TDS_TIME_META;

void encode_time(BYTE* buffer, int hour, int minute, int second, 
                 int fraction, BYTE scale) {
    ULONGLONG ticks = hour * 3600LL;
    ticks = (ticks + minute * 60 + second) * pow(10, scale) + fraction;
    
    // Variable length encoding based on scale
    int bytes = (scale <= 2) ? 3 : (scale <= 4) ? 4 : 5;
    for (int i = 0; i < bytes; i++) {
        buffer[i] = (ticks >> (i * 8)) & 0xFF;
    }
}
```

### DATETIME
```c
// Type: TYPE_DATETIME (0x3D)
// Size: 8 bytes

typedef struct {
    BYTE    type;       // 0x3D
} TDS_DATETIME_META;

typedef struct {
    LONG    days;       // Days since 1900-01-01
    ULONG   ticks;      // 300ths of a second since midnight
} TDS_DATETIME_DATA;

void encode_datetime(BYTE* buffer, SYSTEMTIME* st) {
    LONG days = calculate_days_since_1900(st->wYear, st->wMonth, st->wDay);
    ULONG ticks = (st->wHour * 3600 + st->wMinute * 60 + st->wSecond) * 300 +
                  st->wMilliseconds * 300 / 1000;
    
    encode_int(buffer, days);
    encode_int(buffer + 4, ticks);
}
```

### DATETIME2
```c
// Type: TYPE_DATETIME2 (0x2A)
// Size: 6-8 bytes depending on scale

typedef struct {
    BYTE    type;       // 0x2A
    BYTE    scale;      // 0-7
} TDS_DATETIME2_META;

void encode_datetime2(BYTE* buffer, SYSTEMTIME* st, BYTE scale) {
    // Time component (3-5 bytes based on scale)
    encode_time(buffer, st->wHour, st->wMinute, st->wSecond,
                st->wMilliseconds * pow(10, scale - 3), scale);
    
    // Date component (3 bytes)
    int time_bytes = (scale <= 2) ? 3 : (scale <= 4) ? 4 : 5;
    encode_date(buffer + time_bytes, st->wYear, st->wMonth, st->wDay);
}
```

### SMALLDATETIME
```c
// Type: TYPE_DATETIME4 (0x3A)
// Size: 4 bytes

typedef struct {
    BYTE    type;       // 0x3A
} TDS_SMALLDATETIME_META;

typedef struct {
    USHORT  days;       // Days since 1900-01-01
    USHORT  minutes;    // Minutes since midnight
} TDS_SMALLDATETIME_DATA;

void encode_smalldatetime(BYTE* buffer, SYSTEMTIME* st) {
    USHORT days = calculate_days_since_1900(st->wYear, st->wMonth, st->wDay);
    USHORT minutes = st->wHour * 60 + st->wMinute;
    
    buffer[0] = days & 0xFF;
    buffer[1] = (days >> 8) & 0xFF;
    buffer[2] = minutes & 0xFF;
    buffer[3] = (minutes >> 8) & 0xFF;
}
```

### DATETIMEOFFSET
```c
// Type: TYPE_DATETIMEOFFSET (0x2B)
// Size: 8-10 bytes

typedef struct {
    BYTE    type;       // 0x2B
    BYTE    scale;      // 0-7
} TDS_DATETIMEOFFSET_META;

void encode_datetimeoffset(BYTE* buffer, SYSTEMTIME* st, 
                           SHORT timezone_offset, BYTE scale) {
    // DATETIME2 component
    encode_datetime2(buffer, st, scale);
    
    // Timezone offset in minutes (2 bytes)
    int dt2_bytes = (scale <= 2) ? 6 : (scale <= 4) ? 7 : 8;
    buffer[dt2_bytes] = timezone_offset & 0xFF;
    buffer[dt2_bytes + 1] = (timezone_offset >> 8) & 0xFF;
}
```

### UNIQUEIDENTIFIER
```c
// Type: TYPE_GUID (0x24)
// Size: 16 bytes

typedef struct {
    BYTE    type;       // 0x24
    BYTE    length;     // Always 16
} TDS_GUID_META;

typedef struct {
    DWORD   Data1;
    WORD    Data2;
    WORD    Data3;
    BYTE    Data4[8];
} GUID;

void encode_guid(BYTE* buffer, GUID* guid) {
    // Special byte order for SQL Server
    encode_int(buffer, guid->Data1);
    buffer[4] = guid->Data2 & 0xFF;
    buffer[5] = (guid->Data2 >> 8) & 0xFF;
    buffer[6] = guid->Data3 & 0xFF;
    buffer[7] = (guid->Data3 >> 8) & 0xFF;
    memcpy(buffer + 8, guid->Data4, 8);
}
```

### SQL_VARIANT
```c
// Type: TYPE_VARIANT (0x62)
// Can hold various types

typedef struct {
    BYTE    type;       // 0x62
    ULONG   max_length; // Always 8016
} TDS_VARIANT_META;

typedef struct {
    ULONG   total_length;   // Total length including this header
    BYTE    base_type;      // Actual type of data
    BYTE    properties[7];  // Type-specific properties
    // Followed by actual data
} TDS_VARIANT_DATA;
```

### XML
```c
// Type: TYPE_XML (0xF1)
// XML data

typedef struct {
    BYTE    type;       // 0xF1
    BYTE    schema_present;  // Has schema collection
    // If schema_present:
    BYTE    dbname_length;
    WCHAR   dbname[dbname_length];
    BYTE    schema_length;
    WCHAR   schema[schema_length];
    BYTE    collection_length;
    WCHAR   collection[collection_length];
} TDS_XML_META;

// Data uses PLP format
```

### HIERARCHYID
```c
// Type: TYPE_UDT (0xF0)
// CLR User-Defined Type

typedef struct {
    BYTE    type;       // 0xF0
    USHORT  max_length;
    BYTE    dbname_length;
    WCHAR   dbname[dbname_length];
    BYTE    schema_length;
    WCHAR   schema[schema_length];
    BYTE    type_length;
    WCHAR   type_name[type_length];
} TDS_UDT_META;

// HIERARCHYID encoding
typedef struct {
    USHORT  length;
    BYTE    data[length];  // Encoded path
} HIERARCHYID_DATA;
```

### GEOGRAPHY/GEOMETRY
```c
// Type: TYPE_UDT (0xF0)
// Spatial types use CLR UDT

// Well-Known Binary (WKB) format
typedef struct {
    BYTE    byte_order;     // 1 = little-endian
    ULONG   wkb_type;       // Geometry type
    // Type-specific data follows
} WKB_HEADER;

// POINT
typedef struct {
    WKB_HEADER header;      // wkb_type = 1
    double  x;
    double  y;
} WKB_POINT;

// LINESTRING
typedef struct {
    WKB_HEADER header;      // wkb_type = 2
    ULONG   num_points;
    WKB_POINT points[num_points];
} WKB_LINESTRING;
```

## TDS Tokens and Result Processing

### Token Types
```c
// Token identifiers
#define TOKEN_COLMETADATA   0x81  // Column metadata
#define TOKEN_ALTMETADATA   0x88  // Alternative metadata
#define TOKEN_DATACLASSIFICATION 0xA3  // Data classification
#define TOKEN_ROW           0xD1  // Row data
#define TOKEN_NBCROW        0xD2  // Null bitmap compression row
#define TOKEN_ALTROW        0xD3  // Alternative row
#define TOKEN_ENVCHANGE     0xE3  // Environment change
#define TOKEN_ERROR         0xAA  // Error message
#define TOKEN_INFO          0xAB  // Info message
#define TOKEN_RETURNSTATUS  0x79  // Return status
#define TOKEN_RETURNVALUE   0xAC  // Return value
#define TOKEN_LOGINACK      0xAD  // Login acknowledgment
#define TOKEN_FEATUREEXTACK 0xAE  // Feature extension acknowledgment
#define TOKEN_DONE          0xFD  // Done
#define TOKEN_DONEPROC      0xFE  // Done procedure
#define TOKEN_DONEINPROC    0xFF  // Done in procedure
```

### Column Metadata Token
```c
typedef struct {
    BYTE    token;          // 0x81
    USHORT  column_count;
    // For each column:
    struct {
        ULONG   user_type;  // User-defined type ID
        USHORT  flags;      // Column flags
        TYPE_INFO type_info;// Type-specific metadata
        BYTE    name_length;
        WCHAR   name[name_length];
    } columns[column_count];
} TOKEN_COLMETADATA;

// Column flags
#define COLUMN_NULLABLE     0x0001
#define COLUMN_CASESENSITIVE 0x0002
#define COLUMN_UPDATEABLE   0x0004
#define COLUMN_IDENTITY     0x0010
#define COLUMN_COMPUTED     0x0020
#define COLUMN_FIXED_LENGTH 0x0040
#define COLUMN_SPARSE       0x0100
#define COLUMN_ENCRYPTED    0x0200
#define COLUMN_HIDDEN       0x0400
```

### Row Token
```c
typedef struct {
    BYTE    token;          // 0xD1
    // For each column:
    // - NULL: 0x00 for NULLBIT types
    // - Non-NULL: Type-specific encoding
} TOKEN_ROW;

// NBC (Null Bitmap Compression) Row
typedef struct {
    BYTE    token;          // 0xD2
    BYTE    null_bitmap[(column_count + 7) / 8];
    // Followed by non-null column values
} TOKEN_NBCROW;
```

### Done Token
```c
typedef struct {
    BYTE    token;          // 0xFD, 0xFE, or 0xFF
    USHORT  status;         // Status flags
    USHORT  curcmd;         // Current command
    ULONGLONG rowcount;     // Affected rows (TDS 7.2+)
} TOKEN_DONE;

// Status flags
#define DONE_MORE           0x0001  // More results follow
#define DONE_ERROR          0x0002  // Error occurred
#define DONE_INXACT         0x0004  // In transaction
#define DONE_COUNT          0x0010  // Row count valid
#define DONE_ATTN           0x0020  // Attention acknowledged
#define DONE_SRVERROR       0x0100  // Server error
```

## RPC (Remote Procedure Call) Protocol

### RPC Request
```c
typedef struct {
    USHORT  proc_name_length;  // 0xFFFF for proc ID
    union {
        WCHAR   proc_name[proc_name_length];
        USHORT  proc_id;        // If length = 0xFFFF
    };
    USHORT  option_flags;
    // Parameters follow
} RPC_REQUEST;

// Option flags
#define RPC_RECOMPILE       0x0001
#define RPC_NOMETADATA      0x0002

// Special procedure IDs
#define SP_CURSOR           1
#define SP_CURSOROPEN       2
#define SP_CURSORPREPARE    3
#define SP_CURSOREXECUTE    4
#define SP_CURSORPREPEXEC   5
#define SP_CURSORUNPREPARE  6
#define SP_CURSORFETCH      7
#define SP_CURSOROPTION     8
#define SP_CURSORCLOSE      9
#define SP_EXECUTESQL       10
#define SP_PREPARE          11
#define SP_EXECUTE          12
#define SP_PREPEXEC         13
#define SP_PREPEXECRPC      14
#define SP_UNPREPARE        15
```

### RPC Parameter
```c
typedef struct {
    BYTE    name_length;
    WCHAR   name[name_length];
    BYTE    status_flags;
    TYPE_INFO type_info;
    // Value follows based on type
} RPC_PARAMETER;

// Status flags
#define PARAM_BYREF         0x01
#define PARAM_DEFAULT       0x02
#define PARAM_ENCRYPTED     0x08
```

---