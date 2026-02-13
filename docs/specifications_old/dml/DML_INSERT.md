# **INSERT Statement Specification**

## **1\. Purpose**

The INSERT statement is used to add one or more new rows to a table.

## **2\. Syntax**

\[ WITH cte\_name AS ( ... ) \[, ...\] \]  
INSERT INTO table\_name \[ ( column\_name \[, ...\] ) \]  
{  
    VALUES ( { expression | DEFAULT } \[, ...\] ) \[, ...\]  
    | select\_statement  
    | DEFAULT VALUES  
}  
\[ ON CONFLICT ( conflict\_target ) DO { NOTHING | UPDATE SET ... } \]  
\[ RETURNING { \* | output\_expression \[ \[ AS \] output\_name \] } \[, ...\] \];

## **3\. Clauses and Examples**

### **3.1. Single-Row Insert**

Inserts exactly one row into a table. The column list is optional if values are provided for all columns in their defined order.

\-- Explicitly list columns  
INSERT INTO products (product\_id, product\_name, unit\_price)  
VALUES (101, 'Chai', 18.00);

\-- Omit column list (requires values for all columns in order)  
INSERT INTO products VALUES (102, 'Chang', 19.00, 100);

### **3.2. Multi-Row Insert**

Inserts multiple rows in a single statement by providing a comma-separated list of VALUES tuples. This is significantly more performant than multiple single-row INSERT statements.

INSERT INTO employees (first\_name, last\_name, department\_id) VALUES  
    ('John', 'Smith', 5),  
    ('Jane', 'Doe', 5),  
    ('Peter', 'Jones', 2);

### **3.3. Insert from a Query (INSERT ... SELECT)**

Inserts the result set of a SELECT statement into a table. The columns must match in type and order.

\-- Archive all completed orders from the last year  
INSERT INTO orders\_archive (order\_id, customer\_id, order\_date, total)  
SELECT order\_id, customer\_id, order\_date, total  
FROM orders  
WHERE status \= 'COMPLETED' AND order\_date \< (CURRENT\_DATE \- INTERVAL '1 year');

### **3.4. "Upsert" with ON CONFLICT**

This powerful clause allows you to either update an existing row or do nothing if the attempted insert would violate a unique or primary key constraint.

\-- Assume products.product\_name has a UNIQUE constraint  
INSERT INTO products (product\_name, units\_in\_stock)  
VALUES ('New Widget', 10\)  
ON CONFLICT (product\_name) \-- Specify the column(s) with the constraint  
DO UPDATE SET  
    units\_in\_stock \= products.units\_in\_stock \+ EXCLUDED.units\_in\_stock;  
    \-- \`EXCLUDED\` refers to the values from the proposed INSERT

\-- If you simply want to ignore duplicates, use DO NOTHING  
INSERT INTO event\_log (event\_id, event\_data)  
VALUES (12345, '{"status":"login"}')  
ON CONFLICT (event\_id)  
DO NOTHING;

### **3.5. Retrieving Results with RETURNING**

The RETURNING clause returns values from the rows that were just inserted. This is extremely useful for getting server-generated values like primary keys from a sequence or IDENTITY column, or default values.

\-- Create a new user and immediately get their server-generated UUID  
INSERT INTO users (username, email)  
VALUES ('j.doe', 'jane.doe@example.com')  
RETURNING user\_id, created\_at;

\-- Result of the query would be a single row with the new user's ID and creation timestamp  
\-- | user\_id                                | created\_at                   |  
\-- |----------------------------------------|------------------------------|  
\-- | 'c4a4e8d3-3b8e-4a8e-9b3b-1b3b3b3b3b3b' | '2024-09-15 14:30:00.123456' |  
