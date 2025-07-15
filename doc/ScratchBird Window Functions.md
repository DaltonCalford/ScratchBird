## **In-Depth Analysis of Window Functions**

Window functions perform a calculation across a set of rows that are related to the current row. Unlike aggregate functions, which collapse multiple rows into a single output row, window functions return a value for *every* row. The set of rows they operate on is called a "window" and is defined by the OVER() clause.

### **Basic Syntax**

A window function is called by appending an OVER clause to a standard function call.

function\_name(arguments) OVER ( \[window\_specification\] )

or

function\_name(arguments) OVER window\_name

### **The OVER Clause**

The OVER clause defines the window of rows. It has three main sub-clauses:

1\. PARTITION BY  
This clause divides the rows into groups, or "partitions". The window function is then applied independently to each partition. If PARTITION BY is omitted, the entire result set is treated as a single partition.

* **Syntax**: PARTITION BY expression\_list  
* **Example**: SUM(salary) OVER (PARTITION BY department\_id) calculates the total salary for each department, and this total is shown on every employee row within that department.

2\. ORDER BY  
This clause orders the rows within each partition. The order is crucial for functions that are sensitive to row sequence, such as RANK(), LAG(), and LEAD().

* **Syntax**: ORDER BY expression\_list \[ASC | DESC\] \[NULLS {FIRST | LAST}\]  
* **Example**: RANK() OVER (PARTITION BY department\_id ORDER BY salary DESC) assigns a rank to each employee based on their salary within their department.

3\. Window Frame Clause (ROWS or RANGE)  
This clause specifies a moving sub-set of rows (a "frame") within the current partition. If omitted, the default frame is typically RANGE BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW.

* **Syntax**: {ROWS | RANGE} {frame\_start | BETWEEN frame\_start AND frame\_end}  
* **Frame Start/End**:  
  * UNBOUNDED PRECEDING: The frame starts at the first row of the partition.  
  * n PRECEDING: The frame starts n rows before the current row.  
  * CURRENT ROW: The frame starts or ends at the current row.  
  * n FOLLOWING: The frame ends n rows after the current row.  
  * UNBOUNDED FOLLOWING: The frame ends at the last row of the partition.  
* **ROWS vs. RANGE**:  
  * ROWS defines the frame based on a physical number of rows relative to the current row.  
  * RANGE defines the frame based on a logical range of values relative to the value in the current row (based on the ORDER BY clause). For RANGE, rows with the same value in the ORDER BY column are considered peers.  
* **Example**: SUM(salary) OVER (ORDER BY hire\_date ROWS BETWEEN 1 PRECEDING AND 1 FOLLOWING) calculates the sum of salaries for an employee, the one hired just before them, and the one hired just after.

### **Available Window Functions**

#### **Ranking Functions**

* RANK(): Assigns a rank based on the ORDER BY clause. Leaves gaps in the ranking sequence for ties (e.g., 1, 2, 2, 4).  
* DENSE\_RANK(): Assigns a rank without gaps (e.g., 1, 2, 2, 3).  
* PERCENT\_RANK(): Calculates the relative rank of a row: (rank \- 1\) / (total\_rows \- 1).  
* ROW\_NUMBER(): Assigns a unique number to each row within the partition, starting from 1\.  
* NTILE(n): Divides the rows into n approximately equal-sized buckets and assigns a bucket number to each row.  
* CUME\_DIST(): Calculates the cumulative distribution of a value within the set of rows.

#### **Offset Functions**

* LAG(expression \[, offset \[, default\]\]): Accesses data from a previous row in the partition without a self-join.  
* LEAD(expression \[, offset \[, default\]\]): Accesses data from a subsequent row in the partition.

#### **Value Functions**

* FIRST\_VALUE(expression): Returns the value of the expression from the first row of the window frame.  
* LAST\_VALUE(expression): Returns the value of the expression from the last row of the window frame.  
* NTH\_VALUE(expression, n): Returns the value of the expression from the n-th row of the window frame.

#### **Aggregate Functions as Window Functions**

All standard aggregate functions can be used as window functions:

* COUNT()  
* SUM()  
* AVG()  
* MIN()  
* MAX()  
* LIST()

**Example**: AVG(salary) OVER (PARTITION BY department\_id) shows the average department salary on each employee's row.

### **Named Windows (WINDOW Clause)**

If you use the same window specification for multiple functions in a query, you can define it once with a name in the WINDOW clause of the SELECT statement and then reference it by name.

**Syntax:**

SELECT  
    RANK() OVER w,  
    SUM(salary) OVER w  
FROM employees  
WINDOW w AS (PARTITION BY department\_id ORDER BY salary DESC);

This improves readability and reduces redundancy.