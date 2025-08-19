# ScratchBird API - Data Types and Conversion

## Overview

The ScratchBird Data Types and Conversion API provides comprehensive support for all database data types, automatic type conversion, SQLDA field handling, and custom type extensions. This covers standard SQL types, ScratchBird enhancements, and efficient data marshaling.

## Core Data Type Interface

### Primary Header Files
```cpp
#include <scratchbird/include/scratchbird.h>
#include <scratchbird/include/sb_sqlda.h>
#include <scratchbird/include/sb_types.h>
```

### SQLDA Field Structure

```cpp
typedef struct {
    short       sqltype;        // Data type code
    short       sqlscale;       // Scale factor for numeric types
    short       sqlsubtype;     // Subtype (charset, precision, etc.)
    short       sqllen;         // Field length in bytes
    char*       sqldata;        // Pointer to data buffer
    short*      sqlind;         // Null indicator pointer
    short       sqlname_length; // Length of field name
    char        sqlname[32];    // Field name
    short       relname_length; // Length of relation name
    char        relname[32];    // Relation (table) name
    short       ownname_length; // Length of owner name
    char        ownname[32];    // Owner (schema) name
    short       aliasname_length; // Length of alias name
    char        aliasname[32];  // Alias name
} SB_SQLVAR;
```

## SQL Data Type Constants

### Core Data Types

```cpp
// Standard SQL types
#define SQL_TEXT            452     // Fixed-length character string
#define SQL_VARYING         448     // Variable-length character string
#define SQL_SHORT           500     // 16-bit signed integer
#define SQL_LONG            496     // 32-bit signed integer
#define SQL_FLOAT           482     // 32-bit floating point
#define SQL_DOUBLE          480     // 64-bit floating point
#define SQL_D_FLOAT         530     // VAX D_FLOAT (deprecated)
#define SQL_TIMESTAMP       510     // Date and time
#define SQL_BLOB            520     // Binary Large Object
#define SQL_ARRAY           540     // Array type
#define SQL_QUAD            550     // 64-bit integer (internal)
#define SQL_TYPE_TIME       560     // Time of day
#define SQL_TYPE_DATE       570     // Date only
#define SQL_INT64           580     // 64-bit signed integer
#define SQL_BOOLEAN         32764   // Boolean type
#define SQL_NULL            32766   // NULL type

// ScratchBird extensions
#define SQL_UINT16          600     // 16-bit unsigned integer
#define SQL_UINT32          604     // 32-bit unsigned integer  
#define SQL_UINT64          608     // 64-bit unsigned integer
#define SQL_DECIMAL128      612     // 128-bit decimal
#define SQL_VECTOR          616     // Vector/array for AI/ML
#define SQL_JSON            620     // JSON data type
#define SQL_UUID            624     // UUID type
#define SQL_INET            628     // IP address type
#define SQL_POINT           632     // Geometric point
#define SQL_POLYGON         636     // Geometric polygon
#define SQL_RANGE           640     // Range data type
#define SQL_TSRANGE         644     // Timestamp range
```

### Type Modifiers

```cpp
// Null indicator modifier
#define SQL_TYPE_NULL_MASK  1       // Add to sqltype for nullable field

// Type extraction macros
#define SQL_TYPE_MASK       ~1      // Remove null indicator
#define SB_IS_NULLABLE(type) ((type) & 1)
#define SB_BASE_TYPE(type)   ((type) & SQL_TYPE_MASK)
```

## Data Conversion Functions

### Numeric Conversions

```cpp
// Integer conversions
SB_STATUS sb_convert_short(
    void*       source_data,
    short       source_type,
    short*      target_data
);

SB_STATUS sb_convert_long(
    void*       source_data,
    short       source_type,
    SB_SLONG*   target_data
);

SB_STATUS sb_convert_int64(
    void*       source_data,
    short       source_type,
    SB_INT64*   target_data
);

// Floating point conversions
SB_STATUS sb_convert_float(
    void*       source_data,
    short       source_type,
    float*      target_data
);

SB_STATUS sb_convert_double(
    void*       source_data,
    short       source_type,
    double*     target_data
);
```

**Example - Numeric Conversion:**
```cpp
SB_SQLVAR* field = &output_sqlda->sqlvar[0];
SB_INT64 integer_value;
double double_value;

// Convert any numeric type to 64-bit integer
if (sb_convert_int64(field->sqldata, field->sqltype, &integer_value) == 0) {
    printf("Integer value: %lld\n", integer_value);
}

// Convert to double
if (sb_convert_double(field->sqldata, field->sqltype, &double_value) == 0) {
    printf("Double value: %.6f\n", double_value);
}
```

### String Conversions

```cpp
// String conversion with character set handling
SB_STATUS sb_convert_string(
    void*           source_data,
    short           source_type,
    short           source_charset,
    char*           target_buffer,
    short           target_length,
    short           target_charset,
    short*          actual_length
);

// Varying string extraction
SB_STATUS sb_extract_varying_string(
    char*           varying_data,
    char*           target_buffer,
    short           buffer_size,
    short*          string_length
);
```

**Example - String Extraction:**
```cpp
SB_SQLVAR* field = &output_sqlda->sqlvar[1];
char string_buffer[256];
short string_length;

if (SB_BASE_TYPE(field->sqltype) == SQL_VARYING) {
    // Extract from VARCHAR field
    if (sb_extract_varying_string(field->sqldata, string_buffer,
                                 sizeof(string_buffer), &string_length) == 0) {
        string_buffer[string_length] = '\0';
        printf("String value: '%s' (length: %d)\n", string_buffer, string_length);
    }
} else if (SB_BASE_TYPE(field->sqltype) == SQL_TEXT) {
    // Extract from CHAR field
    string_length = field->sqllen;
    memcpy(string_buffer, field->sqldata, string_length);
    
    // Trim trailing spaces
    while (string_length > 0 && string_buffer[string_length - 1] == ' ') {
        string_length--;
    }
    string_buffer[string_length] = '\0';
    printf("String value: '%s'\n", string_buffer);
}
```

### Date/Time Conversions

```cpp
// Date/time structures
typedef struct {
    int year;
    int month;      // 1-12
    int day;        // 1-31
} SB_DATE;

typedef struct {
    int hour;       // 0-23
    int minute;     // 0-59
    int second;     // 0-59
    int fraction;   // 0-99999999 (100ns units)
} SB_TIME;

typedef struct {
    SB_DATE date;
    SB_TIME time;
    short   timezone; // Minutes from UTC
} SB_TIMESTAMP;

// Conversion functions
SB_STATUS sb_decode_date(
    const SB_DATE*      encoded_date,
    SB_DATE*           decoded_date
);

SB_STATUS sb_encode_date(
    const SB_DATE*      decoded_date,
    SB_DATE*           encoded_date
);

SB_STATUS sb_decode_time(
    const SB_TIME*      encoded_time,
    SB_TIME*           decoded_time
);

SB_STATUS sb_encode_time(
    const SB_TIME*      decoded_time,
    SB_TIME*           encoded_time
);

SB_STATUS sb_decode_timestamp(
    const SB_TIMESTAMP* encoded_timestamp,
    SB_TIMESTAMP*      decoded_timestamp
);
```

**Example - Date/Time Conversion:**
```cpp
SB_SQLVAR* field = &output_sqlda->sqlvar[2];
SB_TIMESTAMP timestamp;
char formatted_date[32];

if (SB_BASE_TYPE(field->sqltype) == SQL_TIMESTAMP) {
    // Decode timestamp from database format
    if (sb_decode_timestamp((SB_TIMESTAMP*)field->sqldata, &timestamp) == 0) {
        snprintf(formatted_date, sizeof(formatted_date),
                "%04d-%02d-%02d %02d:%02d:%02d",
                timestamp.date.year, timestamp.date.month, timestamp.date.day,
                timestamp.time.hour, timestamp.time.minute, timestamp.time.second);
        printf("Timestamp: %s\n", formatted_date);
    }
}
```

## BLOB Handling

### BLOB Operations

```cpp
// BLOB handle
typedef void* SB_BLOB_HANDLE;

// BLOB information structure
typedef struct {
    SB_QUAD     blob_id;        // BLOB identifier
    short       blob_type;      // 0=binary, 1=text
    short       charset;        // Character set for text BLOBs
    SB_INT64    blob_size;      // Total size in bytes
    short       segment_count;  // Number of segments
    short       max_segment;    // Largest segment size
} SB_BLOB_INFO;

// BLOB functions
SB_STATUS sb_open_blob2(
    SB_STATUS*          status_vector,
    SB_DB_HANDLE        db_handle,
    SB_TR_HANDLE        tr_handle,
    SB_BLOB_HANDLE*     blob_handle,
    SB_QUAD*            blob_id,
    short               bpb_length,
    const unsigned char* bpb_buffer
);

SB_STATUS sb_create_blob2(
    SB_STATUS*          status_vector,
    SB_DB_HANDLE        db_handle,
    SB_TR_HANDLE        tr_handle,
    SB_BLOB_HANDLE*     blob_handle,
    SB_QUAD*            blob_id,
    short               bpb_length,
    const unsigned char* bpb_buffer
);

SB_STATUS sb_get_segment(
    SB_STATUS*          status_vector,
    SB_BLOB_HANDLE      blob_handle,
    unsigned short*     actual_length,
    unsigned short      buffer_length,
    char*               buffer
);

SB_STATUS sb_put_segment(
    SB_STATUS*          status_vector,
    SB_BLOB_HANDLE      blob_handle,
    unsigned short      segment_length,
    const char*         segment
);

SB_STATUS sb_close_blob(
    SB_STATUS*          status_vector,
    SB_BLOB_HANDLE*     blob_handle
);
```

**Example - BLOB Reading:**
```cpp
SB_STATUS status[20];
SB_BLOB_HANDLE blob = 0;
SB_QUAD* blob_id;
char buffer[1024];
unsigned short actual_length;
FILE* output_file;

// Get BLOB ID from SQLDA
SB_SQLVAR* field = &output_sqlda->sqlvar[3];
if (SB_BASE_TYPE(field->sqltype) == SQL_BLOB) {
    blob_id = (SB_QUAD*)field->sqldata;
    
    // Open BLOB for reading
    if (sb_open_blob2(status, db, transaction, &blob, blob_id, 0, NULL)) {
        sb_print_status(status);
        return;
    }
    
    output_file = fopen("extracted_blob.dat", "wb");
    
    // Read BLOB in segments
    while (true) {
        int result = sb_get_segment(status, blob, &actual_length,
                                   sizeof(buffer), buffer);
        
        if (result == sb_segment_eof) {
            break; // End of BLOB
        } else if (result != 0 && result != sb_segstr_eof) {
            sb_print_status(status);
            break;
        }
        
        fwrite(buffer, 1, actual_length, output_file);
    }
    
    fclose(output_file);
    sb_close_blob(status, &blob);
    printf("BLOB extracted successfully\n");
}
```

## Array Handling

### Array Descriptors

```cpp
// Array descriptor
typedef struct {
    unsigned char   array_desc_dtype;       // Data type
    char            array_desc_scale;       // Scale factor
    unsigned short  array_desc_length;     // Element length
    char            array_desc_field_name[32]; // Field name
    char            array_desc_relation_name[32]; // Table name
    short           array_desc_dimensions;  // Number of dimensions
    short           array_desc_flags;       // Array flags
    struct {
        short       array_bound_lower;      // Lower bound
        short       array_bound_upper;      // Upper bound
    } array_desc_bounds[16];                // Up to 16 dimensions
} SB_ARRAY_DESC;

// Array operations
SB_STATUS sb_array_get_slice(
    SB_STATUS*          status_vector,
    SB_DB_HANDLE        db_handle,
    SB_TR_HANDLE        tr_handle,
    SB_QUAD*            array_id,
    SB_ARRAY_DESC*      descriptor,
    void*               destination_array,
    SB_SLONG*           slice_length
);

SB_STATUS sb_array_put_slice(
    SB_STATUS*          status_vector,
    SB_DB_HANDLE        db_handle,
    SB_TR_HANDLE        tr_handle,
    SB_QUAD*            array_id,
    SB_ARRAY_DESC*      descriptor,
    void*               source_array,
    SB_SLONG*           slice_length
);
```

**Example - Array Processing:**
```cpp
SB_STATUS status[20];
SB_ARRAY_DESC array_desc;
SB_QUAD* array_id;
int array_data[100]; // Assuming integer array
SB_SLONG slice_length;

// Get array information from SQLDA
SB_SQLVAR* field = &output_sqlda->sqlvar[4];
if (SB_BASE_TYPE(field->sqltype) == SQL_ARRAY) {
    array_id = (SB_QUAD*)field->sqldata;
    
    // Get array descriptor (would be obtained from system tables)
    // ... populate array_desc ...
    
    // Read entire array
    if (sb_array_get_slice(status, db, transaction, array_id,
                          &array_desc, array_data, &slice_length) == 0) {
        
        int element_count = slice_length / sizeof(int);
        printf("Array contains %d elements:\n", element_count);
        
        for (int i = 0; i < element_count; i++) {
            printf("  [%d] = %d\n", i, array_data[i]);
        }
    }
}
```

## ScratchBird Extended Types

### UUID Type

```cpp
// UUID structure
typedef struct {
    unsigned char uuid_bytes[16];
} SB_UUID;

// UUID functions
SB_STATUS sb_generate_uuid(SB_UUID* uuid);
SB_STATUS sb_uuid_from_string(const char* uuid_string, SB_UUID* uuid);
SB_STATUS sb_uuid_to_string(const SB_UUID* uuid, char* uuid_string);
```

### JSON Type

```cpp
// JSON handling functions
SB_STATUS sb_json_parse(
    const char*     json_string,
    void**          json_object
);

SB_STATUS sb_json_extract_path(
    void*           json_object,
    const char*     json_path,
    char*           result_buffer,
    int             buffer_size
);

SB_STATUS sb_json_to_string(
    void*           json_object,
    char*           result_buffer,
    int             buffer_size
);
```

### Vector Type (AI/ML)

```cpp
// Vector structure for AI/ML operations
typedef struct {
    float*      elements;       // Vector elements
    int         dimension;      // Number of dimensions
    int         vector_type;    // Vector type (dense, sparse)
} SB_VECTOR;

// Vector operations
SB_STATUS sb_vector_from_array(
    float*          source_array,
    int             dimension,
    SB_VECTOR*      vector
);

SB_STATUS sb_vector_distance(
    const SB_VECTOR*    vector1,
    const SB_VECTOR*    vector2,
    int                 distance_type,  // Euclidean, cosine, etc.
    float*              distance
);
```

### Network Types

```cpp
// INET type for IP addresses
typedef struct {
    unsigned char   family;         // IPv4=4, IPv6=6
    unsigned char   bits;           // Network mask bits
    unsigned char   address[16];    // IP address bytes
} SB_INET;

// MAC address type
typedef struct {
    unsigned char   mac_bytes[6];
} SB_MACADDR;

// Network type conversion functions
SB_STATUS sb_inet_from_string(const char* ip_string, SB_INET* inet);
SB_STATUS sb_inet_to_string(const SB_INET* inet, char* ip_string);
SB_STATUS sb_macaddr_from_string(const char* mac_string, SB_MACADDR* macaddr);
SB_STATUS sb_macaddr_to_string(const SB_MACADDR* macaddr, char* mac_string);
```

## Type Introspection

### Field Information

```cpp
// Get detailed field information
typedef struct {
    short           field_type;         // Base data type
    short           field_scale;        // Numeric scale
    short           field_precision;    // Numeric precision
    short           field_length;       // Field length
    short           field_charset;      // Character set
    short           field_collation;    // Collation sequence
    char            field_name[64];     // Field name
    char            type_name[32];      // Human-readable type name
    bool            nullable;           // Can be NULL
    bool            has_default;        // Has default value
    char            default_value[256]; // Default value (if any)
} SB_FIELD_INFO;

SB_STATUS sb_get_field_info(
    SB_SQLVAR*      sqlvar,
    SB_FIELD_INFO*  field_info
);
```

**Example - Field Introspection:**
```cpp
SB_FIELD_INFO field_info;
char type_description[128];

for (int i = 0; i < output_sqlda->sqld; i++) {
    SB_SQLVAR* field = &output_sqlda->sqlvar[i];
    
    if (sb_get_field_info(field, &field_info) == 0) {
        printf("Field %d: %s\n", i + 1, field_info.field_name);
        printf("  Type: %s", field_info.type_name);
        
        if (field_info.field_precision > 0) {
            printf("(%d", field_info.field_precision);
            if (field_info.field_scale != 0) {
                printf(",%d", field_info.field_scale);
            }
            printf(")");
        } else if (field_info.field_length > 0) {
            printf("(%d)", field_info.field_length);
        }
        
        if (field_info.nullable) {
            printf(" NULL");
        } else {
            printf(" NOT NULL");
        }
        
        if (field_info.has_default) {
            printf(" DEFAULT %s", field_info.default_value);
        }
        
        printf("\n");
    }
}
```

## Parameter Binding

### Input Parameter Setup

```cpp
// Helper function to set up input parameters
SB_STATUS sb_set_parameter(
    SB_SQLVAR*      param,
    short           data_type,
    void*           data_value,
    short           data_length,
    bool            is_null
);

// Specific parameter setters
SB_STATUS sb_set_string_parameter(
    SB_SQLVAR*      param,
    const char*     string_value
);

SB_STATUS sb_set_integer_parameter(
    SB_SQLVAR*      param,
    SB_INT64        integer_value
);

SB_STATUS sb_set_double_parameter(
    SB_SQLVAR*      param,
    double          double_value
);

SB_STATUS sb_set_timestamp_parameter(
    SB_SQLVAR*      param,
    const SB_TIMESTAMP* timestamp_value
);
```

**Example - Parameter Binding:**
```cpp
SB_SQLDA* input_sqlda;
SB_TIMESTAMP current_time;

// Allocate input SQLDA for 3 parameters
input_sqlda = (SB_SQLDA*)malloc(SB_SQLDA_LENGTH(3));
input_sqlda->sqln = 3;
input_sqlda->sqld = 3;
input_sqlda->version = SB_SQLDA_VERSION1;

// Parameter 1: Customer name (VARCHAR)
sb_set_string_parameter(&input_sqlda->sqlvar[0], "ACME Corporation");

// Parameter 2: Credit limit (DECIMAL)
sb_set_double_parameter(&input_sqlda->sqlvar[1], 50000.00);

// Parameter 3: Registration date (TIMESTAMP)
sb_get_current_timestamp(&current_time);
sb_set_timestamp_parameter(&input_sqlda->sqlvar[2], &current_time);

// Execute prepared statement with parameters
sb_dsql_execute(status, transaction, stmt, 4, input_sqlda);
```

## Memory Management

### SQLDA Memory Management

```cpp
// Allocate SQLDA with proper sizing
SB_SQLDA* sb_allocate_sqlda(int field_count);

// Allocate data buffers for SQLDA fields
SB_STATUS sb_allocate_sqlda_buffers(SB_SQLDA* sqlda);

// Free SQLDA and all associated memory
void sb_free_sqlda(SB_SQLDA* sqlda);

// Reallocate SQLDA for different field count
SB_SQLDA* sb_reallocate_sqlda(SB_SQLDA* old_sqlda, int new_field_count);
```

**Example - Complete SQLDA Management:**
```cpp
SB_SQLDA* manage_sqlda_lifecycle(SB_STMT_HANDLE stmt) {
    SB_STATUS status[20];
    SB_SQLDA* sqlda = NULL;
    
    // Initial allocation (unknown field count)
    sqlda = sb_allocate_sqlda(0);
    
    // Describe to get actual field count
    if (sb_dsql_describe(status, stmt, 4, sqlda)) {
        sb_free_sqlda(sqlda);
        return NULL;
    }
    
    // Reallocate for actual field count
    if (sqlda->sqld > sqlda->sqln) {
        sqlda = sb_reallocate_sqlda(sqlda, sqlda->sqld);
        
        // Describe again with correct size
        if (sb_dsql_describe(status, stmt, 4, sqlda)) {
            sb_free_sqlda(sqlda);
            return NULL;
        }
    }
    
    // Allocate data buffers for all fields
    if (sb_allocate_sqlda_buffers(sqlda)) {
        sb_free_sqlda(sqlda);
        return NULL;
    }
    
    return sqlda;
}
```

## Complete Data Handling Example

```cpp
#include <scratchbird/include/scratchbird.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void process_result_set(SB_DB_HANDLE db, SB_TR_HANDLE tr) {
    SB_STATUS status[20];
    SB_STMT_HANDLE stmt = 0;
    SB_SQLDA* output_sqlda = NULL;
    
    char sql[] = "SELECT customer_id, customer_name, registration_date, "
                 "credit_limit, profile_image, tags, location "
                 "FROM customers WHERE region = ?";
    
    // Allocate and prepare statement
    sb_dsql_allocate_statement(status, db, &stmt);
    
    output_sqlda = sb_allocate_sqlda(10); // Initial guess
    
    if (sb_dsql_prepare(status, tr, stmt, 0, sql, 4, output_sqlda)) {
        sb_print_status(status);
        goto cleanup;
    }
    
    // Reallocate if needed
    if (output_sqlda->sqld > output_sqlda->sqln) {
        output_sqlda = sb_reallocate_sqlda(output_sqlda, output_sqlda->sqld);
        sb_dsql_describe(status, stmt, 4, output_sqlda);
    }
    
    // Allocate output buffers
    sb_allocate_sqlda_buffers(output_sqlda);
    
    // Set up input parameter (region)
    SB_SQLDA* input_sqlda = sb_allocate_sqlda(1);
    sb_set_string_parameter(&input_sqlda->sqlvar[0], "NORTH");
    
    // Execute query
    sb_dsql_execute(status, tr, stmt, 4, input_sqlda);
    
    // Process results
    int row_count = 0;
    printf("Customer Report:\n");
    printf("================\n");
    
    while (true) {
        int fetch_result = sb_dsql_fetch(status, stmt, 4, output_sqlda);
        
        if (fetch_result == 100) break;
        if (fetch_result != 0) {
            sb_print_status(status);
            break;
        }
        
        row_count++;
        printf("\nCustomer %d:\n", row_count);
        
        // Process each field based on its type
        for (int i = 0; i < output_sqlda->sqld; i++) {
            SB_SQLVAR* field = &output_sqlda->sqlvar[i];
            
            printf("  %s: ", field->sqlname);
            
            // Check for NULL
            if (*field->sqlind == -1) {
                printf("NULL\n");
                continue;
            }
            
            switch (SB_BASE_TYPE(field->sqltype)) {
                case SQL_LONG:
                case SQL_SHORT: {
                    SB_INT64 int_value;
                    sb_convert_int64(field->sqldata, field->sqltype, &int_value);
                    printf("%lld\n", int_value);
                    break;
                }
                
                case SQL_INT64: {
                    SB_INT64 value = *(SB_INT64*)field->sqldata;
                    if (field->sqlscale != 0) {
                        // Scaled decimal
                        double decimal_value = value / pow(10, -field->sqlscale);
                        printf("%.2f\n", decimal_value);
                    } else {
                        printf("%lld\n", value);
                    }
                    break;
                }
                
                case SQL_VARYING: {
                    short length = *(short*)field->sqldata;
                    char* str_data = field->sqldata + 2;
                    printf("'%.*s'\n", length, str_data);
                    break;
                }
                
                case SQL_TEXT: {
                    printf("'%.*s'\n", field->sqllen, field->sqldata);
                    break;
                }
                
                case SQL_TIMESTAMP: {
                    SB_TIMESTAMP timestamp;
                    sb_decode_timestamp((SB_TIMESTAMP*)field->sqldata, &timestamp);
                    printf("%04d-%02d-%02d %02d:%02d:%02d\n",
                           timestamp.date.year, timestamp.date.month, timestamp.date.day,
                           timestamp.time.hour, timestamp.time.minute, timestamp.time.second);
                    break;
                }
                
                case SQL_BLOB: {
                    SB_QUAD* blob_id = (SB_QUAD*)field->sqldata;
                    printf("BLOB ID: %lld/%lld\n", 
                           (long long)blob_id->gds_quad_high,
                           (long long)blob_id->gds_quad_low);
                    break;
                }
                
                case SQL_ARRAY: {
                    SB_QUAD* array_id = (SB_QUAD*)field->sqldata;
                    printf("ARRAY ID: %lld/%lld\n",
                           (long long)array_id->gds_quad_high, 
                           (long long)array_id->gds_quad_low);
                    break;
                }
                
                case SQL_JSON: {
                    // ScratchBird JSON extension
                    printf("JSON: %.*s\n", field->sqllen, field->sqldata);
                    break;
                }
                
                default:
                    printf("(Unknown type %d)\n", field->sqltype);
                    break;
            }
        }
    }
    
    printf("\nTotal customers: %d\n", row_count);
    
cleanup:
    if (output_sqlda) sb_free_sqlda(output_sqlda);
    if (input_sqlda) sb_free_sqlda(input_sqlda);
    if (stmt) sb_dsql_free_statement(status, &stmt, SB_DROP);
}
```

## Implementation Files

### Core Type System
- `src/include/types_pub.h` - Public type definitions
- `src/common/dsc.cpp` - Data type descriptors
- `src/dsql/ddl.cpp` - DDL type processing
- `src/dsql/array.epp` - Array type handling

### Data Conversion
- `src/common/cvt.cpp` - Data conversion routines
- `src/common/cvt2.cpp` - Extended conversion functions
- `src/common/classes/BaseStream.cpp` - Stream-based conversions

### BLOB and Array Support
- `src/jrd/blob.cpp` - BLOB operations
- `src/jrd/array.cpp` - Array operations  
- `src/common/classes/Blob.cpp` - BLOB utility classes

### ScratchBird Extensions
- `src/common/classes/UUID.cpp` - UUID type implementation
- `src/common/classes/Json.cpp` - JSON type support
- `src/common/classes/Vector.cpp` - Vector/AI types
- `src/common/classes/Network.cpp` - Network types (INET, MACADDR)

---

*This documentation covers the complete ScratchBird data types and conversion API. See related API documentation for connection management, statement execution, transaction control, and error handling.*