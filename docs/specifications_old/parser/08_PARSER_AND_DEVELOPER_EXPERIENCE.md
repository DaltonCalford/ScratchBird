# **Parser and Developer Experience**

## **1\. Introduction**

Beyond the raw power of its SQL dialect, ScratchBird is defined by a deep focus on developer experience (DX). The parser and its related tooling are designed to be intelligent, intuitive, and helpful, minimizing friction and allowing developers to work more efficiently.

This specification details the user-facing features of the parser and the integrated tools that create a superior development environment.

### **Core DX Principles**

* **Be Forgiving and Explicit**: The parser should understand common developer patterns and dialect variations. When it cannot, it must provide clear, precise, and actionable error messages.  
* **Automate Documentation**: The system should leverage information already present in SQL scripts (like comments) to automatically generate and maintain schema documentation.  
* **Reduce Boilerplate**: Through features like context-aware parsing and automatic statement termination, the dialect reduces the number of strict rules a developer must remember.  
* **Provide Insight**: Integrated tools should make it easy to understand query performance and database behavior without leaving the development environment.

## **2\. Context-Aware Parsing**

The ScratchBird parser is designed to be "context-aware," which means it uses the surrounding syntax to understand the developer's intent. This allows for a more flexible and less restrictive syntax.

### **2.1. Minimal Reserved Words**

Unlike traditional SQL databases that have a long list of reserved keywords, ScratchBird reserves very few words globally (e.g., SELECT, CREATE, FROM). Most keywords are only considered reserved in specific contexts. This allows for more natural naming of tables and columns.

**Example: Using Keywords as Identifiers**

\-- This is a valid statement in ScratchBird because the parser understands  
\-- the context of where a table name or column name is expected.

CREATE TABLE "timestamp" (  
    "timestamp" TIMESTAMP,  
    "value" INTEGER,  
    "check" VARCHAR(10)  
);

SELECT "timestamp", "value" FROM "timestamp" WHERE "check" \= 'VALID';

### **2.2. Automatic Statement Termination**

The parser can detect the end of a statement without a semicolon (;) in many situations, such as in scripting or interactive REPL environments. It recognizes the start of a new, valid DDL or DML keyword as the end of the previous statement. While semicolons are still recommended for clarity in complex scripts, they are not always mandatory.

**Example: Implicit Termination**

\-- The parser correctly identifies these as two separate statements  
\-- without needing a semicolon.

SELECT \* FROM users  
UPDATE products SET price \= price \* 1.1

## **3\. Intelligent Comment Management**

ScratchBird treats comments not just as ignored text, but as a source of documentation. It implements an MSSQL-style system where comments written directly before an object or column are automatically captured and associated with that object.

### **3.1. Automatic COMMENT ON Association**

Comments are automatically converted into database object comments, which can then be queried from system catalogs.

**Example: Self-Documenting DDL**

\-- This is the primary table for storing customer data.  
\-- It includes contact information and status.  
CREATE TABLE customers (  
    \-- The unique identifier for each customer.  
    id UUID PRIMARY KEY,

    \-- The customer's full legal name. Must not be null.  
    name VARCHAR(100) NOT NULL,

    /\*  
     \* The customer's status.  
     \* A \= Active  
     \* I \= Inactive  
     \*/  
    status CHAR(1) DEFAULT 'A'  
);

**Resulting Actions**: The parser automatically executes the equivalent of these commands:

COMMENT ON TABLE customers IS 'This is the primary table for storing customer data. It includes contact information and status.';  
COMMENT ON COLUMN customers.id IS 'The unique identifier for each customer.';  
COMMENT ON COLUMN customers.name IS 'The customer''s full legal name. Must not be null.';  
COMMENT ON COLUMN customers.status IS 'The customer''s status. A \= Active, I \= Inactive';

### **3.2. Comment Preservation**

Associated comments are treated as part of the object's metadata. They are preserved during ALTER operations and are included when generating DDL with SHOW CREATE TABLE.

## **4\. Precise Error Reporting**

Clear and precise error reporting is a critical DX feature. When a query fails to parse, the ScratchBird engine provides detailed feedback to help the developer fix the issue quickly.

### **4.1. Exact Error Location with Visual Cues**

The parser must identify the exact line and column number of an error. It provides a visual indicator (^) pointing to the problematic token.

**Example: Syntax Error**

SELECT \* FROM users WHERE age \> AND name \= 'John';

**Parser Response:**

ERROR at line 1, column 34:  
  Syntax error: Expected expression after '\>' operator  
  Found: AND (keyword)  
  Expected: numeric value, column reference, or expression

  SELECT \* FROM users WHERE age \> AND name \= 'John';  
                                 ^

### **4.2. Context-Aware Messages and Hints**

Error messages are context-aware, providing more specific feedback based on what the parser was expecting. Helpful hints are provided for common mistakes.

**Example: Missing Comma**

CREATE TABLE products (  
    id INTEGER PRIMARY KEY,  
    name VARCHAR(100)  
    price DECIMAL(10,2)  
);

**Parser Response:**

ERROR at line 4, column 5:  
  Syntax error: Unexpected identifier  
  Found: 'price'  
  Expected: ',' or ')'

    price DECIMAL(10,2)  
    ^^^^^

Hint: You may be missing a comma after the previous column definition 'name VARCHAR(100)'.

### **4.3. Multiple Error Detection**

In scripting environments, the parser can continue after encountering an error to find subsequent issues in the same script, allowing a developer to fix multiple problems in one pass.

**Example: Multiple Errors in One Script**

CREATE TABLE bad\_table (  
    id INTEGER PRIMRY KEY,      \-- Typo 1  
    name VARCHR(100),           \-- Typo 2  
    age INTEGER DEFAULT 'text'  \-- Typo 3  
);

**Parser Response:**

ERRORS FOUND: 3

1\. Line 2, column 16: Unknown keyword 'PRIMRY'. Did you mean 'PRIMARY'?  
2\. Line 3, column 10: Unknown data type 'VARCHR'. Did you mean 'VARCHAR'?  
3\. Line 4, column 25: Type mismatch: Cannot assign TEXT literal to INTEGER column.

## **5\. Integrated Developer Tooling**

ScratchBird is designed to support a suite of integrated tools that provide insight and improve code quality. While these are often client-side features, the database engine provides the necessary hooks and commands to power them.

### **5.1. Query Plan Analysis (EXPLAIN)**

The EXPLAIN and EXPLAIN ANALYZE commands provide a detailed breakdown of the query execution plan, allowing developers to identify performance bottlenecks, missing indexes, and inefficient joins.

EXPLAIN ANALYZE SELECT \* FROM customers c  
JOIN orders o ON c.customer\_id \= o.customer\_id  
WHERE c.signup\_date \> '2024-01-01';  
