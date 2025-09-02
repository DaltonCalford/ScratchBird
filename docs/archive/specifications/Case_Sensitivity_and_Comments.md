# Case Sensitivity and Comment Management Specification

## Case Sensitivity Models

### Overview

ScratchBird supports multiple case sensitivity models to ensure compatibility with all major databases while providing flexibility for users.

## Case Handling Modes

### 1. Pascal Case Mode (MSSQL Style) - DEFAULT

```sql
-- Objects created with preserved case
CREATE TABLE CustomerOrders (
    OrderID INTEGER,
    CustomerName VARCHAR(100),
    OrderDate DATE
);

-- Case is preserved for display
SELECT * FROM INFORMATION_SCHEMA.COLUMNS 
WHERE TABLE_NAME = 'CustomerOrders';
-- Returns: OrderID, CustomerName, OrderDate (with original casing)

-- But queries are case-insensitive
SELECT orderid, CUSTOMERNAME, orderdate FROM customerorders;  -- Works
SELECT OrderID, CustomerName, OrderDate FROM CustomerOrders;  -- Works
SELECT ORDERID, customername, ORDERDATE FROM CUSTOMERORDERS;  -- Works
```

### 2. Lower Case Mode (PostgreSQL Style)

```sql
SET SESSION identifier_case_mode = 'lowercase';

-- Unquoted identifiers folded to lowercase
CREATE TABLE CustomerOrders (  -- Stored as customerorders
    OrderID INTEGER,           -- Stored as orderid
    CustomerName VARCHAR(100)  -- Stored as customername
);

-- Quoted identifiers preserve case
CREATE TABLE "CustomerOrders" (  -- Stored as CustomerOrders
    "OrderID" INTEGER            -- Stored as OrderID
);

-- Case-sensitive when quoted
SELECT * FROM customerorders;    -- Works
SELECT * FROM CustomerOrders;    -- Works (folded to lowercase)
SELECT * FROM "CustomerOrders";  -- Different table!
```

### 3. Upper Case Mode (Oracle Style)

```sql
SET SESSION identifier_case_mode = 'uppercase';

-- Unquoted identifiers folded to uppercase
CREATE TABLE CustomerOrders (  -- Stored as CUSTOMERORDERS
    OrderID INTEGER            -- Stored as ORDERID
);

SELECT * FROM customerorders;    -- Works (folded to CUSTOMERORDERS)
SELECT * FROM CUSTOMERORDERS;    -- Works
```

### 4. Case Sensitive Mode (MySQL on Linux)

```sql
SET SESSION identifier_case_mode = 'sensitive';

-- All identifiers are case-sensitive
CREATE TABLE CustomerOrders (...);

SELECT * FROM CustomerOrders;    -- Works
SELECT * FROM customerorders;    -- ERROR: Table not found
```

### 5. Smart Case Mode (ScratchBird Innovation)

```sql
SET SESSION identifier_case_mode = 'smart';

-- Detects naming convention and preserves it
CREATE TABLE customer_orders (...);     -- snake_case detected
CREATE TABLE CustomerOrders (...);      -- PascalCase detected
CREATE TABLE tblCustomerOrders (...);   -- Hungarian notation detected

-- Auto-matches common variations
SELECT * FROM customer_orders;   -- Original
SELECT * FROM CustomerOrders;    -- Auto-converted to customer_orders
SELECT * FROM CUSTOMER_ORDERS;   -- Auto-converted to customer_orders
```

## Implementation

```cpp
class IdentifierManager {
public:
    enum CaseMode {
        PASCAL_PRESERVE,    // MSSQL: Preserve case, compare insensitive
        LOWERCASE_FOLD,     // PostgreSQL: Fold to lower unless quoted
        UPPERCASE_FOLD,     // Oracle: Fold to upper unless quoted
        CASE_SENSITIVE,     // MySQL/Linux: Exact match required
        SMART_CASE         // ScratchBird: Intelligent matching
    };
    
private:
    struct IdentifierEntry {
        string stored_name;      // As stored in catalog
        string canonical_name;   // For comparison
        string display_name;     // For display/results
        bool is_quoted;         // Was created with quotes
    };
    
    map<string, IdentifierEntry> catalog;
    
public:
    string normalize_identifier(const string& name, bool quoted, CaseMode mode) {
        switch (mode) {
            case PASCAL_PRESERVE:
                return {
                    .stored_name = name,
                    .canonical_name = to_lower(name),
                    .display_name = name
                };
                
            case LOWERCASE_FOLD:
                if (quoted) {
                    return {name, name, name};
                } else {
                    auto lower = to_lower(name);
                    return {lower, lower, lower};
                }
                
            case SMART_CASE:
                return smart_normalize(name);
        }
    }
    
    string smart_normalize(const string& name) {
        // Detect naming pattern
        if (is_snake_case(name)) {
            // customer_order -> accept CustomerOrder, CUSTOMER_ORDER
            add_variant("CustomerOrder", to_pascal(name));
            add_variant("CUSTOMER_ORDER", to_upper(name));
        } else if (is_pascal_case(name)) {
            // CustomerOrder -> accept customer_order, CUSTOMER_ORDER
            add_variant("customer_order", to_snake(name));
            add_variant("CUSTOMER_ORDER", to_upper(name));
        }
        // ... other patterns
    }
};
```

## Comment Management

### Intelligent Comment Parsing

ScratchBird automatically extracts and stores comments that appear before object definitions, similar to MSSQL's behavior.

### 1. Leading Comment Detection

```sql
-- This table stores customer information
-- including their contact details and preferences
-- @deprecated: Use CustomerProfiles instead after 2024-06
CREATE TABLE Customers (
    -- Primary key for the customer
    CustomerID INTEGER PRIMARY KEY,
    
    -- Customer's full name
    -- @required
    CustomerName VARCHAR(100) NOT NULL,
    
    /* Customer's email address
       Used for notifications and login
       @unique
       @format: email */
    Email VARCHAR(255) UNIQUE,
    
    -- Customer status
    -- @values: Active, Inactive, Suspended
    Status VARCHAR(20)
);

-- The comments are automatically captured and stored
SELECT 
    OBJECT_NAME,
    OBJECT_TYPE,
    COMMENT_TEXT
FROM INFORMATION_SCHEMA.OBJECT_COMMENTS
WHERE OBJECT_NAME = 'Customers';
-- Returns: "This table stores customer information..."

SELECT 
    COLUMN_NAME,
    COMMENT_TEXT,
    COMMENT_TAGS
FROM INFORMATION_SCHEMA.COLUMN_COMMENTS
WHERE TABLE_NAME = 'Customers';
-- Returns comments for each column with parsed tags
```

### 2. Comment Extraction Rules

```cpp
class CommentExtractor {
    struct CommentBlock {
        string text;
        map<string, string> tags;  // @tag: value pairs
        int start_line;
        int end_line;
    };
    
    CommentBlock extract_leading_comment(const string& sql, size_t object_pos) {
        // Scan backwards from object position
        auto comment_start = find_comment_start(sql, object_pos);
        
        CommentBlock block;
        
        // Extract single-line comments (--)
        while (is_single_line_comment(line)) {
            block.text += extract_comment_text(line);
            extract_tags(line, block.tags);  // Parse @tags
        }
        
        // Extract multi-line comments (/* */)
        if (is_multiline_comment(comment_start)) {
            block.text = extract_multiline_text(comment_start);
            extract_tags(block.text, block.tags);
        }
        
        return block;
    }
    
    void extract_tags(const string& text, map<string, string>& tags) {
        // Parse @tag: value or @tag(value) patterns
        regex tag_pattern(R"(@(\w+)(?:\s*:\s*(.+?))?(?=\s*@|\s*$))");
        // @deprecated: reason
        // @since: 2.0
        // @author: name
        // @see: reference
    }
};
```

### 3. Structured Comment Storage

```sql
-- Comments are stored in system catalog
CREATE TABLE SDB$COMMENTS (
    object_id UUID,
    object_type VARCHAR(20),  -- TABLE, COLUMN, FUNCTION, etc.
    comment_text TEXT,
    comment_tags JSONB,       -- Structured tags
    created_at TIMESTAMP,
    created_by VARCHAR(100)
);

-- Automatic comment extraction on CREATE
CREATE FUNCTION calculate_tax(amount DECIMAL)
RETURNS DECIMAL
AS
-- Calculates tax based on current rates
-- @param amount: The base amount to calculate tax on
-- @returns: The calculated tax amount
-- @throws: InvalidAmountException if amount is negative
-- @since: 1.0
-- @author: John Doe
BEGIN
    RETURN amount * 0.08;
END;

-- Query structured comments
SELECT 
    routine_name,
    comment_text,
    comment_tags->>'@param' as parameters,
    comment_tags->>'@returns' as return_desc,
    comment_tags->>'@since' as version
FROM INFORMATION_SCHEMA.ROUTINE_COMMENTS
WHERE routine_name = 'calculate_tax';
```

### 4. Comment Synchronization

```sql
-- Traditional COMMENT ON syntax also supported
COMMENT ON TABLE Customers IS 'Customer information table';
COMMENT ON COLUMN Customers.Email IS 'Customer email for notifications';

-- But also supports richer syntax
ALTER TABLE Customers 
SET COMMENT = 'Customer information table'
WITH TAGS (
    deprecated = true,
    replacement = 'CustomerProfiles',
    migration_date = '2024-06-01'
);

-- Markdown support in comments
COMMENT ON FUNCTION calculate_tax IS '
# Calculate Tax Function

Calculates tax based on current rates.

## Parameters
- `amount`: The base amount (must be positive)

## Returns
The calculated tax amount

## Example
```sql
SELECT calculate_tax(100.00); -- Returns 8.00
```
';
```

### 5. Comment Inheritance and Templates

```sql
-- Define comment templates
CREATE COMMENT TEMPLATE audit_columns AS '
Column automatically maintained by audit trigger.
@managed-by: system
@do-not-modify: true
';

-- Apply template to multiple columns
ALTER TABLE Customers 
    ALTER COLUMN created_at SET COMMENT USING TEMPLATE audit_columns,
    ALTER COLUMN updated_at SET COMMENT USING TEMPLATE audit_columns;

-- Inherited comments for derived objects
CREATE VIEW ActiveCustomers AS 
SELECT * FROM Customers WHERE Status = 'Active';
-- Inherits table comment with added view-specific note

SELECT COMMENT_TEXT FROM INFORMATION_SCHEMA.VIEW_COMMENTS
WHERE VIEW_NAME = 'ActiveCustomers';
-- Returns: "View of: Customer information table (filtered for active only)"
```

## Configuration Options

```sql
-- System-wide settings
ALTER SYSTEM SET identifier_case_mode = 'pascal_preserve';  -- Default
ALTER SYSTEM SET comment_extraction = 'automatic';          -- Default
ALTER SYSTEM SET comment_tags_enabled = true;              -- Parse @tags
ALTER SYSTEM SET comment_markdown_enabled = true;          -- Support markdown

-- Session-specific settings
SET SESSION identifier_case_mode = 'lowercase';
SET SESSION comment_extraction = 'manual';  -- Only COMMENT ON syntax
SET SESSION preserve_whitespace_in_comments = true;

-- Per-database settings
ALTER DATABASE mydb SET identifier_case_mode = 'sensitive';

-- Per-schema settings
ALTER SCHEMA public SET identifier_case_mode = 'smart';
```

## Compatibility Matrix

| Feature | MSSQL | PostgreSQL | MySQL | Oracle | Firebird | ScratchBird |
|---------|-------|------------|-------|--------|----------|-------------|
| Pascal Case Preserve | ✅ | ❌ | ❌ | ❌ | ❌ | ✅ |
| Lowercase Folding | ❌ | ✅ | ❌ | ❌ | ✅ | ✅ |
| Uppercase Folding | ❌ | ❌ | ❌ | ✅ | ❌ | ✅ |
| Case Sensitive | ❌ | Quoted | Platform | ❌ | ❌ | ✅ |
| Smart Case | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ |
| Leading Comments | ✅ | ❌ | ❌ | ❌ | ❌ | ✅ |
| COMMENT ON | ❌ | ✅ | ❌ | ✅ | ✅ | ✅ |
| Tagged Comments | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ |
| Markdown Comments | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ |

## Benefits

### Case Handling Benefits
1. **True compatibility**: Each client sees identifiers as expected
2. **Migration friendly**: No need to change application code
3. **Developer friendly**: Use preferred naming style
4. **Smart matching**: Reduces "table not found" errors

### Comment Management Benefits
1. **Self-documenting**: Comments extracted from code
2. **Structured metadata**: Tags provide machine-readable info
3. **Rich documentation**: Markdown support for better formatting
4. **Automatic synchronization**: Comments stay with objects
5. **IDE integration**: Comments available for tooltips/autocomplete

## Implementation Priority

1. **Phase 1**: Pascal case preservation (MSSQL compatibility)
2. **Phase 2**: Basic comment extraction
3. **Phase 3**: Smart case mode
4. **Phase 4**: Tagged comments and markdown
5. **Phase 5**: Comment templates and inheritance

## Testing Requirements

```sql
-- Test case preservation
CREATE TABLE TestTable (TestColumn INTEGER);
ASSERT display_name('TestTable') = 'TestTable';
ASSERT canonical_name('TestTable') = 'testtable';
ASSERT query('SELECT * FROM testtable') SUCCEEDS;
ASSERT query('SELECT * FROM TESTTABLE') SUCCEEDS;

-- Test comment extraction
-- This is a test comment
-- @test: true
CREATE TABLE TestCommented (id INTEGER);
ASSERT get_comment('TestCommented') = 'This is a test comment';
ASSERT get_tag('TestCommented', '@test') = 'true';

-- Test smart case
SET SESSION identifier_case_mode = 'smart';
CREATE TABLE user_accounts (...);
ASSERT query('SELECT * FROM UserAccounts') SUCCEEDS;
ASSERT query('SELECT * FROM USER_ACCOUNTS') SUCCEEDS;
```