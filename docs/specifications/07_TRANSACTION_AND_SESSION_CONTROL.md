# **Transaction and Session Control**

## **1\. Introduction**

Transaction and session control statements are fundamental to maintaining data integrity and managing the user's interaction with the database. ScratchBird provides a robust implementation of standard Transaction Control Language (TCL) and a rich set of commands for customizing the behavior of a database session.

### **Core Principles**

* **ACID Compliance**: All transactions in ScratchBird adhere to the ACID properties (Atomicity, Consistency, Isolation, Durability), guaranteeing that data remains in a consistent state even in the event of errors, crashes, or concurrent access.  
* **Transactional DDL**: A key feature of ScratchBird is that nearly all Data Definition Language (DDL) commands are fully transactional. You can create a table, add an index, and grant permissions all within a single transaction that can be committed or rolled back as a single atomic unit.  
* **Explicit Transaction Control**: While single statements are implicitly wrapped in a transaction, any multi-statement operation that must be treated as a single atomic unit should be explicitly wrapped in a BEGIN...COMMIT or BEGIN...ROLLBACK block.  
* **Granular Session Customization**: Users have fine-grained control over their session's environment, including configuration parameters, schema search paths, and security context, using the SET command.

## **2\. Transaction Management**

A transaction is a sequence of operations performed as a single logical unit of work. All of the operations within a transaction must succeed; if any one of them fails, the entire transaction is rolled back, and the database is left unchanged.

### **2.1. Starting and Ending Transactions**

| Command | Description |
| :---- | :---- |
| BEGIN | Starts a new transaction block. START TRANSACTION is an alias. |
| COMMIT | Makes all changes made in the current transaction permanent and visible to others. END is an alias. |
| ROLLBACK | Discards all changes made in the current transaction, restoring the database to the state it was in before the transaction began. ABORT is an alias. |

**Example: A Standard Atomic Transaction**

BEGIN;

\-- Step 1: Debit from a savings account  
UPDATE accounts  
SET balance \= balance \- 100.00  
WHERE account\_id \= 'savings-123';

\-- Step 2: Credit to a checking account  
UPDATE accounts  
SET balance \= balance \+ 100.00  
WHERE account\_id \= 'checking-456';

\-- If both updates succeed, commit the changes.  
\-- If any part of the process had failed, we would issue a ROLLBACK.  
COMMIT;

### **2.2. Savepoints**

Savepoints allow you to set markers within a transaction and roll back to those specific markers without aborting the entire transaction. This is useful for complex procedures with partial failures that can be recovered from.

| Command | Description |
| :---- | :---- |
| SAVEPOINT name | Establishes a new savepoint within the current transaction. |
| ROLLBACK TO SAVEPOINT name | Rolls back all changes made after the savepoint was established, but preserves changes made before it. |
| RELEASE SAVEPOINT name | Destroys a savepoint, releasing its resources. |

**Example: Using a Savepoint for a Partial Rollback**

BEGIN;

INSERT INTO orders (customer\_id, order\_date) VALUES (101, CURRENT\_DATE);

SAVEPOINT order\_created;

\-- Now, attempt to add order lines. This part might fail.  
\-- Let's assume one of the product\_ids is invalid, causing an error.  
INSERT INTO order\_lines (order\_id, product\_id, quantity) VALUES  
    (currval('order\_id\_seq'), 9901, 2), \-- OK  
    (currval('order\_id\_seq'), 9999, 1); \-- ERROR: product 9999 does not exist

\-- If an error occurs here, the EXCEPTION block in procedural code would run:  
ROLLBACK TO SAVEPOINT order\_created;

\-- At this point, the failed INSERT for order\_lines is undone,  
\-- but the initial INSERT into the orders table is still intact.  
\-- We can now log the error and commit the main order record.  
INSERT INTO order\_errors (order\_id, error\_message) VALUES (currval('order\_id\_seq'), 'Invalid product ID');

COMMIT; \-- The main order is saved, along with the error log.

### **2.3. Isolation Levels**

Isolation determines how transaction changes are visible to other concurrent transactions. ScratchBird supports the standard SQL isolation levels.

| Command | Description |
| :---- | :---- |
| SET TRANSACTION ISOLATION LEVEL ... | Sets the isolation level for the *current* transaction. Must be the first statement in a transaction. |
| SET SESSION CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL ... | Sets the default isolation level for *all subsequent* transactions in the current session. |

**Available Levels:**

* **READ COMMITTED (Default)**: Each statement sees only data that was committed before it began. This is the default and provides a good balance between consistency and performance.  
* **REPEATABLE READ**: All statements within the same transaction see the same snapshot of the data as it existed at the start of the transaction. This prevents non-repeatable reads but can lead to serialization failures if concurrent transactions modify the same data.  
* **SERIALIZABLE**: The highest level of isolation. Transactions behave as if they were executed one after another, serially, rather than concurrently. This provides the strongest consistency guarantee but at the cost of reduced concurrency.

## **3\. Session Management**

Session management commands allow you to view and change settings for your current connection to the database.

### **3.1. Setting Session Parameters (SET)**

The SET command modifies a runtime configuration parameter for the current session.

\-- Set the schema search path for the session  
SET SEARCH\_PATH TO 'sales', 'public';

\-- Set the time zone for the session  
SET TIME ZONE 'America/New\_York';

\-- Set a configuration parameter for the current transaction only  
SET LOCAL statement\_timeout \= '5000ms';

### **3.2. Viewing Session Parameters (SHOW)**

The SHOW command displays the current value of a configuration parameter.

SHOW SEARCH\_PATH;  
\-- Result: "sales", "public"

SHOW TIME ZONE;  
\-- Result: America/New\_York

SHOW ALL; \-- Display all configuration parameters

### **3.3. Setting the Security Context**

You can change the current user and role for the session, which affects permission checks for subsequent queries.

\-- Change the active role for the session  
SET ROLE 'reporting\_user';

\-- Change the session's user identifier (requires superuser privileges)  
SET SESSION AUTHORIZATION 'admin\_user';

## **4\. Concurrency Control and Locking**

In addition to transaction isolation, ScratchBird provides mechanisms for explicit locking when fine-grained control over concurrency is needed.

### **4.1. Explicit Table Locks (LOCK TABLE)**

The LOCK TABLE command acquires an explicit lock on a table, overriding the default row-level locking behavior. This should be used with caution as it can severely impact concurrency.

BEGIN;  
\-- Acquire an exclusive lock on the entire table, preventing any other reads or writes  
LOCK TABLE configuration IN ACCESS EXCLUSIVE MODE;

\-- Perform critical updates  
UPDATE configuration SET setting \= 'new\_value' WHERE key \= 'critical\_key';

COMMIT; \-- The lock is released

### **4.2. Row-Level Locking (SELECT ... FOR UPDATE)**

This is the preferred method for handling "read-then-write" race conditions. It is specified as part of a SELECT statement within a transaction and locks the rows returned by the query.

BEGIN;  
\-- Select a specific job from the queue and lock it so no other worker can take it  
SELECT \* FROM job\_queue  
WHERE status \= 'PENDING'  
ORDER BY created\_at  
LIMIT 1  
FOR UPDATE SKIP LOCKED; \-- SKIP LOCKED immediately moves on if the row is already locked

\-- ... process the job in the application ...

\-- Update the job's status  
UPDATE job\_queue SET status \= 'COMPLETE' WHERE job\_id \= ...;

COMMIT;

### **4.3. Two-Phase Commit (2PC)**

For distributed transactions that span multiple databases, ScratchBird supports two-phase commit to ensure atomicity.

\-- In a transaction coordinator:

\-- 1\. PREPARE the transaction on all participating nodes  
PREPARE TRANSACTION 'my\_distributed\_transaction\_id';

\-- 2\. If all nodes successfully prepare, then commit on all nodes  
COMMIT PREPARED 'my\_distributed\_transaction\_id';

\-- 3\. If any node fails to prepare, then roll back on all nodes  
ROLLBACK PREPARED 'my\_distributed\_transaction\_id';  
