# Frequently Asked Questions (FAQ) 🟢

Common questions and answers about ScratchBird database system, organized by topic for easy navigation.

## 🏁 Getting Started

### **Q: What is ScratchBird and how is it different from Firebird?**
**A:** ScratchBird is an enhanced database system built on proven Firebird technology with modern improvements:

- **Hierarchical Schemas**: Create nested schemas like `company.finance.accounting.reports`
- **Enhanced Security**: Multi-factor authentication, advanced auditing, role-based access control
- **Modern Utilities**: 11 enhanced command-line tools with improved features
- **Performance Optimizations**: Better caching, parallel processing, intelligent optimization
- **100% Compatibility**: All existing Firebird applications work without changes

### **Q: Can I migrate from Firebird to ScratchBird?**
**A:** Yes! ScratchBird is 100% compatible with Firebird:

```bash
# Direct database migration - no conversion needed
cp my_firebird_database.fdb my_scratchbird_database.fdb

# Use ScratchBird tools directly
sb_isql -user SYSDBA -password masterkey my_scratchbird_database.fdb
```

Your existing applications, tools, and SQL scripts work without modification.

### **Q: What operating systems does ScratchBird support?**
**A:** ScratchBird runs on:
- **Linux**: All major distributions (Ubuntu, CentOS, RHEL, SUSE, etc.)
- **Windows**: Windows 10, 11, Server 2016/2019/2022
- **macOS**: Intel and Apple Silicon (M1/M2)
- **FreeBSD**: Version 12+

### **Q: How much does ScratchBird cost?**
**A:** ScratchBird is open source and free to use:
- ✅ **No licensing fees** for any use case
- ✅ **Commercial use allowed** without restrictions
- ✅ **No per-user or per-server charges**
- ✅ **Professional support available** (optional)

---

## 🏗️ Installation and Setup

### **Q: What are the minimum system requirements?**
**A:** ScratchBird has modest requirements:

**Minimum:**
- **RAM**: 512MB (1GB recommended)
- **Disk**: 100MB for installation + database storage
- **CPU**: Any modern processor (64-bit recommended)

**Recommended for production:**
- **RAM**: 4GB+ for better caching
- **Disk**: SSD for better performance
- **CPU**: Multi-core for parallel processing

### **Q: Do I need to install any dependencies?**
**A:** For most users, no additional dependencies are needed:

```bash
# ScratchBird includes everything needed
tar -xzf scratchbird-v0.5.0-linux-x86_64.tar.gz
cd scratchbird-v0.5.0-linux-x86_64
./install.sh
```

**Optional dependencies:**
- **readline**: For enhanced sb_isql editing (usually pre-installed)
- **compression libraries**: For advanced backup compression (included)

### **Q: Can I run ScratchBird without administrator privileges?**
**A:** Yes! ScratchBird can run entirely in user space:

```bash
# Extract to user directory
tar -xzf scratchbird-v0.5.0-linux-x86_64.tar.gz -C ~/scratchbird

# Add to PATH in ~/.bashrc
export PATH="$HOME/scratchbird/bin:$PATH"

# Create databases in user directory
sb_isql -user SYSDBA -password masterkey ~/my_database.fdb
```

### **Q: How do I uninstall ScratchBird?**
**A:** Use the included uninstaller:

```bash
# If installed system-wide
sudo /opt/scratchbird/uninstall.sh

# If installed to user directory
rm -rf ~/scratchbird
# Remove from PATH in ~/.bashrc
```

Your databases remain untouched - back them up separately if needed.

---

## 🔧 Database Operations

### **Q: How do I create my first database?**
**A:** Creating a database is simple:

```bash
# Method 1: Using sb_isql
sb_isql -user SYSDBA -password masterkey
```
```sql
SQL> CREATE DATABASE '/path/to/my_database.fdb'
CON> USER 'SYSDBA' PASSWORD 'masterkey';
SQL> QUIT;
```

```bash
# Method 2: Using API
```
```cpp
SBDatabase db;
db.connect("/path/to/my_database.fdb", "SYSDBA", "masterkey");
// Database is created automatically if it doesn't exist
```

### **Q: What are hierarchical schemas and how do I use them?**
**A:** Hierarchical schemas allow nested organization like a file system:

```sql
-- Create nested schema structure
CREATE SCHEMA company;
CREATE SCHEMA company.finance;
CREATE SCHEMA company.finance.accounting;
CREATE SCHEMA company.finance.accounting.reports;

-- Create table in nested schema
CREATE TABLE company.finance.accounting.reports.monthly_summary (
    report_id INTEGER PRIMARY KEY,
    month_year VARCHAR(7),
    total_revenue DECIMAL(15,2)
);

-- Set working schema to avoid typing full paths
SET SCHEMA 'company.finance.accounting';
SELECT * FROM reports.monthly_summary;
```

### **Q: How do I backup and restore databases?**
**A:** ScratchBird provides multiple backup options:

```bash
# Standard backup
sb_gbak -backup -user SYSDBA -password masterkey database.fdb backup.fbk

# Compressed backup
sb_gbak -backup -compress -user SYSDBA database.fdb backup.fbk

# Incremental backup (ScratchBird enhancement)
sb_nbackup -level 0 -user SYSDBA database.fdb full_backup.nb
sb_nbackup -level 1 -user SYSDBA database.fdb incremental_1.nb

# Restore database
sb_gbak -restore -user SYSDBA backup.fbk new_database.fdb
```

### **Q: How do I optimize database performance?**
**A:** Several optimization strategies:

```sql
-- 1. Update table statistics regularly
SET STATISTICS INDEX ALL;

-- 2. Create appropriate indexes
CREATE INDEX idx_customer_email ON customers (email);
CREATE INDEX idx_order_date ON orders (order_date);

-- 3. Use proper data types
-- Good: Use INTEGER for IDs
-- Bad: Use VARCHAR(100) for IDs

-- 4. Optimize queries
-- Good: SELECT specific_columns FROM table WHERE indexed_column = value
-- Bad: SELECT * FROM table WHERE unindexed_column LIKE '%pattern%'
```

```bash
# 5. Increase cache size in scratchbird.conf
DefaultDbCachePages = 10000  # Adjust based on available RAM

# 6. Regular maintenance
sb_gfix -validate database.fdb  # Weekly
sb_gbak -backup database.fdb backup.fbk  # Daily
```

---

## 🔐 Security and User Management

### **Q: How do I create users and manage permissions?**
**A:** Use sb_gsec or SQL commands:

```bash
# Using sb_gsec utility
sb_gsec -add newuser -password secret123 -fname "John" -lname "Doe"
sb_gsec -modify newuser -password newpassword
sb_gsec -delete olduser

# List all users
sb_gsec -display
```

```sql
-- Using SQL (in database)
CREATE USER appuser PASSWORD 'apppass123';
GRANT SELECT, INSERT, UPDATE ON customers TO appuser;

-- Create role
CREATE ROLE sales_team;
GRANT SELECT, INSERT ON orders TO sales_team;
GRANT sales_team TO appuser;
```

### **Q: How do I implement role-based security?**
**A:** Create roles and assign permissions:

```sql
-- Create roles for different access levels
CREATE ROLE read_only;
CREATE ROLE data_entry;
CREATE ROLE manager;
CREATE ROLE admin;

-- Grant permissions to roles
GRANT SELECT ON ALL TABLES IN SCHEMA company TO read_only;
GRANT SELECT, INSERT, UPDATE ON customer_data TO data_entry;
GRANT ALL ON company.finance TO manager;
GRANT ALL ON ALL SCHEMAS TO admin;

-- Assign roles to users
GRANT read_only TO report_user;
GRANT data_entry TO clerk;
GRANT manager TO supervisor;

-- Connect with role
CONNECT 'database.fdb' USER 'supervisor' PASSWORD 'pass' ROLE 'manager';
```

### **Q: How do I enable database encryption?**
**A:** ScratchBird supports multiple encryption options:

```sql
-- 1. Connection encryption (always enabled with SSL/TLS)
CONNECT 'database.fdb' USER 'user' PASSWORD 'pass' USING 'ssl';

-- 2. Backup encryption
```
```bash
sb_gbak -backup -encrypt -password encryption_key database.fdb encrypted_backup.fbk

# 3. Column-level encryption (application level)
```
```sql
CREATE TABLE secure_data (
    id INTEGER PRIMARY KEY,
    encrypted_ssn BLOB,  -- Store encrypted data
    public_name VARCHAR(100)
);
```

### **Q: How do I audit database activities?**
**A:** Enable auditing at multiple levels:

```sql
-- 1. Create audit table
CREATE TABLE audit_log (
    audit_id BIGINT PRIMARY KEY,
    table_name VARCHAR(63),
    operation VARCHAR(10),
    user_name VARCHAR(63),
    timestamp_val TIMESTAMP,
    old_values BLOB,
    new_values BLOB
);

-- 2. Create audit triggers
CREATE TRIGGER customers_audit
FOR customers ACTIVE AFTER UPDATE OR INSERT OR DELETE
AS
BEGIN
    INSERT INTO audit_log (table_name, operation, user_name, timestamp_val)
    VALUES ('CUSTOMERS', 
            CASE WHEN INSERTING THEN 'INSERT'
                 WHEN UPDATING THEN 'UPDATE' 
                 WHEN DELETING THEN 'DELETE' END,
            USER, CURRENT_TIMESTAMP);
END;
```

```bash
# 3. Use trace manager for system-level auditing
sb_tracemgr -start -config audit_trace.conf database.fdb
```

---

## 🛠️ Development and Programming

### **Q: What programming languages can I use with ScratchBird?**
**A:** ScratchBird supports many languages:

**Native Support:**
- **C++**: SBDatabase framework with modern C++17 features
- **C**: Standard Firebird-compatible API

**Language Bindings:**
- **Python**: Via Python-fdb or native modules
- **Java**: Via Jaybird JDBC driver
- **Node.js**: Via node-firebird package
- **PHP**: Via php_pdo_firebird extension
- **C#/.NET**: Via FirebirdSql.Data.FirebirdClient
- **Delphi/Pascal**: Via FireDAC or IBX components

**REST API:**
- **Any language**: Via HTTP/JSON REST interface

### **Q: How do I connect to ScratchBird from my application?**
**A:** Examples for common languages:

**C++:**
```cpp
#include "sb_database.h"

SBDatabase db;
if (db.connect("database.fdb", "user", "password")) {
    std::vector<std::vector<std::string>> results;
    std::vector<std::string> columns;
    db.executeSelect("SELECT * FROM customers", results, columns);
}
```

**Python:**
```python
import fdb

con = fdb.connect(dsn='database.fdb', user='SYSDBA', password='masterkey')
cur = con.cursor()
cur.execute("SELECT * FROM customers")
rows = cur.fetchall()
```

**Node.js:**
```javascript
const Firebird = require('node-firebird');

Firebird.attach({
    host: 'localhost',
    database: 'database.fdb',
    user: 'SYSDBA',
    password: 'masterkey'
}, function(err, db) {
    db.query('SELECT * FROM customers', function(err, result) {
        console.log(result);
    });
});
```

### **Q: How do I handle errors in my application?**
**A:** Use appropriate error handling for your language:

**C++:**
```cpp
try {
    SBDatabase db;
    db.connect("database.fdb", "user", "password");
    db.executeQuery("INSERT INTO customers ...");
} catch (const SBException& e) {
    std::cerr << "Database error: " << e.what() 
              << " (Code: " << e.getErrorCode() << ")" << std::endl;
}
```

**Python:**
```python
import fdb

try:
    con = fdb.connect(dsn='database.fdb', user='user', password='password')
    cur = con.cursor()
    cur.execute("INSERT INTO customers ...")
    con.commit()
except fdb.Error as e:
    print(f"Database error: {e}")
    con.rollback()
```

### **Q: How do I use transactions properly?**
**A:** Follow transaction best practices:

```cpp
// C++ example
bool transferMoney(SBDatabase& db, int from_account, int to_account, double amount) {
    try {
        // Start transaction
        db.startTransaction();
        
        // Check balance
        // ... validation code ...
        
        // Update accounts
        db.executeUpdate("UPDATE accounts SET balance = balance - ? WHERE id = ?");
        db.executeUpdate("UPDATE accounts SET balance = balance + ? WHERE id = ?");
        
        // Commit if all successful
        return db.commitTransaction();
        
    } catch (const SBException& e) {
        // Rollback on any error
        db.rollbackTransaction();
        return false;
    }
}
```

---

## 🚀 Performance and Scaling

### **Q: How many concurrent users can ScratchBird handle?**
**A:** ScratchBird scales well:

**Typical Configurations:**
- **Small applications**: 10-50 concurrent users
- **Medium applications**: 100-500 concurrent users  
- **Large applications**: 1000+ concurrent users
- **Enterprise**: 10,000+ with clustering

**Factors affecting scalability:**
- Hardware specifications (RAM, CPU, storage)
- Query complexity and optimization
- Transaction length and frequency
- Network configuration

### **Q: How do I optimize for large datasets?**
**A:** Several strategies for big data:

```sql
-- 1. Partitioning (via schemas)
CREATE SCHEMA data_2024;
CREATE SCHEMA data_2025;
CREATE TABLE data_2024.sales (...);
CREATE TABLE data_2025.sales (...);

-- 2. Indexing strategy
CREATE INDEX idx_sales_date ON sales (sale_date);
CREATE INDEX idx_sales_customer ON sales (customer_id, sale_date);

-- 3. Query optimization
-- Good: Use indexed columns in WHERE clauses
SELECT * FROM sales WHERE sale_date >= '2024-01-01' AND customer_id = 123;

-- Bad: Functions in WHERE clause prevent index usage
SELECT * FROM sales WHERE YEAR(sale_date) = 2024;
```

```bash
# 4. Configuration tuning
# In scratchbird.conf:
DefaultDbCachePages = 50000    # Increase cache for large datasets
TempCacheLimit = 268435456     # 256MB temp cache
```

### **Q: Can ScratchBird handle real-time applications?**
**A:** Yes, ScratchBird is excellent for real-time use:

- **Low latency**: Sub-millisecond response times for simple queries
- **High throughput**: Thousands of transactions per second
- **MVCC**: No blocking between readers and writers
- **Optimistic locking**: Minimal lock contention

**Best practices for real-time applications:**
- Keep transactions short
- Use appropriate indexes
- Monitor cache hit ratios
- Consider connection pooling

---

## 🔄 Migration and Integration

### **Q: How do I migrate from MySQL/PostgreSQL/SQL Server?**
**A:** Migration strategies vary by source database:

**Schema Migration:**
```sql
-- Convert data types
-- MySQL BIGINT UNSIGNED -> ScratchBird BIGINT
-- PostgreSQL SERIAL -> ScratchBird INTEGER + GENERATOR
-- SQL Server NVARCHAR -> ScratchBird VARCHAR CHARACTER SET UTF8

-- Convert features
-- MySQL AUTO_INCREMENT -> ScratchBird GENERATOR + TRIGGER
-- PostgreSQL SEQUENCE -> ScratchBird GENERATOR
-- SQL Server IDENTITY -> ScratchBird GENERATOR + TRIGGER
```

**Data Migration:**
```bash
# 1. Export from source database
mysqldump -u user -p database > mysql_dump.sql

# 2. Convert SQL (manual or tools)
# 3. Import to ScratchBird
sb_isql -input converted_script.sql -user SYSDBA new_database.fdb
```

**Migration Tools:**
- Database-specific migration scripts
- Third-party migration tools
- Custom ETL processes

### **Q: Can I use ScratchBird with my existing application framework?**
**A:** Yes, ScratchBird works with popular frameworks:

**Web Frameworks:**
- **Django** (Python): Via django-firebird package
- **Spring Boot** (Java): Via Jaybird JDBC driver
- **Express.js** (Node.js): Via node-firebird package
- **ASP.NET** (C#): Via FirebirdSql.Data.FirebirdClient
- **Laravel** (PHP): Via PDO Firebird driver

**ORM Support:**
- **SQLAlchemy** (Python)
- **Hibernate** (Java)
- **Entity Framework** (C#)
- **TypeORM** (TypeScript/Node.js)

### **Q: How do I integrate ScratchBird with cloud services?**
**A:** Several cloud integration options:

**Container Deployment:**
```dockerfile
FROM ubuntu:22.04
COPY scratchbird-v0.5.0-linux-x86_64.tar.gz /tmp/
RUN tar -xzf /tmp/scratchbird-v0.5.0-linux-x86_64.tar.gz -C /opt/
ENV PATH="/opt/scratchbird/bin:$PATH"
EXPOSE 3050
CMD ["sb_server", "-daemon"]
```

**Cloud Storage:**
- Store databases on cloud volumes (EBS, Azure Disks, etc.)
- Use cloud backup services for database backups
- Implement cloud-based disaster recovery

**Monitoring Integration:**
- Export metrics to CloudWatch, Azure Monitor, etc.
- Use cloud logging services
- Set up cloud-based alerting

---

## 🆘 Troubleshooting

### **Q: What should I do if my database is corrupted?**
**A:** Follow these steps in order:

```bash
# 1. Don't panic - make a copy first
cp corrupted.fdb corrupted_backup.fdb

# 2. Try validation
sb_gfix -validate corrupted.fdb

# 3. Try repair if validation shows errors
sb_gfix -mend corrupted.fdb

# 4. If repair fails, try backup/restore
sb_gbak -backup -ignore_checksums corrupted.fdb emergency.fbk
sb_gbak -restore emergency.fbk recovered.fdb

# 5. If backup fails, try data pumping
sb_isql -extract corrupted.fdb > schema.sql
# Then manually extract data table by table
```

### **Q: Why are my queries running slowly?**
**A:** Common performance issues and solutions:

```sql
-- 1. Check if indexes are being used
SET PLAN ON;
SELECT * FROM customers WHERE email = 'user@example.com';
-- Look for "NATURAL" (table scan) vs "INDEX" usage

-- 2. Update statistics
SET STATISTICS INDEX ALL;

-- 3. Create missing indexes
CREATE INDEX idx_customer_email ON customers (email);

-- 4. Optimize query structure
-- Good: Use specific columns
SELECT customer_id, name FROM customers WHERE active = 1;
-- Bad: Use SELECT * on large tables
SELECT * FROM customers;
```

### **Q: How do I resolve connection issues?**
**A:** Check these common causes:

```bash
# 1. Verify database file exists
ls -la /path/to/database.fdb

# 2. Check file permissions
chmod 664 /path/to/database.fdb

# 3. Test with verbose output
sb_isql -verbose -user SYSDBA -password masterkey database.fdb

# 4. Verify user credentials
sb_gsec -display

# 5. Check if database is shutdown
sb_gfix -info database.fdb
```

---

## 🎯 Best Practices

### **Q: What are the essential best practices for ScratchBird?**
**A:** Key recommendations:

**Database Design:**
- Use appropriate data types (don't over-size VARCHAR fields)
- Create indexes on foreign keys and frequently queried columns
- Design hierarchical schemas logically
- Implement proper constraints

**Performance:**
- Update statistics regularly: `SET STATISTICS INDEX ALL`
- Monitor cache hit ratios: `sb_gstat -cache database.fdb`
- Keep transactions short
- Use connection pooling for high-traffic applications

**Security:**
- Use roles instead of direct user permissions
- Implement audit trails for sensitive data
- Use strong passwords and consider multi-factor authentication
- Regular security reviews

**Maintenance:**
- Daily backups: `sb_gbak -backup database.fdb backup.fbk`
- Weekly validation: `sb_gfix -validate database.fdb`
- Monthly statistics updates: `SET STATISTICS INDEX ALL`
- Monitor disk space and database growth

**Development:**
- Always use transactions for data modifications
- Implement proper error handling
- Use prepared statements for repeated queries
- Test schema changes in development first

---

## 📚 Additional Resources

### **Q: Where can I get more help?**
**A:** Multiple support options:

**Documentation:**
- Complete documentation at `/doc/documentation/`
- Specific guides for each utility and feature
- API reference and examples

**Community:**
- ScratchBird Community Forum: Discussion and Q&A
- GitHub Issues: Bug reports and feature requests
- Stack Overflow: Tag questions with "scratchbird"

**Professional Support:**
- Consulting services for migration and optimization
- Training programs for teams
- Enterprise support agreements

### **Q: How do I stay updated with ScratchBird developments?**
**A:** Stay connected:

- **GitHub**: Watch the ScratchBird repository for updates
- **Release Notes**: Check release notes for new versions
- **Blog**: Follow the ScratchBird blog for tips and updates
- **Newsletter**: Subscribe to the monthly newsletter

### **Q: Can I contribute to ScratchBird development?**
**A:** Absolutely! Contributions are welcome:

- **Code contributions**: Submit pull requests on GitHub
- **Documentation**: Help improve documentation
- **Testing**: Test new features and report issues
- **Community**: Help answer questions in forums

---

## 💡 Quick Tips

> **Schema Organization**: Use hierarchical schemas to organize related objects logically
> ```sql
> CREATE SCHEMA company.finance.accounts;
> CREATE SCHEMA company.finance.reporting;
> ```

> **Backup Strategy**: Implement both full and incremental backups
> ```bash
> # Weekly full backup
> sb_gbak -backup database.fdb weekly_full.fbk
> # Daily incremental backup  
> sb_nbackup -level 1 database.fdb daily_incr.nb
> ```

> **Performance Monitoring**: Regular performance checks
> ```bash
> # Check cache efficiency (aim for >90% hit ratio)
> sb_gstat -cache database.fdb
> ```

Still have questions? Check the [**Troubleshooting Guide**](25-troubleshooting.md) or visit the [**Complete Documentation Index**](README.md)!