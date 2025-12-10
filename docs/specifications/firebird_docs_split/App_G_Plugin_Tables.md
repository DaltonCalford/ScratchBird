# App G Plugin Tables



<!-- Original PDF Page: 775 -->

authentication plugin.
SEC$USER_ATTRIBUTES
Additional attributes of users
Column Name Data Type Description
SEC$USER_NAME CHAR(63) Username
SEC$KEY VARCHAR(63) Attribute name
SEC$VALUE VARCHAR(255) Attribute value
SEC$PLUGIN CHAR(63) Authentication plugin name that
manages this user
Displaying a list of users and their attributes
SELECT
  U.SEC$USER_NAME AS LOGIN,
  A.SEC$KEY AS TAG,
  A.SEC$VALUE AS "VALUE",
  U.SEC$PLUGIN AS "PLUGIN"
FROM SEC$USERS U
LEFT JOIN SEC$USER_ATTRIBUTES A
  ON U.SEC$USER_NAME = A.SEC$USER_NAME
    AND U.SEC$PLUGIN = A.SEC$PLUGIN;
LOGIN    TAG     VALUE   PLUGIN
======== ======= ======= ===================
SYSDBA   <null>  <null>  Srp
ALEX     B       x       Srp
ALEX     C       sample  Srp
SYSDBA   <null>  <null>  Legacy_UserManager
Appendix F: Security tables
774

<!-- Original PDF Page: 776 -->

Appendix G: Plugin tables
Plugin tables are tables — or views — created for or by various plugins to the Firebird engine. The
standard plugin tables have the prefix PLG$.

The plugin tables do not always exist. For example, some tables only exist in the
security database, and other tables will only be created on first use of a plugin.
This appendix only documents plugin tables which are created by plugins included
in a standard Firebird 5.0 deployment.
Plugin tables are not considered system tables.

Profiler table names are plugin-specific
The tables listed in this appendix for the profiler (starting with PLG$PROF_) are
created by the Default_Profiler plugin. If a custom profiler plugin is created, it
may use different table names.
List of plugin tables
PLG$PROF_CURSORS
Profiler information on cursors
PLG$PROF_PSQL_STATS
Profiler PSQL statistics
PLG$PROF_PSQL_STATS_VIEW
Profiler aggregated view for PSQL statistics
PLG$PROF_RECORD_SOURCES
Profiler information on record sources
PLG$PROF_RECORD_SOURCE_STATS
Profiler record source statistics
PLG$PROF_RECORD_SOURCE_STATS_VIEW
Profiler aggregated view for record source statistics
PLG$PROF_REQUESTS
Profiler information on requests
PLG$PROF_SESSIONS
Profiler sessions
PLG$PROF_STATEMENTS
Profiler information on statements
Appendix G: Plugin tables
775

<!-- Original PDF Page: 777 -->

PLG$PROF_STATEMENT_STATS_VIEW
Profiler aggregated view for statement statistics
PLG$SRP
Users and authentication information of the Srp user manager
PLG$USERS
User and authentication information of the Legacy_UserManager user manager
PLG$PROF_CURSORS
Profiler information on cursors.
Column Name Data Type Description
PROFILE_ID BIGINT Profile session id
STATEMENT_ID BIGINT Statement id
CURSOR_ID INTEGER Cursor id
NAME CHAR(63) Name of explicit cursor
LINE_NUM INTEGER PSQL line number of the cursor
COLUMN_NUM INTEGER PSQL column number of the cursor
PLG$PROF_PSQL_STATS
Profiler PSQL statistics.
Column Name Data Type Description
PROFILE_ID BIGINT Profile session id
STATEMENT_ID BIGINT Statement id
REQUEST_ID BIGINT Request id
LINE_NUM INTEGER PSQL line number of the statement
COLUMN_NUM INTEGER PSQL column number of the statement
COUNTER BIGINT Number of executed times of the
line/column
MIN_ELAPSED_TIME BIGINT Minimal elapsed time (in nanoseconds)
of a line/column execution
MAX_ELAPSED_TIME BIGINT Maximum elapsed time (in
nanoseconds) of a line/column
execution
TOTAL_ELAPSED_TIME BIGINT Accumulated elapsed time (in
nanoseconds) of the line/column
executions
Appendix G: Plugin tables
776

<!-- Original PDF Page: 778 -->

PLG$PROF_PSQL_STATS_VIEW
Profiler aggregated view for PSQL statistics.
Column Name Data Type Description
PROFILE_ID BIGINT Profile session id
STATEMENT_ID BIGINT Statement id
STATEMENT_TYPE VARCHAR(20) Statement type: BLOCK, FUNCTION,
PROCEDURE or TRIGGER
PACKAGE_NAME CHAR(63) Package name
ROUTINE_NAME CHAR(63) Routine name
PARENT_STATEMENT_ID BIGINT Parent statement id
PARENT_STATEMENT_TYPE VARCHAR(20) Statement type: BLOCK, FUNCTION,
PROCEDURE or TRIGGER
PARENT_ROUTINE_NAME CHAR(63) Parent routine name
SQL_TEXT BLOB TEXT SQL text (if statement type is BLOCK)
LINE_NUM INTEGER PSQL line number of the statement
COLUMN_NUM INTEGER PSQL column number of the statement
COUNTER BIGINT Number of executed times of the
line/column
MIN_ELAPSED_TIME BIGINT Minimal elapsed time (in nanoseconds)
of a line/column execution
MAX_ELAPSED_TIME BIGINT Maximum elapsed time (in
nanoseconds) of a line/column
execution
TOTAL_ELAPSED_TIME BIGINT Accumulated elapsed time (in
nanoseconds) of the line/column
executions
AVG_ELAPSED_TIME BIGINT Average elapsed time (in nanoseconds)
of the line/column executions
PLG$PROF_RECORD_SOURCES
Profiler information on record sources.
Column Name Data Type Description
PROFILE_ID BIGINT Profile session id
STATEMENT_ID BIGINT Statement id
CURSOR_ID INTEGER Cursor id
RECORD_SOURCE_ID INTEGER Record source id
Appendix G: Plugin tables
777

<!-- Original PDF Page: 779 -->

Column Name Data Type Description
PARENT_RECORD_SOURCE_ID INTEGER Parent record source id
LEVEL INTEGER Indentation level for the record source
ACCESS_PATH VARCHAR(255) Access path of the record source
PLG$PROF_RECORD_SOURCE_STATS
Profiler record sources statistics.
Column Name Data Type Description
PROFILE_ID BIGINT Profile session id
STATEMENT_ID BIGINT Statement id
REQUEST_ID BIGINT Request id
CURSOR_ID INTEGER Cursor id
RECORD_SOURCE_ID INTEGER Record source id
OPEN_COUNTER BIGINT Number of times the record source was
opened
OPEN_MIN_ELAPSED_TIME BIGINT Minimal elapsed time (in nanoseconds)
of a record source open
OPEN_MAX_ELAPSED_TIME BIGINT Maximum elapsed time (in
nanoseconds) of a record source open
OPEN_TOTAL_ELAPSED_TIME BIGINT Accumulated elapsed time (in
nanoseconds) of record source opens
FETCH_COUNTER BIGINT Number of fetches from the record
source
FETCH_MIN_ELAPSED_TIME BIGINT Minimal elapsed time (in nanoseconds)
of a record source fetch
FETCH_MAX_ELAPSED_TIME BIGINT Maximum elapsed time (in
nanoseconds) of a record source fetch
FETCH_TOTAL_ELAPSED_TIME BIGINT Accumulated elapsed time (in
nanoseconds) of record source fetches
PLG$PROF_RECORD_SOURCE_STATS_VIEW
Profiler aggregated view for record source statistics.
Column Name Data Type Description
PROFILE_ID BIGINT Profile session id
STATEMENT_ID BIGINT Statement id
Appendix G: Plugin tables
778

<!-- Original PDF Page: 780 -->

Column Name Data Type Description
STATEMENT_TYPE VARCHAR(20) Statement type: BLOCK, FUNCTION,
PROCEDURE or TRIGGER
PACKAGE_NAME CHAR(63) Package name
ROUTINE_NAME CHAR(63) Routine name
PARENT_STATEMENT_ID BIGINT Parent statement id
PARENT_STATEMENT_TYPE VARCHAR(20) Statement type: BLOCK, FUNCTION,
PROCEDURE or TRIGGER
PARENT_ROUTINE_NAME CHAR(63) Parent routine name
SQL_TEXT BLOB TEXT SQL text (if statement type is BLOCK)
CURSOR_ID INTEGER Cursor id
CURSOR_NAME CHAR(63) Name of explicit cursor
CURSOR_LINE_NUM INTEGER PSQL line number of the cursor
CURSOR_COLUMN_NUM INTEGER PSQL column number of the cursor
RECORD_SOURCE_ID INTEGER Record source id
PARENT_RECORD_SOURCE_ID INTEGER Parent record source id
LEVEL INTEGER Indentation level for the record source
ACCESS_PATH VARCHAR(255) Access path of the record source
OPEN_COUNTER BIGINT Number of times the record source was
opened
OPEN_MIN_ELAPSED_TIME BIGINT Minimal elapsed time (in nanoseconds)
of a record source open
OPEN_MAX_ELAPSED_TIME BIGINT Maximum elapsed time (in
nanoseconds) of a record source open
OPEN_TOTAL_ELAPSED_TIME BIGINT Accumulated elapsed time (in
nanoseconds) of record source opens
OPEN_AVG_ELAPSED_TIME BIGINT Average elapsed time (in nanoseconds)
of record source opens
FETCH_COUNTER BIGINT Number of fetches from the record
source
FETCH_MIN_ELAPSED_TIME BIGINT Minimal elapsed time (in nanoseconds)
of a record source fetch
FETCH_MAX_ELAPSED_TIME BIGINT Maximum elapsed time (in
nanoseconds) of a record source fetch
FETCH_TOTAL_ELAPSED_TIME BIGINT Accumulated elapsed time (in
nanoseconds) of record source fetches
FETCH_AVG_ELAPSED_TIME BIGINT Average elapsed time (in nanoseconds)
of record source fetches
Appendix G: Plugin tables
779

<!-- Original PDF Page: 781 -->

Column Name Data Type Description
OPEN_FETCH_TOTAL_ELAPSED_TIME BIGINT Total elapsed time (in nanoseconds) or
record source opens and fetches
PLG$PROF_REQUESTS
Profiler information on requests.
Column Name Data Type Description
PROFILE_ID BIGINT Profile session id
STATEMENT_ID BIGINT Statement id
REQUEST_ID BIGINT Request id
CALLER_STATEMENT_ID BIGINT Caller statement id
CALLER_REQUEST_ID BIGINT Caller request id
START_TIMESTAMP TIMESTAMP WITH TIME
ZONE
Instant when request started
FINISH_TIMESTAMP TIMESTAMP WITH TIME
ZONE
Instant when request finished
TOTAL_ELAPSED_TIME BIGINT Accumulated elapsed time (in
nanoseconds) of the request
PLG$PROF_SESSIONS
Profiler sessions.
Column Name Data Type Description
PROFILE_ID BIGINT Profile session id
ATTACHMENT_ID BIGINT Attachment id
USER_NAME CHAR(63) User which started the profile session
DESCRIPTION VARCHAR(255) Description of the profile session
(parameter of
RDB$PROFILER.START_SESSION)
START_TIMESTAMP TIMESTAMP WITH TIME
ZONE
Instant when session started
FINISH_TIMESTAMP TIMESTAMP WITH TIME
ZONE
Instant when session finished
PLG$PROF_STATEMENTS
Profiler information on statements.
Appendix G: Plugin tables
780

<!-- Original PDF Page: 782 -->

Column Name Data Type Description
PROFILE_ID BIGINT Profile session id
STATEMENT_ID BIGINT Statement id
PARENT_STATEMENT_ID BIGINT Parent statement id
STATEMENT_TYPE VARCHAR(20) Statement type: BLOCK, FUNCTION,
PROCEDURE or TRIGGER
PACKAGE_NAME CHAR(63) Package name
ROUTINE_NAME CHAR(63) Routine name
SQL_TEXT BLOB TEXT SQL text (if statement type is BLOCK)
PLG$PROF_STATEMENT_STATS_VIEW
Profiler aggregated view for statement statistics.
Column Name Data Type Description
PROFILE_ID BIGINT Profile session id
STATEMENT_ID BIGINT Statement id
STATEMENT_TYPE VARCHAR(20) Statement type: BLOCK, FUNCTION,
PROCEDURE or TRIGGER
PACKAGE_NAME CHAR(63) Package name
ROUTINE_NAME CHAR(63) Routine name
PARENT_STATEMENT_ID BIGINT Parent statement id
PARENT_STATEMENT_TYPE VARCHAR(20) Parent statement type: BLOCK, FUNCTION,
PROCEDURE or TRIGGER
PARENT_ROUTINE_NAME CHAR(63) Parent routine name
SQL_TEXT BLOB TEXT SQL text (if statement type is BLOCK)
COUNTER BIGINT Number of executed times of the
line/column
MIN_ELAPSED_TIME BIGINT Minimal elapsed time (in nanoseconds)
of a statement execution
MAX_ELAPSED_TIME BIGINT Maximum elapsed time (in
nanoseconds) of a statement execution
TOTAL_ELAPSED_TIME BIGINT Accumulated elapsed time (in
nanoseconds) of statement executions
AVG_ELAPSED_TIME BIGINT Average elapsed time (in nanoseconds)
of statement executions
Appendix G: Plugin tables
781