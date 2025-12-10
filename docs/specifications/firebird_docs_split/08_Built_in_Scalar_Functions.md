# 08 Built in Scalar Functions



<!-- Original PDF Page: 411 -->

SQLSTATE,
              RDB$ERROR(MESSAGE));
    -- Re-throw exception
    EXCEPTION;
  END
END
3. Handling several errors in one WHEN block
...
WHEN GDSCODE GRANT_OBJ_NOTFOUND,
     GDSCODE GRANT_FLD_NOTFOUND,
     GDSCODE GRANT_NOPRIV,
     GDSCODE GRANT_NOPRIV_ON_BASE
DO
BEGIN
  EXECUTE PROCEDURE LOG_GRANT_ERROR(GDSCODE,
    RDB$ERROR(MESSAGE);
  EXIT;
END
...
4. Catching errors using the SQLSTATE code
EXECUTE BLOCK
AS
  DECLARE VARIABLE I INT;
BEGIN
  BEGIN
    I = 1/0;
    WHEN SQLSTATE '22003' DO
      EXCEPTION E_CUSTOM_EXCEPTION
        'Numeric value out of range.';
    WHEN SQLSTATE '22012' DO
      EXCEPTION E_CUSTOM_EXCEPTION
        'Division by zero.';
    WHEN SQLSTATE '23000' DO
      EXCEPTION E_CUSTOM_EXCEPTION
       'Integrity constraint violation.';
  END
END
See also
EXCEPTION, CREATE EXCEPTION, SQLCODE and GDSCODE Error Codes and Message Texts and SQLSTATE
Codes and Message Texts, GDSCODE, SQLCODE, SQLSTATE, RDB$ERROR()
Chapter 7. Procedural SQL (PSQL) Statements
410

<!-- Original PDF Page: 412 -->

Chapter 8. Built-in Scalar Functions
Unless explicitly mentioned otherwise in an “Available in” section, functions are available in DSQL
and PSQL. Availability of built-in functions in ESQL is not tracked by this Language Reference.
8.1. Context Functions
8.1.1. RDB$GET_CONTEXT()
Retrieves the value of a context variable from a namespace
Result type
VARCHAR(255)
Syntax
RDB$GET_CONTEXT ('<namespace>', <varname>)
<namespace> ::= SYSTEM | USER_SESSION | USER_TRANSACTION | DDL_TRIGGER
<varname>   ::= A case-sensitive quoted string of max. 80 characters
Table 118. RDB$GET_CONTEXT Function Parameters
Parameter Description
namespace Namespace
varname Variable name; case-sensitive with a maximum length of 80 characters
The namespaces
The USER_SESSION and USER_TRANSACTION namespaces are initially empty. A user can create and set
variables with RDB$SET_CONTEXT() and retrieve them with RDB$GET_CONTEXT(). The SYSTEM namespace
is read-only. The DDL_TRIGGER namespace is only valid in DDL triggers, and is read-only. The SYSTEM
and DDL_TRIGGER namespaces contain a number of predefined variables, shown below.
Return values and error behaviour
If the polled variable exists in the given namespace, its value will be returned as a string of max.
255 characters. If the namespace doesn’t exist or if you try to access a non-existing variable in the
SYSTEM or DDL_TRIGGER namespace, an error is raised. If you request a non-existing variable in one of
the user namespaces, NULL is returned. Both namespace and variable names must be given as
single-quoted, case-sensitive, non-NULL strings.
The SYSTEM Namespace
Context variables in the SYSTEM namespace
CLIENT_ADDRESS
For TCP, this is the IP address. For XNET, the local process ID. For all other protocols this variable
is NULL.
Chapter 8. Built-in Scalar Functions
411

<!-- Original PDF Page: 413 -->

CLIENT_HOST
The wire protocol host name of remote client. Value is returned for all supported protocols.
CLIENT_PID
Process ID of remote client application.
CLIENT_PROCESS
Process name of remote client application.
CURRENT_ROLE
Same as global CURRENT_ROLE variable.
CURRENT_USER
Same as global CURRENT_USER variable.
DB_FILE_ID
Unique filesystem-level ID of the current database.
DB_GUID
GUID of the current database.
DB_NAME
Canonical name of current database; either the full path to the database or — if connecting via
the path is disallowed — its alias.
DECFLOAT_ROUND
Rounding mode of the current connection used in operations with DECFLOAT values. See also SET
DECFLOAT.
DECFLOAT_TRAPS
Exceptional conditions for the current connection in operations with DECFLOAT values that cause
a trap. See also SET DECFLOAT.
EFFECTIVE_USER
Effective user at the point RDB$GET_CONTEXT is called; indicates privileges of which user is
currently used to execute a function, procedure, trigger.
ENGINE_VERSION
The Firebird engine (server) version.
EXT_CONN_POOL_ACTIVE_COUNT
Count of active connections associated with the external connection pool.
EXT_CONN_POOL_IDLE_COUNT
Count of currently inactive connections available in the connection pool.
EXT_CONN_POOL_LIFETIME
External connection pool idle connection lifetime, in seconds.
Chapter 8. Built-in Scalar Functions
412

<!-- Original PDF Page: 414 -->

EXT_CONN_POOL_SIZE
External connection pool size.
GLOBAL_CN
Most current value of global Commit Number counter.
ISOLATION_LEVEL
The isolation level of the current transaction: 'READ COMMITTED', 'SNAPSHOT' or 'CONSISTENCY'.
LOCK_TIMEOUT
Lock timeout of the current transaction.
NETWORK_PROTOCOL
The protocol used for the connection: 'TCPv4', 'TCPv6', 'XNET' or NULL.
PARALLEL_WORKERS
The maximum number of parallel workers of the connection.
READ_ONLY
Returns 'TRUE' if current transaction is read-only and 'FALSE' otherwise.
REPLICA_MODE
Replica mode of the database: 'READ-ONLY', 'READ-WRITE' and NULL.
REPLICATION_SEQUENCE
Current replication sequence (number of the latest segment written to the replication journal).
SESSION_ID
Same as global CURRENT_CONNECTION variable.
SESSION_IDLE_TIMEOUT
Connection-level idle timeout, or 0 if no timeout was set. When 0 is reported the database
ConnectionIdleTimeout from databases.conf or firebird.conf applies.
SESSION_TIMEZONE
Current session time zone.
SNAPSHOT_NUMBER
Current snapshot number for the transaction executing this statement. For SNAPSHOT and
SNAPSHOT TABLE STABILITY , this number is stable for the duration of the transaction; for READ
COMMITTED this number will change (increment) as concurrent transactions are committed.
STATEMENT_TIMEOUT
Connection-level statement timeout, or 0 if no timeout was set. When 0 is reported the database
StatementTimeout from databases.conf or firebird.conf applies.
TRANSACTION_ID
Same as global CURRENT_TRANSACTION variable.
Chapter 8. Built-in Scalar Functions
413

<!-- Original PDF Page: 415 -->

WIRE_COMPRESSED
Compression status of the current connection. If the connection is compressed, returns TRUE; if it
is not compressed, returns FALSE. Returns NULL if the connection is embedded.
WIRE_CRYPT_PLUGIN
If connection is encrypted - returns name of current plugin, otherwise NULL.
WIRE_ENCRYPTED
Encryption status of the current connection. If the connection is encrypted, returns TRUE; if it is
not encrypted, returns FALSE. Returns NULL if the connection is embedded.
The DDL_TRIGGER Namespace
The DDL_TRIGGER namespace is valid only when a DDL trigger is running. Its use is also valid in
stored procedures and functions when called by DDL triggers.
The DDL_TRIGGER context works like a stack. Before a DDL trigger is fired, the values relative to the
executed command are pushed onto this stack. After the trigger finishes, the values are popped. So
in the case of cascade DDL statements, when a user DDL command fires a DDL trigger and this
trigger executes another DDL command with EXECUTE STATEMENT , the values of the DDL_TRIGGER
namespace are the ones relative to the command that fired the last DDL trigger on the call stack.
Context variables in the DDL_TRIGGER namespace
EVENT_TYPE
event type (CREATE, ALTER, DROP)
OBJECT_TYPE
object type (TABLE, VIEW, etc)
DDL_EVENT
event name (<ddl event item>), where <ddl event item> is EVENT_TYPE || ' ' || OBJECT_TYPE
OBJECT_NAME
metadata object name
OLD_OBJECT_NAME
for tracking the renaming of a domain (see note)
NEW_OBJECT_NAME
for tracking the renaming of a domain (see note)
SQL_TEXT
sql statement text

ALTER DOMAIN old-name TO new-name sets OLD_OBJECT_NAME and NEW_OBJECT_NAME in
both BEFORE and AFTER triggers. For this command, OBJECT_NAME will have the old
object name in BEFORE triggers, and the new object name in AFTER triggers.
Chapter 8. Built-in Scalar Functions
414

<!-- Original PDF Page: 416 -->

Examples
select rdb$get_context('SYSTEM', 'DB_NAME') from rdb$database
New.UserAddr = rdb$get_context('SYSTEM', 'CLIENT_ADDRESS');
insert into MyTable (TestField)
  values (rdb$get_context('USER_SESSION', 'MyVar'))
See also
RDB$SET_CONTEXT()
8.1.2. RDB$SET_CONTEXT()
Creates, sets or clears a variable in one of the user-writable namespaces
Result type
INTEGER
Syntax
RDB$SET_CONTEXT ('<namespace>', <varname>, <value> | NULL)
<namespace> ::= USER_SESSION | USER_TRANSACTION
<varname>   ::= A case-sensitive quoted string of max. 80 characters
<value>     ::= A value of any type, as long as it's castable
                to a VARCHAR(255)
Table 119. RDB$SET_CONTEXT Function Parameters
Parameter Description
namespace Namespace
varname Variable name. Case-sensitive. Maximum length is 80 characters
value Data of any type provided it can be cast to VARCHAR(255)
The namespaces
The USER_SESSION and USER_TRANSACTION namespaces are initially empty. A user can create and set
variables with RDB$SET_CONTEXT() and retrieve them with RDB$GET_CONTEXT(). The USER_SESSION
context is bound to the current connection, the USER_TRANSACTION context to the current transaction.
Lifecycle
• When a transaction ends, its USER_TRANSACTION context is cleared.
• When a connection is closed, its USER_SESSION context is cleared.
• When a connection is reset using ALTER SESSION RESET , the USER_TRANSACTION and USER_SESSION
contexts are cleared.
Return values and error behaviour
Chapter 8. Built-in Scalar Functions
415

<!-- Original PDF Page: 417 -->

The function returns 1 when the variable already existed before the call and 0 when it didn’t. To
remove a variable from a context, set it to NULL. If the given namespace doesn’t exist, an error is
raised. Both namespace and variable names must be entered as single-quoted, case-sensitive, non-
NULL strings.

• The maximum number of variables in any single context is 1000.
• All USER_TRANSACTION variables survive a ROLLBACK RETAIN (see ROLLBACK Options)
or ROLLBACK TO SAVEPOINT  unaltered, no matter at which point during the
transaction they were set.
• Due to its UDF-like nature, RDB$SET_CONTEXT can — in PSQL only — be called like
a void function, without assigning the result, as in the second example above.
Regular internal functions don’t allow this type of use.
• ALTER SESSION RESET clears both USER_TRANSACTION and USER_SESSION contexts.
Examples
select rdb$set_context('USER_SESSION', 'MyVar', 493) from rdb$database
rdb$set_context('USER_SESSION', 'RecordsFound', RecCounter);
select rdb$set_context('USER_TRANSACTION', 'Savepoints', 'Yes')
  from rdb$database
See also
RDB$GET_CONTEXT()
8.2. Mathematical Functions
8.2.1. ABS()
Absolute value
Result type
Numerical, matching input type
Syntax
ABS (number)
Table 120. ABS Function Parameter
Parameter Description
number An expression of a numeric type
Chapter 8. Built-in Scalar Functions
416

<!-- Original PDF Page: 418 -->

8.2.2. ACOS()
Arc cosine
Result type
DOUBLE PRECISION
Syntax
ACOS (number)
Table 121. ACOS Function Parameter
Parameter Description
number An expression of a numeric type within the range [-1, 1]
• The result is an angle in the range [0, pi].
See also
COS(), ASIN(), ATAN()
8.2.3. ACOSH()
Inverse hyperbolic cosine
Result type
DOUBLE PRECISION
Syntax
ACOSH (number)
Table 122. ACOSH Function Parameter
Parameter Description
number Any non-NULL value in the range [1, INF].
The result is in the range [0, INF].
See also
COSH(), ASINH(), ATANH()
8.2.4. ASIN()
Arc sine
Result type
DOUBLE PRECISION
Chapter 8. Built-in Scalar Functions
417

<!-- Original PDF Page: 419 -->

Syntax
ASIN (number)
Table 123. ASIN Function Parameter
Parameter Description
number An expression of a numeric type within the range [-1, 1]
The result is an angle in the range [-pi/2, pi/2].
See also
SIN(), ACOS(), ATAN()
8.2.5. ASINH()
Inverse hyperbolic sine
Result type
DOUBLE PRECISION
Syntax
ASINH (number)
Table 124. ASINH Function Parameter
Parameter Description
number Any non-NULL value in the range [-INF, INF].
The result is in the range [-INF, INF].
See also
SINH(), ACOSH(), ATANH()
8.2.6. ATAN()
Arc tangent
Result type
DOUBLE PRECISION
Syntax
ATAN (number)
Table 125. ATAN Function Parameter
Chapter 8. Built-in Scalar Functions
418

<!-- Original PDF Page: 420 -->

Parameter Description
number An expression of a numeric type
The result is an angle in the range <-pi/2, pi/2>.
See also
ATAN2(), TAN(), ACOS(), ASIN()
8.2.7. ATAN2()
Two-argument arc tangent
Result type
DOUBLE PRECISION
Syntax
ATAN2 (y, x)
Table 126. ATAN2 Function Parameters
Parameter Description
y An expression of a numeric type
x An expression of a numeric type
Returns the angle whose sine-to-cosine ratio is given by the two arguments, and whose sine and
cosine signs correspond to the signs of the arguments. This allows results across the entire circle,
including the angles -pi/2 and pi/2.
• The result is an angle in the range [-pi, pi].
• If x is negative, the result is pi if y is 0, and -pi if y is -0.
• If both y and x are 0, the result is meaningless. An error will be raised if both arguments are 0.
• A fully equivalent description of this function is the following: ATAN2(y, x) is the angle
between the positive X-axis and the line from the origin to the point (x, y). This also makes
it obvious that ATAN2(0, 0) is undefined.
• If x is greater than 0, ATAN2(y, x) is the same as ATAN(y/x).
• If both sine and cosine of the angle are already known, ATAN2(sin, cos) gives the angle.
8.2.8. ATANH()
Inverse hyperbolic tangent
Result type
DOUBLE PRECISION
Chapter 8. Built-in Scalar Functions
419

<!-- Original PDF Page: 421 -->

Syntax
ATANH (number)
Table 127. ATANH Function Parameter
Parameter Description
number Any non-NULL value in the range <-1, 1>.
The result is a number in the range [-INF, INF].
See also
TANH(), ACOSH(), ASINH()
8.2.9. CEIL(), CEILING()
Ceiling of a number
Result type
BIGINT or INT128 for exact numeric number, or DOUBLE PRECISION  or DECFLOAT for floating point
number
Syntax
CEIL[ING] (number)
Table 128. CEIL[ING] Function Parameters
Parameter Description
number An expression of a numeric type
Returns the smallest whole number greater than or equal to the argument.
See also
FLOOR(), ROUND(), TRUNC()
8.2.10. COS()
Cosine
Result type
DOUBLE PRECISION
Syntax
COS (angle)
Table 129. COS Function Parameter
Chapter 8. Built-in Scalar Functions
420

<!-- Original PDF Page: 422 -->

Parameter Description
angle An angle in radians
The result is in the range [-1, 1].
See also
ACOS(), COT(), SIN(), TAN()
8.2.11. COSH()
Hyperbolic cosine
Result type
DOUBLE PRECISION
Syntax
COSH (number)
Table 130. COSH Function Parameter
Parameter Description
number A number of a numeric type
The result is in the range [1, INF].
See also
ACOSH(), SINH(), TANH()
8.2.12. COT()
Cotangent
Result type
DOUBLE PRECISION
Syntax
COT (angle)
Table 131. COT Function Parameter
Parameter Description
angle An angle in radians
See also
COS(), SIN(), TAN()
Chapter 8. Built-in Scalar Functions
421

<!-- Original PDF Page: 423 -->

8.2.13. EXP()
Natural exponent
Result type
DOUBLE PRECISION
Syntax
EXP (number)
Table 132. EXP Function Parameter
Parameter Description
number A number of a numeric type
Returns the natural exponential, enumber
See also
LN()
8.2.14. FLOOR()
Floor of a number
Result type
BIGINT or INT128 for exact numeric number, or DOUBLE PRECISION  or DECFLOAT for floating point
number
Syntax
FLOOR (number)
Table 133. FLOOR Function Parameter
Parameter Description
number An expression of a numeric type
Returns the largest whole number smaller than or equal to the argument.
See also
CEIL(), CEILING(), ROUND(), TRUNC()
8.2.15. LN()
Natural logarithm
Result type
DOUBLE PRECISION
Chapter 8. Built-in Scalar Functions
422

<!-- Original PDF Page: 424 -->

Syntax
LN (number)
Table 134. LN Function Parameter
Parameter Description
number An expression of a numeric type
An error is raised if the argument is negative or 0.
See also
EXP(), LOG(), LOG10()
8.2.16. LOG()
Logarithm with variable base
Result type
DOUBLE PRECISION
Syntax
LOG (x, y)
Table 135. LOG Function Parameters
Parameter Description
x Base. An expression of a numeric type
y An expression of a numeric type
Returns the x-based logarithm of y.
• If either argument is 0 or below, an error is raised.
• If both arguments are 1, NaN is returned.
• If x = 1 and y < 1, -INF is returned.
• If x = 1 and y > 1, INF is returned.
See also
POWER(), LN(), LOG10()
8.2.17. LOG10()
Decimal (base-10) logarithm
Result type
DOUBLE PRECISION
Chapter 8. Built-in Scalar Functions
423

<!-- Original PDF Page: 425 -->

Syntax
LOG10 (number)
Table 136. LOG10 Function Parameter
Parameter Description
number An expression of a numeric type
An error is raised if the argument is negative or 0.
See also
POWER(), LN(), LOG()
8.2.18. MOD()
Remainder
Result type
SMALLINT, INTEGER or BIGINT depending on the type of a. If a is a floating-point type, the result is a
BIGINT.
Syntax
MOD (a, b)
Table 137. MOD Function Parameters
Parameter Description
a An expression of a numeric type
b An expression of a numeric type
Returns the remainder of an integer division.
• Non-integer arguments are rounded before the division takes place. So, “ mod(7.5, 2.5)” gives 2
(“mod(8, 3)”), not 0.
• Do not confuse MOD() with the mathematical modulus operator; e.g. mathematically, -21 mod 4 is
3, while Firebird’s MOD(-21, 4) is -1. In other words, MOD() behaves as % in languages like C and
Java.
8.2.19. PI()
Approximation of pi.
Result type
DOUBLE PRECISION
Chapter 8. Built-in Scalar Functions
424

<!-- Original PDF Page: 426 -->

Syntax
PI ()
8.2.20. POWER()
Power
Result type
DOUBLE PRECISION
Syntax
POWER (x, y)
Table 138. POWER Function Parameters
Parameter Description
x An expression of a numeric type
y An expression of a numeric type
Returns x to the power of y (xy
).
See also
EXP(), LOG(), LOG10(), SQRT()
8.2.21. RAND()
Generates a random number
Result type
DOUBLE PRECISION
Syntax
RAND ()
Returns a random number between 0 and 1.
8.2.22. ROUND()
Result type
single argument: integer type, DOUBLE PRECISION or DECFLOAT;
two arguments: numerical, matching first argument
Chapter 8. Built-in Scalar Functions
425

<!-- Original PDF Page: 427 -->

Syntax
ROUND (number [, scale])
Table 139. ROUND Function Parameters
Parameter Description
number An expression of a numeric type
scale An integer specifying the number of decimal places toward which
rounding is to be performed, e.g.:
•  2 for rounding to the nearest multiple of 0.01
•  1 for rounding to the nearest multiple of 0.1
•  0 for rounding to the nearest whole number
• -1 for rounding to the nearest multiple of 10
• -2 for rounding to the nearest multiple of 100
Rounds a number to the nearest integer. If the fractional part is exactly 0.5, rounding is upward for
positive numbers and downward for negative numbers. With the optional scale argument, the
number can be rounded to powers-of-ten multiples (tens, hundreds, tenths, hundredths, etc.).

If you are used to the behaviour of the external function ROUND, please notice that
the internal function always rounds halves away from zero, i.e. downward for
negative numbers.
ROUND Examples
If the scale argument is present, the result usually has the same scale as the first argument:
ROUND(123.654, 1) -- returns 123.700 (not 123.7)
ROUND(8341.7, -3) -- returns 8000.0 (not 8000)
ROUND(45.1212, 0) -- returns 45.0000 (not 45)
Otherwise, the result scale is 0:
ROUND(45.1212) -- returns 45
See also
CEIL(), CEILING(), FLOOR(), TRUNC()
8.2.23. SIGN()
Sign or signum
Result type
Chapter 8. Built-in Scalar Functions
426

<!-- Original PDF Page: 428 -->

SMALLINT
Syntax
SIGN (number)
Table 140. SIGN Function Parameter
Parameter Description
number An expression of a numeric type
Returns the sign of the argument: -1, 0 or 1
• number < 0 → -1
• number = 0 → 0
• number > 0 → 1
8.2.24. SIN()
Sine
Result type
DOUBLE PRECISION
Syntax
SIN (angle)
Table 141. SIN Function Parameter
Parameter Description
angle An angle, in radians
The result is in the range [-1, 1].
See also
ASIN(), COS(), COT(), TAN()
8.2.25. SINH()
Hyperbolic sine
Result type
DOUBLE PRECISION
Syntax
SINH (number)
Chapter 8. Built-in Scalar Functions
427

<!-- Original PDF Page: 429 -->

Table 142. SINH Function Parameter
Parameter Description
number An expression of a numeric type
See also
ASINH(), COSH(), TANH()
8.2.26. SQRT()
Square root
Result type
DOUBLE PRECISION
Syntax
SQRT (number)
Table 143. SQRT Function Parameter
Parameter Description
number An expression of a numeric type
If number is negative, an error is raised.
See also
POWER()
8.2.27. TAN()
Tangent
Result type
DOUBLE PRECISION
Syntax
TAN (angle)
Table 144. TAN Function Parameter
Parameter Description
angle An angle, in radians
See also
ATAN(), ATAN2(), COS(), COT(), SIN(), TAN()
Chapter 8. Built-in Scalar Functions
428

<!-- Original PDF Page: 430 -->

8.2.28. TANH()
Hyperbolic tangent
Result type
DOUBLE PRECISION
Syntax
TANH (number)
Table 145. TANH Function Parameters
Parameter Description
number An expression of a numeric type
Due to rounding, the result is in the range [-1, 1] (mathematically, it’s <-1, 1>).
See also
ATANH(), COSH(), TANH()
8.2.29. TRUNC()
Truncate number
Result type
single argument: integer type, DOUBLE PRECISION or DECFLOAT;
two arguments: numerical, matching first argument
Syntax
TRUNC (number [, scale])
Table 146. TRUNC Function Parameters
Parameter Description
number An expression of a numeric type
scale An integer specifying the number of decimal places toward which
truncating is to be performed, e.g.:
•  2 for truncating to the nearest multiple of 0.01
•  1 for truncating to the nearest multiple of 0.1
•  0 for truncating to the nearest whole number
• -1 for truncating to the nearest multiple of 10
• -2 for truncating to the nearest multiple of 100
The single argument variant returns the integer part of a number. With the optional scale
Chapter 8. Built-in Scalar Functions
429

<!-- Original PDF Page: 431 -->

argument, the number can be truncated to powers-of-ten multiples (tens, hundreds, tenths,
hundredths, etc.).

• If the scale argument is present, the result usually has the same scale as the
first argument, e.g.
◦ TRUNC(789.2225, 2) returns 789.2200 (not 789.22)
◦ TRUNC(345.4, -2) returns 300.0 (not 300)
◦ TRUNC(-163.41, 0) returns -163.00 (not -163)
• Otherwise, the result scale is 0:
◦ TRUNC(-163.41) returns -163

If you are used to the behaviour of the external function TRUNCATE, please notice
that the internal function TRUNC always truncates toward zero, i.e. upward for
negative numbers.
See also
CEIL(), CEILING(), FLOOR(), ROUND()
8.3. String and Binary Functions
8.3.1. ASCII_CHAR()
Character from ASCII code
Result type
CHAR(1) CHARACTER SET NONE
Syntax
ASCII_CHAR (code)
Table 147. ASCII_CHAR Function Parameter
Parameter Description
code An integer within the range from 0 to 255
Returns the ASCII character corresponding to the number passed in the argument.

• If you are used to the behaviour of the ASCII_CHAR UDF, which returns an empty
string if the argument is 0, please notice that the internal function returns a
character with ASCII code 0 (character NUL) here.
See also
ASCII_VAL(), UNICODE_CHAR()
Chapter 8. Built-in Scalar Functions
430

<!-- Original PDF Page: 432 -->

8.3.2. ASCII_VAL()
ASCII code from string
Result type
SMALLINT
Syntax
ASCII_VAL (ch)
Table 148. ASCII_VAL Function Parameter
Parameter Description
ch A string of the [VAR]CHAR data type or a text BLOB with the maximum size
of 32,767 bytes
Returns the ASCII code of the character passed in.
• If the argument is a string with more than one character, the ASCII code of the first character is
returned.
• If the argument is an empty string, 0 is returned.
• If the argument is NULL, NULL is returned.
• If the first character of the argument string is multi-byte, an error is raised.
See also
ASCII_CHAR(), UNICODE_VAL()
8.3.3. BASE64_DECODE()
Decodes a base64 string to binary
Result type
VARBINARY or BLOB
Syntax
BASE64_DECODE (base64_data)
Table 149. BASE64_DECODE Function Parameter
Parameter Description
base64_data Base64 encoded data, padded with = to multiples of 4
BASE64_DECODE decodes a string with base64-encoded data, and returns the decoded value as
VARBINARY or BLOB as appropriate for the input. If the length of the type of base64_data is not a
multiple of 4, an error is raised at prepare time. If the length of the value of base64_data is not a
multiple of 4, an error is raised at execution time.
Chapter 8. Built-in Scalar Functions
431

<!-- Original PDF Page: 433 -->

When the input is not BLOB, the length of the resulting type is calculated as type_length * 3 / 4 ,
where type_length is the maximum length in characters of the input type.
Example of BASE64_DECODE
select cast(base64_decode('VGVzdCBiYXNlNjQ=') as varchar(12))
from rdb$database;
CAST
============
Test base64
See also
BASE64_ENCODE(), HEX_DECODE()
8.3.4. BASE64_ENCODE()
Encodes a (binary) value to a base64 string
Result type
VARCHAR CHARACTER SET ASCII or BLOB SUB_TYPE TEXT CHARACTER SET ASCII
Syntax
BASE64_ENCODE (binary_data)
Table 150. BASE64_ENCODE Function Parameter
Parameter Description
binary_data Binary data (or otherwise convertible to binary) to encode
BASE64_ENCODE encodes binary_data with base64, and returns the encoded value as a VARCHAR
CHARACTER SET ASCII or BLOB SUB_TYPE TEXT CHARACTER SET ASCII  as appropriate for the input. The
returned value is padded with ‘=’ so its length is a multiple of 4.
When the input is not BLOB, the length of the resulting type is calculated as type_length * 4 / 3 
rounded up to a multiple of four, where type_length is the maximum length in bytes of the input
type. If this length exceeds the maximum length of VARCHAR, the function returns a BLOB.
Example of BASE64_ENCODE
select base64_encode('Test base64')
from rdb$database;
BASE64_ENCODE
================
VGVzdCBiYXNlNjQ=
Chapter 8. Built-in Scalar Functions
432

<!-- Original PDF Page: 434 -->

See also
BASE64_DECODE(), HEX_ENCODE()
8.3.5. BIT_LENGTH()
String or binary length in bits
Result type
INTEGER, or BIGINT for BLOB
Syntax
BIT_LENGTH (string)
Table 151. BIT_LENGTH Function Parameter
Parameter Description
string An expression of a string type
Gives the length in bits of the input string. For multi-byte character sets, this may be less than the
number of characters times 8 times the “formal” number of bytes per character as found in
RDB$CHARACTER_SETS.

With arguments of type CHAR, this function takes the entire formal string length (i.e.
the declared length of a field or variable) into account. If you want to obtain the
“logical” bit length, not counting the trailing spaces, right-TRIM the argument before
passing it to BIT_LENGTH.
BIT_LENGTH Examples
select bit_length('Hello!') from rdb$database
-- returns 48
select bit_length(_iso8859_1 'Grüß di!') from rdb$database
-- returns 64: ü and ß take up one byte each in ISO8859_1
select bit_length
  (cast (_iso8859_1 'Grüß di!' as varchar(24) character set utf8))
from rdb$database
-- returns 80: ü and ß take up two bytes each in UTF8
select bit_length
  (cast (_iso8859_1 'Grüß di!' as char(24) character set utf8))
from rdb$database
-- returns 208: all 24 CHAR positions count, and two of them are 16-bit
See also
Chapter 8. Built-in Scalar Functions
433

<!-- Original PDF Page: 435 -->

OCTET_LENGTH(), CHAR_LENGTH(), CHARACTER_LENGTH()
8.3.6. BLOB_APPEND()
Efficient concatenation of blobs
Result type
BLOB
Syntax
BLOB_APPEND(expr1, expr2 [, exprN ... ])
Table 152. BLOB_APPEND Function Parameters
Parameter Description
exprN An expression of a type convertible to BLOB
The BLOB_APPEND function concatenates blobs without creating intermediate BLOBs, avoiding
excessive memory consumption and growth of the database file. The BLOB_APPEND function takes two
or more arguments and adds them to a BLOB which remains open for further modification by a
subsequent BLOB_APPEND call.
The resulting BLOB is left open for writing instead of being closed when the function returns. In
other words, the BLOB can be appended as many times as required. The engine marks the BLOB
returned by BLOB_APPEND with an internal flag, BLB_close_on_read, and closes it automatically when
needed.
The first argument determines the behaviour of the function:
1. NULL: new, empty BLOB SUB_TYPE TEXT  is created, using the connection character set as the
character set
2. permanent BLOB (from a table) or temporary BLOB which was already closed: new BLOB is created
with the same subtype and, if subtype is TEXT the same character set, populated with the content
of the original BLOB.
3. temporary unclosed BLOB with the BLB_close_on_read flag (e.g. created by another call to
BLOB_APPEND): used as-is, remaining arguments are appended to this BLOB
4. other data types: a new BLOB SUB_TYPE TEXT  is created, populated with the original argument
converted to string. If the original value is a character type, its character set is used (for string
literals, the connection character set), otherwise the connection character set.
Other arguments can be of any type. The following behavior is defined for them:
1. NULLs are ignored (behaves as empty string)
2. BLOBs, if necessary, are transliterated to the character set of the first argument and their
contents are appended to the result
3. other data types are converted to strings (as usual) and appended to the result
Chapter 8. Built-in Scalar Functions
434

<!-- Original PDF Page: 436 -->

The BLOB_APPEND function returns a temporary unclosed BLOB with the BLB_close_on_read flag. If the
first argument is such a temporary unclosed BLOB (e.g. created by a previous call to BLOB_APPEND), it
will be used as-is, otherwise a new BLOB is created. Thus, a series of operations like blob =
BLOB_APPEND (blob, …) will result in the creation of at most one BLOB (unless you try to append a
BLOB to itself). This blob will be automatically closed by the engine when the client reads it, assigns
it to a table, or uses it in other expressions that require reading the content.

Important caveats for BLOB_APPEND
1. The NULL behaviour of BLOB_APPEND is different from normal concatenation
(using ||). Occurrence of NULL will behave as if an empty string was used. In
other words, NULL is effectively ignored.
In normal concatenation, concatenating with NULL results in NULL.

Testing a blob for NULL using the IS [NOT] NULL  operator does not read it and
therefore a temporary blob with the BLB_close_on_read flag will not be closed after
such a test.

Use LIST or BLOB_APPEND functions to concatenate blobs. This reduces memory
consumption and disk I/O, and also prevents database growth due to the creation
of many temporary blobs when using the concatenation operator.
BLOB_APPEND Examples
execute block
returns (b blob sub_type text)
as
begin
  -- creates a new temporary not closed BLOB
  -- and writes the string from the 2nd argument into it
  b = blob_append(null, 'Hello ');
  -- adds two strings to the temporary BLOB without closing it
  b = blob_append(b, 'World', '!');
  -- comparing a BLOB with a string will close it, because the BLOB needs to be read
  if (b = 'Hello World!') then
  begin
  -- ...
  end
  -- creates a temporary closed BLOB by adding a string to it
  b = b || 'Close';
  suspend;
end
Chapter 8. Built-in Scalar Functions
435

<!-- Original PDF Page: 437 -->

See also
Concatenation Operator, LIST(), RDB$BLOB_UTIL
8.3.7. CHAR_LENGTH(), CHARACTER_LENGTH()
String length in characters
Result type
INTEGER, or BIGINT for BLOB
Syntax
  CHAR_LENGTH (string)
| CHARACTER_LENGTH (string)
Table 153. CHAR[ACTER]_LENGTH Function Parameter
Parameter Description
string An expression of a string type
Gives the length in characters of the input string.

• With arguments of type CHAR, this function returns the formal string length (i.e.
the declared length of a field or variable). If you want to obtain the “logical”
length, not counting the trailing spaces, right- TRIM the argument before passing
it to CHAR[ACTER]_LENGTH.
• This function fully supports text BLOBs of any length and character set.
CHAR_LENGTH Examples
select char_length('Hello!') from rdb$database
-- returns 6
select char_length(_iso8859_1 'Grüß di!') from rdb$database
-- returns 8
select char_length
  (cast (_iso8859_1 'Grüß di!' as varchar(24) character set utf8))
from rdb$database
-- returns 8; the fact that ü and ß take up two bytes each is irrelevant
select char_length
  (cast (_iso8859_1 'Grüß di!' as char(24) character set utf8))
from rdb$database
-- returns 24: all 24 CHAR positions count
See also
Chapter 8. Built-in Scalar Functions
436

<!-- Original PDF Page: 438 -->

BIT_LENGTH(), OCTET_LENGTH()
8.3.8. CRYPT_HASH()
Cryptographic hash
Result type
VARBINARY
Syntax
CRYPT_HASH (value USING <hash>)
<hash> ::= MD5 | SHA1 | SHA256 | SHA512
Table 154. CRYPT_HASH Function Parameter
Parameter Description
value Expression of value of any type; non-string or non-binary types are
converted to string
hash Cryptographic hash algorithm to apply
CRYPT_HASH returns a cryptographic hash calculated from the input argument using the specified
algorithm. If the input argument is not a string or binary type, it is converted to string before
hashing.
This function returns a VARBINARY with the length depending on the specified algorithm.

• The MD5 and SHA1 algorithms are not recommended for security purposes due to
known attacks to generate hash collisions. These two algorithms are provided
for backward-compatibility only.
• When hashing string or binary values, take into account the effects of trailing
blanks (spaces or NULs). The value 'ab' in a CHAR(5) (3 trailing spaces) has a
different hash than if it is stored in a VARCHAR(5) (no trailing spaces) or CHAR(6)
(4 trailing spaces).
To avoid this, make sure you always use a variable length data type, or the
same fixed length data type, or normalize values before hashing, for example
using TRIM(TRAILING FROM value).
Examples of CRYPT_HASH
Hashing x with the SHA512 algorithm
select crypt_hash(x using sha512) from y;
See also
HASH()
Chapter 8. Built-in Scalar Functions
437

<!-- Original PDF Page: 439 -->

8.3.9. HASH()
Non-cryptographic hash
Result type
INTEGER, BIGINT
Syntax
HASH (value [USING <hash>])
<hash> ::= CRC32
Table 155. HASH Function Parameter
Parameter Description
value Expression of value of any type; non-string or non-binary types are
converted to string
hash Non-cryptographic hash algorithm to apply
HASH returns a hash value for the input argument. If the input argument is not a string or binary
type, it is converted to string before hashing.
The optional USING clause specifies the non-cryptographic hash algorithm to apply. When the USING
clause is absent, the legacy PJW algorithm is applied; this is identical to its behaviour in previous
Firebird versions.
This function fully supports text BLOBs of any length and character set.
Supported algorithms
not specified
When no algorithm is specified, Firebird applies the 64-bit variant of the non-cryptographic PJW
hash function (also known as ELF64). This is a fast algorithm for general purposes (hash tables,
etc.), but its collision quality is suboptimal. Other hash functions — specified explicitly in the
USING clause, or cryptographic hashes through CRYPT_HASH() — should be used for more reliable
hashing.
The HASH function returns BIGINT for this algorithm
CRC32
With CRC32, Firebird applies the CRC32 algorithm using the polynomial 0x04C11DB7.
The HASH function returns INTEGER for this algorithm.
Examples of HASH
1. Hashing x with the CRC32 algorithm
Chapter 8. Built-in Scalar Functions
438

<!-- Original PDF Page: 440 -->

select hash(x using crc32) from y;
2. Hashing x with the legacy PJW algorithm
select hash(x) from y;
See also
CRYPT_HASH()
8.3.10. HEX_DECODE()
Decode a hexadecimal string to binary
Result type
VARBINARY or BLOB
Syntax
HEX_DECODE (hex_data)
Table 156. HEX_DECODE Function Parameter
Parameter Description
hex_data Hex encoded data
HEX_DECODE decodes a string with hex-encoded data, and returns the decoded value as VARBINARY or
BLOB as appropriate for the input. If the length of the type of hex_data is not a multiple of 2, an error
is raised at prepare time. If the length of the value of hex_data is not a multiple of 2, an error is
raised at execution time.
When the input is not BLOB, the length of the resulting type is calculated as type_length / 2, where
type_length is the maximum length in characters of the input type.
Example of HEX_DECODE
select cast(hex_decode('48657861646563696D616C') as varchar(12))
from rdb$database;
CAST
============
Hexadecimal
See also
HEX_ENCODE(), BASE64_DECODE()
Chapter 8. Built-in Scalar Functions
439

<!-- Original PDF Page: 441 -->

8.3.11. HEX_ENCODE()
Encodes a (binary) value to a hexadecimal string
Result type
VARCHAR CHARACTER SET ASCII or BLOB SUB_TYPE TEXT CHARACTER SET ASCII
Syntax
HEX_ENCODE (binary_data)
Table 157. HEX_ENCODE Function Parameter
Parameter Description
binary_data Binary data (or otherwise convertible to binary) to encode
HEX_ENCODE encodes binary_data with hex, and returns the encoded value as a VARCHAR CHARACTER SET
ASCII or BLOB SUB_TYPE TEXT CHARACTER SET ASCII as appropriate for the input.
When the input is not BLOB, the length of the resulting type is calculated as type_length * 2, where
type_length is the maximum length in bytes of the input type. If this length exceeds the maximum
length of VARCHAR, the function returns a BLOB.
Example of HEX_ENCODE
select hex_encode('Hexadecimal')
from rdb$database;
HEX_ENCODE
======================
48657861646563696D616C
See also
HEX_DECODE(), BASE64_ENCODE()
8.3.12. LEFT()
Extracts the leftmost part of a string
Result type
VARCHAR or BLOB
Syntax
LEFT (string, length)
Table 158. LEFT Function Parameters
Chapter 8. Built-in Scalar Functions
440

<!-- Original PDF Page: 442 -->

Parameter Description
string An expression of a string type
length Integer expression. The number of characters to return
• This function fully supports text BLOBs of any length, including those with a multi-byte character
set.
• If string is a BLOB, the result is a BLOB. Otherwise, the result is a VARCHAR(n) with n the length of
the input string.
• If the length argument exceeds the string length, the input string is returned unchanged.
• If the length argument is not a whole number, bankers' rounding (round-to-even) is applied, i.e.
0.5 becomes 0, 1.5 becomes 2, 2.5 becomes 2, 3.5 becomes 4, etc.
See also
RIGHT()
8.3.13. LOWER()
Converts a string to lowercase
Result type
(VAR)CHAR, (VAR)BINARY or BLOB
Syntax
LOWER (string)
Table 159. LOWER Function ParameterS
Parameter Description
string An expression of a string type
Returns the lowercase equivalent of the input string. The exact result depends on the character set.
With ASCII or NONE for instance, only ASCII characters are lowercased; with character set OCTETS
/(VAR)BINARY, the entire string is returned unchanged.
LOWER Examples
select Sheriff from Towns
  where lower(Name) = 'cooper''s valley'
See also
UPPER()
Chapter 8. Built-in Scalar Functions
441

<!-- Original PDF Page: 443 -->

8.3.14. LPAD()
Left-pads a string
Result type
VARCHAR or BLOB
Syntax
LPAD (str, endlen [, padstr])
Table 160. LPAD Function Parameters
Parameter Description
str An expression of a string type
endlen Output string length
padstr The character or string to be used to pad the source string up to the
specified length. Default is space (“' '”)
Left-pads a string with spaces or with a user-supplied string until a given length is reached.
• This function fully supports text BLOBs of any length and character set.
• If str is a BLOB, the result is a BLOB. Otherwise, the result is a VARCHAR(endlen).
• If padstr is given and equal to '' (empty string), no padding takes place.
• If endlen is less than the current string length, the string is truncated to endlen, even if padstr is
the empty string.

When used on a BLOB, this function may need to load the entire object into memory.
Although it does try to limit memory consumption, this may affect performance if
huge BLOBs are involved.
LPAD Examples
lpad ('Hello', 12)               -- returns '       Hello'
lpad ('Hello', 12, '-')          -- returns '-------Hello'
lpad ('Hello', 12, '')           -- returns 'Hello'
lpad ('Hello', 12, 'abc')        -- returns 'abcabcaHello'
lpad ('Hello', 12, 'abcdefghij') -- returns 'abcdefgHello'
lpad ('Hello', 2)                -- returns 'He'
lpad ('Hello', 2, '-')           -- returns 'He'
lpad ('Hello', 2, '')            -- returns 'He'
See also
RPAD()
Chapter 8. Built-in Scalar Functions
442

<!-- Original PDF Page: 444 -->

8.3.15. OCTET_LENGTH()
Length in bytes (octets) of a string or binary value
Result type
INTEGER, or BIGINT for BLOB
Syntax
OCTET_LENGTH (string)
Table 161. OCTET_LENGTH Function Parameter
Parameter Description
string An expression of a string type
Gives the length in bytes (octets) of the input string. For multi-byte character sets, this may be less
than the number of characters times the “formal” number of bytes per character as found in
RDB$CHARACTER_SETS.

With arguments of type CHAR or BINARY, this function takes the entire formal string
length (i.e. the declared length of a field or variable) into account. If you want to
obtain the “logical” byte length, not counting the trailing spaces, right- TRIM the
argument before passing it to OCTET_LENGTH.
OCTET_LENGTH Examples
select octet_length('Hello!') from rdb$database
-- returns 6
select octet_length(_iso8859_1 'Grüß di!') from rdb$database
-- returns 8: ü and ß take up one byte each in ISO8859_1
select octet_length
  (cast (_iso8859_1 'Grüß di!' as varchar(24) character set utf8))
from rdb$database
-- returns 10: ü and ß take up two bytes each in UTF8
select octet_length
  (cast (_iso8859_1 'Grüß di!' as char(24) character set utf8))
from rdb$database
-- returns 26: all 24 CHAR positions count, and two of them are 2-byte
See also
BIT_LENGTH(), CHAR_LENGTH(), CHARACTER_LENGTH()
Chapter 8. Built-in Scalar Functions
443

<!-- Original PDF Page: 445 -->

8.3.16. OVERLAY()
Overwrites part of, or inserts into, a string
Result type
VARCHAR or BLOB
Syntax
OVERLAY (string PLACING replacement FROM pos [FOR length])
Table 162. OVERLAY Function Parameters
Parameter Description
string The string into which the replacement takes place
replacement Replacement string
pos The position from which replacement takes place (starting position)
length The number of characters that are to be overwritten
By default, the number of characters removed from (overwritten in) the host string equals the
length of the replacement string. With the optional fourth argument, a different number of
characters can be specified for removal.
• This function supports BLOBs of any length.
• If string or replacement is a BLOB, the result is a BLOB. Otherwise, the result is a VARCHAR(n) with n
the sum of the lengths of string and replacement.
• As usual in SQL string functions, pos is 1-based.
• If pos is beyond the end of string, replacement is placed directly after string.
• If the number of characters from pos to the end of string is smaller than the length of
replacement (or than the length argument, if present), string is truncated at pos and replacement
placed after it.
• The effect of a “FOR 0” clause is that replacement is inserted into string.
• If any argument is NULL, the result is NULL.
• If pos or length is not a whole number, bankers' rounding (round-to-even) is applied, i.e. 0.5
becomes 0, 1.5 becomes 2, 2.5 becomes 2, 3.5 becomes 4, etc.
 When used on a BLOB, this function may need to load the entire object into memory.
This may affect performance if huge BLOBs are involved.
OVERLAY Examples
overlay ('Goodbye' placing 'Hello' from 2)   -- returns 'GHelloe'
overlay ('Goodbye' placing 'Hello' from 5)   -- returns 'GoodHello'
overlay ('Goodbye' placing 'Hello' from 8)   -- returns 'GoodbyeHello'
Chapter 8. Built-in Scalar Functions
444

<!-- Original PDF Page: 446 -->

overlay ('Goodbye' placing 'Hello' from 20)  -- returns 'GoodbyeHello'
overlay ('Goodbye' placing 'Hello' from 2 for 0) -- r. 'GHellooodbye'
overlay ('Goodbye' placing 'Hello' from 2 for 3) -- r. 'GHellobye'
overlay ('Goodbye' placing 'Hello' from 2 for 6) -- r. 'GHello'
overlay ('Goodbye' placing 'Hello' from 2 for 9) -- r. 'GHello'
overlay ('Goodbye' placing '' from 4)        -- returns 'Goodbye'
overlay ('Goodbye' placing '' from 4 for 3)  -- returns 'Gooe'
overlay ('Goodbye' placing '' from 4 for 20) -- returns 'Goo'
overlay ('' placing 'Hello' from 4)          -- returns 'Hello'
overlay ('' placing 'Hello' from 4 for 0)    -- returns 'Hello'
overlay ('' placing 'Hello' from 4 for 20)   -- returns 'Hello'
See also
REPLACE()
8.3.17. POSITION()
Finds the position of the first or next occurrence of a substring in a string
Result type
INTEGER
Syntax
  POSITION (substr IN string)
| POSITION (substr, string [, startpos])
Table 163. POSITION Function Parameters
Parameter Description
substr The substring whose position is to be searched for
string The string which is to be searched
startpos The position in string where the search is to start
Returns the (1-based) position of the first occurrence of a substring in a host string. With the
optional third argument, the search starts at a given offset, disregarding any matches that may
occur earlier in the string. If no match is found, the result is 0.

• The optional third argument is only supported in the second syntax (comma
syntax).
• The empty string is considered a substring of every string. Therefore, if substr
is '' (empty string) and string is not NULL, the result is:
◦ 1 if startpos is not given;
Chapter 8. Built-in Scalar Functions
445

<!-- Original PDF Page: 447 -->

◦ startpos if startpos lies within string;
◦ 0 if startpos lies beyond the end of string.
• This function fully supports text BLOBs of any size and character set.
 When used on a BLOB, this function may need to load the entire object into memory.
This may affect performance if huge BLOBs are involved.
POSITION Examples
position ('be' in 'To be or not to be')   -- returns 4
position ('be', 'To be or not to be')     -- returns 4
position ('be', 'To be or not to be', 4)  -- returns 4
position ('be', 'To be or not to be', 8)  -- returns 17
position ('be', 'To be or not to be', 18) -- returns 0
position ('be' in 'Alas, poor Yorick!')   -- returns 0
See also
SUBSTRING()
8.3.18. REPLACE()
Replaces all occurrences of a substring in a string
Result type
VARCHAR or BLOB
Syntax
REPLACE (str, find, repl)
Table 164. REPLACE Function Parameters
Parameter Description
str The string in which the replacement is to take place
find The string to search for
repl The replacement string
• This function fully supports text BLOBs of any length and character set.
• If any argument is a BLOB, the result is a BLOB. Otherwise, the result is a VARCHAR(n) with n
calculated from the lengths of str, find and repl in such a way that even the maximum possible
number of replacements won’t overflow the field.
• If find is the empty string, str is returned unchanged.
• If repl is the empty string, all occurrences of find are deleted from str.
• If any argument is NULL, the result is always NULL, even if nothing would have been replaced.
Chapter 8. Built-in Scalar Functions
446

<!-- Original PDF Page: 448 -->

 When used on a BLOB, this function may need to load the entire object into memory.
This may affect performance if huge BLOBs are involved.
REPLACE Examples
replace ('Billy Wilder',  'il', 'oog') -- returns 'Boogly Woogder'
replace ('Billy Wilder',  'il',    '') -- returns 'Bly Wder'
replace ('Billy Wilder',  null, 'oog') -- returns NULL
replace ('Billy Wilder',  'il',  null) -- returns NULL
replace ('Billy Wilder', 'xyz',  null) -- returns NULL (!)
replace ('Billy Wilder', 'xyz', 'abc') -- returns 'Billy Wilder'
replace ('Billy Wilder',    '', 'abc') -- returns 'Billy Wilder'
See also
OVERLAY(), SUBSTRING(), POSITION(), CHAR_LENGTH(), CHARACTER_LENGTH()
8.3.19. REVERSE()
Reverses a string
Result type
VARCHAR
Syntax
REVERSE (string)
Table 165. REVERSE Function Parameter
Parameter Description
string An expression of a string type
REVERSE Examples
reverse ('spoonful')            -- returns 'lufnoops'
reverse ('Was it a cat I saw?') -- returns '?was I tac a ti saW'

This function is useful if you want to group, search or order on string endings, e.g.
when dealing with domain names or email addresses:
create index ix_people_email on people
  computed by (reverse(email));
select * from people
  where reverse(email) starting with reverse('.br');
Chapter 8. Built-in Scalar Functions
447

<!-- Original PDF Page: 449 -->

8.3.20. RIGHT()
Extracts the rightmost part of a string
Result type
VARCHAR or BLOB
Syntax
RIGHT (string, length)
Table 166. RIGHT Function Parameters
Parameter Description
string An expression of a string type
length Integer. The number of characters to return
• This function supports text BLOBs of any length.
• If string is a BLOB, the result is a BLOB. Otherwise, the result is a VARCHAR(n) with n the length of
the input string.
• If the length argument exceeds the string length, the input string is returned unchanged.
• If the length argument is not a whole number, bankers' rounding (round-to-even) is applied, i.e.
0.5 becomes 0, 1.5 becomes 2, 2.5 becomes 2, 3.5 becomes 4, etc.
 When used on a BLOB, this function may need to load the entire object into memory.
This may affect performance if huge BLOBs are involved.
See also
LEFT(), SUBSTRING()
8.3.21. RPAD()
Right-pads a string
Result type
VARCHAR or BLOB
Syntax
RPAD (str, endlen [, padstr])
Table 167. RPAD Function Parameters
Parameter Description
str An expression of a string type
endlen Output string length
Chapter 8. Built-in Scalar Functions
448

<!-- Original PDF Page: 450 -->

Parameter Description
endlen The character or string to be used to pad the source string up to the
specified length. Default is space (' ')
Right-pads a string with spaces or with a user-supplied string until a given length is reached.
• This function fully supports text BLOBs of any length and character set.
• If str is a BLOB, the result is a BLOB. Otherwise, the result is a VARCHAR(endlen).
• If padstr is given and equals '' (empty string), no padding takes place.
• If endlen is less than the current string length, the string is truncated to endlen, even if padstr is
the empty string.

When used on a BLOB, this function may need to load the entire object into memory.
Although it does try to limit memory consumption, this may affect performance if
huge BLOBs are involved.
RPAD Examples
rpad ('Hello', 12)               -- returns 'Hello       '
rpad ('Hello', 12, '-')          -- returns 'Hello-------'
rpad ('Hello', 12, '')           -- returns 'Hello'
rpad ('Hello', 12, 'abc')        -- returns 'Helloabcabca'
rpad ('Hello', 12, 'abcdefghij') -- returns 'Helloabcdefg'
rpad ('Hello', 2)                -- returns 'He'
rpad ('Hello', 2, '-')           -- returns 'He'
rpad ('Hello', 2, '')            -- returns 'He'
See also
LPAD()
8.3.22. SUBSTRING()
Extracts a substring by position and length, or by SQL regular expression
Result types
VARCHAR or BLOB
Syntax
SUBSTRING ( <substring-args> )
<substring-args> ::=
    str FROM startpos [FOR length]
  | str SIMILAR <similar-pattern> ESCAPE <escape>
<similar-pattern> ::=
  <similar-pattern-R1>
Chapter 8. Built-in Scalar Functions
449

<!-- Original PDF Page: 451 -->

<escape> " <similar-pattern-R2> <escape> "
  <similar-pattern-R3>
Table 168. SUBSTRING Function Parameters
Parameter Description
str An expression of a string type
startpos Integer expression, the position from which to start retrieving the
substring
length The number of characters to retrieve after the startpos
similar-pattern SQL regular expression pattern to search for the substring
escape Escape character
Returns a string’s substring starting at the given position, either to the end of the string or with a
given length, or extracts a substring using an SQL regular expression pattern.
If any argument is NULL, the result is also NULL.

When used on a BLOB, this function may need to load the entire object into memory.
Although it does try to limit memory consumption, this may affect performance if
huge BLOBs are involved.
Positional SUBSTRING
In its simple, positional form (with FROM), this function returns the substring starting at character
position startpos (the first character being 1). Without the FOR argument, it returns all the
remaining characters in the string. With FOR, it returns length characters or the remainder of the
string, whichever is shorter.
When startpos is smaller than 1, substring behaves as if the string has 1 - startpos extra positions
before the actual first character at position 1. The length is considered from this imaginary start of
the string, so the resulting string could be shorter than the specified length, or even empty.
The function fully supports binary and text BLOBs of any length, and with any character set. If str is
a BLOB, the result is also a BLOB. For any other argument type, the result is a VARCHAR.
For non-BLOB arguments, the width of the result field is always equal to the length of str, regardless
of startpos and length. So, substring('pinhead' from 4 for 2)  will return a VARCHAR(7) containing
the string 'he'.
Example
insert into AbbrNames(AbbrName)
  select substring(LongName from 1 for 3) from LongNames;
select substring('abcdef' from 1 for 2) from rdb$database;
-- result: 'ab'
Chapter 8. Built-in Scalar Functions
450

<!-- Original PDF Page: 452 -->

select substring('abcdef' from 2) from rdb$database;
-- result: 'bcdef'
select substring('abcdef' from 0 for 2) from rdb$database;
-- result: 'a'
-- and NOT 'ab', because there is "nothing" at position 0
select substring('abcdef' from -5 for 2) from rdb$database;
-- result: ''
-- length ends before the actual start of the string
Regular Expression SUBSTRING
In the regular expression form (with SIMILAR), the SUBSTRING function returns part of the string
matching an SQL regular expression pattern. If no match is found, NULL is returned.
The SIMILAR pattern is formed from three SQL regular expression patterns, R1, R2 and R3. The
entire pattern takes the form of R1 || '<escape>"' || R2 || ' <escape>"' || R3, where <escape> is
the escape character defined in the ESCAPE clause. R2 is the pattern that matches the substring to
extract, and is enclosed between escaped double quotes ( <escape>", e.g. “#"” with escape character ‘
#’). R1 matches the prefix of the string, and R3 the suffix of the string. Both R1 and R3 are optional
(they can be empty), but the pattern must match the entire string. In other words, it is not sufficient
to specify a pattern that only finds the substring to extract.

The escaped double quotes around R2 can be compared to defining a single
capture group in more common regular expression syntax like PCRE. That is, the
full pattern is equivalent to R1(R2)R3, which must match the entire input string,
and the capture group is the substring to be returned.
 If any one of R1, R2, or R3 is not a zero-length string and does not have the format
of an SQL regular expression, then an exception is raised.
The full SQL regular expression format is described in Syntax: SQL Regular Expressions
Examples
substring('abcabc' similar 'a#"bcab#"c' escape '#')  -- bcab
substring('abcabc' similar 'a#"%#"c' escape '#')     -- bcab
substring('abcabc' similar '_#"%#"_' escape '#')     -- bcab
substring('abcabc' similar '#"(abc)*#"' escape '#')  -- abcabc
substring('abcabc' similar '#"abc#"' escape '#')     -- <null>
See also
POSITION(), LEFT(), RIGHT(), CHAR_LENGTH(), CHARACTER_LENGTH(), SIMILAR TO
8.3.23. TRIM()
Trims leading and/or trailing spaces or other substrings from a string
Chapter 8. Built-in Scalar Functions
451

<!-- Original PDF Page: 453 -->

Result type
VARCHAR or BLOB
Syntax
TRIM ([<adjust>] str)
<adjust> ::=  {[<where>] [what]} FROM
<where> ::=  BOTH | LEADING | TRAILING
Table 169. TRIM Function Parameters
Parameter Description
str An expression of a string type
where The position the substring is to be removed from — BOTH | LEADING |
TRAILING. BOTH is the default
what The substring that should be removed (multiple times if there are several
matches) from the beginning, the end, or both sides of the input string str.
By default, it is space (' ')
Removes leading and/or trailing spaces (or optionally other strings) from the input string.
 If str is a BLOB, the result is a BLOB. Otherwise, it is a VARCHAR(n) with n the formal
length of str.
 When used on a BLOB, this function may need to load the entire object into memory.
This may affect performance if huge BLOBs are involved.
TRIM Examples
select trim ('  Waste no space   ') from rdb$database
-- returns 'Waste no space'
select trim (leading from '  Waste no space   ') from rdb$database
-- returns 'Waste no space   '
select trim (leading '.' from '  Waste no space   ') from rdb$database
-- returns '  Waste no space   '
select trim (trailing '!' from 'Help!!!!') from rdb$database
-- returns 'Help'
select trim ('la' from 'lalala I love you Ella') from rdb$database
-- returns ' I love you El'
select trim ('la' from 'Lalala I love you Ella') from rdb$database
Chapter 8. Built-in Scalar Functions
452

<!-- Original PDF Page: 454 -->

-- returns 'Lalala I love you El'
8.3.24. UNICODE_CHAR()
Character from Unicode code point
Result type
CHAR(1) CHARACTER SET UTF8
Syntax
UNICODE_CHAR (code)
Table 170. UNICODE_CHAR Function Parameter
Parameter Description
code The Unicode code point (range 0…0x10FFFF)
Returns the character corresponding to the Unicode code point passed in the argument.
See also
UNICODE_VAL(), ASCII_CHAR()
8.3.25. UNICODE_VAL()
Unicode code point from string
Result type
INTEGER
Syntax
UNICODE_VAL (ch)
Table 171. UNICODE_VAL Function Parameter
Parameter Description
ch A string of the [VAR]CHAR data type or a text BLOB
Returns the Unicode code point (range 0…0x10FFFF) of the character passed in.
• If the argument is a string with more than one character, the Unicode code point of the first
character is returned.
• If the argument is an empty string, 0 is returned.
• If the argument is NULL, NULL is returned.
See also
UNICODE_CHAR(), ASCII_VAL()
Chapter 8. Built-in Scalar Functions
453

<!-- Original PDF Page: 455 -->

8.3.26. UPPER()
Converts a string to uppercase
Result type
(VAR)CHAR, (VAR)BINARY or BLOB
Syntax
UPPER (str)
Table 172. UPPER Function Parameter
Parameter Description
str An expression of a string type
Returns the uppercase equivalent of the input string. The exact result depends on the character set.
With ASCII or NONE for instance, only ASCII characters are uppercased; with character set OCTETS
/(VAR)BINARY, the entire string is returned unchanged.
UPPER Examples
select upper(_iso8859_1 'Débâcle')
from rdb$database
-- returns 'DÉBÂCLE'
select upper(_iso8859_1 'Débâcle' collate fr_fr)
from rdb$database
-- returns 'DEBACLE', following French uppercasing rules
See also
LOWER()
8.4. Date and Time Functions
8.4.1. DATEADD()
Adds or subtracts datetime units from a datetime value
Result type
DATE, TIME or TIMESTAMP
Syntax
DATEADD (<args>)
<args> ::=
    <amount> <unit> TO <datetime>
Chapter 8. Built-in Scalar Functions
454

<!-- Original PDF Page: 456 -->

| <unit>, <amount>, <datetime>
<amount> ::= an integer expression (negative to subtract)
<unit> ::=
    YEAR | MONTH | WEEK | DAY
  | HOUR | MINUTE | SECOND | MILLISECOND
<datetime> ::= a DATE, TIME or TIMESTAMP expression
Table 173. DATEADD Function Parameters
Parameter Description
amount An integer expression of the SMALLINT, INTEGER or BIGINT type. For unit
MILLISECOND, the type is NUMERIC(18, 1). A negative value is subtracted.
unit Date/time unit
datetime An expression of the DATE, TIME or TIMESTAMP type
Adds the specified number of years, months, weeks, days, hours, minutes, seconds or milliseconds
to a date/time value.
• The result type is determined by the third argument.
• With TIMESTAMP and DATE arguments, all units can be used.
• With TIME arguments, only HOUR, MINUTE, SECOND and MILLISECOND can be used.
Examples of DATEADD
dateadd (28 day to current_date)
dateadd (-6 hour to current_time)
dateadd (month, 9, DateOfConception)
dateadd (-38 week to DateOfBirth)
dateadd (minute, 90, cast('now' as time))
dateadd (? year to date '11-Sep-1973')
select
  cast(dateadd(-1 * extract(millisecond from ts) millisecond to ts) as varchar(30)) as
t,
  extract(millisecond from ts) as ms
from (
  select timestamp '2014-06-09 13:50:17.4971' as ts
  from rdb$database
) a
T                        MS
------------------------ ------
2014-06-09 13:50:17.0000  497.1
Chapter 8. Built-in Scalar Functions
455

<!-- Original PDF Page: 457 -->

See also
DATEDIFF(), Operations Using Date and Time Values
8.4.2. DATEDIFF()
Difference between two datetime values in a datetime unit
Result type
BIGINT, or NUMERIC(18,1) for MILLISECOND
Syntax
DATEDIFF (<args>)
<args> ::=
    <unit> FROM <moment1> TO <moment2>
  | <unit>, <moment1>, <moment2>
<unit> ::=
    YEAR | MONTH | WEEK | DAY
  | HOUR | MINUTE | SECOND | MILLISECOND
<momentN> ::= a DATE, TIME or TIMESTAMP expression
Table 174. DATEDIFF Function Parameters
Parameter Description
unit Date/time unit
moment1 An expression of the DATE, TIME or TIMESTAMP type
moment2 An expression of the DATE, TIME or TIMESTAMP type
Returns the number of years, months, weeks, days, hours, minutes, seconds or milliseconds elapsed
between two date/time values.
• DATE and TIMESTAMP arguments can be combined. No other mixes are allowed.
• With TIMESTAMP and DATE arguments, all units can be used.
• With TIME arguments, only HOUR, MINUTE, SECOND and MILLISECOND can be used.
Computation
• DATEDIFF doesn’t look at any smaller units than the one specified in the first argument. As a
result,
◦ datediff (year, date '1-Jan-2009', date '31-Dec-2009') returns 0, but
◦ datediff (year, date '31-Dec-2009', date '1-Jan-2010') returns 1
• It does, however, look at all the bigger units. So:
◦ datediff (day, date '26-Jun-1908', date '11-Sep-1973') returns 23818
• A negative result value indicates that moment2 lies before moment1.
Chapter 8. Built-in Scalar Functions
456

<!-- Original PDF Page: 458 -->

DATEDIFF Examples
datediff (hour from current_timestamp to timestamp '12-Jun-2059 06:00')
datediff (minute from time '0:00' to current_time)
datediff (month, current_date, date '1-1-1900')
datediff (day from current_date to cast(? as date))
See also
DATEADD(), Operations Using Date and Time Values
8.4.3. EXTRACT()
Extracts a datetime unit from a datetime value
Result type
SMALLINT or NUMERIC
Syntax
EXTRACT (<part> FROM <datetime>)
<part> ::=
    YEAR | MONTH | QUARTER | WEEK
  | DAY | WEEKDAY | YEARDAY
  | HOUR | MINUTE | SECOND | MILLISECOND
  | TIMEZONE_HOUR | TIMEZONE_MINUTE
<datetime> ::= a DATE, TIME or TIMESTAMP expression
Table 175. EXTRACT Function Parameters
Parameter Description
part Date/time unit
datetime An expression of the DATE, TIME or TIMESTAMP type
Extracts and returns an element from a DATE, TIME or TIMESTAMP expression.
Returned Data Types and Ranges
The returned data types and possible ranges are shown in the table below. If you try to extract a
part that isn’t present in the date/time argument (e.g. SECOND from a DATE or YEAR from a TIME), an
error occurs.
Table 176. Types and ranges of EXTRACT results
Part Type Range Comment
YEAR SMALLINT 1-9999  
MONTH SMALLINT 1-12  
Chapter 8. Built-in Scalar Functions
457

<!-- Original PDF Page: 459 -->

Part Type Range Comment
QUARTER SMALLINT 1-4  
WEEK SMALLINT 1-53  
DAY SMALLINT 1-31  
WEEKDAY SMALLINT 0-6 0 = Sunday
YEARDAY SMALLINT 0-365 0 = January 1
HOUR SMALLINT 0-23  
MINUTE SMALLINT 0-59  
SECOND NUMERIC(9,4) 0.0000-59.9999 includes millisecond as fraction
MILLISECOND NUMERIC(9,1) 0.0-999.9  
TIMEZONE_HOUR SMALLINT -23 - +23  
TIMEZONE_MINUTE SMALLINT -59 - +59  
MILLISECOND
Extracts the millisecond value from a TIME or TIMESTAMP. The data type returned is NUMERIC(9,1).

If you extract the millisecond from CURRENT_TIME, be aware that this variable
defaults to seconds precision, so the result will always be 0. Extract from
CURRENT_TIME(3) or CURRENT_TIMESTAMP to get milliseconds precision.
WEEK
Extracts the ISO-8601 week number from a DATE or TIMESTAMP. ISO-8601 weeks start on a Monday
and always have the full seven days. Week 1 is the first week that has a majority (at least 4) of its
days in the new year. The first 1-3 days of the year may belong to the last week (52 or 53) of the
previous year. Likewise, a year’s final 1-3 days may belong to week 1 of the following year.

Be careful when combining WEEK and YEAR results. For instance, 30 December 2008
lies in week 1 of 2009, so extract(week from date '30 Dec 2008')  returns 1.
However, extracting YEAR always gives the calendar year, which is 2008. In this
case, WEEK and YEAR are at odds with each other. The same happens when the first
days of January belong to the last week of the previous year.
Please also notice that WEEKDAY is not ISO-8601 compliant: it returns 0 for Sunday,
whereas ISO-8601 specifies 7.
See also
Data Types for Dates and Times
8.4.4. FIRST_DAY()
Returns the first day of a time period containing a datetime value
Result Type
Chapter 8. Built-in Scalar Functions
458

<!-- Original PDF Page: 460 -->

DATE, TIMESTAMP (with or without time zone)
Syntax
FIRST_DAY(OF <period> FROM date_or_timestamp)
<period> ::= YEAR | MONTH | QUARTER | WEEK
Table 177. FIRST_DAY Function Parameters
Parameter Description
date_or_timestamp Expression of type DATE, TIMESTAMP WITHOUT TIME ZONE or TIMESTAMP WITH
TIME ZONE
FIRST_DAY returns a date or timestamp (same as the type of date_or_timestamp) with the first day of
the year, month or week of a given date or timestamp value.

• The first day of the week is considered as Sunday, following the same rules as
for EXTRACT() with WEEKDAY.
• When a timestamp is passed, the return value preserves the time part.
Examples of FIRST_DAY
select
  first_day(of month from current_date),
  first_day(of year from current_timestamp),
  first_day(of week from date '2017-11-01'),
  first_day(of quarter from date '2017-11-01')
from rdb$database;
8.4.5. LAST_DAY()
Returns the last day of a time period containing a datetime value
Result Type
DATE, TIMESTAMP (with or without time zone)
Syntax
LAST_DAY(OF <period> FROM date_or_timestamp)
<period> ::= YEAR | MONTH | QUARTER | WEEK
Table 178. LAST_DAY Function Parameters
Chapter 8. Built-in Scalar Functions
459

<!-- Original PDF Page: 461 -->

Parameter Description
date_or_timestamp Expression of type DATE, TIMESTAMP WITHOUT TIME ZONE or TIMESTAMP WITH
TIME ZONE
LAST_DAY returns a date or timestamp (same as the type of date_or_timestamp) with the last day of
the year, month or week of a given date or timestamp value.

• The last day of the week is considered as Saturday, following the same rules as
for EXTRACT() with WEEKDAY.
• When a timestamp is passed, the return value preserves the time part.
Examples of LAST_DAY
select
  last_day(of month from current_date),
  last_day(of year from current_timestamp),
  last_day(of week from date '2017-11-01'),
  last_day(of quarter from date '2017-11-01')
from rdb$database;
8.5. Type Casting Functions
8.5.1. CAST()
Converts a value from one data type to another
Result type
As specified by target_type
Syntax
CAST (<expression> AS <target_type>)
<target_type> ::= <domain_or_non_array_type> | <array_datatype>
<domain_or_non_array_type> ::=
  !! See Scalar Data Types Syntax !!
<array_datatype> ::=
  !! See Array Data Types Syntax !!
Table 179. CAST Function Parameters
Parameter Description
expression SQL expression
sql_datatype SQL data type
Chapter 8. Built-in Scalar Functions
460

<!-- Original PDF Page: 462 -->

CAST converts an expression to the desired data type or domain. If the conversion is not possible, an
error is raised.
“Shorthand” Syntax
Alternative syntax, supported only when casting a string literal to a DATE, TIME or TIMESTAMP:
datatype 'date/timestring'
This syntax was already available in InterBase, but was never properly documented. In the SQL
standard, this feature is called “datetime literals”.

Since Firebird 4.0, the use of 'NOW', 'YESTERDAY' and 'TOMORROW' in the shorthand
cast is no longer allowed; only literals defining a fixed moment in time are
supported.
Allowed Type Conversions
The following table shows the type conversions possible with CAST.
Table 180. Possible Type-castings
with CAST
From To
Numeric types Numeric types
[VAR]CHAR
BLOB
[VAR]CHAR
BLOB
[VAR]CHAR
BLOB
Numeric types
DATE
TIME
TIMESTAMP
DATE
TIME
[VAR]CHAR
BLOB
TIMESTAMP
TIMESTAMP [VAR]CHAR
BLOB
DATE
TIME
Keep in mind that sometimes information is lost, for instance when you cast a TIMESTAMP to a DATE.
Also, the fact that types are CAST-compatible is in itself no guarantee that a conversion will succeed.
“CAST(123456789 as SMALLINT) ” will definitely result in an error, as will “ CAST('Judgement Day' as
DATE)”.
Chapter 8. Built-in Scalar Functions
461

<!-- Original PDF Page: 463 -->

Casting Parameters
You can also cast statement parameters to a data type:
cast (? as integer)
This gives you control over the type of the parameter set up by the engine. Please notice that with
statement parameters, you always need a full-syntax cast — shorthand casts are not supported.
Casting to a Domain or its Type
Casting to a domain or its base type are supported. When casting to a domain, any constraints ( NOT
NULL and/or CHECK) declared for the domain must be satisfied, or the cast will fail. Please be aware
that a CHECK passes if it evaluates to TRUE or NULL! So, given the following statements:
create domain quint as int check (value >= 5000);
select cast (2000 as quint) from rdb$database;     ①
select cast (8000 as quint) from rdb$database;     ②
select cast (null as quint) from rdb$database;     ③
only cast number 1 will result in an error.
When the TYPE OF modifier is used, the expression is cast to the base type of the domain, ignoring
any constraints. With domain quint defined as above, the following two casts are equivalent and
will both succeed:
select cast (2000 as type of quint) from rdb$database;
select cast (2000 as int) from rdb$database;
If TYPE OF is used with a (VAR)CHAR type, its character set and collation are retained:
create domain iso20 varchar(20) character set iso8859_1;
create domain dunl20 varchar(20) character set iso8859_1 collate du_nl;
create table zinnen (zin varchar(20));
commit;
insert into zinnen values ('Deze');
insert into zinnen values ('Die');
insert into zinnen values ('die');
insert into zinnen values ('deze');
select cast(zin as type of iso20) from zinnen order by 1;
-- returns Deze -> Die -> deze -> die
select cast(zin as type of dunl20) from zinnen order by 1;
-- returns deze -> Deze -> die -> Die
Chapter 8. Built-in Scalar Functions
462

<!-- Original PDF Page: 464 -->


If a domain’s definition is changed, existing CASTs to that domain or its type may
become invalid. If these CASTs occur in PSQL modules, their invalidation may be
detected. See the note The RDB$VALID_BLR field, in Appendix A.
Casting to a Column’s Type
It is also possible to cast expressions to the type of an existing table or view column. Only the type
itself is used; in the case of string types, this includes the character set but not the collation.
Constraints and default values of the source column are not applied.
create table ttt (
  s varchar(40) character set utf8 collate unicode_ci_ai
);
commit;
select cast ('Jag har många vänner' as type of column ttt.s)
from rdb$database;

Warnings
If a column’s definition is altered, existing CASTs to that column’s type may become
invalid. If these CASTs occur in PSQL modules, their invalidation may be detected.
See the note The RDB$VALID_BLR field, in Appendix A.
Cast Examples
A full-syntax cast:
select cast ('12' || '-June-' || '1959' as date) from rdb$database
A shorthand string-to-date cast:
update People set AgeCat = 'Old'
  where BirthDate < date '1-Jan-1943'
Notice that you can drop even the shorthand cast from the example above, as the engine will
understand from the context (comparison to a DATE field) how to interpret the string:
update People set AgeCat = 'Old'
  where BirthDate < '1-Jan-1943'
However, this is not always possible. The cast below cannot be dropped, otherwise the engine
would find itself with an integer to be subtracted from a string:
Chapter 8. Built-in Scalar Functions
463

<!-- Original PDF Page: 465 -->

select cast('today' as date) - 7 from rdb$database
8.6. Bitwise Functions
8.6.1. BIN_AND()
Bitwise AND
Result type
integer type (the widest type of the arguments)
 SMALLINT result is returned only if all the arguments are explicit SMALLINTs or
NUMERIC(n, 0) with n <= 4; otherwise small integers return an INTEGER result.
Syntax
BIN_AND (number, number [, number ...])
Table 181. BIN_AND Function Parameters
Parameter Description
number A number of an integer type
Returns the result of the bitwise AND operation on the argument(s).
See also
BIN_OR(), BIN_XOR()
8.6.2. BIN_NOT()
Bitwise NOT
Result type
integer type matching the argument
 SMALLINT result is returned only if all the arguments are explicit SMALLINTs or
NUMERIC(n, 0) with n <= 4; otherwise small integers return an INTEGER result.
Syntax
BIN_NOT (number)
Table 182. BIN_NOT Function Parameter
Parameter Description
number A number of an integer type
Chapter 8. Built-in Scalar Functions
464

<!-- Original PDF Page: 466 -->

Returns the result of the bitwise NOT operation on the argument, i.e. one’s complement.
See also
BIN_OR(), BIN_XOR() and others in this set.
8.6.3. BIN_OR()
Bitwise OR
Result type
integer type (the widest type of the arguments)
 SMALLINT result is returned only if all the arguments are explicit SMALLINTs or
NUMERIC(n, 0) with n <= 4; otherwise small integers return an INTEGER result.
Syntax
BIN_OR (number, number [, number ...])
Table 183. BIN_OR Function Parameters
Parameter Description
number A number of an integer type
Returns the result of the bitwise OR operation on the argument(s).
See also
BIN_AND(), BIN_XOR()
8.6.4. BIN_SHL()
Bitwise left-shift
Result type
BIGINT or INT128 depending on the first argument
Syntax
BIN_SHL (number, shift)
Table 184. BIN_SHL Function Parameters
Parameter Description
number A number of an integer type
shift The number of bits the number value is shifted by
Returns the first argument bitwise left-shifted by the second argument, i.e. a << b or a·2b.
Chapter 8. Built-in Scalar Functions
465

<!-- Original PDF Page: 467 -->

See also
BIN_SHR()
8.6.5. BIN_SHR()
Bitwise right-shift with sign extension
Result type
BIGINT or INT128 depending on the first argument
Syntax
BIN_SHR (number, shift)
Table 185. BIN_SHR Function Parameters
Parameter Description
number A number of an integer type
shift The number of bits the number value is shifted by
Returns the first argument bitwise right-shifted by the second argument, i.e. a >> b or a/2b.
The operation performed is an arithmetic right shift (x86 SAR), meaning that the sign of the first
operand is always preserved.
See also
BIN_SHL()
8.6.6. BIN_XOR()
Bitwise XOR
Result type
integer type (the widest type of the arguments)
 SMALLINT result is returned only if all the arguments are explicit SMALLINTs or
NUMERIC(n, 0) with n <= 4; otherwise small integers return an INTEGER result.
Syntax
BIN_XOR (number, number [, number ...])
Table 186. BIN_XOR Function Parameters
Parameter Description
number A number of an integer type
Returns the result of the bitwise XOR operation on the argument(s).
Chapter 8. Built-in Scalar Functions
466

<!-- Original PDF Page: 468 -->

See also
BIN_AND(), BIN_OR()
8.7. UUID Functions
8.7.1. CHAR_TO_UUID()
Converts a string UUID to its binary representation
Result type
BINARY(16)
Syntax
CHAR_TO_UUID (ascii_uuid)
Table 187. CHAR_TO_UUID Function Parameter
Parameter Description
ascii_uuid A 36-character representation of UUID. ‘-’ (hyphen) in positions 9, 14, 19
and 24; valid hexadecimal digits in any other positions, e.g. 'A0bF4E45-
3029-2a44-D493-4998c9b439A3'
Converts a human-readable 36-char UUID string to the corresponding 16-byte UUID.
CHAR_TO_UUID Examples
select char_to_uuid('A0bF4E45-3029-2a44-D493-4998c9b439A3') from rdb$database
-- returns A0BF4E4530292A44D4934998C9B439A3 (16-byte string)
select char_to_uuid('A0bF4E45-3029-2A44-X493-4998c9b439A3') from rdb$database
-- error: -Human readable UUID argument for CHAR_TO_UUID must
--         have hex digit at position 20 instead of "X (ASCII 88)"
See also
UUID_TO_CHAR(), GEN_UUID()
8.7.2. GEN_UUID()
Generates a random binary UUID
Result type
BINARY(16)
Syntax
GEN_UUID ()
Chapter 8. Built-in Scalar Functions
467

<!-- Original PDF Page: 469 -->

Returns a universally unique ID as a 16-byte character string.
GEN_UUID Example
select gen_uuid() from rdb$database
-- returns e.g. 017347BFE212B2479C00FA4323B36320 (16-byte string)
See also
UUID_TO_CHAR(), CHAR_TO_UUID()
8.7.3. UUID_TO_CHAR()
Converts a binary UUID to its string representation
Result type
CHAR(36)
Syntax
UUID_TO_CHAR (uuid)
Table 188. UUID_TO_CHAR Function Parameters
Parameter Description
uuid 16-byte UUID
Converts a 16-byte UUID to its 36-character, human-readable ASCII representation.
UUID_TO_CHAR Examples
select uuid_to_char(x'876C45F4569B320DBCB4735AC3509E5F') from rdb$database
-- returns '876C45F4-569B-320D-BCB4-735AC3509E5F'
select uuid_to_char(gen_uuid()) from rdb$database
-- returns e.g. '680D946B-45FF-DB4E-B103-BB5711529B86'
select uuid_to_char('Firebird swings!') from rdb$database
-- returns '46697265-6269-7264-2073-77696E677321'
See also
CHAR_TO_UUID(), GEN_UUID()
8.8. Functions for Sequences (Generators)
Chapter 8. Built-in Scalar Functions
468

<!-- Original PDF Page: 470 -->

8.8.1. GEN_ID()
Increments a sequence (generator) value and returns its new value
Result type
BIGINT — dialect 2 and 3
INTEGER — dialect 1
Syntax
GEN_ID (generator-name, step)
Table 189. GEN_ID Function Parameters
Parameter Description
generator-name Identifier name of a generator (sequence)
step An integer expression of the increment
If step equals 0, the function will leave the value of the generator unchanged and return its current
value.
The SQL-compliant NEXT VALUE FOR syntax is preferred, except when an increment other than the
configured increment of the sequence is needed.

If the value of the step parameter is less than zero, it will decrease the value of the
generator. You should be cautious with such manipulations in the database, as
they could compromise data integrity (meaning, subsequent insert statements
could fail due to generating of duplicate id values).
 In dialect 1, the result type is INTEGER, in dialect 2 and 3 it is BIGINT.
GEN_ID Example
new.rec_id = gen_id(gen_recnum, 1);
See also
NEXT VALUE FOR, CREATE SEQUENCE (GENERATOR)
8.9. Conditional Functions
8.9.1. COALESCE()
Returns the first non-NULL argument
Result type
Depends on input
Chapter 8. Built-in Scalar Functions
469

<!-- Original PDF Page: 471 -->

Syntax
COALESCE (<exp1>, <exp2> [, <expN> ... ])
Table 190. COALESCE Function Parameters
Parameter Description
exp1, exp2 … expN A list of expressions of compatible types
The COALESCE function takes two or more arguments and returns the value of the first non- NULL
argument. If all the arguments evaluate to NULL, the result is NULL.
COALESCE Examples
This example picks the Nickname from the Persons table. If it happens to be NULL, it goes on to
FirstName. If that too is NULL, “'Mr./Mrs.'” is used. Finally, it adds the family name. All in all, it tries
to use the available data to compose a full name that is as informal as possible. This scheme only
works if absent nicknames and first names are NULL: if one of them is an empty string, COALESCE will
happily return that to the caller. That problem can be fixed by using NULLIF().
select
  coalesce (Nickname, FirstName, 'Mr./Mrs.') || ' ' || LastName
    as FullName
from Persons
See also
IIF(), NULLIF(), CASE
8.9.2. DECODE()
Shorthand “simple CASE”-equivalent function
Result type
Depends on input
Syntax
DECODE(<testexpr>,
  <expr1>, <result1>
  [<expr2>, <result2> ...]
  [, <defaultresult>])
Table 191. DECODE Function Parameters
Parameter Description
testexpr An expression of any compatible type that is compared to the expressions
expr1, expr2 … exprN
Chapter 8. Built-in Scalar Functions
470

<!-- Original PDF Page: 472 -->

Parameter Description
expr1, expr2, … exprN Expressions of any compatible types, to which the testexpr expression is
compared
result1, result2, …
resultN
Returned values of any type
defaultresult The expression to be returned if none of the conditions is met
DECODE is a shorthand for the so-called “simple CASE” construct , in which a given expression is
compared to a number of other expressions until a match is found. The result is determined by the
value listed after the matching expression. If no match is found, the default result is returned, if
present, otherwise NULL is returned.
The equivalent CASE construct:
CASE <testexpr>
  WHEN <expr1> THEN <result1>
  [WHEN <expr2> THEN <result2> ...]
  [ELSE <defaultresult>]
END
 Matching is done with the ‘ =’ operator, so if testexpr is NULL, it won’t match any of
the exprs, not even those that are NULL.
DECODE Examples
select name,
  age,
  decode(upper(sex),
         'M', 'Male',
         'F', 'Female',
         'Unknown'),
  religion
from people
See also
CASE, Simple CASE
8.9.3. IIF()
Ternary conditional function
Result type
Depends on input
Chapter 8. Built-in Scalar Functions
471

<!-- Original PDF Page: 473 -->

Syntax
IIF (<condition>, ResultT, ResultF)
Table 192. IIF Function Parameters
Parameter Description
condition A true|false expression
resultT The value returned if the condition is true
resultF The value returned if the condition is false
IIF takes three arguments. If the first evaluates to true, the second argument is returned; otherwise
the third is returned.
IIF could be likened to the ternary “<condition> ? resultT : resultF” operator in C-like languages.
 IIF(<condition>, resultT, resultF) is a shorthand for “CASE WHEN <condition> THEN
resultT ELSE resultF END”.
IIF Examples
select iif( sex = 'M', 'Sir', 'Madam' ) from Customers
See also
CASE, DECODE()
8.9.4. MAXVALUE()
Returns the maximum value of its arguments
Result type
Varies according to input — result will be of the same data type as the first expression in the list
(expr1).
Syntax
MAXVALUE (<expr1> [, ... , <exprN> ])
Table 193. MAXVALUE Function Parameters
Parameter Description
expr1 … exprN List of expressions of compatible types
Returns the maximum value from a list of numerical, string, or date/time expressions. This function
fully supports text BLOBs of any length and character set.
Chapter 8. Built-in Scalar Functions
472

<!-- Original PDF Page: 474 -->

If one or more expressions resolve to NULL, MAXVALUE returns NULL. This behaviour differs from the
aggregate function MAX.
MAXVALUE Examples
SELECT MAXVALUE(PRICE_1, PRICE_2) AS PRICE
  FROM PRICELIST
See also
MINVALUE()
8.9.5. MINVALUE()
Returns the minimum value of its arguments
Result type
Varies according to input — result will be of the same data type as the first expression in the list
(expr1).
Syntax
MINVALUE (<expr1> [, ... , <exprN> ])
Table 194. MINVALUE Function Parameters
Parameter Description
expr1 … exprN List of expressions of compatible types
Returns the minimum value from a list of numerical, string, or date/time expressions. This function
fully supports text BLOBs of any length and character set.
If one or more expressions resolve to NULL, MINVALUE returns NULL. This behaviour differs from the
aggregate function MIN.
MINVALUE Examples
SELECT MINVALUE(PRICE_1, PRICE_2) AS PRICE
  FROM PRICELIST
See also
MAXVALUE()
8.9.6. NULLIF()
Conditional NULL function
Result type
Chapter 8. Built-in Scalar Functions
473

<!-- Original PDF Page: 475 -->

Depends on input
Syntax
NULLIF (<exp1>, <exp2>)
Table 195. NULLIF Function Parameters
Parameter Description
exp1 An expression
exp2 Another expression of a data type compatible with exp1
NULLIF returns the value of the first argument, unless it is equal to the second. In that case, NULL is
returned.
NULLIF Example
select avg( nullif(Weight, -1) ) from FatPeople
This will return the average weight of the persons listed in FatPeople, excluding those having a
weight of -1, since AVG skips NULL data. Presumably, -1 indicates “weight unknown” in this table. A
plain AVG(Weight) would include the -1 weights, thus skewing the result.
See also
COALESCE(), DECODE(), IIF(), CASE
8.10. Special Functions for DECFLOAT
8.10.1. COMPARE_DECFLOAT()
Compares two DECFLOAT values to be equal, different or unordered
Result type
SMALLINT
Syntax
COMPARE_DECFLOAT (decfloat1, decfloat2)
Table 196. COMPARE_DECFLOAT Function Parameters
Parameter Description
decfloatn Value or expression of type DECFLOAT, or cast-compatible with DECFLOAT
The result is a SMALLINT value, as follows:
Chapter 8. Built-in Scalar Functions
474

<!-- Original PDF Page: 476 -->

0 Values are equal
1 First value is less than second
2 First value is greater than second
3 Values are unordered, i.e. one or both is NaN/sNaN
Unlike the comparison operators (‘ <’, ‘ =’, ‘ >’, etc.), comparison is exact: COMPARE_DECFLOAT(2.17,
2.170) returns 2 not 0.
See also
TOTALORDER()
8.10.2. NORMALIZE_DECFLOAT()
Returns the simplest, normalized form of a DECFLOAT
Result type
DECFLOAT
Syntax
NORMALIZE_DECFLOAT (decfloat_value)
Table 197. NORMALIZE_DECFLOAT Function Parameters
Parameter Description
decfloat_value Value or expression of type DECFLOAT, or cast-compatible with DECFLOAT
For any non-zero value, trailing zeroes are removed with appropriate correction of the exponent.
Examples of NORMALIZE_DECFLOAT
-- will return 12
select normalize_decfloat(12.00)
from rdb$database;
-- will return 1.2E+2
select normalize_decfloat(120)
from rdb$database;
8.10.3. QUANTIZE()
Returns a value that is equal in value — except for rounding — to the first argument, but with the
same exponent as the second argument
Result type
DECFLOAT
Chapter 8. Built-in Scalar Functions
475

<!-- Original PDF Page: 477 -->

Syntax
QUANTIZE (decfloat_value, exp_value)
Table 198. QUANTIZE Function Parameters
Parameter Description
decfloat_value Value or expression to quantize; needs to be of type DECFLOAT, or cast-
compatible with DECFLOAT
exp_value Value or expression to use for its exponent; needs to be of type DECFLOAT,
or cast-compatible with DECFLOAT
QUANTIZE returns a DECFLOAT value that is equal in value and sign (except for rounding) to
decfloat_value, and that has an exponent equal to the exponent of exp_value. The type of the return
value is DECFLOAT(16) if both arguments are DECFLOAT(16), otherwise the result type is DECFLOAT(34).

The target exponent is the exponent used in the Decimal64 or Decimal128 storage
format of DECFLOAT of exp_value. This is not necessarily the same as the exponent
displayed in tools like isql. For example, the value 1.23E+2 is coefficient 123 and
exponent 0, while 1.2 is coefficient 12 and exponent -1.
If the exponent of decfloat_value is greater than the one of exp_value, the coefficient of
decfloat_value is multiplied by a power of ten, and its exponent decreased. If the exponent is
smaller, then its coefficient is rounded using the current decfloat rounding mode, and its exponent
is increased.
When it is not possible to achieve the target exponent because the coefficient would exceed the
target precision (16 or 34 decimal digits), either a “ Decfloat float invalid operation” error is raised or
NaN is returned (depending on the current decfloat traps configuration).
There are almost no restrictions on the exp_value. However, in almost all usages, NaN/sNaN/Infinity
will produce an exception (unless allowed by the current decfloat traps configuration), NULL will
make the function return NULL, and so on.
Examples of QUANTIZE
select v, pic, quantize(v, pic) from examples;
     V    PIC QUANTIZE
====== ====== ========
  3.16  0.001    3.160
  3.16   0.01     3.16
  3.16    0.1      3.2
  3.16      1        3
  3.16   1E+1     0E+1
  -0.1      1       -0
     0   1E+5     0E+5
   316    0.1    316.0
Chapter 8. Built-in Scalar Functions
476

<!-- Original PDF Page: 478 -->

316      1      316
   316   1E+1   3.2E+2
   316   1E+2     3E+2
8.10.4. TOTALORDER()
Determines the total or linear order of its arguments
Result type
SMALLINT
Syntax
TOTALORDER (decfloat1, decfloat2)
Table 199. TOTALORDER Function Parameters
Parameter Description
decfloatn Value or expression of type DECFLOAT, or cast-compatible with DECFLOAT
TOTALORDER compares two DECFLOAT values including any special values. The comparison is exact,
and returns a SMALLINT, one of:
-1 First value is less than second
0 Values are equal
1 First value is greater than second.
For TOTALORDER comparisons, DECFLOAT values are ordered as follows:
-NaN < -sNaN < -INF < -0.1 < -0.10 < -0 < 0 < 0.10 < 0.1 < INF < sNaN < NaN
See also
COMPARE_DECFLOAT()
8.11. Cryptographic Functions
8.11.1. DECRYPT()
Decrypts data using a symmetric cipher
Result type
VARBINARY or BLOB
Syntax
DECRYPT ( encrypted_input
Chapter 8. Built-in Scalar Functions
477

<!-- Original PDF Page: 479 -->

USING <algorithm> [MODE <mode>]
  KEY key
  [IV iv] [<ctr_type>] [CTR_LENGTH ctr_length]
  [COUNTER initial_counter] )
!! See syntax of <<fblangref50-scalarfuncs-encrypt,ENCRYPT>> for further rules !!
Table 200. DECRYPT Function Parameters
Parameter Description
encrypted_input Encrypted input as a blob or (binary) string
See ENCRYPT Function Parameters for other parameters

• Sizes of data strings (like encrypted_input, key and iv) must meet the
requirements of the selected algorithm and mode.
• This function returns BLOB SUB_TYPE BINARY when the first argument is a BLOB,
and VARBINARY for all other text and binary types.
• When the encrypted data was text, it must be explicitly cast to a string type of
appropriate character set.
• The ins and outs of the various algorithms are considered beyond the scope of
this language reference. We recommend searching the internet for further
details on the algorithms.
DECRYPT Examples
select decrypt(x'0154090759DF' using sober128 key 'AbcdAbcdAbcdAbcd' iv '01234567')
  from rdb$database;
select decrypt(secret_field using aes mode ofb key '0123456701234567' iv init_vector)
  from secure_table;
See also
ENCRYPT(), RSA_DECRYPT()
8.11.2. ENCRYPT()
Encrypts data using a symmetric cipher
Result type
VARBINARY or BLOB
Syntax
ENCRYPT ( input
  USING <algorithm> [MODE <mode>]
  KEY key
  [IV iv] [<ctr_type>] [CTR_LENGTH ctr_length]
Chapter 8. Built-in Scalar Functions
478

<!-- Original PDF Page: 480 -->

[COUNTER initial_counter] )
<algorithm> ::= <block_cipher> | <stream_cipher>
<block_cipher> ::=
    AES | ANUBIS | BLOWFISH | KHAZAD | RC5
  | RC6 | SAFER+ | TWOFISH | XTEA
<stream_cipher> ::= CHACHA20 | RC4 | SOBER128
<mode> ::= CBC | CFB | CTR | ECB | OFB
<ctr_type> ::= CTR_BIG_ENDIAN | CTR_LITTLE_ENDIAN
Table 201. ENCRYPT Function Parameters
Parameter Description
input Input to encrypt as a blob or (binary) string
algorithm The algorithm to use for decryption
mode The algorithm mode; only for block ciphers
key The encryption/decryption key
iv Initialization vector or nonce; should be specified for block ciphers in all
modes except ECB, and all stream ciphers except RC4
ctr_type Endianness of the counter; only for CTR mode. Default is
CTR_LITTLE_ENDIAN.
ctr_length Counter length; only for CTR mode. Default is size of iv.
initial_counter Initial counter value; only for CHACHA20. Default is 0.

• This function returns BLOB SUB_TYPE BINARY when the first argument is a BLOB,
and VARBINARY for all other text and binary types.
• Sizes of data strings (like key and iv) must meet the requirements of the
selected algorithm and mode, see table Encryption Algorithm Requirements.
◦ In general, the size of iv must match the block size of the algorithm
◦ For ECB and CBC mode, input must be multiples of the block size, you will
need to manually pad with zeroes or spaces as appropriate.
• The ins and outs of the various algorithms and modes are considered beyond
the scope of this language reference. We recommend searching the internet for
further details on the algorithms.
• Although specified as separate options in this Language Reference, in the
actual syntax CTR_LENGTH and COUNTER are aliases.
Table 202. Encryption Algorithm Requirements
Chapter 8. Built-in Scalar Functions
479

<!-- Original PDF Page: 481 -->

Algori
thm
Key size (bytes) Block size (bytes) Notes
Block Ciphers
AES 16, 24, 32 16 Key size determines the AES variant:
16 bytes → AES-128
24 bytes → AES-192
32 bytes → AES-256
ANUBIS 16 - 40, in steps of 4 (4x) 16  
BLOWFI
SH
8 - 56 8  
KHAZAD 16 8  
RC5 8 - 128 8  
RC6 8 - 128 16  
SAFER+ 16, 24, 32 16  
TWOFIS
H
16, 24, 32 16  
XTEA 16 8  
Stream Ciphers
CHACHA
20
16, 32 1 Nonce size (IV) is 8 or 12 bytes. For
nonce size 8, initial_counter is a 64-bit
integer, for size 12, 32-bit.
RC4 5 - 256 1  
SOBER1
28
4x 1 Nonce size (IV) is 4y bytes, the length is
independent of key size.
ENCRYPT Examples
select encrypt('897897' using sober128 key 'AbcdAbcdAbcdAbcd' iv '01234567')
  from rdb$database;
See also
DECRYPT(), RSA_ENCRYPT()
8.11.3. RSA_DECRYPT()
Decrypts data using an RSA private key and removes OAEP or PKCS 1.5 padding
Result type
VARBINARY
Chapter 8. Built-in Scalar Functions
480

<!-- Original PDF Page: 482 -->

Syntax
RSA_DECRYPT (encrypted_input KEY private_key
  [LPARAM tag_string] [HASH <hash>] [PKCS_1_5])
<hash> ::= MD5 | SHA1 | SHA256 | SHA512
Table 203. RSA_DECRYPT Function Parameters
Parameter Description
encrypted_input Input data to decrypt
private_key Private key to apply, PKCS#1 format
tag_string An additional system-specific tag to identify which system encrypted the
message; default is NULL. If the tag does not match what was used during
encryption, RSA_DECRYPT will not decrypt the data.
hash The hash used for OAEP padding; default is SHA256.
RSA_DECRYPT decrypts encrypted_input using the RSA private key and then removes padding from the
resulting data.
By default, OAEP padding is used. The PKCS_1_5 option will switch to the less secure PKCS 1.5
padding.
 The PKCS_1_5 option is only for backward compatibility with systems applying
PKCS 1.5 padding. For security reasons, it should not be used in new projects.

• This function returns VARBINARY.
• When the encrypted data was text, it must be explicitly cast to a string type of
appropriate character set.
RSA_DECRYPT Examples
 Run the examples of the RSA_PRIVATE and RSA_PUBLIC, RSA_ENCRYPT functions first.
select cast(rsa_decrypt(rdb$get_context('USER_SESSION', 'msg')
  key rdb$get_context('USER_SESSION', 'private_key')) as varchar(128))
from rdb$database;
See also
RSA_ENCRYPT(), RSA_PRIVATE(), DECRYPT()
8.11.4. RSA_ENCRYPT()
Pads data using OAEP or PKCS 1.5 and then encrypts it with an RSA public key
Chapter 8. Built-in Scalar Functions
481

<!-- Original PDF Page: 483 -->

Result type
VARBINARY
Syntax
RSA_ENCRYPT (input KEY public_key
  [LPARAM tag_string] [HASH <hash>] [PKCS_1_5])
<hash> ::= MD5 | SHA1 | SHA256 | SHA512
Table 204. RSA_ENCRYPT Function Parameters
Parameter Description
input Input data to encrypt
public_key Public key to apply, PKCS#1 format
tag_string An additional system-specific tag to identify which system encrypted the
message; default is NULL.
hash The hash used for OAEP padding; default is SHA256.
RSA_ENCRYPT pads input using the OAEP or PKCS 1.5 padding scheme and then encrypts it using the
specified RSA public key. This function is normally used to encrypt short symmetric keys which are
then used in block ciphers to encrypt a message.
By default, OAEP padding is used. The PKCS_1_5 option will switch to the less secure PKCS 1.5
padding.
 The PKCS_1_5 option is only for backward compatibility with systems applying
PKCS 1.5 padding. For security reasons, it should not be used in new projects.
RSA_ENCRYPT Examples
 Run the examples of the RSA_PRIVATE and RSA_PUBLIC functions first.
select rdb$set_context('USER_SESSION', 'msg', rsa_encrypt('Some message'
  key rdb$get_context('USER_SESSION', 'public_key'))) from rdb$database;
See also
RSA_DECRYPT(), RSA_PUBLIC(), ENCRYPT()
8.11.5. RSA_PRIVATE()
Generates an RSA private key
Result type
VARBINARY
Chapter 8. Built-in Scalar Functions
482

<!-- Original PDF Page: 484 -->

Syntax
RSA_PRIVATE (key_length)
Table 205. RSA_PRIVATE Function Parameters
Parameter Description
key_length Key length in bytes; minimum 4, maximum 1024. A size of 256 bytes (2048
bits) or larger is recommended.
RSA_PRIVATE generates an RSA private key of the specified length (in bytes) in PKCS#1 format.
 The larger the length specified, the longer it takes for the function to generate a
private key.
RSA_PRIVATE Examples
select rdb$set_context('USER_SESSION', 'private_key', rsa_private(256))
  from rdb$database;

Putting private keys in the context variables is not secure; we’re doing it here for
demonstration purposes. SYSDBA and users with the role RDB$ADMIN or the system
privilege MONITOR_ANY_ATTACHMENT can see all context variables from all
attachments.
See also
RSA_PUBLIC(), RSA_DECRYPT()
8.11.6. RSA_PUBLIC()
Generates an RSA public key
Result type
VARBINARY
Syntax
RSA_PUBLIC (private_key)
Table 206. RSA_PUBLIC Function Parameters
Parameter Description
private_key RSA private key in PKCS#1 format
RSA_PUBLIC returns the RSA public key in PKCS#1 format for the provided RSA private key (also
PKCS#1 format).
Chapter 8. Built-in Scalar Functions
483

<!-- Original PDF Page: 485 -->

RSA_PUBLIC Examples
 Run the example of the RSA_PRIVATE function first.
select rdb$set_context('USER_SESSION', 'public_key',
  rsa_public(rdb$get_context('USER_SESSION', 'private_key'))) from rdb$database;
See also
RSA_PRIVATE(), RSA_ENCRYPT()
8.11.7. RSA_SIGN_HASH()
PSS encodes a message hash and signs it with an RSA private key
Result type
VARBINARY
Syntax
RSA_SIGN_HASH (message_digest
  KEY private_key
  [HASH <hash>] [SALT_LENGTH salt_length]
  [PKCS_1_5])
<hash> ::= MD5 | SHA1 | SHA256 | SHA512
Table 207. RSA_SIGN_HASH Function Parameters
Parameter Description
message_digest Hash of message to sign. The hash algorithm used should match hash
private_key RSA private key in PKCS#1 format
hash Hash to generate PSS encoding; default is SHA256. This should be the same
hash as used to generate message_digest.
salt_length Length of the desired salt in bytes; default is 8; minimum 1, maximum 32.
The recommended value is between 8 and 16.
RSA_SIGN_HASH performs PSS encoding of the message_digest to be signed, and signs using the RSA
private key.
By default, OAEP padding is used. The PKCS_1_5 option will switch to the less secure PKCS 1.5
padding.
 The PKCS_1_5 option is only for backward compatibility with systems applying
PKCS 1.5 padding. For security reasons, it should not be used in new projects.
 This function expects the hash of a message (or message digest), not the actual
Chapter 8. Built-in Scalar Functions
484

<!-- Original PDF Page: 486 -->

message. The hash argument should specify the algorithm that was used to
generate that hash.
A function that accepts the actual message to hash might be introduced in a future
version of Firebird.
PSS encoding
Probabilistic Signature Scheme (PSS) is a cryptographic signature scheme specifically
developed to allow modern methods of security analysis to prove that its security directly
relates to that of the RSA problem. There is no such proof for the traditional PKCS#1 v1.5
scheme.
RSA_SIGN_HASH Examples
 Run the example of the RSA_PRIVATE function first.
select rdb$set_context('USER_SESSION', 'msg',
  rsa_sign_hash(crypt_hash('Test message' using sha256)
    key rdb$get_context('USER_SESSION', 'private_key'))) from rdb$database;
See also
RSA_VERIFY_HASH(), RSA_PRIVATE(), CRYPT_HASH()
8.11.8. RSA_VERIFY_HASH()
Verifies a message hash against a signature using an RSA public key
Result type
BOOLEAN
Syntax
RSA_VERIFY_HASH (message_digest
  SIGNATURE signature KEY public_key
  [HASH <hash>] [SALT_LENGTH salt_length]
  [PKCS_1_5])
<hash> ::= MD5 | SHA1 | SHA256 | SHA512
Table 208. RSA_VERIFY Function Parameters
Parameter Description
message_digest Hash of message to verify. The hash algorithm used should match hash
signature Expected signature of input generated by RSA_SIGN_HASH
Chapter 8. Built-in Scalar Functions
485

<!-- Original PDF Page: 487 -->

Parameter Description
public_key RSA public key in PKCS#1 format matching the private key used to sign
hash Hash to use for the message digest; default is SHA256. This should be the
same hash as used to generate message_digest, and as used in
RSA_SIGN_HASH
salt_length Length of the salt in bytes; default is 8; minimum 1, maximum 32. Value
must match the length used in RSA_SIGN_HASH.
RSA_VERIFY_HASH performs PSS encoding of the message_digest to be verified, and verifies the digital
signature using the provided RSA public key.
By default, OAEP padding is used. The PKCS_1_5 option will switch to the less secure PKCS 1.5
padding.
 The PKCS_1_5 option is only for backward compatibility with systems applying
PKCS 1.5 padding. For security reasons, it should not be used in new projects.

This function expects the hash of a message (or message digest), not the actual
message. The hash argument should specify the algorithm that was used to
generate that hash.
A function that accepts the actual message to hash might be introduced in a future
version of Firebird.
RSA_VERIFY_HASH Examples
 Run the examples of the RSA_PRIVATE, RSA_PUBLIC and RSA_SIGN_HASH functions first.
select rsa_verify_hash(
  crypt_hash('Test message' using sha256)
  signature rdb$get_context('USER_SESSION', 'msg')
  key rdb$get_context('USER_SESSION', 'public_key'))
from rdb$database
See also
RSA_SIGN_HASH(), RSA_PUBLIC(), CRYPT_HASH()
8.12. Other Functions
Functions that don’t fit in any other category.
8.12.1. MAKE_DBKEY()
Creates a DBKEY value
Result type
Chapter 8. Built-in Scalar Functions
486

<!-- Original PDF Page: 488 -->

BINARY(8)
Syntax
MAKE_DBKEY (relation, recnum [, dpnum [, ppnum]])
Table 209. RDB$GET_TRANSACTION_CN Function Parameters
Parameter Description
relation Relation name or relation id
recnum Record number. Either absolute (if dpnum and ppnum are absent), or
relative (if dpnum present)
dpnum Data page number. Either absolute (if ppnum is absent) or relative (if
ppnum present)
ppnum Pointer page number.
MAKE_DBKEY creates a DBKEY value using a relation name or ID, record number, and (optionally)
logical numbers of data page and pointer page.

1. If relation is a string expression or literal, then it is treated as a relation name,
and the engine searches for the corresponding relation ID. The search is case-
sensitive. In the case of string literal, relation ID is evaluated at query
preparation time. In the case of expression, relation ID is evaluated at
execution time. If the relation cannot be found, then error isc_relnotdef is
raised.
2. If relation is a numeric expression or literal, then it is treated as a relation ID
and used “as is”, without verification against existing relations. If the argument
value is negative or greater than the maximum allowed relation ID (65535
currently), then NULL is returned.
3. Argument recnum represents an absolute record number in the relation (if the
next arguments dpnum and ppnum are missing), or a record number relative to
the first record, specified by the next arguments.
4. Argument dpnum is a logical number of data page in the relation (if the next
argument ppnum is missing), or number of data pages relative to the first data
page addressed by the given ppnum.
5. Argument ppnum is a logical number of pointer page in the relation.
6. All numbers are zero-based. Maximum allowed value for dpnum and ppnum is
232
 (4294967296). If dpnum is specified, then recnum can be negative. If dpnum
is missing and recnum is negative, then NULL is returned. If ppnum is specified,
then dpnum can be negative. If ppnum is missing and dpnum is negative, then
NULL is returned.
7. If any of specified arguments is NULL, the result is also NULL.
8. Argument relation is described as INTEGER during query preparation, but it can
be overridden by a client application as VARCHAR or CHAR. Arguments recnum,
Chapter 8. Built-in Scalar Functions
487

<!-- Original PDF Page: 489 -->

dpnum and ppnum are described as BIGINT.
Examples of MAKE_DBKEY
1. Select record using relation name (note that relation name is uppercase)
select *
from rdb$relations
where rdb$db_key = make_dbkey('RDB$RELATIONS', 0)
2. Select record using relation ID
select *
from rdb$relations
where rdb$db_key = make_dbkey(6, 0)
3. Select all records physically residing on the first data page
select *
from rdb$relations
where rdb$db_key >= make_dbkey(6, 0, 0)
and rdb$db_key < make_dbkey(6, 0, 1)
4. Select all records physically residing on the first data page of 6th pointer page
select *
from SOMETABLE
where rdb$db_key >= make_dbkey('SOMETABLE', 0, 0, 5)
and rdb$db_key < make_dbkey('SOMETABLE', 0, 1, 5)
8.12.2. RDB$ERROR()
Returns PSQL error information inside a WHEN … DO block
Available in
PSQL
Result type
Varies (see table below)
Syntax
RDB$ERROR (<context>)
<context> ::=
Chapter 8. Built-in Scalar Functions
488

<!-- Original PDF Page: 490 -->

GDSCODE | SQLCODE | SQLSTATE | EXCEPTION | MESSAGE
Table 210. Contexts
Context Result type Description
GDSCODE INTEGER Firebird error code, see also GDSCODE
SQLCODE INTEGER (deprecated) SQL code, see also SQLCODE
SQLSTATE CHAR(5) CHARACTER
SET ASCII
SQLstate, see also SQLSTATE
EXCEPTION VARCHAR(63)
CHARACTER SET UTF8
Name of the active user-defined exception or NULL if the
active exception is a system exception
MESSAGE VARCHAR(1024)
CHARACTER SET UTF8
Message text of the active exception
RDB$ERROR returns data of the specified context about the active PSQL exception. Its scope is
confined to exception-handling blocks in PSQL ( WHEN … DO). Outside the exception handling blocks,
RDB$ERROR always returns NULL. This function cannot be called from DSQL.
Example of RDB$ERROR
BEGIN
  ...
WHEN ANY DO
  EXECUTE PROCEDURE P_LOG_EXCEPTION(RDB$ERROR(MESSAGE));
END
See also
Trapping and Handling Errors, GDSCODE, SQLCODE, SQLSTATE
8.12.3. RDB$GET_TRANSACTION_CN()
Returns the commit number (“CN”) of a transaction
Result type
BIGINT
Syntax
RDB$GET_TRANSACTION_CN (transaction_id)
Table 211. RDB$GET_TRANSACTION_CN Function Parameters
Parameter Description
transaction_id Transaction id
If the return value is greater than 1, it is the actual CN of the transaction if it was committed after
Chapter 8. Built-in Scalar Functions
489

<!-- Original PDF Page: 491 -->

the database was started.
The function can also return one of the following results, indicating the commit status of the
transaction:
-2 Transaction is dead (rolled back)
-1 Transaction is in limbo
 0 Transaction is still active
 1 Transaction committed before the database started or less than the Oldest Interesting
Transaction for the database
NULL Transaction number supplied is NULL or greater than Next Transaction for the
database
 For more information about CN, consult the Firebird 4.0 Release Notes.
RDB$GET_TRANSACTION_CN Examples
select rdb$get_transaction_cn(current_transaction) from rdb$database;
select rdb$get_transaction_cn(123) from rdb$database;
8.12.4. RDB$ROLE_IN_USE()
Checks if a role is active for the current connection
Result type
BOOLEAN
Syntax
RDB$ROLE_IN_USE (role_name)
Table 212. RDB$ROLE_IN_USE Function Parameters
Parameter Description
role_name String expression for the role to check. Case-sensitive, must match the
role name as stored in RDB$ROLES
RDB$ROLE_IN_USE returns TRUE if the specified role is active for the current connection, and FALSE
otherwise. Contrary to CURRENT_ROLE — which only returns the explicitly specified role — this
function can be used to check for roles that are active by default, or cumulative roles activated by
an explicitly specified role.
RDB$ROLE_IN_USE Examples
Chapter 8. Built-in Scalar Functions
490