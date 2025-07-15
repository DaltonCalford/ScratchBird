## **In-Depth Analysis of the SELECT Statement**

The SELECT statement is used to retrieve data from the database. Its structure is composed of several major clauses that define the data source, filtering, grouping, ordering, and final result set shaping. This document provides a detailed breakdown of each component as defined in the ScratchBird parser grammar.

### **Top-Level Structure**

The complete SELECT statement combines a core query expression with optional clauses that control locking and optimization strategy.

\-- Core query expression  
select\_expression

\-- Optional final clauses  
\[FOR UPDATE \[OF column\_list\]\]  
\[WITH LOCK \[SKIP LOCKED\]\]  
\[OPTIMIZE FOR {FIRST | ALL} ROWS\]

* **select\_expression**: This is the main body of the query, which is detailed in the sections below.  
* **FOR UPDATE \[OF column\_list\]**: This clause places an update lock on the rows identified by the query. This prevents other transactions from modifying or deleting the rows until the current transaction is committed or rolled back. The optional OF column\_list is parsed but ignored by the engine; the lock is always placed on the entire row.  
* **WITH LOCK**: A ScratchBird-specific clause that provides an explicit pessimistic lock on the selected rows. This is useful for "select-with-lock" patterns.  
  * **SKIP LOCKED**: If WITH LOCK is specified, this optional sub-clause instructs the engine to simply skip any rows that are already locked by another transaction, rather than waiting for the lock to be released.  
* **OPTIMIZE FOR {FIRST | ALL} ROWS**: A hint to the query optimizer.  
  * FOR FIRST ROWS: Tells the optimizer to generate a plan that returns the first few rows as quickly as possible, even if the total time to retrieve all rows is longer. This is ideal for interactive applications displaying the first page of results.  
  * FOR ALL ROWS (Default): Tells the optimizer to generate a plan that minimizes the time required to retrieve the entire result set.

### **The Core Query: select\_expression**

The core of the statement is the select\_expression, which can be a simple SELECT block, a series of blocks combined with set operators, or include common table expressions (CTEs).

\[WITH \[RECURSIVE\] cte\_definition \[, ...\]\]  
query\_body  
\[ORDER BY ... \]  
\[ { ROWS ... | OFFSET ... FETCH ... } \]

#### **1\. WITH Clause (Common Table Expressions)**

The WITH clause defines one or more named temporary result sets, known as Common Table Expressions (CTEs), that can be referenced within the main query body.

* **WITH \[RECURSIVE\]**:  
  * WITH: Introduces a list of one or more CTEs.  
  * RECURSIVE: Allows a CTE to refer to itself, which is essential for querying hierarchical or graph-like data structures.  
* CTE Definition:  
  cte\_name \[(column\_alias\_list)\] AS (select\_expression)  
  * cte\_name: The name used to reference the CTE later in the query.  
  * column\_alias\_list: (Optional) A list of aliases for the columns produced by the CTE's select\_expression.  
  * AS (select\_expression): The SELECT statement that defines the CTE's result set.

#### **2\. Query Body**

The query body defines the main result set and can be a single query block or multiple blocks joined by set operators.

* **query\_spec**: A single SELECT ... FROM ... block (detailed below).  
* **Set Operators**:  
  * **UNION**: Combines the result sets of two SELECT statements and removes duplicate rows.  
  * **UNION ALL**: Combines the result sets and includes all duplicate rows.

#### **3\. ORDER BY Clause**

Sorts the final result set.

* **Syntax**: ORDER BY expression \[ASC | DESC\] \[NULLS {FIRST | LAST}\] \[, ...\]  
* ASC: Ascending order (default).  
* DESC: Descending order.  
* NULLS FIRST: Places NULL values at the beginning of the sorted set.  
* NULLS LAST: Places NULL values at the end of the sorted set.

#### **4\. Row Limiting and Offset Clauses**

These clauses restrict the number of rows returned after ordering.

* **SQL Standard Syntax**:  
  * OFFSET n {ROW | ROWS}: Skips the first n rows of the result set.  
  * FETCH {FIRST | NEXT} \[n\] {ROW | ROWS} ONLY: Returns only the first n rows after the offset. If n is omitted, it defaults to 1\.  
* **ScratchBird-Specific Syntax**:  
  * FIRST n: Returns the first n rows.  
  * SKIP n: Skips the first n rows.  
  * ROWS n: Returns the first n rows.  
  * ROWS m TO n: Returns rows from m to n inclusive.

### **The SELECT Block: query\_spec**

This is the fundamental building block of a query.

SELECT  
    \[DISTINCT | ALL\]  
    select\_list  
FROM  
    from\_list  
\[WHERE search\_condition\]  
\[GROUP BY expression\_list\]  
\[HAVING search\_condition\]  
\[WINDOW window\_definition\_list\]  
\[PLAN plan\_expression\]

* **SELECT \[DISTINCT | ALL\]**:  
  * ALL (Default): Returns all rows that meet the criteria.  
  * DISTINCT: Removes duplicate rows from the result set.  
* **select\_list**: A comma-separated list of expressions that form the columns of the result set.  
  * \*: Selects all columns from all tables in the FROM clause.  
  * table\_alias.\*: Selects all columns from the specified table or alias.  
  * expression \[AS\] alias: An expression (column, literal, function call, etc.) that can be given an optional alias.  
* **FROM from\_list**: Specifies the data source(s).  
  * Can be a single table reference or a comma-separated list for older-style joins.  
  * **Table Reference**:  
    * table\_name \[AS\] alias \[AT database_link_name\]
    * **Derived Table**: (select\_expression) \[AS\] alias \[(column\_aliases)\]  
    * **LATERAL Derived Table**: A derived table that can reference columns from preceding items in the FROM list.  
    * **Stored Procedure**: procedure\_name(arguments)  
    * **JOIN Clauses**:  
      * INNER JOIN ... ON \<condition\>  
      * LEFT | RIGHT | FULL \[OUTER\] JOIN ... ON \<condition\>  
      * CROSS JOIN  
      * NATURAL JOIN  
      * ... JOIN ... USING (column\_list)  
* **WHERE search\_condition**: Filters rows from the source based on a boolean condition.  
* **GROUP BY expression\_list**: Groups rows that have the same values into summary rows. The select\_list is then typically restricted to aggregate functions (COUNT, SUM, AVG, etc.) and the grouping expressions themselves.  
* **HAVING search\_condition**: Filters the results of a GROUP BY clause. It is applied after grouping, whereas WHERE is applied before.  
* **WINDOW window\_definition\_list**: Defines one or more named windows (window\_name AS (window\_spec)) for use in window functions within the select\_list. A window specification can include:  
  * PARTITION BY ...: Divides rows into partitions.  
  * ORDER BY ...: Orders rows within each partition.  
  * ROWS | RANGE ...: Defines a frame within the partition (e.g., ROWS BETWEEN 1 PRECEDING AND 1 FOLLOWING).  
* **PLAN plan\_expression**: Provides an explicit access plan to the optimizer, overriding its default choices.  
  * PLAN (table NATURAL): Forces a table scan.  
  * PLAN (table INDEX (index\_name, ...)): Forces the use of specific indices.  
  * PLAN (JOIN (table1 ..., table2 ...)): Forces a join order.
