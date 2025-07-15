## **In-Depth Analysis of the SET TRANSACTION Statement**

The SET TRANSACTION statement is used to explicitly start a new transaction and define its characteristics. These characteristics control how the transaction interacts with other concurrent transactions, how it handles data visibility, and how it behaves regarding locks. If a transaction is started implicitly (e.g., by the first DML statement), it uses the database's default settings.

**Full Syntax:**

SET TRANSACTION  
    \[transaction\_option \[transaction\_option ...\]\]

A transaction can have multiple options specified, such as SET TRANSACTION READ ONLY WAIT ISOLATION LEVEL SNAPSHOT;.

### **Transaction Options**

#### **1\. Access Mode**

This option controls whether the transaction can modify data.

* **READ WRITE**: (Default) The transaction is allowed to perform INSERT, UPDATE, and DELETE operations.  
* **READ ONLY**: The transaction is restricted to SELECT statements only. Attempting any data modification will result in an error. This can sometimes improve performance as the engine knows no write-locks will be needed.

#### **2\. Wait Mode (Lock Resolution)**

This option defines how the transaction behaves when it encounters a resource locked by another transaction.

* **WAIT**: (Default) If the transaction needs to access a locked resource, it will wait until the lock is released.  
* **NO WAIT**: If the transaction encounters a locked resource, it will immediately fail with a lock conflict error, rather than waiting.  
* **LOCK TIMEOUT \<seconds\>**: A compromise where the transaction will wait for the specified number of seconds for a lock to be released. If the lock is not released within the timeout period, a lock timeout error occurs.

#### **3\. Isolation Level**

This is the most critical option, defining how the transaction is isolated from the effects of other concurrent transactions and determining what data it can "see".

* **ISOLATION LEVEL SNAPSHOT**: This is the default and most common level in ScratchBird. The transaction operates on a "snapshot" of the database as it existed the moment the transaction started. It cannot see any changes committed by other transactions after it began. This provides a highly consistent view and prevents "dirty reads" and "non-repeatable reads". It is also known as CONSISTENCY.  
* **ISOLATION LEVEL SNAPSHOT TABLE STABILITY**: A stricter version of SNAPSHOT. It ensures that no other transaction can UPDATE or DELETE rows in any table accessed by this transaction until it completes. This prevents "phantom reads" but significantly reduces concurrency.  
* **ISOLATION LEVEL READ COMMITTED**: In this mode, the transaction can see changes that were committed by other transactions after it started.  
  * **RECORD\_VERSION** (or VERSION): The transaction will not read a newer version of a row if it has already read an older version of that same row. This is the default READ COMMITTED behavior.  
  * **NO RECORD\_VERSION** (or NO VERSION): The transaction can see the latest committed version of any row, even if it has previously read an older version of that same row within the same transaction (a "non-repeatable read").  
  * **READ CONSISTENCY**: A mode that prevents the transaction from reading a row that is part of an inconsistent state (e.g., a partially committed update).

#### **4\. Table Reservation (RESERVING)**

This allows a transaction to acquire explicit locks on one or more tables at the very beginning of the transaction, preventing deadlocks that could occur if locks were acquired later on an as-needed basis.

* **Syntax**: RESERVING table\_list \[FOR \[SHARED | PROTECTED\] {READ | WRITE}\]  
* table\_list: A comma-separated list of tables to lock.  
* READ | WRITE: Specifies the type of lock.  
* SHARED | PROTECTED: Specifies the lock level, controlling what other transactions can do with the locked table.

#### **5\. Other Options**

* **NO AUTO UNDO**: A performance-related option that can be used for read-only transactions that access a large number of records. It tells the engine not to retain pre-images of records for potential rollback.  
* **IGNORE LIMBO**: Instructs the transaction to ignore any "limbo" transactions (transactions that were prepared for a two-phase commit but whose final status is unknown).