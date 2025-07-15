## **In-Depth Analysis of Logical Replication Syntax**

ScratchBird supports logical replication, which allows changes from one database (the publisher) to be streamed to other databases (subscribers). The core concept on the publisher's side is the **PUBLICATION**, which defines the set of tables whose changes (INSERT, UPDATE, DELETE) will be published.

The syntax in the parse.y file primarily manages this feature at the database and table levels, rather than through a standalone CREATE PUBLICATION object.

### **1\. Enabling and Disabling Replication at the Database Level**

Before any tables can be published, logical replication must be enabled for the entire database.

**Syntax:**

ALTER DATABASE ENABLE PUBLICATION;

ALTER DATABASE DISABLE PUBLICATION;

* **ENABLE PUBLICATION**: This command "switches on" the logical replication feature for the database. The engine will begin logging changes for tables that are included in the publication set.  
* **DISABLE PUBLICATION**: This command "switches off" the feature. The engine will stop logging changes for replication purposes.

### **2\. Managing the Default Publication Set**

By default, ScratchBird operates with a single, database-wide publication. You can control which tables are part of this set using the ALTER DATABASE statement.

**Syntax:**

ALTER DATABASE INCLUDE {ALL | TABLE table\_list} TO PUBLICATION;

ALTER DATABASE EXCLUDE {ALL | TABLE table\_list} FROM PUBLICATION;

#### **Clause Breakdown:**

* **INCLUDE ... TO PUBLICATION**: Adds tables to the set of tables whose changes will be replicated.  
  * INCLUDE ALL: Includes all user tables in the database in the publication.  
  * INCLUDE TABLE table\_list: Adds one or more specific tables to the publication. table\_list is a comma-separated list of table names.  
* **EXCLUDE ... FROM PUBLICATION**: Removes tables from the publication set.  
  * EXCLUDE ALL: Removes all user tables from the publication.  
  * EXCLUDE TABLE table\_list: Removes one or more specific tables from the publication.

**Example:**

\-- Start by publishing only the products and categories tables  
ALTER DATABASE INCLUDE TABLE PRODUCTS, CATEGORIES TO PUBLICATION;

\-- Later, remove the categories table  
ALTER DATABASE EXCLUDE TABLE CATEGORIES FROM PUBLICATION;

### **3\. Table-Level Publication Control**

You can also specify whether a table should be included in the publication set at the time of its creation.

**Syntax (within CREATE TABLE):**

CREATE TABLE table\_name (  
    ...  
)  
\[ENABLE PUBLICATION | DISABLE PUBLICATION\];

* **ENABLE PUBLICATION**: (Default) The newly created table will automatically be included in the database's publication set.  
* **DISABLE PUBLICATION**: The newly created table will be excluded from the publication set, even if an ALTER DATABASE INCLUDE ALL command is run later. Its publication status must be managed explicitly.