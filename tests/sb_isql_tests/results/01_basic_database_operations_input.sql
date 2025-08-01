-- =================================================================
-- SCRATCHBIRD BASIC DATABASE OPERATIONS TEST
-- =================================================================

-- Test 1: Database Creation and Connection
-- ================================================
CREATE DATABASE '/home/dcalford/Documents/claude/GitHubRepo/ScratchBird/tests/sb_isql_tests/test_databases/01_basic_database_operations_basic_operations_test.fdb'
    USER 'SYSDBA' PASSWORD 'masterkey'
    DEFAULT CHARACTER SET UTF8
    PAGE_SIZE 8192;

-- Verify connection
SELECT 'DATABASE_CREATED_SUCCESSFULLY' AS STATUS FROM RDB;

-- Test 2: Basic Schema Operations
-- ===============================
CREATE SCHEMA test_schema;
SET SCHEMA 'test_schema';
SELECT CURRENT_SCHEMA FROM RDB;

-- Test 3: Table Creation with Various Data Types
-- ==============================================
CREATE TABLE customers (
    customer_id INTEGER NOT NULL PRIMARY KEY,
    customer_name VARCHAR(100) NOT NULL,
    email VARCHAR(255) UNIQUE,
    phone VARCHAR(20),
    registration_date DATE DEFAULT CURRENT_DATE,
    credit_limit DECIMAL(10,2) DEFAULT 1000.00,
    is_active BOOLEAN DEFAULT TRUE,
    notes BLOB SUB_TYPE TEXT,
    created_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Verify table structure
SELECT 
    r.RDB,
    f.RDB,
    f.RDB,
    r.RDB
FROM RDB r
JOIN RDB f ON r.RDB = f.RDB
WHERE r.RDB = 'CUSTOMERS'
ORDER BY r.RDB;

-- Test 4: Constraints and Indexes
-- ===============================
-- Add check constraint
ALTER TABLE customers 
    ADD CONSTRAINT chk_credit_limit 
    CHECK (credit_limit >= 0);

-- Create index on frequently queried column
CREATE INDEX idx_customer_email ON customers (email);
CREATE INDEX idx_customer_name ON customers (customer_name);

-- Test 5: Data Manipulation Operations
-- ===================================
-- Insert test data
INSERT INTO customers (customer_id, customer_name, email, phone, credit_limit, notes) 
VALUES (1, 'John Smith', 'john.smith@email.com', '555-1234', 5000.00, 'Premium customer');

INSERT INTO customers (customer_id, customer_name, email, phone, credit_limit) 
VALUES (2, 'Jane Doe', 'jane.doe@email.com', '555-5678', 2500.00);

INSERT INTO customers (customer_id, customer_name, email, credit_limit) 
VALUES (3, 'Bob Johnson', 'bob.johnson@email.com', 1500.00);

-- Verify inserts
SELECT 'INSERTED_RECORDS_COUNT' AS OPERATION, COUNT(*) AS RESULT FROM customers;

-- Test 6: Data Query Operations
-- ============================
-- Basic SELECT
SELECT customer_id, customer_name, email, credit_limit 
FROM customers 
ORDER BY customer_id;

-- WHERE clause filtering
SELECT customer_name, credit_limit 
FROM customers 
WHERE credit_limit > 2000 
ORDER BY credit_limit DESC;

-- Test 7: Update Operations
-- ========================
UPDATE customers 
SET credit_limit = credit_limit * 1.1 
WHERE customer_id = 1;

-- Verify update
SELECT customer_id, customer_name, credit_limit 
FROM customers 
WHERE customer_id = 1;

-- Test 8: Aggregate Functions
-- ==========================
SELECT 
    COUNT(*) AS total_customers,
    AVG(credit_limit) AS avg_credit_limit,
    MAX(credit_limit) AS max_credit_limit,
    MIN(credit_limit) AS min_credit_limit,
    SUM(credit_limit) AS total_credit_exposure
FROM customers;

-- Test 9: Date and Time Functions
-- ==============================
SELECT 
    CURRENT_DATE AS current_date,
    CURRENT_TIME AS current_time,
    CURRENT_TIMESTAMP AS current_timestamp,
    EXTRACT(YEAR FROM CURRENT_DATE) AS current_year,
    EXTRACT(MONTH FROM CURRENT_DATE) AS current_month
FROM RDB;

-- Test 10: String Functions
-- =========================
SELECT 
    customer_name,
    UPPER(customer_name) AS name_upper,
    LOWER(customer_name) AS name_lower,
    CHAR_LENGTH(customer_name) AS name_length,
    SUBSTRING(customer_name FROM 1 FOR 10) AS name_substr
FROM customers;

-- Test 11: NULL Handling
-- =====================
INSERT INTO customers (customer_id, customer_name, email) 
VALUES (4, 'Test Customer', 'test@email.com');

SELECT 
    customer_name,
    phone,
    COALESCE(phone, 'No phone provided') AS phone_display,
    CASE WHEN phone IS NULL THEN 'Missing' ELSE 'Available' END AS phone_status
FROM customers
WHERE customer_id = 4;

-- Test 12: Transaction Testing
-- ===========================
COMMIT;

-- Start explicit transaction
SET TRANSACTION;

INSERT INTO customers (customer_id, customer_name, email) 
VALUES (5, 'Transaction Test', 'transaction@email.com');

-- Verify within transaction
SELECT COUNT(*) AS records_in_transaction FROM customers;

-- Rollback transaction
ROLLBACK;

-- Verify rollback
SELECT COUNT(*) AS records_after_rollback FROM customers;

-- Test 13: View Creation and Usage
-- ================================
CREATE VIEW active_customers AS
SELECT 
    customer_id,
    customer_name,
    email,
    credit_limit,
    registration_date
FROM customers
WHERE is_active = TRUE;

-- Query view
SELECT * FROM active_customers ORDER BY customer_name;

-- Test 14: Sequence/Generator Operations
-- =====================================
CREATE SEQUENCE customer_id_seq
    START WITH 1000
    INCREMENT BY 1;

-- Test sequence
SELECT NEXT VALUE FOR customer_id_seq AS next_id FROM RDB;
SELECT NEXT VALUE FOR customer_id_seq AS next_id FROM RDB;
SELECT NEXT VALUE FOR customer_id_seq AS next_id FROM RDB;

-- Test 15: System Information Queries
-- ===================================
-- Database information
SELECT 
    MON,
    MON,
    MON,
    MON,
    MON,
    MON,
    MON,
    MON
FROM MON;

-- Connection information
SELECT 
    MON,
    MONdcalford,
    MON,
    MON,
    MON,
    MON
FROM MON
WHERE MON = CURRENT_CONNECTION;

-- Test 16: Cleanup and Verification
-- =================================
-- Drop objects in reverse order
DROP VIEW active_customers;
DROP SEQUENCE customer_id_seq;
DROP INDEX idx_customer_name;
DROP INDEX idx_customer_email;
DROP TABLE customers;
DROP SCHEMA test_schema;

-- Final verification
SELECT 'TEST_COMPLETED_SUCCESSFULLY' AS FINAL_STATUS FROM RDB;

-- Close connection
EXIT;
