# sb_isql - Interactive SQL Shell 🟢

sb_isql is ScratchBird's enhanced interactive SQL utility that provides a powerful command-line interface for database operations. It offers 100% compatibility with Firebird's ISQL while adding modern features like hierarchical schema support, advanced editing, and multiple export formats.

## 🚀 Quick Start

### **Basic Connection**
```bash
# Connect to database
sb_isql -user SYSDBA -password masterkey mydatabase.fdb

# Connect with trusted authentication
sb_isql -trusted mydatabase.fdb

# Connect and run script
sb_isql -input script.sql -user SYSDBA mydatabase.fdb
```

### **Interactive Mode**
```sql
SQL> SELECT * FROM customers;
SQL> SHOW TABLES;
SQL> QUIT;
```

---

## 📋 Command Reference

### **Connection Options**
| Option | Description | Example |
|--------|-------------|---------|
| `-user <username>` | Database username | `-user SYSDBA` |
| `-password <password>` | Database password | `-password masterkey` |
| `-role <role>` | SQL role name | `-role DB_ADMIN` |
| `-trusted` | Use trusted authentication | `-trusted` |
| `-fetch_password` | Fetch password from file | `-fetch_password ~/.dbpass` |

### **Input/Output Options**
| Option | Description | Example |
|--------|-------------|---------|
| `-input <file>` | Read commands from file | `-input setup.sql` |
| `-output <file>` | Write output to file | `-output results.txt` |
| `-merge <file>` | Merge stderr and stdout | `-merge combined.log` |
| `-echo` | Echo commands | `-echo` |
| `-bail` | Bail on first error | `-bail` |
| `-quiet` | Quiet mode | `-quiet` |

### **Schema Options** (ScratchBird Enhancement)
| Option | Description | Example |
|--------|-------------|---------|
| `-schema <name>` | Set current schema | `-schema finance.accounting` |
| `-home_schema <name>` | Set home schema | `-home_schema company.main` |

### **Display Options**
| Option | Description | Example |
|--------|-------------|---------|
| `-stats` | Show performance statistics | `-stats` |
| `-plan` | Show execution plan | `-plan` |
| `-noheaders` | Don't show column headers | `-noheaders` |
| `-list` | List format output | `-list` |
| `-pagesize <size>` | Set page size | `-pagesize 50` |

### **Other Options**
| Option | Description | Example |
|--------|-------------|---------|
| `-term <char>` | Set statement terminator | `-term ^` |
| `-x` | Extract DDL for database | `-x` |
| `-a` | Extract DDL for all objects | `-a` |
| `-z` | Show version | `-z` |
| `-?` | Show help | `-?` |

---

## 🎯 Interactive Commands

### **Connection Commands**
```sql
-- Connect to database
CONNECT 'mydatabase.fdb' USER 'SYSDBA' PASSWORD 'masterkey';

-- Connect with role
CONNECT 'mydatabase.fdb' USER 'manager' PASSWORD 'secret' ROLE 'SALES_MANAGER';

-- Disconnect
DISCONNECT;
```

### **Transaction Commands**
```sql
-- Commit transaction
COMMIT;

-- Rollback transaction
ROLLBACK;

-- Start transaction with specific options
SET TRANSACTION ISOLATION LEVEL SNAPSHOT;
```

### **Schema Commands** (ScratchBird Enhancement)
```sql
-- Set current schema
SET SCHEMA 'finance.accounting';

-- Set home schema
SET HOME SCHEMA 'company.main';

-- Show current schema
SHOW SCHEMA;

-- Show home schema
SHOW HOME SCHEMA;

-- Show all schemas
SHOW SCHEMAS;
```

### **Display Commands**
```sql
-- Show all tables
SHOW TABLES;

-- Show tables in specific schema
SET SCHEMA 'finance';
SHOW TABLES;

-- Show table structure
SHOW TABLE customers;

-- Show indexes
SHOW INDEXES;

-- Show procedures
SHOW PROCEDURES;

-- Show triggers
SHOW TRIGGERS;

-- Show domains
SHOW DOMAINS;

-- Show roles
SHOW ROLES;

-- Show users
SHOW USERS;
```

### **SET Commands**
```sql
-- Enable/disable statistics
SET STATS ON;
SET STATS OFF;

-- Enable/disable execution plan
SET PLAN ON;
SET PLAN OFF;

-- Set autocommit
SET AUTOCOMMIT ON;
SET AUTOCOMMIT OFF;

-- Set terminator
SET TERM ^;

-- Set list format
SET LIST ON;
SET LIST OFF;

-- Set page size
SET PAGESIZE 25;
```

---

## 🔧 Advanced Features

### **Hierarchical Schema Support**
ScratchBird's unique feature allows nested schemas:

```sql
-- Create hierarchical schema structure
CREATE SCHEMA company;
CREATE SCHEMA company.finance;
CREATE SCHEMA company.finance.accounting;
CREATE SCHEMA company.finance.accounting.reports;

-- Set working schema
SET SCHEMA 'company.finance.accounting';

-- Now you can reference tables without full path
SELECT * FROM customers;  -- Instead of company.finance.accounting.customers

-- Show schema hierarchy
SHOW SCHEMA HIERARCHY;
```

### **Enhanced Editing**
```sql
-- Command history (use arrow keys)
-- Previous command: ↑
-- Next command: ↓

-- Line editing
-- Home: Go to beginning of line
-- End: Go to end of line
-- Ctrl+A: Beginning of line
-- Ctrl+E: End of line
-- Ctrl+K: Kill to end of line

-- Multi-line editing
SQL> SELECT customer_name,
CON>        order_date,
CON>        total_amount
CON> FROM orders
CON> WHERE order_date >= '2025-01-01';
```

### **Export Formats**
```bash
# CSV export
sb_isql -output results.csv -format csv mydatabase.fdb
# Then execute: SELECT * FROM customers;

# JSON export
sb_isql -output results.json -format json mydatabase.fdb

# XML export
sb_isql -output results.xml -format xml mydatabase.fdb

# HTML export
sb_isql -output results.html -format html mydatabase.fdb
```

### **Script Processing**
```sql
-- Input redirection
INPUT /path/to/script.sql;

-- Output redirection
OUTPUT /path/to/output.txt;
SELECT * FROM customers;
OUTPUT;  -- Close output file

-- Conditional execution
SET BAIL ON;  -- Stop on first error
INPUT critical_script.sql;
```

---

## 💼 Real-World Examples

### **Database Setup Script**
```sql
-- setup_database.sql
-- Complete database setup with hierarchical schemas

-- Create main schema structure
CREATE SCHEMA ecommerce;
CREATE SCHEMA ecommerce.customers;
CREATE SCHEMA ecommerce.products;
CREATE SCHEMA ecommerce.orders;
CREATE SCHEMA ecommerce.analytics;

-- Set working schema
SET SCHEMA 'ecommerce.customers';

-- Create customer tables
CREATE TABLE profiles (
    customer_id INTEGER PRIMARY KEY,
    email VARCHAR(100) UNIQUE NOT NULL,
    first_name VARCHAR(50),
    last_name VARCHAR(50),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE addresses (
    address_id INTEGER PRIMARY KEY,
    customer_id INTEGER NOT NULL,
    address_type VARCHAR(20) DEFAULT 'shipping',
    street_address VARCHAR(200),
    city VARCHAR(50),
    state VARCHAR(50),
    postal_code VARCHAR(20),
    country VARCHAR(50) DEFAULT 'USA',
    FOREIGN KEY (customer_id) REFERENCES profiles(customer_id)
);

-- Switch to products schema
SET SCHEMA 'ecommerce.products';

CREATE TABLE catalog (
    product_id INTEGER PRIMARY KEY,
    sku VARCHAR(50) UNIQUE NOT NULL,
    name VARCHAR(200) NOT NULL,
    description TEXT,
    price DECIMAL(10,2) NOT NULL,
    category VARCHAR(50),
    active BOOLEAN DEFAULT TRUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT positive_price CHECK (price > 0)
);

-- Add sample data
INSERT INTO catalog (product_id, sku, name, price, category) VALUES
(1, 'LAPTOP-001', 'Gaming Laptop Pro', 1299.99, 'Electronics'),
(2, 'MOUSE-001', 'Wireless Mouse', 29.99, 'Electronics'),
(3, 'BOOK-001', 'Database Design Guide', 49.99, 'Books');

COMMIT;
```

**Run the setup:**
```bash
sb_isql -input setup_database.sql -user SYSDBA -password masterkey mystore.fdb
```

### **Data Analysis Script**
```sql
-- analysis.sql
-- Comprehensive data analysis with statistics

SET STATS ON;
SET PLAN ON;

-- Set working schema
SET SCHEMA 'ecommerce.analytics';

-- Customer analysis
SELECT 'Customer Analysis' as report_section;

SELECT 
    COUNT(*) as total_customers,
    COUNT(CASE WHEN created_at >= CURRENT_DATE - 30 THEN 1 END) as new_customers_30d,
    COUNT(CASE WHEN created_at >= CURRENT_DATE - 7 THEN 1 END) as new_customers_7d
FROM ecommerce.customers.profiles;

-- Product performance
SELECT 'Product Performance' as report_section;

SELECT 
    p.category,
    COUNT(*) as product_count,
    AVG(p.price) as avg_price,
    MIN(p.price) as min_price,
    MAX(p.price) as max_price
FROM ecommerce.products.catalog p
WHERE p.active = TRUE
GROUP BY p.category
ORDER BY avg_price DESC;

-- Schema statistics
SELECT 'Schema Statistics' as report_section;
SHOW SCHEMA HIERARCHY;
```

### **Maintenance Script**
```sql
-- maintenance.sql
-- Database maintenance operations

-- Update statistics for all tables
SET STATISTICS INDEX ALL;

-- Analyze table usage
SELECT 
    r.RDB$RELATION_NAME as table_name,
    r.RDB$OWNER_NAME as schema_name
FROM RDB$RELATIONS r 
WHERE r.RDB$SYSTEM_FLAG = 0
ORDER BY r.RDB$OWNER_NAME, r.RDB$RELATION_NAME;

-- Check foreign key constraints
SELECT 
    rc.RDB$CONSTRAINT_NAME,
    rc.RDB$RELATION_NAME,
    rc.RDB$CONSTRAINT_TYPE
FROM RDB$RELATION_CONSTRAINTS rc
WHERE rc.RDB$CONSTRAINT_TYPE = 'FOREIGN KEY';

-- Backup recommendations
SELECT 'Last backup should be checked manually' as maintenance_note;
```

---

## 🔍 Debugging and Troubleshooting

### **Performance Analysis**
```sql
-- Enable performance monitoring
SET STATS ON;
SET PLAN ON;

-- Run your query
SELECT c.first_name, c.last_name, COUNT(o.order_id) as order_count
FROM ecommerce.customers.profiles c
LEFT JOIN ecommerce.orders.transactions o ON c.customer_id = o.customer_id
WHERE c.created_at >= '2024-01-01'
GROUP BY c.customer_id, c.first_name, c.last_name
HAVING COUNT(o.order_id) > 5
ORDER BY order_count DESC;

-- Check execution plan and timing
```

### **Error Handling**
```sql
-- Enable error details
SET BAIL OFF;  -- Continue on errors for debugging

-- Test problematic query
SELECT * FROM non_existent_table;

-- Check for constraint violations
INSERT INTO ecommerce.products.catalog (product_id, sku, name, price)
VALUES (1, 'DUPLICATE-SKU', 'Test Product', -10.00);  -- Will fail due to CHECK constraint
```

### **Connection Issues**
```bash
# Test connection
sb_isql -z  # Check version

# Verbose connection debugging
sb_isql -user SYSDBA -password wrong_password -verbose mydatabase.fdb

# Test with trusted authentication
sb_isql -trusted mydatabase.fdb
```

---

## 🔧 Configuration

### **Environment Variables**
```bash
# Set default user
export ISC_USER=SYSDBA
export ISC_PASSWORD=masterkey

# Set default database
export ISC_DATABASE=/path/to/default.fdb

# Now you can connect without specifying credentials
sb_isql $ISC_DATABASE
```

### **Configuration Files**
Create `~/.isqlrc` for personal settings:
```sql
-- ~/.isqlrc - Personal ISQL settings
SET STATS ON;
SET PLAN OFF;
SET PAGESIZE 50;
SET SCHEMA 'default_schema';
```

### **Custom Aliases**
```bash
# Add to ~/.bashrc
alias isql-dev='sb_isql -user DEV_USER -password dev123 /path/to/dev.fdb'
alias isql-prod='sb_isql -user SYSDBA -trusted /path/to/prod.fdb'
alias isql-test='sb_isql -input test_suite.sql -bail /path/to/test.fdb'
```

---

## 📊 Integration Examples

### **Automated Reporting**
```bash
#!/bin/bash
# generate_report.sh - Automated report generation

DB_FILE="/data/production.fdb"
REPORT_DATE=$(date +%Y-%m-%d)
REPORT_FILE="/reports/daily_report_$REPORT_DATE.html"

# Generate HTML report
sb_isql -user REPORT_USER -password secret \
        -input report_queries.sql \
        -output "$REPORT_FILE" \
        -format html \
        "$DB_FILE"

# Email report
mail -s "Daily Database Report $REPORT_DATE" \
     -a "$REPORT_FILE" \
     admin@company.com < /dev/null
```

### **CI/CD Integration**
```bash
#!/bin/bash
# test_database.sh - Database testing in CI/CD

# Run database tests
sb_isql -input test_schema.sql -bail -quiet test.fdb
if [ $? -ne 0 ]; then
    echo "Schema tests failed"
    exit 1
fi

sb_isql -input test_data.sql -bail -quiet test.fdb
if [ $? -ne 0 ]; then
    echo "Data tests failed"
    exit 1
fi

sb_isql -input test_queries.sql -bail -quiet test.fdb
if [ $? -ne 0 ]; then
    echo "Query tests failed"
    exit 1
fi

echo "All database tests passed"
```

### **Data Migration**
```bash
#!/bin/bash
# migrate_data.sh - Data migration script

SOURCE_DB="old_system.fdb"
TARGET_DB="new_system.fdb"

# Export data from old system
sb_isql -user SYSDBA -password masterkey \
        -input export_data.sql \
        -output migration_data.sql \
        "$SOURCE_DB"

# Import into new system
sb_isql -user SYSDBA -password masterkey \
        -input migration_data.sql \
        -bail \
        "$TARGET_DB"

echo "Migration completed"
```

---

## 💡 Best Practices

### **Security**
```sql
-- Use roles instead of direct user permissions
CONNECT '/secure/database.fdb' USER 'app_user' PASSWORD 'secret' ROLE 'APPLICATION';

-- Avoid embedding passwords in scripts
-- Use environment variables or config files instead
```

### **Performance**
```sql
-- Always commit transactions promptly
INSERT INTO large_table (data) VALUES ('...');
COMMIT;  -- Don't leave transactions open

-- Use prepared statements for repeated queries
-- (Note: sb_isql automatically optimizes repeated queries)
```

### **Schema Organization**
```sql
-- Use hierarchical schemas logically
SET SCHEMA 'company.department.function';

-- Keep related objects together
CREATE TABLE company.finance.accounts (...);
CREATE VIEW company.finance.account_summary AS ...;
CREATE PROCEDURE company.finance.calculate_balance (...);
```

---

## 🆘 Troubleshooting

### **Common Issues**

**Issue**: "Connection failed" error
```bash
# Check if database exists
ls -la mydatabase.fdb

# Test with verbose output
sb_isql -verbose -user SYSDBA -password masterkey mydatabase.fdb
```

**Issue**: "Permission denied" error
```bash
# Check file permissions
ls -la mydatabase.fdb

# Try trusted authentication
sb_isql -trusted mydatabase.fdb
```

**Issue**: "Schema not found" error
```sql
-- Check available schemas
SHOW SCHEMAS;

-- Check current schema
SHOW SCHEMA;

-- Set correct schema
SET SCHEMA 'correct.schema.name';
```

### **Performance Issues**
```sql
-- Check execution plan
SET PLAN ON;
-- Run slow query

-- Update statistics
SET STATISTICS INDEX ALL;

-- Check for missing indexes
-- Look for NATURAL scans in execution plan
```

---

## 🎯 Next Steps

- **[sb_gbak - Backup/Restore](11-sb_gbak.md)** - Learn database backup operations
- **[SQL Language Guide](06-sql-language.md)** - Master ScratchBird SQL
- **[Hierarchical Schemas](07-hierarchical-schemas.md)** - Advanced schema design
- **[API Reference](17-api-reference.md)** - Programming with ScratchBird

## 📚 Related Documentation

- [**Command Reference**](29-command-reference.md) - Complete command reference
- [**Error Codes**](31-error-codes.md) - Error code reference
- [**Best Practices**](28-best-practices.md) - Recommended practices