## **In-Depth Analysis of the MERGE Statement**

The MERGE statement (sometimes called "upsert") performs INSERT, UPDATE, or DELETE operations on a target table based on a comparison with a source data set. It is a powerful tool for synchronizing data between two tables.

**Full Syntax:**

MERGE INTO target\_table \[AS alias\]  
USING source\_table \[AS alias\]  
ON join\_condition  
merge\_when\_clause \[merge\_when\_clause ...\]  
\[PLAN plan\_expression\]  
\[ORDER BY expression\_list\]  
\[RETURNING select\_list \[INTO variable\_list\]\]

### **Main Clause Breakdown**

* **MERGE INTO target\_table**: The table to be modified. Can be specified as \`table\_name AT link\_name\` to insert into a remote table.   
* **USING source\_table**: The data source containing the new or updated data. This can be a table, view, or derived table (a subquery).  
* **ON join\_condition**: The mandatory boolean condition that links rows from the target\_table to the source\_table, typically by matching primary or unique keys.

### **WHEN Clauses**

The MERGE statement's logic is defined in one or more WHEN clauses, which are evaluated for each row in the join.

#### **1\. WHEN MATCHED**

This clause specifies the action to take when a row from the source\_table finds a matching row in the target\_table based on the ON condition.

**Syntax:**

WHEN MATCHED \[AND search\_condition\] THEN { UPDATE SET ... | DELETE }

* **AND search\_condition**: (Optional) An additional condition that must be true for the action to be performed. This allows for conditional updates or deletes.  
* **THEN UPDATE SET ...**: Modifies the matching row in the target\_table. The SET clause can reference columns from both the source and target tables.  
* **THEN DELETE**: Deletes the matching row from the target\_table.

#### **2\. WHEN NOT MATCHED \[BY TARGET\]**

This clause specifies the action to take when a row from the source\_table does **not** find a matching row in the target\_table. This is typically used to insert new rows.

**Syntax:**

WHEN NOT MATCHED \[BY TARGET\] \[AND search\_condition\] THEN INSERT \[(column\_list)\] VALUES (value\_list)

* **BY TARGET**: This is the default and can be omitted.  
* **AND search\_condition**: (Optional) An additional filter on the source row before inserting.  
* **THEN INSERT ...**: Inserts a new row into the target\_table. The VALUES list can only reference columns from the source\_table.

#### **3\. WHEN NOT MATCHED BY SOURCE**

This clause specifies the action to take when a row in the target\_table does **not** find a matching row in the source\_table. This is useful for updating or deleting rows in the target that are no longer present in the source.

**Syntax:**

WHEN NOT MATCHED BY SOURCE \[AND search\_condition\] THEN { UPDATE SET ... | DELETE }

* **AND search\_condition**: (Optional) An additional filter on the target row before performing the action.  
* **THEN UPDATE SET ...**: Modifies the non-matching row in the target\_table. The SET clause can only reference columns from the target\_table and cannot use literal values.  
* **THEN DELETE**: Deletes the non-matching row from the target\_table.

### **Optional Final Clauses**

* **PLAN / ORDER BY / RETURNING**: These optional clauses function similarly to how they do in UPDATE or DELETE statements, allowing for plan specification, ordering (less common in MERGE), and returning values from the modified rows.
