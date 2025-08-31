# MySQL and MariaDB Protocol Complete Specification

## MySQL Protocol Overview

- **Default Port**: 3306
- **Byte Order**: Little-Endian
- **Protocol Version**: 10
- **Packet-based**: Max packet size 16MB (2^24 - 1 bytes)

## Packet Structure

### Basic Packet Format
```c
typedef struct {
    uint32_t length:24;     // Packet length (3 bytes)
    uint8_t  sequence_id;   // Sequence number
    uint8_t  payload[];     // Packet payload
} MySQLPacket;

// Reading a packet
int read_packet(int socket, uint8_t** payload, uint32_t* length) {
    uint8_t header[4];
    recv(socket, header, 4, 0);
    
    *length = header[0] | (header[1] << 8) | (header[2] << 16);
    uint8_t seq_id = header[3];
    
    *payload = malloc(*length);
    recv(socket, *payload, *length, 0);
    
    return seq_id;
}

// Writing a packet
void write_packet(int socket, uint8_t* data, uint32_t length, uint8_t seq_id) {
    uint8_t header[4];
    header[0] = length & 0xFF;
    header[1] = (length >> 8) & 0xFF;
    header[2] = (length >> 16) & 0xFF;
    header[3] = seq_id;
    
    send(socket, header, 4, 0);
    send(socket, data, length, 0);
}
```

### Length-Encoded Integer
```c
// MySQL uses variable-length encoding for integers
uint64_t read_lenenc_int(uint8_t** buffer) {
    uint8_t first = *(*buffer)++;
    
    if (first < 0xFB) {
        return first;
    } else if (first == 0xFC) {
        uint16_t value = (*buffer)[0] | ((*buffer)[1] << 8);
        *buffer += 2;
        return value;
    } else if (first == 0xFD) {
        uint32_t value = (*buffer)[0] | ((*buffer)[1] << 8) | ((*buffer)[2] << 16);
        *buffer += 3;
        return value;
    } else if (first == 0xFE) {
        uint64_t value = 0;
        for (int i = 0; i < 8; i++) {
            value |= ((uint64_t)(*buffer)[i]) << (i * 8);
        }
        *buffer += 8;
        return value;
    } else { // 0xFB = NULL
        return 0xFFFFFFFFFFFFFFFF;  // NULL indicator
    }
}

void write_lenenc_int(uint8_t** buffer, uint64_t value) {
    if (value < 251) {
        *(*buffer)++ = value;
    } else if (value < 0x10000) {
        *(*buffer)++ = 0xFC;
        *(*buffer)++ = value & 0xFF;
        *(*buffer)++ = (value >> 8) & 0xFF;
    } else if (value < 0x1000000) {
        *(*buffer)++ = 0xFD;
        *(*buffer)++ = value & 0xFF;
        *(*buffer)++ = (value >> 8) & 0xFF;
        *(*buffer)++ = (value >> 16) & 0xFF;
    } else {
        *(*buffer)++ = 0xFE;
        for (int i = 0; i < 8; i++) {
            *(*buffer)++ = (value >> (i * 8)) & 0xFF;
        }
    }
}
```

### Length-Encoded String
```c
typedef struct {
    uint64_t length;
    char*    string;
} LengthEncodedString;

LengthEncodedString read_lenenc_string(uint8_t** buffer) {
    LengthEncodedString result;
    result.length = read_lenenc_int(buffer);
    
    if (result.length == 0xFFFFFFFFFFFFFFFF) {
        result.string = NULL;
    } else {
        result.string = malloc(result.length + 1);
        memcpy(result.string, *buffer, result.length);
        result.string[result.length] = '\0';
        *buffer += result.length;
    }
    
    return result;
}

void write_lenenc_string(uint8_t** buffer, const char* string) {
    if (string == NULL) {
        *(*buffer)++ = 0xFB;  // NULL
    } else {
        size_t length = strlen(string);
        write_lenenc_int(buffer, length);
        memcpy(*buffer, string, length);
        *buffer += length;
    }
}
```

## Connection Phase

### Initial Handshake
```c
typedef struct {
    uint8_t  protocol_version;  // Always 10
    char*    server_version;     // Null-terminated
    uint32_t connection_id;
    uint8_t  auth_plugin_data_part_1[8];
    uint8_t  filler;            // Always 0x00
    uint16_t capability_flags_1;
    uint8_t  character_set;
    uint16_t status_flags;
    uint16_t capability_flags_2;
    uint8_t  auth_plugin_data_len;
    uint8_t  reserved[10];      // All 0x00
    uint8_t  auth_plugin_data_part_2[13];  // MAX(13, auth_plugin_data_len - 8)
    char*    auth_plugin_name;  // Null-terminated
} HandshakeV10;

// Capability flags
#define CLIENT_LONG_PASSWORD     0x00000001
#define CLIENT_FOUND_ROWS        0x00000002
#define CLIENT_LONG_FLAG         0x00000004
#define CLIENT_CONNECT_WITH_DB   0x00000008
#define CLIENT_NO_SCHEMA         0x00000010
#define CLIENT_COMPRESS          0x00000020
#define CLIENT_ODBC              0x00000040
#define CLIENT_LOCAL_FILES       0x00000080
#define CLIENT_IGNORE_SPACE      0x00000100
#define CLIENT_PROTOCOL_41       0x00000200
#define CLIENT_INTERACTIVE       0x00000400
#define CLIENT_SSL               0x00000800
#define CLIENT_IGNORE_SIGPIPE    0x00001000
#define CLIENT_TRANSACTIONS      0x00002000
#define CLIENT_RESERVED          0x00004000
#define CLIENT_SECURE_CONNECTION 0x00008000
#define CLIENT_MULTI_STATEMENTS  0x00010000
#define CLIENT_MULTI_RESULTS     0x00020000
#define CLIENT_PS_MULTI_RESULTS  0x00040000
#define CLIENT_PLUGIN_AUTH       0x00080000
#define CLIENT_CONNECT_ATTRS     0x00100000
#define CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA 0x00200000
#define CLIENT_CAN_HANDLE_EXPIRED_PASSWORDS   0x00400000
#define CLIENT_SESSION_TRACK     0x00800000
#define CLIENT_DEPRECATE_EOF     0x01000000

// Status flags
#define SERVER_STATUS_IN_TRANS   0x0001
#define SERVER_STATUS_AUTOCOMMIT 0x0002
#define SERVER_MORE_RESULTS_EXISTS 0x0008
#define SERVER_STATUS_NO_GOOD_INDEX_USED 0x0010
#define SERVER_STATUS_NO_INDEX_USED 0x0020
#define SERVER_STATUS_CURSOR_EXISTS 0x0040
#define SERVER_STATUS_LAST_ROW_SENT 0x0080
#define SERVER_STATUS_DB_DROPPED 0x0100
#define SERVER_STATUS_NO_BACKSLASH_ESCAPES 0x0200
#define SERVER_STATUS_METADATA_CHANGED 0x0400
#define SERVER_QUERY_WAS_SLOW    0x0800
#define SERVER_PS_OUT_PARAMS     0x1000
#define SERVER_STATUS_IN_TRANS_READONLY 0x2000
#define SERVER_SESSION_STATE_CHANGED 0x4000
```

### Handshake Response
```c
typedef struct {
    uint32_t capability_flags;
    uint32_t max_packet_size;
    uint8_t  character_set;
    uint8_t  reserved[23];      // All 0x00
    char*    username;          // Null-terminated
    uint8_t  auth_response_length;  // Or length-encoded
    uint8_t* auth_response;
    char*    database;          // If CLIENT_CONNECT_WITH_DB
    char*    auth_plugin_name;  // If CLIENT_PLUGIN_AUTH
    // Connection attributes if CLIENT_CONNECT_ATTRS
} HandshakeResponse41;

// Authentication methods
void mysql_native_password(uint8_t* output, const char* password, 
                          uint8_t* salt, size_t salt_len) {
    // SHA1(password)
    SHA_CTX ctx;
    uint8_t hash1[20];
    SHA1_Init(&ctx);
    SHA1_Update(&ctx, password, strlen(password));
    SHA1_Final(hash1, &ctx);
    
    // SHA1(SHA1(password))
    uint8_t hash2[20];
    SHA1_Init(&ctx);
    SHA1_Update(&ctx, hash1, 20);
    SHA1_Final(hash2, &ctx);
    
    // SHA1(salt + SHA1(SHA1(password)))
    uint8_t hash3[20];
    SHA1_Init(&ctx);
    SHA1_Update(&ctx, salt, salt_len);
    SHA1_Update(&ctx, hash2, 20);
    SHA1_Final(hash3, &ctx);
    
    // XOR hash1 with hash3
    for (int i = 0; i < 20; i++) {
        output[i] = hash1[i] ^ hash3[i];
    }
}

void caching_sha2_password(uint8_t* output, const char* password,
                           uint8_t* salt, size_t salt_len) {
    // SHA256(password)
    SHA256_CTX ctx;
    uint8_t hash1[32];
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, password, strlen(password));
    SHA256_Final(hash1, &ctx);
    
    // SHA256(SHA256(SHA256(password)))
    uint8_t hash2[32];
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, hash1, 32);
    SHA256_Final(hash2, &ctx);
    
    uint8_t hash3[32];
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, hash2, 32);
    SHA256_Final(hash3, &ctx);
    
    // SHA256(hash3 + salt)
    uint8_t hash4[32];
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, hash3, 32);
    SHA256_Update(&ctx, salt, salt_len);
    SHA256_Final(hash4, &ctx);
    
    // XOR hash1 with hash4
    for (int i = 0; i < 32; i++) {
        output[i] = hash1[i] ^ hash4[i];
    }
}
```

## Command Protocol

### Command Packet
```c
typedef struct {
    uint8_t command;
    uint8_t payload[];
} CommandPacket;

// Command types
#define COM_SLEEP           0x00
#define COM_QUIT            0x01
#define COM_INIT_DB         0x02
#define COM_QUERY           0x03
#define COM_FIELD_LIST      0x04
#define COM_CREATE_DB       0x05
#define COM_DROP_DB         0x06
#define COM_REFRESH         0x07
#define COM_SHUTDOWN        0x08
#define COM_STATISTICS      0x09
#define COM_PROCESS_INFO    0x0A
#define COM_CONNECT         0x0B
#define COM_PROCESS_KILL    0x0C
#define COM_DEBUG           0x0D
#define COM_PING            0x0E
#define COM_TIME            0x0F
#define COM_DELAYED_INSERT  0x10
#define COM_CHANGE_USER     0x11
#define COM_BINLOG_DUMP     0x12
#define COM_TABLE_DUMP      0x13
#define COM_CONNECT_OUT     0x14
#define COM_REGISTER_SLAVE  0x15
#define COM_STMT_PREPARE    0x16
#define COM_STMT_EXECUTE    0x17
#define COM_STMT_SEND_LONG_DATA 0x18
#define COM_STMT_CLOSE      0x19
#define COM_STMT_RESET      0x1A
#define COM_SET_OPTION      0x1B
#define COM_STMT_FETCH      0x1C
#define COM_DAEMON          0x1D
#define COM_BINLOG_DUMP_GTID 0x1E
#define COM_RESET_CONNECTION 0x1F
```

### Query Command
```c
typedef struct {
    uint8_t command;        // COM_QUERY (0x03)
    char    query[];        // SQL query (not null-terminated)
} QueryCommand;

void send_query(int socket, const char* sql) {
    size_t sql_len = strlen(sql);
    uint8_t* packet = malloc(1 + sql_len);
    packet[0] = COM_QUERY;
    memcpy(packet + 1, sql, sql_len);
    
    write_packet(socket, packet, 1 + sql_len, 0);
    free(packet);
}
```

## Result Set Protocol

### Column Count
```c
uint64_t read_column_count(uint8_t* packet) {
    uint8_t* ptr = packet;
    return read_lenenc_int(&ptr);
}
```

### Column Definition
```c
typedef struct {
    LengthEncodedString catalog;
    LengthEncodedString schema;
    LengthEncodedString table;
    LengthEncodedString org_table;
    LengthEncodedString name;
    LengthEncodedString org_name;
    uint16_t character_set;
    uint32_t column_length;
    uint8_t  type;
    uint16_t flags;
    uint8_t  decimals;
    uint16_t filler;        // Always 0x00
    uint64_t default_value; // Optional
} ColumnDefinition41;

// Column types
#define MYSQL_TYPE_DECIMAL     0x00
#define MYSQL_TYPE_TINY        0x01
#define MYSQL_TYPE_SHORT       0x02
#define MYSQL_TYPE_LONG        0x03
#define MYSQL_TYPE_FLOAT       0x04
#define MYSQL_TYPE_DOUBLE      0x05
#define MYSQL_TYPE_NULL        0x06
#define MYSQL_TYPE_TIMESTAMP   0x07
#define MYSQL_TYPE_LONGLONG    0x08
#define MYSQL_TYPE_INT24       0x09
#define MYSQL_TYPE_DATE        0x0A
#define MYSQL_TYPE_TIME        0x0B
#define MYSQL_TYPE_DATETIME    0x0C
#define MYSQL_TYPE_YEAR        0x0D
#define MYSQL_TYPE_NEWDATE     0x0E
#define MYSQL_TYPE_VARCHAR     0x0F
#define MYSQL_TYPE_BIT         0x10
#define MYSQL_TYPE_TIMESTAMP2  0xF1
#define MYSQL_TYPE_DATETIME2   0xF2
#define MYSQL_TYPE_TIME2       0xF3
#define MYSQL_TYPE_JSON        0xF5
#define MYSQL_TYPE_NEWDECIMAL  0xF6
#define MYSQL_TYPE_ENUM        0xF7
#define MYSQL_TYPE_SET         0xF8
#define MYSQL_TYPE_TINY_BLOB   0xF9
#define MYSQL_TYPE_MEDIUM_BLOB 0xFA
#define MYSQL_TYPE_LONG_BLOB   0xFB
#define MYSQL_TYPE_BLOB        0xFC
#define MYSQL_TYPE_VAR_STRING  0xFD
#define MYSQL_TYPE_STRING      0xFE
#define MYSQL_TYPE_GEOMETRY    0xFF

// Column flags
#define NOT_NULL_FLAG       0x0001
#define PRI_KEY_FLAG        0x0002
#define UNIQUE_KEY_FLAG     0x0004
#define MULTIPLE_KEY_FLAG   0x0008
#define BLOB_FLAG           0x0010
#define UNSIGNED_FLAG       0x0020
#define ZEROFILL_FLAG       0x0040
#define BINARY_FLAG         0x0080
#define ENUM_FLAG           0x0100
#define AUTO_INCREMENT_FLAG 0x0200
#define TIMESTAMP_FLAG      0x0400
#define SET_FLAG            0x0800
#define NO_DEFAULT_VALUE_FLAG 0x1000
#define ON_UPDATE_NOW_FLAG  0x2000
#define NUM_FLAG            0x8000
```

### Result Row (Text Protocol)
```c
typedef struct {
    uint8_t header;         // 0x00 or 0xFE (EOF) or 0xFF (Error)
    // For each column:
    //   Length-encoded string value
    //   Or 0xFB for NULL
} TextResultRow;

void read_text_row(uint8_t* packet, int column_count, char** values) {
    uint8_t* ptr = packet;
    
    for (int i = 0; i < column_count; i++) {
        if (*ptr == 0xFB) {
            values[i] = NULL;
            ptr++;
        } else {
            LengthEncodedString str = read_lenenc_string(&ptr);
            values[i] = str.string;
        }
    }
}
```

### EOF Packet
```c
typedef struct {
    uint8_t  header;        // 0xFE
    uint16_t warnings;
    uint16_t status_flags;
} EOFPacket;

// OK Packet (similar structure)
typedef struct {
    uint8_t  header;        // 0x00 or 0xFE
    uint64_t affected_rows; // Length-encoded
    uint64_t last_insert_id; // Length-encoded
    uint16_t status_flags;
    uint16_t warnings;
    char*    info;          // Length-encoded string
    char*    session_state_changes; // If CLIENT_SESSION_TRACK
    char*    info2;         // Additional info
} OKPacket;
```

### Error Packet
```c
typedef struct {
    uint8_t  header;        // 0xFF
    uint16_t error_code;
    char     sql_state_marker; // '#'
    char     sql_state[5];
    char*    error_message;
} ErrorPacket;

void read_error_packet(uint8_t* packet) {
    ErrorPacket err;
    err.header = packet[0];
    err.error_code = packet[1] | (packet[2] << 8);
    
    if (packet[3] == '#') {
        err.sql_state_marker = packet[3];
        memcpy(err.sql_state, packet + 4, 5);
        err.error_message = (char*)(packet + 9);
    } else {
        err.error_message = (char*)(packet + 3);
    }
    
    printf("Error %d (SQLSTATE %.*s): %s\n", 
           err.error_code, 5, err.sql_state, err.error_message);
}
```

## Prepared Statement Protocol

### Prepare Statement
```c
typedef struct {
    uint8_t command;        // COM_STMT_PREPARE (0x16)
    char    query[];        // SQL with ? placeholders
} PrepareCommand;

typedef struct {
    uint8_t  status;        // 0x00 = OK
    uint32_t statement_id;
    uint16_t num_columns;
    uint16_t num_params;
    uint8_t  reserved;      // 0x00
    uint16_t warning_count;
} PrepareOKPacket;
```

### Execute Statement
```c
typedef struct {
    uint8_t  command;       // COM_STMT_EXECUTE (0x17)
    uint32_t statement_id;
    uint8_t  flags;         // CURSOR_TYPE_*
    uint32_t iteration_count; // Always 1
    // If num_params > 0:
    uint8_t  null_bitmap[(num_params + 7) / 8];
    uint8_t  new_params_bound_flag; // 0 or 1
    // If new_params_bound_flag == 1:
    //   Type definitions for each parameter
    // Parameter values (non-NULL only)
} ExecuteCommand;

// Cursor types
#define CURSOR_TYPE_NO_CURSOR  0x00
#define CURSOR_TYPE_READ_ONLY  0x01
#define CURSOR_TYPE_FOR_UPDATE 0x02
#define CURSOR_TYPE_SCROLLABLE 0x04

// Parameter binding
typedef struct {
    uint8_t  type;          // MYSQL_TYPE_*
    uint8_t  unsigned_flag; // 0x80 if unsigned
} ParamTypeDefinition;

void bind_parameter(uint8_t** buffer, uint8_t type, void* value, size_t length) {
    switch (type) {
        case MYSQL_TYPE_TINY:
            *(*buffer)++ = *(uint8_t*)value;
            break;
        case MYSQL_TYPE_SHORT:
            *(uint16_t*)*buffer = *(uint16_t*)value;
            *buffer += 2;
            break;
        case MYSQL_TYPE_LONG:
            *(uint32_t*)*buffer = *(uint32_t*)value;
            *buffer += 4;
            break;
        case MYSQL_TYPE_LONGLONG:
            *(uint64_t*)*buffer = *(uint64_t*)value;
            *buffer += 8;
            break;
        case MYSQL_TYPE_FLOAT:
            *(float*)*buffer = *(float*)value;
            *buffer += 4;
            break;
        case MYSQL_TYPE_DOUBLE:
            *(double*)*buffer = *(double*)value;
            *buffer += 8;
            break;
        case MYSQL_TYPE_STRING:
        case MYSQL_TYPE_VAR_STRING:
        case MYSQL_TYPE_BLOB:
            write_lenenc_int(buffer, length);
            memcpy(*buffer, value, length);
            *buffer += length;
            break;
    }
}
```

### Binary Result Row
```c
typedef struct {
    uint8_t header;         // 0x00
    uint8_t null_bitmap[(column_count + 7 + 2) / 8];
    // Non-NULL values in binary format
} BinaryResultRow;

void read_binary_row(uint8_t* packet, ColumnDefinition41* columns, 
                    int column_count, void** values) {
    uint8_t* ptr = packet + 1;  // Skip header
    
    // Read null bitmap
    int bitmap_len = (column_count + 7 + 2) / 8;
    uint8_t* null_bitmap = ptr;
    ptr += bitmap_len;
    
    // Read column values
    for (int i = 0; i < column_count; i++) {
        // Check if NULL (bit position is i + 2)
        int byte_pos = (i + 2) / 8;
        int bit_pos = (i + 2) % 8;
        
        if (null_bitmap[byte_pos] & (1 << bit_pos)) {
            values[i] = NULL;
            continue;
        }
        
        // Read based on type
        switch (columns[i].type) {
            case MYSQL_TYPE_TINY:
                values[i] = malloc(1);
                *(uint8_t*)values[i] = *ptr++;
                break;
                
            case MYSQL_TYPE_SHORT:
            case MYSQL_TYPE_YEAR:
                values[i] = malloc(2);
                *(uint16_t*)values[i] = *(uint16_t*)ptr;
                ptr += 2;
                break;
                
            case MYSQL_TYPE_LONG:
            case MYSQL_TYPE_INT24:
                values[i] = malloc(4);
                *(uint32_t*)values[i] = *(uint32_t*)ptr;
                ptr += 4;
                break;
                
            case MYSQL_TYPE_LONGLONG:
                values[i] = malloc(8);
                *(uint64_t*)values[i] = *(uint64_t*)ptr;
                ptr += 8;
                break;
                
            case MYSQL_TYPE_FLOAT:
                values[i] = malloc(4);
                *(float*)values[i] = *(float*)ptr;
                ptr += 4;
                break;
                
            case MYSQL_TYPE_DOUBLE:
                values[i] = malloc(8);
                *(double*)values[i] = *(double*)ptr;
                ptr += 8;
                break;
                
            case MYSQL_TYPE_DATE:
            case MYSQL_TYPE_DATETIME:
            case MYSQL_TYPE_TIMESTAMP: {
                uint8_t length = *ptr++;
                values[i] = malloc(sizeof(MYSQL_TIME));
                MYSQL_TIME* time = (MYSQL_TIME*)values[i];
                
                if (length >= 4) {
                    time->year = ptr[0] | (ptr[1] << 8);
                    time->month = ptr[2];
                    time->day = ptr[3];
                    ptr += 4;
                }
                if (length >= 7) {
                    time->hour = ptr[0];
                    time->minute = ptr[1];
                    time->second = ptr[2];
                    ptr += 3;
                }
                if (length >= 11) {
                    time->second_part = ptr[0] | (ptr[1] << 8) | 
                                       (ptr[2] << 16) | (ptr[3] << 24);
                    ptr += 4;
                }
                break;
            }
                
            case MYSQL_TYPE_TIME: {
                uint8_t length = *ptr++;
                values[i] = malloc(sizeof(MYSQL_TIME));
                MYSQL_TIME* time = (MYSQL_TIME*)values[i];
                
                if (length >= 8) {
                    time->neg = ptr[0];
                    time->day = ptr[1] | (ptr[2] << 8) | 
                               (ptr[3] << 16) | (ptr[4] << 24);
                    time->hour = ptr[5];
                    time->minute = ptr[6];
                    time->second = ptr[7];
                    ptr += 8;
                }
                if (length >= 12) {
                    time->second_part = ptr[0] | (ptr[1] << 8) | 
                                       (ptr[2] << 16) | (ptr[3] << 24);
                    ptr += 4;
                }
                break;
            }
                
            case MYSQL_TYPE_STRING:
            case MYSQL_TYPE_VAR_STRING:
            case MYSQL_TYPE_BLOB:
            case MYSQL_TYPE_TINY_BLOB:
            case MYSQL_TYPE_MEDIUM_BLOB:
            case MYSQL_TYPE_LONG_BLOB: {
                uint64_t length = read_lenenc_int(&ptr);
                values[i] = malloc(length + 1);
                memcpy(values[i], ptr, length);
                ((char*)values[i])[length] = '\0';
                ptr += length;
                break;
            }
                
            case MYSQL_TYPE_NEWDECIMAL: {
                uint64_t length = read_lenenc_int(&ptr);
                values[i] = malloc(length + 1);
                memcpy(values[i], ptr, length);
                ((char*)values[i])[length] = '\0';
                ptr += length;
                break;
            }
        }
    }
}
```

## MySQL Data Type Encodings

### Numeric Types

#### BIT(M)
```c
// Type: MYSQL_TYPE_BIT (0x10)
// Size: (M + 7) / 8 bytes

void encode_bit(uint8_t* buffer, uint64_t value, int bits) {
    int bytes = (bits + 7) / 8;
    for (int i = 0; i < bytes; i++) {
        buffer[i] = (value >> (i * 8)) & 0xFF;
    }
}

uint64_t decode_bit(uint8_t* buffer, int bits) {
    int bytes = (bits + 7) / 8;
    uint64_t value = 0;
    for (int i = 0; i < bytes; i++) {
        value |= ((uint64_t)buffer[i]) << (i * 8);
    }
    return value & ((1ULL << bits) - 1);
}
```

#### DECIMAL/NUMERIC
```c
// Type: MYSQL_TYPE_NEWDECIMAL (0xF6)
// Binary format for DECIMAL

typedef struct {
    uint8_t precision;
    uint8_t scale;
    uint8_t sign;           // 0 = positive, 1 = negative
    uint8_t data[];         // Packed decimal digits
} MySQLDecimal;

// Decimal packing: 9 digits per 4 bytes
void pack_decimal(uint8_t* buffer, const char* decimal_str) {
    // Implementation would pack decimal digits
    // Each group of 9 digits packed into 4 bytes
}
```

### Date/Time Types

#### DATE
```c
// Type: MYSQL_TYPE_DATE (0x0A)
// Size: 3 bytes in binary protocol

typedef struct {
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
} MySQLDate;

void encode_date(uint8_t* buffer, uint16_t year, uint8_t month, uint8_t day) {
    buffer[0] = year & 0xFF;
    buffer[1] = (year >> 8) & 0xFF;
    buffer[2] = month;
    buffer[3] = day;
}
```

#### TIME
```c
// Type: MYSQL_TYPE_TIME (0x0B)
// Size: 8-12 bytes in binary protocol

typedef struct {
    uint8_t  negative;
    uint32_t days;
    uint8_t  hours;
    uint8_t  minutes;
    uint8_t  seconds;
    uint32_t microseconds;  // Optional
} MySQLTime;

void encode_time(uint8_t* buffer, MySQLTime* time, uint8_t* length) {
    if (time->days == 0 && time->hours == 0 && 
        time->minutes == 0 && time->seconds == 0 && 
        time->microseconds == 0) {
        *length = 0;
        return;
    }
    
    buffer[0] = time->negative;
    buffer[1] = time->days & 0xFF;
    buffer[2] = (time->days >> 8) & 0xFF;
    buffer[3] = (time->days >> 16) & 0xFF;
    buffer[4] = (time->days >> 24) & 0xFF;
    buffer[5] = time->hours;
    buffer[6] = time->minutes;
    buffer[7] = time->seconds;
    
    if (time->microseconds > 0) {
        buffer[8] = time->microseconds & 0xFF;
        buffer[9] = (time->microseconds >> 8) & 0xFF;
        buffer[10] = (time->microseconds >> 16) & 0xFF;
        buffer[11] = (time->microseconds >> 24) & 0xFF;
        *length = 12;
    } else {
        *length = 8;
    }
}
```

#### DATETIME
```c
// Type: MYSQL_TYPE_DATETIME (0x0C)
// Size: 0, 4, 7, or 11 bytes

typedef struct {
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
    uint32_t microsecond;
} MySQLDateTime;

void encode_datetime(uint8_t* buffer, MySQLDateTime* dt, uint8_t* length) {
    if (dt->year == 0 && dt->month == 0 && dt->day == 0) {
        *length = 0;
        return;
    }
    
    buffer[0] = dt->year & 0xFF;
    buffer[1] = (dt->year >> 8) & 0xFF;
    buffer[2] = dt->month;
    buffer[3] = dt->day;
    
    if (dt->hour == 0 && dt->minute == 0 && dt->second == 0 && 
        dt->microsecond == 0) {
        *length = 4;
        return;
    }
    
    buffer[4] = dt->hour;
    buffer[5] = dt->minute;
    buffer[6] = dt->second;
    
    if (dt->microsecond == 0) {
        *length = 7;
    } else {
        buffer[7] = dt->microsecond & 0xFF;
        buffer[8] = (dt->microsecond >> 8) & 0xFF;
        buffer[9] = (dt->microsecond >> 16) & 0xFF;
        buffer[10] = (dt->microsecond >> 24) & 0xFF;
        *length = 11;
    }
}
```

#### TIMESTAMP
```c
// Type: MYSQL_TYPE_TIMESTAMP (0x07)
// Same encoding as DATETIME
// Stored as UTC, converted to session timezone
```

#### YEAR
```c
// Type: MYSQL_TYPE_YEAR (0x0D)
// Size: 1 byte

void encode_year(uint8_t* buffer, uint16_t year) {
    buffer[0] = year - 1900;
}

uint16_t decode_year(uint8_t* buffer) {
    return buffer[0] + 1900;
}
```

### JSON Type
```c
// Type: MYSQL_TYPE_JSON (0xF5)
// Binary JSON format

typedef struct {
    uint8_t  type;          // JSON type
    uint32_t length;        // Total length
    uint8_t  data[];        // Binary JSON data
} MySQLJSON;

// JSON types in binary format
#define JSONB_TYPE_SMALL_OBJECT  0x00
#define JSONB_TYPE_LARGE_OBJECT  0x01
#define JSONB_TYPE_SMALL_ARRAY   0x02
#define JSONB_TYPE_LARGE_ARRAY   0x03
#define JSONB_TYPE_LITERAL       0x04  // true/false/null
#define JSONB_TYPE_INT16         0x05
#define JSONB_TYPE_UINT16        0x06
#define JSONB_TYPE_INT32         0x07
#define JSONB_TYPE_UINT32        0x08
#define JSONB_TYPE_INT64         0x09
#define JSONB_TYPE_UINT64        0x0A
#define JSONB_TYPE_DOUBLE        0x0B
#define JSONB_TYPE_STRING        0x0C
#define JSONB_TYPE_OPAQUE        0x0F

// JSON binary format structure
typedef struct {
    uint8_t  type;
    uint32_t element_count;  // For objects/arrays
    uint32_t size;          // Total size
    // Key entries (for objects)
    // Value entries
    // Key data (for objects)
    // Value data
} JSONBContainer;
```

### Spatial Types
```c
// Type: MYSQL_TYPE_GEOMETRY (0xFF)
// Well-Known Binary (WKB) format

typedef struct {
    uint32_t srid;          // Spatial Reference ID
    uint8_t  byte_order;    // 1 = little-endian
    uint32_t wkb_type;      // Geometry type
    // Type-specific data
} MySQLGeometry;

// WKB geometry types
#define WKB_POINT              1
#define WKB_LINESTRING         2
#define WKB_POLYGON            3
#define WKB_MULTIPOINT         4
#define WKB_MULTILINESTRING    5
#define WKB_MULTIPOLYGON       6
#define WKB_GEOMETRYCOLLECTION 7

void encode_point(uint8_t* buffer, double x, double y, uint32_t srid) {
    *(uint32_t*)buffer = srid;
    buffer += 4;
    
    *buffer++ = 1;  // Little-endian
    *(uint32_t*)buffer = WKB_POINT;
    buffer += 4;
    
    *(double*)buffer = x;
    buffer += 8;
    *(double*)buffer = y;
}
```

## MariaDB Extended Protocol

### MariaDB-Specific Features

#### Extended Type Information
```c
// MariaDB extends column metadata
typedef struct {
    // Standard MySQL fields
    ColumnDefinition41 base;
    
    // MariaDB extensions
    char*    extended_type_info;  // JSON format
    uint8_t  is_unsigned;
    uint8_t  is_zerofill;
} MariaDBColumnDefinition;
```

#### INET4 Type
```c
// MariaDB-specific type for IPv4
// Stored as 4-byte integer

void encode_inet4(uint8_t* buffer, const char* ip_str) {
    struct in_addr addr;
    inet_pton(AF_INET, ip_str, &addr);
    *(uint32_t*)buffer = addr.s_addr;
}

void decode_inet4(uint8_t* buffer, char* ip_str) {
    struct in_addr addr;
    addr.s_addr = *(uint32_t*)buffer;
    inet_ntop(AF_INET, &addr, ip_str, INET_ADDRSTRLEN);
}
```

#### INET6 Type
```c
// MariaDB-specific type for IPv6
// Stored as 16-byte binary

void encode_inet6(uint8_t* buffer, const char* ip_str) {
    struct in6_addr addr;
    inet_pton(AF_INET6, ip_str, &addr);
    memcpy(buffer, &addr, 16);
}

void decode_inet6(uint8_t* buffer, char* ip_str) {
    struct in6_addr addr;
    memcpy(&addr, buffer, 16);
    inet_ntop(AF_INET6, &addr, ip_str, INET6_ADDRSTRLEN);
}
```

#### UUID Type
```c
// MariaDB UUID type
// Stored as 16-byte binary

void encode_uuid(uint8_t* buffer, const char* uuid_str) {
    // Parse UUID string
    unsigned int data[16];
    sscanf(uuid_str, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
           &data[0], &data[1], &data[2], &data[3],
           &data[4], &data[5], &data[6], &data[7],
           &data[8], &data[9], &data[10], &data[11],
           &data[12], &data[13], &data[14], &data[15]);
    
    for (int i = 0; i < 16; i++) {
        buffer[i] = data[i];
    }
}
```

#### Dynamic Columns
```c
// MariaDB dynamic columns
// Binary format for flexible schema

typedef struct {
    uint16_t column_count;
    // For each column:
    //   uint16_t name_length
    //   char name[name_length]
    //   uint8_t type
    //   value (type-specific encoding)
} DynamicColumns;

void encode_dynamic_column(uint8_t** buffer, const char* name, 
                          uint8_t type, void* value) {
    uint16_t name_len = strlen(name);
    *(uint16_t*)*buffer = name_len;
    *buffer += 2;
    
    memcpy(*buffer, name, name_len);
    *buffer += name_len;
    
    *(*buffer)++ = type;
    
    // Encode value based on type
    switch (type) {
        case MYSQL_TYPE_LONG:
            *(uint32_t*)*buffer = *(uint32_t*)value;
            *buffer += 4;
            break;
        case MYSQL_TYPE_STRING:
            write_lenenc_string(buffer, (char*)value);
            break;
        // ... other types
    }
}
```

### MariaDB Bulk Operations
```c
// Bulk insert protocol
typedef struct {
    uint8_t  command;       // COM_STMT_BULK_EXECUTE
    uint32_t statement_id;
    uint16_t flags;
    // For each row:
    //   null_bitmap
    //   parameter values
} BulkExecuteCommand;

#define BULK_FLAG_SEND_TYPES_TO_SERVER 0x01

void execute_bulk_insert(int socket, uint32_t stmt_id, 
                         void** rows, int row_count, 
                         int param_count) {
    // Calculate packet size
    size_t packet_size = 1 + 4 + 2;  // Header
    packet_size += row_count * ((param_count + 7) / 8);  // Null bitmaps
    // Add size for actual data...
    
    uint8_t* packet = malloc(packet_size);
    uint8_t* ptr = packet;
    
    *ptr++ = 0xFA;  // COM_STMT_BULK_EXECUTE
    *(uint32_t*)ptr = stmt_id;
    ptr += 4;
    *(uint16_t*)ptr = BULK_FLAG_SEND_TYPES_TO_SERVER;
    ptr += 2;
    
    // Add rows
    for (int i = 0; i < row_count; i++) {
        // Add null bitmap
        // Add parameter values
    }
    
    write_packet(socket, packet, packet_size, 0);
    free(packet);
}
```

## Replication Protocol

### Binlog Dump
```c
typedef struct {
    uint8_t  command;       // COM_BINLOG_DUMP
    uint32_t binlog_pos;
    uint16_t flags;
    uint32_t server_id;
    char*    binlog_filename;
} BinlogDumpCommand;

// Binlog event header
typedef struct {
    uint32_t timestamp;
    uint8_t  event_type;
    uint32_t server_id;
    uint32_t event_size;
    uint32_t log_pos;
    uint16_t flags;
} BinlogEventHeader;

// Event types
#define UNKNOWN_EVENT              0
#define START_EVENT_V3             1
#define QUERY_EVENT                2
#define STOP_EVENT                 3
#define ROTATE_EVENT               4
#define INTVAR_EVENT               5
#define LOAD_EVENT                 6
#define SLAVE_EVENT                7
#define CREATE_FILE_EVENT          8
#define APPEND_BLOCK_EVENT         9
#define EXEC_LOAD_EVENT           10
#define DELETE_FILE_EVENT         11
#define NEW_LOAD_EVENT            12
#define RAND_EVENT                13
#define USER_VAR_EVENT            14
#define FORMAT_DESCRIPTION_EVENT  15
#define XID_EVENT                 16
#define BEGIN_LOAD_QUERY_EVENT    17
#define EXECUTE_LOAD_QUERY_EVENT  18
#define TABLE_MAP_EVENT           19
#define WRITE_ROWS_EVENT_V1       20
#define UPDATE_ROWS_EVENT_V1      21
#define DELETE_ROWS_EVENT_V1      22
#define INCIDENT_EVENT            26
#define HEARTBEAT_LOG_EVENT       27
#define IGNORABLE_LOG_EVENT       28
#define ROWS_QUERY_LOG_EVENT      29
#define WRITE_ROWS_EVENT          30
#define UPDATE_ROWS_EVENT         31
#define DELETE_ROWS_EVENT         32
#define GTID_LOG_EVENT            33
#define ANONYMOUS_GTID_LOG_EVENT  34
#define PREVIOUS_GTIDS_LOG_EVENT  35
```

## Compression Protocol

### Compressed Packet
```c
typedef struct {
    uint32_t compressed_length:24;
    uint8_t  compressed_seq_id;
    uint32_t uncompressed_length:24;
    uint8_t  unused;
    uint8_t  payload[];     // zlib compressed data
} CompressedPacket;

void send_compressed(int socket, uint8_t* data, uint32_t length, uint8_t seq_id) {
    // Compress data
    uLongf compressed_len = compressBound(length);
    uint8_t* compressed = malloc(compressed_len);
    
    if (compress2(compressed, &compressed_len, data, length, Z_DEFAULT_COMPRESSION) == Z_OK) {
        // Send compressed packet
        uint8_t header[7];
        header[0] = compressed_len & 0xFF;
        header[1] = (compressed_len >> 8) & 0xFF;
        header[2] = (compressed_len >> 16) & 0xFF;
        header[3] = seq_id;
        header[4] = length & 0xFF;
        header[5] = (length >> 8) & 0xFF;
        header[6] = (length >> 16) & 0xFF;
        
        send(socket, header, 7, 0);
        send(socket, compressed, compressed_len, 0);
    }
    
    free(compressed);
}
```