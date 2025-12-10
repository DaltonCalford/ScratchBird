# 15 Management Statements



<!-- Original PDF Page: 623 -->

access to a table, view or another executable object, the target object is not accessible if the invoker
does not have the necessary privileges on that object. That is, by default all executable objects have
the SQL SECURITY INVOKER property, and any caller lacking the necessary privileges will be rejected.
The default SQL Security behaviour of a database can be overridden using ALTER DATABASE.
If a routine has the SQL SECURITY DEFINER property applied, the invoking user or routine will be able
to execute it if the required privileges have been granted to its owner, without the need for the
caller to be granted those privileges as well.
In summary:
• If INVOKER is set, the access rights for executing the call to an executable object are determined
by checking the current user’s active set of privileges
• If DEFINER is set, the access rights of the object owner will be applied instead, regardless of the
current user’s active set of privileges.
Chapter 14. Security
622

<!-- Original PDF Page: 624 -->

Chapter 15. Management Statements
Management statement are a class of SQL statements for administering aspects of the client/server
environment, usually for the current session. Typically, such statements start with the verb SET.

The isql tool also has a collection of SET commands. Those commands are not part
of Firebird’s SQL lexicon. For information on isqls SET commands, see Isql Set
Commands in Firebird Interactive SQL Utility.
Management statements can run anywhere DSQL can run, but typically, the developer will want to
run a management statement in a database trigger. A subset of management statement can be used
directly in PSQL modules without the need to wrap them in an EXECUTE STATEMENT block. For more
details of the current set, see Management Statements in PSQL in the PSQL chapter.
Most of the management statements affect the current connection (attachment, or “session”) only,
and do not require any authorization over and above the login privileges of the current user
without elevated privileges.
Some management statements operate beyond the scope of the current session. Examples are the
ALTER DATABASE {BEGIN | END} BACKUP  statements to control the “copy-safe” mode, or the ALTER
EXTERNAL CONNECTIONS POOL  statements to manage connection pooling. A set of system privileges ,
analogous with SQL privileges granted for database objects, is provided to enable the required
authority to run a specific management statement in this category.

Some statements of this class use the verb ALTER, although management statements
should not be confused with DDL ALTER statements that modify database objects
like tables, views, procedures, roles, et al.
Although some ALTER DATABASE clauses (e.g. BEGIN BACKUP) can be considered as
management statements, they are documented in the DDL chapter.
Unless explicitly mentioned otherwise in an “Available in” section, management statements are
available in DSQL and PSQL. Availability in ESQL is not tracked by this Language Reference.
15.1. Data Type Behaviour
15.1.1. SET BIND (Data Type Coercion Rules)
Configures data type coercion rules for the current session
Syntax
SET BIND OF <type_from> TO <type_to>
<type_from> ::=
    <scalar_datatype>
  | <blob_datatype>
  | TIME ZONE
Chapter 15. Management Statements
623

<!-- Original PDF Page: 625 -->

| VARCHAR | {CHARACTER | CHAR} VARYING
<type_to> ::=
    <scalar_datatype>
  | <blob_datatype>
  | VARCHAR | {CHARACTER | CHAR} VARYING
  | LEGACY | NATIVE | EXTENDED
  | EXTENDED TIME WITH TIME ZONE
  | EXTENDED TIMESTAMP WITH TIME ZONE
<scalar_datatype> ::=
  !! See Scalar Data Types Syntax !!
<blob_datatype> ::=
  !! See BLOB Data Types Syntax !!
This statement makes it possible to substitute one data type with another when performing client-
server interactions. In other words, type_from returned by the engine is represented as type_to in
the client API.

Only fields returned by the database engine in regular messages are substituted
according to these rules. Variables returned as an array slice are not affected by
the SET BIND statement.
When a partial type definition is used (e.g. CHAR instead of CHAR(n)) in from_type, the coercion is
performed for all CHAR columns. The special partial type TIME ZONE stands for TIME WITH TIME ZONE 
and TIMESTAMP WITH TIME ZONE. When a partial type definition is used in to_type, the engine defines
missing details about that type automatically based on source column.
Changing the binding of any NUMERIC or DECIMAL data type does not affect the underlying integer
type. In contrast, changing the binding of an integer data type also affects appropriate NUMERIC and
DECIMAL types. For example, SET BIND OF INT128 TO DOUBLE PRECISION  will also map NUMERIC and
DECIMAL with precision 19 or higher, as these types use INT128 as their underlying type.
The special type LEGACY is used when a data type, missing in previous Firebird version, should be
represented in a way, understandable by old client software (possibly with data loss). The coercion
rules applied in this case are shown in the table below.
Table 267. Native to LEGACY coercion rules
Native data type Legacy data type
BOOLEAN CHAR(5)
DECFLOAT DOUBLE PRECISION
INT128 BIGINT
TIME WITH TIME ZONE TIME WITHOUT TIME ZONE
TIMESTAMP WITH TIME ZONE TIMESTAMP WITHOUT TIME ZONE
Using EXTENDED for type_to causes the engine to coerce to an extended form of the type_from data
Chapter 15. Management Statements
624

<!-- Original PDF Page: 626 -->

type. Currently, this works only for TIME/TIMESTAMP WITH TIME ZONE , they are coerced to EXTENDED
TIME/TIMESTAMP WITH TIME ZONE . The EXTENDED type contains both the time zone name, and the
corresponding GMT offset, so it remains usable if the client application cannot process named time
zones properly (e.g. due to the missing ICU library).
Setting a binding to NATIVE resets the existing coercion rule for this data type and returns it in its
native format.
The initial bind rules of a connection be configured through the DPB by providing a semicolon
separated list of <type_from> TO <type_to> options as the string value of isc_dpb_set_bind.
Execution of ALTER SESSION RESET will revert to the binding rules configured through the DPB, or
otherwise the system default.

It is also possible to configure a default set of data type coercion rules for all clients
through the DataTypeCompatibility configuration option, either as a global
configuration in firebird.conf or per database in databases.conf.
DataTypeCompatibility currently has two possible values: 3.0 and 2.5. The 3.0
option maps data types introduced after Firebird 3.0 — in particular DECIMAL
/NUMERIC with precision 19 or higher, INT128, DECFLOAT, and TIME/TIMESTAMP WITH
TIME ZONE — to data types supported in Firebird 3.0. The 2.5 option also converts
the BOOLEAN data type.
See the Native to LEGACY coercion rules for details. This setting allows legacy client
applications to work with Firebird 5.0 without recompiling or otherwise adjusting
them to understand the new data types.
SET BIND Examples
-- native
SELECT CAST('123.45' AS DECFLOAT(16)) FROM RDB$DATABASE;
                   CAST
=======================
                 123.45
-- double
SET BIND OF DECFLOAT TO DOUBLE PRECISION;
SELECT CAST('123.45' AS DECFLOAT(16)) FROM RDB$DATABASE;
                   CAST
=======================
      123.4500000000000
-- still double
SET BIND OF DECFLOAT(34) TO CHAR;
SELECT CAST('123.45' AS DECFLOAT(16)) FROM RDB$DATABASE;
Chapter 15. Management Statements
625

<!-- Original PDF Page: 627 -->

CAST
=======================
      123.4500000000000
-- text
SELECT CAST('123.45' AS DECFLOAT(34)) FROM RDB$DATABASE;
CAST
==========================================
123.45
In the case of missing ICU on the client side:
SELECT CURRENT_TIMESTAMP FROM RDB$DATABASE;
                                        CURRENT_TIMESTAMP
=========================================================
2020-02-21 16:26:48.0230 GMT*
SET BIND OF TIME ZONE TO EXTENDED;
SELECT CURRENT_TIMESTAMP FROM RDB$DATABASE;
                                        CURRENT_TIMESTAMP
=========================================================
2020-02-21 19:26:55.6820 +03:00
15.1.2. SET DECFLOAT
Configures DECFLOAT rounding and error behaviour for the current session
Syntax
SET DECFLOAT
  { ROUND <round_mode>
  | TRAPS TO [<trap_opt> [, <trap_opt> ...]] }
<round_mode> ::=
    CEILING | UP | HALF_UP | HALF_EVEN
  | HALF_DOWN | DOWN | FLOOR | REROUND
<trap_opt> ::=
    DIVISON_BY_ZERO | INEXACT | INVALID_OPERATION
  | OVERFLOW | UNDERFLOW
SET DECFLOAT ROUND
SET DECFLOAT ROUND  changes the rounding behaviour of operations on DECFLOAT. The default
rounding mode is HALF_UP. The initial configuration of a connection can also be specified using the
Chapter 15. Management Statements
626

<!-- Original PDF Page: 628 -->

DPB tag isc_dpb_decfloat_round with the desired round_mode as string value.
The valid rounding modes are:
CEILING towards +infinity
UP away from 0
HALF_UP to nearest, if equidistant, then up (default)
HALF_EVEN to nearest, if equidistant, ensure last digit in the result will be even
HALF_DOWN to nearest, if equidistant, then down
DOWN towards 0
FLOOR towards -infinity
REROUND up if digit to be rounded is 0 or 5, down in other cases
The current value for the connection can be found using RDB$GET_CONTEXT('SYSTEM',
'DECFLOAT_ROUND').
Execution of ALTER SESSION RESET will revert to the value configured through the DPB, or otherwise
the system default.
SET DECFLOAT TRAPS
SET DECFLOAT TRAPS  changes the error behaviour of operations on DECFLOAT. The default traps are
DIVISION_BY_ZERO,INVALID_OPERATION,OVERFLOW; this default matches the behaviour specified in the
SQL standard for DECFLOAT. This statement controls whether certain exceptional conditions result in
an error (“trap”) or alternative handling (for example, an underflow returns 0 when not set, or an
overflow returns an infinity). The initial configuration of a connection can also be specified using
the DPB tag isc_dpb_decfloat_traps with the desired comma-separated trap_opt values as a string
value.
Valid trap options (exceptional conditions) are:
Division_by_zero (set by default)
Inexact  — 
Invalid_operation (set by default)
Overflow (set by default)
Underflow  — 
The current value for the connection can be found using RDB$GET_CONTEXT('SYSTEM',
'DECFLOAT_TRAPS').
Execution of ALTER SESSION RESET will revert to the value configured through the DPB, or otherwise
the system default.
Chapter 15. Management Statements
627

<!-- Original PDF Page: 629 -->

15.2. Connections Pool Management
Management statements to manage the external connections pool.
This connection pool is part of the Firebird server and used for connections to other databases or
servers from the Firebird server itself.
15.2.1. ALTER EXTERNAL CONNECTIONS POOL
Manages the external connections pool
Syntax
ALTER EXTERNAL CONNECTIONS POOL
  { CLEAR ALL
  | CLEAR OLDEST
  | SET LIFETIME lifetime <time-unit>
  | SET SIZE size }
<time-unit> ::= SECOND | MINUTE | HOUR
Table 268. ALTER EXTERNAL CONNECTIONS POOL Statement Parameters
Parameter Description
lifetime Maximum lifetime of a connection in the pool. Minimum values is 1
SECOND, maximum is 24 HOUR.
size Maximum size of the connection pool. Range 0 - 1000. Setting to 0 disables
the external connections pool.
When prepared it is described like a DDL statement, but its effect is immediate — it is executed
immediately and to completion, without waiting for transaction commit.
This statement can be issued from any connection, and changes are applied to the in-memory
instance of the pool in the current Firebird process. If the process is Firebird Classic, execution only
affects the current process (current connection), and does not affect other Classic processes.
Changes made with ALTER EXTERNAL CONNECTIONS POOL are not persistent: after a restart, Firebird will
use the pool settings configured in firebird.conf by ExtConnPoolSize and ExtConnPoolLifeTime.
Clauses of ALTER EXTERNAL CONNECTIONS POOL
CLEAR ALL
Closes all idle connections and disassociates currently active connections; they are immediately
closed when unused.
CLEAR OLDEST
Closes expired connections
Chapter 15. Management Statements
628

<!-- Original PDF Page: 630 -->

SET LIFETIME
Configures the maximum lifetime of an idle connection in the pool. The default value (in
seconds) is set using the parameter ExtConnPoolLifetime in firebird.conf.
SET SIZE
Configures the maximum number of idle connections in the pool. The default value is set using
the parameter ExtConnPoolSize in firebird.conf.
How the Connection Pool Works
Every successful connection is associated with a pool, which maintains two lists — one for idle
connections and one for active connections. When a connection in the “active” list has no active
requests and no active transactions, it is assumed to be “unused”. A reset of the unused connection
is attempted using an ALTER SESSION RESET statement and,
• if the reset succeeds (no errors occur) the connection is moved into the “idle” list;
• if the reset fails, the connection is closed;
• if the pool has reached its maximum size, the oldest idle connection is closed.
• When the lifetime of an idle connection expires, it is deleted from the pool and closed.
New Connections
When the engine is asked to create a new external connection, the pool first looks for a candidate in
the “idle” list. The search, which is case-sensitive, involves four parameters:
1. connection string
2. username
3. password
4. role
If a suitable connection is found, it is tested to check that it is still alive.
• If it fails the check, it is deleted, and the search is repeated, without reporting any error to the
client
• Otherwise, the live connection is moved from the “idle” list to the “active” list and returned to
the caller
• If there are multiple suitable connections, the most recently used one is chosen
• If there is no suitable connection, a new one is created and added to the “active” list.
Who Can Alter the External Connections Pool
The ALTER EXTERNAL CONNECTIONS POOL statement can be executed by:
• Administrators
• Users with the MODIFY_EXT_CONN_POOL privilege
Chapter 15. Management Statements
629

<!-- Original PDF Page: 631 -->

See also
RDB$GET_CONTEXT
15.3. Changing the Current Role
15.3.1. SET ROLE
Sets the active role of the current session
Available in
DSQL
Syntax
SET ROLE {role_name | NONE}
Table 269. SET ROLE Statement Parameters
Parameter Description
role_name The name of the role to apply
The SET ROLE statement allows a user to assume a different role; it sets the CURRENT_ROLE context
variable to role_name, if that role has been granted to the CURRENT_USER. For this session, the user
receives the privileges granted by that role. Any rights granted to the previous role are removed
from the session. Use NONE instead of role_name to clear the CURRENT_ROLE.
When the specified role does not exist or has not been explicitly granted to the user, the error “ Role
role_name is invalid or unavailable” is raised.
SET ROLE Examples
1. Change the current role to MANAGER
SET ROLE manager;
select current_role from rdb$database;
ROLE
=======================
MANAGER
2. Clear the current role
SET ROLE NONE;
select current_role from rdb$database;
ROLE
=======================
Chapter 15. Management Statements
630

<!-- Original PDF Page: 632 -->

NONE
See also
SET TRUSTED ROLE, GRANT
15.3.2. SET TRUSTED ROLE
Sets the active role of the current session to the trusted role
Available in
DSQL
Syntax
SET TRUSTED ROLE
The SET TRUSTED ROLE statement makes it possible to assume the role assigned to the user through a
mapping rule (see Mapping of Users to Objects ). The role assigned through a mapping rule is
assumed automatically on connect, if the user hasn’t specified an explicit role. The SET TRUSTED ROLE
statement makes it possible to assume the mapped (or “trusted”) role at a later time, or to assume it
again after the current role was changed using SET ROLE.
A trusted role is not a specific type of role, but can be any role that was created using CREATE ROLE,
or a predefined system role such as RDB$ADMIN. An attachment (session) has a trusted role when the
security objects mapping subsystem  finds a match between the authentication result passed from
the plugin and a local or global mapping to a role for the current database. The role may be one
that is not granted explicitly to that user.
When a session has no trusted role, executing SET TRUSTED ROLE will raise error “ Your attachment
has no trusted role”.

While the CURRENT_ROLE can be changed using SET ROLE, it is not always possible to
revert to a trusted role using the same command, because SET ROLE checks if the
role has been granted to the user. With SET TRUSTED ROLE, the trusted role can be
assumed again even when SET ROLE fails.
SET TRUSTED ROLE Examples
1. Assuming a mapping rule that assigns the role ROLE1 to a user ALEX:
CONNECT 'employee' USER ALEX PASSWORD 'password';
SELECT CURRENT_ROLE FROM RDB$DATABASE;
ROLE
===============================
ROLE1
SET ROLE ROLE2;
Chapter 15. Management Statements
631

<!-- Original PDF Page: 633 -->

SELECT CURRENT_ROLE FROM RDB$DATABASE;
ROLE
===============================
ROLE2
SET TRUSTED ROLE;
SELECT CURRENT_ROLE FROM RDB$DATABASE;
ROLE
===============================
ROLE1
See also
SET ROLE, Mapping of Users to Objects
15.4. Session Timeouts
Statements for management of timeouts of the current connection.
15.4.1. SET SESSION IDLE TIMEOUT
Sets the session idle timeout
Syntax
SET SESSION IDLE TIMEOUT value [<time-unit>]
<time-unit> ::= MINUTE | HOUR | SECOND
Table 270. SET SESSION IDLE TIMEOUT Statement Parameters
Parameter Description
value The timeout duration expressed in time-unit. A value of 0 defers to
connection idle timeout configured for the database.
time-unit Time unit of the timeout. Defaults to MINUTE.
The SET SESSION IDLE TIMEOUT sets an idle timeout at connection level and takes effect immediately.
The statement can run outside transaction control (without an active transaction).
Setting a value larger than configured for the database is allowed, but is effectively ignored, see also
Determining the Timeout that is In Effect.
The current timeout set for the session can be retrieved through RDB$GET_CONTEXT, namespace SYSTEM
and variable SESSION_IDLE_TIMEOUT. Information is also available from MON$ATTACHMENTS:
MON$IDLE_TIMEOUT
Connection-level idle timeout in seconds; 0 if timeout is not set.
Chapter 15. Management Statements
632

<!-- Original PDF Page: 634 -->

MON$IDLE_TIMER
Idle timer expiration time; contains NULL if an idle timeout was not set, or if a timer is not
running.
Both RDB$GET_CONTEXT('SYSTEM', 'SESSION_IDLE_TIMEOUT')  and MON$ATTACHMENTS.MON$IDLE_TIMEOUT
report the idle timeout configured for the connection; they do not report the effective idle timeout.
The session idle timeout is reset when ALTER SESSION RESET is executed.
Idle Session Timeouts
An idle session timeout allows a use connection to close automatically after a specified period of
inactivity. A database administrator can use it to enforce closure of old connections that have
become inactive, to reduce unnecessary consumption of resources. It can also be used by
application and tools developers as an alternative to writing their own modules for controlling
connection lifetime.
By default, the idle timeout is not enabled. No minimum or maximum limit is imposed, but a
reasonably large period — such as a few hours — is recommended.
How the Idle Session Timeout Works
• When the user API call leaves the engine (returns to the calling connection) a special idle timer
associated with the current connection is started
• When another user API call from that connection enters the engine, the idle timer is stopped
and reset to zero
• If the maximum idle time is exceeded, the engine immediately closes the connection in the same
way as with asynchronous connection cancellation:
◦ all active statements and cursors are closed
◦ all active transactions are rolled back
◦ The network connection remains open at this point, allowing the client application to get the
exact error code on the next API call. The network connection will be closed on the server
side, after an error is reported or in due course as a result of a network timeout from a
client-side disconnection.

Whenever a connection is cancelled, the next user API call returns the error
isc_att_shutdown with a secondary error specifying the exact reason. Now, we have
isc_att_shut_idle
Idle timeout expired
in addition to
isc_att_shut_killed
Killed by database administrator
Chapter 15. Management Statements
633

<!-- Original PDF Page: 635 -->

isc_att_shut_db_down
Database is shut down
isc_att_shut_engine
Engine is shut down
Setting the Idle Session Timeout
 The idle timer will not start if the timeout period is set to zero.
An idle session timeout can be set:
• At database level, the database administrator can set the configuration parameter
ConnectionIdleTimeout, an integer value in minutes. The default value of zero means no timeout
is set. It is configurable per-database, so it may be set globally in firebird.conf and overridden
for individual databases in databases.conf as required.
The scope of this method is all user connections, except system connections (garbage collector,
cache writer, etc.).
• at connection level, the idle session timeout is supported by both the SET SESSION IDLE TIMEOUT
statement and the API ( setIdleTimeout). The scope of this method is specific to the supplied
connection (attachment). Its value in the API is in seconds. In the SQL syntax it can be hours,
minutes or seconds. Scope for this method is the connection to which it is applied.
 For more information about the API calls, consult the Firebird 4.0 Release Notes.
Determining the Timeout that is In Effect
The effective idle timeout value is determined whenever a user API call leaves the engine, checking
first at connection level and then at database level. A connection-level timeout can override the
value of a database-level setting, as long as the period of time for the connection-level setting is no
longer than any non-zero timeout that is applicable at database level.

Take note of the difference between the time units at each level. At database level,
in the configuration files, the unit for SessionTimeout is minutes. In SQL, the default
unit is minutes but can also be expressed in hours or seconds explicitly. At the API
level, the unit is seconds.
Absolute precision is not guaranteed in any case, especially when the system load
is high, but timeouts are guaranteed not to expire earlier than the moment
specified.
15.4.2. SET STATEMENT TIMEOUT
Sets the statement timeout for a connection
Chapter 15. Management Statements
634

<!-- Original PDF Page: 636 -->

Syntax
SET STATEMENT TIMEOUT value [<time-unit>]
<time-unit> ::= SECOND | MILLISECOND | MINUTE | HOUR
Table 271. SET STATEMENT TIMEOUT Statement Parameters
Parameter Description
value The timeout duration expressed in time-unit. A value of 0 defers to
statement timeout configured for the database.
time-unit Time unit of the timeout. Defaults to SECOND.
The SET STATEMENT TIMEOUT  sets a statement timeout at connection level and takes effect
immediately. The statement can run outside transaction control (without an active transaction).
Setting a value larger than configured for the database is allowed, but is effectively ignored, see also
Determining the Statement Timeout that is In Effect.
The current statement timeout set for the session can be retrieved through RDB$GET_CONTEXT,
namespace SYSTEM and variable STATEMENT_TIMEOUT. Information is also available from
MON$ATTACHMENTS:
MON$STATEMENT_TIMEOUT
Connection-level statement timeout in milliseconds; 0 if timeout is not set.
In MON$STATEMENTS:
MON$STATEMENT_TIMEOUT
Statement-level statement timeout in milliseconds; 0 if timeout is not set.
MON$STATEMENT_TIMER
Timeout timer expiration time; contains NULL if an idle timeout was not set, or if a timer is not
running.
Both RDB$GET_CONTEXT('SYSTEM', 'STATEMENT_TIMEOUT')  and MON$ATTACHMENTS.MON$STATEMENT_TIMEOUT
report the statement timeout configured for the connection, and
MON$STATEMENTS.MON$STATEMENT_TIMEOUT for the statement; they do not report the effective statement
timeout.
The statement timeout is reset when ALTER SESSION RESET is executed.
Statement Timeouts
The statement timeout feature allows execution of a statement to be stopped automatically when it
has been running longer than a given timeout period. It gives the database administrator an
instrument for limiting excessive resource consumption from heavy queries.
Statement timeouts can also be useful to application developers when creating and debugging
Chapter 15. Management Statements
635

<!-- Original PDF Page: 637 -->

complex queries without advance knowledge of execution time. Testers and others could find them
handy for detecting long-running queries and establishing finite run times for test suites.
How the Statement Timeout Works
When the statement starts execution, or a cursor is opened, the engine starts a special timer. It is
stopped when the statement completes execution, or the last record has been fetched by the cursor.
 A fetch does not reset this timer.
When the timeout point is reached:
• if statement execution is active, it stops at closest possible moment
• if statement is not active currently (between fetches, for example), it is marked as cancelled,
and the next fetch will break execution and return an error

Statement types excluded from timeouts
Statement timeouts are not applicable to some types of statement and will be
ignored:
• All DDL statements
• All internal queries issued by the engine itself
Setting a Statement Timeout
 The timer will not start if the timeout period is set to zero.
A statement timeout can be set:
• at database level, by the database administrator, by setting the configuration parameter
StatementTimeout in firebird.conf or databases.conf. StatementTimeout is an integer representing
the number of seconds after which statement execution will be cancelled automatically by the
engine. Zero means no timeout is set. A non-zero setting will affect all statements in all
connections.
• at connection level, using SET STATEMENT TIMEOUT  or the API for setting a statement timeout
(setStatementTimeout). A connection-level setting (via SQL or the API) affects all statements for
the given connection; units for the timeout period at this level can be specified to any
granularity from hours to milliseconds.
• at statement level, using the API, in milliseconds
Determining the Statement Timeout that is In Effect
The statement timeout value that is in effect is determined whenever a statement starts executing,
or a cursor is opened. In searching out the timeout in effect, the engine goes up through the levels,
from statement through to database and/or global levels until it finds a non-zero value. If the value
in effect turns out to be zero then no statement timer is running and no timeout applies.
A statement-level or connection-level timeout can override the value of a database-level setting, as
Chapter 15. Management Statements
636

<!-- Original PDF Page: 638 -->

long as the period of time for the lower-level setting is no longer than any non-zero timeout that is
applicable at database level.

Take note of the difference between the time units at each level. At database level,
in the conf file, the unit for StatementTimeout is seconds. In SQL, the default unit is
seconds but can be expressed in hours, minutes or milliseconds explicitly. At the
API level, the unit is milliseconds.
Absolute precision is not guaranteed in any case, especially when the system load
is high, but timeouts are guaranteed not to expire earlier than the moment
specified.
Whenever a statement times out and is cancelled, the next user API call returns the error
isc_cancelled with a secondary error specifying the exact reason, viz.,
isc_cfg_stmt_timeout
Config level timeout expired
isc_att_stmt_timeout
Attachment level timeout expired
isc_req_stmt_timeout
Statement level timeout expired

Notes about Statement Timeouts
1. A client application could wait longer than the time set by the timeout value if
the engine needs to undo a large number of actions as a result of the statement
cancellation
2. When the engine runs an EXECUTE STATEMENT statement, it passes the remainder
of the currently active timeout to the new statement. If the external (remote)
engine does not support statement timeouts, the local engine silently ignores
any corresponding error.
3. When the engine acquires a lock from the lock manager, it tries to lower the
value of the lock timeout using the remainder of the currently active statement
timeout, if possible. Due to lock manager internals, any statement timeout
remainder will be rounded up to whole seconds.
15.5. Time Zone Management
Statements for management of time zone features of the current connections.
15.5.1. SET TIME ZONE
Sets the session time zone
Chapter 15. Management Statements
637

<!-- Original PDF Page: 639 -->

Syntax
SET TIME ZONE { time_zone_string | LOCAL }
Changes the session time zone to the specified time zone. Specifying LOCAL will revert to initial
session time zone of the session (either the default or as specified through connection property
isc_dpb_session_time_zone).
Executing ALTER SESSION RESET has the same effect on the session time zone as SET TIME ZONE LOCAL,
but will also reset other session properties.
SET TIME ZONE Examples
set time zone '-02:00';
set time zone 'America/Sao_Paulo';
set time zone local;
15.6. Optimizer Configuration
15.6.1. SET OPTIMIZE
Configures whether the optimizer should optimize for fetching first or all rows.
Syntax
SET OPTIMIZE <optimize-mode>
<optimize-mode> ::=
    FOR {FIRST | ALL} ROWS
  | TO DEFAULT
This feature allows the optimizer to consider another (hopefully better) plan if only a subset or
rows is fetched initially by the user application (with the remaining rows being fetched on
demand), thus improving the response time.
It can also be specified at the statement level using the OPTIMIZE FOR clause.
The default behaviour can be specified globally using the OptimizeForFirstRows setting in
firebird.conf or databases.conf.
15.7. Reset Session State
15.7.1. ALTER SESSION RESET
Resets the session state of the current connection to its initial values
Chapter 15. Management Statements
638

<!-- Original PDF Page: 640 -->

Syntax
ALTER SESSION RESET
Resetting the session can be useful for reusing the connection by a client application (for example,
by a client-side connection pool). When this statement is executed, all user context variables are
cleared, contents of global temporary tables are cleared, and all session-level settings are reset to
their initial values.
It is possible to execute ALTER SESSION RESET without a transaction.
Execution of ALTER SESSION RESET performs the following steps:
• Error isc_ses_reset_err ( 335545206) is raised if any transaction is active in the current session
other than the current transaction (the one executing ALTER SESSION RESET ) and two-phase
transactions in the prepared state.
• System variable RESETTING is set to TRUE.
• ON DISCONNECT database triggers are fired, if present and if database triggers are not disabled for
the current connection.
• The current transaction (the one executing ALTER SESSION RESET ), if present, is rolled back. A
warning is reported if this transaction modified data before resetting the session.
• Session configuration is reset to their initial values. This includes, but is not limited to:
◦ DECFLOAT parameters (TRAP and ROUND) are reset to the initial values defined using the DPB at
connect time, or otherwise the system default.
◦ Session and statement timeouts are reset to zero.
◦ The current role is restored to the initial value defined using DPB at connect time, and — if
the role changed — the security classes cache is cleared.
◦ The session time zone is reset to the initial value defined using the DPB at connect time, or
otherwise the system default.
◦ The bind configuration is reset to the initial value defined using the DPB at connect time, or
otherwise the database or system default.
◦ In general, configuration values should revert to the values configured using the DPB at
connect time, or otherwise the database or system default.
• Context variables defined for the USER_SESSION namespace are removed ( USER_TRANSACTION was
cleared earlier by the transaction roll back).
• Global temporary tables defined as ON COMMIT PRESERVE ROWS  are truncated (their contents is
cleared).
• ON CONNECT database triggers are fired, if present and if database triggers are not disabled for the
current connection.
• A new transaction is implicitly started with the same parameters as the transaction that was
rolled back (if there was a transaction)
• System variable RESETTING is set to FALSE.
Chapter 15. Management Statements
639