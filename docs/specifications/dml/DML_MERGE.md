# **MERGE Statement Specification**

## **1\. Purpose**

The MERGE statement performs a series of INSERT, UPDATE, or DELETE operations on a target table based on the results of joining it with a source table or query. It is the most powerful tool for "upsert" or synchronization logic, as it can handle multiple conditions in a single atomic statement.

## **2\. Syntax**

\[ WITH cte\_name AS ( ... ) \[, ...\] \]  
MERGE INTO target\_table \[ \[ AS \] target\_alias \]  
USING source\_data \[ \[ AS \] source\_alias \]  
ON ( join\_condition )  
\[ WHEN MATCHED \[ AND condition \] THEN { merge\_update | merge\_delete } \] \[...\]  
\[ WHEN NOT MATCHED \[ BY TARGET \] \[ AND condition \] THEN merge\_insert \] \[...\]  
\[ WHEN NOT MATCHED BY SOURCE \[ AND condition \] THEN { merge\_update | merge\_delete } \] \[...\];

\-- Actions  
merge\_update ::= UPDATE SET { column \= value } \[, ...\]  
merge\_delete ::= DELETE  
merge\_insert ::= INSERT \[ ( column \[, ...\] ) \] VALUES ( value \[, ...\] )

## **3\. Clauses Explained**

* **MERGE INTO target\_table**: The table to be modified.  
* **USING source\_data**: A table, view, or subquery that provides the data for comparison.  
* **ON (join\_condition)**: The condition that links rows from the source to rows in the target. This is the core of the MERGE.  
* **WHEN MATCHED**: This clause specifies the action to take if a row from the source **finds a matching row** in the target based on the ON condition. The action is typically an UPDATE or DELETE.  
* **WHEN NOT MATCHED \[BY TARGET\]**: This clause specifies the action to take if a row from the source **does not find a matching row** in the target. The action is always an INSERT.  
* **WHEN NOT MATCHED BY SOURCE**: This clause specifies the action to take if a row in the target **does not find a matching row** in the source. The action is typically an UPDATE or DELETE (e.g., marking records as "inactive").

## **4\. Examples**

### **4.1. Basic Upsert (Insert or Update)**

This is the most common use case: synchronizing a target table with a source of new data.

\-- Source table with staging data  
CREATE TABLE customer\_updates (  
    customer\_id INT PRIMARY KEY,  
    new\_email VARCHAR(255),  
    last\_updated TIMESTAMP  
);

\-- Target table  
CREATE TABLE customers (  
    customer\_id INT PRIMARY KEY,  
    email VARCHAR(255),  
    created\_at TIMESTAMP,  
    updated\_at TIMESTAMP  
);

\-- MERGE statement to synchronize  
MERGE INTO customers AS t  \-- Target  
USING customer\_updates AS s \-- Source  
ON (t.customer\_id \= s.customer\_id) \-- Join condition

\-- If a customer exists, update their email and updated\_at timestamp  
WHEN MATCHED THEN  
    UPDATE SET  
        email \= s.new\_email,  
        updated\_at \= s.last\_updated

\-- If a customer is new, insert their record  
WHEN NOT MATCHED BY TARGET THEN  
    INSERT (customer\_id, email, created\_at, updated\_at)  
    VALUES (s.customer\_id, s.new\_email, s.last\_updated, s.last\_updated);

### **4.2. Full Synchronization (Insert, Update, Delete)**

This example extends the upsert to also handle records that exist in the target but not in the source (e.g., they should be deleted or marked inactive).

\-- Synchronize an inventory table with a master product list  
MERGE INTO inventory AS i  
USING products AS p  
ON (i.product\_id \= p.product\_id)

\-- Case 1: Product exists in inventory. Update its name from the master list.  
WHEN MATCHED AND i.product\_name \<\> p.product\_name THEN  
    UPDATE SET product\_name \= p.product\_name

\-- Case 2: Product is in the master list but not in inventory. Insert it.  
WHEN NOT MATCHED BY TARGET THEN  
    INSERT (product\_id, product\_name, quantity)  
    VALUES (p.product\_id, p.product\_name, 0\)

\-- Case 3: Product is in inventory but has been removed from the master list. Delete it.  
WHEN NOT MATCHED BY SOURCE THEN  
    DELETE;  
