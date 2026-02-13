# **DML Statements Overview**

## **1\. Introduction**

Data Manipulation Language (DML) statements are the cornerstone of interacting with data stored in a ScratchBird database. These commands are used to retrieve, add, modify, and remove data from tables.

ScratchBird's DML implementation is both powerful and flexible, adhering to SQL standards while incorporating advanced features and compatibility with other major database dialects.

### **Core DML Principles in ScratchBird**

* **Comprehensive SELECT**: The SELECT statement is fully featured, supporting advanced analytics through window functions, complex query organization with recursive Common Table Expressions (CTEs), and modern join syntax including LATERAL joins.  
* **Powerful Data Modification**: INSERT, UPDATE, and DELETE statements are enhanced with a RETURNING clause, allowing them to return data from the rows that were modified. This is highly effective for getting default values, such as auto-generated primary keys, in a single round trip.  
* **Advanced "Upsert" Logic**: The dialect supports both the PostgreSQL-style INSERT ... ON CONFLICT statement for simple upsert operations and the more powerful, standard MERGE statement for complex conditional logic.  
* **High-Performance Bulk Operations**: The COPY command provides a highly optimized, streaming interface for loading and unloading large volumes of data to and from the database, bypassing the overhead of individual INSERT statements.

## **2\. DML Command Index**

The DML statements in ScratchBird are detailed in the following specifications. Each document covers the full syntax, options, and practical usage examples for the command.

| Command | Description | Detailed Specification |
| :---- | :---- | :---- |
| **SELECT** | Retrieves data from one or more tables, views, or functions. The most complex and powerful DML command. | [DML\_SELECT.md](DML_SELECT.md) |
| **INSERT** | Adds one or more new rows of data into a table. | [DML\_INSERT.md](DML_INSERT.md) |
| **UPDATE** | Modifies existing rows in a table that match specified criteria. | [DML\_UPDATE.md](DML_UPDATE.md) |
| **DELETE** | Removes existing rows from a table that match specified criteria. | [DML\_DELETE.md](DML_DELETE.md) |
| **MERGE** | Performs a conditional INSERT, UPDATE, or DELETE on a target table based on its comparison with a source dataset. | [DML\_MERGE.md](DML_MERGE.md) |
| **COPY** | A command for high-performance bulk data loading and unloading between a table and a file. | [DML_COPY.md](DML_COPY.md) |
