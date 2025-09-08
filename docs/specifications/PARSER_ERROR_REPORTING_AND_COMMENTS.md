# Parser Error Reporting and Intelligent Comment Management

## Error Detection and Reporting

### Precise Error Location

The ScratchBird parser must provide exact error locations with visual indicators:

```sql
-- Example error reporting
SELECT * FROM users WHERE age > AND name = 'John';
                                 ^
ERROR at line 1, column 34:
  Syntax error: Expected expression after '>' operator
  Found: AND (keyword)
  Expected: numeric value, column reference, or expression

-- Multi-line error reporting
CREATE TABLE products (
    id INTEGER PRIMARY KEY,
    name VARCHAR(100)
    price DECIMAL(10,2)  -- Missing comma
    ^^^^^
ERROR at line 4, column 5:
  Syntax error: Expected ',' or ')' after column definition
  Found: price (identifier)
  Hint: Missing comma after previous column definition 'name VARCHAR(100)'
```

### Error Context Information

```c
typedef struct ParserError {
    // Location information
    uint32_t    line_number;        // Line where error occurred
    uint32_t    column_number;      // Column position
    uint32_t    absolute_position;  // Byte position in input
    
    // Error details
    const char* error_code;         // Standardized error code
    const char* error_message;      // Human-readable message
    const char* actual_token;        // What was found
    const char* expected_tokens[10]; // What was expected
    
    // Context
    const char* statement_text;     // Full statement being parsed
    uint32_t    error_start_pos;    // Start of error token
    uint32_t    error_end_pos;      // End of error token
    
    // Helpful information
    const char* hint;               // Suggestion for fixing
    const char* detail;             // Additional context
    const char* documentation_link; // Link to docs
} ParserError;
```

### Visual Error Indicators

```sql
-- Complex expression error
SELECT 
    customer_id,
    SUM(amount * (1 + tax_rate / 100 + discount) 
        ^                                      ^
ERROR at line 3, column 9-46:
  Syntax error: Unmatched parentheses in expression
  Opening '(' at column 9 has no matching ')'
  Hint: Add closing parenthesis after 'discount'

-- Type mismatch error with context
INSERT INTO orders (order_date, amount) VALUES ('not-a-date', 100);
                                                 ^^^^^^^^^^^^
ERROR at line 1, column 50-61:
  Type error: Invalid date literal
  Found: 'not-a-date' (string)
  Expected: Date in format 'YYYY-MM-DD' or date expression
  Example: '2024-01-15' or CURRENT_DATE
```

### Error Recovery and Multiple Errors

```sql
-- Parser continues after error to find more issues
CREATE TABLE bad_table (
    id INTEGER PRIMRY KEY,      -- Typo: PRIMRY
               ^^^^^^
    name VARCHR(100),           -- Typo: VARCHR
         ^^^^^^
    age INTEGER DEFAULT 'text'  -- Type mismatch
                        ^^^^^^
);

ERRORS:
1. Line 2, column 16: Unknown keyword 'PRIMRY'. Did you mean 'PRIMARY'?
2. Line 3, column 10: Unknown data type 'VARCHR'. Did you mean 'VARCHAR'?
3. Line 4, column 25: Type mismatch: DEFAULT value 'text' cannot be assigned to INTEGER column
```

### Context-Aware Error Messages

```sql
-- Parser understands context for better errors
CREATE TABLE orders (
    id INTEGER PRIMARY KEY,
    customer INTEGER REFERENCES customers  -- Missing (id)
);
                                          ^
ERROR at line 3, column 43:
  Incomplete foreign key reference
  Found: End of column definition
  Expected: '(' column_name ')' after table name 'customers'
  Hint: Specify the referenced column: REFERENCES customers(id)

-- Identifier vs keyword context
CREATE TABLE select (  -- 'select' used as table name
    select INTEGER     -- 'select' used as column name
);

WARNING at line 1, column 14:
  Using keyword 'select' as identifier
  Hint: Consider using a different name or quote the identifier: "select"
  Note: This is valid in ScratchBird due to context-aware parsing
```

---

## Intelligent Comment Management

### MSSQL-Style Comment Discovery

ScratchBird automatically discovers comments before DDL statements and applies them as object documentation:

```sql
-- This table stores customer information
-- including contact details and preferences
CREATE TABLE customers (
    -- Primary key for customer identification
    id INTEGER PRIMARY KEY,
    
    -- Customer's full legal name
    name VARCHAR(100) NOT NULL,
    
    -- Email must be unique across system
    email VARCHAR(255) UNIQUE,
    
    /*
     * Customer status:
     * A = Active
     * I = Inactive  
     * S = Suspended
     */
    status CHAR(1) DEFAULT 'A'
);

-- Result: Comments are automatically stored as:
COMMENT ON TABLE customers IS 'This table stores customer information including contact details and preferences';
COMMENT ON COLUMN customers.id IS 'Primary key for customer identification';
COMMENT ON COLUMN customers.name IS 'Customer''s full legal name';
COMMENT ON COLUMN customers.email IS 'Email must be unique across system';
COMMENT ON COLUMN customers.status IS 'Customer status: A = Active, I = Inactive, S = Suspended';
```

### Comment Association Rules

```sql
-- Rule 1: Comments immediately before DDL become object comments
-- This is the orders table
CREATE TABLE orders (...);  -- Associates with table

-- Rule 2: Comments before columns become column comments
CREATE TABLE products (
    -- Product SKU
    sku VARCHAR(50),  -- Associates with column
    
    price DECIMAL(10,2)  -- Inline comment also captured
);

-- Rule 3: Multi-line comments are preserved
/**
 * Complex stored procedure for order processing
 * 
 * Parameters:
 *   @order_id - Order to process
 *   @user_id - User performing action
 * 
 * Returns:
 *   0 - Success
 *   1 - Order not found
 *   2 - Insufficient permissions
 */
CREATE PROCEDURE process_order(
    order_id INTEGER,
    user_id INTEGER
) AS ...

-- Rule 4: Comments between statements are not associated
CREATE TABLE t1 (...);

-- This is a standalone comment

CREATE TABLE t2 (...);  -- Not associated with t2
```

### Comment Extraction Patterns

```sql
-- Pattern 1: JavaDoc style
/**
 * @table users
 * @description User account information
 * @author John Doe
 * @created 2024-01-15
 * @modified 2024-01-20
 */
CREATE TABLE users (...);

-- Extracted as extended properties:
COMMENT ON TABLE users IS 'User account information';
-- Metadata stored: author='John Doe', created='2024-01-15', modified='2024-01-20'

-- Pattern 2: Markdown in comments
/**
 * # Order Processing Table
 * 
 * This table handles **critical** order data.
 * 
 * ## Important Notes:
 * - Never delete records, only mark as cancelled
 * - All monetary values in cents
 * 
 * See: [Design Doc](http://wiki/orders)
 */
CREATE TABLE orders (...);

-- Pattern 3: Tagged comments for different purposes
--@doc This is user documentation
--@internal This is for developers only
--@deprecated Use new_table instead
CREATE TABLE old_table (...);
```

### Comment Preservation in DDL

```sql
-- When generating DDL, preserve comments
SHOW CREATE TABLE customers;

-- Output includes comments:
-- This table stores customer information
-- including contact details and preferences
CREATE TABLE customers (
    -- Primary key for customer identification
    id INTEGER PRIMARY KEY,
    -- Customer's full legal name
    name VARCHAR(100) NOT NULL,
    -- Email must be unique across system
    email VARCHAR(255) UNIQUE,
    /*
     * Customer status:
     * A = Active
     * I = Inactive  
     * S = Suspended
     */
    status CHAR(1) DEFAULT 'A'
);
```

### Programmatic Comment Access

```sql
-- Access comments via system tables
SELECT 
    column_name,
    comment_text,
    comment_type,
    comment_tags
FROM sys.column_comments
WHERE table_name = 'customers';

-- Result:
column_name | comment_text                    | comment_type | comment_tags
------------|---------------------------------|--------------|-------------
id          | Primary key for customer...    | inline       | NULL
name        | Customer's full legal name     | inline       | NULL
status      | Customer status: A = Active... | block        | NULL
```

### Comment Synchronization

```sql
-- Comments survive ALTER operations
ALTER TABLE customers RENAME COLUMN name TO full_name;
-- Comment automatically moves with column

-- Comments in views reference source
CREATE VIEW active_customers AS
SELECT 
    id,      -- Inherits comment from customers.id
    full_name -- Inherits comment from customers.full_name
FROM customers
WHERE status = 'A';

-- Override inherited comments
CREATE VIEW customer_summary AS
SELECT 
    id,      -- @override Customer identifier
    full_name AS display_name  -- @override Display name for UI
FROM customers;
```

---

## Implementation Requirements

### Parser Error Handler

```c
typedef struct ErrorHandler {
    // Error collection
    ParserError* errors;
    uint32_t     error_count;
    uint32_t     max_errors;      // Stop after N errors
    
    // Error recovery
    bool         recovery_mode;    // Try to continue parsing
    uint32_t     recovery_point;   // Where to resume
    
    // Context tracking
    Stack*       context_stack;    // What we're parsing
    const char*  current_statement;
    uint32_t     current_line;
    uint32_t     current_column;
    
    // Output formatting
    bool         color_output;     // Use ANSI colors
    bool         show_hints;       // Include hints
    bool         show_context;     // Show surrounding code
} ErrorHandler;

// Error reporting function
void report_parser_error(ErrorHandler* handler, 
                        uint32_t position,
                        const char* found,
                        const char* expected,
                        const char* hint) {
    // Generate visual indicator
    char* indicator = generate_error_indicator(
        handler->current_statement,
        position
    );
    
    // Format error message
    fprintf(stderr, 
        "%s:%d:%d: ERROR: Expected %s but found '%s'\n%s\n%s\n",
        handler->current_file,
        handler->current_line,
        handler->current_column,
        expected,
        found,
        handler->current_statement,
        indicator
    );
    
    if (hint && handler->show_hints) {
        fprintf(stderr, "HINT: %s\n", hint);
    }
}
```

### Comment Manager

```c
typedef struct CommentManager {
    // Comment storage
    HashMap*     pending_comments;  // Comments awaiting association
    List*        statement_comments; // Comments for current statement
    
    // Comment parsing
    bool         in_comment;
    CommentType  comment_type;      // Line, block, javadoc
    StringBuilder* comment_buffer;
    
    // Association rules
    bool         auto_associate;     // Auto-apply to next DDL
    bool         extract_tags;       // Parse @tags
    bool         preserve_markdown;  // Keep markdown formatting
    
} CommentManager;

// Associate comments with DDL
void associate_comments(CommentManager* mgr, DDLStatement* stmt) {
    if (mgr->pending_comments->count > 0) {
        // Apply to object
        stmt->comment = merge_comments(mgr->pending_comments);
        
        // Apply to columns if CREATE TABLE
        if (stmt->type == DDL_CREATE_TABLE) {
            for (Column* col = stmt->columns; col; col = col->next) {
                if (col->preceding_comment) {
                    col->comment = col->preceding_comment;
                }
            }
        }
        
        clear_pending_comments(mgr);
    }
}
```

---

## Benefits

### For Error Reporting
1. **Precise Location**: Developers know exactly where the problem is
2. **Clear Expectations**: Shows what the parser expected
3. **Helpful Hints**: Suggests fixes for common mistakes
4. **Multiple Errors**: Finds several issues in one pass
5. **Context Aware**: Errors make sense for the specific situation

### For Comment Management
1. **Automatic Documentation**: Comments become part of the schema
2. **Standard Compliance**: Works like MSSQL users expect
3. **Preservation**: Comments survive schema changes
4. **Discoverability**: Comments accessible via system tables
5. **Intelligence**: Parser understands comment context and purpose

This specification ensures ScratchBird provides excellent developer experience with clear error messages and intelligent comment handling.