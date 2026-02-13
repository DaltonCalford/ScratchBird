# **02\_DDL\_STATEMENTS\_OVERVIEW.md: DDL Statements Overview**

## **1\. Introduction**

Data Definition Language (DDL) statements are used to define, modify, and remove the structure of database objects in ScratchBird. The dialect provides a rich set of DDL commands for managing everything from top-level databases down to individual table columns and procedural code modules.

### **Core DDL Principles in ScratchBird**

* **Transactional DDL**: All DDL commands are fully transactional. A CREATE TABLE or ALTER VIEW statement can be included within a BEGIN...COMMIT/ROLLBACK block. This allows for complex schema migrations to be applied atomically, ensuring the database schema is always in a consistent state.  
* **IF \[NOT\] EXISTS Clause**: Most CREATE and DROP statements support the optional IF EXISTS or IF NOT EXISTS clause, which turns a potential error into a notice, simplifying scripting and automated deployments.  
* **UUID-Based Object Identity**: While users interact with objects via human-readable names, all objects within the system catalog are uniquely identified by a UUID. This internal mechanism allows objects like tables, views, and procedures to be renamed without breaking dependencies in compiled procedural code (SBLR).

## **2\. DDL Command Categories**

The DDL statements in ScratchBird can be grouped into the following logical categories. Each category represents a core aspect of the database architecture.

[Image of a database schema diagram](https://encrypted-tbn3.gstatic.com/licensed-image?q=tbn:ANd9GcQKeHjMOC7bbZ1etC9niVszwh0kgylLrL9Ft2DyBwLkSQwP0isT1euKD303uy_31aLiFa0tCKHuArK8VDrGstXqxBubnX5zNarrM8Dr2jmksm0luC0)

### **2.1. Foundational Objects**

These objects define the fundamental structure and storage of the database.

| Object | Description | Detailed Specification |
| :---- | :---- | :---- |
| **DATABASE** | The top-level container for all schemas, tables, and other objects. Manages global settings like page size and default character set. | [DDL\_DATABASES.md](DDL_DATABASES.md) |
| **SCHEMA** | A namespace within a database that contains a logical grouping of objects like tables, views, and functions. Essential for organizing large applications and managing permissions. | [DDL\_SCHEMAS.md](DDL_SCHEMAS.md) |

### **2.2. Data Storage Objects**

These objects are directly responsible for storing and organizing data.

| Object | Description | Detailed Specification |
| :---- | :---- | :---- |
| **TABLE** | The primary data structure, consisting of columns and rows. ScratchBird supports temporary, external, and partitioned tables. | [DDL\_TABLES.md](DDL_TABLES.md) |
| **INDEX** | A performance-optimization structure that provides faster retrieval of rows from a table. Supports various methods like B-tree, Hash, and GIN. | [DDL\_INDEXES.md](DDL_INDEXES.md) |
| **VIEW** | A virtual table based on the result-set of a SELECT statement. Simplifies complex queries and provides an abstraction layer for security. | [DDL\_VIEWS.md](DDL_VIEWS.md) |
| **SEQUENCE** | A user-defined object that generates a sequence of integers. Commonly used for creating unique primary key values. Also known as a GENERATOR. | [DDL\_SEQUENCES.md](DDL_SEQUENCES.md) |

### **2.3. Type System and Integrity Objects**

These objects allow for the creation of custom data types and powerful, reusable integrity rules.

| Object | Description | Detailed Specification |
| :---- | :---- | :---- |
| **TYPE** | A reusable user-defined type (enum, record, range, base) that can be referenced by columns, domains, and functions. | [DDL\_TYPES.md](DDL_TYPES.md) |
| **DOMAIN** | A user-defined data type with optional constraints (NOT NULL, CHECK). ScratchBird extends this concept to include complex records, enums, sets, and embedded security rules. | [DDL\_DOMAINS\_COMPREHENSIVE.md](../types/DDL_DOMAINS_COMPREHENSIVE.md) |

### **2.4. Programmability Objects**

These objects enable complex business logic to be stored and executed directly within the database.

| Object | Description | Detailed Specification |
| :---- | :---- | :---- |
| **FUNCTION** | A subprogram that performs a calculation and must return a single value or a set of rows (SETOF). Can be used in DML statements. | [DDL\_FUNCTIONS.md](DDL_FUNCTIONS.md) |
| **PROCEDURE** | A subprogram that performs an action. Can control transactions and does not have to return a value. Called explicitly with CALL. | [DDL\_PROCEDURES.md](DDL_PROCEDURES.md) |
| **TRIGGER** | A procedure that is automatically executed in response to DML events (INSERT, UPDATE, DELETE) on a specified table. | [DDL\_TRIGGERS.md](DDL_TRIGGERS.md) |
| **EXCEPTION** | A user-defined, named error condition that can be raised and handled within the PSQL procedural language. | [DDL\_EXCEPTIONS.md](DDL_EXCEPTIONS.md) |
| **PACKAGE** | A schema object that encapsulates logically related types, variables, procedures, and functions into a single, managed unit. | [DDL\_PACKAGES.md](DDL_PACKAGES.md) |

### **2.5. Security Objects**

These objects are the foundation of the database's access control system.

| Object | Description | Detailed Specification |
| :---- | :---- | :---- |
| **ROLE & GROUP** | A security principal that can be granted privileges and can own objects. A role with LOGIN is a "user"; a role without LOGIN is a "group" for bundling permissions. Supports hierarchical inheritance. | [DDL\_ROLES\_AND\_GROUPS.md](DDL_ROLES_AND_GROUPS.md) |

### **2.6. Extensibility Objects**

These objects allow for extending the database's native capabilities with external code.

| Object | Description | Detailed Specification |
| :---- | :---- | :---- |
| **USER-DEFINED RESOURCE** | A mechanism for creating functions in external languages (e.g., C, Python). Consists of a LIBRARY object pointing to a shared library file and FUNCTION definitions bound to it. | [DDL\_USER\_DEFINED\_RESOURCES.md](DDL_USER_DEFINED_RESOURCES.md) |
