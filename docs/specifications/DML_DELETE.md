# **DELETE Statement Specification**

## **1\. Purpose**

The DELETE statement is used to remove existing rows from a table.

## **2\. Syntax**

\[ WITH cte\_name AS ( ... ) \[, ...\] \]  
DELETE FROM table\_name \[ \[ AS \] alias \]  
\[ USING using\_list \]  
\[ WHERE condition | WHERE CURRENT OF cursor\_name \]  
\[ RETURNING { \* | output\_expression \[ \[ AS \] output\_name \] } \[, ...\] \];

## **3\. Clauses and Examples**

### **3.1. Basic DELETE**

Removes all rows that satisfy the WHERE clause. If the WHERE clause is omitted, all rows are removed from the table, but the table structure itself remains.

\-- Delete a specific order  
DELETE FROM orders  
WHERE order\_id \= 10248;

\-- Delete all orders placed before a certain date  
DELETE FROM orders  
WHERE order\_date \< '2022-01-01';

**Warning**: DELETE FROM table\_name; without a WHERE clause will empty the entire table. For large tables, TRUNCATE TABLE table\_name; is a much faster, non-transactional alternative if you do not need to log individual row deletions.

### **3.2. DELETE ... USING (Delete with Join)**

This syntax allows you to reference columns from other tables in the WHERE clause to determine which rows to delete from the target table.

\-- Delete all customers who have not placed an order in over 5 years  
DELETE FROM customers  
USING (  
    \-- Subquery to find the last order date for each customer  
    SELECT customer\_id, MAX(order\_date) as last\_order\_date  
    FROM orders  
    GROUP BY customer\_id  
) AS last\_orders  
WHERE customers.customer\_id \= last\_orders.customer\_id  
  AND last\_orders.last\_order\_date \< (CURRENT\_DATE \- INTERVAL '5 years');

### **3.3. WHERE CURRENT OF (Cursor Delete)**

Within a PSQL procedural block, this allows you to delete the specific row that a cursor is currently positioned on. The cursor must have been declared with the FOR UPDATE option.

\-- In a PSQL function or procedure  
DECLARE  
    log\_cursor CURSOR FOR  
        SELECT \* FROM system\_logs WHERE level \= 'DEBUG' FOR UPDATE;  
BEGIN  
    OPEN log\_cursor;  
    LOOP  
        FETCH log\_cursor INTO ...;  
        EXIT WHEN NOT FOUND;

        \-- Delete the log entry the cursor just fetched  
        DELETE FROM system\_logs  
        WHERE CURRENT OF log\_cursor;  
    END LOOP;  
    CLOSE log\_cursor;  
END;

### **3.4. Retrieving Results with RETURNING**

The RETURNING clause can be used with DELETE to return the data from the rows that were just removed. This is useful for auditing, logging, or moving the deleted data to an archive table.

\-- Delete inactive users and return their data for logging  
DELETE FROM users  
WHERE last\_login \< (CURRENT\_DATE \- INTERVAL '2 years')  
RETURNING user\_id, username, last\_login;

\-- The result of this query can be directly inserted into an archive table.  
