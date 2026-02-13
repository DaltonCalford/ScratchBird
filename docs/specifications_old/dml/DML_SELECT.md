# **SELECT Statement Specification**

## **1\. Purpose**

The SELECT statement is used to retrieve data from the database. It is the primary tool for querying and can range from simple single-table lookups to complex analytical queries involving multiple tables, aggregations, and advanced calculations.

## **2\. Syntax**

\[ WITH \[ RECURSIVE \] cte\_name AS ( ... ) \[, ...\] \]  
SELECT \[ DISTINCT \[ ON ( expression \[, ...\] ) \] \]  
    select\_list  
\[ FROM from\_item \[, ...\] \]  
\[ WHERE condition \]  
\[ GROUP BY grouping\_element \[, ...\] \]  
\[ HAVING condition \]  
\[ WINDOW window\_name AS ( window\_definition ) \[, ...\] \]  
\[ { UNION | INTERSECT | EXCEPT } \[ ALL \] select\_statement \]  
\[ ORDER BY expression \[ ASC | DESC \] \[ NULLS { FIRST | LAST } \] \[, ...\] \]  
\[ LIMIT count \]  
\[ OFFSET start \]  
\[ FOR { UPDATE | SHARE } \[ OF table\_name \[, ...\] \] \[ NOWAIT | SKIP LOCKED \] \];

## **3\. Clauses and Examples**

### **3.1. SELECT List**

Defines the columns to be returned.

\-- Select all columns  
SELECT \* FROM products;

\-- Select specific columns with aliases  
SELECT  
    product\_name AS name,  
    unit\_price AS price,  
    units\_in\_stock \* unit\_price AS inventory\_value  
FROM products;

### **3.2. FROM and JOIN**

Specifies the source table(s) for the query.

\-- Simple FROM  
SELECT \* FROM customers;

\-- INNER JOIN to match rows from two tables  
SELECT o.order\_id, c.customer\_name  
FROM orders AS o  
INNER JOIN customers AS c ON o.customer\_id \= c.customer\_id;

\-- LEFT JOIN to include all rows from the left table  
SELECT e.employee\_name, d.department\_name  
FROM employees AS e  
LEFT JOIN departments AS d ON e.department\_id \= d.department\_id;

\-- LATERAL JOIN to reference preceding FROM items  
SELECT p.product\_name, s.sale\_amount  
FROM products p,  
LATERAL (  
    SELECT \* FROM sales s  
    WHERE s.product\_id \= p.product\_id  
    ORDER BY s.sale\_date DESC LIMIT 5  
) AS s;

### **3.3. WHERE Clause**

Filters rows based on a specified condition.

SELECT \* FROM orders  
WHERE order\_date \>= '2024-01-01' AND status \= 'SHIPPED';

### **3.4. GROUP BY and HAVING**

Groups rows that have the same values into summary rows and then filters those groups.

SELECT  
    category\_id,  
    COUNT(\*) AS number\_of\_products,  
    AVG(unit\_price) AS average\_price  
FROM products  
GROUP BY category\_id  
HAVING COUNT(\*) \> 10; \-- Only show categories with more than 10 products

### **3.5. ORDER BY, LIMIT, OFFSET**

Sorts the result set and optionally limits the number of rows returned.

\-- Get the 10 most expensive products  
SELECT product\_name, unit\_price  
FROM products  
ORDER BY unit\_price DESC NULLS LAST  
LIMIT 10;

\-- Paginate results, showing the third page of 20 items each  
SELECT \* FROM articles  
ORDER BY publication\_date DESC  
LIMIT 20 OFFSET 40;

### **3.6. Window Functions (OVER)**

Performs calculations across a set of table rows that are somehow related to the current row. Window functions support partitioning, ordering, frame specifications (ROWS, RANGE, GROUPS), frame exclusions, and NULL handling options.

\-- Rank employees by salary within each department
SELECT
    employee\_name,
    department\_name,
    salary,
    RANK() OVER (PARTITION BY department\_name ORDER BY salary DESC) as salary\_rank
FROM employees;

\-- Using GROUPS frame to include entire peer groups
SELECT
    product\_id,
    order\_date,
    quantity,
    SUM(quantity) OVER (
        ORDER BY order\_date
        GROUPS BETWEEN 1 PRECEDING AND 1 FOLLOWING
    ) as quantity\_with\_adjacent\_days
FROM orders;

\-- Using frame exclusions to exclude current row or peer group
SELECT
    employee\_id,
    salary,
    AVG(salary) OVER (
        ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING
        EXCLUDE CURRENT ROW
    ) as avg\_other\_salaries
FROM employees;

\-- Using IGNORE NULLS with navigational functions
SELECT
    measurement\_date,
    temperature,
    LAST\_VALUE(temperature) IGNORE NULLS OVER (
        ORDER BY measurement\_date
        ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
    ) as last\_valid\_temperature
FROM weather\_data;

For detailed window function syntax and examples, see the Window Functions specification.

### **3.7. Common Table Expressions (WITH)**

Defines temporary, named result sets that can be referenced within the main query. Especially useful for recursive queries.

\-- Find all subordinates of an employee using a recursive CTE  
WITH RECURSIVE subordinates AS (  
    \-- Anchor member: the starting employee  
    SELECT employee\_id, name, manager\_id, 0 AS level  
    FROM employees  
    WHERE employee\_id \= 101

    UNION ALL

    \-- Recursive member: employees who report to the previous level  
    SELECT e.employee\_id, e.name, e.manager\_id, s.level \+ 1  
    FROM employees e  
    INNER JOIN subordinates s ON s.employee\_id \= e.manager\_id  
)  
SELECT \* FROM subordinates;

### **3.8. FOR UPDATE / SHARE**

Locks the selected rows to prevent concurrent modification, ensuring data consistency for read-then-write operations within a transaction.

BEGIN;  
\-- Select the current account balance and lock the row  
SELECT balance FROM accounts WHERE account\_id \= '123' FOR UPDATE;

\-- After performing application logic, update the locked row  
UPDATE accounts SET balance \= balance \- 100.00 WHERE account\_id \= '123';  
COMMIT;  
