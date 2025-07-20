# Quick Start Guide 🟢

Get ScratchBird up and running in just a few minutes! This guide will have you creating databases and running SQL queries in no time.

## 🚀 Installation (5 minutes)

### **Option 1: Download Pre-built Package (Recommended)**

#### **Linux** 🐧
```bash
# Download the latest release
wget https://github.com/dcalford/ScratchBird/releases/download/v0.5.0/scratchbird-v0.5.0-linux-x86_64.tar.gz

# Extract
tar -xzf scratchbird-v0.5.0-linux-x86_64.tar.gz
cd scratchbird-v0.5.0-linux-x86_64

# Install (optional - or run from current directory)
sudo ./install.sh

# Test installation
sb_isql -z
```

#### **Windows** 🪟
1. Download `scratchbird-v0.5.0-windows-x64.zip`
2. Extract to `C:\ScratchBird\`
3. Add `C:\ScratchBird\bin` to your PATH
4. Open Command Prompt and run: `sb_isql -z`

### **Option 2: Build from Source**
If you prefer to build from source, follow the [Installation Guide](03-installation.md).

## 🎯 Your First Database (2 minutes)

### **Step 1: Create a Database**
```bash
# Create a new database file
sb_isql -user SYSDBA -password masterkey /tmp/my_first_db.fdb

# You'll see this when connected:
# ScratchBird Interactive SQL Utility
# SB-T0.5.0.1 ScratchBird 0.5
# SQL>
```

> 💡 **Tip**: The database file will be created automatically when you first connect.

### **Step 2: Create Your First Table**
```sql
-- Create a simple customer table
CREATE TABLE customers (
    id INTEGER PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    email VARCHAR(100) UNIQUE,
    created_date DATE DEFAULT CURRENT_DATE
);

-- Confirm table was created
SHOW TABLES;
```

### **Step 3: Add Some Data**
```sql
-- Insert sample customers
INSERT INTO customers (id, name, email) VALUES (1, 'John Smith', 'john@example.com');
INSERT INTO customers (id, name, email) VALUES (2, 'Jane Doe', 'jane@example.com');
INSERT INTO customers (id, name, email) VALUES (3, 'Bob Wilson', 'bob@example.com');

-- Commit the changes
COMMIT;
```

### **Step 4: Query Your Data**
```sql
-- See all customers
SELECT * FROM customers;

-- Find a specific customer
SELECT * FROM customers WHERE name LIKE 'John%';

-- Count customers
SELECT COUNT(*) as total_customers FROM customers;
```

### **Step 5: Exit**
```sql
-- Exit the SQL shell
QUIT;
```

**🎉 Congratulations!** You've just created your first ScratchBird database!

---

## 🏗️ Real-World Example: Blog Database

Let's create a more realistic example - a simple blog database with hierarchical schemas.

### **Connect to a New Database**
```bash
sb_isql -user SYSDBA -password masterkey /tmp/blog.fdb
```

### **Create Hierarchical Schema Structure**
```sql
-- Create main blog schema
CREATE SCHEMA blog;

-- Create content sub-schema
CREATE SCHEMA blog.content;

-- Create user management sub-schema  
CREATE SCHEMA blog.users;

-- Create analytics sub-schema
CREATE SCHEMA blog.analytics;

-- Show our schema structure
SHOW SCHEMAS;
```

### **Create Tables in Different Schemas**
```sql
-- Users table in the users schema
CREATE TABLE blog.users.accounts (
    user_id INTEGER PRIMARY KEY,
    username VARCHAR(50) UNIQUE NOT NULL,
    email VARCHAR(100) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    is_admin BOOLEAN DEFAULT FALSE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Blog posts in the content schema
CREATE TABLE blog.content.posts (
    post_id INTEGER PRIMARY KEY,
    title VARCHAR(200) NOT NULL,
    content TEXT NOT NULL,
    author_id INTEGER NOT NULL,
    published BOOLEAN DEFAULT FALSE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (author_id) REFERENCES blog.users.accounts(user_id)
);

-- Page views in analytics schema
CREATE TABLE blog.analytics.page_views (
    view_id INTEGER PRIMARY KEY,
    post_id INTEGER,
    ip_address VARCHAR(45),
    user_agent TEXT,
    viewed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (post_id) REFERENCES blog.content.posts(post_id)
);
```

### **Add Sample Data**
```sql
-- Add users
INSERT INTO blog.users.accounts (user_id, username, email, password_hash, is_admin) 
VALUES (1, 'admin', 'admin@myblog.com', 'hashed_password_123', TRUE);

INSERT INTO blog.users.accounts (user_id, username, email, password_hash) 
VALUES (2, 'writer1', 'writer@myblog.com', 'hashed_password_456');

-- Add blog posts
INSERT INTO blog.content.posts (post_id, title, content, author_id, published) 
VALUES (1, 'Welcome to ScratchBird!', 'This is our first blog post about ScratchBird database...', 1, TRUE);

INSERT INTO blog.content.posts (post_id, title, content, author_id, published) 
VALUES (2, 'Getting Started with Hierarchical Schemas', 'Learn how to organize your data with nested schemas...', 2, TRUE);

-- Add some page views
INSERT INTO blog.analytics.page_views (view_id, post_id, ip_address) 
VALUES (1, 1, '192.168.1.100');

INSERT INTO blog.analytics.page_views (view_id, post_id, ip_address) 
VALUES (2, 1, '192.168.1.101');

INSERT INTO blog.analytics.page_views (view_id, post_id, ip_address) 
VALUES (3, 2, '192.168.1.100');

-- Commit all changes
COMMIT;
```

### **Query Across Schemas**
```sql
-- Get all published posts with author information
SELECT 
    p.title,
    p.content,
    u.username as author,
    p.created_at
FROM blog.content.posts p
JOIN blog.users.accounts u ON p.author_id = u.user_id
WHERE p.published = TRUE
ORDER BY p.created_at DESC;

-- Get post analytics
SELECT 
    p.title,
    COUNT(v.view_id) as total_views
FROM blog.content.posts p
LEFT JOIN blog.analytics.page_views v ON p.post_id = v.post_id
GROUP BY p.post_id, p.title
ORDER BY total_views DESC;

-- Find most active users
SELECT 
    u.username,
    COUNT(p.post_id) as posts_written,
    u.created_at as joined_date
FROM blog.users.accounts u
LEFT JOIN blog.content.posts p ON u.user_id = p.author_id
GROUP BY u.user_id, u.username, u.created_at
ORDER BY posts_written DESC;
```

---

## 🛠️ Essential Utilities Tour

Now let's explore the powerful utilities that come with ScratchBird.

### **Database Statistics**
```bash
# Get comprehensive database statistics
sb_gstat -user SYSDBA -password masterkey /tmp/blog.fdb

# Get table-specific statistics
sb_gstat -table blog.content.posts -user SYSDBA -password masterkey /tmp/blog.fdb
```

### **Backup Your Database**
```bash
# Create a backup
sb_gbak -backup -user SYSDBA -password masterkey /tmp/blog.fdb /tmp/blog_backup.fbk

# Restore from backup
sb_gbak -restore -user SYSDBA -password masterkey /tmp/blog_backup.fbk /tmp/blog_restored.fdb
```

### **Database Validation**
```bash
# Validate database integrity
sb_gfix -validate -user SYSDBA -password masterkey /tmp/blog.fdb

# Get database information
sb_gfix -info -user SYSDBA -password masterkey /tmp/blog.fdb
```

### **User Management**
```bash
# Add a new database user
sb_gsec -add newuser -password secretpass -fname "New" -lname "User"

# List all users
sb_gsec -display

# Modify user password
sb_gsec -modify newuser -password newpassword
```

---

## 🎯 Next Steps

### **For Beginners**
- **[First Database Tutorial](04-first-database.md)** - Deeper dive into database creation
- **[SQL Language Guide](06-sql-language.md)** - Learn ScratchBird SQL
- **[sb_isql Tutorial](10-sb_isql.md)** - Master the interactive SQL tool

### **For Developers**
- **[API Reference](17-api-reference.md)** - Programming with ScratchBird
- **[SBDatabase Class](18-sbdatabase-class.md)** - Database connectivity framework
- **[Error Handling](19-error-handling.md)** - Robust application development

### **For Administrators**
- **[Installation Guide](03-installation.md)** - Production installation
- **[Administrator Guide](21-admin-guide.md)** - Database administration
- **[Security Guide](08-security.md)** - Secure your databases

## 🆘 Need Help?

### **Common Issues**
- **Connection Problems**: Check [Troubleshooting](25-troubleshooting.md)
- **SQL Errors**: See [Error Codes](31-error-codes.md)
- **General Questions**: Visit [FAQ](26-faq.md)

### **Getting Support**
- 📖 **Documentation**: Browse the complete [Documentation Index](README.md)
- 💬 **Community**: Join the [ScratchBird Community Forum](https://community.scratchbird.org/)
- 🐛 **Issues**: Report bugs on [GitHub Issues](https://github.com/dcalford/ScratchBird/issues)

---

## 💡 Quick Tips

> **Use Schema Context**: Set your default schema to avoid typing full paths:
> ```sql
> SET SCHEMA 'blog.content';
> SELECT * FROM posts;  -- No need for blog.content.posts
> ```

> **Backup Regularly**: Always backup before major changes:
> ```bash
> sb_gbak -backup -user SYSDBA -password masterkey mydb.fdb mydb_backup.fbk
> ```

> **Monitor Performance**: Use statistics to understand your database:
> ```bash
> sb_gstat -header -user SYSDBA -password masterkey mydb.fdb
> ```

**🎉 You're now ready to build amazing applications with ScratchBird!**