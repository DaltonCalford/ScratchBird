# 13 Transaction Control



<!-- Original PDF Page: 559 -->

Example
create trigger bi_customers for customers before insert as
begin
  New.added_by  = USER;
  New.purchases = 0;
end
Chapter 12. Context Variables
558

<!-- Original PDF Page: 560 -->

Chapter 13. Transaction Control
Almost all operations in Firebird occur in the context of a transaction. Units of work are isolated
between a start point and end point. Changes to data remain reversible until the moment the client
application instructs the server to commit them.
Unless explicitly mentioned otherwise in an “Available in” section, transaction control statements
are available in DSQL. Availability in ESQL is — bar some exceptions — not tracked by this
Language Reference. Transaction control statements are not available in PSQL.
13.1. Transaction Statements
Firebird has a small lexicon of SQL statements to start, manage, commit and reverse (roll back) the
transactions that form the boundaries of most database tasks:
SET TRANSACTION
configures and starts a transaction
COMMIT
signals the end of a unit of work and writes changes permanently to the database
ROLLBACK
undoes the changes performed in the transaction or to a savepoint
SAVEPOINT
marks a position in the log of work done, in case a partial rollback is needed
RELEASE SAVEPOINT
erases a savepoint
13.1.1. SET TRANSACTION
Configures and starts a transaction
Available in
DSQL, ESQL
Syntax
SET TRANSACTION
   [NAME tr_name]
   [<tr_option> ...]
<tr_option> ::=
     READ {ONLY | WRITE}
   | [NO] WAIT
   | [ISOLATION LEVEL] <isolation_level>
   | NO AUTO UNDO
Chapter 13. Transaction Control
559

<!-- Original PDF Page: 561 -->

| RESTART REQUESTS
   | AUTO COMMIT
   | IGNORE LIMBO
   | LOCK TIMEOUT seconds
   | RESERVING <tables>
   | USING <dbhandles>
<isolation_level> ::=
    SNAPSHOT [AT NUMBER snapshot_number]
  | SNAPSHOT TABLE [STABILITY]
  | READ {UNCOMMITED | COMMITTED} [<read-commited-opt>]
<read-commited-opt> ::=
  [NO] RECORD_VERSION | READ CONSISTENCY
<tables> ::= <table_spec> [, <table_spec> ...]
<table_spec> ::= tablename [, tablename ...]
  [FOR [SHARED | PROTECTED] {READ | WRITE}]
<dbhandles> ::= dbhandle [, dbhandle ...]
Table 249. SET TRANSACTION Statement Parameters
Parameter Description
tr_name Transaction name. Available only in ESQL
tr_option Optional transaction option. Each option should be specified at most
once, and some options are mutually exclusive (e.g. READ ONLY vs READ
WRITE, WAIT vs NO WAIT)
seconds The time in seconds for the statement to wait in case a conflict occurs.
Has to be greater than or equal to 0.
snapshot_number Snapshot number to use for this transaction
tables The list of tables to reserve
dbhandles The list of databases the database can access. Available only in ESQL
table_spec Table reservation specification
tablename The name of the table to reserve
dbhandle The handle of the database the transaction can access. Available only in
ESQL
Generally, only client applications start transactions. Exceptions are when the server starts an
autonomous transaction, and transactions for certain background system threads/processes, such
as sweeping.
A client application can start any number of concurrently running transactions. A single connection
can have multiple concurrent active transactions (though not all drivers or access components
support this). A limit does exist, for the total number of transactions in all client applications
Chapter 13. Transaction Control
560

<!-- Original PDF Page: 562 -->

working with one particular database from the moment the database was restored from its gbak
backup or from the moment the database was created originally. The limit is 2
48
 — 281,474,976,710,656.
All clauses in the SET TRANSACTION statement are optional. If the statement starting a transaction has
no clauses specified, the transaction will be started with default values for access mode, lock
resolution mode and isolation level, which are:
SET TRANSACTION
  READ WRITE
  WAIT
  ISOLATION LEVEL SNAPSHOT;
 Database drivers or access components may use different defaults for transactions
started through their API. Check their documentation for details.
The server assigns integer numbers to transactions sequentially. Whenever a client starts any
transaction, either explicitly defined or by default, the server sends the transaction ID to the client.
This number can be retrieved in SQL using the context variable CURRENT_TRANSACTION.

Some database drivers — or their governing specifications — require that you
configure and start transaction through API methods. In that case, using SET
TRANSACTION is either not supported, or may result in unspecified behaviour. An
example of this is JDBC and the Firebird JDBC driver Jaybird.
Check the documentation of your driver for details.
The NAME and USING clauses are only valid in ESQL.
Transaction Name
The optional NAME attribute defines the name of a transaction. Use of this attribute is available only
in Embedded SQL (ESQL). In ESQL applications, named transactions make it possible to have
several transactions active simultaneously in one application. If named transactions are used, a
host-language variable with the same name must be declared and initialized for each named
transaction. This is a limitation that prevents dynamic specification of transaction names and thus
rules out transaction naming in DSQL.
Transaction Parameters
The main parameters of a transaction are:
• data access mode (READ WRITE, READ ONLY)
• lock resolution mode (WAIT, NO WAIT) with an optional LOCK TIMEOUT specification
• isolation level (READ COMMITTED, SNAPSHOT, SNAPSHOT TABLE STABILITY).
 The READ UNCOMMITTED isolation level is a synonym for READ COMMITTED, and is
Chapter 13. Transaction Control
561

<!-- Original PDF Page: 563 -->

provided only for syntax compatibility. It provides identical semantics as READ
COMMITTED, and does not allow you to view uncommitted changes of other
transactions.
• a mechanism for reserving or releasing tables (the RESERVING clause)
Access Mode
The two database access modes for transactions are READ WRITE and READ ONLY.
• If the access mode is READ WRITE, operations in the context of this transaction can be both read
operations and data update operations. This is the default mode.
• If the access mode is READ ONLY, only SELECT operations can be executed in the context of this
transaction. Any attempt to change data in the context of such a transaction will result in
database exceptions. However, this does not apply to global temporary tables (GTT), which are
allowed to be changed in READ ONLY transactions, see Global Temporary Tables (GTT) in Chapter
5, Data Definition (DDL) Statements for details.
Lock Resolution Mode
When several client processes work with the same database, locks may occur when one process
makes uncommitted changes in a table row, or deletes a row, and another process tries to update or
delete the same row. Such locks are called update conflicts.
Locks may occur in other situations when multiple transaction isolation levels are used.
The two lock resolution modes are WAIT and NO WAIT.
WAIT Mode
In the WAIT mode (the default mode), if a conflict occurs between two parallel processes executing
concurrent data updates in the same database, a WAIT transaction will wait till the other transaction
has finished — by committing ( COMMIT) or rolling back ( ROLLBACK). The client application with the
WAIT transaction will be put on hold until the conflict is resolved.
If a LOCK TIMEOUT is specified for the WAIT transaction, waiting will continue only for the number of
seconds specified in this clause. If the lock is unresolved at the end of the specified interval, the
error message “Lock time-out on wait transaction” is returned to the client.
Lock resolution behaviour can vary a little, depending on the transaction isolation level.
NO WAIT Mode
In the NO WAIT mode, a transaction will immediately throw a database exception if a conflict occurs.

LOCK TIMEOUT  is a separate transaction option, but can only be used for WAIT
transactions. Specifying LOCK TIMEOUT with a NO WAIT transaction will raise an error
“invalid parameter in transaction parameter block -Option isc_tpb_lock_timeout is
not valid if isc_tpb_nowait was used previously in TPB”
Chapter 13. Transaction Control
562

<!-- Original PDF Page: 564 -->

Isolation Level
Keeping the work of one database task separated from others is what isolation is about. Changes
made by one statement become visible to all remaining statements executing within the same
transaction, regardless of its isolation level. Changes that are in progress within other transactions
remain invisible to the current transaction as long as they remain uncommitted. The isolation level
and, sometimes, other attributes, determine how transactions will interact when another
transaction wants to commit work.
The ISOLATION LEVEL attribute defines the isolation level for the transaction being started. It is the
most significant transaction parameter for determining its behavior towards other concurrently
running transactions.
The three isolation levels supported in Firebird are:
• SNAPSHOT
• SNAPSHOT TABLE STABILITY
• READ COMMITTED  with three specifications ( READ CONSISTENCY , NO RECORD_VERSION  and
RECORD_VERSION)
SNAPSHOT Isolation Level
SNAPSHOT isolation level — the default level — allows the transaction to see only those changes that
were committed before it was started. Any committed changes made by concurrent transactions
will not be seen in a SNAPSHOT transaction while it is active. The changes will become visible to a
new transaction once the current transaction is either committed or rolled back, but not if it is only
a roll back to a savepoint.
The SNAPSHOT isolation level is also known as “concurrency”.

Autonomous Transactions
Changes made by autonomous transactions are not seen in the context of the
SNAPSHOT transaction that launched it.
Sharing Snapshot Transactions
Using SNAPSHOT AT NUMBER snaphot_number, a SNAPSHOT transaction can be started sharing the
snapshot of another transaction. With this feature it’s possible to create parallel processes (using
different attachments) reading consistent data from a database. For example, a backup process may
create multiple threads reading data from the database in parallel, or a web service may dispatch
distributed sub-services doing processing in parallel.
Alternatively, this feature can also be used via the API, using Transaction Parameter Buffer item
isc_tpb_at_snapshot_number.
The snapshot_number from an active transaction can be obtained with RDB$GET_CONTEXT('SYSTEM',
'SNAPSHOT_NUMBER') in SQL or using the transaction information API call with
fb_info_tra_snapshot_number information tag. The snapshot_number passed to the new transaction
must be a snapshot of a currently active transaction.
Chapter 13. Transaction Control
563

<!-- Original PDF Page: 565 -->


To share a stable view between transactions, the other transaction also needs to
have isolation level SNAPSHOT. With READ COMMITTED, the snapshot number will move
forward.
Example
SET TRANSACTION SNAPSHOT AT NUMBER 12345;
SNAPSHOT TABLE STABILITY Isolation Level
The SNAPSHOT TABLE STABILITY  — or SNAPSHOT TABLE — isolation level is the most restrictive. As in
SNAPSHOT, a transaction in SNAPSHOT TABLE STABILITY  isolation sees only those changes that were
committed before the current transaction was started. After a SNAPSHOT TABLE STABILITY is started,
no other transactions can make any changes to any table in the database that has changes pending
for this transaction. Other transactions can read other data, but any attempt at inserting, updating
or deleting by a parallel process will cause conflict exceptions.
The RESERVING clause can be used to allow other transactions to change data in some tables.
If any other transaction has an uncommitted change pending in any (non- SHARED) table listed in the
RESERVING clause, trying to start a SNAPSHOT TABLE STABILITY transaction will result in an indefinite
wait (default or explicit WAIT), or an exception (NO WAIT or after expiration of the LOCK TIMEOUT).
The SNAPSHOT TABLE STABILITY isolation level is also known as “consistency”.
READ COMMITTED Isolation Level
The READ COMMITTED isolation level allows all data changes that other transactions have committed
since it started to be seen immediately by the uncommitted current transaction. Uncommitted
changes are not visible to a READ COMMITTED transaction.
To retrieve the updated list of rows in the table you are interested in — “refresh” — the SELECT
statement needs to be executed again, whilst still in the uncommitted READ COMMITTED transaction.
Variants of READ COMMITTED
One of three modifying parameters can be specified for READ COMMITTED transactions, depending on
the kind of conflict resolution desired: READ CONSISTENCY, RECORD_VERSION or NO RECORD_VERSION. When
the ReadConsistency setting is set to 1 in firebird.conf (the default) or in databases.conf, these
variants are effectively ignored and behave as READ CONSISTENCY. Otherwise, these variants are
mutually exclusive.
• NO RECORD_VERSION (the default if ReadConsistency = 0) is a kind of two-phase locking mechanism:
it will make the transaction unable to write to any row that has an update pending from
another transaction.
◦ with NO WAIT specified, it will throw a lock conflict error immediately
◦ with WAIT specified, it will wait until the other transaction is either committed or rolled back.
If the other transaction is rolled back, or if it is committed and its transaction ID is older
Chapter 13. Transaction Control
564

<!-- Original PDF Page: 566 -->

than the current transaction’s ID, then the current transaction’s change is allowed. A lock
conflict error is returned if the other transaction was committed and its ID was newer than
that of the current transaction.
• With RECORD_VERSION specified, the transaction reads the latest committed version of the row,
regardless of other pending versions of the row. The lock resolution strategy ( WAIT or NO WAIT)
does not affect the behavior of the transaction at its start in any way.
• With READ CONSISTENCY specified (or ReadConsistency = 1), the execution of a statement obtains a
snapshot of the database to ensure a consistent read at the statement-level of the transactions
committed when execution started.
The other two variants can result in statement-level inconsistent reads as they may read some
but not all changes of a concurrent transaction if that transaction commits during statement
execution. For example, a SELECT COUNT(*) could read some, but not all inserted records of
another transaction if the commit of that transaction occurs while the statement is reading
records.
This statement-level snapshot is obtained for the execution of a top-level statement, nested
statements (triggers, stored procedures and functions, dynamics statements, etc.) use the
statement-level snapshot created for the top-level statement.
 Obtaining a snapshot for READ CONSISTENCY is a very cheap action.
 Setting ReadConsistency is set to 1 by default in firebird.conf.
Handling of Update Conflicts with READ CONSISTENCY
When a statement executes in a READ COMMITTED READ CONSISTENCY  transaction, its
database view is retained in a fashion similar to a SNAPSHOT transaction. This makes it
pointless to wait for the concurrent transaction to commit, in the hope of being able to read
the newly-committed record version. So, when a READ COMMITTED READ CONSISTENCY 
transaction reads data, it behaves similarly to a READ COMMITTED RECORD VERSION 
transaction: it walks the back versions chain looking for a record version visible to the
current snapshot.
When an update conflict occurs, the behaviour of a READ COMMITTED READ CONSISTENCY 
transaction is different from READ COMMITTED RECORD VERSION. The following actions are
performed:
1. Transaction isolation mode is temporarily switched to READ COMMITTED NO RECORD
VERSION.
2. A write-lock is taken for the conflicting record.
3. Remaining records of the current UPDATE/DELETE cursor are processed, and they are write-
locked too.
4. Once the cursor is fetched, all modifications performed since the top-level statement was
started are undone, already taken write-locks for every updated/deleted/locked record are
preserved, all inserted records are removed.
Chapter 13. Transaction Control
565

<!-- Original PDF Page: 567 -->

5. Transaction isolation mode is restored to READ COMMITTED READ CONSISTENCY , a new
statement-level snapshot is created, and the top-level statement is restarted.
This algorithm ensures that already updated records remain locked after restart, they are
visible to the new snapshot, and could be updated again with no further conflicts. Also, due to
READ CONSISTENCY nature, the modified record set remains consistent.

• This restart algorithm is applied to UPDATE, DELETE, SELECT WITH LOCK and
MERGE statements, with or without the RETURNING clause, executed directly
by a client application or inside a PSQL object (stored
procedure/function, trigger, EXECUTE BLOCK, etc).
• If an UPDATE/DELETE statement is positioned on an explicit cursor (using
the WHERE CURRENT OF  clause), then the step (3) above is skipped, i.e.
remaining cursor records are not fetched and write-locked.
• If the top-level statement is selectable and update conflict happens after
one or more records were returned to the client side, then an update
conflict error is reported as usual and restart is not initiated.
• Restart does not happen for statements executed inside autonomous
blocks (IN AUTONOMOUS TRANSACTION DO …).
• After 10 unsuccessful attempts the restart algorithm is aborted, all write
locks are released, transaction isolation mode is restored to READ
COMMITTED READ CONSISTENCY, and an update conflict error is raised.
• Any error not handled at step (3) above aborts the restart algorithm and
statement execution continues normally.
• UPDATE/DELETE triggers fire multiple times for the same record if the
statement execution was restarted and the record is updated/deleted
again.
• Statement restart is usually fully transparent to client applications and
no special actions should be taken by developers to handle it in any way.
The only exception is the code with side effects that are outside the
transactional control, for example:
◦ usage of external tables, sequences or context variables
◦ sending e-mails using UDF or UDR
◦ usage of autonomous transactions or external queries
and so on. Beware that such code could be executed more than once if
update conflicts happen.
• There is no way to detect whether a restart happened, but it could be
done manually using code with side effects as described above, for
example using a context variable.
• Due to historical reasons, error isc_update_conflict is reported as the
secondary error code, with the primary error code being isc_deadlock.
Chapter 13. Transaction Control
566

<!-- Original PDF Page: 568 -->

NO AUTO UNDO
The NO AUTO UNDO  option affects the handling of record versions (garbage) produced by the
transaction in the event of rollback. With NO AUTO UNDO flagged, the ROLLBACK statement marks the
transaction as rolled back without deleting the record versions created in the transaction. They are
left to be mopped up later by garbage collection.
NO AUTO UNDO might be useful when a lot of separate statements are executed that change data in
conditions where the transaction is likely to be committed successfully most of the time.
The NO AUTO UNDO option is ignored for transactions where no changes are made.
RESTART REQUESTS
According to the Firebird sources, this will
Restart all requests in the current attachment to utilize the passed
transaction.
— src/jrd/tra.cpp
The exact semantics and effects of this clause are not clear, and we recommend you do not use this
clause.
AUTO COMMIT
Specifying AUTO COMMIT  enables auto-commit mode for the transaction. In auto-commit mode,
Firebird will internally execute the equivalent of COMMIT RETAIN after each statement execution.

This is not a generally useful auto-commit mode; the same transaction context is
retained until the transaction is ended through a commit or rollback. In other
words, when you use SNAPSHOT or SNAPSHOT TABLE STABILITY, this auto-commit will
not change record visibility (effects of transactions that were committed after this
transaction was started will not be visible).
For READ COMMITTED, the same warnings apply as for commit retaining: prolonged
use of a single transaction in auto-commit mode can inhibit garbage collection and
degrade performance.
IGNORE LIMBO
This flag is used to signal that records created by limbo transactions are to be ignored. Transactions
are left “in limbo” if the second stage of a two-phase commit fails.

Historical Note
IGNORE LIMBO surfaces the TPB parameter isc_tpb_ignore_limbo, available in the API
since InterBase times and is mainly used by gfix.
RESERVING
The RESERVING clause in the SET TRANSACTION statement reserves tables specified in the table list.
Chapter 13. Transaction Control
567

<!-- Original PDF Page: 569 -->

Reserving a table prevents other transactions from making changes in them or even, with the
inclusion of certain parameters, from reading data from them while this transaction is running.
A RESERVING clause can also be used to specify a list of tables that can be changed by other
transactions, even if the transaction is started with the SNAPSHOT TABLE STABILITY isolation level.
One RESERVING clause is used to specify as many reserved tables as required.
Options for RESERVING Clause
If one of the keywords SHARED or PROTECTED is omitted, SHARED is assumed. If the whole FOR clause is
omitted, FOR SHARED READ  is assumed. The names and compatibility of the four access options for
reserving tables are not obvious.
Table 250. Compatibility of Access Options for RESERVING
  SHARED READ SHARED WRITE PROTECTED READ PROTECTED
WRITE
SHARED READ Yes Yes Yes Yes
SHARED WRITE Yes Yes No No
PROTECTED READ Yes No Yes No
PROTECTED
WRITE
Yes No No No
The combinations of these RESERVING clause flags for concurrent access depend on the isolation
levels of the concurrent transactions:
• SNAPSHOT isolation
◦ Concurrent SNAPSHOT transactions with SHARED READ do not affect one other’s access
◦ A concurrent mix of SNAPSHOT and READ COMMITTED transactions with SHARED WRITE do not
affect one another’s access, but they block transactions with SNAPSHOT TABLE STABILITY 
isolation from either reading from or writing to the specified table(s)
◦ Concurrent transactions with any isolation level and PROTECTED READ can only read data from
the reserved tables. Any attempt to write to them will cause an exception
◦ With PROTECTED WRITE, concurrent transactions with SNAPSHOT and READ COMMITTED isolation
cannot write to the specified tables. Transactions with SNAPSHOT TABLE STABILITY  isolation
cannot read from or write to the reserved tables at all.
• SNAPSHOT TABLE STABILITY isolation
◦ All concurrent transactions with SHARED READ, regardless of their isolation levels, can read
from or write (if in READ WRITE mode) to the reserved tables
◦ Concurrent transactions with SNAPSHOT and READ COMMITTED isolation levels and SHARED WRITE
can read data from and write (if in READ WRITE mode) to the specified tables but concurrent
access to those tables from transactions with SNAPSHOT TABLE STABILITY  is blocked whilst
these transactions are active
◦ Concurrent transactions with any isolation level and PROTECTED READ can only read from the
Chapter 13. Transaction Control
568

<!-- Original PDF Page: 570 -->

reserved tables
◦ With PROTECTED WRITE, concurrent SNAPSHOT and READ COMMITTED transactions can read from
but not write to the reserved tables. Access by transactions with the SNAPSHOT TABLE
STABILITY isolation level is blocked.
• READ COMMITTED isolation
◦ With SHARED READ, all concurrent transactions with any isolation level can both read from
and write (if in READ WRITE mode) to the reserved tables
◦ SHARED WRITE allows all transactions in SNAPSHOT and READ COMMITTED isolation to read from
and write (if in READ WRITE mode) to the specified tables and blocks access from transactions
with SNAPSHOT TABLE STABILITY isolation
◦ With PROTECTED READ, concurrent transactions with any isolation level can only read from the
reserved tables
◦ With PROTECTED WRITE, concurrent transactions in SNAPSHOT and READ COMMITTED isolation can
read from but not write to the specified tables. Access from transactions in SNAPSHOT TABLE
STABILITY isolation is blocked.

In Embedded SQL, the USING clause can be used to conserve system resources by
limiting the number of databases a transaction can access. USING is mutually
exclusive with RESERVING. A USING clause in SET TRANSACTION syntax is not supported
in DSQL.
See also
COMMIT, ROLLBACK
13.1.2. COMMIT
Commits a transaction
Available in
DSQL, ESQL
Syntax
COMMIT [TRANSACTION tr_name] [WORK]
  [RETAIN [SNAPSHOT] | RELEASE];
Table 251. COMMIT Statement Parameter
Parameter Description
tr_name Transaction name. Available only in ESQL
The COMMIT statement commits all work carried out in the context of this transaction (inserts,
updates, deletes, selects, execution of procedures). New record versions become available to other
transactions and, unless the RETAIN clause is employed, all server resources allocated to its work are
released.
Chapter 13. Transaction Control
569

<!-- Original PDF Page: 571 -->

If any conflicts or other errors occur in the database during the process of committing the
transaction, the transaction is not committed, and the reasons are passed back to the user
application for handling, and the opportunity to attempt another commit or to roll the transaction
back.
The TRANSACTION and RELEASE clauses are only valid in ESQL.
COMMIT Options
• The optional TRANSACTION tr_name clause, available only in Embedded SQL, specifies the name of
the transaction to be committed. With no TRANSACTION clause, COMMIT is applied to the default
transaction.

In ESQL applications, named transactions make it possible to have several
transactions active simultaneously in one application. If named transactions
are used, a host-language variable with the same name must be declared and
initialized for each named transaction. This is a limitation that prevents
dynamic specification of transaction names and thus, rules out transaction
naming in DSQL.
• The keyword RELEASE is available only in Embedded SQL and enables disconnection from all
databases after the transaction is committed. RELEASE is retained in Firebird only for
compatibility with legacy versions of InterBase. It has been superseded in ESQL by the
DISCONNECT statement.
• The RETAIN [SNAPSHOT] clause is used for the “soft” commit, variously referred to amongst host
languages and their practitioners as COMMIT WITH RETAIN, “CommitRetaining”, “warm commit”, et
al. The transaction is committed, but some server resources are retained and a new transaction
is restarted transparently with the same Transaction ID. The state of row caches and cursors
remains as it was before the soft commit.
For soft-committed transactions whose isolation level is SNAPSHOT or SNAPSHOT TABLE STABILITY,
the view of database state does not update to reflect changes by other transactions, and the user
of the application instance continues to have the same view as when the original transaction
started. Changes made during the life of the retained transaction are visible to that transaction,
of course.

Recommendation
Use of the COMMIT statement in preference to ROLLBACK is recommended for ending
transactions that only read data from the database, because COMMIT consumes
fewer server resources and helps to optimize the performance of subsequent
transactions.
See also
SET TRANSACTION, ROLLBACK
Chapter 13. Transaction Control
570

<!-- Original PDF Page: 572 -->

13.1.3. ROLLBACK
Rolls back a transaction or to a savepoint
Available in
DSQL, ESQL
Syntax
  ROLLBACK [TRANSACTION tr_name] [WORK]
    [RETAIN [SNAPSHOT] | RELEASE]
| ROLLBACK [WORK] TO [SAVEPOINT] sp_name
Table 252. ROLLBACK Statement Parameters
Parameter Description
tr_name Transaction name. Available only in ESQL
sp_name Savepoint name. Available only in DSQL
The ROLLBACK statement rolls back all work carried out in the context of this transaction (inserts,
updates, deletes, selects, execution of procedures). ROLLBACK never fails and, thus, never causes
exceptions. Unless the RETAIN clause is employed, all server resources allocated to the work of the
transaction are released.
The TRANSACTION and RELEASE clauses are only valid in ESQL. The ROLLBACK TO SAVEPOINT statement is
not available in ESQL.
ROLLBACK Options
• The optional TRANSACTION tr_name clause, available only in Embedded SQL, specifies the name of
the transaction to be committed. With no TRANSACTION clause, ROLLBACK is applied to the default
transaction.

In ESQL applications, named transactions make it possible to have several
transactions active simultaneously in one application. If named transactions
are used, a host-language variable with the same name must be declared and
initialized for each named transaction. This is a limitation that prevents
dynamic specification of transaction names and thus, rules out transaction
naming in DSQL.
• The keyword RETAIN keyword specifies that, although all work of the transaction is to be rolled
back, the transaction context is to be retained. Some server resources are retained, and the
transaction is restarted transparently with the same Transaction ID. The state of row caches and
cursors is kept as it was before the “soft” rollback.
For transactions whose isolation level is SNAPSHOT or SNAPSHOT TABLE STABILITY , the view of
database state is not updated by the soft rollback to reflect changes by other transactions. The
user of the application instance continues to have the same view as when the transaction
started originally. Changes that were made and soft-committed during the life of the retained
Chapter 13. Transaction Control
571

<!-- Original PDF Page: 573 -->

transaction are visible to that transaction, of course.
See also
SET TRANSACTION, COMMIT
ROLLBACK TO SAVEPOINT
The ROLLBACK TO SAVEPOINT statement specifies the name of a savepoint to which changes are to be
rolled back. The effect is to roll back all changes made within the transaction, from the specified
savepoint forward until the point when ROLLBACK TO SAVEPOINT is requested.
ROLLBACK TO SAVEPOINT performs the following operations:
• Any database mutations performed since the savepoint was created are undone. User variables
set with RDB$SET_CONTEXT() remain unchanged.
• Any savepoints that were created after the one named are destroyed. Savepoints earlier than
the one named are preserved, along with the named savepoint itself. Repeated rollbacks to the
same savepoint are thus allowed.
• All implicit and explicit record locks that were acquired since the savepoint are released. Other
transactions that have requested access to rows locked after the savepoint are not notified and
will continue to wait until the transaction is committed or rolled back. Other transactions that
have not already requested the rows can request and access the unlocked rows immediately.
See also
SAVEPOINT, RELEASE SAVEPOINT
13.1.4. SAVEPOINT
Creates a savepoint
Syntax
SAVEPOINT sp_name
Table 253. SAVEPOINT Statement Parameter
Parameter Description
sp_name Savepoint name. Available only in DSQL
The SAVEPOINT statement creates an SQL-compliant savepoint that acts as a marker in the “stack” of
data activities within a transaction. Subsequently, the tasks performed in the “stack” can be undone
back to this savepoint, leaving the earlier work and older savepoints untouched. Savepoints are
sometimes called “nested transactions”.
If a savepoint already exists with the same name as the name supplied for the new one, the existing
savepoint is released, and a new one is created using the supplied name.
To roll changes back to the savepoint, the statement ROLLBACK TO SAVEPOINT is used.
Chapter 13. Transaction Control
572

<!-- Original PDF Page: 574 -->


Memory Considerations
The internal mechanism beneath savepoints can consume large amounts of
memory, especially if the same rows receive multiple updates in one transaction.
When a savepoint is no longer needed, but the transaction still has work to do, a
RELEASE SAVEPOINT statement will erase it and thus free the resources.
Sample DSQL session with savepoints
CREATE TABLE TEST (ID INTEGER);
COMMIT;
INSERT INTO TEST VALUES (1);
COMMIT;
INSERT INTO TEST VALUES (2);
SAVEPOINT Y;
DELETE FROM TEST;
SELECT * FROM TEST; -- returns no rows
ROLLBACK TO Y;
SELECT * FROM TEST; -- returns two rows
ROLLBACK;
SELECT * FROM TEST; -- returns one row
See also
ROLLBACK TO SAVEPOINT, RELEASE SAVEPOINT
13.1.5. RELEASE SAVEPOINT
Releases a savepoint
Syntax
RELEASE SAVEPOINT sp_name [ONLY]
Table 254. RELEASE SAVEPOINT Statement Parameter
Parameter Description
sp_name Savepoint name. Available only in DSQL
The statement RELEASE SAVEPOINT  erases a named savepoint, freeing up all the resources it
encompasses. By default, all the savepoints created after the named savepoint are released as well.
The qualifier ONLY directs the engine to release only the named savepoint.
See also
SAVEPOINT
13.1.6. Internal Savepoints
By default, the engine uses an automatic transaction-level system savepoint to perform transaction
rollback. When a ROLLBACK statement is issued, all changes performed in this transaction are backed
Chapter 13. Transaction Control
573