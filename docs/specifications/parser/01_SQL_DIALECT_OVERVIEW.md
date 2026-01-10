# **01\_SQL\_DIALECT\_OVERVIEW.md: SQL Dialect Overview**

## **1\. Introduction**

ScratchBird implements a comprehensive and powerful SQL dialect that merges standard SQL features with advanced capabilities inspired by PostgreSQL, Firebird, and other enterprise-grade databases. This document serves as the authoritative overview of the language, defining its core principles, features, and multi-dialect compatibility.

The language is designed around a modular parsing architecture and a multi-stage compilation pipeline, ensuring both maintainability and high performance.

### **Compilation and Execution Model**

Every SQL statement submitted to ScratchBird undergoes a sophisticated compilation process before execution:

1. **Parsing**: The raw SQL text is converted into an Abstract Syntax Tree (AST).  
2. **Semantic Analysis**: The AST is validated for correctness, and types are checked.  
3. **Bytecode Generation**: The validated AST is compiled into a highly efficient, platform-independent **ScratchBird Bytecode Language Representation (SBLR)**.  
4. **Optimization**: The SBLR code is passed through multiple optimization stages, including constant folding and dead code elimination.  
5. **Execution**: The final SBLR bytecode is executed by a high-performance, stack-based Virtual Machine (VM), which features adaptive specialization and optional Just-In-Time (JIT) compilation for frequently executed "hot" code paths.

## **2\. Core Principles**

### **Context-Aware Parsing**

The ScratchBird parser is designed to be intelligent and developer-friendly.

* **Minimal Reserved Words**: Keywords are only reserved in contexts where they would be ambiguous. For example, CREATE TABLE timestamp (timestamp TIMESTAMP); is a valid statement.  
* **Automatic Statement Termination**: The parser can often detect the end of a statement without a semicolon, making interactive sessions smoother.  
* **Intelligent Identifier Resolution**: Identifiers are resolved by checking scopes in a logical order (columns, then variables, then tables, etc.).  
* **Precise Error Reporting**: Syntax errors are reported with the exact line and column number, a visual indicator (^), and helpful hints for correction.

### **Comment Support & Automatic Documentation**

ScratchBird supports standard SQL comments and includes an intelligent documentation feature inspired by MSSQL.

\-- This is a single-line comment.

/\*  
  This is a  
  multi-line comment.  
\*/

\-- This comment will be automatically attached to the table below.  
CREATE TABLE customers (  
    \-- This comment will be attached to the id column.  
    id INT PRIMARY KEY,  
    name VARCHAR(100)  
);

\-- Retrieve the comment programmatically  
COMMENT ON TABLE customers IS 'This comment will be automatically attached...';

### **Multi-Dialect Compatibility**

The engine is built to understand and translate syntax from several popular SQL dialects, easing migration and improving tooling compatibility.

* **PostgreSQL**: :: type casting, $$ dollar-quoting, RETURNING clauses.  
* **MySQL**: \` backtick identifiers, LIMIT offset, count, SHOW commands.  
* **Firebird**: EXECUTE BLOCK, LIST() aggregate function, GEN\_ID() sequences.  
* **MSSQL**: \[ \] bracket identifiers, TOP N clauses, IDENTITY columns. (post-gold)

## **3\. Statement Categories**

The ScratchBird SQL dialect is organized into the following standard categories.

### **DDL (Data Definition Language)**

Used to define and manage the structure of the database and its objects.

* **Database Objects**: CREATE, ALTER, DROP for DATABASE, SCHEMA, TABLESPACE.  
* **Schema Objects**: CREATE, ALTER, DROP for TABLE, VIEW, INDEX, SEQUENCE, DOMAIN, TRIGGER, FUNCTION, PROCEDURE.  
* **Security Objects**: CREATE, ALTER, DROP for ROLE, USER.  
* **Advanced Objects**: MATERIALIZED VIEW, PACKAGE, EXCEPTION, LIBRARY.

### **DML (Data Manipulation Language)**

Used for querying, adding, modifying, and removing data.

* **SELECT**: A comprehensive implementation including Common Table Expressions (CTEs), window functions, lateral joins, and set operators.  
* **INSERT**: Supports multi-row VALUES clauses and INSERT ... SELECT. Includes ON CONFLICT for "upsert" operations.  
* **UPDATE**: Supports joins in the FROM clause for updating based on other tables.  
* **DELETE**: Supports joins in the USING clause.  
* **MERGE**: Provides powerful WHEN MATCHED and WHEN NOT MATCHED logic.  
* **COPY**: High-performance bulk data loading from files or STDIN.

### **DCL (Data Control Language)**

Used to manage permissions and access to the database.

* **GRANT**: Assigns privileges on objects to roles or users. Supports hierarchical permissions on schemas and role composition (groups).  
* **REVOKE**: Removes previously granted privileges.

### **TCL (Transaction Control Language)**

Used to manage transactions and ensure data integrity.

* **BEGIN / START TRANSACTION**: Starts a new transaction.  
* **COMMIT**: Saves the work done in a transaction.  
* **ROLLBACK**: Undoes the work done in a transaction.  
* **SAVEPOINT**: Sets an intermediate point within a transaction to which you can later roll back.

### **PSQL (Procedural SQL)**

A powerful procedural language for writing stored procedures, functions, and triggers.

* **Anonymous Blocks**: EXECUTE BLOCK for scripting without creating a permanent procedure.  
* **Autonomous Transactions**: Allows a block of code to commit or roll back independently of the main transaction.  
* **Control Flow**: IF, CASE, LOOP, WHILE, FOR statements.  
* **Advanced Features**: Universal cursors, custom exception handling with TRY/EXCEPT, SELECT INTO for variable assignment.

### **Utility Statements**

Commands for administration, diagnostics, and session management.

* **SHOW**: Displays information about database objects and system status.  
* **DESCRIBE**: Shows the structure of a table or view.  
* **EXPLAIN**: Displays the query execution plan. EXPLAIN ANALYZE executes the query and shows the actual plan with performance metrics.  
* **SET**: Modifies session parameters.
