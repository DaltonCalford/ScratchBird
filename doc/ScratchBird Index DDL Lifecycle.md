## **In-Depth Analysis of Index DDL and Statistics**

An index is a database structure that improves the speed of data retrieval operations on a table at the cost of additional writes and storage space. ScratchBird uses indices to quickly locate data without having to scan every row.

### **CREATE INDEX**

This statement defines a new index on a table.

**Syntax:**

CREATE \[UNIQUE\] \[ASC | DESC\] INDEX \[IF NOT EXISTS\] index\_name  
    ON table\_name  
    { (column\_list) | COMPUTED BY (expression) }

#### **Clause Breakdown:**

* **UNIQUE**: (Optional) Ensures that all values in the indexed column(s) are unique. An attempt to insert a duplicate value will result in an error.  
* **ASC | DESC**: (Optional) Specifies the sort direction of the index. ASC (ascending) is the default. A descending index (DESC) can improve performance for queries that sort in descending order or find MIN/MAX values.  
* **IF NOT EXISTS**: Prevents an error if an index with the same name already exists.  
* **index\_name**: The name of the new index.  
* **ON table\_name**: The mandatory table on which the index is created.  
* **Index Type**: You must specify one of the following:  
  * **(column\_list)**: A standard index on one or more columns. For a composite index, list the columns in order of significance.  
  * **COMPUTED BY (expression)**: An expression-based index. The index is built on the result of the expression rather than on the raw column data. This is useful for indexing the result of a function call or a calculation.

### **ALTER INDEX**

In ScratchBird, ALTER INDEX has a very specific purpose: to activate or deactivate an index. You cannot use it to change the columns or expression of an index; for that, you must DROP and CREATE it again.

**Syntax:**

ALTER INDEX index\_name {ACTIVE | INACTIVE}

* **ACTIVE**: The index is used by the query optimizer. This is the default state.  
* **INACTIVE**: The index is maintained (updated on DML operations) but is ignored by the query optimizer. This is useful for temporarily disabling an index for performance testing without the cost of rebuilding it.

### **DROP INDEX**

Permanently deletes an index from the database.

**Syntax:**

DROP INDEX \[IF EXISTS\] index\_name;

* **IF EXISTS**: Prevents an error if the index does not exist.

### **SET STATISTICS**

This command forces a recalculation of the statistics for a given index. The query optimizer relies on index statistics (specifically, its selectivity) to choose the most efficient query plan. While ScratchBird updates statistics automatically, this command is useful after a massive data load or change where the data distribution has significantly shifted.

**Syntax:**

SET STATISTICS INDEX index\_name;

This command prompts the engine to re-scan the index and update its internal metadata about the key distribution.