# **Table Partitioning Specification**

## **1\. Introduction**

Table partitioning is a database design technique used to divide very large tables into smaller, more manageable pieces called **partitions**. While logically the partitioned table is treated as a single entity for DML operations, physically the data is stored in separate partitions. This strategy can provide significant performance benefits for querying and maintenance.

ScratchBird supports declarative partitioning, where the database engine automatically routes rows to the correct partition based on the value of one or more **partition key** columns.

### **Benefits of Partitioning**

* **Query Performance**: Queries that access only a subset of the data can scan only the relevant partitions instead of the entire table, a technique known as **partition pruning**.  
* **Maintenance Efficiency**: Administrative tasks like rebuilding indexes, truncating data, or running backups can be performed on individual partitions, reducing the impact on a large table.  
* **Improved Availability**: Partition-level maintenance allows other partitions to remain online and available.

## **2\. Partitioning Strategies**

ScratchBird supports the following partitioning strategies:

### **2.1. Range Partitioning**

Partitions are defined by a continuous, non-overlapping range of values in the partition key column(s). This is ideal for data with a clear range, such as dates or sequential IDs.

### **2.2. List Partitioning**

Partitions are defined by a list of explicit, discrete values for the partition key. This is useful for partitioning based on categorical data, such as regions, departments, or status codes.

### **2.3. Hash Partitioning**

Rows are distributed evenly across a specified number of partitions based on a hash value computed from the partition key. This strategy is best for evenly distributing data when there is no logical range or list to partition by.

## **3\. Creating Partitioned Tables**

You define a partitioned table using the PARTITION BY clause in the CREATE TABLE statement. The individual partitions are then created as separate tables that INHERIT from the main partitioned table.

### **3.1. CREATE TABLE ... PARTITION BY RANGE**

**Syntax:**

CREATE TABLE table\_name (  
    column\_definitions...  
) PARTITION BY RANGE (partition\_key\_column);

**Example: Partitioning a Sales Table by Date**

\-- 1\. Create the main "parent" table with the partitioning strategy  
CREATE TABLE sales (  
    sale\_id        BIGINT,  
    product\_id     INT,  
    sale\_date      DATE NOT NULL,  
    amount         DECIMAL(10, 2\)  
) PARTITION BY RANGE (sale\_date);

\-- 2\. Create the individual partitions for each time range  
CREATE TABLE sales\_y2024\_q1 PARTITION OF sales  
    FOR VALUES FROM ('2024-01-01') TO ('2024-04-01');

CREATE TABLE sales\_y2024\_q2 PARTITION OF sales  
    FOR VALUES FROM ('2024-04-01') TO ('2024-07-01');

CREATE TABLE sales\_y2024\_q3 PARTITION OF sales  
    FOR VALUES FROM ('2024-07-01') TO ('2024-10-01');

\-- It's good practice to create a default partition for values outside the defined ranges  
CREATE TABLE sales\_default PARTITION OF sales DEFAULT;

\-- Inserts are automatically routed to the correct partition  
INSERT INTO sales (sale\_id, sale\_date, amount) VALUES (1, '2024-05-15', 99.99); \-- Goes into sales\_y2024\_q2

\-- This query will only scan the sales\_y2024\_q2 partition  
SELECT \* FROM sales WHERE sale\_date \= '2024-05-15';

### **3.2. CREATE TABLE ... PARTITION BY LIST**

**Syntax:**

CREATE TABLE table\_name (  
    column\_definitions...  
) PARTITION BY LIST (partition\_key\_column);

**Example: Partitioning Customers by Region**

CREATE TABLE customers (  
    customer\_id    INT,  
    name           TEXT,  
    region\_code    VARCHAR(10) NOT NULL  
) PARTITION BY LIST (region\_code);

CREATE TABLE customers\_north\_america PARTITION OF customers  
    FOR VALUES IN ('USA', 'CAN', 'MEX');

CREATE TABLE customers\_europe PARTITION OF customers  
    FOR VALUES IN ('GBR', 'FRA', 'DEU');

CREATE TABLE customers\_asia PARTITION OF customers  
    FOR VALUES IN ('CHN', 'JPN', 'IND');

\-- This insert will be routed to the customers\_europe partition  
INSERT INTO customers (customer\_id, name, region\_code) VALUES (1, 'ACME Corp', 'FRA');

## **4\. DML Routing and Migration Semantics**

This section defines how INSERT/UPDATE/DELETE behave for partitioned tables.

### **4.1. INSERT Routing**

- Rows are routed by the partition key(s) to a single target partition.
- If a DEFAULT partition exists, rows that do not match any explicit partition
  are routed to DEFAULT.
- If no partition matches and there is no DEFAULT partition, the statement
  fails with `PARTITION_NOT_FOUND`.

### **4.2. UPDATE Routing (Partition-Key Changes)**

- If the UPDATE does **not** change the partition key, the row stays in the
  same partition (no migration).
- If the UPDATE **changes** the partition key, the row **migrates** to the
  new target partition. The operation is treated as a single atomic change:
  a delete from the source partition + insert into the target partition.
- If the new partition key does not map to a valid partition and there is no
  DEFAULT partition, the statement fails with `PARTITION_NOT_FOUND`.
- If multiple partitions match (catalog corruption), the statement fails with
  `PARTITION_AMBIGUOUS`.

### **4.3. DELETE Routing**

- DELETE is applied to the partition that contains each matching row.
- Partition pruning is applied when predicates include the partition key;
  otherwise the engine may scan multiple partitions.

### **4.4. DML on Child Partitions**

- Direct DML against a child partition targets that partition only and does
  not perform routing or migration.

### **4.5. Error Handling**

- `PARTITION_NOT_FOUND`: No valid partition (and no DEFAULT).
- `PARTITION_AMBIGUOUS`: Multiple partitions match the key.
- `PARTITION_CONSTRAINT_VIOLATION`: Partition constraint check fails.

---

## **5\. Managing Partitions**

You can add and remove partitions from a partitioned table as your data grows and changes.

### **5.1. Attaching a New Partition (ALTER TABLE ... ATTACH PARTITION)**

This command adds a new partition to an existing partitioned table. The table being attached must have a structure that matches the parent and a constraint that matches the partition rule.

**Syntax:**

ALTER TABLE parent\_table\_name  
    ATTACH PARTITION child\_partition\_name  
    FOR VALUES { IN (...) | FROM (...) TO (...) };

**Example:**

\-- Create a new table for Q4 sales data  
CREATE TABLE sales\_y2024\_q4 (  
    CHECK ( sale\_date \>= DATE '2024-10-01' AND sale\_date \< DATE '2025-01-01' )  
) INHERITS (sales);

\-- Attach it as a new partition  
ALTER TABLE sales  
    ATTACH PARTITION sales\_y2024\_q4  
    FOR VALUES FROM ('2024-10-01') TO ('2025-01-01');

### **5.2. Detaching a Partition (ALTER TABLE ... DETACH PARTITION)**

This command removes a partition from a partitioned table, converting it into a standalone table. This is a very fast operation as it only involves updating metadata.

**Syntax:**

ALTER TABLE parent\_table\_name  
    DETACH PARTITION child\_partition\_name;

**Example: Archiving old data**

\-- Detach the Q1 partition to archive it  
ALTER TABLE sales DETACH PARTITION sales\_y2024\_q1;

\-- The 'sales\_y2024\_q1' table is now a regular, standalone table.  
\-- You can now move it to archival storage, etc.

## **6\. Dropping Partitions**

To drop a partition, you simply drop it like a regular table. The command will fail if the table is still attached as a partition. You must DETACH it first before dropping it.

**Example:**

\-- 1\. Detach the partition  
ALTER TABLE sales DETACH PARTITION sales\_y2024\_q1;

\-- 2\. Drop the now-standalone table  
DROP TABLE sales\_y2024\_q1;  
