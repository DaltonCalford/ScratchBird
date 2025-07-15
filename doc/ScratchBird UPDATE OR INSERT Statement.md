## **In-Depth Analysis of the UPDATE OR INSERT Statement**

The UPDATE OR INSERT statement is a ScratchBird-specific, non-standard "upsert" command. It provides a concise way to modify a row if it exists, or insert it if it does not, based on a set of matching columns.

**Full Syntax:**

UPDATE OR INSERT INTO table\_name \[(insert\_column\_list)\]  
    VALUES (value\_list)  
    \[MATCHING (matching\_column\_list)\]  
    \[RETURNING select\_list \[INTO variable\_list\]\]

### **Statement Logic**

1. The statement first attempts to find a row in table\_name where the values in the matching\_column\_list are equal to the corresponding values provided in the value\_list.  
2. **If a match is found**: The statement performs an UPDATE on that row. The columns to be updated are those specified in the insert\_column\_list, and their new values are taken from the value\_list.  
3. **If no match is found**: The statement performs an INSERT, creating a new row. The columns to be populated are those in the insert\_column\_list (or all columns if omitted), and the values are taken from the value\_list.

### **Clause Breakdown**

* **UPDATE OR INSERT INTO table\_name \[(insert\_column\_list)\]**:  
  * table\_name: The mandatory name of the table to be modified. Can be specified as \`table\_name AT link\_name\` to insert into a remote table.   
  * (insert\_column\_list): (Optional) A comma-separated list of columns. This list serves a dual purpose:  
    1. For the INSERT operation, it specifies which columns to populate.  
    2. For the UPDATE operation, it specifies which columns to modify in the SET clause (which is implicitly generated).  
* **VALUES (value\_list)**:  
  * A mandatory list of values. These values correspond to the columns in the insert\_column\_list. They are used to find a matching row, to update an existing row, and to insert a new row.  
* **MATCHING (matching\_column\_list)**: (Optional)  
  * This is the key clause that defines how to find an existing row.  
  * matching\_column\_list: A comma-separated list of columns to use for the search.  
  * **If MATCHING is omitted**, ScratchBird will use the table's defined **PRIMARY KEY** columns for the search. If there is no primary key, the statement will fail.  
* **RETURNING select\_list \[INTO variable\_list\]**: (Optional)  
  * Functions identically to the RETURNING clause in INSERT or UPDATE. It returns values from the row that was either inserted or updated.  
  * This is particularly useful for getting a primary key value, whether the row was just inserted or already existed.

### **Comparison to MERGE**

While both statements can perform "upsert" logic, UPDATE OR INSERT is simpler but less flexible than MERGE.

* **UPDATE OR INSERT**: Best for simple cases where you want to insert or update based on a primary or unique key. It cannot perform deletes or have complex conditional logic.  
* **MERGE**: A more powerful, SQL-standard statement that can handle complex synchronization logic, including conditional updates/inserts, deleting non-matching rows, and using different source and target tables.
