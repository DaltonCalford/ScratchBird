# App E Monitoring Tables



<!-- Original PDF Page: 757 -->

Column Name Data Type Description
RDB$OBJECT_TYPE SMALLINT Identifies the type of the object the
privilege is granted ON
0 - table
1 - view
2 - trigger
5 - procedure
7 - exception
8 - user
9 - domain
11 - character set
13 - role
14 - generator (sequence)
15 - function
16 - BLOB filter
17 - collation
18 - package
RDB$VIEW_RELATIONS
RDB$VIEW_RELATIONS stores the tables that are referred to in view definitions. There is one record for
each table in a view.
Column Name Data Type Description
RDB$VIEW_NAME CHAR(63) View name
RDB$RELATION_NAME CHAR(63) The name of the table, view or stored
procedure the view references
RDB$VIEW_CONTEXT SMALLINT The alias used to reference the view
column in the BLR code of the query
definition
RDB$CONTEXT_NAME CHAR(255) The text associated with the alias
reported in the RDB$VIEW_CONTEXT column
RDB$CONTEXT_TYPE SMALLINT Context type:
0 - table
1 - view
2 - stored procedure
RDB$PACKAGE_NAME CHAR(63) Package name for a stored procedure in
a package
Appendix D: System Tables
756

<!-- Original PDF Page: 758 -->

Appendix E: Monitoring Tables
The Firebird engine can monitor activities in a database and make them available for user queries
via the monitoring tables. The definitions of these tables are always present in the database, all
named with the prefix MON$. The tables are virtual: they are populated with data only at the moment
when the user queries them. That is also one good reason why it is no use trying to create triggers
for them!
The key notion in understanding the monitoring feature is an activity snapshot . The activity
snapshot represents the current state of the database at the start of the transaction in which the
monitoring table query runs. It delivers a lot of information about the database itself, active
connections, users, transactions prepared, running queries and more.
The snapshot is created when any monitoring table is queried for the first time. It is preserved until
the end of the current transaction to maintain a stable, consistent view for queries across multiple
tables, such as a master-detail query. In other words, monitoring tables always behave as though
they were in SNAPSHOT TABLE STABILITY (“consistency”) isolation, even if the current transaction is
started with a lower isolation level.
To refresh the snapshot, the current transaction must be completed and the monitoring tables must
be re-queried in a new transaction context.
Access Security
• SYSDBA and the database owner have full access to all information available from the
monitoring tables
• Regular users can see information about their own connections; other connections are not
visible to them
 In a highly loaded environment, collecting information via the monitoring tables
could have a negative impact on system performance.
List of Monitoring Tables
MON$ATTACHMENTS
Information about active attachments to the database
MON$CALL_STACK
Calls to the stack by active queries of stored procedures and triggers
MON$COMPILED_STATEMENTS
Virtual table listing compiled statements
MON$CONTEXT_VARIABLES
Information about custom context variables
MON$DATABASE
Information about the database to which the CURRENT_CONNECTION is attached
Appendix E: Monitoring Tables
757

<!-- Original PDF Page: 759 -->

MON$IO_STATS
Input/output statistics
MON$MEMORY_USAGE
Memory usage statistics
MON$RECORD_STATS
Record-level statistics
MON$STATEMENTS
Statements prepared for execution
MON$TABLE_STATS
Table-level statistics
MON$TRANSACTIONS
Started transactions
MON$ATTACHMENTS
MON$ATTACHMENTS displays information about active attachments to the database.
Column Name Data Type Description
MON$ATTACHMENT_ID BIGINT Connection identifier
MON$SERVER_PID INTEGER Server process identifier
MON$STATE SMALLINT Connection state:
0 - idle
1 - active
MON$ATTACHMENT_NAME VARCHAR(255) Connection string — the file name and
full path to the primary database file
MON$USER CHAR(63) The name of the user who is using this
connection
MON$ROLE CHAR(63) The role name specified when the
connection was established. If no role
was specified when the connection was
established, the field contains the text
NONE
MON$REMOTE_PROTOCOL VARCHAR(10) Remote protocol name
MON$REMOTE_ADDRESS VARCHAR(255) Remote address (address and server
name)
MON$REMOTE_PID INTEGER Remote client process identifier
Appendix E: Monitoring Tables
758

<!-- Original PDF Page: 760 -->

Column Name Data Type Description
MON$CHARACTER_SET_ID SMALLINT Connection character set identifier (see
RDB$CHARACTER_SET in system table
RDB$TYPES)
MON$TIMESTAMP TIMESTAMP WITH TIME
ZONE
The date and time when the connection
was started
MON$GARBAGE_COLLECTION SMALLINT Garbage collection flag (as specified in
the attachment’s DPB): 1=allowed, 0=not
allowed
MON$REMOTE_PROCESS VARCHAR(255) The full file name and path to the
executable file that established this
connection
MON$STAT_ID INTEGER Statistics identifier
MON$CLIENT_VERSION VARCHAR(255) Client library version
MON$REMOTE_VERSION VARCHAR(255) Remote protocol version
MON$REMOTE_HOST VARCHAR(255) Name of the remote host
MON$REMOTE_OS_USER VARCHAR(255) Name of remote user
MON$AUTH_METHOD VARCHAR(255) Name of authentication plugin used to
connect
MON$SYSTEM_FLAG SMALLINT Flag that indicates the type of
connection:
0 - normal connection
1 - system connection
MON$IDLE_TIMEOUT INTEGER Connection-level idle timeout in
seconds. When 0 is reported the
database ConnectionIdleTimeout from
databases.conf or firebird.conf applies.
MON$IDLE_TIMER TIMESTAMP WITH TIME
ZONE
Idle timer expiration time
MON$STATEMENT_TIMEOUT INTEGER Connection-level statement timeout in
milliseconds. When 0 is reported the
database StatementTimeout from
databases.conf or firebird.conf applies.
MON$WIRE_COMPRESSED BOOLEAN Wire compression active (TRUE) or
inactive (FALSE)
MON$WIRE_ENCRYPTED BOOLEAN Wire encryption active (TRUE) or
inactive (FALSE)
MON$WIRE_CRYPT_PLUGIN VARCHAR(63) Name of the wire encryption plugin
used
MON$SESSION_TIMEZONE CHAR(63) Name of the session time zone
Appendix E: Monitoring Tables
759

<!-- Original PDF Page: 761 -->

Column Name Data Type Description
MON$PARALLEL_WORKERS INTEGER Maximum number of parallel workers
for this connection, 1 means no parallel
workers. “Garbage Collector” and
“Cache Writer” connections may report
0.
Retrieving information about client applications
SELECT MON$USER, MON$REMOTE_ADDRESS, MON$REMOTE_PID, MON$TIMESTAMP
FROM MON$ATTACHMENTS
WHERE MON$ATTACHMENT_ID <> CURRENT_CONNECTION
Using MON$ATTACHMENTS to Kill a Connection
Monitoring tables are read-only. However, the server has a built-in mechanism for deleting (and
only deleting) records in the MON$ATTACHMENTS table, which makes it possible to close a connection to
the database.

Notes
• All the current activity in the connection being deleted is immediately stopped
and all active transactions are rolled back
• The closed connection will return an error with the isc_att_shutdown code to
the application
• Subsequent attempts to use this connection (i.e., use its handle in API calls) will
return errors
• Termination of system connections ( MON$SYSTEM_FLAG = 1 ) is not possible. The
server will skip system connections in a DELETE FROM MON$ATTACHMENTS.
Closing all connections except for your own (current):
DELETE FROM MON$ATTACHMENTS
WHERE MON$ATTACHMENT_ID <> CURRENT_CONNECTION
MON$COMPILED_STATEMENTS
Virtual table listing compiled statements.
Column Name Data Type Description
MON$COMPILED_STATEMENT_ID BIGINT Compiled statement id
MON$SQL_TEXT BLOB TEXT Statement text
MON$EXPLAINED_PLAN BLOB TEXT Explained query plan
MON$OBJECT_NAME CHAR(63) PSQL object name
Appendix E: Monitoring Tables
760

<!-- Original PDF Page: 762 -->

Column Name Data Type Description
MON$OBJECT_TYPE SMALLINT PSQL object type:
2 - trigger
5 - stored procedure
15 - stored function
MON$PACKAGE_NAME CHAR(63) PSQL object package name
MON$STAT_ID INTEGER Statistics identifier
MON$CALL_STACK
MON$CALL_STACK displays calls to the stack from queries executing in stored procedures and triggers.
Column Name Data Type Description
MON$CALL_ID BIGINT Call identifier
MON$STATEMENT_ID BIGINT The identifier of the top-level SQL
statement, the one that initiated the
chain of calls. Use this identifier to find
the records about the active statement
in the MON$STATEMENTS table
MON$CALLER_ID BIGINT The identifier of the calling trigger or
stored procedure
MON$OBJECT_NAME CHAR(63) PSQL object name
MON$OBJECT_TYPE SMALLINT PSQL object type:
2 - trigger
5 - stored procedure
15 - stored function
MON$TIMESTAMP TIMESTAMP WITH TIME
ZONE
The date and time when the call was
started
MON$SOURCE_LINE INTEGER The number of the source line in the
SQL statement being executed at the
moment of the snapshot
MON$SOURCE_COLUMN INTEGER The number of the source column in the
SQL statement being executed at the
moment of the snapshot
MON$STAT_ID INTEGER Statistics identifier
MON$PACKAGE_NAME CHAR(63) Package name for stored procedures or
functions in a package
MON$COMPILED_STATEMENT_ID BIGINT Compiled statement id
 Information about calls during the execution of the EXECUTE STATEMENT statement
Appendix E: Monitoring Tables
761

<!-- Original PDF Page: 763 -->

does not get into the call stack.
Get the call stack for all connections except your own
WITH RECURSIVE
  HEAD AS (
    SELECT
      CALL.MON$STATEMENT_ID, CALL.MON$CALL_ID,
      CALL.MON$OBJECT_NAME, CALL.MON$OBJECT_TYPE
    FROM MON$CALL_STACK CALL
    WHERE CALL.MON$CALLER_ID IS NULL
    UNION ALL
    SELECT
      CALL.MON$STATEMENT_ID, CALL.MON$CALL_ID,
      CALL.MON$OBJECT_NAME, CALL.MON$OBJECT_TYPE
    FROM MON$CALL_STACK CALL
      JOIN HEAD ON CALL.MON$CALLER_ID = HEAD.MON$CALL_ID
  )
SELECT MON$ATTACHMENT_ID, MON$OBJECT_NAME, MON$OBJECT_TYPE
FROM HEAD
  JOIN MON$STATEMENTS STMT ON STMT.MON$STATEMENT_ID = HEAD.MON$STATEMENT_ID
WHERE STMT.MON$ATTACHMENT_ID <> CURRENT_CONNECTION
MON$CONTEXT_VARIABLES
MON$CONTEXT_VARIABLES displays information about custom context variables.
Column Name Data Type Description
MON$ATTACHMENT_ID BIGINT Connection identifier. It contains a valid
value only for a connection-level
context variable. For transaction-level
variables it is NULL.
MON$TRANSACTION_ID BIGINT Transaction identifier. It contains a
valid value only for transaction-level
context variables. For connection-level
variables it is NULL.
MON$VARIABLE_NAME VARCHAR(80) Context variable name
MON$VARIABLE_VALUE VARCHAR(32765) Context variable value
Retrieving all session context variables for the current connection
SELECT
  VAR.MON$VARIABLE_NAME,
  VAR.MON$VARIABLE_VALUE
FROM MON$CONTEXT_VARIABLES VAR
WHERE VAR.MON$ATTACHMENT_ID = CURRENT_CONNECTION
Appendix E: Monitoring Tables
762

<!-- Original PDF Page: 764 -->

MON$DATABASE
MON$DATABASE displays the header information from the database the current user is connected to.
Column Name Data Type Description
MON$DATABASE_NAME VARCHAR(255) The file name and full path of the
primary database file, or the database
alias
MON$PAGE_SIZE SMALLINT Database page size in bytes
MON$ODS_MAJOR SMALLINT Major ODS version, e.g., 11
MON$ODS_MINOR SMALLINT Minor ODS version, e.g., 2
MON$OLDEST_TRANSACTION BIGINT The number of the oldest [interesting]
transaction (OIT)
MON$OLDEST_ACTIVE BIGINT The number of the oldest active
transaction (OAT)
MON$OLDEST_SNAPSHOT BIGINT The number of the transaction that was
active at the moment when the OAT was
started — oldest snapshot transaction
(OST)
MON$NEXT_TRANSACTION BIGINT The number of the next transaction, as
it stood when the monitoring snapshot
was taken
MON$PAGE_BUFFERS INTEGER The number of pages allocated in RAM
for the database page cache
MON$SQL_DIALECT SMALLINT Database SQL Dialect: 1 or 3
MON$SHUTDOWN_MODE SMALLINT The current shutdown state of the
database:
0 - the database is online
1 - multi-user shutdown
2 - single-user shutdown
3 - full shutdown
MON$SWEEP_INTERVAL INTEGER Sweep interval
MON$READ_ONLY SMALLINT Flag indicating whether the database is
read-only (value 1) or read-write (value
0)
MON$FORCED_WRITES SMALLINT Indicates whether the write mode of the
database is set for synchronous write
(forced writes ON, value is 1) or
asynchronous write (forced writes OFF,
value is 0)
Appendix E: Monitoring Tables
763

<!-- Original PDF Page: 765 -->

Column Name Data Type Description
MON$RESERVE_SPACE SMALLINT The flag indicating reserve_space (value
1) or use_all_space (value 0) for filling
database pages
MON$CREATION_DATE TIMESTAMP WITH TIME
ZONE
The date and time when the database
was created or was last restored
MON$PAGES BIGINT The number of pages allocated for the
database on an external device
MON$STAT_ID INTEGER Statistics identifier
MON$BACKUP_STATE SMALLINT Current physical backup (nBackup)
state:
0 - normal
1 - stalled
2 - merge
MON$CRYPT_PAGE BIGINT Number of encrypted pages
MON$OWNER CHAR(63) Username of the database owner
MON$SEC_DATABASE CHAR(7) Displays what type of security database
is used:
Default - default security database, i.e.
security5.fdb
Self - current database is used as
security database
Other - another database is used as
security database (not itself or
security5.fdb)
MON$CRYPT_STATE SMALLINT Current state of database encryption
0 - not encrypted
1 - encrypted
2 - decryption in progress
3 - encryption in progress
MON$GUID CHAR(38) Database GUID (persistent until
restore/fixup)
MON$FILE_ID VARCHAR(255) Unique ID of the database file at the
filesystem level
MON$NEXT_ATTACHMENT BIGINT Current value of the next attachment ID
counter
MON$NEXT_STATEMENT BIGINT Current value of the next statement ID
counter
Appendix E: Monitoring Tables
764

<!-- Original PDF Page: 766 -->

Column Name Data Type Description
MON$REPLICA_MODE SMALLINT Database replica mode
0 - not a replica
1 - read-only replica
2 - read-write replica
MON$IO_STATS
MON$IO_STATS displays input/output statistics. The counters are cumulative, by group, for each group
of statistics.
Column Name Data Type Description
MON$STAT_ID INTEGER Statistics identifier
MON$STAT_GROUP SMALLINT Statistics group:
0 - database
1 - connection
2 - transaction
3 - statement
4 - call
MON$PAGE_READS BIGINT Count of database pages read
MON$PAGE_WRITES BIGINT Count of database pages written to
MON$PAGE_FETCHES BIGINT Count of database pages fetched
MON$PAGE_MARKS BIGINT Count of database pages marked
MON$MEMORY_USAGE
MON$MEMORY_USAGE displays memory usage statistics.
Column Name Data Type Description
MON$STAT_ID INTEGER Statistics identifier
MON$STAT_GROUP SMALLINT Statistics group:
0 - database
1 - connection
2 - transaction
3 - operator
4 - call
Appendix E: Monitoring Tables
765

<!-- Original PDF Page: 767 -->

Column Name Data Type Description
MON$MEMORY_USED BIGINT The amount of memory in use, in bytes.
This data is about the high-level
memory allocation performed by the
server. It can be useful to track down
memory leaks and excessive memory
usage in connections, procedures, etc.
MON$MEMORY_ALLOCATED BIGINT The amount of memory allocated by the
operating system, in bytes. This data is
about the low-level memory allocation
performed by the Firebird memory
manager — the amount of memory
allocated by the operating
system — which can allow you to
control the physical memory usage.
MON$MAX_MEMORY_USED BIGINT The maximum number of bytes used by
this object
MON$MAX_MEMORY_ALLOCATED BIGINT The maximum number of bytes
allocated for this object by the operating
system

Counters associated with database-level records MON$DATABASE (MON$STAT_GROUP = 0),
display memory allocation for all connections. In the Classic and SuperClassic zero
values of the counters indicate that these architectures have no common cache.
Minor memory allocations are not accrued here but are added to the database
memory pool instead.
Getting 10 requests consuming the most memory
SELECT
  STMT.MON$ATTACHMENT_ID,
  STMT.MON$SQL_TEXT,
  MEM.MON$MEMORY_USED
FROM MON$MEMORY_USAGE MEM
NATURAL JOIN MON$STATEMENTS STMT
ORDER BY MEM.MON$MEMORY_USED DESC
FETCH FIRST 10 ROWS ONLY
MON$RECORD_STATS
MON$RECORD_STATS displays record-level statistics. The counters are cumulative, by group, for each
group of statistics.
Appendix E: Monitoring Tables
766

<!-- Original PDF Page: 768 -->

Column Name Data Type Description
MON$STAT_ID INTEGER Statistics identifier
MON$STAT_GROUP SMALLINT Statistics group:
0 - database
1 - connection
2 - transaction
3 - statement
4 - call
MON$RECORD_SEQ_READS BIGINT Count of records read sequentially
MON$RECORD_IDX_READS BIGINT Count of records read via an index
MON$RECORD_INSERTS BIGINT Count of inserted records
MON$RECORD_UPDATES BIGINT Count of updated records
MON$RECORD_DELETES BIGINT Count of deleted records
MON$RECORD_BACKOUTS BIGINT Count of records backed out
MON$RECORD_PURGES BIGINT Count of records purged
MON$RECORD_EXPUNGES BIGINT Count of records expunged
MON$RECORD_LOCKS BIGINT Number of records locked
MON$RECORD_WAITS BIGINT Number of update, delete or lock
attempts on records owned by other
active transactions. Transaction is in
WAIT mode.
MON$RECORD_CONFLICTS BIGINT Number of unsuccessful update, delete
or lock attempts on records owned by
other active transactions. These are
reported as update conflicts.
MON$BACKVERSION_READS BIGINT Number of back-versions read to find
visible records
MON$FRAGMENT_READS BIGINT Number of fragmented records read
MON$RECORD_RPT_READS BIGINT Number of repeated reads of records
MON$RECORD_IMGC BIGINT Number of records processed by the
intermediate garbage collector
MON$STATEMENTS
MON$STATEMENTS displays statements prepared for execution.
Column Name Data Type Description
MON$STATEMENT_ID BIGINT Statement identifier
MON$ATTACHMENT_ID BIGINT Connection identifier
Appendix E: Monitoring Tables
767

<!-- Original PDF Page: 769 -->

Column Name Data Type Description
MON$TRANSACTION_ID BIGINT Transaction identifier
MON$STATE SMALLINT Statement state:
0 - idle
1 - active
2 - stalled
MON$TIMESTAMP TIMESTAMP WITH TIME
ZONE
The date and time when the statement
was prepared
MON$SQL_TEXT BLOB TEXT Statement text in SQL
MON$STAT_ID INTEGER Statistics identifier
MON$EXPLAINED_PLAN BLOB TEXT Explained execution plan
MON$STATEMENT_TIMEOUT INTEGER Connection-level statement timeout in
milliseconds. When 0 is reported the
timeout of
MON$ATTACHMENT.MON$STATEMENT_TIMEOUT
for this connection applies.
MON$STATEMENT_TIMER TIMESTAMP WITH TIME
ZONE
Statement timer expiration time
MON$COMPILED_STATEMENT_ID BIGINT Compiled statement id
The STALLED state indicates that, at the time of the snapshot, the statement had an open cursor and
was waiting for the client to resume fetching rows.
Display active queries, excluding those running in your connection
SELECT
  ATT.MON$USER,
  ATT.MON$REMOTE_ADDRESS,
  STMT.MON$SQL_TEXT,
  STMT.MON$TIMESTAMP
FROM MON$ATTACHMENTS ATT
JOIN MON$STATEMENTS STMT ON ATT.MON$ATTACHMENT_ID = STMT.MON$ATTACHMENT_ID
WHERE ATT.MON$ATTACHMENT_ID <> CURRENT_CONNECTION
AND STMT.MON$STATE = 1
Using MON$STATEMENTS to Cancel a Query
Monitoring tables are read-only. However, the server has a built-in mechanism for deleting (and
only deleting) records in the MON$STATEMENTS table, which makes it possible to cancel a running
query.

Notes
• If no statements are currently being executed in the connection, any attempt to
Appendix E: Monitoring Tables
768

<!-- Original PDF Page: 770 -->

cancel queries will not proceed
• After a query is cancelled, calling execute/fetch API functions will return an
error with the isc_cancelled code
• Subsequent queries from this connection will proceed as normal
• Cancellation of the statement does not occur synchronously, it only marks the
request for cancellation, and the cancellation itself is done asynchronously by
the server
Example
Cancelling all active queries for the specified connection:
DELETE FROM MON$STATEMENTS
  WHERE MON$ATTACHMENT_ID = 32
MON$TABLE_STATS
MON$TABLE_STATS reports table-level statistics.
Column Name Data Type Description
MON$STAT_ID INTEGER Statistics identifier
MON$STAT_GROUP SMALLINT Statistics group:
0 - database
1 - connection
2 - transaction
3 - statement
4 - call
MON$TABLE_NAME CHAR(63) Name of the table
MON$RECORD_STAT_ID INTEGER Link to MON$RECORD_STATS
Getting statistics at the record level for each table for the current connection
SELECT
  t.mon$table_name,
  r.mon$record_inserts,
  r.mon$record_updates,
  r.mon$record_deletes,
  r.mon$record_backouts,
  r.mon$record_purges,
  r.mon$record_expunges,
  ------------------------
  r.mon$record_seq_reads,
  r.mon$record_idx_reads,
  r.mon$record_rpt_reads,
  r.mon$backversion_reads,
Appendix E: Monitoring Tables
769

<!-- Original PDF Page: 771 -->

r.mon$fragment_reads,
  ------------------------
  r.mon$record_locks,
  r.mon$record_waits,
  r.mon$record_conflicts,
  ------------------------
  a.mon$stat_id
FROM mon$record_stats r
JOIN mon$table_stats t ON r.mon$stat_id = t.mon$record_stat_id
JOIN mon$attachments a ON t.mon$stat_id = a.mon$stat_id
WHERE a.mon$attachment_id = CURRENT_CONNECTION
MON$TRANSACTIONS
MON$TRANSACTIONS reports started transactions.
Column Name Data Type Description
MON$TRANSACTION_ID BIGINT Transaction identifier (number)
MON$ATTACHMENT_ID BIGINT Connection identifier
MON$STATE SMALLINT Transaction state:
0 - idle
1 - active
MON$TIMESTAMP TIMESTAMP WITH TIME
ZONE
The date and time when the transaction
was started
MON$TOP_TRANSACTION BIGINT Top-level transaction identifier
(number)
MON$OLDEST_TRANSACTION BIGINT Transaction ID of the oldest [interesting]
transaction (OIT)
MON$OLDEST_ACTIVE BIGINT Transaction ID of the oldest active
transaction (OAT)
MON$ISOLATION_MODE SMALLINT Isolation mode (level):
0 - consistency (snapshot table stability)
1 - concurrency (snapshot)
2 - read committed record version
3 - read committed no record version
4 - read committed read consistency
MON$LOCK_TIMEOUT SMALLINT Lock timeout:
-1 - wait forever
0 - no waiting
1 or greater - lock timeout in seconds
Appendix E: Monitoring Tables
770