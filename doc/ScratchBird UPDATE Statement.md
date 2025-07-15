## **In-Depth Analysis of the UPDATE Statement**

The UPDATE statement is used to modify existing rows in a table. ScratchBird supports two main forms: the **searched update**, which modifies rows based on a search condition, and the **positioned update**, which modifies the row currently pointed to by a cursor.

### **1\. Searched UPDATE**

This is the most common form of the UPDATE statement. It finds rows matching a WHERE clause and applies changes to them.

**Full Syntax:**

UPDATE table\_name \[AS\] alias  
SET  
    assignment\_list  
\[WHERE search\_condition\]  
\[PLAN plan\_expression\]  
\[ORDER BY expression\_list\]  
\[ROWS n | ROWS m TO n\]  
\[SKIP LOCKED\]  
\[RETURNING select\_list \[INTO variable\_list\]\]

#### **Clause Breakdown:**

* **UPDATE table\_name \[AS\] alias**:  
  * table\_name: The mandatory name of the table to be updated. Can be specified as \`table\_name AT link\_name\` to insert into a remote table.   
  * alias: (Optional) A temporary alias for the table name, which can be used to qualify column names within the statement, especially in the WHERE clause if it involves subqueries.  
* **SET assignment\_list**:  
  * This is a mandatory clause specifying which columns to change and what their new values should be.  
  * assignment\_list: A comma-separated list of one or more assignments.  
  * **Assignment Syntax**: column\_name \= { value\_expression | DEFAULT }  
    * column\_name: The name of the column to modify.  
    * value\_expression: Any valid expression that resolves to a value compatible with the column's data type. This can be a literal, a variable, a function call, or an expression involving other columns from the same row.  
    * DEFAULT: Resets the column to its defined DEFAULT value.  
* **WHERE search\_condition**: (Optional)  
  * This clause filters which rows will be updated. If the WHERE clause is omitted, **all rows in the table will be updated**.  
  * search\_condition: A boolean expression that evaluates to TRUE for the rows that should be modified.  
* **PLAN plan\_expression**: (Optional)  
  * Provides an explicit query plan to the ScratchBird optimizer, overriding its default choices for how to find the rows to update. This is an advanced feature used for performance tuning.  
  * Example: PLAN (MY\_TABLE INDEX (MY\_INDEX))  
* **ORDER BY expression\_list**: (Optional)  
  * Used in conjunction with the ROWS clause to determine which rows are selected for the update when the result set is ordered. For example, you could update the 10 oldest orders.  
  * The syntax is identical to the SELECT statement's ORDER BY clause.  
* **ROWS Clause (Row Limiting)**: (Optional)  
  * Limits the number of rows that will be updated, based on the order defined by the ORDER BY clause.  
  * ROWS n: Updates the first n rows.  
  * ROWS m TO n: Updates rows from position m to n.  
  * This is a ScratchBird-specific extension.  
* **SKIP LOCKED**: (Optional)  
  * Instructs the engine to simply ignore (skip) any rows that it attempts to update but finds are already locked by another transaction. Without this clause, the UPDATE statement would wait for the lock to be released.  
* **RETURNING select\_list \[INTO variable\_list\]**: (Optional)  
  * Returns values from the modified rows. This is extremely useful for getting new values (like from a BEFORE UPDATE trigger) or the primary key of the updated row without needing a separate SELECT statement.  
  * select\_list: A list of columns or expressions to return (e.g., id, col1, OLD.col1, NEW.col2).  
  * INTO variable\_list: (For PSQL) Places the returned values into local variables.

### **2\. Positioned UPDATE**

This form of UPDATE is used within a PSQL block (BEGIN...END) to modify the single row that was most recently fetched by a declared cursor.

**Syntax:**

UPDATE table\_name  
SET assignment\_list  
WHERE CURRENT OF cursor\_name  
\[RETURNING select\_list \[INTO variable\_list\]\]

#### **Clause Breakdown:**

* **UPDATE table\_name / SET assignment\_list**: These are identical to their counterparts in the searched update.  
* **WHERE CURRENT OF cursor\_name**:  
  * This is the defining clause of a positioned update. It specifies that the update should apply only to the row currently held by the named cursor\_name.  
  * The cursor must have been declared with a FOR UPDATE clause.  
* **RETURNING ...**: (Optional) Identical in function to the RETURNING clause in a searched update, allowing you to capture values from the single row that was modified.
