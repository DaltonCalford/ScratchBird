### DDL: BLOB Filters, Mappings, and Global Temporary Tables

**What it is**

This section covers specialized DDL features: BLOB Filters for processing binary large objects, Mappings for data transformation rules, and Global Temporary Tables (GTT) for session-specific temporary data. These features support advanced data handling scenarios including document processing, ETL operations, and session-isolated computations.

**Why it matters**

- **BLOB Processing**: Handle large binary data efficiently with filters
- **Data Transformation**: Define reusable mapping rules for ETL
- **Session Isolation**: Use GTTs for temporary computations without affecting other sessions
- **Performance**: Optimize handling of special data types and patterns
- **Modularity**: Encapsulate complex transformations in reusable components

**How to use it**

Create BLOB filters to process binary data during storage or retrieval. Define mappings to transform data between different formats or schemas. Use Global Temporary Tables for session-specific work that doesn't need persistence. These features work together to handle complex data processing requirements.

## BLOB Filters

### CREATE BLOB FILTER

```sql
-- Basic BLOB filter for compression
CREATE BLOB FILTER compress_filter
TYPE COMPRESSION
WITH (
    algorithm = 'zlib',
    level = 6
);

-- Image processing filter
CREATE BLOB FILTER image_thumbnail
TYPE TRANSFORMATION
LANGUAGE FUNCTION
AS thumbnail_generator
WITH (
    max_width = 200,
    max_height = 200,
    quality = 85
);

-- Encryption filter
CREATE BLOB FILTER encrypt_sensitive
TYPE ENCRYPTION
WITH (
    algorithm = 'AES256',
    key_id = 'master_key_001'
);

-- Document extraction filter
CREATE BLOB FILTER pdf_text_extractor
TYPE EXTRACTION
LANGUAGE EXTERNAL
AS '/usr/lib/filters/pdf_extract.so'
WITH (
    output_format = 'text',
    ocr_enabled = true
);

-- Virus scanning filter
CREATE BLOB FILTER antivirus_scan
TYPE VALIDATION
LANGUAGE EXTERNAL
AS 'clamav_scan'
WITH (
    action_on_threat = 'reject',
    quarantine_path = '/var/quarantine'
);
```

### Using BLOB Filters

```sql
-- Apply filter to column
CREATE TABLE documents (
    id SERIAL PRIMARY KEY,
    name VARCHAR(255),
    content BLOB FILTER compress_filter,
    thumbnail BLOB FILTER image_thumbnail
);

-- Apply multiple filters (pipeline)
ALTER TABLE sensitive_docs
ALTER COLUMN data 
SET FILTER (encrypt_sensitive, compress_filter);

-- Conditional filter application
CREATE TABLE user_uploads (
    id SERIAL PRIMARY KEY,
    file_type VARCHAR(50),
    data BLOB,
    CONSTRAINT apply_filter 
        CHECK (
            CASE file_type
                WHEN 'image' THEN apply_filter(data, 'image_thumbnail')
                WHEN 'document' THEN apply_filter(data, 'pdf_text_extractor')
                ELSE true
            END
        )
);
```

### ALTER BLOB FILTER

```sql
-- Modify filter parameters
ALTER BLOB FILTER compress_filter
SET level = 9;

-- Add new parameter
ALTER BLOB FILTER image_thumbnail
ADD PARAMETER format = 'webp';

-- Change filter implementation
ALTER BLOB FILTER pdf_text_extractor
SET FUNCTION = 'new_pdf_extractor';

-- Rename filter
ALTER BLOB FILTER old_filter RENAME TO new_filter;
```

### DROP BLOB FILTER

```sql
-- Drop filter
DROP BLOB FILTER obsolete_filter;

-- Drop if exists
DROP BLOB FILTER IF EXISTS temp_filter;

-- Drop with cascade (removes from all columns)
DROP BLOB FILTER used_filter CASCADE;
```

## Mappings

### CREATE MAPPING

```sql
-- Simple value mapping
CREATE MAPPING status_codes AS
    'A' -> 'Active',
    'I' -> 'Inactive',
    'P' -> 'Pending',
    'D' -> 'Deleted'
DEFAULT 'Unknown';

-- Range mapping
CREATE MAPPING age_groups AS
    0..17 -> 'Minor',
    18..64 -> 'Adult',
    65..120 -> 'Senior'
DEFAULT 'Invalid';

-- Pattern mapping
CREATE MAPPING email_domains AS
    '%@gmail.com' -> 'Gmail',
    '%@yahoo.com' -> 'Yahoo',
    '%@outlook.com' -> 'Outlook',
    '%@%.edu' -> 'Educational',
    '%@%.gov' -> 'Government'
DEFAULT 'Other';

-- Complex mapping with expressions
CREATE MAPPING risk_level
TYPE EXPRESSION AS
    CASE 
        WHEN score < 300 THEN 'High Risk'
        WHEN score BETWEEN 300 AND 600 THEN 'Medium Risk'
        WHEN score > 600 THEN 'Low Risk'
    END;

-- JSON mapping
CREATE MAPPING json_transform
TYPE JSON AS
    '$.old_field' -> '$.new_field',
    '$.nested.value' -> '$.flat_value',
    '$.array[*].id' -> '$.id_list[*]';

-- External mapping function
CREATE MAPPING currency_conversion
TYPE FUNCTION
AS convert_currency
WITH (base_currency = 'USD');
```

### Using Mappings

```sql
-- Apply mapping in SELECT
SELECT 
    id,
    name,
    APPLY_MAPPING(status, 'status_codes') AS status_text
FROM users;

-- Use mapping in view
CREATE VIEW user_friendly AS
SELECT 
    id,
    name,
    APPLY_MAPPING(age, 'age_groups') AS age_group,
    APPLY_MAPPING(email, 'email_domains') AS email_provider
FROM users;

-- Mapping in ETL
INSERT INTO target_table (id, status_text)
SELECT 
    id,
    APPLY_MAPPING(status_code, 'status_codes')
FROM source_table;

-- Reverse mapping
SELECT 
    REVERSE_MAPPING('Active', 'status_codes') AS status_code;
-- Returns: 'A'
```

### ALTER MAPPING

```sql
-- Add new mapping entry
ALTER MAPPING status_codes
ADD 'S' -> 'Suspended';

-- Modify existing mapping
ALTER MAPPING status_codes
ALTER 'P' -> 'Processing';

-- Remove mapping entry
ALTER MAPPING status_codes
DROP 'D';

-- Change default value
ALTER MAPPING status_codes
SET DEFAULT 'Undefined';

-- Rename mapping
ALTER MAPPING old_mapping RENAME TO new_mapping;
```

### DROP MAPPING

```sql
-- Drop mapping
DROP MAPPING obsolete_mapping;

-- Drop if exists
DROP MAPPING IF EXISTS temp_mapping;

-- Drop with dependencies check
DROP MAPPING used_mapping RESTRICT;
```

## Global Temporary Tables (GTT)

### CREATE GLOBAL TEMPORARY TABLE

```sql
-- Session-level GTT (data deleted on commit)
CREATE GLOBAL TEMPORARY TABLE session_temp (
    id INTEGER,
    data VARCHAR(100),
    calculated_value DECIMAL(10,2)
) ON COMMIT DELETE ROWS;

-- Transaction-level GTT (data preserved until session ends)
CREATE GLOBAL TEMPORARY TABLE transaction_temp (
    batch_id INTEGER,
    processing_status VARCHAR(20),
    error_message TEXT
) ON COMMIT PRESERVE ROWS;

-- GTT with indexes
CREATE GLOBAL TEMPORARY TABLE calc_workspace (
    row_id SERIAL,
    category VARCHAR(50),
    amount DECIMAL(15,2),
    running_total DECIMAL(15,2)
) ON COMMIT DELETE ROWS;

CREATE INDEX idx_gtt_category ON calc_workspace(category);

-- GTT with constraints
CREATE GLOBAL TEMPORARY TABLE import_staging (
    id INTEGER PRIMARY KEY,
    email VARCHAR(255) UNIQUE,
    amount DECIMAL(10,2) CHECK (amount > 0),
    status VARCHAR(20) DEFAULT 'pending'
) ON COMMIT PRESERVE ROWS;
```

### Using Global Temporary Tables

```sql
-- Session 1
INSERT INTO session_temp VALUES (1, 'Session 1 Data', 100.00);
SELECT * FROM session_temp;  -- Shows data

-- Session 2 (concurrent)
SELECT * FROM session_temp;  -- Empty, different session
INSERT INTO session_temp VALUES (2, 'Session 2 Data', 200.00);
SELECT * FROM session_temp;  -- Shows only Session 2 data

-- Transaction handling
BEGIN;
INSERT INTO transaction_temp VALUES (1, 'processing', NULL);
-- Data visible within transaction
COMMIT;
-- ON COMMIT DELETE ROWS: Data gone
-- ON COMMIT PRESERVE ROWS: Data still there

-- Complex usage example
CREATE GLOBAL TEMPORARY TABLE analysis_temp (
    customer_id INTEGER,
    total_orders INTEGER,
    total_amount DECIMAL(15,2)
) ON COMMIT DELETE ROWS;

-- Populate with aggregated data
INSERT INTO analysis_temp
SELECT 
    customer_id,
    COUNT(*) AS total_orders,
    SUM(amount) AS total_amount
FROM orders
WHERE order_date >= CURRENT_DATE - INTERVAL '30 days'
GROUP BY customer_id;

-- Use for further analysis
SELECT 
    CASE 
        WHEN total_amount > 10000 THEN 'VIP'
        WHEN total_amount > 1000 THEN 'Regular'
        ELSE 'New'
    END AS customer_tier,
    COUNT(*) AS customer_count,
    AVG(total_amount) AS avg_amount
FROM analysis_temp
GROUP BY 1;
```

### ALTER GLOBAL TEMPORARY TABLE

```sql
-- Add column
ALTER GLOBAL TEMPORARY TABLE session_temp
ADD COLUMN created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP;

-- Drop column
ALTER GLOBAL TEMPORARY TABLE session_temp
DROP COLUMN calculated_value;

-- Add constraint
ALTER GLOBAL TEMPORARY TABLE import_staging
ADD CONSTRAINT check_email CHECK (email LIKE '%@%');

-- Note: Cannot change ON COMMIT behavior after creation
-- Must drop and recreate for that
```

### DROP GLOBAL TEMPORARY TABLE

```sql
-- Drop GTT
DROP GLOBAL TEMPORARY TABLE session_temp;

-- Drop if exists
DROP GLOBAL TEMPORARY TABLE IF EXISTS old_temp;

-- Drop cascade
DROP GLOBAL TEMPORARY TABLE calc_workspace CASCADE;
```

## Combined Usage Patterns

### ETL Pipeline with GTT and Mappings

```sql
-- Step 1: Load raw data into GTT
CREATE GLOBAL TEMPORARY TABLE etl_staging (
    raw_id VARCHAR(50),
    raw_status VARCHAR(10),
    raw_amount VARCHAR(20),
    raw_date VARCHAR(20)
) ON COMMIT PRESERVE ROWS;

-- Step 2: Load and validate
INSERT INTO etl_staging
SELECT * FROM external_source
WHERE raw_date IS NOT NULL;

-- Step 3: Transform using mappings
INSERT INTO permanent_table (id, status, amount, process_date)
SELECT 
    CAST(raw_id AS INTEGER),
    APPLY_MAPPING(raw_status, 'status_codes'),
    CAST(REPLACE(raw_amount, ',', '') AS DECIMAL(10,2)),
    TO_DATE(raw_date, 'MM/DD/YYYY')
FROM etl_staging
WHERE CAST(REPLACE(raw_amount, ',', '') AS DECIMAL(10,2)) > 0;

-- Step 4: Cleanup happens automatically at session end
```

### Document Processing with BLOB Filters

```sql
-- Create document storage with automatic processing
CREATE TABLE document_archive (
    id SERIAL PRIMARY KEY,
    upload_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    file_name VARCHAR(255),
    file_type VARCHAR(50),
    original_data BLOB FILTER antivirus_scan,
    compressed_data BLOB FILTER compress_filter,
    extracted_text TEXT,
    metadata JSONB
);

-- Trigger to extract text from documents
CREATE TRIGGER extract_document_text
AFTER INSERT ON document_archive
FOR EACH ROW
WHEN (NEW.file_type IN ('pdf', 'docx', 'txt'))
EXECUTE FUNCTION process_document();

CREATE FUNCTION process_document() RETURNS TRIGGER AS $$
BEGIN
    NEW.extracted_text := apply_filter(NEW.original_data, 'pdf_text_extractor');
    NEW.metadata := extract_metadata(NEW.original_data);
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;
```

### Session-Specific Calculations

```sql
-- Create workspace for complex calculations
CREATE GLOBAL TEMPORARY TABLE calculation_workspace (
    step_number INTEGER,
    intermediate_result DECIMAL(20,10),
    formula_used TEXT,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
) ON COMMIT PRESERVE ROWS;

-- Procedure using GTT
CREATE PROCEDURE complex_calculation(input_value DECIMAL)
AS
BEGIN
    -- Step 1
    INSERT INTO calculation_workspace (step_number, intermediate_result, formula_used)
    VALUES (1, input_value * 1.1, 'Initial adjustment: x * 1.1');
    
    -- Step 2
    INSERT INTO calculation_workspace (step_number, intermediate_result, formula_used)
    SELECT 
        2,
        intermediate_result * 0.95,
        'Discount application: x * 0.95'
    FROM calculation_workspace
    WHERE step_number = 1;
    
    -- Final result
    SELECT intermediate_result 
    FROM calculation_workspace 
    WHERE step_number = (SELECT MAX(step_number) FROM calculation_workspace);
END;
```

## Performance Considerations

### BLOB Filter Performance

```sql
-- Chain filters efficiently
CREATE BLOB FILTER efficient_chain
TYPE PIPELINE AS (
    compress_filter,    -- Compress first
    encrypt_sensitive   -- Then encrypt
);

-- Async filter processing
ALTER BLOB FILTER heavy_processing
SET async = true,
    max_workers = 4;
```

### Mapping Optimization

```sql
-- Create indexed mapping for large datasets
CREATE MAPPING large_mapping AS SELECT ...
WITH INDEX;

-- Cache frequently used mappings
ALTER MAPPING popular_mapping
SET cache = true,
    cache_size = '10MB';
```

### GTT Best Practices

```sql
-- Use appropriate ON COMMIT clause
-- DELETE ROWS: For single-transaction work
-- PRESERVE ROWS: For multi-transaction sessions

-- Index GTTs for performance
CREATE INDEX ON session_temp(category);

-- Analyze GTT for optimizer
ANALYZE session_temp;
```

## Implementation Details

**Parser** (`src/engine/parser_ddl.cpp`):
- `parse_ddl_blob_filter`: BLOB filter DDL
- `parse_ddl_mapping`: Mapping DDL
- `parse_ddl_gtt`: Global temporary table DDL

**Storage**:
- BLOB filter pipeline execution
- Mapping cache management
- GTT session isolation

**Code Anchors**:
- BLOB filter parser: `src/engine/parser_ddl.cpp` (parse_ddl_blob_filter)
- Mapping parser: `src/engine/parser_ddl.cpp` (parse_ddl_mapping)
- GTT parser: `src/engine/parser_ddl.cpp` (parse_ddl_gtt)
- Filter execution: `src/engine/blob_filters.cpp`
- Mapping engine: `src/engine/mapping_engine.cpp`
- GTT manager: `src/engine/gtt_manager.cpp`

## See also

- [Data Types](./sql-data-types.md) - BLOB and other types
- [Tables](./ddl-tables.md) - Permanent tables
- [Functions](./psql-routines-and-triggers.md) - Custom transformations
- [Session](./session-and-transaction.md) - Session management
- [Performance](./explain-analyze.md) - Query optimization