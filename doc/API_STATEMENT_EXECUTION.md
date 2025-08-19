# ScratchBird API - Statement Execution

## Overview

The ScratchBird Statement Execution API provides comprehensive SQL statement preparation, execution, and result set handling. This covers both immediate execution for simple queries and prepared statements for optimal performance.

## Core Statement Interface

### Primary Header Files
```cpp
#include <scratchbird/include/scratchbird.h>
#include <scratchbird/include/sbclient_pub.h>
#include <scratchbird/include/sb_sqlda.h>
```

### Statement Handle Structure

```cpp
typedef struct {
    void* stmt_handle;
    SB_DB_HANDLE db_handle;
    SB_TR_HANDLE tr_handle;
    char* sql_text;
    short sql_dialect;
    SB_SQLDA* input_sqlda;
    SB_SQLDA* output_sqlda;
    int statement_type;
    int cursor_type;
    bool prepared;
    SB_ERROR_INFO last_error;
} SB_STATEMENT;
```

## Statement Preparation

### sb_dsql_allocate_statement()
Allocates a new statement handle for SQL execution.

```cpp
SB_STATUS sb_dsql_allocate_statement(
    SB_STATUS*      status_vector,
    SB_DB_HANDLE    db_handle,
    SB_STMT_HANDLE* stmt_handle
);
```

**Example:**
```cpp
SB_STATUS status[20];
SB_STMT_HANDLE stmt = 0;

if (sb_dsql_allocate_statement(status, db, &stmt)) {
    sb_print_status(status);
    return;
}
```

### sb_dsql_prepare()
Prepares an SQL statement for execution.

```cpp
SB_STATUS sb_dsql_prepare(
    SB_STATUS*      status_vector,
    SB_TR_HANDLE    tr_handle,
    SB_STMT_HANDLE  stmt_handle,
    unsigned short  length,
    const char*     string,
    unsigned short  dialect,
    SB_SQLDA*       sqlda
);
```

**Parameters:**
- `tr_handle`: Transaction handle
- `stmt_handle`: Statement handle
- `length`: SQL string length (0 for null-terminated)
- `string`: SQL statement text
- `dialect`: SQL dialect (1-4)
- `sqlda`: Output SQLDA for SELECT statements

**Example:**
```cpp
SB_STATUS status[20];
SB_SQLDA* sqlda = NULL;
char sql[] = "SELECT customer_id, customer_name, credit_limit "
             "FROM customers WHERE region = ?";

// Allocate output SQLDA
sqlda = (SB_SQLDA*)malloc(SB_SQLDA_LENGTH(3));
sqlda->sqln = 3;
sqlda->version = SB_SQLDA_VERSION1;

if (sb_dsql_prepare(status, transaction, stmt, 0, sql, 4, sqlda)) {
    sb_print_status(status);
    return;
}

printf("Statement prepared successfully\n");
printf("Output columns: %d\n", sqlda->sqld);
```

### sb_dsql_describe()
Retrieves information about statement parameters or result columns.

```cpp
SB_STATUS sb_dsql_describe(
    SB_STATUS*      status_vector,
    SB_STMT_HANDLE  stmt_handle,
    unsigned short  dialect,
    SB_SQLDA*       sqlda
);

SB_STATUS sb_dsql_describe_bind(
    SB_STATUS*      status_vector,
    SB_STMT_HANDLE  stmt_handle,
    unsigned short  dialect,
    SB_SQLDA*       sqlda
);
```

**Example:**
```cpp
SB_STATUS status[20];
SB_SQLDA* input_sqlda = NULL;

// Get input parameter count first
input_sqlda = (SB_SQLDA*)malloc(SB_SQLDA_LENGTH(0));
input_sqlda->sqln = 0;
input_sqlda->version = SB_SQLDA_VERSION1;

sb_dsql_describe_bind(status, stmt, 4, input_sqlda);

if (input_sqlda->sqld > 0) {
    // Reallocate for actual parameter count
    input_sqlda = (SB_SQLDA*)realloc(input_sqlda, 
                                    SB_SQLDA_LENGTH(input_sqlda->sqld));
    input_sqlda->sqln = input_sqlda->sqld;
    
    // Get parameter descriptions
    if (sb_dsql_describe_bind(status, stmt, 4, input_sqlda)) {
        sb_print_status(status);
        return;
    }
}
```

## Statement Execution

### sb_dsql_execute()
Executes a prepared statement (non-SELECT).

```cpp
SB_STATUS sb_dsql_execute(
    SB_STATUS*      status_vector,
    SB_TR_HANDLE    tr_handle,
    SB_STMT_HANDLE  stmt_handle,
    unsigned short  dialect,
    SB_SQLDA*       sqlda
);
```

**Example - INSERT with Parameters:**
```cpp
SB_STATUS status[20];
SB_SQLDA* input_sqlda;
char sql[] = "INSERT INTO customers (customer_name, region, credit_limit) "
             "VALUES (?, ?, ?)";

// Prepare statement
sb_dsql_prepare(status, transaction, stmt, 0, sql, 4, NULL);

// Set up input parameters
input_sqlda = (SB_SQLDA*)malloc(SB_SQLDA_LENGTH(3));
input_sqlda->sqln = 3;
input_sqlda->sqld = 3;
input_sqlda->version = SB_SQLDA_VERSION1;

// Parameter 1: customer_name (VARCHAR)
input_sqlda->sqlvar[0].sqltype = SQL_VARYING + 1; // nullable
input_sqlda->sqlvar[0].sqllen = 100;
input_sqlda->sqlvar[0].sqldata = malloc(102); // length + data
*(short*)input_sqlda->sqlvar[0].sqldata = strlen("ACME Corp");
strcpy(input_sqlda->sqlvar[0].sqldata + 2, "ACME Corp");
input_sqlda->sqlvar[0].sqlind = malloc(sizeof(short));
*input_sqlda->sqlvar[0].sqlind = 0; // not null

// Parameter 2: region (VARCHAR)
input_sqlda->sqlvar[1].sqltype = SQL_VARYING + 1;
input_sqlda->sqlvar[1].sqllen = 50;
input_sqlda->sqlvar[1].sqldata = malloc(52);
*(short*)input_sqlda->sqlvar[1].sqldata = strlen("NORTH");
strcpy(input_sqlda->sqlvar[1].sqldata + 2, "NORTH");
input_sqlda->sqlvar[1].sqlind = malloc(sizeof(short));
*input_sqlda->sqlvar[1].sqlind = 0;

// Parameter 3: credit_limit (DECIMAL)
input_sqlda->sqlvar[2].sqltype = SQL_INT64 + 1;
input_sqlda->sqlvar[2].sqllen = sizeof(SB_INT64);
input_sqlda->sqlvar[2].sqlscale = -2; // 2 decimal places
input_sqlda->sqlvar[2].sqldata = malloc(sizeof(SB_INT64));
*(SB_INT64*)input_sqlda->sqlvar[2].sqldata = 5000000; // 50000.00
input_sqlda->sqlvar[2].sqlind = malloc(sizeof(short));
*input_sqlda->sqlvar[2].sqlind = 0;

// Execute statement
if (sb_dsql_execute(status, transaction, stmt, 4, input_sqlda)) {
    sb_print_status(status);
} else {
    printf("Customer inserted successfully\n");
}
```

### sb_dsql_execute2()
Executes a prepared statement with both input and output parameters.

```cpp
SB_STATUS sb_dsql_execute2(
    SB_STATUS*      status_vector,
    SB_TR_HANDLE    tr_handle,
    SB_STMT_HANDLE  stmt_handle,
    unsigned short  dialect,
    SB_SQLDA*       in_sqlda,
    SB_SQLDA*       out_sqlda
);
```

**Example - RETURNING Clause:**
```cpp
SB_STATUS status[20];
SB_SQLDA* input_sqlda;
SB_SQLDA* output_sqlda;
char sql[] = "INSERT INTO orders (customer_id, order_date, amount) "
             "VALUES (?, CURRENT_DATE, ?) "
             "RETURNING order_id, order_date";

// Prepare statement
sb_dsql_prepare(status, transaction, stmt, 0, sql, 4, NULL);

// Set up input parameters (customer_id, amount)
// ... input_sqlda setup ...

// Set up output parameters (order_id, order_date)
output_sqlda = (SB_SQLDA*)malloc(SB_SQLDA_LENGTH(2));
output_sqlda->sqln = 2;
output_sqlda->version = SB_SQLDA_VERSION1;

// Describe output columns
sb_dsql_describe(status, stmt, 4, output_sqlda);

// Allocate output buffers
for (int i = 0; i < output_sqlda->sqld; i++) {
    SB_SQLVAR* var = &output_sqlda->sqlvar[i];
    var->sqldata = malloc(var->sqllen);
    var->sqlind = malloc(sizeof(short));
}

// Execute with input and output
if (sb_dsql_execute2(status, transaction, stmt, 4, 
                     input_sqlda, output_sqlda)) {
    sb_print_status(status);
} else {
    // Process returned values
    int order_id = *(int*)output_sqlda->sqlvar[0].sqldata;
    printf("New order ID: %d\n", order_id);
}
```

## Cursor Operations

### sb_dsql_set_cursor_name()
Assigns a name to a cursor for positioned operations.

```cpp
SB_STATUS sb_dsql_set_cursor_name(
    SB_STATUS*      status_vector,
    SB_STMT_HANDLE  stmt_handle,
    const char*     cursor_name,
    unsigned short  type
);
```

### sb_dsql_fetch()
Fetches the next row from a SELECT statement result set.

```cpp
SB_STATUS sb_dsql_fetch(
    SB_STATUS*      status_vector,
    SB_STMT_HANDLE  stmt_handle,
    unsigned short  dialect,
    SB_SQLDA*       sqlda
);
```

**Complete SELECT Example:**
```cpp
SB_STATUS status[20];
SB_SQLDA* output_sqlda;
char sql[] = "SELECT customer_id, customer_name, credit_limit "
             "FROM customers WHERE region = ? ORDER BY customer_name";

// Prepare statement
sb_dsql_prepare(status, transaction, stmt, 0, sql, 4, NULL);

// Set up input parameter (region)
// ... input parameter setup ...

// Execute query
sb_dsql_execute(status, transaction, stmt, 4, input_sqlda);

// Set up output SQLDA
output_sqlda = (SB_SQLDA*)malloc(SB_SQLDA_LENGTH(3));
output_sqlda->sqln = 3;
output_sqlda->version = SB_SQLDA_VERSION1;

// Describe output columns
sb_dsql_describe(status, stmt, 4, output_sqlda);

// Allocate output buffers
for (int i = 0; i < output_sqlda->sqld; i++) {
    SB_SQLVAR* var = &output_sqlda->sqlvar[i];
    var->sqldata = malloc(var->sqllen);
    var->sqlind = malloc(sizeof(short));
}

// Fetch all rows
int row_count = 0;
while (true) {
    int fetch_result = sb_dsql_fetch(status, stmt, 4, output_sqlda);
    
    if (fetch_result == 100) {
        // No more rows
        break;
    } else if (fetch_result != 0) {
        // Error occurred
        sb_print_status(status);
        break;
    }
    
    // Process current row
    row_count++;
    
    // Get customer_id (INTEGER)
    int customer_id = *(int*)output_sqlda->sqlvar[0].sqldata;
    
    // Get customer_name (VARCHAR)
    short name_length = *(short*)output_sqlda->sqlvar[1].sqldata;
    char customer_name[101];
    strncpy(customer_name, output_sqlda->sqlvar[1].sqldata + 2, name_length);
    customer_name[name_length] = '\0';
    
    // Get credit_limit (DECIMAL)
    SB_INT64 credit_raw = *(SB_INT64*)output_sqlda->sqlvar[2].sqldata;
    double credit_limit = credit_raw / 100.0; // scale -2
    
    printf("Customer %d: %s, Credit: %.2f\n", 
           customer_id, customer_name, credit_limit);
}

printf("Total rows fetched: %d\n", row_count);
```

## Immediate Execution

### sb_dsql_execute_immediate()
Executes an SQL statement immediately without preparation.

```cpp
SB_STATUS sb_dsql_execute_immediate(
    SB_STATUS*      status_vector,
    SB_DB_HANDLE    db_handle,
    SB_TR_HANDLE    tr_handle,
    unsigned short  length,
    const char*     string,
    unsigned short  dialect,
    SB_SQLDA*       sqlda
);
```

**Example:**
```cpp
SB_STATUS status[20];
char sql[] = "UPDATE customers SET last_updated = CURRENT_TIMESTAMP "
             "WHERE region = 'INACTIVE'";

if (sb_dsql_execute_immediate(status, db, transaction, 
                              0, sql, 4, NULL)) {
    sb_print_status(status);
} else {
    printf("Batch update completed\n");
}
```

## Statement Information

### sb_dsql_sql_info()
Retrieves information about a prepared statement.

```cpp
SB_STATUS sb_dsql_sql_info(
    SB_STATUS*      status_vector,
    SB_STMT_HANDLE  stmt_handle,
    short           item_list_length,
    const char*     item_list,
    short           buffer_length,
    char*           buffer
);
```

**Information Items:**
```cpp
sb_info_sql_select          // Statement is SELECT
sb_info_sql_insert          // Statement is INSERT
sb_info_sql_update          // Statement is UPDATE
sb_info_sql_delete          // Statement is DELETE
sb_info_sql_ddl             // Statement is DDL
sb_info_sql_get_plan        // Get query execution plan
sb_info_sql_records         // Number of affected records
sb_info_sql_stmt_type       // Statement type code
sb_info_sql_batch_fetch     // Batch fetch capability
```

**Example:**
```cpp
SB_STATUS status[20];
char info_buffer[1024];
char request[] = {
    sb_info_sql_stmt_type,
    sb_info_sql_get_plan,
    sb_info_sql_records,
    sb_info_end
};

if (!sb_dsql_sql_info(status, stmt, sizeof(request), request,
                      sizeof(info_buffer), info_buffer)) {
    // Parse statement information
    sb_parse_sql_info(info_buffer, sizeof(info_buffer));
}
```

## Batch Operations

### sb_dsql_batch_execute()
Executes a prepared statement multiple times with different parameter sets.

```cpp
SB_STATUS sb_dsql_batch_execute(
    SB_STATUS*      status_vector,
    SB_TR_HANDLE    tr_handle,
    SB_STMT_HANDLE  stmt_handle,
    unsigned short  dialect,
    SB_SQLDA*       sqlda_array,
    unsigned short  array_size
);
```

**Example - Batch Insert:**
```cpp
SB_STATUS status[20];
SB_SQLDA* batch_sqlda[100]; // Batch of 100 inserts
char sql[] = "INSERT INTO order_items (order_id, product_id, quantity, price) "
             "VALUES (?, ?, ?, ?)";

// Prepare statement
sb_dsql_prepare(status, transaction, stmt, 0, sql, 4, NULL);

// Set up batch parameters
for (int i = 0; i < 100; i++) {
    batch_sqlda[i] = (SB_SQLDA*)malloc(SB_SQLDA_LENGTH(4));
    batch_sqlda[i]->sqln = 4;
    batch_sqlda[i]->sqld = 4;
    batch_sqlda[i]->version = SB_SQLDA_VERSION1;
    
    // Set parameters for this row
    // ... parameter setup for batch_sqlda[i] ...
}

// Execute batch
if (sb_dsql_batch_execute(status, transaction, stmt, 4, 
                          batch_sqlda, 100)) {
    sb_print_status(status);
} else {
    printf("Batch insert of 100 items completed\n");
}
```

## Advanced Features

### Scrollable Cursors

```cpp
// Cursor types
#define SB_CURSOR_FORWARD_ONLY    0
#define SB_CURSOR_STATIC         1
#define SB_CURSOR_KEYSET         2
#define SB_CURSOR_DYNAMIC        3

// Fetch directions
#define SB_FETCH_NEXT            1
#define SB_FETCH_PRIOR           2
#define SB_FETCH_FIRST           3
#define SB_FETCH_LAST            4
#define SB_FETCH_ABSOLUTE        5
#define SB_FETCH_RELATIVE        6

SB_STATUS sb_dsql_fetch_scroll(
    SB_STATUS*      status_vector,
    SB_STMT_HANDLE  stmt_handle,
    unsigned short  dialect,
    unsigned short  direction,
    long            offset,
    SB_SQLDA*       sqlda
);
```

### Array Fetch

```cpp
SB_STATUS sb_dsql_fetch_array(
    SB_STATUS*      status_vector,
    SB_STMT_HANDLE  stmt_handle,
    unsigned short  dialect,
    SB_SQLDA*       sqlda,
    unsigned short  array_size,
    unsigned short* rows_fetched
);
```

**Example:**
```cpp
SB_STATUS status[20];
SB_SQLDA* output_sqlda;
unsigned short rows_fetched;
int total_rows = 0;

// Fetch 50 rows at a time
while (true) {
    int result = sb_dsql_fetch_array(status, stmt, 4, output_sqlda, 
                                     50, &rows_fetched);
    
    if (result == 100 && rows_fetched == 0) {
        break; // No more rows
    } else if (result != 0 && result != 100) {
        sb_print_status(status);
        break;
    }
    
    // Process fetched rows
    for (int i = 0; i < rows_fetched; i++) {
        // Process row i in the array
        total_rows++;
    }
    
    if (rows_fetched < 50) {
        break; // Partial fetch indicates end
    }
}

printf("Total rows processed: %d\n", total_rows);
```

## Statement Resource Management

### sb_dsql_free_statement()
Releases statement resources.

```cpp
SB_STATUS sb_dsql_free_statement(
    SB_STATUS*      status_vector,
    SB_STMT_HANDLE* stmt_handle,
    unsigned short  option
);
```

**Options:**
```cpp
#define SB_CLOSE        1  // Close cursor, keep statement
#define SB_DROP         2  // Release all statement resources
#define SB_UNPREPARE    4  // Unprepare statement
```

**Example:**
```cpp
SB_STATUS status[20];

// Close cursor but keep statement prepared
sb_dsql_free_statement(status, &stmt, SB_CLOSE);

// Completely release statement
sb_dsql_free_statement(status, &stmt, SB_DROP);
```

## Error Handling

### Statement Error Codes

```cpp
#define sb_dsql_cursor_err          335544569L
#define sb_dsql_sqlda_err           335544570L
#define sb_dsql_relation_err        335544571L
#define sb_dsql_field_err          335544572L
#define sb_dsql_datatype_err       335544573L
#define sb_dsql_var_count_err      335544574L
#define sb_dsql_stmt_handle        335544575L
#define sb_dsql_command_err        335544576L
#define sb_dsql_const_err          335544577L
#define sb_dsql_cursor_open_err    335544578L
#define sb_dsql_cursor_close_err   335544579L
#define sb_dsql_no_cursor          335544580L
#define sb_dsql_cursor_redefined   335544581L
#define sb_dsql_cursor_not_open    335544582L
#define sb_dsql_invalid_cursor     335544583L
```

## Complete Statement Example

```cpp
#include <scratchbird/include/scratchbird.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int execute_parameterized_query(SB_DB_HANDLE db, SB_TR_HANDLE tr) {
    SB_STATUS status[20];
    SB_STMT_HANDLE stmt = 0;
    SB_SQLDA* input_sqlda = NULL;
    SB_SQLDA* output_sqlda = NULL;
    
    char sql[] = "SELECT customer_id, customer_name, credit_limit "
                 "FROM customers WHERE region = ? AND credit_limit >= ? "
                 "ORDER BY customer_name";
    
    // Allocate statement
    if (sb_dsql_allocate_statement(status, db, &stmt)) {
        sb_print_status(status);
        return 1;
    }
    
    // Prepare statement
    output_sqlda = (SB_SQLDA*)malloc(SB_SQLDA_LENGTH(3));
    output_sqlda->sqln = 3;
    output_sqlda->version = SB_SQLDA_VERSION1;
    
    if (sb_dsql_prepare(status, tr, stmt, 0, sql, 4, output_sqlda)) {
        sb_print_status(status);
        goto cleanup;
    }
    
    // Set up input parameters
    input_sqlda = (SB_SQLDA*)malloc(SB_SQLDA_LENGTH(2));
    input_sqlda->sqln = 2;
    input_sqlda->sqld = 2;
    input_sqlda->version = SB_SQLDA_VERSION1;
    
    // Parameter 1: region (VARCHAR)
    input_sqlda->sqlvar[0].sqltype = SQL_VARYING + 1;
    input_sqlda->sqlvar[0].sqllen = 50;
    input_sqlda->sqlvar[0].sqldata = malloc(52);
    *(short*)input_sqlda->sqlvar[0].sqldata = strlen("NORTH");
    strcpy(input_sqlda->sqlvar[0].sqldata + 2, "NORTH");
    input_sqlda->sqlvar[0].sqlind = malloc(sizeof(short));
    *input_sqlda->sqlvar[0].sqlind = 0;
    
    // Parameter 2: credit_limit (DECIMAL)
    input_sqlda->sqlvar[1].sqltype = SQL_INT64 + 1;
    input_sqlda->sqlvar[1].sqllen = sizeof(SB_INT64);
    input_sqlda->sqlvar[1].sqlscale = -2;
    input_sqlda->sqlvar[1].sqldata = malloc(sizeof(SB_INT64));
    *(SB_INT64*)input_sqlda->sqlvar[1].sqldata = 1000000; // 10000.00
    input_sqlda->sqlvar[1].sqlind = malloc(sizeof(short));
    *input_sqlda->sqlvar[1].sqlind = 0;
    
    // Allocate output buffers
    for (int i = 0; i < output_sqlda->sqld; i++) {
        SB_SQLVAR* var = &output_sqlda->sqlvar[i];
        var->sqldata = malloc(var->sqllen);
        var->sqlind = malloc(sizeof(short));
    }
    
    // Execute query
    if (sb_dsql_execute(status, tr, stmt, 4, input_sqlda)) {
        sb_print_status(status);
        goto cleanup;
    }
    
    // Fetch and display results
    int row_count = 0;
    printf("Customers in NORTH region with credit >= 10000.00:\n");
    printf("%-10s %-30s %15s\n", "ID", "Name", "Credit Limit");
    printf("%-10s %-30s %15s\n", "---", "----", "------------");
    
    while (true) {
        int fetch_result = sb_dsql_fetch(status, stmt, 4, output_sqlda);
        
        if (fetch_result == 100) break;
        if (fetch_result != 0) {
            sb_print_status(status);
            break;
        }
        
        row_count++;
        
        int customer_id = *(int*)output_sqlda->sqlvar[0].sqldata;
        
        short name_length = *(short*)output_sqlda->sqlvar[1].sqldata;
        char customer_name[101];
        strncpy(customer_name, output_sqlda->sqlvar[1].sqldata + 2, name_length);
        customer_name[name_length] = '\0';
        
        SB_INT64 credit_raw = *(SB_INT64*)output_sqlda->sqlvar[2].sqldata;
        double credit_limit = credit_raw / 100.0;
        
        printf("%-10d %-30s %15.2f\n", customer_id, customer_name, credit_limit);
    }
    
    printf("\nTotal customers found: %d\n", row_count);
    
cleanup:
    // Clean up resources
    if (input_sqlda) {
        for (int i = 0; i < input_sqlda->sqld; i++) {
            free(input_sqlda->sqlvar[i].sqldata);
            free(input_sqlda->sqlvar[i].sqlind);
        }
        free(input_sqlda);
    }
    
    if (output_sqlda) {
        for (int i = 0; i < output_sqlda->sqld; i++) {
            free(output_sqlda->sqlvar[i].sqldata);
            free(output_sqlda->sqlvar[i].sqlind);
        }
        free(output_sqlda);
    }
    
    if (stmt) {
        sb_dsql_free_statement(status, &stmt, SB_DROP);
    }
    
    return 0;
}
```

## Implementation Files

### Core Statement Processing
- `src/dsql/dsql.cpp` - DSQL statement execution engine
- `src/dsql/make.cpp` - Statement compilation and optimization
- `src/dsql/parse.y` - SQL parser grammar
- `src/dsql/DdlNodes.epp` - DDL statement nodes
- `src/dsql/StmtNodes.cpp` - DML statement nodes

### SQLDA Management
- `src/include/scratchbird/include/sb_sqlda.h` - SQLDA structure definitions
- `src/common/classes/sqlda.cpp` - SQLDA utility functions
- `src/dsql/sqlda.cpp` - SQLDA descriptor management

### Cursor Implementation
- `src/dsql/Cursor.cpp` - Cursor management
- `src/jrd/RecordBuffer.cpp` - Record buffer management
- `src/jrd/VirtualTable.cpp` - Virtual table cursors

---

*This documentation covers the complete ScratchBird statement execution API. See related API documentation for connection management, transaction control, and error handling.*