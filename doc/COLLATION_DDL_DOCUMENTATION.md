# ScratchBird COLLATION - Complete DDL Documentation

**Version**: Alpha 0.6.0  
**Implementation Date**: July 2025  
**Status**: ✅ **Test Ready** - Still missing features to be Implemented  
**Documentation Type**: User Guide & Technical Reference

---

## Overview

COLLATION objects in ScratchBird define text sorting and comparison rules for character data. Collations determine how text is ordered in sorting operations, how string comparisons are performed, and how case and accent sensitivity are handled. They are essential for internationalization and locale-specific text processing.

### Key Features and Capabilities

- **Text Sorting Rules**: Define ordering sequences for different languages and locales
- **Case Sensitivity Control**: Configure case-sensitive or case-insensitive comparisons  
- **Accent Sensitivity Control**: Handle accented characters in comparisons
- **Padding Behavior**: Control space padding in string comparisons
- **Character Set Integration**: Work with different character encodings
- **Custom Collation Creation**: Define custom sorting rules for specific needs
- **International Support**: Built-in support for multiple languages and locales
- **Performance Optimization**: Efficient text comparison algorithms

### ScratchBird-Specific Enhancements

1. **Flexible Attribute System**: Granular control over case, accent, and padding behavior
2. **External Collation Support**: Integration with external collation libraries
3. **Hierarchical Schema Support**: Collations support 3-level qualified names
4. **Enhanced System Catalog**: Extended RDB$COLLATIONS table with detailed metadata
5. **Custom Specific Attributes**: Support for custom collation parameters
6. **Base Collation Inheritance**: Create collations derived from existing ones
7. **Character Set Compatibility**: Full integration with ScratchBird's character set system
8. **International Standards**: Unicode and ICU library integration

---

## DDL Syntax Reference

### CREATE COLLATION

Creates a new collation with specified attributes and behavior.

#### **Basic Syntax**
```sql
CREATE COLLATION [IF NOT EXISTS] [schema_name.]collation_name
    FOR character_set_name
    [FROM {base_collation_name | EXTERNAL ('external_name')}]
    [NO PAD | PAD SPACE]
    [CASE {SENSITIVE | INSENSITIVE}]
    [ACCENT {SENSITIVE | INSENSITIVE}]
    ['specific_attributes'];
```

#### **Complete Syntax with All Options**
```sql
CREATE COLLATION [IF NOT EXISTS] [[catalog.]schema.]collation_name
    FOR character_set_name
    [FROM base_collation_name]
    [FROM EXTERNAL ('external_library_name')]
    [NO PAD | PAD SPACE]
    [CASE SENSITIVE | CASE INSENSITIVE]
    [ACCENT SENSITIVE | ACCENT INSENSITIVE]
    ['specific_attributes_string'];
```

#### **Parameters**

- **IF NOT EXISTS**: Skip creation if collation already exists (no error)
- **schema_name**: Optional schema qualification (supports hierarchical schemas)
- **collation_name**: Collation identifier (63 characters max)
- **character_set_name**: Target character set for the collation
- **FROM base_collation**: Inherit from existing collation
- **FROM EXTERNAL**: Use external collation library
- **NO PAD | PAD SPACE**: String padding behavior in comparisons
- **CASE SENSITIVE/INSENSITIVE**: Case sensitivity control
- **ACCENT SENSITIVE/INSENSITIVE**: Accent sensitivity control
- **specific_attributes**: Custom parameters for collation behavior

---

## CREATE COLLATION Examples

### **Basic Collation Creation**

#### **Simple Case-Insensitive Collation**
```sql
-- Create case-insensitive collation for UTF8 
CREATE COLLATION utf8_ci FOR UTF8
    CASE INSENSITIVE;

-- Case-insensitive and accent-insensitive
CREATE COLLATION utf8_ci_ai FOR UTF8  
    CASE INSENSITIVE
    ACCENT INSENSITIVE;

-- Case-sensitive with no padding
CREATE COLLATION utf8_cs_no_pad FOR UTF8
    CASE SENSITIVE
    NO PAD;
```

#### **Base Collation Inheritance**
```sql
-- Create collation based on existing one
CREATE COLLATION custom_utf8 FOR UTF8
    FROM UTF8_UNICODE
    CASE INSENSITIVE;

-- Create specialized collation from system collation
CREATE COLLATION business_collation FOR UTF8
    FROM UTF8_UNICODE
    CASE INSENSITIVE
    ACCENT INSENSITIVE
    PAD SPACE;
```

### **Advanced Collation Examples**

#### **Language-Specific Collations**
```sql
-- German collation with proper ß handling
CREATE COLLATION german_utf8 FOR UTF8
    FROM UTF8_UNICODE
    CASE INSENSITIVE
    'LOCALE=de_DE;VERSION=134.0.34';

-- French collation with accent handling
CREATE COLLATION french_utf8 FOR UTF8
    FROM UTF8_UNICODE  
    CASE INSENSITIVE
    ACCENT SENSITIVE
    'LOCALE=fr_FR;VERSION=134.0.34';

-- Spanish collation
CREATE COLLATION spanish_utf8 FOR UTF8
    FROM UTF8_UNICODE
    CASE INSENSITIVE
    'LOCALE=es_ES;VERSION=134.0.34';
```

#### **Business-Specific Collations**
```sql
-- Product code collation (case-sensitive, no accents)
CREATE COLLATION product_code_collation FOR ASCII
    CASE SENSITIVE
    NO PAD;

-- Customer name collation (case-insensitive, accent-insensitive)
CREATE COLLATION customer_name_collation FOR UTF8
    FROM UTF8_UNICODE
    CASE INSENSITIVE
    ACCENT INSENSITIVE
    PAD SPACE;

-- Financial data collation (strict binary comparison)
CREATE COLLATION financial_strict FOR UTF8
    FROM UTF8
    CASE SENSITIVE
    ACCENT SENSITIVE
    NO PAD;
```

### **External Collation Integration**

#### **ICU Library Collations**
```sql
-- Use external ICU collation
CREATE COLLATION icu_unicode FOR UTF8
    FROM EXTERNAL ('UNICODE')
    CASE INSENSITIVE
    'NUMERIC-SORT=1';

-- ICU with specific locale
CREATE COLLATION icu_german FOR UTF8
    FROM EXTERNAL ('UNICODE')
    'LOCALE=de;NUMERIC-SORT=1;CASE-LEVEL=1';

-- ICU for Asian languages
CREATE COLLATION icu_japanese FOR UTF8
    FROM EXTERNAL ('UNICODE')
    'LOCALE=ja;STRENGTH=1';
```

#### **Custom External Collations**
```sql
-- Custom collation library
CREATE COLLATION custom_business FOR UTF8
    FROM EXTERNAL ('BUSINESS_SORT_LIB')
    CASE INSENSITIVE
    'ALGORITHM=BUSINESS_STANDARD;VERSION=2.1';
```

### **Hierarchical Schema Collations**

#### **3-Level Schema Qualification**
```sql
-- International division collations
CREATE COLLATION international.europe.german_standard FOR UTF8
    FROM UTF8_UNICODE
    CASE INSENSITIVE
    'LOCALE=de_DE';

CREATE COLLATION international.asia.japanese_standard FOR UTF8
    FROM UTF8_UNICODE
    'LOCALE=ja_JP;STRENGTH=2';

-- Business unit specific collations
CREATE COLLATION business.finance.currency_sort FOR UTF8
    FROM UTF8_UNICODE
    CASE SENSITIVE
    NO PAD;

CREATE COLLATION business.hr.employee_name_sort FOR UTF8
    FROM UTF8_UNICODE
    CASE INSENSITIVE
    ACCENT INSENSITIVE;
```

### **Character Set Specific Collations**

#### **ASCII Collations**
```sql
-- Binary ASCII collation
CREATE COLLATION ascii_binary FOR ASCII
    CASE SENSITIVE
    NO PAD;

-- Case-insensitive ASCII
CREATE COLLATION ascii_ci FOR ASCII
    CASE INSENSITIVE
    PAD SPACE;
```

#### **ISO8859_1 Collations**
```sql
-- Western European collation
CREATE COLLATION iso_western FOR ISO8859_1
    FROM ISO8859_1
    CASE INSENSITIVE
    ACCENT INSENSITIVE;

-- French-specific ISO collation
CREATE COLLATION iso_french FOR ISO8859_1
    FROM ISO8859_1
    CASE INSENSITIVE
    ACCENT SENSITIVE;
```

#### **WIN1252 Collations**
```sql
-- Windows-1252 business collation
CREATE COLLATION win1252_business FOR WIN1252
    FROM WIN1252
    CASE INSENSITIVE
    PAD SPACE;
```

### **Conditional Creation**

#### **IF NOT EXISTS Usage**
```sql
-- Safe collation creation
CREATE COLLATION IF NOT EXISTS default_utf8_ci FOR UTF8
    CASE INSENSITIVE;

-- Schema-qualified conditional creation
CREATE COLLATION IF NOT EXISTS business.standard_sort FOR UTF8
    FROM UTF8_UNICODE
    CASE INSENSITIVE
    ACCENT INSENSITIVE;
```

---

## Using Collations in Database Operations

### **Column Definition with Collations**

#### **Table Creation with Collations**
```sql
-- Create table with specific collations
CREATE TABLE customers (
    customer_id INTEGER PRIMARY KEY,
    first_name VARCHAR(50) COLLATE utf8_ci,
    last_name VARCHAR(50) COLLATE utf8_ci,
    company_name VARCHAR(100) COLLATE customer_name_collation,
    product_code CHAR(10) COLLATE product_code_collation,
    notes TEXT COLLATE UTF8_UNICODE
);

-- Mixed collations in single table
CREATE TABLE international_customers (
    customer_id INTEGER,
    name_english VARCHAR(100) COLLATE UTF8_UNICODE,
    name_local VARCHAR(100) COLLATE icu_unicode,
    address VARCHAR(200) COLLATE utf8_ci_ai,
    postal_code VARCHAR(20) COLLATE ascii_binary
);
```

#### **Domain Definition with Collations**
```sql
-- Create domains with specific collations
CREATE DOMAIN customer_name AS VARCHAR(100) 
    COLLATE customer_name_collation;

CREATE DOMAIN product_code AS CHAR(15)
    COLLATE product_code_collation;

CREATE DOMAIN international_text AS VARCHAR(500)
    COLLATE icu_unicode;

-- Use domains in tables
CREATE TABLE products (
    product_id INTEGER,
    product_code product_code,
    product_name customer_name,
    description international_text
);
```

### **Query Operations with Collations**

#### **Explicit Collation in Queries**
```sql
-- Override default collation in comparisons
SELECT * FROM customers 
WHERE first_name COLLATE utf8_ci = 'JOHN';

-- Case-sensitive search with normally case-insensitive column
SELECT * FROM customers
WHERE company_name COLLATE UTF8_UNICODE = 'Acme Corp';

-- Accent-insensitive search
SELECT * FROM customers  
WHERE last_name COLLATE utf8_ci_ai = 'García';
```

#### **Sorting with Different Collations**
```sql
-- Sort using specific collation
SELECT first_name, last_name 
FROM customers
ORDER BY last_name COLLATE german_utf8, first_name;

-- Multi-level sorting with different collations
SELECT product_code, product_name, category
FROM products
ORDER BY 
    category COLLATE utf8_ci,
    product_code COLLATE ascii_binary,
    product_name COLLATE customer_name_collation;

-- International sorting
SELECT customer_name
FROM international_customers
ORDER BY customer_name COLLATE icu_unicode;
```

#### **String Functions with Collations**
```sql
-- String comparisons with collation
SELECT * FROM customers
WHERE UPPER(first_name COLLATE utf8_ci) = UPPER('john');

-- Pattern matching with collation
SELECT * FROM products
WHERE product_name COLLATE utf8_ci_ai LIKE '%café%';

-- Substring operations
SELECT SUBSTRING(company_name COLLATE utf8_ci FROM 1 FOR 10)
FROM customers;
```

### **Index Creation with Collations**

#### **Collation-Aware Indexes**
```sql
-- Create index with specific collation
CREATE INDEX idx_customer_name ON customers (
    last_name COLLATE german_utf8,
    first_name COLLATE german_utf8
);

-- Composite index with mixed collations
CREATE INDEX idx_product_search ON products (
    category COLLATE utf8_ci,
    product_code COLLATE ascii_binary
);

-- Functional index with collation
CREATE INDEX idx_customer_upper ON customers (
    UPPER(company_name COLLATE utf8_ci)
);
```

---

## System Catalog Integration

ScratchBird stores collation definitions in the RDB$COLLATIONS system table.

### **Querying Collation Information**

#### **List All Collations**
```sql
-- Show all available collations
SELECT 
    RDB$COLLATION_NAME as COLLATION_NAME,
    RDB$CHARACTER_SET_ID as CHARSET_ID,
    cs.RDB$CHARACTER_SET_NAME as CHARACTER_SET,
    RDB$COLLATION_ID as COLLATION_ID,
    RDB$COLLATION_ATTRIBUTES as ATTRIBUTES,
    RDB$SYSTEM_FLAG as IS_SYSTEM,
    RDB$DESCRIPTION as DESCRIPTION,
    RDB$BASE_COLLATION_NAME as BASE_COLLATION,
    RDB$SPECIFIC_ATTRIBUTES as SPECIFIC_ATTRS
FROM RDB$COLLATIONS coll
LEFT JOIN RDB$CHARACTER_SETS cs ON coll.RDB$CHARACTER_SET_ID = cs.RDB$CHARACTER_SET_ID
ORDER BY cs.RDB$CHARACTER_SET_NAME, coll.RDB$COLLATION_NAME;
```

#### **Collations by Character Set**
```sql
-- List collations for specific character set
SELECT 
    c.RDB$COLLATION_NAME as COLLATION_NAME,
    c.RDB$COLLATION_ID as ID,
    CASE 
        WHEN c.RDB$COLLATION_ATTRIBUTES IS NULL THEN 'DEFAULT'
        WHEN BIN_AND(c.RDB$COLLATION_ATTRIBUTES, 1) = 1 THEN 'PAD_SPACE'
        ELSE 'NO_PAD'
    END as PADDING,
    CASE 
        WHEN BIN_AND(c.RDB$COLLATION_ATTRIBUTES, 2) = 2 THEN 'CASE_INSENSITIVE'
        ELSE 'CASE_SENSITIVE'
    END as CASE_SENSITIVITY,
    CASE 
        WHEN BIN_AND(c.RDB$COLLATION_ATTRIBUTES, 4) = 4 THEN 'ACCENT_INSENSITIVE'
        ELSE 'ACCENT_SENSITIVE'
    END as ACCENT_SENSITIVITY
FROM RDB$COLLATIONS c
JOIN RDB$CHARACTER_SETS cs ON c.RDB$CHARACTER_SET_ID = cs.RDB$CHARACTER_SET_ID
WHERE cs.RDB$CHARACTER_SET_NAME = 'UTF8'
ORDER BY c.RDB$COLLATION_NAME;
```

#### **Custom vs System Collations**
```sql
-- Distinguish between system and user-defined collations
SELECT 
    RDB$COLLATION_NAME,
    RDB$CHARACTER_SET_ID,
    CASE RDB$SYSTEM_FLAG
        WHEN 1 THEN 'SYSTEM'
        WHEN 0 THEN 'USER_DEFINED'
        ELSE 'UNKNOWN'
    END as COLLATION_TYPE,
    RDB$BASE_COLLATION_NAME,
    RDB$SPECIFIC_ATTRIBUTES
FROM RDB$COLLATIONS
ORDER BY RDB$SYSTEM_FLAG, RDB$COLLATION_NAME;
```

### **Schema-Aware Collation Queries**

#### **Collations by Schema**
```sql
-- List collations grouped by schema (if schema support added)
SELECT 
    COALESCE(RDB$SCHEMA_NAME, 'DEFAULT') as SCHEMA_NAME,
    RDB$COLLATION_NAME as COLLATION_NAME,
    cs.RDB$CHARACTER_SET_NAME as CHARACTER_SET,
    RDB$DESCRIPTION as DESCRIPTION
FROM RDB$COLLATIONS c
LEFT JOIN RDB$CHARACTER_SETS cs ON c.RDB$CHARACTER_SET_ID = cs.RDB$CHARACTER_SET_ID
WHERE c.RDB$SYSTEM_FLAG = 0 OR c.RDB$SYSTEM_FLAG IS NULL
ORDER BY SCHEMA_NAME, RDB$COLLATION_NAME;
```

#### **Collation Usage Analysis**
```sql
-- Find tables/columns using specific collations
SELECT DISTINCT
    rf.RDB$RELATION_NAME as TABLE_NAME,
    rf.RDB$FIELD_NAME as COLUMN_NAME,
    f.RDB$FIELD_NAME as DOMAIN_NAME,
    c.RDB$COLLATION_NAME as COLLATION_NAME,
    cs.RDB$CHARACTER_SET_NAME as CHARACTER_SET
FROM RDB$RELATION_FIELDS rf
JOIN RDB$FIELDS f ON rf.RDB$FIELD_SOURCE = f.RDB$FIELD_NAME
LEFT JOIN RDB$COLLATIONS c ON f.RDB$COLLATION_ID = c.RDB$COLLATION_ID 
    AND f.RDB$CHARACTER_SET_ID = c.RDB$CHARACTER_SET_ID
LEFT JOIN RDB$CHARACTER_SETS cs ON f.RDB$CHARACTER_SET_ID = cs.RDB$CHARACTER_SET_ID
WHERE c.RDB$COLLATION_NAME IS NOT NULL
  AND (c.RDB$SYSTEM_FLAG = 0 OR c.RDB$SYSTEM_FLAG IS NULL)
ORDER BY c.RDB$COLLATION_NAME, rf.RDB$RELATION_NAME;
```

### **Collation Attribute Analysis**

#### **Decode Collation Attributes**
```sql
-- Detailed collation attributes breakdown
SELECT 
    RDB$COLLATION_NAME,
    RDB$COLLATION_ATTRIBUTES,
    CASE WHEN BIN_AND(RDB$COLLATION_ATTRIBUTES, 1) = 1 
        THEN 'PAD SPACE' ELSE 'NO PAD' END as PADDING_MODE,
    CASE WHEN BIN_AND(RDB$COLLATION_ATTRIBUTES, 2) = 2 
        THEN 'CASE INSENSITIVE' ELSE 'CASE SENSITIVE' END as CASE_MODE,
    CASE WHEN BIN_AND(RDB$COLLATION_ATTRIBUTES, 4) = 4 
        THEN 'ACCENT INSENSITIVE' ELSE 'ACCENT SENSITIVE' END as ACCENT_MODE,
    RDB$BASE_COLLATION_NAME as BASE_COLLATION,
    RDB$SPECIFIC_ATTRIBUTES as CUSTOM_ATTRIBUTES
FROM RDB$COLLATIONS
WHERE RDB$SYSTEM_FLAG = 0 OR RDB$SYSTEM_FLAG IS NULL
ORDER BY RDB$COLLATION_NAME;
```

---

## DROP COLLATION

Removes collation definitions from the database. Collations can only be dropped if no tables, domains, or other database objects depend on them.

### **DROP COLLATION Syntax**
```sql
DROP COLLATION [IF EXISTS] [schema_name.]collation_name;
```

### **DROP COLLATION Examples**

#### **Basic Collation Removal**
```sql
-- Drop collation if it exists
DROP COLLATION IF EXISTS custom_utf8;

-- Drop collation from specific schema
DROP COLLATION business.finance.currency_sort;

-- Drop collation (will fail if in use)
DROP COLLATION german_utf8;
```

#### **Dependency Checking**
```sql
-- Check collation dependencies before dropping
SELECT DISTINCT
    rf.RDB$RELATION_NAME as TABLE_NAME,
    rf.RDB$FIELD_NAME as COLUMN_NAME,
    'COLLATION: ' || c.RDB$COLLATION_NAME as DEPENDENCY_TYPE
FROM RDB$RELATION_FIELDS rf
JOIN RDB$FIELDS f ON rf.RDB$FIELD_SOURCE = f.RDB$FIELD_NAME
JOIN RDB$COLLATIONS c ON f.RDB$COLLATION_ID = c.RDB$COLLATION_ID
WHERE c.RDB$COLLATION_NAME = 'CUSTOM_UTF8';

-- Drop collation after removing dependencies
DROP COLLATION custom_utf8;
```

---

## Advanced Collation Features

### **Collation Performance and Optimization**

#### **High-Performance Text Operations**
```sql
-- Efficient text sorting with optimized collations
CREATE COLLATION fast_sort FOR UTF8
    FROM UTF8_UNICODE
    CASE INSENSITIVE
    NO PAD;  -- NO PAD is faster for VARCHAR comparisons

-- Index-optimized collation
CREATE INDEX idx_fast_search ON large_table (
    search_column COLLATE fast_sort
);

-- Query using optimized collation
SELECT * FROM large_table
WHERE search_column COLLATE fast_sort = 'search_term'
ORDER BY search_column COLLATE fast_sort;
```

#### **Memory-Efficient Collations**
```sql
-- Lightweight collation for large datasets
CREATE COLLATION lightweight_ci FOR UTF8
    CASE INSENSITIVE
    NO PAD
    ''; -- No specific attributes for minimal overhead
```

### **International and Locale-Specific Features**

#### **Multi-Language Support**
```sql
-- Create collations for multi-language applications
CREATE COLLATION multilang_european FOR UTF8
    FROM UTF8_UNICODE
    CASE INSENSITIVE
    ACCENT INSENSITIVE
    'LOCALE=en_US';

-- Regional variations
CREATE COLLATION spanish_traditional FOR UTF8
    FROM UTF8_UNICODE
    'LOCALE=es_ES_TRADITIONAL';

CREATE COLLATION spanish_modern FOR UTF8
    FROM UTF8_UNICODE  
    'LOCALE=es_ES_MODERN';
```

#### **Currency and Numeric Sorting**
```sql
-- Collation for financial data with numeric sorting
CREATE COLLATION financial_numeric FOR UTF8
    FROM UTF8_UNICODE
    CASE SENSITIVE
    'NUMERIC-SORT=1;STRENGTH=4';

-- Use in financial queries
SELECT account_code, balance
FROM accounts
ORDER BY account_code COLLATE financial_numeric;
```

### **Custom Collation Development**

#### **Business Rule Collations**
```sql
-- Custom collation for product codes
CREATE COLLATION product_code_sort FOR ASCII
    FROM ASCII
    CASE SENSITIVE
    NO PAD;

-- Use in product queries
SELECT product_code, product_name
FROM products  
ORDER BY product_code COLLATE product_code_sort;

-- Custom collation for versioning
CREATE COLLATION version_sort FOR UTF8
    FROM UTF8_UNICODE
    'NUMERIC-SORT=1;CASE-LEVEL=0';
```

### **Collation Testing and Validation**

#### **Collation Behavior Testing**
```sql
-- Test collation behavior
SELECT 
    'Test' COLLATE utf8_ci = 'test' as case_insensitive_test,
    'café' COLLATE utf8_ci_ai = 'cafe' as accent_insensitive_test,
    'A ' COLLATE utf8_ci = 'A' as padding_test;

-- Compare sorting behavior
WITH test_data AS (
    SELECT 'Apple' as name UNION ALL
    SELECT 'apple' UNION ALL  
    SELECT 'APPLE' UNION ALL
    SELECT 'Äpfel' UNION ALL
    SELECT 'apfel'
)
SELECT name, 
       ROW_NUMBER() OVER (ORDER BY name COLLATE utf8_ci) as ci_order,
       ROW_NUMBER() OVER (ORDER BY name COLLATE UTF8_UNICODE) as cs_order
FROM test_data;
```

#### **Performance Testing**
```sql
-- Compare collation performance
SELECT COUNT(*) 
FROM large_text_table
WHERE text_column COLLATE utf8_ci LIKE 'pattern%';

-- vs

SELECT COUNT(*)
FROM large_text_table  
WHERE text_column COLLATE UTF8_UNICODE LIKE 'pattern%';
```

---

## Error Handling and Troubleshooting

### **Common Collation Errors**

#### **Collation Creation Errors** 
```sql
-- Error: Collation already exists
CREATE COLLATION utf8_ci FOR UTF8 CASE INSENSITIVE;
-- Solution: Use IF NOT EXISTS

-- Error: Invalid character set
CREATE COLLATION bad_collation FOR NONEXISTENT_CHARSET CASE INSENSITIVE;
-- Solution: Use valid character set

-- Error: Invalid base collation
CREATE COLLATION derived_collation FOR UTF8 FROM NONEXISTENT_BASE;
-- Solution: Use existing base collation
```

#### **Collation Usage Errors**
```sql
-- Error: Collation not compatible with character set
SELECT * FROM table1 WHERE ascii_column COLLATE utf8_ci = 'test';
-- Solution: Use collation matching column's character set

-- Error: Cannot drop collation in use
DROP COLLATION customer_name_collation;
-- Solution: Remove dependencies first
```

### **Debugging Collation Issues**

#### **Collation Compatibility Checking**
```sql
-- Check character set compatibility
SELECT 
    c.RDB$COLLATION_NAME,
    cs.RDB$CHARACTER_SET_NAME,
    c.RDB$COLLATION_ATTRIBUTES
FROM RDB$COLLATIONS c
JOIN RDB$CHARACTER_SETS cs ON c.RDB$CHARACTER_SET_ID = cs.RDB$CHARACTER_SET_ID
WHERE c.RDB$COLLATION_NAME = 'YOUR_COLLATION';
```

#### **Query Result Analysis**
```sql
-- Debug unexpected sorting results
SELECT 
    text_value,
    HEX(text_value) as hex_representation,
    UPPER(text_value COLLATE utf8_ci) as upper_case,
    text_value COLLATE utf8_ci as collated_value
FROM test_table
ORDER BY text_value COLLATE utf8_ci;
```

### **Collation Migration Issues**

#### **Character Set Conversion**
```sql
-- Migrate data between collations
CREATE TABLE temp_conversion AS
SELECT 
    id,
    CAST(text_column AS VARCHAR(100) CHARACTER SET UTF8 COLLATE utf8_ci) as converted_text
FROM original_table;

-- Verify conversion results
SELECT original.text_column, temp.converted_text
FROM original_table original
JOIN temp_conversion temp ON original.id = temp.id
WHERE original.text_column <> temp.converted_text;
```

---

## Best Practices

### **Collation Design Guidelines**

1. **Character Set Matching**: Always match collations to appropriate character sets
2. **Performance Considerations**: Choose efficient collations for high-volume operations
3. **Consistency**: Use consistent collations across related tables and columns
4. **Internationalization**: Plan for multi-language support from the beginning
5. **Testing**: Thoroughly test collation behavior with representative data

### **Recommended Collation Patterns**

#### **Application-Level Standards**
```sql
-- Standard application collations
CREATE COLLATION app_text_ci FOR UTF8
    FROM UTF8_UNICODE
    CASE INSENSITIVE
    ACCENT INSENSITIVE
    PAD SPACE;

CREATE COLLATION app_code_cs FOR ASCII
    CASE SENSITIVE
    NO PAD;

CREATE COLLATION app_name_ci FOR UTF8
    FROM UTF8_UNICODE
    CASE INSENSITIVE
    ACCENT INSENSITIVE;

-- Use in application tables
CREATE DOMAIN user_text AS VARCHAR(500) COLLATE app_text_ci;
CREATE DOMAIN system_code AS VARCHAR(50) COLLATE app_code_cs;
CREATE DOMAIN person_name AS VARCHAR(100) COLLATE app_name_ci;
```

#### **International Application Patterns**
```sql
-- Multi-region collations
CREATE COLLATION global_english FOR UTF8
    FROM UTF8_UNICODE
    CASE INSENSITIVE
    'LOCALE=en_US';

CREATE COLLATION global_european FOR UTF8  
    FROM UTF8_UNICODE
    CASE INSENSITIVE
    ACCENT INSENSITIVE
    'LOCALE=en_US';

-- Use in international tables
CREATE TABLE global_customers (
    customer_id INTEGER,
    name_english VARCHAR(100) COLLATE global_english,
    name_local VARCHAR(100) COLLATE global_european,
    address VARCHAR(200) COLLATE global_european
);
```

---

## Implementation Details

### **Primary Implementation Files**

#### **Parser and Grammar**
- **File**: `src/dsql/parse.y:2459-2525`
- **Classes**: `collation_clause`, `collation_attribute`, `collation_specific_attribute`
- **Functionality**: Parsing CREATE/DROP COLLATION syntax with attributes

#### **DDL Node Classes**
- **File**: `src/dsql/DdlNodes.h:827-928`
- **Classes**:
  - `CreateCollationNode` (lines 827-893): Collation creation with attributes
  - `DropCollationNode` (lines 896-928): Collation removal with dependency checking

#### **System Catalog Integration**
- **File**: `src/jrd/relations.h:478-488`
- **Table**: `RDB$COLLATIONS` - Stores collation definitions
- **Fields**:
  - `f_coll_name`: Collation name
  - `f_coll_id`: Collation ID within character set
  - `f_coll_cs_id`: Character set ID
  - `f_coll_attr`: Collation attributes (pad, case, accent)
  - `f_coll_base_collation_name`: Base collation name
  - `f_coll_specific_attr`: Custom specific attributes

#### **Internationalization Support**
- **File**: `src/common/intlobj_new.h:340-342`
- **Constants**: 
  - `TEXTTYPE_ATTR_PAD_SPACE` (1): Space padding behavior
  - `TEXTTYPE_ATTR_CASE_INSENSITIVE` (2): Case sensitivity control
  - `TEXTTYPE_ATTR_ACCENT_INSENSITIVE` (4): Accent sensitivity control

### **Core Classes and Functions**

#### **CreateCollationNode Methods**
- `setAttribute()`: Sets collation attributes (case, accent, padding)
- `unsetAttribute()`: Removes collation attributes
- `execute()`: Creates collation in system catalog
- Character set validation and compatibility checking

#### **DropCollationNode Methods**
- Dependency checking before collation removal
- Cascade deletion of related metadata
- Schema qualification support

#### **Collation Integration**
- **File**: `src/dsql/parse.y:2875-2878`
- **Function**: `collate_clause` - COLLATE syntax in expressions
- Runtime collation application in queries and comparisons
- Integration with string functions and sorting operations

### **Storage Structures**

Collations are stored in the RDB$COLLATIONS system table with:

- **Identity**: Name, ID, character set association
- **Attributes**: Bit flags for case, accent, and padding behavior
- **Inheritance**: Base collation references for derived collations
- **Customization**: Specific attributes for external library integration
- **Metadata**: System flags, descriptions, function names

---

## Administrative Operations

### **Collation Backup and Restore**

#### **Backup Considerations**
- Collation definitions are included in database backups as DDL
- Character set dependencies are preserved
- Custom specific attributes are maintained
- External collation references may need library availability

#### **Restore Procedures**
```sql
-- Verify collations after restore
SELECT RDB$COLLATION_NAME, RDB$CHARACTER_SET_ID, RDB$COLLATION_ATTRIBUTES
FROM RDB$COLLATIONS
WHERE RDB$SYSTEM_FLAG = 0 OR RDB$SYSTEM_FLAG IS NULL;

-- Test collation functionality
SELECT 'Test' COLLATE your_collation = 'test';
```

### **Collation Maintenance**

#### **Regular Maintenance Tasks**
```sql
-- Monitor collation usage
CREATE VIEW collation_usage AS
SELECT 
    c.RDB$COLLATION_NAME,
    cs.RDB$CHARACTER_SET_NAME,
    COUNT(DISTINCT rf.RDB$RELATION_NAME) as TABLES_USING,
    COUNT(rf.RDB$FIELD_NAME) as COLUMNS_USING
FROM RDB$COLLATIONS c
LEFT JOIN RDB$CHARACTER_SETS cs ON c.RDB$CHARACTER_SET_ID = cs.RDB$CHARACTER_SET_ID
LEFT JOIN RDB$FIELDS f ON c.RDB$COLLATION_ID = f.RDB$COLLATION_ID
LEFT JOIN RDB$RELATION_FIELDS rf ON f.RDB$FIELD_NAME = rf.RDB$FIELD_SOURCE
WHERE c.RDB$SYSTEM_FLAG = 0 OR c.RDB$SYSTEM_FLAG IS NULL
GROUP BY c.RDB$COLLATION_NAME, cs.RDB$CHARACTER_SET_NAME;

-- Check for unused collations
SELECT RDB$COLLATION_NAME
FROM collation_usage
WHERE TABLES_USING = 0;
```

---

## Migration from Other Database Systems

### **From PostgreSQL**
PostgreSQL collations translate to ScratchBird with modifications:
```sql
-- PostgreSQL
CREATE COLLATION german (provider = libc, locale = 'de_DE');

-- ScratchBird equivalent
CREATE COLLATION german FOR UTF8
    FROM UTF8_UNICODE
    'LOCALE=de_DE';
```

### **From Oracle**
Oracle NLS settings can be represented as collations:
```sql
-- Oracle NLS setting equivalent
CREATE COLLATION oracle_like FOR UTF8
    FROM UTF8_UNICODE
    CASE INSENSITIVE
    'LOCALE=en_US';
```

### **From SQL Server**
SQL Server collations map to ScratchBird attributes:
```sql
-- SQL Server: SQL_Latin1_General_CP1_CI_AS
-- ScratchBird equivalent
CREATE COLLATION latin1_ci FOR WIN1252
    FROM WIN1252
    CASE INSENSITIVE
    ACCENT SENSITIVE;
```

---

