## **In-Depth Analysis of the PLAN Clause**

The PLAN clause is an advanced feature in ScratchBird that allows a developer to manually specify the data retrieval strategy for a SELECT, UPDATE, or DELETE statement. By providing a plan, you override the choices made by the query optimizer. This is typically only used for performance tuning when the optimizer has chosen a sub-optimal plan for a specific query.

**Syntax:**

PLAN plan\_expression

A plan\_expression is a nested structure that describes how tables should be joined and how each table should be accessed.

### **Access Methods**

For each table, you can specify one of three access methods:

1. **NATURAL**:  
   * **Syntax**: PLAN (table\_alias NATURAL)  
   * **Description**: Forces a full table scan. The engine will read every row in the table sequentially. This is efficient for small tables or when a large percentage of the table's rows need to be accessed.  
2. **INDEX**:  
   * **Syntax**: PLAN (table\_alias INDEX (index\_1, index\_2, ...))  
   * **Description**: Forces the use of one or more indices to find the required rows. The optimizer will evaluate the provided indices and use the most suitable one(s) based on the WHERE clause. This is typically much more efficient than NATURAL if the index is selective.  
3. **ORDER (Navigational Access)**:  
   * **Syntax**: PLAN (table\_alias ORDER index\_name)  
   * **Description**: Forces the retrieval of rows in the order defined by the specified index\_name. This is very useful for queries with an ORDER BY clause that matches the index, as it can eliminate a separate sorting step.

### **Join Methods**

The PLAN clause also controls the join strategy.

1. **JOIN (Nested Loop Join)**:  
   * **Syntax**: PLAN (JOIN (plan\_for\_table1, plan\_for\_table2))  
   * **Description**: This specifies a nested loop join. The tables are joined in the order they appear in the JOIN expression. The first table is the "outer" table, and the second is the "inner" table. The engine iterates through each row of the outer table and, for each one, finds matching rows in the inner table using its specified access method (NATURAL, INDEX, or ORDER).  
2. **MERGE (Sort-Merge Join)**:  
   * **Syntax**: PLAN (MERGE (plan\_for\_table1, plan\_for\_table2))  
   * **Description**: Forces a sort-merge join. Both input tables are sorted on the join keys, and then the sorted lists are merged together. This can be efficient for joining two large tables where indices are not effective.

### **Example**

Consider the following query:

SELECT \*  
FROM CUSTOMERS C  
JOIN ORDERS O ON C.ID \= O.CUST\_ID  
WHERE C.CITY \= 'New York' AND O.ORDER\_DATE \> '2023-01-01';

An explicit plan could look like this:

PLAN (JOIN (C INDEX (IDX\_CUST\_CITY), O INDEX (IDX\_ORD\_CUSTID, IDX\_ORD\_DATE)))

**This plan instructs the optimizer to:**

1. Use a JOIN (nested loop) strategy.  
2. Use the CUSTOMERS table (C) as the outer table.  
3. Access CUSTOMERS using the IDX\_CUST\_CITY index to find customers in 'New York'.  
4. For each customer found, access the ORDERS table (O) using either IDX\_ORD\_CUSTID (to find orders for that customer) or IDX\_ORD\_DATE (to find recent orders), whichever it deems more selective for the inner loop.