# 12 Context Variables



<!-- Original PDF Page: 542 -->

RDB$ZONE_OFFSET
type SMALLINT — The zone’s offset, in minutes
RDB$DST_OFFSET
type SMALLINT — The zone’s DST offset, in minutes
RDB$EFFECTIVE_OFFSET
type SMALLINT — Effective offset (ZONE_OFFSET + DST_OFFSET)
Example
select *
  from rdb$time_zone_util.transitions(
    'America/Sao_Paulo',
    timestamp '2017-01-01',
    timestamp '2019-01-01');
Returns (RDB$ prefix left off for brevity):
             START_TIMESTAMP                END_TIMESTAMP ZONE_OFFSET DST_OFFSET
EFFECTIVE_OFFSET
============================ ============================ =========== ==========
================
2016-10-16 03:00:00.0000 GMT 2017-02-19 01:59:59.9999 GMT       -180        60
-120
2017-02-19 02:00:00.0000 GMT 2017-10-15 02:59:59.9999 GMT       -180         0
-180
2017-10-15 03:00:00.0000 GMT 2018-02-18 01:59:59.9999 GMT       -180        60
-120
2018-02-18 02:00:00.0000 GMT 2018-10-21 02:59:59.9999 GMT       -180         0
-180
2018-10-21 03:00:00.0000 GMT 2019-02-17 01:59:59.9999 GMT       -180        60
-120
Chapter 11. System Packages
541

<!-- Original PDF Page: 543 -->

Chapter 12. Context Variables
Unless explicitly mentioned otherwise in an “Available in” section, context variables are available
in at least DSQL and PSQL. Availability in ESQL is — bar some exceptions — not tracked by this
Language Reference.
12.1. CURRENT_CONNECTION
Unique identifier of the current connection.
Type
BIGINT
Syntax
CURRENT_CONNECTION
Its value is derived from a counter on the database header page, which is incremented for each
new connection. When a database is restored, this counter is reset to zero.
Examples
select current_connection from rdb$database
execute procedure P_Login(current_connection)
12.2. CURRENT_DATE
Current server date in the session time zone
Type
DATE
Syntax
CURRENT_DATE

Within a PSQL module (procedure, trigger or executable block), the value of
CURRENT_DATE will remain constant every time it is read. If multiple modules call or
trigger each other, the value will remain constant throughout the duration of the
outermost module. If you need a progressing value in PSQL (e.g. to measure time
intervals), use 'TODAY'.
Examples
select current_date from rdb$database
Chapter 12. Context Variables
542

<!-- Original PDF Page: 544 -->

-- returns e.g. 2011-10-03
12.3. CURRENT_ROLE
Current explicit role of the connection
Type
VARCHAR(63)
Syntax
CURRENT_ROLE
CURRENT_ROLE is a context variable containing the explicitly specified role of the currently connected
user. If there is no explicitly specified role, CURRENT_ROLE is 'NONE'.
CURRENT_ROLE always represents a valid role or 'NONE'. If a user connects with a non-existing role,
the engine silently resets it to 'NONE' without returning an error.

Roles that are active by default and not explicitly specified on connect or using SET
ROLE are not returned by CURRENT_ROLE. Use RDB$ROLE_IN_USE to check for all active
roles.
Example
if (current_role <> 'MANAGER')
  then exception only_managers_may_delete;
else
  delete from Customers where custno = :custno;
See also
RDB$ROLE_IN_USE
12.4. CURRENT_TIME
Current server time in the session time zone, with time zone information
Type
TIME WITH TIME ZONE
 Data type changed in Firebird 4.0 from TIME WITHOUT TIME ZONE  to TIME WITH TIME
ZONE. Use LOCALTIME to obtain TIME WITHOUT TIME ZONE.
Syntax
CURRENT_TIME [ (<precision>) ]
Chapter 12. Context Variables
543

<!-- Original PDF Page: 545 -->

<precision> ::= 0 | 1 | 2 | 3
The optional precision argument is not supported in ESQL.
Table 243. CURRENT_TIME Parameter
Parameter Description
precision Precision. The default value is 0. Not supported in ESQL
The default is 0 decimals, i.e. seconds precision.

• CURRENT_TIME has a default precision of 0 decimals, where CURRENT_TIMESTAMP has
a default precision of 3 decimals. As a result, CURRENT_TIMESTAMP is not the exact
sum of CURRENT_DATE and CURRENT_TIME, unless you explicitly specify a precision
(i.e. CURRENT_TIME(3) or CURRENT_TIMESTAMP(0)).
• Within a PSQL module (procedure, trigger or executable block), the value of
CURRENT_TIME will remain constant every time it is read. If multiple modules call
or trigger each other, the value will remain constant throughout the duration
of the outermost module. If you need a progressing value in PSQL (e.g. to
measure time intervals), use 'NOW'.

CURRENT_TIME and Firebird Time Zone Support
Firebird 4.0 added support for time zones. As part of this support, an
incompatibility with the CURRENT_TIME expression was introduced compared to
previous version.
Since Firebird 4.0, CURRENT_TIME returns the TIME WITH TIME ZONE type. In order for
your queries to be compatible with database code of Firebird 4.0 and higher,
Firebird 3.0.4 and Firebird 2.5.9 introduced the LOCALTIME expression. In Firebird
3.0.4 and Firebird 2.5.9, LOCALTIME is a synonym for CURRENT_TIME.
In Firebird 5.0, LOCALTIME returns TIME [WITHOUT TIME ZONE] ), while CURRENT_TIME
returns TIME WITH TIME ZONE.
Examples
select current_time from rdb$database
-- returns e.g. 14:20:19.0000
select current_time(2) from rdb$database
-- returns e.g. 14:20:23.1200
See also
CURRENT_TIMESTAMP, LOCALTIME, LOCALTIMESTAMP
Chapter 12. Context Variables
544

<!-- Original PDF Page: 546 -->

12.5. CURRENT_TIMESTAMP
Current server date and time in the session time zone, with time zone information
Type
TIMESTAMP WITH TIME ZONE
 Data type changed in Firebird 4.0 from TIMESTAMP WITHOUT TIME ZONE  to TIMESTAMP
WITH TIME ZONE. Use LOCALTIMESTAMP to obtain TIMESTAMP WITHOUT TIME ZONE.
Syntax
CURRENT_TIMESTAMP [ (<precision>) ]
<precision> ::= 0 | 1 | 2 | 3
The optional precision argument is not supported in ESQL.
Table 244. CURRENT_TIMESTAMP Parameter
Parameter Description
precision Precision. The default value is 3. Not supported in ESQL
The default is 3 decimals, i.e. milliseconds precision.

• The default precision of CURRENT_TIME is 0 decimals, so CURRENT_TIMESTAMP is not
the exact sum of CURRENT_DATE and CURRENT_TIME, unless you explicitly specify a
precision (i.e. CURRENT_TIME(3) or CURRENT_TIMESTAMP(0)).
• Within a PSQL module (procedure, trigger or executable block), the value of
CURRENT_TIMESTAMP will remain constant every time it is read. If multiple
modules call or trigger each other, the value will remain constant throughout
the duration of the outermost module. If you need a progressing value in PSQL
(e.g. to measure time intervals), use 'NOW'.

CURRENT_TIMESTAMP and Firebird Time Zone Support
Firebird 4.0 added support for time zones. As part of this support, an
incompatibility with the CURRENT_TIMESTAMP expression was introduced compared
to previous versions.
Since Firebird 4.0, CURRENT_TIMESTAMP returns the TIMESTAMP WITH TIME ZONE type. In
order for your queries to be compatible with database code of Firebird 4.0 and
higher, Firebird 3.0.4 and Firebird 2.5.9 introduced the LOCALTIMESTAMP expression.
In Firebird 3.0.4 and Firebird 2.5.9, LOCALTIMESTAMP is a synonym for
CURRENT_TIMESTAMP.
In Firebird 5.0, LOCALTIMESTAMP returns TIMESTAMP [WITHOUT TIME ZONE] , while
CURRENT_TIMESTAMP returns TIMESTAMP WITH TIME ZONE.
Chapter 12. Context Variables
545

<!-- Original PDF Page: 547 -->

Examples
select current_timestamp from rdb$database
-- returns e.g. 2008-08-13 14:20:19.6170
select current_timestamp(2) from rdb$database
-- returns e.g. 2008-08-13 14:20:23.1200
See also
CURRENT_TIME, LOCALTIME, LOCALTIMESTAMP
12.6. CURRENT_TRANSACTION
Unique identifier of the current transaction
Type
BIGINT
Syntax
CURRENT_TRANSACTION
The transaction identifier is derived from a counter on the database header page, which is
incremented for each new transaction. When a database is restored, this counter is reset to zero.
Examples
select current_transaction from rdb$database
New.Txn_ID = current_transaction;
12.7. CURRENT_USER
Name of the user of the current connection
Type
VARCHAR(63)
Syntax
CURRENT_USER
CURRENT_USER is equivalent to USER.
Example
create trigger bi_customers for customers before insert as
begin
Chapter 12. Context Variables
546

<!-- Original PDF Page: 548 -->

New.added_by  = CURRENT_USER;
    New.purchases = 0;
end
12.8. DELETING
Indicates if the trigger fired for a DELETE operation
Available in
PSQL — DML triggers only
Type
BOOLEAN
Syntax
DELETING
Intended for use in multi-action triggers.
Example
if (deleting) then
begin
  insert into Removed_Cars (id, make, model, removed)
    values (old.id, old.make, old.model, current_timestamp);
end
12.9. GDSCODE
Firebird error code of the error in a WHEN … DO block
Available in
PSQL
Type
INTEGER
Syntax
GDSCODE
In a “ WHEN … DO” error handling block, the GDSCODE context variable contains the numeric value of
the current Firebird error code. GDSCODE is non-zero in WHEN … DO blocks, if the current error has a
Firebird error code. Outside error handlers, GDSCODE is always 0. Outside PSQL, it doesn’t exist at all.
 After WHEN GDSCODE, you must use symbolic names like grant_obj_notfound etc. But
Chapter 12. Context Variables
547

<!-- Original PDF Page: 549 -->

the GDSCODE context variable is an INTEGER. If you want to compare it against a
specific error, the numeric value must be used, e.g. 335544551 for
grant_obj_notfound.
Example
when gdscode grant_obj_notfound, gdscode grant_fld_notfound,
   gdscode grant_nopriv, gdscode grant_nopriv_on_base
do
begin
  execute procedure log_grant_error(gdscode);
  exit;
end
12.10. INSERTING
Indicates if the trigger fired for an INSERT operation
Available in
PSQL — triggers only
Type
BOOLEAN
Syntax
INSERTING
Intended for use in multi-action triggers.
Example
if (inserting or updating) then
begin
  if (new.serial_num is null) then
    new.serial_num = gen_id(gen_serials, 1);
end
12.11. LOCALTIME
Current server time in the session time zone, without time zone information
Type
TIME WITHOUT TIME ZONE
Syntax
LOCALTIME [ (<precision>) ]
Chapter 12. Context Variables
548

<!-- Original PDF Page: 550 -->

<precision> ::= 0 | 1 | 2 | 3
The optional precision argument is not supported in ESQL.
Table 245. LOCALTIME Parameter
Parameter Description
precision Precision. The default value is 0. Not supported in ESQL
LOCALTIME returns the current server time in the session time zone. The default is 0 decimals, i.e.
seconds precision.

• LOCALTIME was introduced in Firebird 3.0.4 and Firebird 2.5.9 as an alias of
CURRENT_TIME. In Firebird 5.0, CURRENT_TIME returns a TIME WITH TIME ZONE 
instead of a TIME [WITHOUT TIME ZONE] , while LOCALTIME returns TIME [WITHOUT
TIME ZONE]. It is recommended to use LOCALTIME when you do not need time
zone information.
• LOCALTIME has a default precision of 0 decimals, where LOCALTIMESTAMP has a
default precision of 3 decimals. As a result, LOCALTIMESTAMP is not the exact sum
of CURRENT_DATE and LOCALTIME, unless you explicitly specify a precision (i.e.
LOCALTIME(3) or LOCALTIMESTAMP(0)).
• Within a PSQL module (procedure, trigger or executable block), the value of
LOCALTIME will remain constant every time it is read. If multiple modules call or
trigger each other, the value will remain constant throughout the duration of
the outermost module. If you need a progressing value in PSQL (e.g. to measure
time intervals), use 'NOW'.
Examples
select localtime from rdb$database
-- returns e.g. 14:20:19.0000
select localtime(2) from rdb$database
-- returns e.g. 14:20:23.1200
See also
CURRENT_TIME, LOCALTIMESTAMP
12.12. LOCALTIMESTAMP
Current server time and date in the session time zone, without time zone information
Type
TIMESTAMP WITHOUT TIME ZONE
Chapter 12. Context Variables
549

<!-- Original PDF Page: 551 -->

Syntax
LOCALTIMESTAMP [ (<precision>) ]
<precision> ::= 0 | 1 | 2 | 3
The optional precision argument is not supported in ESQL.
Table 246. LOCALTIMESTAMP Parameter
Parameter Description
precision Precision. The default value is 3. Not supported in ESQL
LOCALTIMESTAMP returns the current server date and time in the session time zone. The default is 3
decimals, i.e. milliseconds precision.

• LOCALTIMESTAMP was introduced in Firebird 3.0.4 and Firebird 2.5.9 as a
synonym of CURRENT_TIMESTAMP. In Firebird 5.0, CURRENT_TIMESTAMP returns a
TIMESTAMP WITH TIME ZONE  instead of a TIMESTAMP [WITHOUT TIME ZONE] , while
LOCALTIMESTAMP returns TIMESTAMP [WITHOUT TIME ZONE] . It is recommended to
use LOCALTIMESTAMP when you do not need time zone information.
• The default precision of LOCALTIME is 0 decimals, so LOCALTIMESTAMP is not the
exact sum of CURRENT_DATE and LOCALTIME, unless you explicitly specify a
precision (i.e. LOCATIME(3) or LOCALTIMESTAMP(0)).
• Within a PSQL module (procedure, trigger or executable block), the value of
LOCALTIMESTAMP will remain constant every time it is read. If multiple modules
call or trigger each other, the value will remain constant throughout the
duration of the outermost module. If you need a progressing value in PSQL (e.g.
to measure time intervals), use 'NOW'.
Examples
select localtimestamp from rdb$database
-- returns e.g. 2008-08-13 14:20:19.6170
select localtimestamp(2) from rdb$database
-- returns e.g. 2008-08-13 14:20:23.1200
See also
CURRENT_TIMESTAMP, LOCALTIME
12.13. NEW
Record with the inserted or updated values of a row
Available in
PSQL — triggers only,
Chapter 12. Context Variables
550

<!-- Original PDF Page: 552 -->

DSQL — RETURNING clause of UPDATE, UPDATE OR INSERT and MERGE
Type
Record type
Syntax
NEW.column_name
Table 247. NEW Parameters
Parameter Description
column_name Column name to access
NEW contains the new version of a database record that has just been inserted or updated. NEW is
read-only in AFTER triggers.

In multi-action triggers NEW is always available. However, if the trigger is fired by a
DELETE, there will be no new version of the record. In that situation, reading from
NEW will always return NULL; writing to it will cause a runtime exception.
12.14. 'NOW'
Current date and/or time in cast context
Type
CHAR(3), or depends on explicit CAST
'NOW' is not a variable, but a string literal or datetime mnemonic. It is, however, special in the sense
that when you CAST() it to a datetime type, you will get the current date and/or time. If the datetime
type has a time component, the precision is 3 decimals, i.e. milliseconds. 'NOW' is case-insensitive,
and the engine ignores leading or trailing spaces when casting.

• 'NOW' always returns the actual date/time, even in PSQL modules, where
CURRENT_DATE, CURRENT_TIME and CURRENT_TIMESTAMP return the same value
throughout the duration of the outermost routine. This makes 'NOW' useful for
measuring time intervals in triggers, procedures and executable blocks.
• Except in the situation mentioned above, reading CURRENT_DATE, CURRENT_TIME
and CURRENT_TIMESTAMP is generally preferable to casting 'NOW'. Be aware though
that CURRENT_TIME defaults to seconds precision; to get milliseconds precision,
use CURRENT_TIME(3).
• Firebird 3.0 and earlier allowed the use of 'NOW' in datetime literals (a.k.a.
"`shorthand casts"`), this is no longer allowed since Firebird 4.0.
Examples
select 'Now' from rdb$database
Chapter 12. Context Variables
551

<!-- Original PDF Page: 553 -->

-- returns 'Now'
select cast('Now' as date) from rdb$database
-- returns e.g. 2008-08-13
select cast('now' as time) from rdb$database
-- returns e.g. 14:20:19.6170
select cast('NOW' as timestamp) from rdb$database
-- returns e.g. 2008-08-13 14:20:19.6170
12.15. OLD
Record with the initial values of a row before update or delete
Available in
PSQL — triggers only,
DSQL — RETURNING clause of UPDATE, UPDATE OR INSERT and MERGE
Type
Record type
Syntax
OLD.column_name
Table 248. OLD Parameters
Parameter Description
column_name Column name to access
OLD contains the existing version of a database record just before a deletion or update. The OLD
record is read-only.

In multi-action triggers OLD is always available. However, if the trigger is fired by
an INSERT, there is obviously no pre-existing version of the record. In that situation,
reading from OLD will always return NULL.
12.16. RESETTING
Indicates if the trigger fired during a session reset
Available in
PSQL — triggers only
Type
BOOLEAN
Chapter 12. Context Variables
552

<!-- Original PDF Page: 554 -->

Syntax
RESETTING
Its value is TRUE if session reset is in progress and FALSE otherwise. Intended for use in ON DISCONNECT
and ON CONNECT database triggers to detect an ALTER SESSION RESET.
12.17. ROW_COUNT
Number of affected rows of the last executed statement
Available in
PSQL
Type
INTEGER
Syntax
ROW_COUNT
The ROW_COUNT context variable contains the number of rows affected by the most recent DML
statement (INSERT, UPDATE, DELETE, SELECT or FETCH) in the current PSQL module.
Behaviour with SELECT and FETCH
• After a singleton SELECT, ROW_COUNT is 1 if a data row was retrieved and 0 otherwise.
• In a FOR SELECT loop, ROW_COUNT is incremented with every iteration (starting at 0 before the
first).
• After a FETCH from a cursor, ROW_COUNT is 1 if a data row was retrieved and 0 otherwise. Fetching
more records from the same cursor does not increment ROW_COUNT beyond 1.
 ROW_COUNT cannot be used to determine the number of rows affected by an EXECUTE
STATEMENT or EXECUTE PROCEDURE command.
Example
update Figures set Number = 0 where id = :id;
if (row_count = 0) then
  insert into Figures (id, Number) values (:id, 0);
12.18. SQLCODE
SQLCODE of the Firebird error in a WHEN … DO block
Available in
PSQL
Chapter 12. Context Variables
553

<!-- Original PDF Page: 555 -->

Deprecated in
2.5.1
Type
INTEGER
Syntax
SQLCODE
In a “ WHEN … DO” error handling block, the SQLCODE context variable contains the numeric value of
the current SQL error code. SQLCODE is non-zero in WHEN … DO blocks, if the current error has a SQL
error code. Outside error handlers, SQLCODE is always 0. Outside PSQL, it doesn’t exist at all.

SQLCODE is now deprecated in favour of the SQL-2003-compliant SQLSTATE status
code. Support for SQLCODE and WHEN SQLCODE will be discontinued in a future version
of Firebird.
Example
when any
do
begin
  if (sqlcode <> 0) then
    Msg = 'An SQL error occurred!';
  else
    Msg = 'Something bad happened!';
  exception ex_custom Msg;
end
12.19. SQLSTATE
SQLSTATE code of the Firebird error in a WHEN … DO block
Available in
PSQL
Type
CHAR(5)
Syntax
SQLSTATE
In a “ WHEN …  DO ” error handler, the SQLSTATE context variable contains the 5-character, SQL-
compliant status code of the current error. Outside error handlers, SQLSTATE is always '00000'.
Outside PSQL, it is not available at all.
Chapter 12. Context Variables
554

<!-- Original PDF Page: 556 -->


• SQLSTATE is destined to replace SQLCODE. The latter is now deprecated in Firebird
and will disappear in a future version.
• Each SQLSTATE code is the concatenation of a 2-character class and a 3-character
subclass. Classes 00 (successful completion), 01 (warning) and 02 (no data)
represent completion conditions . Every status code outside these classes is an
exception. Because classes 00, 01 and 02 don’t raise an error, they won’t ever
show up in the SQLSTATE variable.
• For a complete listing of SQLSTATE codes, consult the SQLSTATE Codes and
Message Texts section in Appendix B, Exception Codes and Messages.
Example
when any
do
begin
  Msg = case sqlstate
          when '22003' then 'Numeric value out of range.'
          when '22012' then 'Division by zero.'
          when '23000' then 'Integrity constraint violation.'
          else 'Something bad happened! SQLSTATE = ' || sqlstate
        end;
  exception ex_custom Msg;
end
12.20. 'TODAY'
Current date in cast context
Type
CHAR(5), or depends on explicit CAST
'TODAY' is not a variable, but a string literal or date mnemonic. It is, however, special in the sense
that when you CAST() it to a date/time type, you will get the current date. If the target datetime type
has a time component, it will be set to zero. 'TODAY' is case-insensitive, and the engine ignores
leading or trailing spaces when casting.

• 'TODAY' always returns the actual date, even in PSQL modules, where
CURRENT_DATE, CURRENT_TIME and CURRENT_TIMESTAMP return the same value
throughout the duration of the outermost routine. This makes 'TODAY' useful
for measuring time intervals in triggers, procedures and executable blocks (at
least if your procedures are running for days).
• Except in the situation mentioned above, reading CURRENT_DATE, is generally
preferable to casting 'TODAY'.
• Firebird 3.0 and earlier allowed the use of 'TODAY' in datetime literals (a.k.a.
"`shorthand casts"`), this is no longer allowed since Firebird 4.0.
Chapter 12. Context Variables
555

<!-- Original PDF Page: 557 -->

• When cast to a TIMESTAMP WITH TIME ZONE, the time reflected will be 00:00:00 in
UTC rebased to the session time zone.
Examples
select 'Today' from rdb$database
-- returns 'Today'
select cast('Today' as date) from rdb$database
-- returns e.g. 2011-10-03
select cast('TODAY' as timestamp) from rdb$database
-- returns e.g. 2011-10-03 00:00:00.0000
12.21. 'TOMORROW'
Tomorrow’s date in cast context
Type
CHAR(8), or depends on explicit CAST
'TOMORROW' is not a variable, but a string literal. It is, however, special in the sense that when you
CAST() it to a date/time type, you will get the date of the next day. See also 'TODAY'.
Examples
select 'Tomorrow' from rdb$database
-- returns 'Tomorrow'
select cast('Tomorrow' as date) from rdb$database
-- returns e.g. 2011-10-04
select cast('TOMORROW' as timestamp) from rdb$database
-- returns e.g. 2011-10-04 00:00:00.0000
12.22. UPDATING
Indicates if the trigger fired for an UPDATE operation
Available in
PSQL — triggers only
Type
BOOLEAN
Chapter 12. Context Variables
556

<!-- Original PDF Page: 558 -->

Syntax
UPDATING
Intended for use in multi-action triggers.
Example
if (inserting or updating) then
begin
  if (new.serial_num is null) then
    new.serial_num = gen_id(gen_serials, 1);
end
12.23. 'YESTERDAY'
Yesterday’s date in cast context
Type
CHAR(9), or depends on explicit CAST
'YESTERDAY' is not a variable, but a string literal. It is, however, special in the sense that when you
CAST() it to a date/time type, you will get the date of the day before. See also 'TODAY'.
Examples
select 'Yesterday' from rdb$database
-- returns 'Yesterday'
select cast('Yesterday as date) from rdb$database
-- returns e.g. 2011-10-02
select cast('YESTERDAY' as timestamp) from rdb$database
-- returns e.g. 2011-10-02 00:00:00.0000
12.24. USER
Name of the user of the current connection
Type
VARCHAR(63)
Syntax
USER
USER is equivalent to (or, alias of) CURRENT_USER.
Chapter 12. Context Variables
557