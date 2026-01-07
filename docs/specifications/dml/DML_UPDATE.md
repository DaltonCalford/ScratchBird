# **UPDATE Statement Specification**

## **1\. Purpose**

The UPDATE statement is used to modify the values of existing rows in a table.

## **2\. Syntax**

\[ WITH cte\_name AS ( ... ) \[, ...\] \]  
UPDATE table\_name \[ \[ AS \] alias \]  
SET {  
      column\_name \= { expression | DEFAULT }  
    | ( column\_name \[, ...\] ) \= ( { expression | DEFAULT } \[, ...\] )  
    } \[, ...\]  
\[ FROM from\_list \]  
\[ WHERE condition | WHERE CURRENT OF cursor\_name \]  
\[ RETURNING { \* | output\_expression \[ \[ AS \] output\_name \] } \[, ...\] \];

## **3\. Clauses and Examples**

### **3.1. Basic UPDATE**

Modifies all rows that satisfy the WHERE clause. If WHERE is omitted, all rows in the table are updated.

\-- Increase the price of a single product  
UPDATE products  
SET unit\_price \= 25.50  
WHERE product\_id \= 101;

\-- Give a 10% discount to all products in a specific category  
UPDATE products  
SET unit\_price \= unit\_price \* 0.90  
WHERE category\_id \= 5;

\-- Update multiple columns at once  
UPDATE employees  
SET job\_title \= 'Senior Developer', salary \= 120000  
WHERE employee\_id \= 7;

### **3.2. UPDATE ... FROM (Update with Join)**

This powerful syntax allows you to join the target table to other tables in a FROM clause and use values from those other tables to determine which rows to update and what values to set.

\-- Update the inventory count in the products table based on a recent\_deliveries table  
UPDATE products  
SET units\_in\_stock \= products.units\_in\_stock \+ rd.quantity  
FROM recent\_deliveries AS rd  
WHERE products.product\_id \= rd.product\_id;

### **3.3. WHERE CURRENT OF (Cursor Update)**

Within a PSQL procedural block, this allows you to update the specific row that a cursor is currently positioned on. The cursor must have been declared with the FOR UPDATE option.

\-- In a PSQL function or procedure  
DECLARE  
    my\_cursor CURSOR FOR  
        SELECT \* FROM orders WHERE status \= 'PENDING' FOR UPDATE;  
BEGIN  
    OPEN my\_cursor;  
    LOOP  
        FETCH my\_cursor INTO ...;  
        EXIT WHEN NOT FOUND;

        \-- Update the row the cursor just fetched  
        UPDATE orders  
        SET status \= 'PROCESSING'  
        WHERE CURRENT OF my\_cursor;  
    END LOOP;  
    CLOSE my\_cursor;  
END;

### **3.4. Retrieving Results with RETURNING**

Similar to INSERT, the RETURNING clause can be used with UPDATE to return values from the rows that were just modified. This is useful for seeing the state of the data *after* the change.

\-- Update an account balance and return the new balance  
UPDATE accounts  
SET balance \= balance \- 100.00  
WHERE account\_id \= 'acc-123'  
RETURNING account\_id, balance AS new\_balance;

\-- Result of the query:  
\-- | account\_id | new\_balance |  
\-- |------------|-------------|  
\-- | 'acc-123'  | 9900.00     |  
