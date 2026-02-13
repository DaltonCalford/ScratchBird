# Firebird 5 SQL + PSQL — Standalone
## 1. Scope
This document defines the full SQL grammar and PSQL behavior for Firebird 5.0, including DDL/DML/PSQL, dialect-specific rules, built-in functions, and error behavior. It is fully self-contained and does not rely on external source trees.

## 2. Primary Grammar Artifacts (Internal)
- `10_sql_grammar_full.y` — complete Bison/Yacc grammar (authoritative).
- `formal/sql_grammar.json` — machine‑readable grammar graph (authoritative).
- `formal/sql_tokens.json` and `formal/sql_nonterminals.json` — token and nonterminal inventories.

Implementations MUST follow these artifacts exactly.
## 3. Token Set (Complete)
The following `%token` declarations are taken directly from `parse.y`. These tokens define the full lexical vocabulary recognized by the parser.
```
%token <metaNamePtr> ACTIVE
%token <metaNamePtr> ADD
%token <metaNamePtr> AFTER
%token <metaNamePtr> ALL
%token <metaNamePtr> ALTER
%token <metaNamePtr> AND
%token <metaNamePtr> ANY
%token <metaNamePtr> AS
%token <metaNamePtr> ASC
%token <metaNamePtr> AT
%token <metaNamePtr> AVG
%token <metaNamePtr> AUTO
%token <metaNamePtr> BEFORE
%token <metaNamePtr> BEGIN
%token <metaNamePtr> BETWEEN
%token <metaNamePtr> BLOB
%token <metaNamePtr> BY
%token <metaNamePtr> CAST
%token <metaNamePtr> CHARACTER
%token <metaNamePtr> CHECK
%token <metaNamePtr> COLLATE
%token <metaNamePtr> COMMIT
%token <metaNamePtr> COMMITTED
%token <metaNamePtr> COMPUTED
%token <metaNamePtr> CONCATENATE
%token <metaNamePtr> CONDITIONAL
%token <metaNamePtr> CONSTRAINT
%token <metaNamePtr> CONTAINING
%token <metaNamePtr> COUNT
%token <metaNamePtr> CREATE
%token <metaNamePtr> CSTRING
%token <metaNamePtr> CURRENT
%token <metaNamePtr> CURSOR
%token <metaNamePtr> DATABASE
%token <metaNamePtr> DATE
%token <metaNamePtr> DB_KEY
%token <metaNamePtr> DECIMAL
%token <metaNamePtr> DECLARE
%token <metaNamePtr> DEFAULT
%token <metaNamePtr> DELETE
%token <metaNamePtr> DESC
%token <metaNamePtr> DISTINCT
%token <metaNamePtr> DO
%token <metaNamePtr> DOMAIN
%token <metaNamePtr> DROP
%token <metaNamePtr> ELSE
%token <metaNamePtr> END
%token <metaNamePtr> ENTRY_POINT
%token <metaNamePtr> ESCAPE
%token <metaNamePtr> EXCEPTION
%token <metaNamePtr> EXECUTE
%token <metaNamePtr> EXISTS
%token <metaNamePtr> EXIT
%token <metaNamePtr> EXTERNAL
%token <metaNamePtr> FILTER
%token <metaNamePtr> FOR
%token <metaNamePtr> FOREIGN
%token <metaNamePtr> FROM
%token <metaNamePtr> FULL
%token <metaNamePtr> FUNCTION
%token <metaNamePtr> GDSCODE
%token <metaNamePtr> GEQ
%token <metaNamePtr> GENERATOR
%token <metaNamePtr> GEN_ID
%token <metaNamePtr> GRANT
%token <metaNamePtr> GROUP
%token <metaNamePtr> HAVING
%token <metaNamePtr> IF
%token <metaNamePtr> IN
%token <metaNamePtr> INACTIVE
%token <metaNamePtr> INNER
%token <metaNamePtr> INPUT_TYPE
%token <metaNamePtr> INDEX
%token <metaNamePtr> INSERT
%token <metaNamePtr> INTEGER
%token <metaNamePtr> INTO
%token <metaNamePtr> IS
%token <metaNamePtr> ISOLATION
%token <metaNamePtr> JOIN
%token <metaNamePtr> KEY
%token <metaNamePtr> CHAR
%token <metaNamePtr> DEC
%token <metaNamePtr> DOUBLE
%token <metaNamePtr> FILE
%token <metaNamePtr> FLOAT
%token <metaNamePtr> INT
%token <metaNamePtr> LONG
%token <metaNamePtr> NULL
%token <metaNamePtr> NUMERIC
%token <metaNamePtr> UPPER
%token <metaNamePtr> VALUE
%token <metaNamePtr> LENGTH
%token <metaNamePtr> LEFT
%token <metaNamePtr> LEQ
%token <metaNamePtr> LEVEL
%token <metaNamePtr> LIKE
%token <metaNamePtr> MANUAL
%token <metaNamePtr> MAXIMUM
%token <metaNamePtr> MERGE
%token <metaNamePtr> MINIMUM
%token <metaNamePtr> MODULE_NAME
%token <metaNamePtr> NAMES
%token <metaNamePtr> NATIONAL
%token <metaNamePtr> NATURAL
%token <metaNamePtr> NCHAR
%token <metaNamePtr> NEQ
%token <metaNamePtr> NO
%token <metaNamePtr> NOT
%token <metaNamePtr> NOT_GTR
%token <metaNamePtr> NOT_LSS
%token <metaNamePtr> OF
%token <metaNamePtr> ON
%token <metaNamePtr> ONLY
%token <metaNamePtr> OPTION
%token <metaNamePtr> OR
%token <metaNamePtr> ORDER
%token <metaNamePtr> OUTER
%token <metaNamePtr> OUTPUT_TYPE
%token <metaNamePtr> OVERFLOW
%token <metaNamePtr> PAGE
%token <metaNamePtr> PAGES
%token <metaNamePtr> PAGE_SIZE
%token <metaNamePtr> PARAMETER
%token <metaNamePtr> PASSWORD
%token <metaNamePtr> PLAN
%token <metaNamePtr> POSITION
%token <metaNamePtr> POST_EVENT
%token <metaNamePtr> PRECISION
%token <metaNamePtr> PRIMARY
%token <metaNamePtr> PRIVILEGES
%token <metaNamePtr> PROCEDURE
%token <metaNamePtr> PROTECTED
%token <metaNamePtr> READ
%token <metaNamePtr> REAL
%token <metaNamePtr> REFERENCES
%token <metaNamePtr> RESERVING
%token <metaNamePtr> RETAIN
%token <metaNamePtr> RETURNING_VALUES
%token <metaNamePtr> RETURNS
%token <metaNamePtr> REVOKE
%token <metaNamePtr> RIGHT
%token <metaNamePtr> ROLLBACK
%token <metaNamePtr> SEGMENT
%token <metaNamePtr> SELECT
%token <metaNamePtr> SET
%token <metaNamePtr> SHADOW
%token <metaNamePtr> SHARED
%token <metaNamePtr> SINGULAR
%token <metaNamePtr> SIZE
%token <metaNamePtr> SMALLINT
%token <metaNamePtr> SNAPSHOT
%token <metaNamePtr> SOME
%token <metaNamePtr> SORT
%token <metaNamePtr> SQLCODE
%token <metaNamePtr> STABILITY
%token <metaNamePtr> STARTING
%token <metaNamePtr> STATISTICS
%token <metaNamePtr> SUB_TYPE
%token <metaNamePtr> SUSPEND
%token <metaNamePtr> SUM
%token <metaNamePtr> TABLE
%token <metaNamePtr> THEN
%token <metaNamePtr> TO
%token <metaNamePtr> TRANSACTION
%token <metaNamePtr> TRIGGER
%token <metaNamePtr> UNCOMMITTED
%token <metaNamePtr> UNION
%token <metaNamePtr> UNIQUE
%token <metaNamePtr> UPDATE
%token <metaNamePtr> USER
%token <metaNamePtr> VALUES
%token <metaNamePtr> VARCHAR
%token <metaNamePtr> VARIABLE
%token <metaNamePtr> VARYING
%token <metaNamePtr> VERSION
%token <metaNamePtr> VIEW
%token <metaNamePtr> WAIT
%token <metaNamePtr> WHEN
%token <metaNamePtr> WHERE
%token <metaNamePtr> WHILE
%token <metaNamePtr> WITH
%token <metaNamePtr> WORK
%token <metaNamePtr> WRITE
%token <stringPtr> FLOAT_NUMBER DECIMAL_NUMBER
%token <lim64ptr> LIMIT64_NUMBER LIMIT64_INT NUM128
%token <metaNamePtr> SYMBOL
%token <int32Val> NUMBER32BIT
%token <intlStringPtr> STRING
%token <metaNamePtr> INTRODUCER
%token <metaNamePtr> ACTION
%token <metaNamePtr> ADMIN
%token <metaNamePtr> BLOBID
%token <metaNamePtr> CASCADE
%token <metaNamePtr> FREE_IT			// ISC SQL extension
%token <metaNamePtr> RESTRICT
%token <metaNamePtr> ROLE
%token <metaNamePtr> TEMP
%token <metaNamePtr> COLUMN
%token <metaNamePtr> TYPE
%token <metaNamePtr> EXTRACT
%token <metaNamePtr> YEAR
%token <metaNamePtr> MONTH
%token <metaNamePtr> DAY
%token <metaNamePtr> HOUR
%token <metaNamePtr> MINUTE
%token <metaNamePtr> SECOND
%token <metaNamePtr> WEEKDAY			// ISC SQL extension
%token <metaNamePtr> YEARDAY			// ISC SQL extension
%token <metaNamePtr> TIME
%token <metaNamePtr> TIMESTAMP
%token <metaNamePtr> CURRENT_DATE
%token <metaNamePtr> CURRENT_TIME
%token <metaNamePtr> CURRENT_TIMESTAMP
%token <scaledNumber> NUMBER64BIT SCALEDINT
%token <metaNamePtr> CURRENT_USER
%token <metaNamePtr> CURRENT_ROLE
%token <metaNamePtr> BREAK
%token <metaNamePtr> SUBSTRING
%token <metaNamePtr> RECREATE
%token <metaNamePtr> DESCRIPTOR
%token <metaNamePtr> FIRST
%token <metaNamePtr> SKIP
%token <metaNamePtr> CURRENT_CONNECTION
%token <metaNamePtr> CURRENT_TRANSACTION
%token <metaNamePtr> BIGINT
%token <metaNamePtr> CASE
%token <metaNamePtr> NULLIF
%token <metaNamePtr> COALESCE
%token <metaNamePtr> USING
%token <metaNamePtr> NULLS
%token <metaNamePtr> LAST
%token <metaNamePtr> ROW_COUNT
%token <metaNamePtr> LOCK
%token <metaNamePtr> SAVEPOINT
%token <metaNamePtr> RELEASE
%token <metaNamePtr> STATEMENT
%token <metaNamePtr> LEAVE
%token <metaNamePtr> INSERTING
%token <metaNamePtr> UPDATING
%token <metaNamePtr> DELETING
%token <metaNamePtr> BACKUP
%token <metaNamePtr> DIFFERENCE
%token <metaNamePtr> OPEN
%token <metaNamePtr> CLOSE
%token <metaNamePtr> FETCH
%token <metaNamePtr> ROWS
%token <metaNamePtr> BLOCK
%token <metaNamePtr> IIF
%token <metaNamePtr> SCALAR_ARRAY
%token <metaNamePtr> CROSS
%token <metaNamePtr> NEXT
%token <metaNamePtr> SEQUENCE
%token <metaNamePtr> RESTART
%token <metaNamePtr> BOTH
%token <metaNamePtr> COLLATION
%token <metaNamePtr> COMMENT
%token <metaNamePtr> BIT_LENGTH
%token <metaNamePtr> CHAR_LENGTH
%token <metaNamePtr> CHARACTER_LENGTH
%token <metaNamePtr> LEADING
%token <metaNamePtr> LOWER
%token <metaNamePtr> OCTET_LENGTH
%token <metaNamePtr> TRAILING
%token <metaNamePtr> TRIM
%token <metaNamePtr> RETURNING
%token <metaNamePtr> IGNORE
%token <metaNamePtr> LIMBO
%token <metaNamePtr> UNDO
%token <metaNamePtr> REQUESTS
%token <metaNamePtr> TIMEOUT
%token <metaNamePtr> ABS
%token <metaNamePtr> ACCENT
%token <metaNamePtr> ACOS
%token <metaNamePtr> ALWAYS
%token <metaNamePtr> ASCII_CHAR
%token <metaNamePtr> ASCII_VAL
%token <metaNamePtr> ASIN
%token <metaNamePtr> ATAN
%token <metaNamePtr> ATAN2
%token <metaNamePtr> BIN_AND
%token <metaNamePtr> BIN_OR
%token <metaNamePtr> BIN_SHL
%token <metaNamePtr> BIN_SHR
%token <metaNamePtr> BIN_XOR
%token <metaNamePtr> CEIL
%token <metaNamePtr> CONNECT
%token <metaNamePtr> COS
%token <metaNamePtr> COSH
%token <metaNamePtr> COT
%token <metaNamePtr> DATEADD
%token <metaNamePtr> DATEDIFF
%token <metaNamePtr> DECODE
%token <metaNamePtr> DISCONNECT
%token <metaNamePtr> EXP
%token <metaNamePtr> FLOOR
%token <metaNamePtr> GEN_UUID
%token <metaNamePtr> GENERATED
%token <metaNamePtr> GLOBAL
%token <metaNamePtr> HASH
%token <metaNamePtr> INSENSITIVE
%token <metaNamePtr> LIST
%token <metaNamePtr> LN
%token <metaNamePtr> LOG
%token <metaNamePtr> LOG10
%token <metaNamePtr> LPAD
%token <metaNamePtr> MATCHED
%token <metaNamePtr> MATCHING
%token <metaNamePtr> MAXVALUE
%token <metaNamePtr> MILLISECOND
%token <metaNamePtr> MINVALUE
%token <metaNamePtr> MOD
%token <metaNamePtr> OVERLAY
%token <metaNamePtr> PAD
%token <metaNamePtr> PI
%token <metaNamePtr> PLACING
%token <metaNamePtr> POWER
%token <metaNamePtr> PRESERVE
%token <metaNamePtr> RAND
%token <metaNamePtr> RECURSIVE
%token <metaNamePtr> REPLACE
%token <metaNamePtr> REVERSE
%token <metaNamePtr> ROUND
%token <metaNamePtr> RPAD
%token <metaNamePtr> SENSITIVE
%token <metaNamePtr> SIGN
%token <metaNamePtr> SIN
%token <metaNamePtr> SINH
%token <metaNamePtr> SPACE
%token <metaNamePtr> SQRT
%token <metaNamePtr> START
%token <metaNamePtr> TAN
%token <metaNamePtr> TANH
%token <metaNamePtr> TEMPORARY
%token <metaNamePtr> TRUNC
%token <metaNamePtr> WEEK
%token <metaNamePtr> AUTONOMOUS
%token <metaNamePtr> CHAR_TO_UUID
%token <metaNamePtr> FIRSTNAME
%token <metaNamePtr> GRANTED
%token <metaNamePtr> LASTNAME
%token <metaNamePtr> MIDDLENAME
%token <metaNamePtr> MAPPING
%token <metaNamePtr> OS_NAME
%token <metaNamePtr> SIMILAR
%token <metaNamePtr> UUID_TO_CHAR
%token <metaNamePtr> CALLER
%token <metaNamePtr> COMMON
%token <metaNamePtr> DATA
%token <metaNamePtr> SOURCE
%token <metaNamePtr> TWO_PHASE
%token <metaNamePtr> BIND_PARAM
%token <metaNamePtr> BIN_NOT
%token <metaNamePtr> BODY
%token <metaNamePtr> CONTINUE
%token <metaNamePtr> DDL
%token <metaNamePtr> DECRYPT
%token <metaNamePtr> ENCRYPT
%token <metaNamePtr> ENGINE
%token <metaNamePtr> NAME
%token <metaNamePtr> OVER
%token <metaNamePtr> PACKAGE
%token <metaNamePtr> PARTITION
%token <metaNamePtr> RDB_GET_CONTEXT
%token <metaNamePtr> RDB_SET_CONTEXT
%token <metaNamePtr> SCROLL
%token <metaNamePtr> PRIOR
%token <metaNamePtr> ABSOLUTE
%token <metaNamePtr> RELATIVE
%token <metaNamePtr> ACOSH
%token <metaNamePtr> ASINH
%token <metaNamePtr> ATANH
%token <metaNamePtr> RETURN
%token <metaNamePtr> DETERMINISTIC
%token <metaNamePtr> IDENTITY
%token <metaNamePtr> DENSE_RANK
%token <metaNamePtr> FIRST_VALUE
%token <metaNamePtr> NTH_VALUE
%token <metaNamePtr> LAST_VALUE
%token <metaNamePtr> LAG
%token <metaNamePtr> LEAD
%token <metaNamePtr> RANK
%token <metaNamePtr> ROW_NUMBER
%token <metaNamePtr> SQLSTATE
%token <metaNamePtr> BOOLEAN
%token <metaNamePtr> FALSE
%token <metaNamePtr> TRUE
%token <metaNamePtr> UNKNOWN
%token <metaNamePtr> USAGE
%token <metaNamePtr> RDB_RECORD_VERSION
%token <metaNamePtr> LINGER
%token <metaNamePtr> TAGS
%token <metaNamePtr> PLUGIN
%token <metaNamePtr> SERVERWIDE
%token <metaNamePtr> INCREMENT
%token <metaNamePtr> TRUSTED
%token <metaNamePtr> ROW
%token <metaNamePtr> OFFSET
%token <metaNamePtr> STDDEV_SAMP
%token <metaNamePtr> STDDEV_POP
%token <metaNamePtr> VAR_SAMP
%token <metaNamePtr> VAR_POP
%token <metaNamePtr> COVAR_SAMP
%token <metaNamePtr> COVAR_POP
%token <metaNamePtr> CORR
%token <metaNamePtr> REGR_AVGX
%token <metaNamePtr> REGR_AVGY
%token <metaNamePtr> REGR_COUNT
%token <metaNamePtr> REGR_INTERCEPT
%token <metaNamePtr> REGR_R2
%token <metaNamePtr> REGR_SLOPE
%token <metaNamePtr> REGR_SXX
%token <metaNamePtr> REGR_SXY
%token <metaNamePtr> REGR_SYY
%token <metaNamePtr> BASE64_DECODE
%token <metaNamePtr> BASE64_ENCODE
%token <metaNamePtr> BINARY
%token <metaNamePtr> BIND
%token <metaNamePtr> COMPARE_DECFLOAT
%token <metaNamePtr> CONSISTENCY
%token <metaNamePtr> COUNTER
%token <metaNamePtr> CRYPT_HASH
%token <metaNamePtr> CTR_BIG_ENDIAN
%token <metaNamePtr> CTR_LENGTH
%token <metaNamePtr> CTR_LITTLE_ENDIAN
%token <metaNamePtr> CUME_DIST
%token <metaNamePtr> DECFLOAT
%token <metaNamePtr> DEFINER
%token <metaNamePtr> DISABLE
%token <metaNamePtr> ENABLE
%token <metaNamePtr> EXCESS
%token <metaNamePtr> EXCLUDE
%token <metaNamePtr> EXTENDED
%token <metaNamePtr> FIRST_DAY
%token <metaNamePtr> FOLLOWING
%token <metaNamePtr> HEX_DECODE
%token <metaNamePtr> HEX_ENCODE
%token <metaNamePtr> IDLE
%token <metaNamePtr> INCLUDE
%token <metaNamePtr> INT128
%token <metaNamePtr> INVOKER
%token <metaNamePtr> IV
%token <metaNamePtr> LAST_DAY
%token <metaNamePtr> LATERAL
%token <metaNamePtr> LEGACY
%token <metaNamePtr> LOCAL
%token <metaNamePtr> LOCALTIME
%token <metaNamePtr> LOCALTIMESTAMP
%token <metaNamePtr> LPARAM
%token <metaNamePtr> MAKE_DBKEY
%token <metaNamePtr> MESSAGE
%token <metaNamePtr> MODE
%token <metaNamePtr> NATIVE
%token <metaNamePtr> NORMALIZE_DECFLOAT
%token <metaNamePtr> NTILE
%token <metaNamePtr> NUMBER
%token <metaNamePtr> OTHERS
%token <metaNamePtr> OVERRIDING
%token <metaNamePtr> PERCENT_RANK
%token <metaNamePtr> PRECEDING
%token <metaNamePtr> PRIVILEGE
%token <metaNamePtr> PUBLICATION
%token <metaNamePtr> QUANTIZE
%token <metaNamePtr> RANGE
%token <metaNamePtr> RESETTING
%token <metaNamePtr> RDB_ERROR
%token <metaNamePtr> RDB_GET_TRANSACTION_CN
%token <metaNamePtr> RDB_ROLE_IN_USE
%token <metaNamePtr> RDB_SYSTEM_PRIVILEGE
%token <metaNamePtr> RESET
%token <metaNamePtr> RSA_DECRYPT
%token <metaNamePtr> RSA_ENCRYPT
%token <metaNamePtr> RSA_PRIVATE
%token <metaNamePtr> RSA_PUBLIC
%token <metaNamePtr> RSA_SIGN_HASH
%token <metaNamePtr> RSA_VERIFY_HASH
%token <metaNamePtr> SALT_LENGTH
%token <metaNamePtr> SECURITY
%token <metaNamePtr> SESSION
%token <metaNamePtr> SIGNATURE
%token <metaNamePtr> SQL
%token <metaNamePtr> SYSTEM
%token <metaNamePtr> TIES
%token <metaNamePtr> TIMEZONE_HOUR
%token <metaNamePtr> TIMEZONE_MINUTE
%token <metaNamePtr> TOTALORDER
%token <metaNamePtr> TRAPS
%token <metaNamePtr> UNBOUNDED
%token <metaNamePtr> VARBINARY
%token <metaNamePtr> WINDOW
%token <metaNamePtr> WITHOUT
%token <metaNamePtr> ZONE
%token <metaNamePtr> CONNECTIONS
%token <metaNamePtr> POOL
%token <metaNamePtr> LIFETIME
%token <metaNamePtr> CLEAR
%token <metaNamePtr> OLDEST
%token <metaNamePtr> DEBUG
%token <metaNamePtr> PKCS_1_5
%token <metaNamePtr> BLOB_APPEND
%token <metaNamePtr> LOCKED
%token <metaNamePtr> OPTIMIZE
%token <metaNamePtr> QUARTER
%token <metaNamePtr> TARGET
%token <metaNamePtr> TIMEZONE_NAME
%token <metaNamePtr> UNICODE_CHAR
%token <metaNamePtr> UNICODE_VAL
%token <metaNamePtr> OWNER
%token <metaNamePtr> ANY_VALUE
%token <metaNamePtr> BIN_AND_AGG
%token <metaNamePtr> BIN_OR_AGG
%token <metaNamePtr> BIN_XOR_AGG
%token <metaNamePtr> BTRIM
%token <metaNamePtr> CALL
%token <metaNamePtr> CURRENT_SCHEMA
%token <metaNamePtr> DOWNTO
%token <metaNamePtr> ERROR
%token <metaNamePtr> FORMAT
%token <metaNamePtr> GENERATE_SERIES
%token <metaNamePtr> GREATEST
%token <metaNamePtr> LEAST
%token <metaNamePtr> LISTAGG
%token <metaNamePtr> LTRIM
%token <metaNamePtr> NAMED_ARG_ASSIGN
%token <metaNamePtr> RTRIM
%token <metaNamePtr> SCHEMA
%token <metaNamePtr> SEARCH_PATH
%token <metaNamePtr> TRUNCATE
%token <metaNamePtr> UNLIST
%token <metaNamePtr> WITHIN
```
## 4. Full Grammar (Authoritative)
The complete unabridged grammar is provided in `10_sql_grammar_full.y`. Implementations must follow it exactly.
## 5. Feature References (SQL Extensions)
Each of the following documents defines Firebird-specific SQL or PSQL behavior and edge cases: 
```
README.PSQL_stack_trace.txt
README.aggregate_filter.md
README.aggregate_functions.md
README.aggregate_tracking
README.alternate_string_quoting.txt
README.autonomous_transactions.txt
README.blob_append.md
README.blob_util.md
README.builtin_functions.txt
README.call.md
README.case
README.cast.format.md
README.coalesce
README.column_type_psql.txt
README.common_table_expressions
README.context_variables
README.context_variables2
README.cumulative_roles.txt
README.current_time
README.cursor_variables.txt
README.cursors
README.data_type_results_of_aggregations.txt
README.data_types
README.db_triggers.txt
README.ddl.txt
README.ddl_access.txt
README.ddl_triggers.txt
README.declare_var_initializer.md
README.default_parameters
README.derived_tables.txt
README.distinct
README.domains_psql.txt
README.exception_handling
README.execute_block
README.execute_statement
README.execute_statement2
README.explicit_locks
README.expression_indices
README.external_connections_pool
README.floating_point_types.md
README.generate_series.md
README.global_temporary_tables
README.hex_literals.txt
README.identity_columns.txt
README.iif
README.joins.txt
README.keywords
README.leave_labels
README.length
README.linger
README.list
README.listagg
README.local_temporary_tables.md
README.management_statements_psql.md
README.mapping.html
README.merge.txt
README.name_resolution.md
README.named_arguments.md
README.null_value
README.nullif
README.offset_fetch.txt
README.order_by_expressions_nulls
README.packages.txt
README.partial_indices
README.plan
README.profiler.md
README.range_based_for.md
README.regr_functions.txt
README.returning
README.rows
README.savepoints
README.schemas.md
README.scrollable_cursors.txt
README.select_expressions
README.sequence_generators
README.set_bind.md
README.set_role
README.set_transaction.txt
README.similar_to.txt
README.skip_locked.md
README.sql_package.md
README.sql_security.txt
README.statistical_functions.txt
README.subroutines.txt
README.substring_similar.txt
README.time_zone.md
README.trim
README.universal_triggers
README.unlist
README.update_or_insert
README.user_management
README.view_updates
README.window_functions.md
```
## 6. Dialects
- Firebird supports SQL Dialects 1 and 3. Dialect affects numeric interpretation and certain syntax checks. Dialect is negotiated per-attachment and per-statement (`p_sqlst_SQL_dialect`).
## 7. Lexical Structure
- Identifier rules, quoted identifiers, string literals, character set introducers, and numeric literal formats are defined in this document and by the token inventories in `formal/sql_tokens.json`.
## 8. Statement Families
All DDL/DML/PSQL statements are defined in the grammar file. The implementation must honor grammar ambiguity resolutions and precedence rules embedded in `parse.y`.
## 9. Error Behavior
Errors are raised as `isc_*` status codes with SQLSTATE where applicable. The full error mapping is defined in `40_response_formats_and_types.md`.

## 10. SQL Extensions Source Pack
All Firebird SQL extension documents are copied verbatim to:
These files are authoritative for feature behavior and edge cases.
