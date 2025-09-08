# Domain Security/Integrity and CTE Materialization Specification

## Part 1: Domain-Based Security and Data Integrity

### Overview

Domains carry their constraints and security rules wherever they're used, providing consistent data integrity enforcement across the entire database. This is fundamentally different from table-level constraints which only apply to specific columns.

### Domain Constraints Follow the Type

```sql
-- Create a domain with embedded security rules
CREATE DOMAIN ssn AS VARCHAR(11)
    CHECK (VALUE ~ '^\d{3}-\d{2}-\d{4}$')
    WITH SECURITY (
        MASK_FUNCTION = 'mask_ssn',  -- Always mask when displayed
        AUDIT_ACCESS = TRUE,          -- Log all access
        REQUIRE_PERMISSION = 'VIEW_SENSITIVE_DATA',
        ENCRYPTION = 'AES256'         -- Encrypt at rest
    )
    WITH VALIDATION (
        ON_VIOLATION = 'REJECT',      -- REJECT, WARN, or LOG
        ERROR_MESSAGE = 'Invalid SSN format',
        VALIDATION_FUNCTION = 'validate_ssn'  -- Custom validation
    );

-- Create a domain for email with integrity rules
CREATE DOMAIN email AS VARCHAR(255)
    CHECK (VALUE ~ '^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$')
    WITH INTEGRITY (
        UNIQUE_ACROSS_DATABASE = TRUE,  -- Global uniqueness
        CASE_INSENSITIVE = TRUE,        -- john@example = JOHN@example
        NORMALIZE_FUNCTION = 'normalize_email',  -- Lowercase, trim
        VERIFY_FUNCTION = 'verify_email_exists'  -- External validation
    );

-- Domain for monetary values with business rules
CREATE DOMAIN money_amount AS DECIMAL(19,4)
    CHECK (VALUE >= 0)
    WITH CONSTRAINTS (
        MIN_VALUE = 0.00,
        MAX_VALUE = 999999999.9999,
        PRECISION_RULES = 'ROUND_HALF_UP',
        CURRENCY_CODE = 'USD'
    )
    WITH AUDIT (
        LOG_CHANGES = TRUE,
        LOG_READS = FALSE,
        ALERT_THRESHOLD = 100000.00  -- Alert on large amounts
    );
```

### Domain Constraints Apply Everywhere

```sql
-- Table using domains
CREATE TABLE customers (
    customer_id UUID GENERATED ALWAYS AS IDENTITY,
    email email NOT NULL,  -- Domain constraints apply
    ssn ssn,               -- Domain security applies
    credit_limit money_amount DEFAULT 1000.00
);

-- Attempting to insert invalid data fails at domain level
INSERT INTO customers (email, ssn, credit_limit) 
VALUES ('invalid-email', '123456789', -100);  -- All three fail domain validation

-- Variables in procedures also enforce domain rules
CREATE PROCEDURE process_payment(
    @customer_email email,  -- Must be valid email
    @amount money_amount    -- Must be positive, within limits
) AS $$
BEGIN
    -- Domain constraints are checked on assignment
    DECLARE @test_ssn ssn;
    SET @test_ssn = '123-45-6789';  -- Valid
    SET @test_ssn = '123456789';    -- ERROR: Domain constraint violation
END;
$$ LANGUAGE plpgsql;

-- Function parameters enforce domain rules
CREATE FUNCTION calculate_tax(@amount money_amount) 
RETURNS money_amount AS $$
BEGIN
    -- Domain rules ensure @amount is valid
    RETURN @amount * 0.08;
END;
$$ LANGUAGE plpgsql;
```

### Security Rules Travel with Domains

```sql
-- Create domain for classified data
CREATE DOMAIN classified_info AS TEXT
    WITH SECURITY (
        CLASSIFICATION = 'TOP_SECRET',
        REQUIRE_CLEARANCE = 'TS/SCI',
        AUDIT_ALL_ACCESS = TRUE,
        REDACT_IN_LOGS = TRUE,
        EXPIRE_AFTER = '90 days'
    );

-- Any use of this domain enforces security
CREATE TABLE classified_documents (
    doc_id UUID,
    content classified_info  -- Security rules apply
);

-- Even in temporary tables
CREATE TEMP TABLE temp_classified (
    data classified_info  -- Security still enforced
);

-- Views must respect domain security
CREATE VIEW public_view AS
SELECT 
    doc_id,
    CASE 
        WHEN has_permission('TS/SCI') THEN content
        ELSE '[REDACTED]'
    END AS content
FROM classified_documents;
```

### Domain-Based Data Quality Rules

```sql
-- Phone number with quality rules
CREATE DOMAIN phone_number AS VARCHAR(20)
    WITH QUALITY (
        -- Validation stages
        PARSE_FUNCTION = 'parse_phone_number',
        VALIDATE_FUNCTION = 'validate_phone_format',
        STANDARDIZE_FUNCTION = 'format_e164',
        
        -- Quality scoring
        QUALITY_SCORE_FUNCTION = 'score_phone_quality',
        MIN_QUALITY_SCORE = 0.7,
        
        -- Data enrichment
        ENRICH_FUNCTION = 'lookup_phone_metadata',
        CACHE_ENRICHMENT = '30 days'
    );

-- Address domain with complex validation
CREATE DOMAIN mailing_address AS RECORD (
    street1 VARCHAR(100),
    street2 VARCHAR(100),
    city VARCHAR(50),
    state CHAR(2),
    postal_code VARCHAR(10),
    country CHAR(2)
) WITH VALIDATION (
    -- Multi-field validation
    VALIDATE_FUNCTION = 'validate_address_usps',
    STANDARDIZE_FUNCTION = 'standardize_address',
    GEOCODE_FUNCTION = 'geocode_address',
    
    -- Referential integrity
    VERIFY_AGAINST = 'postal_codes_table',
    
    -- Update rules
    ON_UPDATE_CASCADE = TRUE,
    TRACK_CHANGES = TRUE
);
```

### Domain Inheritance and Composition

```sql
-- Base domain with common rules
CREATE DOMAIN base_identifier AS VARCHAR(50)
    WITH CONSTRAINTS (
        MIN_LENGTH = 3,
        MAX_LENGTH = 50,
        PATTERN = '^[A-Za-z0-9_-]+$'
    )
    WITH SECURITY (
        AUDIT_ACCESS = FALSE,
        CASE_SENSITIVE = FALSE
    );

-- Specialized domains inherit base rules
CREATE DOMAIN user_id AS base_identifier
    WITH CONSTRAINTS (
        PREFIX = 'USR_',
        PATTERN = '^USR_[A-Z0-9]{10}$'
    ) INHERITS base_identifier;

CREATE DOMAIN product_sku AS base_identifier
    WITH CONSTRAINTS (
        PATTERN = '^[A-Z]{3}-[0-9]{4}-[A-Z0-9]{3}$'
    ) INHERITS base_identifier;
```

### Runtime Domain Enforcement

```sql
-- Domain constraints checked at runtime
CREATE FUNCTION dynamic_domain_check() RETURNS VOID AS $$
DECLARE
    @data VARIANT;
    @domain_name VARCHAR(100);
BEGIN
    -- Get data and its intended domain
    SET @data = get_user_input();
    SET @domain_name = 'email';
    
    -- Validate against domain at runtime
    IF NOT validate_against_domain(@data, @domain_name) THEN
        RAISE EXCEPTION 'Domain validation failed for %', @domain_name;
    END IF;
    
    -- Cast to domain type (enforces all rules)
    EXECUTE 'SELECT $1::' || @domain_name USING @data;
END;
$$ LANGUAGE plpgsql;
```

## Part 2: Advanced CTE Materialization with Indexing

### CTE Materialization Options

```sql
-- Default: Non-materialized (evaluated on demand)
WITH customer_orders AS (
    SELECT c.customer_id, c.name, COUNT(o.order_id) as order_count
    FROM customers c
    LEFT JOIN orders o ON c.customer_id = o.customer_id
    GROUP BY c.customer_id, c.name
)
SELECT * FROM customer_orders WHERE order_count > 10;

-- Explicitly materialized (computed once, stored)
WITH customer_orders AS MATERIALIZED (
    SELECT c.customer_id, c.name, COUNT(o.order_id) as order_count
    FROM customers c
    LEFT JOIN orders o ON c.customer_id = o.customer_id
    GROUP BY c.customer_id, c.name
)
SELECT * FROM customer_orders WHERE order_count > 10;

-- Explicitly not materialized (force re-evaluation)
WITH customer_orders AS NOT MATERIALIZED (
    SELECT c.customer_id, c.name, COUNT(o.order_id) as order_count
    FROM customers c
    LEFT JOIN orders o ON c.customer_id = o.customer_id
    GROUP BY c.customer_id, c.name
)
SELECT * FROM customer_orders WHERE order_count > 10;
```

### CTE with Indexing

```sql
-- Materialized CTE with index creation
WITH customer_summary AS MATERIALIZED (
    SELECT 
        c.customer_id,
        c.name,
        c.email,
        COUNT(o.order_id) as order_count,
        SUM(o.total_amount) as total_spent
    FROM customers c
    LEFT JOIN orders o ON c.customer_id = o.customer_id
    GROUP BY c.customer_id, c.name, c.email
    HAVING COUNT(o.order_id) > 0
) INDEXED BY (
    INDEX idx_customer_id ON (customer_id),
    INDEX idx_total_spent ON (total_spent DESC),
    INDEX idx_order_count ON (order_count DESC),
    INDEX idx_email ON (email) UNIQUE
)
SELECT * FROM customer_summary
WHERE total_spent > 1000
ORDER BY total_spent DESC;

-- Multiple indexes on materialized CTE
WITH RECURSIVE category_tree AS MATERIALIZED (
    -- Anchor: top-level categories
    SELECT 
        category_id,
        parent_id,
        name,
        0 as level,
        ARRAY[category_id] as path
    FROM categories
    WHERE parent_id IS NULL
    
    UNION ALL
    
    -- Recursive: child categories
    SELECT 
        c.category_id,
        c.parent_id,
        c.name,
        ct.level + 1,
        ct.path || c.category_id
    FROM categories c
    JOIN category_tree ct ON c.parent_id = ct.category_id
) INDEXED BY (
    INDEX idx_level ON (level),
    INDEX idx_parent ON (parent_id),
    INDEX idx_path ON (path) USING GIN,
    CLUSTER ON idx_level  -- Physically order by level
)
SELECT * FROM category_tree
WHERE level <= 3;
```

### Advanced Materialization Options

```sql
-- Materialized with statistics
WITH sales_analysis AS MATERIALIZED (
    SELECT 
        product_id,
        date_trunc('month', sale_date) as month,
        SUM(quantity) as units_sold,
        SUM(amount) as revenue,
        AVG(amount/quantity) as avg_price
    FROM sales
    WHERE sale_date >= CURRENT_DATE - INTERVAL '1 year'
    GROUP BY product_id, date_trunc('month', sale_date)
) WITH OPTIONS (
    ANALYZE = TRUE,  -- Gather statistics
    STORAGE = 'COLUMNAR',  -- Use columnar storage
    COMPRESSION = 'ZSTD',  -- Compress the materialized data
    PARALLEL_WORKERS = 4  -- Use parallel processing
) INDEXED BY (
    INDEX idx_product_month ON (product_id, month) CLUSTER,
    INDEX idx_revenue ON (revenue DESC) INCLUDE (units_sold)
)
SELECT * FROM sales_analysis
WHERE revenue > 10000;

-- Partitioned materialized CTE
WITH large_dataset AS MATERIALIZED (
    SELECT *
    FROM huge_table
    WHERE created_date >= '2024-01-01'
) PARTITIONED BY RANGE (created_date) (
    PARTITION p_jan VALUES LESS THAN ('2024-02-01'),
    PARTITION p_feb VALUES LESS THAN ('2024-03-01'),
    PARTITION p_mar VALUES LESS THAN ('2024-04-01')
) INDEXED BY (
    LOCAL INDEX idx_id ON (id),  -- Local to each partition
    GLOBAL INDEX idx_status ON (status)  -- Across all partitions
)
SELECT * FROM large_dataset
WHERE status = 'ACTIVE';
```

### Conditional Materialization

```sql
-- Materialize based on cost estimate
WITH expensive_calculation AS MATERIALIZED WHEN COST > 1000 (
    SELECT complex_calculation(*)
    FROM large_table
)
SELECT * FROM expensive_calculation;

-- Materialize based on row count estimate
WITH filtered_data AS MATERIALIZED WHEN ROWS > 10000 (
    SELECT *
    FROM transactions
    WHERE amount > 1000
)
SELECT * FROM filtered_data;

-- Adaptive materialization
WITH dynamic_cte AS AUTO MATERIALIZED (
    -- Let optimizer decide based on statistics
    SELECT *
    FROM complex_view
    WHERE conditions
)
SELECT * FROM dynamic_cte;
```

### CTE Caching and Reuse

```sql
-- Cache materialized CTE for session
WITH cached_data AS MATERIALIZED CACHED FOR SESSION (
    SELECT expensive_computation()
    FROM large_table
)
SELECT * FROM cached_data;  -- First use: computes and caches
-- ... later in same session ...
SELECT * FROM cached_data;  -- Reuses cached result

-- Cache with expiration
WITH time_sensitive AS MATERIALIZED CACHED FOR '5 minutes' (
    SELECT * FROM real_time_data
    WHERE timestamp > CURRENT_TIMESTAMP - INTERVAL '1 hour'
)
SELECT * FROM time_sensitive;

-- Named CTE cache (reusable across statements)
CREATE MATERIALIZED CTE CACHE daily_summary AS (
    SELECT date, SUM(amount) as total
    FROM transactions
    WHERE date = CURRENT_DATE
    GROUP BY date
) REFRESH EVERY '1 hour';

-- Use named cache
WITH daily_summary AS CACHED
SELECT * FROM daily_summary;
```

### Memory vs Disk Materialization

```sql
-- Force memory materialization (fast but limited size)
WITH memory_cte AS MATERIALIZED IN MEMORY (
    SELECT * FROM small_lookup_table
) INDEXED BY (
    INDEX idx_code ON (code) HASH  -- Hash index for memory
)
SELECT * FROM memory_cte;

-- Force disk materialization (slower but unlimited size)
WITH disk_cte AS MATERIALIZED ON DISK (
    SELECT * FROM huge_table
) INDEXED BY (
    INDEX idx_id ON (id) BTREE TABLESPACE fast_ssd
)
SELECT * FROM disk_cte;

-- Hybrid: memory with disk spillover
WITH hybrid_cte AS MATERIALIZED MEMORY THRESHOLD '1GB' (
    SELECT * FROM variable_size_result
)
SELECT * FROM hybrid_cte;
```

### CTE Index Types

```sql
-- Different index types for materialized CTEs
WITH indexed_cte AS MATERIALIZED (
    SELECT *
    FROM products
) INDEXED BY (
    -- B-tree for range queries
    INDEX idx_price ON (price) BTREE,
    
    -- Hash for equality lookups
    INDEX idx_sku ON (sku) HASH,
    
    -- GIN for full-text search
    INDEX idx_description ON (description) GIN,
    
    -- BRIN for large sequential data
    INDEX idx_created ON (created_date) BRIN,
    
    -- Partial index
    INDEX idx_active ON (product_id) WHERE status = 'ACTIVE',
    
    -- Expression index
    INDEX idx_upper_name ON (UPPER(name)),
    
    -- Multi-column covering index
    INDEX idx_category_price ON (category_id, price) INCLUDE (name, sku)
)
SELECT * FROM indexed_cte;
```

### Performance Hints and Monitoring

```sql
-- Provide hints to optimizer
WITH /*+ MATERIALIZE PARALLEL(4) */ heavy_computation AS (
    SELECT complex_aggregation()
    FROM large_table
)
SELECT * FROM heavy_computation;

-- Monitor materialization
WITH monitored_cte AS MATERIALIZED (
    SELECT * FROM large_table
) WITH MONITORING (
    LOG_STATISTICS = TRUE,
    TRACK_MEMORY = TRUE,
    ALERT_IF_SPILLS = TRUE
)
SELECT * FROM monitored_cte;

-- Get CTE statistics
SELECT * FROM sys.cte_statistics
WHERE query_id = pg_backend_pid();
```

### Examples

#### Example 1: Hierarchical Query with Materialization

```sql
WITH RECURSIVE org_chart AS MATERIALIZED (
    -- Get all employees with their full hierarchy
    SELECT 
        e.employee_id,
        e.name,
        e.manager_id,
        0 as level,
        ARRAY[e.employee_id] as path,
        e.name::TEXT as path_names
    FROM employees e
    WHERE manager_id IS NULL
    
    UNION ALL
    
    SELECT 
        e.employee_id,
        e.name,
        e.manager_id,
        oc.level + 1,
        oc.path || e.employee_id,
        oc.path_names || ' > ' || e.name
    FROM employees e
    JOIN org_chart oc ON e.manager_id = oc.employee_id
) INDEXED BY (
    INDEX idx_level ON (level),
    INDEX idx_manager ON (manager_id),
    INDEX idx_path_gin ON (path) USING GIN,
    CLUSTER ON idx_level
)
SELECT * FROM org_chart
WHERE level = 3
   OR 'John Smith' IN (string_to_array(path_names, ' > '));
```

#### Example 2: Time-Series Analysis with Windowing

```sql
WITH time_series AS MATERIALIZED (
    SELECT 
        sensor_id,
        reading_time,
        value,
        AVG(value) OVER (
            PARTITION BY sensor_id 
            ORDER BY reading_time 
            ROWS BETWEEN 10 PRECEDING AND CURRENT ROW
        ) as moving_avg,
        LAG(value, 1) OVER (
            PARTITION BY sensor_id 
            ORDER BY reading_time
        ) as prev_value
    FROM sensor_readings
    WHERE reading_time >= CURRENT_DATE - INTERVAL '7 days'
) INDEXED BY (
    INDEX idx_sensor_time ON (sensor_id, reading_time) CLUSTER,
    INDEX idx_anomaly ON (sensor_id, reading_time) 
        WHERE ABS(value - moving_avg) > moving_avg * 0.2
)
SELECT * FROM time_series
WHERE ABS(value - moving_avg) > moving_avg * 0.2  -- Anomalies
ORDER BY sensor_id, reading_time;
```

This comprehensive approach ensures data integrity through domains while providing powerful CTE materialization for performance optimization!