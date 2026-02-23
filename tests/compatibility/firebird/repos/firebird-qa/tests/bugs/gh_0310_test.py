#coding:utf-8

"""
ID:          310
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/310
TITLE:       System table with keywords
DESCRIPTION:
JIRA:        CORE-6482
FBTEST:      bugs.core_6482
NOTES:
    [28.03.2024] pzotov
    Added temporary mark 'disabled_in_forks' to SKIP this test when QA runs agains *fork* of standard FB.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set count on;
    select * from rdb$keywords order by rdb$keyword_name;
"""

act = isql_act('db', test_script)

expected_stdout_5 = """
    RDB$KEYWORD_NAME                ABS
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                ABSOLUTE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                ACCENT
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                ACOS
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                ACOSH
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                ACTION
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                ACTIVE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                ADD
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                ADMIN
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                AFTER
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                ALL
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                ALTER
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                ALWAYS
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                AND
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                ANY
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                AS
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                ASC
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                ASCENDING
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                ASCII_CHAR
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                ASCII_VAL
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                ASIN
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                ASINH
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                AT
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                ATAN
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                ATAN2
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                ATANH
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                AUTO
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                AUTONOMOUS
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                AVG
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                BACKUP
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                BASE64_DECODE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                BASE64_ENCODE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                BEFORE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                BEGIN
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                BETWEEN
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                BIGINT
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                BINARY
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                BIND
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                BIN_AND
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                BIN_NOT
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                BIN_OR
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                BIN_SHL
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                BIN_SHR
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                BIN_XOR
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                BIT_LENGTH
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                BLOB
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                BLOBID
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                BLOB_APPEND
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                BLOCK
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                BODY
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                BOOLEAN
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                BOTH
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                BREAK
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                BY
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                CALLER
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                CASCADE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                CASE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                CAST
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                CEIL
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                CEILING
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                CHAR
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                CHARACTER
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                CHARACTER_LENGTH
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                CHAR_LENGTH
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                CHAR_TO_UUID
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                CHECK
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                CLEAR
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                CLOSE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                COALESCE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                COLLATE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                COLLATION
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                COLUMN
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                COMMENT
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                COMMIT
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                COMMITTED
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                COMMON
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                COMPARE_DECFLOAT
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                COMPUTED
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                CONDITIONAL
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                CONNECT
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                CONNECTIONS
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                CONSISTENCY
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                CONSTRAINT
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                CONTAINING
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                CONTINUE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                CORR
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                COS
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                COSH
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                COT
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                COUNT
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                COUNTER
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                COVAR_POP
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                COVAR_SAMP
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                CREATE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                CROSS
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                CRYPT_HASH
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                CSTRING
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                CTR_BIG_ENDIAN
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                CTR_LENGTH
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                CTR_LITTLE_ENDIAN
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                CUME_DIST
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                CURRENT
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                CURRENT_CONNECTION
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                CURRENT_DATE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                CURRENT_ROLE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                CURRENT_TIME
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                CURRENT_TIMESTAMP
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                CURRENT_TRANSACTION
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                CURRENT_USER
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                CURSOR
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                DATA
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                DATABASE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                DATE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                DATEADD
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                DATEDIFF
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                DAY
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                DDL
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                DEBUG
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                DEC
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                DECFLOAT
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                DECIMAL
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                DECLARE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                DECODE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                DECRYPT
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                DEFAULT
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                DEFINER
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                DELETE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                DELETING
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                DENSE_RANK
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                DESC
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                DESCENDING
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                DESCRIPTOR
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                DETERMINISTIC
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                DIFFERENCE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                DISABLE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                DISCONNECT
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                DISTINCT
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                DO
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                DOMAIN
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                DOUBLE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                DROP
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                ELSE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                ENABLE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                ENCRYPT
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                END
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                ENGINE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                ENTRY_POINT
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                ESCAPE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                EXCEPTION
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                EXCESS
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                EXCLUDE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                EXECUTE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                EXISTS
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                EXIT
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                EXP
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                EXTENDED
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                EXTERNAL
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                EXTRACT
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                FALSE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                FETCH
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                FILE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                FILTER
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                FIRST
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                FIRSTNAME
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                FIRST_DAY
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                FIRST_VALUE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                FLOAT
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                FLOOR
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                FOLLOWING
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                FOR
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                FOREIGN
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                FREE_IT
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                FROM
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                FULL
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                FUNCTION
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                GDSCODE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                GENERATED
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                GENERATOR
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                GEN_ID
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                GEN_UUID
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                GLOBAL
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                GRANT
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                GRANTED
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                GROUP
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                HASH
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                HAVING
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                HEX_DECODE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                HEX_ENCODE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                HOUR
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                IDENTITY
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                IDLE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                IF
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                IGNORE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                IIF
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                IN
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                INACTIVE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                INCLUDE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                INCREMENT
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                INDEX
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                INNER
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                INPUT_TYPE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                INSENSITIVE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                INSERT
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                INSERTING
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                INT
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                INT128
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                INTEGER
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                INTO
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                INVOKER
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                IS
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                ISOLATION
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                IV
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                JOIN
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                KEY
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                LAG
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                LAST
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                LASTNAME
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                LAST_DAY
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                LAST_VALUE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                LATERAL
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                LEAD
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                LEADING
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                LEAVE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                LEFT
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                LEGACY
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                LENGTH
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                LEVEL
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                LIFETIME
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                LIKE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                LIMBO
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                LINGER
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                LIST
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                LN
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                LOCAL
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                LOCALTIME
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                LOCALTIMESTAMP
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                LOCK
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                LOCKED
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                LOG
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                LOG10
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                LONG
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                LOWER
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                LPAD
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                LPARAM
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                MAKE_DBKEY
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                MANUAL
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                MAPPING
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                MATCHED
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                MATCHING
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                MAX
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                MAXVALUE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                MERGE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                MESSAGE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                MIDDLENAME
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                MILLISECOND
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                MIN
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                MINUTE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                MINVALUE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                MOD
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                MODE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                MODULE_NAME
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                MONTH
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                NAME
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                NAMES
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                NATIONAL
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                NATIVE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                NATURAL
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                NCHAR
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                NEXT
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                NO
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                NORMALIZE_DECFLOAT
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                NOT
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                NTH_VALUE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                NTILE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                NULL
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                NULLIF
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                NULLS
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                NUMBER
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                NUMERIC
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                OCTET_LENGTH
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                OF
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                OFFSET
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                OLDEST
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                ON
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                ONLY
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                OPEN
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                OPTIMIZE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                OPTION
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                OR
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                ORDER
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                OS_NAME
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                OTHERS
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                OUTER
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                OUTPUT_TYPE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                OVER
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                OVERFLOW
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                OVERLAY
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                OVERRIDING
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                PACKAGE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                PAD
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                PAGE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                PAGES
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                PAGE_SIZE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                PARAMETER
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                PARTITION
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                PASSWORD
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                PERCENT_RANK
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                PI
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                PKCS_1_5
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                PLACING
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                PLAN
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                PLUGIN
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                POOL
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                POSITION
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                POST_EVENT
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                POWER
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                PRECEDING
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                PRECISION
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                PRESERVE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                PRIMARY
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                PRIOR
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                PRIVILEGE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                PRIVILEGES
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                PROCEDURE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                PROTECTED
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                PUBLICATION
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                QUANTIZE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                QUARTER
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                RAND
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                RANGE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                RANK
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                RDB$DB_KEY
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                RDB$ERROR
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                RDB$GET_CONTEXT
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                RDB$GET_TRANSACTION_CN
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                RDB$RECORD_VERSION
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                RDB$ROLE_IN_USE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                RDB$SET_CONTEXT
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                RDB$SYSTEM_PRIVILEGE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                READ
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                REAL
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                RECORD_VERSION
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                RECREATE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                RECURSIVE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                REFERENCES
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                REGR_AVGX
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                REGR_AVGY
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                REGR_COUNT
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                REGR_INTERCEPT
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                REGR_R2
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                REGR_SLOPE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                REGR_SXX
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                REGR_SXY
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                REGR_SYY
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                RELATIVE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                RELEASE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                REPLACE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                REQUESTS
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                RESERV
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                RESERVING
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                RESET
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                RESETTING
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                RESTART
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                RESTRICT
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                RETAIN
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                RETURN
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                RETURNING
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                RETURNING_VALUES
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                RETURNS
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                REVERSE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                REVOKE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                RIGHT
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                ROLE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                ROLLBACK
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                ROUND
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                ROW
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                ROWS
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                ROW_COUNT
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                ROW_NUMBER
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                RPAD
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                RSA_DECRYPT
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                RSA_ENCRYPT
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                RSA_PRIVATE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                RSA_PUBLIC
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                RSA_SIGN_HASH
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                RSA_VERIFY_HASH
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                SALT_LENGTH
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                SAVEPOINT
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                SCALAR_ARRAY
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                SCHEMA
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                SCROLL
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                SECOND
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                SECURITY
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                SEGMENT
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                SELECT
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                SENSITIVE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                SEQUENCE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                SERVERWIDE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                SESSION
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                SET
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                SHADOW
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                SHARED
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                SIGN
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                SIGNATURE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                SIMILAR
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                SIN
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                SINGULAR
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                SINH
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                SIZE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                SKIP
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                SMALLINT
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                SNAPSHOT
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                SOME
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                SORT
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                SOURCE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                SPACE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                SQL
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                SQLCODE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                SQLSTATE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                SQRT
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                STABILITY
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                START
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                STARTING
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                STARTS
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                STATEMENT
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                STATISTICS
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                STDDEV_POP
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                STDDEV_SAMP
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                SUBSTRING
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                SUB_TYPE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                SUM
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                SUSPEND
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                SYSTEM
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                TABLE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                TAGS
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                TAN
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                TANH
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                TARGET
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                TEMP
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                TEMPORARY
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                THEN
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                TIES
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                TIME
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                TIMEOUT
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                TIMESTAMP
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                TIMEZONE_HOUR
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                TIMEZONE_MINUTE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                TIMEZONE_NAME
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                TO
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                TOTALORDER
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                TRAILING
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                TRANSACTION
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                TRAPS
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                TRIGGER
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                TRIM
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                TRUE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                TRUNC
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                TRUSTED
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                TWO_PHASE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                TYPE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                UNBOUNDED
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                UNCOMMITTED
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                UNDO
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                UNICODE_CHAR
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                UNICODE_VAL
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                UNION
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                UNIQUE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                UNKNOWN
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                UPDATE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                UPDATING
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                UPPER
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                USAGE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                USER
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                USING
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                UUID_TO_CHAR
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                VALUE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                VALUES
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                VARBINARY
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                VARCHAR
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                VARIABLE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                VARYING
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                VAR_POP
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                VAR_SAMP
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                VIEW
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                WAIT
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                WEEK
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                WEEKDAY
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                WHEN
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                WHERE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                WHILE
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                WINDOW
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                WITH
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                WITHOUT
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                WORK
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                WRITE
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                YEAR
    RDB$KEYWORD_RESERVED            <true>

    RDB$KEYWORD_NAME                YEARDAY
    RDB$KEYWORD_RESERVED            <false>

    RDB$KEYWORD_NAME                ZONE
    RDB$KEYWORD_RESERVED            <false>


    Records affected: 498
"""

expected_stdout_6 = """
    RDB$KEYWORD_NAME                ABS
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                ABSOLUTE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                ACCENT
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                ACOS
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                ACOSH
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                ACTION
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                ACTIVE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                ADD
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                ADMIN
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                AFTER
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                ALL
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                ALTER
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                ALWAYS
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                AND
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                ANY
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                ANY_VALUE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                AS
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                ASC
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                ASCENDING
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                ASCII_CHAR
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                ASCII_VAL
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                ASIN
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                ASINH
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                AT
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                ATAN
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                ATAN2
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                ATANH
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                AUTO
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                AUTONOMOUS
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                AVG
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                BACKUP
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                BASE64_DECODE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                BASE64_ENCODE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                BEFORE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                BEGIN
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                BETWEEN
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                BIGINT
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                BINARY
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                BIND
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                BIN_AND
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                BIN_AND_AGG
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                BIN_NOT
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                BIN_OR
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                BIN_OR_AGG
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                BIN_SHL
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                BIN_SHR
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                BIN_XOR
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                BIN_XOR_AGG
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                BIT_LENGTH
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                BLOB
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                BLOBID
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                BLOB_APPEND
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                BLOCK
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                BODY
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                BOOLEAN
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                BOTH
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                BREAK
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                BTRIM
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                BY
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                CALL
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                CALLER
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                CASCADE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                CASE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                CAST
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                CEIL
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                CEILING
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                CHAR
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                CHARACTER
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                CHARACTER_LENGTH
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                CHAR_LENGTH
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                CHAR_TO_UUID
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                CHECK
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                CLEAR
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                CLOSE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                COALESCE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                COLLATE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                COLLATION
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                COLUMN
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                COMMENT
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                COMMIT
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                COMMITTED
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                COMMON
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                COMPARE_DECFLOAT
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                COMPUTED
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                CONDITIONAL
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                CONNECT
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                CONNECTIONS
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                CONSISTENCY
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                CONSTRAINT
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                CONTAINING
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                CONTINUE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                CORR
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                COS
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                COSH
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                COT
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                COUNT
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                COUNTER
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                COVAR_POP
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                COVAR_SAMP
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                CREATE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                CROSS
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                CRYPT_HASH
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                CSTRING
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                CTR_BIG_ENDIAN
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                CTR_LENGTH
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                CTR_LITTLE_ENDIAN
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                CUME_DIST
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                CURRENT
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                CURRENT_CONNECTION
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                CURRENT_DATE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                CURRENT_ROLE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                CURRENT_SCHEMA
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                CURRENT_TIME
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                CURRENT_TIMESTAMP
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                CURRENT_TRANSACTION
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                CURRENT_USER
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                CURSOR
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                DATA
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                DATABASE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                DATE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                DATEADD
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                DATEDIFF
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                DAY
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                DDL
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                DEBUG
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                DEC
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                DECFLOAT
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                DECIMAL
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                DECLARE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                DECODE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                DECRYPT
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                DEFAULT
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                DEFINER
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                DELETE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                DELETING
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                DENSE_RANK
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                DESC
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                DESCENDING
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                DESCRIPTOR
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                DETERMINISTIC
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                DIFFERENCE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                DISABLE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                DISCONNECT
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                DISTINCT
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                DO
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                DOMAIN
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                DOUBLE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                DOWNTO
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                DROP
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                ELSE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                ENABLE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                ENCRYPT
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                END
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                ENGINE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                ENTRY_POINT
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                ERROR
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                ESCAPE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                EXCEPTION
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                EXCESS
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                EXCLUDE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                EXECUTE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                EXISTS
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                EXIT
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                EXP
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                EXTENDED
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                EXTERNAL
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                EXTRACT
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                FALSE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                FETCH
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                FILE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                FILTER
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                FIRST
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                FIRSTNAME
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                FIRST_DAY
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                FIRST_VALUE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                FLOAT
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                FLOOR
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                FOLLOWING
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                FOR
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                FOREIGN
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                FORMAT
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                FREE_IT
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                FROM
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                FULL
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                FUNCTION
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                GDSCODE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                GENERATED
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                GENERATE_SERIES
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                GENERATOR
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                GEN_ID
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                GEN_UUID
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                GLOBAL
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                GRANT
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                GRANTED
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                GREATEST
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                GROUP
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                HASH
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                HAVING
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                HEX_DECODE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                HEX_ENCODE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                HOUR
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                IDENTITY
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                IDLE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                IF
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                IGNORE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                IIF
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                IN
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                INACTIVE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                INCLUDE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                INCREMENT
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                INDEX
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                INNER
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                INPUT_TYPE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                INSENSITIVE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                INSERT
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                INSERTING
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                INT
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                INT128
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                INTEGER
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                INTO
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                INVOKER
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                IS
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                ISOLATION
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                IV
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                JOIN
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                KEY
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                LAG
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                LAST
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                LASTNAME
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                LAST_DAY
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                LAST_VALUE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                LATERAL
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                LEAD
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                LEADING
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                LEAST
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                LEAVE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                LEFT
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                LEGACY
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                LENGTH
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                LEVEL
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                LIFETIME
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                LIKE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                LIMBO
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                LINGER
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                LIST
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                LISTAGG
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                LN
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                LOCAL
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                LOCALTIME
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                LOCALTIMESTAMP
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                LOCK
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                LOCKED
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                LOG
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                LOG10
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                LONG
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                LOWER
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                LPAD
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                LPARAM
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                LTRIM
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                MAKE_DBKEY
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                MANUAL
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                MAPPING
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                MATCHED
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                MATCHING
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                MAX
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                MAXVALUE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                MERGE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                MESSAGE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                MIDDLENAME
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                MILLISECOND
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                MIN
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                MINUTE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                MINVALUE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                MOD
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                MODE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                MODULE_NAME
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                MONTH
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                NAME
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                NAMES
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                NATIONAL
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                NATIVE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                NATURAL
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                NCHAR
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                NEXT
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                NO
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                NORMALIZE_DECFLOAT
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                NOT
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                NTH_VALUE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                NTILE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                NULL
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                NULLIF
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                NULLS
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                NUMBER
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                NUMERIC
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                OCTET_LENGTH
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                OF
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                OFFSET
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                OLDEST
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                ON
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                ONLY
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                OPEN
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                OPTIMIZE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                OPTION
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                OR
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                ORDER
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                OS_NAME
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                OTHERS
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                OUTER
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                OUTPUT_TYPE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                OVER
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                OVERFLOW
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                OVERLAY
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                OVERRIDING
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                OWNER
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                PACKAGE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                PAD
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                PAGE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                PAGES
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                PAGE_SIZE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                PARAMETER
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                PARTITION
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                PASSWORD
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                PERCENT_RANK
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                PI
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                PKCS_1_5
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                PLACING
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                PLAN
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                PLUGIN
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                POOL
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                POSITION
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                POST_EVENT
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                POWER
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                PRECEDING
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                PRECISION
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                PRESERVE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                PRIMARY
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                PRIOR
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                PRIVILEGE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                PRIVILEGES
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                PROCEDURE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                PROTECTED
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                PUBLICATION
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                QUANTIZE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                QUARTER
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                RAND
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                RANGE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                RANK
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                RDB$DB_KEY
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                RDB$ERROR
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                RDB$GET_CONTEXT
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                RDB$GET_TRANSACTION_CN
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                RDB$RECORD_VERSION
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                RDB$ROLE_IN_USE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                RDB$SET_CONTEXT
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                RDB$SYSTEM_PRIVILEGE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                READ
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                REAL
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                RECORD_VERSION
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                RECREATE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                RECURSIVE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                REFERENCES
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                REGR_AVGX
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                REGR_AVGY
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                REGR_COUNT
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                REGR_INTERCEPT
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                REGR_R2
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                REGR_SLOPE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                REGR_SXX
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                REGR_SXY
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                REGR_SYY
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                RELATIVE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                RELEASE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                REPLACE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                REQUESTS
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                RESERV
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                RESERVING
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                RESET
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                RESETTING
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                RESTART
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                RESTRICT
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                RETAIN
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                RETURN
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                RETURNING
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                RETURNING_VALUES
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                RETURNS
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                REVERSE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                REVOKE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                RIGHT
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                ROLE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                ROLLBACK
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                ROUND
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                ROW
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                ROWS
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                ROW_COUNT
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                ROW_NUMBER
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                RPAD
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                RSA_DECRYPT
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                RSA_ENCRYPT
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                RSA_PRIVATE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                RSA_PUBLIC
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                RSA_SIGN_HASH
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                RSA_VERIFY_HASH
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                RTRIM
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                SALT_LENGTH
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                SAVEPOINT
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                SCALAR_ARRAY
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                SCHEMA
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                SCROLL
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                SEARCH_PATH
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                SECOND
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                SECURITY
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                SEGMENT
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                SELECT
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                SENSITIVE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                SEQUENCE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                SERVERWIDE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                SESSION
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                SET
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                SHADOW
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                SHARED
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                SIGN
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                SIGNATURE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                SIMILAR
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                SIN
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                SINGULAR
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                SINH
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                SIZE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                SKIP
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                SMALLINT
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                SNAPSHOT
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                SOME
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                SORT
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                SOURCE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                SPACE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                SQL
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                SQLCODE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                SQLSTATE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                SQRT
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                STABILITY
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                START
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                STARTING
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                STARTS
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                STATEMENT
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                STATISTICS
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                STDDEV_POP
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                STDDEV_SAMP
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                SUBSTRING
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                SUB_TYPE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                SUM
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                SUSPEND
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                SYSTEM
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                TABLE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                TAGS
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                TAN
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                TANH
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                TARGET
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                TEMP
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                TEMPORARY
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                THEN
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                TIES
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                TIME
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                TIMEOUT
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                TIMESTAMP
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                TIMEZONE_HOUR
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                TIMEZONE_MINUTE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                TIMEZONE_NAME
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                TO
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                TOTALORDER
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                TRAILING
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                TRANSACTION
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                TRAPS
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                TRIGGER
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                TRIM
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                TRUE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                TRUNC
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                TRUNCATE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                TRUSTED
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                TWO_PHASE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                TYPE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                UNBOUNDED
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                UNCOMMITTED
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                UNDO
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                UNICODE_CHAR
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                UNICODE_VAL
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                UNION
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                UNIQUE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                UNKNOWN
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                UNLIST
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                UPDATE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                UPDATING
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                UPPER
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                USAGE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                USER
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                USING
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                UUID_TO_CHAR
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                VALUE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                VALUES
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                VARBINARY
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                VARCHAR
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                VARIABLE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                VARYING
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                VAR_POP
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                VAR_SAMP
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                VIEW
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                WAIT
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                WEEK
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                WEEKDAY
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                WHEN
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                WHERE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                WHILE
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                WINDOW
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                WITH
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                WITHIN
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                WITHOUT
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                WORK
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                WRITE
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                YEAR
    RDB$KEYWORD_RESERVED            <true>
    RDB$KEYWORD_NAME                YEARDAY
    RDB$KEYWORD_RESERVED            <false>
    RDB$KEYWORD_NAME                ZONE
    RDB$KEYWORD_RESERVED            <false>
    Records affected: 519
"""

@pytest.mark.disabled_in_forks
@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5 if act.is_version('<6') else expected_stdout_6
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
