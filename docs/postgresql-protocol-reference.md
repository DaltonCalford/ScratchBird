# PostgreSQL Wire Protocol Complete Specification

## Protocol Overview

- **Default Port**: 5432
- **Byte Order**: Network (Big-Endian)
- **Protocol Version**: 3.0 (0x00030000)
- **Message-based**: Each message has a type byte and length

## Connection and Startup

### Startup Message
```c
typedef struct {
    UINT32  length;         // Total message length including this field
    UINT32  protocol;       // Protocol version (196608 = 3.0)
    // Null-terminated parameter pairs follow
    char    parameters[];   // "user\0username\0database\0dbname\0\0"
} StartupMessage;

// Protocol version calculation
#define PG_PROTOCOL_MAJOR 3
#define PG_PROTOCOL_MINOR 0
#define PG_PROTOCOL_VERSION ((PG_PROTOCOL_MAJOR << 16) | PG_PROTOCOL_MINOR)

// Common startup parameters
const char* startup_params[] = {
    "user", "username",
    "database", "dbname",
    "options", "-c statement_timeout=0",
    "application_name", "myapp",
    "client_encoding", "UTF8",
    NULL
};
```

### SSL Request
```c
typedef struct {
    UINT32  length;         // Always 8
    UINT32  ssl_code;       // 80877103 (0x04D2162F)
} SSLRequest;

// Server response
// 'S' - SSL supported
// 'N' - SSL not supported
```

### Authentication Messages
```c
// From server
typedef struct {
    char    type;           // 'R'
    UINT32  length;         // Message length
    UINT32  auth_type;      // Authentication type
    // Type-specific data follows
} AuthenticationMessage;

// Authentication types
#define AUTH_OK                 0
#define AUTH_KERBEROS_V5        2
#define AUTH_CLEARTEXT_PASSWORD 3
#define AUTH_MD5_PASSWORD       5
#define AUTH_SCM_CREDENTIAL     6
#define AUTH_GSS                7
#define AUTH_GSS_CONTINUE       8
#define AUTH_SSPI               9
#define AUTH_SASL              10
#define AUTH_SASL_CONTINUE     11
#define AUTH_SASL_FINAL        12

// MD5 authentication
typedef struct {
    char    type;           // 'R'
    UINT32  length;         // 12
    UINT32  auth_type;      // 5
    BYTE    salt[4];        // Random salt
} AuthenticationMD5;

// MD5 password calculation
void calculate_md5_password(char* output, const char* password, 
                           const char* username, BYTE* salt) {
    // Step 1: MD5(password + username)
    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, password, strlen(password));
    MD5_Update(&ctx, username, strlen(username));
    unsigned char hash1[16];
    MD5_Final(hash1, &ctx);
    
    // Convert to hex
    char hex1[33];
    for (int i = 0; i < 16; i++) {
        sprintf(hex1 + i*2, "%02x", hash1[i]);
    }
    
    // Step 2: MD5(hex1 + salt)
    MD5_Init(&ctx);
    MD5_Update(&ctx, hex1, 32);
    MD5_Update(&ctx, salt, 4);
    unsigned char hash2[16];
    MD5_Final(hash2, &ctx);
    
    // Convert to hex with "md5" prefix
    strcpy(output, "md5");
    for (int i = 0; i < 16; i++) {
        sprintf(output + 3 + i*2, "%02x", hash2[i]);
    }
}

// SASL/SCRAM-SHA-256 authentication
typedef struct {
    char    type;           // 'R'
    UINT32  length;
    UINT32  auth_type;      // 10
    // Null-terminated mechanism names follow
    char    mechanisms[];   // "SCRAM-SHA-256\0SCRAM-SHA-256-PLUS\0\0"
} AuthenticationSASL;
```

## Message Formats

### Message Header
```c
typedef struct {
    char    type;           // Message type identifier
    UINT32  length;         // Message length (excluding type byte)
} MessageHeader;

// Frontend message types (client to server)
#define MSG_BIND                'B'
#define MSG_CLOSE               'C'
#define MSG_COPY_DATA           'd'
#define MSG_COPY_DONE           'c'
#define MSG_COPY_FAIL           'f'
#define MSG_DESCRIBE            'D'
#define MSG_EXECUTE             'E'
#define MSG_FLUSH               'H'
#define MSG_FUNCTION_CALL       'F'
#define MSG_PARSE               'P'
#define MSG_PASSWORD            'p'
#define MSG_QUERY               'Q'
#define MSG_SYNC                'S'
#define MSG_TERMINATE           'X'

// Backend message types (server to client)
#define MSG_AUTH_REQUEST        'R'
#define MSG_BACKEND_KEY_DATA    'K'
#define MSG_BIND_COMPLETE       '2'
#define MSG_CLOSE_COMPLETE      '3'
#define MSG_COMMAND_COMPLETE    'C'
#define MSG_COPY_IN_RESPONSE    'G'
#define MSG_COPY_OUT_RESPONSE   'H'
#define MSG_COPY_BOTH_RESPONSE  'W'
#define MSG_DATA_ROW            'D'
#define MSG_EMPTY_QUERY         'I'
#define MSG_ERROR               'E'
#define MSG_FUNCTION_RESULT     'V'
#define MSG_NO_DATA             'n'
#define MSG_NOTICE              'N'
#define MSG_NOTIFICATION        'A'
#define MSG_PARAMETER_DESC      't'
#define MSG_PARAMETER_STATUS    'S'
#define MSG_PARSE_COMPLETE      '1'
#define MSG_PORTAL_SUSPENDED    's'
#define MSG_READY_FOR_QUERY     'Z'
#define MSG_ROW_DESCRIPTION     'T'
```

### Query Message
```c
typedef struct {
    char    type;           // 'Q'
    UINT32  length;
    char    query[];        // Null-terminated SQL string
} QueryMessage;

// Example
void send_query(int socket, const char* sql) {
    size_t sql_len = strlen(sql) + 1;
    size_t msg_len = 4 + sql_len;
    
    char* buffer = malloc(1 + msg_len);
    buffer[0] = 'Q';
    *(UINT32*)(buffer + 1) = htonl(msg_len);
    strcpy(buffer + 5, sql);
    
    send(socket, buffer, 1 + msg_len, 0);
    free(buffer);
}
```

### Parse Message (Extended Query)
```c
typedef struct {
    char    type;           // 'P'
    UINT32  length;
    char    statement_name[];  // Null-terminated (empty for unnamed)
    char    query[];          // Null-terminated SQL
    UINT16  param_count;      // Number of parameter types
    UINT32  param_types[];    // OIDs of parameter types
} ParseMessage;
```

### Bind Message
```c
typedef struct {
    char    type;           // 'B'
    UINT32  length;
    char    portal_name[];     // Null-terminated
    char    statement_name[];  // Null-terminated
    UINT16  format_code_count; // Number of format codes
    UINT16  format_codes[];    // 0=text, 1=binary
    UINT16  param_count;       // Number of parameters
    // For each parameter:
    //   UINT32 length (-1 for NULL)
    //   BYTE data[length]
    UINT16  result_format_count;
    UINT16  result_formats[];  // 0=text, 1=binary
} BindMessage;
```

### Execute Message
```c
typedef struct {
    char    type;           // 'E'
    UINT32  length;
    char    portal_name[];  // Null-terminated
    UINT32  max_rows;       // 0 = unlimited
} ExecuteMessage;
```

### Row Description Message
```c
typedef struct {
    char    type;           // 'T'
    UINT32  length;
    UINT16  field_count;
    // For each field:
    struct {
        char    name[];     // Null-terminated field name
        UINT32  table_oid;  // Table OID (0 if not a table field)
        UINT16  column_num; // Column number (0 if not a table column)
        UINT32  type_oid;   // Data type OID
        UINT16  type_size;  // Data type size (-1 for variable)
        UINT32  type_mod;   // Type modifier
        UINT16  format;     // 0=text, 1=binary
    } fields[];
} RowDescriptionMessage;
```

### Data Row Message
```c
typedef struct {
    char    type;           // 'D'
    UINT32  length;
    UINT16  column_count;
    // For each column:
    //   UINT32 length (-1 for NULL)
    //   BYTE data[length]
} DataRowMessage;

// Reading a data row
void read_data_row(int socket, int column_count) {
    for (int i = 0; i < column_count; i++) {
        UINT32 length;
        recv(socket, &length, 4, 0);
        length = ntohl(length);
        
        if (length == 0xFFFFFFFF) {
            // NULL value
            continue;
        }
        
        char* data = malloc(length + 1);
        recv(socket, data, length, 0);
        data[length] = '\0';
        
        // Process data based on type
        free(data);
    }
}
```

## PostgreSQL Data Type Binary Formats

### Integer Types

#### SMALLINT (INT2)
```c
// OID: 21
// Size: 2 bytes

void encode_int2(unsigned char* buffer, int16_t value) {
    *(int16_t*)buffer = htons(value);
}

int16_t decode_int2(const unsigned char* buffer) {
    return ntohs(*(int16_t*)buffer);
}
```

#### INTEGER (INT4)
```c
// OID: 23
// Size: 4 bytes

void encode_int4(unsigned char* buffer, int32_t value) {
    *(int32_t*)buffer = htonl(value);
}

int32_t decode_int4(const unsigned char* buffer) {
    return ntohl(*(int32_t*)buffer);
}
```

#### BIGINT (INT8)
```c
// OID: 20
// Size: 8 bytes

void encode_int8(unsigned char* buffer, int64_t value) {
    // Network byte order (big-endian)
    for (int i = 7; i >= 0; i--) {
        buffer[i] = value & 0xFF;
        value >>= 8;
    }
}

int64_t decode_int8(const unsigned char* buffer) {
    int64_t value = 0;
    for (int i = 0; i < 8; i++) {
        value = (value << 8) | buffer[i];
    }
    return value;
}
```

### NUMERIC/DECIMAL
```c
// OID: 1700
// Variable size

typedef struct {
    int16_t ndigits;        // Number of digits (base 10000)
    int16_t weight;         // Weight of first digit
    int16_t sign;           // 0x0000=positive, 0x4000=negative, 0xC000=NaN
    int16_t dscale;         // Display scale
    int16_t digits[];       // Digits in base 10000
} PGNumeric;

#define NUMERIC_POS     0x0000
#define NUMERIC_NEG     0x4000
#define NUMERIC_NAN     0xC000
#define NUMERIC_BASE    10000

void encode_numeric(unsigned char* buffer, double value, int precision, int scale) {
    PGNumeric* num = (PGNumeric*)buffer;
    
    // Handle special cases
    if (isnan(value)) {
        num->ndigits = 0;
        num->weight = 0;
        num->sign = htons(NUMERIC_NAN);
        num->dscale = 0;
        return;
    }
    
    // Determine sign
    num->sign = htons(value < 0 ? NUMERIC_NEG : NUMERIC_POS);
    value = fabs(value);
    
    // Scale the value
    value *= pow(10, scale);
    
    // Convert to base 10000 digits
    int64_t ival = (int64_t)value;
    int ndigits = 0;
    int16_t digits[40];  // Max precision
    
    while (ival > 0) {
        digits[ndigits++] = ival % NUMERIC_BASE;
        ival /= NUMERIC_BASE;
    }
    
    // Reverse digits and encode
    num->ndigits = htons(ndigits);
    num->weight = htons(ndigits - 1 - scale / 4);
    num->dscale = htons(scale);
    
    for (int i = 0; i < ndigits; i++) {
        num->digits[i] = htons(digits[ndigits - 1 - i]);
    }
}
```

### Floating-Point Types

#### REAL (FLOAT4)
```c
// OID: 700
// Size: 4 bytes

void encode_float4(unsigned char* buffer, float value) {
    union {
        float f;
        uint32_t i;
    } converter;
    converter.f = value;
    *(uint32_t*)buffer = htonl(converter.i);
}

float decode_float4(const unsigned char* buffer) {
    union {
        float f;
        uint32_t i;
    } converter;
    converter.i = ntohl(*(uint32_t*)buffer);
    return converter.f;
}
```

#### DOUBLE PRECISION (FLOAT8)
```c
// OID: 701
// Size: 8 bytes

void encode_float8(unsigned char* buffer, double value) {
    union {
        double d;
        uint64_t i;
    } converter;
    converter.d = value;
    
    // Convert to network byte order
    for (int i = 7; i >= 0; i--) {
        buffer[i] = converter.i & 0xFF;
        converter.i >>= 8;
    }
}

double decode_float8(const unsigned char* buffer) {
    union {
        double d;
        uint64_t i;
    } converter;
    
    converter.i = 0;
    for (int i = 0; i < 8; i++) {
        converter.i = (converter.i << 8) | buffer[i];
    }
    return converter.d;
}
```

### Character Types

#### CHAR (BPCHAR)
```c
// OID: 1042
// Fixed-length, blank-padded

void encode_bpchar(unsigned char* buffer, const char* value, int length) {
    int value_len = strlen(value);
    memcpy(buffer, value, value_len < length ? value_len : length);
    
    // Pad with spaces
    if (value_len < length) {
        memset(buffer + value_len, ' ', length - value_len);
    }
}
```

#### VARCHAR
```c
// OID: 1043
// Variable-length

void encode_varchar(unsigned char* buffer, const char* value) {
    strcpy((char*)buffer, value);
}
```

#### TEXT
```c
// OID: 25
// Variable-length, no limit

void encode_text(unsigned char* buffer, const char* value) {
    strcpy((char*)buffer, value);
}
```

### Binary Type

#### BYTEA
```c
// OID: 17
// Variable-length binary

void encode_bytea(unsigned char* buffer, const unsigned char* data, int length) {
    memcpy(buffer, data, length);
}

// Text format uses hex encoding: \xDEADBEEF
void encode_bytea_text(char* buffer, const unsigned char* data, int length) {
    strcpy(buffer, "\\x");
    for (int i = 0; i < length; i++) {
        sprintf(buffer + 2 + i*2, "%02x", data[i]);
    }
}
```

### Date/Time Types

#### DATE
```c
// OID: 1082
// Size: 4 bytes
// Days since 2000-01-01

#define POSTGRES_EPOCH_DATE 10957  // Days from Unix epoch to 2000-01-01

void encode_date(unsigned char* buffer, int year, int month, int day) {
    // Calculate days since 2000-01-01
    struct tm tm = {0};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    
    time_t t = mktime(&tm);
    int32_t days = t / 86400 - POSTGRES_EPOCH_DATE;
    
    encode_int4(buffer, days);
}

void decode_date(const unsigned char* buffer, int* year, int* month, int* day) {
    int32_t days = decode_int4(buffer);
    
    time_t t = (days + POSTGRES_EPOCH_DATE) * 86400;
    struct tm* tm = gmtime(&t);
    
    *year = tm->tm_year + 1900;
    *month = tm->tm_mon + 1;
    *day = tm->tm_mday;
}
```

#### TIME
```c
// OID: 1083
// Size: 8 bytes
// Microseconds since midnight

void encode_time(unsigned char* buffer, int hour, int minute, int second, int microsec) {
    int64_t usec = ((hour * 3600LL + minute * 60 + second) * 1000000LL) + microsec;
    encode_int8(buffer, usec);
}

void decode_time(const unsigned char* buffer, int* hour, int* minute, 
                 int* second, int* microsec) {
    int64_t usec = decode_int8(buffer);
    
    *microsec = usec % 1000000;
    usec /= 1000000;
    *second = usec % 60;
    usec /= 60;
    *minute = usec % 60;
    *hour = usec / 60;
}
```

#### TIMESTAMP
```c
// OID: 1114
// Size: 8 bytes
// Microseconds since 2000-01-01 00:00:00

#define POSTGRES_EPOCH_TIMESTAMP 946684800LL  // Unix timestamp of 2000-01-01

void encode_timestamp(unsigned char* buffer, time_t seconds, int microsec) {
    int64_t pg_timestamp = (seconds - POSTGRES_EPOCH_TIMESTAMP) * 1000000LL + microsec;
    encode_int8(buffer, pg_timestamp);
}

void decode_timestamp(const unsigned char* buffer, time_t* seconds, int* microsec) {
    int64_t pg_timestamp = decode_int8(buffer);
    
    *microsec = pg_timestamp % 1000000;
    *seconds = (pg_timestamp / 1000000) + POSTGRES_EPOCH_TIMESTAMP;
}
```

#### TIMESTAMPTZ
```c
// OID: 1184
// Size: 8 bytes
// Microseconds since 2000-01-01 00:00:00 UTC

// Same encoding as TIMESTAMP but always in UTC
void encode_timestamptz(unsigned char* buffer, time_t utc_seconds, int microsec) {
    encode_timestamp(buffer, utc_seconds, microsec);
}
```

#### INTERVAL
```c
// OID: 1186
// Size: 16 bytes

typedef struct {
    int64_t time;           // Microseconds
    int32_t day;            // Days
    int32_t month;          // Months
} PGInterval;

void encode_interval(unsigned char* buffer, int months, int days, int64_t microseconds) {
    encode_int8(buffer, microseconds);
    encode_int4(buffer + 8, days);
    encode_int4(buffer + 12, months);
}

void decode_interval(const unsigned char* buffer, int* months, int* days, int64_t* microseconds) {
    *microseconds = decode_int8(buffer);
    *days = decode_int4(buffer + 8);
    *months = decode_int4(buffer + 12);
}
```

### Boolean Type

#### BOOLEAN
```c
// OID: 16
// Size: 1 byte

void encode_bool(unsigned char* buffer, bool value) {
    buffer[0] = value ? 1 : 0;
}

bool decode_bool(const unsigned char* buffer) {
    return buffer[0] != 0;
}
```

### Geometric Types

#### POINT
```c
// OID: 600
// Size: 16 bytes

typedef struct {
    double x;
    double y;
} PGPoint;

void encode_point(unsigned char* buffer, double x, double y) {
    encode_float8(buffer, x);
    encode_float8(buffer + 8, y);
}

void decode_point(const unsigned char* buffer, double* x, double* y) {
    *x = decode_float8(buffer);
    *y = decode_float8(buffer + 8);
}
```

#### LINE
```c
// OID: 628
// Size: 24 bytes
// Represents Ax + By + C = 0

typedef struct {
    double A;
    double B;
    double C;
} PGLine;

void encode_line(unsigned char* buffer, double a, double b, double c) {
    encode_float8(buffer, a);
    encode_float8(buffer + 8, b);
    encode_float8(buffer + 16, c);
}
```

#### BOX
```c
// OID: 603
// Size: 32 bytes

typedef struct {
    PGPoint high;           // Upper right corner
    PGPoint low;            // Lower left corner
} PGBox;

void encode_box(unsigned char* buffer, double x1, double y1, double x2, double y2) {
    // High point
    encode_float8(buffer, fmax(x1, x2));
    encode_float8(buffer + 8, fmax(y1, y2));
    // Low point
    encode_float8(buffer + 16, fmin(x1, x2));
    encode_float8(buffer + 24, fmin(y1, y2));
}
```

#### POLYGON
```c
// OID: 604
// Variable size

typedef struct {
    int32_t npoints;
    PGPoint points[];
} PGPolygon;

void encode_polygon(unsigned char* buffer, PGPoint* points, int npoints) {
    encode_int4(buffer, npoints);
    buffer += 4;
    
    for (int i = 0; i < npoints; i++) {
        encode_float8(buffer, points[i].x);
        encode_float8(buffer + 8, points[i].y);
        buffer += 16;
    }
}
```

### Network Types

#### INET
```c
// OID: 869
// Size: 7 or 19 bytes

typedef struct {
    uint8_t family;         // AF_INET (2) or AF_INET6 (3)
    uint8_t bits;           // Netmask bits
    uint8_t is_cidr;        // 0 for INET, 1 for CIDR
    uint8_t nb;             // Number of bytes (4 or 16)
    uint8_t data[];         // IP address bytes
} PGInet;

void encode_inet(unsigned char* buffer, const char* ip_string) {
    PGInet* inet = (PGInet*)buffer;
    
    struct in_addr addr4;
    struct in6_addr addr6;
    
    if (inet_pton(AF_INET, ip_string, &addr4) == 1) {
        inet->family = AF_INET;
        inet->bits = 32;
        inet->is_cidr = 0;
        inet->nb = 4;
        memcpy(inet->data, &addr4, 4);
    } else if (inet_pton(AF_INET6, ip_string, &addr6) == 1) {
        inet->family = AF_INET6;
        inet->bits = 128;
        inet->is_cidr = 0;
        inet->nb = 16;
        memcpy(inet->data, &addr6, 16);
    }
}
```

#### CIDR
```c
// OID: 650
// Same as INET with is_cidr = 1

void encode_cidr(unsigned char* buffer, const char* cidr_string) {
    // Parse CIDR notation (e.g., "192.168.1.0/24")
    char ip[INET6_ADDRSTRLEN];
    int bits;
    sscanf(cidr_string, "%[^/]/%d", ip, &bits);
    
    encode_inet(buffer, ip);
    PGInet* inet = (PGInet*)buffer;
    inet->bits = bits;
    inet->is_cidr = 1;
}
```

#### MACADDR
```c
// OID: 829
// Size: 6 bytes

void encode_macaddr(unsigned char* buffer, const unsigned char mac[6]) {
    memcpy(buffer, mac, 6);
}

void decode_macaddr(const unsigned char* buffer, unsigned char mac[6]) {
    memcpy(mac, buffer, 6);
}
```

#### MACADDR8
```c
// OID: 774
// Size: 8 bytes

void encode_macaddr8(unsigned char* buffer, const unsigned char mac[8]) {
    memcpy(buffer, mac, 8);
}
```

### UUID Type

#### UUID
```c
// OID: 2950
// Size: 16 bytes

typedef struct {
    uint32_t time_low;
    uint16_t time_mid;
    uint16_t time_hi_version;
    uint8_t  clock_seq_hi_variant;
    uint8_t  clock_seq_low;
    uint8_t  node[6];
} PGUuid;

void encode_uuid(unsigned char* buffer, const char* uuid_string) {
    // Parse UUID string (e.g., "550e8400-e29b-41d4-a716-446655440000")
    unsigned int data[16];
    sscanf(uuid_string, "%2x%2x%2x%2x-%2x%2x-%2x%2x-%2x%2x-%2x%2x%2x%2x%2x%2x",
           &data[0], &data[1], &data[2], &data[3],
           &data[4], &data[5], &data[6], &data[7],
           &data[8], &data[9], &data[10], &data[11],
           &data[12], &data[13], &data[14], &data[15]);
    
    for (int i = 0; i < 16; i++) {
        buffer[i] = data[i];
    }
}

void decode_uuid(const unsigned char* buffer, char* uuid_string) {
    sprintf(uuid_string, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            buffer[0], buffer[1], buffer[2], buffer[3],
            buffer[4], buffer[5], buffer[6], buffer[7],
            buffer[8], buffer[9], buffer[10], buffer[11],
            buffer[12], buffer[13], buffer[14], buffer[15]);
}
```

### JSON Types

#### JSON
```c
// OID: 114
// Text representation

void encode_json(unsigned char* buffer, const char* json_string) {
    strcpy((char*)buffer, json_string);
}
```

#### JSONB
```c
// OID: 3802
// Binary JSON format

typedef struct {
    uint32_t version;       // Format version (1)
    // Followed by binary JSON data
} PGJsonb;

// JSONB container header
typedef struct {
    uint32_t header;        // Type and element count
    // JEntry array follows
    // Data follows JEntry array
} JsonbContainer;

// JEntry - describes location and type of each value
typedef struct {
    uint32_t type_and_len;  // Type in high 3 bits, length/offset in low 29 bits
} JEntry;

// JSONB value types
#define JSONB_NULL      0x00
#define JSONB_STRING    0x01
#define JSONB_NUMERIC   0x02
#define JSONB_FALSE     0x03
#define JSONB_TRUE      0x04
#define JSONB_ARRAY     0x05
#define JSONB_OBJECT    0x06

void encode_jsonb_simple(unsigned char* buffer, const char* json_string) {
    PGJsonb* jsonb = (PGJsonb*)buffer;
    jsonb->version = htonl(1);
    
    // For complex encoding, would need full JSON parser
    // This is simplified example for a string value
    JsonbContainer* container = (JsonbContainer*)(buffer + 4);
    container->header = htonl(JSONB_STRING << 28 | strlen(json_string));
    strcpy((char*)(container + 1), json_string);
}
```

### Array Types

#### Array Format
```c
// Arrays have OID = element_type_oid
// For example, INT4 array has OID 1007

typedef struct {
    int32_t ndim;           // Number of dimensions
    int32_t flags;          // 0 or 1 (has nulls)
    uint32_t element_type;  // Element type OID
    // For each dimension:
    int32_t dim_size;       // Dimension size
    int32_t dim_lower;      // Lower bound (usually 1)
    // Followed by elements:
    // For each element:
    //   int32_t length (-1 for NULL)
    //   byte data[length]
} PGArray;

void encode_int4_array(unsigned char* buffer, int32_t* values, int count) {
    PGArray* arr = (PGArray*)buffer;
    arr->ndim = htonl(1);
    arr->flags = htonl(0);  // No nulls
    arr->element_type = htonl(23);  // INT4 OID
    
    int32_t* dims = (int32_t*)(arr + 1);
    dims[0] = htonl(count);  // Size
    dims[1] = htonl(1);      // Lower bound
    
    unsigned char* data = (unsigned char*)&dims[2];
    for (int i = 0; i < count; i++) {
        *(int32_t*)data = htonl(4);  // Element length
        data += 4;
        encode_int4(data, values[i]);
        data += 4;
    }
}

void decode_int4_array(const unsigned char* buffer, int32_t** values, int* count) {
    PGArray* arr = (PGArray*)buffer;
    int ndim = ntohl(arr->ndim);
    
    if (ndim != 1) {
        // Handle multi-dimensional arrays
        return;
    }
    
    int32_t* dims = (int32_t*)(arr + 1);
    *count = ntohl(dims[0]);
    
    *values = malloc(*count * sizeof(int32_t));
    unsigned char* data = (unsigned char*)&dims[2];
    
    for (int i = 0; i < *count; i++) {
        int32_t length = ntohl(*(int32_t*)data);
        data += 4;
        
        if (length == -1) {
            // NULL element
            (*values)[i] = 0;
        } else {
            (*values)[i] = decode_int4(data);
            data += length;
        }
    }
}
```

### Range Types

#### INT4RANGE
```c
// OID: 3904

typedef struct {
    uint8_t flags;          // Range flags
    // If has lower bound:
    int32_t lower;
    // If has upper bound:
    int32_t upper;
} PGInt4Range;

// Range flags
#define RANGE_EMPTY         0x01
#define RANGE_LB_INC        0x02  // Lower bound inclusive
#define RANGE_UB_INC        0x04  // Upper bound inclusive
#define RANGE_LB_INF        0x08  // Lower bound infinite
#define RANGE_UB_INF        0x10  // Upper bound infinite

void encode_int4range(unsigned char* buffer, int32_t lower, int32_t upper, 
                      bool lower_inc, bool upper_inc) {
    PGInt4Range* range = (PGInt4Range*)buffer;
    
    range->flags = 0;
    if (lower_inc) range->flags |= RANGE_LB_INC;
    if (upper_inc) range->flags |= RANGE_UB_INC;
    
    unsigned char* data = buffer + 1;
    
    // Lower bound
    *(int32_t*)data = htonl(4);
    data += 4;
    encode_int4(data, lower);
    data += 4;
    
    // Upper bound
    *(int32_t*)data = htonl(4);
    data += 4;
    encode_int4(data, upper);
}
```

### Special Types

#### OID
```c
// OID: 26
// Size: 4 bytes
// Object identifier

void encode_oid(unsigned char* buffer, uint32_t oid) {
    *(uint32_t*)buffer = htonl(oid);
}

uint32_t decode_oid(const unsigned char* buffer) {
    return ntohl(*(uint32_t*)buffer);
}
```

#### XID
```c
// OID: 28
// Size: 4 bytes
// Transaction ID

void encode_xid(unsigned char* buffer, uint32_t xid) {
    *(uint32_t*)buffer = htonl(xid);
}
```

#### PG_LSN
```c
// OID: 3220
// Size: 8 bytes
// Log Sequence Number (WAL position)

typedef struct {
    uint32_t file;          // Log file number
    uint32_t offset;        // Offset in file
} PGLsn;

void encode_pg_lsn(unsigned char* buffer, uint64_t lsn) {
    encode_int8(buffer, lsn);
}

void decode_pg_lsn(const unsigned char* buffer, uint32_t* file, uint32_t* offset) {
    uint64_t lsn = decode_int8(buffer);
    *file = lsn >> 32;
    *offset = lsn & 0xFFFFFFFF;
}
```

## COPY Protocol

### Copy In/Out Response
```c
typedef struct {
    char    type;           // 'G' (in) or 'H' (out) or 'W' (both)
    uint32_t length;
    uint8_t  format;        // 0=text, 1=binary
    uint16_t column_count;
    uint16_t column_formats[]; // Format for each column
} CopyResponse;
```

### Copy Data
```c
typedef struct {
    char    type;           // 'd'
    uint32_t length;
    // For binary format:
    char    signature[11];  // "PGCOPY\n\377\r\n\0"
    uint32_t flags;         // Extension flags
    uint32_t header_ext;    // Header extension length
    // Data rows follow
} CopyData;

// Binary copy row format
typedef struct {
    uint16_t field_count;
    // For each field:
    //   int32_t length (-1 for NULL)
    //   byte data[length]
} CopyBinaryRow;
```

## Error and Notice Messages

### Error/Notice Format
```c
typedef struct {
    char    type;           // 'E' (error) or 'N' (notice)
    uint32_t length;
    // Multiple fields, each:
    char    field_type;     // Field identifier
    char    value[];        // Null-terminated string
    // Terminated by '\0'
} ErrorMessage;

// Field types
#define ERR_SEVERITY        'S'  // ERROR, WARNING, NOTICE, etc.
#define ERR_SEVERITY_V      'V'  // Localized severity
#define ERR_SQLSTATE        'C'  // SQLSTATE code
#define ERR_MESSAGE         'M'  // Primary message
#define ERR_DETAIL          'D'  // Detail message
#define ERR_HINT            'H'  // Hint message
#define ERR_POSITION        'P'  // Error position
#define ERR_INTERNAL_POS    'p'  // Internal position
#define ERR_INTERNAL_QUERY  'q'  // Internal query
#define ERR_WHERE           'W'  // Context
#define ERR_SCHEMA          's'  // Schema name
#define ERR_TABLE           't'  // Table name
#define ERR_COLUMN          'c'  // Column name
#define ERR_DATATYPE        'd'  // Data type name
#define ERR_CONSTRAINT      'n'  // Constraint name
#define ERR_FILE            'F'  // File name
#define ERR_LINE            'L'  // Line number
#define ERR_ROUTINE         'R'  // Routine name

// Example: Parsing error message
void parse_error_message(const unsigned char* buffer, uint32_t length) {
    const unsigned char* end = buffer + length;
    
    while (buffer < end) {
        char field_type = *buffer++;
        if (field_type == '\0') break;
        
        const char* value = (const char*)buffer;
        size_t value_len = strlen(value);
        
        switch (field_type) {
            case ERR_SEVERITY:
                printf("Severity: %s\n", value);
                break;
            case ERR_SQLSTATE:
                printf("SQLSTATE: %s\n", value);
                break;
            case ERR_MESSAGE:
                printf("Message: %s\n", value);
                break;
            // ... handle other fields
        }
        
        buffer += value_len + 1;
    }
}
```

## Transaction and Session Management

### Ready for Query
```c
typedef struct {
    char    type;           // 'Z'
    uint32_t length;        // Always 5
    char    status;         // Transaction status
} ReadyForQuery;

// Transaction status
#define TXN_IDLE            'I'  // Idle (not in transaction)
#define TXN_IN_BLOCK        'T'  // In transaction block
#define TXN_FAILED          'E'  // In failed transaction block
```

### Parameter Status
```c
typedef struct {
    char    type;           // 'S'
    uint32_t length;
    char    name[];         // Parameter name (null-terminated)
    char    value[];        // Parameter value (null-terminated)
} ParameterStatus;

// Common parameters
// - server_version
// - server_encoding
// - client_encoding
// - application_name
// - is_superuser
// - session_authorization
// - DateStyle
// - IntervalStyle
// - TimeZone
// - integer_datetimes
// - standard_conforming_strings
```

### Backend Key Data
```c
typedef struct {
    char    type;           // 'K'
    uint32_t length;        // Always 12
    uint32_t process_id;    // Backend process ID
    uint32_t secret_key;    // Secret key for cancellation
} BackendKeyData;

// Cancel request (separate connection)
typedef struct {
    uint32_t length;        // 16
    uint32_t cancel_code;   // 80877102 (0x04D2162E)
    uint32_t process_id;    // From BackendKeyData
    uint32_t secret_key;    // From BackendKeyData
} CancelRequest;
```

## Large Object Protocol

### Function Call Message
```c
typedef struct {
    char    type;           // 'F'
    uint32_t length;
    uint32_t function_oid;  // Function OID
    uint16_t format_count;  // Number of format codes
    uint16_t formats[];     // 0=text, 1=binary
    uint16_t arg_count;     // Number of arguments
    // For each argument:
    //   int32_t length (-1 for NULL)
    //   byte data[length]
    uint16_t result_format; // 0=text, 1=binary
} FunctionCall;

// Large object functions
#define LO_OPEN     952
#define LO_CLOSE    953
#define LO_CREATE   957
#define LO_UNLINK   964
#define LO_READ     954
#define LO_WRITE    955
#define LO_LSEEK    956
#define LO_TELL     958
#define LO_TRUNCATE 1004
```

### Function Result
```c
typedef struct {
    char    type;           // 'V'
    uint32_t length;
    int32_t result_length;  // -1 for NULL
    // If not NULL:
    unsigned char result[]; // Result data
} FunctionResult;
```