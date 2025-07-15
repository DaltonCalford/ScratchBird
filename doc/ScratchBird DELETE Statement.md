## **In-Depth Analysis of the DELETE Statement**

The DELETE statement is used to remove rows from a table. Like the UPDATE statement, ScratchBird provides two primary forms: the **searched delete**, which removes all rows that match a specific condition, and the **positioned delete**, which removes the single row currently identified by a cursor.

### **1\. Searched DELETE**

This form of DELETE is used to remove one or more rows from a table based on a search condition.

**Full Syntax:**

DELETE FROM table\_name \[AS\] alias  
\[WHERE search\_condition\]  
\[PLAN plan\_expression\]  
\[ORDER BY expression\_list\]  
\[ROWS n | ROWS m TO n\]  
\[SKIP LOCKED\]  
\[RETURNING select\_list \[INTO variable\_list\]\]

#### **Clause Breakdown:**

* **DELETE FROM table\_name \[AS\] alias**:  
  * table\_name: The mandatory name of the table from which to delete rows. Can be specified as \`table\_name AT link\_name\` to insert into a remote table.   
  * alias: (Optional) A temporary alias for the table name. This is primarily useful within the WHERE clause if it contains subqueries that also reference the target table.  
* **WHERE search\_condition**: (Optional)  
  * This clause is crucial for controlling which rows are deleted. If the WHERE clause is omitted, **all rows in the table will be deleted**.  
  * search\_condition: A boolean expression that must evaluate to TRUE for a row to be removed.  
* **PLAN plan\_expression**: (Optional)  
  * An advanced feature that allows you to provide an explicit execution plan to the ScratchBird optimizer. This overrides the optimizer's default strategy for finding the rows to delete and is used for performance tuning.  
  * Example: PLAN (MY\_TABLE NATURAL) to force a table scan.  
* **ORDER BY expression\_list**: (Optional)  
  * This clause is used in conjunction with the ROWS clause. It sorts the set of rows that match the WHERE clause before the ROWS limit is applied. For example, you could use it to delete the 10 most recently added log entries.  
  * The syntax is identical to the ORDER BY clause in a SELECT statement.  
* **ROWS Clause (Row Limiting)**: (Optional)  
  * A ScratchBird-specific extension that limits the number of rows to be deleted from the set identified by the WHERE and ORDER BY clauses.  
  * ROWS n: Deletes the first n rows.  
  * ROWS m TO n: Deletes rows from position m to n.  
* **SKIP LOCKED**: (Optional)  
  * When the DELETE statement encounters a row that is locked by another transaction, this clause instructs the engine to simply skip that row and continue, rather than waiting for the lock to be released.  
* **RETURNING select\_list \[INTO variable\_list\]**: (Optional)  
  * Returns values from the rows that were just deleted. This is very useful for logging or auditing purposes, as it allows you to capture the state of a row at the moment of its deletion without a prior SELECT.  
  * select\_list: A list of columns or expressions to return.  
  * INTO variable\_list: (For PSQL) Places the returned values from the deleted row(s) into local variables. This only works if the DELETE is guaranteed to affect at most one row.

### **2\. Positioned DELETE**

This form of DELETE is used within a PSQL block (BEGIN...END). It deletes the single row that was most recently fetched by a specified cursor, providing a way to iterate through a result set and delete rows one by one.

**Syntax:**

DELETE FROM table\_name  
WHERE CURRENT OF cursor\_name  
\[RETURNING select\_list \[INTO variable\_list\]\]

#### **Clause Breakdown:**

* **DELETE FROM table\_name**: Specifies the table from which to delete, identical to the searched delete.  
* **WHERE CURRENT OF cursor\_name**:  
  * This is the key clause that distinguishes a positioned delete. It targets the single row currently pointed to by the cursor\_name.  
  * The cursor must have been declared with a FOR UPDATE clause to be eligible for use in a positioned DELETE.  
* **RETURNING ...**: (Optional)  
  * Functions identically to the RETURNING clause in a searched delete. It allows you to capture the values from the single row that was just deleted and, in PSQL, place them into variables.
