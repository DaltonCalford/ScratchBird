# 07 PSQL Statements



<!-- Original PDF Page: 349 -->

Statements.
6.8.3. Statement Terminators
Some SQL statement editors — specifically the isql utility that comes with Firebird, and possibly
some third-party editors — employ an internal convention that requires all statements to be
terminated with a semicolon. This creates a conflict with PSQL syntax when coding in these
environments. If you are unacquainted with this problem and its solution, please study the details
in the PSQL chapter in the section entitled Switching the Terminator in isql.
Chapter 6. Data Manipulation (DML) Statements
348

<!-- Original PDF Page: 350 -->

Chapter 7. Procedural SQL (PSQL) Statements
Procedural SQL (PSQL) is a procedural extension of SQL. This language subset is used for writing
PSQL modules: stored procedures, stored functions, triggers, and PSQL blocks.
PSQL provides all the basic constructs of traditional structured programming languages, and also
includes DML statements ( SELECT, INSERT, UPDATE, DELETE, etc.), with a slightly modified syntax in
some cases.
7.1. Elements of PSQL
A PSQL module may contain declarations of local variables, subroutines and cursors, assignments,
conditional statements, loops, statements for raising custom exceptions, error handling and sending
messages (events) to client applications. DML triggers have access to special context variables, two
“records” that store, respectively, the NEW values for all columns during insert and update activity,
and the OLD values during update and delete work, and three Boolean variables —  INSERTING,
UPDATING and DELETING — to determine the event that fired the trigger.
Statements that modify metadata (DDL) are not available in PSQL.
7.1.1. DML Statements with Parameters
If DML statements (SELECT, INSERT, UPDATE, DELETE, etc.) in the body of a module (procedure, function,
trigger or block) use parameters, only named parameters can be used. If DML statements contain
named parameters, then they must be previously declared as local variables using DECLARE
[VARIABLE] in the declaration section of the module, or as input or output variables in the module
header.
When a DML statement with parameters is included in PSQL code, the parameter name must be
prefixed by a colon (‘ :’) in most situations. The colon is optional in statement syntax that is specific
to PSQL, such as assignments and conditionals and the INTO clause. The colon prefix on parameters
is not required when calling stored procedures from within another PSQL module.
7.1.2. Transactions
Stored procedures and functions (including those defined in packages) are executed in the context
of the transaction in which they are called. Triggers are executed as an intrinsic part of the
operation of the DML statement: thus, their execution is within the same transaction context as the
statement itself. Individual transactions are launched for database event triggers fired on connect
or disconnect.
Statements that start and end transactions are not available in PSQL, but it is possible to run a
statement or a block of statements in an autonomous transaction.
7.1.3. Module Structure
PSQL code modules consist of a header and a body. The DDL statements for defining them are
complex statements ; that is, they consist of a single statement that encloses blocks of multiple
Chapter 7. Procedural SQL (PSQL) Statements
349

<!-- Original PDF Page: 351 -->

statements. These statements begin with a verb ( CREATE, ALTER, DROP, RECREATE, CREATE OR ALTER , or
EXECUTE BLOCK) and end with the last END statement of the body.
The Module Header
The header provides the module name and defines any input and output parameters or — for
functions — the return type. Stored procedures and PSQL blocks may have input and output
parameters. Functions may have input parameters and must have a scalar return type. Triggers do
not have either input or output parameters, but DML triggers do have the NEW and OLD “records”,
and INSERTING, UPDATING and DELETING variables.
The header of a trigger indicates the DML event (insert, update or delete, or a combination) or DDL
or database event and the phase of operation (BEFORE or AFTER that event) that will cause it to “fire”.
The Module Body
The module body is either a PSQL module body, or an external module body. PSQL blocks can only
have a PSQL module body.
Syntax of a Module Body
<module-body> ::=
  <psql-module-body> | <external-module-body>
<psql-module-body> ::=
  AS
    [<declarations>]
  BEGIN
    [<PSQL_statements>]
  END
<external-module-body> ::=
  EXTERNAL [NAME <extname>] ENGINE engine
  [AS '<extbody>']
<declarations> ::= <declare-item> [<declare-item ...]
<declare-item> ::=
    <declare-var>
  | <declare-cursor>
  | <declare-subfunc>
  | <declare-subproc>
<extname> ::=
  '<module-name>!<routine-name>[!<misc-info>]'
<declare-var> ::= !! See DECLARE VARIABLE !!
<declare-cursor> ::= !! See DECLARE .. CURSOR !!
<declare-subfunc> ::= !! See DECLARE FUNCTION !!
Chapter 7. Procedural SQL (PSQL) Statements
350

<!-- Original PDF Page: 352 -->

<declare-subproc>> ::= !! See DECLARE PROCEDURE !!
Table 94. Module Body Parameters
Parameter Description
declarations Section for declaring local variables, named cursors, and subroutines
PSQL_statements Procedural SQL statements. Some PSQL statements may not be valid in all
types of PSQL. For example, RETURN <value>; is only valid in functions.
declare_var Local variable declaration
declare_cursor Named cursor declaration
declare-subfunc Sub-function declaration or forward declaration
declare-subproc Sub-procedure declaration or forward declaration
extname String identifying the external procedure
engine String identifying the UDR engine
extbody External procedure body. A string literal that can be used by UDRs for
various purposes.
module-name The name of the module that contains the procedure
routine-name The internal name of the procedure inside the external module
misc-info Optional string that is passed to the procedure in the external module
The PSQL Module Body
The PSQL module body starts with an optional section that declares variables and subroutines,
followed by a block of statements that run in a logical sequence, like a program. A block of
statements — or compound statement — is enclosed by the BEGIN and END keywords, and is executed
as a single unit of code. The main BEGIN…END block may contain any number of other BEGIN…END
blocks, both embedded and sequential. Blocks can be nested to a maximum depth of 512 blocks. All
statements except BEGIN and END are terminated by semicolons (‘ ;’). No other character is valid for
use as a terminator for PSQL statements.
Switching the Terminator in isql
Here we digress a little, to explain how to switch the terminator character in the isql utility to
make it possible to define PSQL modules in that environment without conflicting with isql
itself, which uses the same character, semicolon (‘;’), as its own statement terminator.
isql Command SET TERM
Sets the terminator character(s) to avoid conflict with the terminator character in PSQL
statements
Available in
ISQL only
Chapter 7. Procedural SQL (PSQL) Statements
351

<!-- Original PDF Page: 353 -->

Syntax
SET TERM new_terminator old_terminator
Table 95. SET TERM Parameters
Argument Description
new_terminator New terminator
old_terminator Old terminator
When you write your triggers, stored procedures, stored functions or PSQL blocks in
isql — either in the interactive interface or in scripts — running a SET TERM  statement is
needed to switch the normal isql statement terminator from the semicolon to another
character or short string, to avoid conflicts with the non-changeable semicolon terminator in
PSQL. The switch to an alternative terminator needs to be done before you begin defining
PSQL objects or running your scripts.
The alternative terminator can be any string of characters except for a space, an apostrophe
or the current terminator character(s). Any letter character(s) used will be case-sensitive.
Example
Changing the default semicolon to ‘ ^’ (caret) and using it to submit a stored procedure
definition: character as an alternative terminator character:
SET TERM ^;
CREATE OR ALTER PROCEDURE SHIP_ORDER (
  PO_NUM CHAR(8))
AS
BEGIN
  /* Stored procedure body */
END^
/* Other stored procedures and triggers */
SET TERM ;^
/* Other DDL statements */
The External Module Body
The external module body specifies the UDR engine used to execute the external module, and
optionally specifies the name of the UDR routine to call ( <extname>) and/or a string ( <extbody>)
with UDR-specific semantics.
Configuration of external modules and UDR engines is not covered further in this Language
Reference. Consult the documentation of a specific UDR engine for details.
Chapter 7. Procedural SQL (PSQL) Statements
352

<!-- Original PDF Page: 354 -->

7.2. Stored Procedures
A stored procedure is executable code stored in the database metadata for execution on the server.
It can be called by other stored procedures (including itself), functions, triggers and client
applications. A procedure that calls itself is known as recursive.
7.2.1. Benefits of Stored Procedures
Stored procedures have the following advantages:
Modularity
applications working with the database can use the same stored procedure, thereby reducing
the size of the application code and avoiding code duplication.
Simpler Application Support
when a stored procedure is modified, changes appear immediately to all host applications,
without the need to recompile them if the parameters were unchanged.
Enhanced Performance
since stored procedures are executed on a server instead of at the client, network traffic is
reduced, which improves performance.
7.2.2. Types of Stored Procedures
Firebird supports two types of stored procedures: executable and selectable.
Executable Procedures
Executable procedures usually modify data in a database. They can receive input parameters and
return a single set of output ( RETURNS) parameters. They are called using the EXECUTE PROCEDURE
statement. See an example of an executable stored procedure  at the end of the CREATE PROCEDURE
section of Chapter 5, Data Definition (DDL) Statements.
Selectable Procedures
Selectable stored procedures usually retrieve data from a database, returning an arbitrary number
of rows to the caller. The caller receives the output one row at a time from a row buffer that the
database engine prepares for it.
Selectable procedures can be useful for obtaining complex sets of data that are often impossible or
too difficult or too slow to retrieve using regular DSQL SELECT queries. Typically, this style of
procedure iterates through a looping process of extracting data, perhaps transforming it before
filling the output variables (parameters) with fresh data at each iteration of the loop. A SUSPEND
statement at the end of the iteration fills the buffer and waits for the caller to fetch the row.
Execution of the next iteration of the loop begins when the buffer has been cleared.
Selectable procedures may have input parameters, and the output set is specified by the RETURNS
clause in the header.
Chapter 7. Procedural SQL (PSQL) Statements
353

<!-- Original PDF Page: 355 -->

A selectable stored procedure is called with a SELECT statement. See an example of a selectable
stored procedure  at the end of the CREATE PROCEDURE section  of Chapter 5, Data Definition (DDL)
Statements.
7.2.3. Creating a Stored Procedure
The syntax for creating executable stored procedures and selectable stored procedures is the same.
The difference comes in the logic of the program code, specifically the absence or presence of a
SUSPEND statement.
For information about creating stored procedures, see CREATE PROCEDURE  in Chapter 5, Data
Definition (DDL) Statements.
7.2.4. Modifying a Stored Procedure
For information about modifying existing stored procedures, see ALTER PROCEDURE, CREATE OR ALTER
PROCEDURE, RECREATE PROCEDURE.
7.2.5. Dropping a Stored Procedure
For information about dropping (deleting) stored procedures, see DROP PROCEDURE.
7.3. Stored Functions
A stored function is executable code stored in the database metadata for execution on the server. It
can be called by other stored functions (including itself), procedures, triggers, and client
applications through DML statements. A function that calls itself is known as recursive.
Unlike stored procedures, stored functions always return one scalar value. To return a value from a
stored function, use the RETURN statement, which immediately terminates the function.
7.3.1. Creating a Stored Function
For information about creating stored functions, see CREATE FUNCTION in Chapter 5, Data Definition
(DDL) Statements.
7.3.2. Modifying a Stored Function
For information about modifying stored functions, see ALTER FUNCTION, CREATE OR ALTER FUNCTION ,
RECREATE FUNCTION.
7.3.3. Dropping a Stored Function
For information about dropping (deleting) stored functions, see DROP FUNCTION.
7.4. PSQL Blocks
A self-contained, unnamed (“anonymous”) block of PSQL code can be executed dynamically in
Chapter 7. Procedural SQL (PSQL) Statements
354

<!-- Original PDF Page: 356 -->

DSQL, using the EXECUTE BLOCK syntax. The header of a PSQL block may optionally contain input and
output parameters. The body may contain local variables, cursor declarations and local routines,
followed by a block of PSQL statements, and is similar to a stored procedure. A PSQL block cannot
use a UDR module body.
A PSQL block is not defined and stored as an object, unlike stored procedures and triggers. It
executes in run-time and cannot reference itself.
Like stored procedures, anonymous PSQL blocks can be used to process data and to retrieve data
from the database.
Syntax (incomplete)
EXECUTE BLOCK
  [(<inparam> = ? [, <inparam> = ? ...])]
  [RETURNS (<outparam> [, <outparam> ...])]
  <psql-module-body>
<psql-module-body> ::=
  !! See Syntax of Module Body !!
Table 96. PSQL Block Parameters
Argument Description
inparam Input parameter description
outparam Output parameter description
declarations A section for declaring local variables and named cursors
PSQL statements PSQL and DML statements
See also
See EXECUTE BLOCK for details.
7.5. Packages
A package is a group of stored procedures and functions defined as a single database object.
Firebird packages are made up of two parts: a header ( PACKAGE keyword) and a body ( PACKAGE BODY
keywords). This separation is similar to Delphi modules; the header corresponds to the interface
part, and the body corresponds to the implementation part.
7.5.1. Benefits of Packages
The notion of “packaging” the code components of a database operation addresses has several
advantages:
Modularisation
Blocks of interdependent code are grouped into logical modules, as done in other programming
Chapter 7. Procedural SQL (PSQL) Statements
355

<!-- Original PDF Page: 357 -->

languages.
In programming, it is well recognised that grouping code in various ways, in namespaces, units
or classes, for example, is a good thing. This is not possible with standard stored procedures and
functions in the database. Although they can be grouped in different script files, two problems
remain:
a. The grouping is not represented in the database metadata.
b. Scripted routines all participate in a flat namespace and are callable by everyone (we are not
referring to security permissions here).
Easier tracking of dependencies
Packages make it easy to track dependencies between a collection of related routines, as well as
between this collection and other routines, both packaged and unpackaged.
Whenever a packaged routine determines that it uses a certain database object, a dependency
on that object is registered in Firebird’s system tables. Thereafter, to drop, or maybe alter that
object, you first need to remove what depends on it. Since the dependency on other objects only
exists for the package body, and not the package header, this package body can easily be
removed, even if another object depends on this package. When the body is dropped, the header
remains, allowing you to recreate its body once the changes related to the removed object are
done.
Simplify permission management
As Firebird — by default — runs routines with the caller (invoker) privileges, it is necessary also
to grant resource usage to each routine when these resources would not be directly accessible to
the caller. Usage of each routine needs to be granted to users and/or roles.
Packaged routines do not have individual privileges. The privileges apply to the package as a
whole. Privileges granted to packages are valid for all package body routines, including private
ones, but are stored for the package header. An EXECUTE privilege on a package granted to a user
(or other object), grants that user the privilege to execute all routines defined in the package
header.
For example
GRANT SELECT ON TABLE secret TO PACKAGE pk_secret;
GRANT EXECUTE ON PACKAGE pk_secret TO ROLE role_secret;
Private scopes
Stored procedures and functions can be privates; that is, make them available only for internal
usage within the defining package.
All programming languages have the notion of routine scope, which is not possible without some
form of grouping. Firebird packages also work like Delphi units in this regard. If a routine is not
declared in the package header (interface) and is implemented in the body (implementation), it
becomes a private routine. A private routine can only be called from inside its package.
Chapter 7. Procedural SQL (PSQL) Statements
356

<!-- Original PDF Page: 358 -->

7.5.2. Creating a Package
For information on creating packages, see CREATE PACKAGE, and CREATE PACKAGE BODY  in Chapter 5,
Data Definition (DDL) Statements.
7.5.3. Modifying a Package
For information on modifying existing package header or bodies, see ALTER PACKAGE, CREATE OR ALTER
PACKAGE, RECREATE PACKAGE, ALTER PACKAGE BODY, and RECREATE PACKAGE BODY.
7.5.4. Dropping a Package
For information on dropping (deleting) a package, see DROP PACKAGE, and DROP PACKAGE BODY.
7.6. Triggers
A trigger is another form of executable code that is stored in the metadata of the database for
execution by the server. A trigger cannot be called directly. It is called automatically (“fired”) when
data-changing events involving one particular table or view occur, or on a specific database or DDL
event.
A trigger applies to exactly one table or view or database event, and only one phase in an event
(BEFORE or AFTER the event). A single DML trigger might be written to fire only when one specific
data-changing event occurs ( INSERT, UPDATE or DELETE), or it might be written to apply to more than
one of those.
A DML trigger is executed in the context of the transaction in which the data-changing DML
statement is running. For triggers that respond to database events, the rule is different: for DDL
triggers and transaction triggers, the trigger runs in the same transaction that executed the DDL, for
other types, a new default transaction is started.
7.6.1. Firing Order (Order of Execution)
More than one trigger can be defined for each phase-event combination. The order in which they
are executed — also known as “firing order” — can be specified explicitly with the optional POSITION
argument in the trigger definition. You have 32,767 numbers to choose from. Triggers with the
lowest position numbers fire first.
If a POSITION clause is omitted, or if several matching event-phase triggers have the same position
number, then the triggers will fire in alphabetical order.
7.6.2. DML Triggers
DML triggers are those that fire when a DML operation changes the state of data: updating rows in
tables, inserting new rows or deleting rows. They can be defined for both tables and views.
Trigger Options
Six base options are available for the event-phase combination for tables and views:
Chapter 7. Procedural SQL (PSQL) Statements
357

<!-- Original PDF Page: 359 -->

Before a new row is inserted BEFORE INSERT
After a new row is inserted AFTER INSERT
Before a row is updated BEFORE UPDATE
After a row is updated AFTER UPDATE
Before a row is deleted BEFORE DELETE
After a row is deleted AFTER DELETE
These base forms are for creating single phase/single-event triggers. Firebird also supports forms
for creating triggers for one phase and multiple-events, BEFORE INSERT OR UPDATE OR DELETE , for
example, or AFTER UPDATE OR DELETE: the combinations are your choice.
 “Multi-phase” triggers, such as BEFORE OR AFTER …, are not possible.
The Boolean context variables INSERTING, UPDATING and DELETING can be used in the body of a trigger
to determine the type of event that fired the trigger.
OLD and NEW Context Variables
For DML triggers, the Firebird engine provides access to sets of OLD and NEW context variables (or,
“records”). Each is a record of the values of the entire row: one for the values as they are before the
data-changing event (the BEFORE phase) and one for the values as they will be after the event (the
AFTER phase). They are referenced in statements using the form NEW.column_name and
OLD.column_name, respectively. The column_name can be any column in the table’s definition, not just
those that are being updated.
The NEW and OLD variables are subject to some rules:
• In all triggers, OLD is read-only
• In BEFORE UPDATE and BEFORE INSERT code, the NEW value is read/write, unless it is a COMPUTED BY
column
• In INSERT triggers, references to OLD are invalid and will throw an exception
• In DELETE triggers, references to NEW are invalid and will throw an exception
• In all AFTER trigger code, NEW is read-only
7.6.3. Database Triggers
A trigger associated with a database or transaction event can be defined for the following events:
Connecting to a
database
ON CONNECT Before the trigger is executed, a transaction is automatically
started with the default isolation level (snapshot
(concurrency), write, wait)
Disconnecting from
a database
ON DISCONNECT Before the trigger is executed, a transaction is automatically
started with the default isolation level (snapshot
(concurrency), write, wait)
Chapter 7. Procedural SQL (PSQL) Statements
358

<!-- Original PDF Page: 360 -->

When a transaction
is started
ON TRANSACTION
START
The trigger is executed in the transaction context of the
started transaction (immediately after start)
When a transaction
is committed
ON TRANSACTION
COMMIT
The trigger is executed in the transaction context of the
committing transaction (immediately before commit)
When a transaction
is cancelled
ON TRANSACTION
ROLLBACK
The trigger is executed in the transaction context of the
rolling back transaction (immediately before roll back)
7.6.4. DDL Triggers
DDL triggers fire on specified metadata change events in a specified phase. BEFORE triggers run
before changes to system tables. AFTER triggers run after changes to system tables.
DDL triggers are a specific type of database trigger, so most rules for and semantics of database
triggers also apply for DDL triggers.
Semantics
1. BEFORE triggers are fired before changes to the system tables. AFTER triggers are fired after
system table changes.
 Important Rule
The event type [BEFORE | AFTER] of a DDL trigger cannot be changed.
2. When a DDL statement fires a trigger that raises an exception ( BEFORE or AFTER, intentionally or
unintentionally) the statement will not be committed. That is, exceptions can be used to ensure
that a DDL operation will fail if the conditions are not precisely as intended.
3. DDL trigger actions are executed only when committing the transaction in which the affected
DDL command runs. Never overlook the fact that what is possible to do in an AFTER trigger is
exactly what is possible to do after a DDL command without autocommit. You cannot, for
example, create a table and then use it in the trigger.
4. With “CREATE OR ALTER” statements, a trigger is fired one time at the CREATE event or the ALTER
event, according to the previous existence of the object. With RECREATE statements, a trigger is
fired for the DROP event if the object exists, and for the CREATE event.
5. ALTER and DROP events are generally not fired when the object name does not exist. For the
exception, see point 6.
6. The exception to rule 5 is that BEFORE ALTER/DROP USER  triggers fire even when the username
does not exist. This is because, underneath, these commands perform DML on the security
database, and the verification is not done before the command on it is run. This is likely to be
different with embedded users, so do not write code that depends on this.
7. If an exception is raised after the DDL command starts its execution and before AFTER triggers
are fired, AFTER triggers will not be fired.
8. Packaged procedures and functions do not fire individual {CREATE | ALTER | DROP} {PROCEDURE |
FUNCTION} triggers.
Chapter 7. Procedural SQL (PSQL) Statements
359

<!-- Original PDF Page: 361 -->

The DDL_TRIGGER Context Namespace
When a DDL trigger is running, the DDL_TRIGGER namespace is available for use with
RDB$GET_CONTEXT. This namespace contains information on the currently firing trigger.
See also The DDL_TRIGGER Namespace in RDB$GET_CONTEXT in Chapter 8, Built-in Scalar Functions.
7.6.5. Creating Triggers
For information on creating triggers, see CREATE TRIGGER, CREATE OR ALTER TRIGGER , and RECREATE
TRIGGER in Chapter 5, Data Definition (DDL) Statements.
7.6.6. Modifying Triggers
For information on modifying triggers, see ALTER TRIGGER, CREATE OR ALTER TRIGGER , and RECREATE
TRIGGER.
7.6.7. Dropping a Trigger
For information on dropping (deleting) triggers, see DROP TRIGGER.
7.7. Writing the Body Code
This section takes a closer look at the procedural SQL language constructs and statements that are
available for coding the body of a stored procedure, functions, trigger, and PSQL blocks.
Colon Marker (‘:’)
The colon marker prefix (‘ :’) is used in PSQL to mark a reference to a variable in a DML
statement. The colon marker is not required before variable names in other PSQL code.
The colon prefix can also be used for the NEW and OLD contexts, and for cursor variables.
7.7.1. Assignment Statements
Assigns a value to a variable
Syntax
varname = <value_expr>;
Table 97. Assignment Statement Parameters
Argument Description
varname Name of a parameter or local variable
value_expr An expression, constant or variable whose value resolves to the same
data type as varname
Chapter 7. Procedural SQL (PSQL) Statements
360

<!-- Original PDF Page: 362 -->

PSQL uses the equal symbol (‘ =’) as its assignment operator. The assignment statement assigns a
SQL expression value on the right to the variable on the left of the operator. The expression can be
any valid SQL expression: it may contain literals, internal variable names, arithmetic, logical and
string operations, calls to internal functions, stored functions or external functions (UDFs).
Example using assignment statements
CREATE PROCEDURE MYPROC (
  a INTEGER,
  b INTEGER,
  name VARCHAR (30)
)
RETURNS (
  c INTEGER,
  str VARCHAR(100))
AS
BEGIN
  -- assigning a constant
  c = 0;
  str = '';
  SUSPEND;
  -- assigning expression values
  c = a + b;
  str = name || CAST(b AS VARCHAR(10));
  SUSPEND;
  -- assigning expression value built by a query
  c = (SELECT 1 FROM rdb$database);
  -- assigning a value from a context variable
  str = CURRENT_USER;
  SUSPEND;
END
See also
DECLARE VARIABLE
7.7.2. Management Statements in PSQL
Management statement are allowed in PSQL modules (triggers, procedures, functions and PSQL
blocks), which is especially helpful for applications that need management statements to be
executed at the start of a session, specifically in ON CONNECT triggers.
The management statements permitted in PSQL are:
ALTER SESSION RESET
SET BIND
SET DECFLOAT
SET ROLE
Chapter 7. Procedural SQL (PSQL) Statements
361

<!-- Original PDF Page: 363 -->

SET SESSION IDLE TIMEOUT
SET STATEMENT TIMEOUT
SET TIME ZONE
SET TRUSTED ROLE
Example of Management Statements in PSQL
create or alter trigger on_connect on connect
as
begin
    set bind of decfloat to double precision;
    set time zone 'America/Sao_Paulo';
end

Although useful as a workaround, using ON CONNECT triggers to configure bind and
time zone is usually not the right approach. Alternatives are handling this through
DefaultTimeZone in firebird.conf and DataTypeCompatibility in firebird.conf or
databases.conf, or isc_dpb_session_time_zone or isc_dpb_set_bind in the DPB.
See also
Management Statements
7.7.3. DECLARE VARIABLE
Declares a local variable
Syntax
DECLARE [VARIABLE] varname
  <domain_or_non_array_type> [NOT NULL] [COLLATE collation]
  [{DEFAULT | = } <initvalue>];
<domain_or_non_array_type> ::=
  !! See Scalar Data Types Syntax !!
<initvalue> ::= <literal> | <context_var>
Table 98. DECLARE VARIABLE Statement Parameters
Argument Description
varname Name of the local variable
collation Collation
initvalue Initial value for this variable
literal Literal of a type compatible with the type of the local variable
Chapter 7. Procedural SQL (PSQL) Statements
362

<!-- Original PDF Page: 364 -->

Argument Description
context_var Any context variable whose type is compatible with the type of the local
variable
The statement DECLARE [VARIABLE] is used for declaring a local variable. One DECLARE [VARIABLE]
statement is required for each local variable. Any number of DECLARE [VARIABLE] statements can be
included and in any order. The name of a local variable must be unique among the names of local
variables and input and output parameters declared for the module.
A special case of DECLARE [VARIABLE]  — declaring cursors — is covered separately in DECLARE ..
CURSOR
Data Type for Variables
A local variable can be of any SQL type other than an array.
• A domain name can be specified as the type; the variable will inherit all of its attributes.
• If the TYPE OF domain clause is used instead, the variable will inherit only the domain’s data type,
and, if applicable, its character set and collation attributes. Any default value or constraints
such as NOT NULL or CHECK constraints are not inherited.
• If the TYPE OF COLUMN relation.column option is used to “borrow” from a column in a table or
view, the variable will inherit only the column’s data type, and, if applicable, its character set
and collation attributes. Any other attributes are ignored.
NOT NULL Constraint
For local variables, you can specify the NOT NULL constraint, disallowing NULL values for the variable.
If a domain has been specified as the data type and the domain already has the NOT NULL constraint,
the declaration is unnecessary. For other forms, including use of a domain that is nullable, the NOT
NULL constraint can be included if needed.
CHARACTER SET and COLLATE clauses
Unless specified, the character set and collation of a string variable will be the database defaults. A
CHARACTER SET clause can be specified to handle string data that needs a different character set. A
valid collation (COLLATE clause) can also be included, with or without the character set clause.
Initializing a Variable
Local variables are NULL when execution of the module begins. They can be explicitly initialized so
that a starting or default value is available when they are first referenced. The initial value can be
specified in two ways, DEFAULT <initvalue>  and = <initvalue> . The value can be any type-
compatible literal or context variable, including NULL.
 Be sure to use this clause for any variables that have a NOT NULL constraint and do
not otherwise have a default value available (i.e. inherited from a domain).
Chapter 7. Procedural SQL (PSQL) Statements
363

<!-- Original PDF Page: 365 -->

Examples of various ways to declare local variables
CREATE OR ALTER PROCEDURE SOME_PROC
AS
  -- Declaring a variable of the INT type
  DECLARE I INT;
  -- Declaring a variable of the INT type that does not allow NULL
  DECLARE VARIABLE J INT NOT NULL;
  -- Declaring a variable of the INT type with the default value of 0
  DECLARE VARIABLE K INT DEFAULT 0;
  -- Declaring a variable of the INT type with the default value of 1
  DECLARE VARIABLE L INT = 1;
  -- Declaring a variable based on the COUNTRYNAME domain
  DECLARE FARM_COUNTRY COUNTRYNAME;
  -- Declaring a variable of the type equal to the COUNTRYNAME domain
  DECLARE FROM_COUNTRY TYPE OF COUNTRYNAME;
  -- Declaring a variable with the type of the CAPITAL column in the COUNTRY table
  DECLARE CAPITAL TYPE OF COLUMN COUNTRY.CAPITAL;
BEGIN
  /* PSQL statements */
END
See also
Data Types and Subtypes, Custom Data Types — Domains, CREATE DOMAIN
7.7.4. DECLARE .. CURSOR
Declares a named cursor
Syntax
DECLARE [VARIABLE] cursor_name
  [[NO] SCROLL] CURSOR
  FOR (<select>);
Table 99. DECLARE … CURSOR Statement Parameters
Argument Description
cursor_name Cursor name
select SELECT statement
The DECLARE … CURSOR …  FOR statement binds a named cursor to the result set obtained by the
SELECT statement specified in the FOR clause. In the body code, the cursor can be opened, used to
iterate row-by-row through the result set, and closed. While the cursor is open, the code can
perform positioned updates and deletes using the WHERE CURRENT OF  in the UPDATE or DELETE
statement.
 Syntactically, the DECLARE … CURSOR statement is a special case of DECLARE VARIABLE.
Chapter 7. Procedural SQL (PSQL) Statements
364

<!-- Original PDF Page: 366 -->

Forward-Only and Scrollable Cursors
The cursor can be forward-only (unidirectional) or scrollable. The optional clause SCROLL makes the
cursor scrollable, the NO SCROLL clause, forward-only. By default, cursors are forward-only.
Forward-only cursors can — as the name implies — only move forward in the dataset. Forward-only
cursors only support the FETCH [NEXT FROM] statement, other fetch options raise an error. Scrollable
cursors allow you to move not only forward in the dataset, but also back, as well as N positions
relative to the current position.
 Scrollable cursors are materialized as a temporary dataset, as such, they consume
additional memory or disk space, so use them only when you really need them.
Cursor Idiosyncrasies
• The optional FOR UPDATE clause can be included in the SELECT statement, but its absence does not
prevent successful execution of a positioned update or delete
• Care should be taken to ensure that the names of declared cursors do not conflict with any
names used subsequently in statements for AS CURSOR clauses
• If the cursor is needed only to walk the result set, it is nearly always easier and less error-prone
to use a FOR SELECT statement with the AS CURSOR clause. Declared cursors must be explicitly
opened, used to fetch data, and closed. The context variable ROW_COUNT has to be checked after
each fetch and, if its value is zero, the loop has to be terminated. A FOR SELECT statement does
this automatically.
Nevertheless, declared cursors provide a high level of control over sequential events and allow
several cursors to be managed in parallel.
• The SELECT statement may contain parameters. For instance:
SELECT NAME || :SFX FROM NAMES WHERE NUMBER = :NUM
Each parameter has to have been declared beforehand as a PSQL variable, or as input or output
parameters. When the cursor is opened, the parameter is assigned the current value of the
variable.

Unstable Variables and Cursors
If the value of the PSQL variable used in the SELECT statement of the cursor
changes during the execution of the loop, then its new value may — but not
always — be used when selecting the next rows. It is better to avoid such
situations. If you really need this behaviour, then you should thoroughly test your
code and make sure you understand how changes to the variable affect the query
results.
Note particularly that the behaviour may depend on the query plan, specifically on
the indexes being used. Currently, there are no strict rules for this behaviour, and
this may change in future versions of Firebird.
Chapter 7. Procedural SQL (PSQL) Statements
365

<!-- Original PDF Page: 367 -->

Examples Using Named Cursors
1. Declaring a named cursor in a trigger.
CREATE OR ALTER TRIGGER TBU_STOCK
  BEFORE UPDATE ON STOCK
AS
  DECLARE C_COUNTRY CURSOR FOR (
    SELECT
      COUNTRY,
      CAPITAL
    FROM COUNTRY
  );
BEGIN
  /* PSQL statements */
END
2. Declaring a scrollable cursor
EXECUTE BLOCK
  RETURNS (
    N INT,
    RNAME CHAR(63))
AS
  - Declaring a scrollable cursor
  DECLARE C SCROLL CURSOR FOR (
    SELECT
      ROW_NUMBER() OVER (ORDER BY RDB$RELATION_NAME) AS N,
      RDB$RELATION_NAME
    FROM RDB$RELATIONS
    ORDER BY RDB$RELATION_NAME);
BEGIN
  / * PSQL statements * /
END
3. A collection of scripts for creating views with a PSQL block using named cursors.
EXECUTE BLOCK
RETURNS (
  SCRIPT BLOB SUB_TYPE TEXT)
AS
  DECLARE VARIABLE FIELDS VARCHAR(8191);
  DECLARE VARIABLE FIELD_NAME TYPE OF RDB$FIELD_NAME;
  DECLARE VARIABLE RELATION RDB$RELATION_NAME;
  DECLARE VARIABLE SOURCE TYPE OF COLUMN RDB$RELATIONS.RDB$VIEW_SOURCE;
  DECLARE VARIABLE CUR_R CURSOR FOR (
    SELECT
      RDB$RELATION_NAME,
Chapter 7. Procedural SQL (PSQL) Statements
366

<!-- Original PDF Page: 368 -->

RDB$VIEW_SOURCE
    FROM
      RDB$RELATIONS
    WHERE
      RDB$VIEW_SOURCE IS NOT NULL);
  -- Declaring a named cursor where
  -- a local variable is used
  DECLARE CUR_F CURSOR FOR (
    SELECT
      RDB$FIELD_NAME
    FROM
      RDB$RELATION_FIELDS
    WHERE
      -- the variable must be declared earlier
      RDB$RELATION_NAME = :RELATION);
BEGIN
  OPEN CUR_R;
  WHILE (1 = 1) DO
  BEGIN
    FETCH CUR_R
    INTO :RELATION, :SOURCE;
    IF (ROW_COUNT = 0) THEN
      LEAVE;
    FIELDS = NULL;
    -- The CUR_F cursor will use the value
    -- of the RELATION variable initiated above
    OPEN CUR_F;
    WHILE (1 = 1) DO
    BEGIN
      FETCH CUR_F
      INTO :FIELD_NAME;
      IF (ROW_COUNT = 0) THEN
        LEAVE;
      IF (FIELDS IS NULL) THEN
        FIELDS = TRIM(FIELD_NAME);
      ELSE
        FIELDS = FIELDS || ', ' || TRIM(FIELD_NAME);
    END
    CLOSE CUR_F;
    SCRIPT = 'CREATE VIEW ' || RELATION;
    IF (FIELDS IS NOT NULL) THEN
      SCRIPT = SCRIPT || ' (' || FIELDS || ')';
    SCRIPT = SCRIPT || ' AS ' || ASCII_CHAR(13);
    SCRIPT = SCRIPT || SOURCE;
    SUSPEND;
  END
Chapter 7. Procedural SQL (PSQL) Statements
367

<!-- Original PDF Page: 369 -->

CLOSE CUR_R;
END
See also
OPEN, FETCH, CLOSE
7.7.5. DECLARE FUNCTION
Declares a sub-function
Syntax
<declare-subfunc> ::= <subfunc-forward> | <subfunc-def>
<subfunc-forward> ::= <subfunc-header>;
<subfunc-def> ::= <subfunc-header> <psql-module-body>
<subfunc-header>  ::=
  DECLARE FUNCTION subfuncname [ ( [ <in_params> ] ) ]
  RETURNS <domain_or_non_array_type> [COLLATE collation]
  [DETERMINISTIC]
<in_params> ::=
  !! See CREATE FUNCTION Syntax !!
<domain_or_non_array_type> ::=
  !! See Scalar Data Types Syntax !!
<psql-module-body> ::=
  !! See Syntax of Module Body !!
Table 100. DECLARE FUNCTION Statement Parameters
Argument Description
subfuncname Sub-function name
collation Collation name
The DECLARE FUNCTION statement declares a sub-function. A sub-function is only visible to the PSQL
module that defined the sub-function.
A sub-function can use variables, but not cursors, from its parent module. It can access other
routines from its parent modules, including recursive calls to itself.
Sub-functions have a number of restrictions:
• A sub-function cannot be nested in another subroutine. Subroutines are only supported in top-
level PSQL modules (stored procedures, stored functions, triggers and PSQL blocks). This
restriction is not enforced by the syntax, but attempts to create nested sub-functions will raise
Chapter 7. Procedural SQL (PSQL) Statements
368

<!-- Original PDF Page: 370 -->

an error “feature is not supported” with detail message “nested sub function”.
• Currently, a sub-function has no direct access to use cursors from its parent module.
A sub-function can be forward declared to resolve mutual dependencies between subroutines, and
must be followed by its actual definition. When a sub-function is forward declared and has
parameters with default values, the default values should only be specified in the forward
declaration, and should not be repeated in subfunc_def.

Declaring a sub-function with the same name as a stored function will hide that
stored function from your module. It will not be possible to call that stored
function.
 Contrary to DECLARE [VARIABLE] , a DECLARE FUNCTION  is not terminated by a
semicolon. The END of its main BEGIN … END block is considered its terminator.
Examples of Sub-Functions
1. Sub-function within a stored function
CREATE OR ALTER FUNCTION FUNC1 (n1 INTEGER, n2 INTEGER)
  RETURNS INTEGER
AS
- Subfunction
  DECLARE FUNCTION SUBFUNC (n1 INTEGER, n2 INTEGER)
    RETURNS INTEGER
  AS
  BEGIN
    RETURN n1 + n2;
  END
BEGIN
  RETURN SUBFUNC (n1, n2);
END
2. Recursive function call
execute block returns (i integer, o integer)
as
    -- Recursive function without forward declaration.
    declare function fibonacci(n integer) returns integer
    as
    begin
      if (n = 0 or n = 1) then
       return n;
     else
       return fibonacci(n - 1) + fibonacci(n - 2);
    end
begin
  i = 0;
Chapter 7. Procedural SQL (PSQL) Statements
369

<!-- Original PDF Page: 371 -->

while (i < 10)
  do
  begin
    o = fibonacci(i);
    suspend;
    i = i + 1;
  end
end
See also
DECLARE PROCEDURE, CREATE FUNCTION
7.7.6. DECLARE PROCEDURE
Declares a sub-procedure
Syntax
<declare-subproc> ::= <subproc-forward> | <subproc-def>
<subproc-forward> ::= <subproc-header>;
<subproc-def> ::= <subproc-header> <psql-module-body>
<subproc-header>  ::=
DECLARE subprocname [ ( [ <in_params> ] ) ]
  [RETURNS (<out_params>)]
<in_params> ::=
  !! See CREATE PROCEDURE Syntax !!
<domain_or_non_array_type> ::=
  !! See Scalar Data Types Syntax !!
<psql-module-body> ::=
  !! See Syntax of Module Body !!
Table 101. DECLARE PROCEDURE Statement Parameters
Argument Description
subprocname Sub-procedure name
collation Collation name
The DECLARE PROCEDURE statement declares a sub-procedure. A sub-procedure is only visible to the
PSQL module that defined the sub-procedure.
A sub-procedure can use variables, but not cursors, from its parent module. It can access other
routines from its parent modules.
Chapter 7. Procedural SQL (PSQL) Statements
370

<!-- Original PDF Page: 372 -->

Sub-procedures have a number of restrictions:
• A sub-procedure cannot be nested in another subroutine. Subroutines are only supported in
top-level PSQL modules (stored procedures, stored functions, triggers and PSQL blocks). This
restriction is not enforced by the syntax, but attempts to create nested sub-procedures will raise
an error “feature is not supported” with detail message “nested sub procedure”.
• Currently, the sub-procedure has no direct access to use cursors from its parent module.
A sub-procedure can be forward declared to resolve mutual dependencies between subroutines,
and must be followed by its actual definition. When a sub-procedure is forward declared and has
parameters with default values, the default values should only be specified in the forward
declaration, and should not be repeated in subproc_def.

Declaring a sub-procedure with the same name as a stored procedure, table or
view will hide that stored procedure, table or view from your module. It will not
be possible to call that stored procedure, table or view.
 Contrary to DECLARE [VARIABLE] , a DECLARE PROCEDURE  is not terminated by a
semicolon. The END of its main BEGIN … END block is considered its terminator.
Examples of Sub-Procedures
1. Subroutines in EXECUTE BLOCK
EXECUTE BLOCK
  RETURNS (name VARCHAR(63))
AS
  -- Sub-procedure returning a list of tables
  DECLARE PROCEDURE get_tables
    RETURNS (table_name VARCHAR(63))
  AS
  BEGIN
    FOR SELECT RDB$RELATION_NAME
      FROM RDB$RELATIONS
      WHERE RDB$VIEW_BLR IS NULL
      INTO table_name
    DO SUSPEND;
  END
  -- Sub-procedure returning a list of views
  DECLARE PROCEDURE get_views
    RETURNS (view_name VARCHAR(63))
  AS
  BEGIN
    FOR SELECT RDB$RELATION_NAME
      FROM RDB$RELATIONS
      WHERE RDB$VIEW_BLR IS NOT NULL
      INTO view_name
    DO SUSPEND;
  END
Chapter 7. Procedural SQL (PSQL) Statements
371

<!-- Original PDF Page: 373 -->

BEGIN
  FOR SELECT table_name
    FROM get_tables
    UNION ALL
    SELECT view_name
    FROM get_views
    INTO name
  DO SUSPEND;
END
2. With forward declaration and parameter with default value
execute block returns (o integer)
as
    -- Forward declaration of P1.
    declare procedure p1(i integer = 1) returns (o integer);
    -- Forward declaration of P2.
    declare procedure p2(i integer) returns (o integer);
    -- Implementation of P1 should not re-declare parameter default value.
    declare procedure p1(i integer) returns (o integer)
    as
    begin
        execute procedure p2(i) returning_values o;
    end
    declare procedure p2(i integer) returns (o integer)
    as
    begin
        o = i;
    end
begin
    execute procedure p1 returning_values o;
    suspend;
end
See also
DECLARE FUNCTION, CREATE PROCEDURE
7.7.7. BEGIN … END
Delimits a block of statements
Syntax
<block> ::=
  BEGIN
    [<compound_statement> ...]
Chapter 7. Procedural SQL (PSQL) Statements
372

<!-- Original PDF Page: 374 -->

END
<compound_statement> ::= {<block> | <statement>}
The BEGIN … END construct is a two-part statement that wraps a block of statements that are
executed as one unit of code. Each block starts with the keyword BEGIN and ends with the keyword
END. Blocks can be nested a maximum depth of 512 nested blocks. A block can be empty, allowing
them to act as stubs, without the need to write dummy statements.
The BEGIN … END itself should not be followed by a statement terminator (semicolon). However,
when defining or altering a PSQL module in the isql utility, that application requires that the last
END statement be followed by its own terminator character, that was previously switched — using
SET TERM — to a string other than a semicolon. That terminator is not part of the PSQL syntax.
The final, or outermost, END statement in a trigger terminates the trigger. What the final END
statement does in a stored procedure depends on the type of procedure:
• In a selectable procedure, the final END statement returns control to the caller, returning
SQLCODE 100, indicating that there are no more rows to retrieve
• In an executable procedure, the final END statement returns control to the caller, along with the
current values of any output parameters defined.
BEGIN … END Examples
A sample procedure from the employee.fdb database, showing simple usage of BEGIN … END blocks:
SET TERM ^;
CREATE OR ALTER PROCEDURE DEPT_BUDGET (
  DNO CHAR(3))
RETURNS (
  TOT DECIMAL(12,2))
AS
  DECLARE VARIABLE SUMB DECIMAL(12,2);
  DECLARE VARIABLE RDNO CHAR(3);
  DECLARE VARIABLE CNT  INTEGER;
BEGIN
  TOT = 0;
  SELECT BUDGET
  FROM DEPARTMENT
  WHERE DEPT_NO = :DNO
  INTO :TOT;
  SELECT COUNT(BUDGET)
  FROM DEPARTMENT
  WHERE HEAD_DEPT = :DNO
  INTO :CNT;
  IF (CNT = 0) THEN
    SUSPEND;
Chapter 7. Procedural SQL (PSQL) Statements
373

<!-- Original PDF Page: 375 -->

FOR SELECT DEPT_NO
    FROM DEPARTMENT
    WHERE HEAD_DEPT = :DNO
    INTO :RDNO
  DO
  BEGIN
    EXECUTE PROCEDURE DEPT_BUDGET(:RDNO)
      RETURNING_VALUES :SUMB;
    TOT = TOT + SUMB;
  END
  SUSPEND;
END^
SET TERM ;^
See also
EXIT, SET TERM
7.7.8. IF … THEN … ELSE
Conditional branching
Syntax
IF (<condition>)
  THEN <compound_statement>
  [ELSE <compound_statement>]
Table 102. IF … THEN … ELSE Parameters
Argument Description
condition A logical condition returning TRUE, FALSE or UNKNOWN
compound_statement A single statement, or statements wrapped in BEGIN … END
The conditional branch statement IF … THEN is used to branch the execution process in a PSQL
module. The condition is always enclosed in parentheses. If the condition returns the value TRUE,
execution branches to the statement or the block of statements after the keyword THEN. If an ELSE is
present, and the condition returns FALSE or UNKNOWN, execution branches to the statement or the
block of statements after it.
Multi-Branch Decisions
PSQL does not provide more advanced multi-branch jumps, such as CASE or SWITCH. However,
it is possible to chain IF …  THEN …  ELSE  statements, see the example section below.
Alternatively, the CASE statement from DSQL is available in PSQL and is able to satisfy at least
some use cases in the manner of a switch:
Chapter 7. Procedural SQL (PSQL) Statements
374

<!-- Original PDF Page: 376 -->

CASE <test_expr>
  WHEN <expr> THEN <result>
  [WHEN <expr> THEN <result> ...]
  [ELSE <defaultresult>]
END
CASE
  WHEN <bool_expr> THEN <result>
  [WHEN <bool_expr> THEN <result> ...]
  [ELSE <defaultresult>]
END
Example in PSQL
...
C = CASE
      WHEN A=2 THEN 1
      WHEN A=1 THEN 3
      ELSE 0
    END;
...
IF Examples
1. An example using the IF statement. Assume that the variables FIRST, LINE2 and LAST were
declared earlier.
...
IF (FIRST IS NOT NULL) THEN
  LINE2 = FIRST || ' ' || LAST;
ELSE
  LINE2 = LAST;
...
2. Given IF … THEN … ELSE is a statement, it is possible to chain them together. Assume that the
INT_VALUE and STRING_VALUE variables were declared earlier.
IF (INT_VALUE = 1) THEN
  STRING_VALUE = 'one';
ELSE IF (INT_VALUE = 2) THEN
  STRING_VALUE = 'two';
ELSE IF (INT_VALUE = 3) THEN
  STRING_VALUE = 'three';
ELSE
  STRING_VALUE = 'too much';
Chapter 7. Procedural SQL (PSQL) Statements
375

<!-- Original PDF Page: 377 -->

This specific example can be replaced with a simple CASE or the DECODE function.
See also
WHILE … DO, CASE
7.7.9. WHILE … DO
Looping construct
Syntax
[label:]
WHILE (<condition>) DO
  <compound_statement>
Table 103. WHILE … DO Parameters
Argument Description
label Optional label for LEAVE and CONTINUE. Follows the rules for identifiers.
condition A logical condition returning TRUE, FALSE or UNKNOWN
compound_statement A single statement, or statements wrapped in BEGIN … END
A WHILE statement implements the looping construct in PSQL. The statement or the block of
statements will be executed as long as the condition returns TRUE. Loops can be nested to any depth.
WHILE … DO Examples
A procedure calculating the sum of numbers from 1 to I shows how the looping construct is used.
CREATE PROCEDURE SUM_INT (I INTEGER)
RETURNS (S INTEGER)
AS
BEGIN
  s = 0;
  WHILE (i > 0) DO
  BEGIN
    s = s + i;
    i = i - 1;
  END
END
Executing the procedure in isql:
EXECUTE PROCEDURE SUM_INT(4);
the result is:
Chapter 7. Procedural SQL (PSQL) Statements
376

<!-- Original PDF Page: 378 -->

S
==========
10
See also
IF … THEN … ELSE, BREAK, LEAVE, CONTINUE, EXIT, FOR SELECT, FOR EXECUTE STATEMENT
7.7.10. BREAK
Exits a loop
Syntax
[label:]
<loop_stmt>
BEGIN
  ...
  BREAK;
  ...
END
<loop_stmt> ::=
    FOR <select_stmt> INTO <var_list> DO
  | FOR EXECUTE STATEMENT ... INTO <var_list> DO
  | WHILE (<condition>)} DO
Table 104. BREAK Statement Parameters
Argument Description
label Label
select_stmt SELECT statement
condition A logical condition returning TRUE, FALSE or UNKNOWN
The BREAK statement immediately terminates the inner loop of a WHILE or FOR looping statement.
Code continues to be executed from the first statement after the terminated loop block.
BREAK is similar to LEAVE, except it doesn’t support a label.
See also
LEAVE
7.7.11. LEAVE
Exits a loop
Syntax
[label:]
Chapter 7. Procedural SQL (PSQL) Statements
377

<!-- Original PDF Page: 379 -->

<loop_stmt>
BEGIN
  ...
  LEAVE [label];
  ...
END
<loop_stmt> ::=
    FOR <select_stmt> INTO <var_list> DO
  | FOR EXECUTE STATEMENT ... INTO <var_list> DO
  | WHILE (<condition>)} DO
Table 105. LEAVE Statement Parameters
Argument Description
label Label
select_stmt SELECT statement
condition A logical condition returning TRUE, FALSE or UNKNOWN
The LEAVE statement immediately terminates the inner loop of a WHILE or FOR looping statement.
Using the optional label parameter, LEAVE can also exit an outer loop, that is, the loop labelled with
label. Code continues to be executed from the first statement after the terminated loop block.
LEAVE Examples
1. Leaving a loop if an error occurs on an insert into the NUMBERS table. The code continues to be
executed from the line C = 0.
...
WHILE (B < 10) DO
BEGIN
  INSERT INTO NUMBERS(B)
  VALUES (:B);
  B = B + 1;
  WHEN ANY DO
  BEGIN
    EXECUTE PROCEDURE LOG_ERROR (
      CURRENT_TIMESTAMP,
      'ERROR IN B LOOP');
    LEAVE;
  END
END
C = 0;
...
2. An example using labels in the LEAVE statement. LEAVE LOOPA terminates the outer loop and LEAVE
LOOPB terminates the inner loop. Note that the plain LEAVE statement would be enough to
terminate the inner loop.
Chapter 7. Procedural SQL (PSQL) Statements
378

<!-- Original PDF Page: 380 -->

...
STMT1 = 'SELECT NAME FROM FARMS';
LOOPA:
FOR EXECUTE STATEMENT :STMT1
INTO :FARM DO
BEGIN
  STMT2 = 'SELECT NAME ' || 'FROM ANIMALS WHERE FARM = ''';
  LOOPB:
  FOR EXECUTE STATEMENT :STMT2 || :FARM || ''''
  INTO :ANIMAL DO
  BEGIN
    IF (ANIMAL = 'FLUFFY') THEN
      LEAVE LOOPB;
    ELSE IF (ANIMAL = FARM) THEN
      LEAVE LOOPA;
    SUSPEND;
  END
END
...
See also
BREAK, CONTINUE, EXIT
7.7.12. CONTINUE
Continues with the next iteration of a loop
Syntax
[label:]
<loop_stmt>
BEGIN
  ...
  CONTINUE [label];
  ...
END
<loop_stmt> ::=
    FOR <select_stmt> INTO <var_list> DO
  | FOR EXECUTE STATEMENT ... INTO <var_list> DO
  | WHILE (<condition>)} DO
Table 106. CONTINUE Statement Parameters
Argument Description
label Label
select_stmt SELECT statement
Chapter 7. Procedural SQL (PSQL) Statements
379

<!-- Original PDF Page: 381 -->

Argument Description
condition A logical condition returning TRUE, FALSE or UNKNOWN
The CONTINUE statement skips the remainder of the current block of a loop and starts the next
iteration of the current WHILE or FOR loop. Using the optional label parameter, CONTINUE can also start
the next iteration of an outer loop, that is, the loop labelled with label.
CONTINUE Examples
Using the CONTINUE statement
FOR SELECT A, D
  FROM ATABLE INTO achar, ddate
DO
BEGIN
  IF (ddate < current_date - 30) THEN
    CONTINUE;
  /* do stuff */
END
See also
BREAK, LEAVE, EXIT
7.7.13. EXIT
Terminates execution of a module
Syntax
EXIT;
The EXIT statement causes execution of the current PSQL module to jump to the final END statement
from any point in the code, thus terminating the program.
Calling EXIT in a function will result in the function returning NULL.
EXIT Examples
Using the EXIT statement in a selectable procedure
CREATE PROCEDURE GEN_100
  RETURNS (I INTEGER)
AS
BEGIN
  I = 1;
  WHILE (1=1) DO
  BEGIN
    SUSPEND;
    IF (I=100) THEN
Chapter 7. Procedural SQL (PSQL) Statements
380

<!-- Original PDF Page: 382 -->

EXIT;
    I = I + 1;
  END
END
See also
BREAK, LEAVE, CONTINUE, SUSPEND
7.7.14. SUSPEND
Passes output to the buffer and suspends execution while waiting for caller to fetch it
Syntax
SUSPEND;
The SUSPEND statement is used in selectable stored procedures to pass the values of output
parameters to a buffer and suspend execution. Execution remains suspended until the calling
application fetches the contents of the buffer. Execution resumes from the statement directly after
the SUSPEND statement. In practice, this is likely to be a new iteration of a looping process.

Important Notes
1. The SUSPEND statement can only occur in stored procedures or sub-procedures
2. The presence of the SUSPEND keyword defines a stored procedure as a selectable
procedure
3. Applications using interfaces that wrap the API perform the fetches from
selectable procedures transparently.
4. If a selectable procedure is executed using EXECUTE PROCEDURE, it behaves as an
executable procedure. When a SUSPEND statement is executed in such a stored
procedure, it is the same as executing the EXIT statement, resulting in
immediate termination of the procedure.
5. SUSPEND“breaks” the atomicity of the block in which it is located. If an error
occurs in a selectable procedure, statements executed after the final SUSPEND
statement will be rolled back. Statements that executed before the final SUSPEND
statement will not be rolled back unless the transaction is rolled back.
SUSPEND Examples
Using the SUSPEND statement in a selectable procedure
CREATE PROCEDURE GEN_100
  RETURNS (I INTEGER)
AS
BEGIN
  I = 1;
  WHILE (1=1) DO
Chapter 7. Procedural SQL (PSQL) Statements
381

<!-- Original PDF Page: 383 -->

BEGIN
    SUSPEND;
    IF (I=100) THEN
      EXIT;
    I = I + 1;
  END
END
See also
EXIT
7.7.15. EXECUTE STATEMENT
Executes dynamically created SQL statements
Syntax
<execute_statement> ::= EXECUTE STATEMENT <argument>
  [<option> ...]
  [INTO <variables>];
<argument> ::= <paramless_stmt>
            | (<paramless_stmt>)
            | (<stmt_with_params>) (<param_values>)
<param_values> ::= <named_values> | <positional_values>
<named_values> ::= <named_value> [, <named_value> ...]
<named_value> ::= [EXCESS] paramname := <value_expr>
<positional_values> ::= <value_expr> [, <value_expr> ...]
<option> ::=
    WITH {AUTONOMOUS | COMMON} TRANSACTION
  | WITH CALLER PRIVILEGES
  | AS USER user
  | PASSWORD password
  | ROLE role
  | ON EXTERNAL [DATA SOURCE] <connection_string>
<connection_string> ::=
  !! See <filespec> in the CREATE DATABASE syntax !!
<variables> ::= [:]varname [, [:]varname ...]
Table 107. EXECUTE STATEMENT Statement Parameters
Chapter 7. Procedural SQL (PSQL) Statements
382

<!-- Original PDF Page: 384 -->

Argument Description
paramless_stmt Literal string or variable containing a non-parameterized SQL query
stmt_with_params Literal string or variable containing a parameterized SQL query
paramname SQL query parameter name
value_expr SQL expression resolving to a value
user Username. It can be a string, CURRENT_USER or a string variable
password Password. It can be a string or a string variable
role Role. It can be a string, CURRENT_ROLE or a string variable
connection_string Connection string. It can be a string literal or a string variable
varname Variable
The statement EXECUTE STATEMENT takes a string parameter and executes it as if it were a DSQL
statement. If the statement returns data, it can be passed to local variables by way of an INTO clause.
EXECUTE STATEMENT can only produce a single row of data. Statements producing multiple rows of
data must be executed with FOR EXECUTE STATEMENT.
Parameterized Statements
You can use parameters — either named or positional — in the DSQL statement string. Each
parameter must be assigned a value.
Special Rules for Parameterized Statements
1. Named and positional parameters cannot be mixed in one query
2. Each parameter must be used in the statement text.
To relax this rule, named parameters can be prefixed with the keyword EXCESS to indicate that
the parameter may be absent from the statement text. This option is useful for dynamically
generated statements that conditionally include or exclude certain parameters.
3. If the statement has parameters, they must be enclosed in parentheses when EXECUTE STATEMENT
is called, regardless of whether they come directly as strings, as variable names or as
expressions
4. Each named parameter must be prefixed by a colon (‘ :’) in the statement string itself, but not
when the parameter is assigned a value
5. Positional parameters must be assigned their values in the same order as they appear in the
query text
6. The assignment operator for parameters is the special operator “ :=”, similar to the assignment
operator in Pascal
7. Each named parameter can be used in the statement more than once, but its value must be
assigned only once
8. With positional parameters, the number of assigned values must match the number of
Chapter 7. Procedural SQL (PSQL) Statements
383

<!-- Original PDF Page: 385 -->

parameter placeholders (question marks) in the statement exactly
9. A named parameter in the statement text can only be a regular identifier (it cannot be a quoted
identifier)
Examples of EXECUTE STATEMENT with parameters
1. With named parameters:
...
DECLARE license_num VARCHAR(15);
DECLARE connect_string VARCHAR (100);
DECLARE stmt VARCHAR (100) =
  'SELECT license '
  'FROM cars '
  'WHERE driver = :driver AND location = :loc';
BEGIN
  -- ...
  EXECUTE STATEMENT (stmt)
    (driver := current_driver,
     loc := current_location)
  ON EXTERNAL connect_string
  INTO license_num;
2. The same code with positional parameters:
DECLARE license_num VARCHAR (15);
DECLARE connect_string VARCHAR (100);
DECLARE stmt VARCHAR (100) =
  'SELECT license '
  'FROM cars '
  'WHERE driver = ? AND location = ?';
BEGIN
  -- ...
  EXECUTE STATEMENT (stmt)
    (current_driver, current_location)
  ON EXTERNAL connect_string
  INTO license_num;
3. Use of EXCESS to allow named parameters to be unused (note: this is a FOR EXECUTE STATEMENT):
CREATE PROCEDURE P_EXCESS (A_ID INT, A_TRAN INT = NULL, A_CONN INT = NULL)
  RETURNS (ID INT, TRAN INT, CONN INT)
AS
DECLARE S VARCHAR(255) = 'SELECT * FROM TTT WHERE ID = :ID';
DECLARE W VARCHAR(255) = '';
BEGIN
  IF (A_TRAN IS NOT NULL)
  THEN W = W || ' AND TRAN = :a';
Chapter 7. Procedural SQL (PSQL) Statements
384

<!-- Original PDF Page: 386 -->

IF (A_CONN IS NOT NULL)
  THEN W = W || ' AND CONN = :b';
  IF (W <> '')
  THEN S = S || W;
  -- could raise error if TRAN or CONN is null
  -- FOR EXECUTE STATEMENT (:S) (a := :A_TRAN, b := A_CONN, id := A_ID)
  -- OK in all cases
  FOR EXECUTE STATEMENT (:S) (EXCESS a := :A_TRAN, EXCESS b := A_CONN, id := A_ID)
    INTO :ID, :TRAN, :CONN
      DO SUSPEND;
END
WITH {AUTONOMOUS | COMMON} TRANSACTION
By default, the executed SQL statement runs within the current transaction. Using WITH AUTONOMOUS
TRANSACTION causes a separate transaction to be started, with the same parameters as the current
transaction. This separate transaction will be committed when the statement was executed without
errors and rolled back otherwise.
The clause WITH COMMON TRANSACTION  uses the current transaction whenever possible; this is the
default behaviour. If the statement must run in a separate connection, an already started
transaction within that connection is used, if available. Otherwise, a new transaction is started with
the same parameters as the current transaction. Any new transactions started under the “ COMMON”
regime are committed or rolled back with the current transaction.
WITH CALLER PRIVILEGES
By default, the SQL statement is executed with the privileges of the current user. Specifying WITH
CALLER PRIVILEGES combines the privileges of the calling procedure or trigger with those of the user,
as if the statement were executed directly by the routine. WITH CALLER PRIVILEGES has no effect if the
ON EXTERNAL clause is also present.
ON EXTERNAL [DATA SOURCE]
With ON EXTERNAL [DATA SOURCE], the SQL statement is executed in a separate connection to the same
or another database, possibly even on another server. If connection_string is NULL or “ ''” (empty
string), the entire ON EXTERNAL [DATA SOURCE]  clause is considered absent, and the statement is
executed against the current database.
Connection Pooling
• External connections made by statements WITH COMMON TRANSACTION  (the default) will remain
open until the current transaction ends. They can be reused by subsequent calls to EXECUTE
STATEMENT, but only if connection_string is identical, including case
• External connections made by statements WITH AUTONOMOUS TRANSACTION are closed as soon as the
statement has been executed
Chapter 7. Procedural SQL (PSQL) Statements
385

<!-- Original PDF Page: 387 -->

• Statements using WITH AUTONOMOUS TRANSACTION can and will re-use connections that were opened
earlier by statements WITH COMMON TRANSACTION. If this happens, the reused connection will be left
open after the statement has been executed. (It must be, because it has at least one active
transaction!)
Transaction Pooling
• If WITH COMMON TRANSACTION is in effect, transactions will be reused as much as possible. They will
be committed or rolled back together with the current transaction
• If WITH AUTONOMOUS TRANSACTION  is specified, a fresh transaction will always be started for the
statement. This transaction will be committed or rolled back immediately after the statement’s
execution
Exception Handling
When ON EXTERNAL is used, the extra connection is always made via a so-called external provider,
even if the connection is to the current database. One of the consequences is that exceptions cannot
be caught in the usual way. Every exception caused by the statement is wrapped in either an
eds_connection or an eds_statement error. To catch them in your PSQL code, you have to use WHEN
GDSCODE eds_connection, WHEN GDSCODE eds_statement or WHEN ANY.
 Without ON EXTERNAL, exceptions are caught in the usual way, even if an extra
connection is made to the current database.
Miscellaneous Notes
• The character set used for the external connection is the same as that for the current connection
• Two-phase commits are not supported
AS USER, PASSWORD and ROLE
The optional AS USER, PASSWORD and ROLE clauses allow specification of which user will execute the
SQL statement and with which role. The method of user login, and whether a separate connection is
opened, depends on the presence and values of the ON EXTERNAL [DATA SOURCE] , AS USER, PASSWORD
and ROLE clauses:
• If ON EXTERNAL is present, a new connection is always opened, and:
◦ If at least one of AS USER, PASSWORD and ROLE is present, native authentication is attempted
with the given parameter values (locally or remotely, depending on connection_string). No
defaults are used for missing parameters
◦ If all three are absent, and connection_string contains no hostname, then the new
connection is established on the local server with the same user and role as the current
connection. The term 'local' means “on the same machine as the server” here. This is not
necessarily the location of the client
◦ If all three are absent, and connection_string contains a hostname, then trusted
authentication is attempted on the remote host (again, 'remote' from the perspective of the
server). If this succeeds, the remote operating system will provide the username (usually the
Chapter 7. Procedural SQL (PSQL) Statements
386

<!-- Original PDF Page: 388 -->

operating system account under which the Firebird process runs)
• If ON EXTERNAL is absent:
◦ If at least one of AS USER, PASSWORD and ROLE is present, a new connection to the current
database is opened with the supplied parameter values. No defaults are used for missing
parameters
◦ If all three are absent, the statement is executed within the current connection

If a parameter value is NULL or “ ''” (empty string), the entire parameter is
considered absent. Additionally, AS USER is considered absent if its value is equal to
CURRENT_USER, and ROLE if it is the same as CURRENT_ROLE.
Caveats with EXECUTE STATEMENT
1. There is no way to validate the syntax of the enclosed statement
2. There are no dependency checks to discover whether tables or columns have been dropped
3. Execution is considerably slower than when the same statements are executed directly as PSQL
code
4. Return values are strictly checked for data type to avoid unpredictable type-casting exceptions.
For example, the string '1234' would convert to an integer, 1234, but 'abc' would give a
conversion error
All in all, this feature is meant to be used cautiously, and you should always take the caveats into
account. If you can achieve the same result with PSQL and/or DSQL, it will almost always be
preferable.
See also
FOR EXECUTE STATEMENT
7.7.16. FOR SELECT
Loops row-by-row through a query result set
Syntax
[label:]
FOR <select_stmt> [AS CURSOR cursor_name]
  DO <compound_statement>
Table 108. FOR SELECT Statement Parameters
Argument Description
label Optional label for LEAVE and CONTINUE. Follows the rules for identifiers.
select_stmt SELECT statement
cursor_name Cursor name. It must be unique among cursor names in the PSQL module
(stored procedure, stored function, trigger or PSQL block)
Chapter 7. Procedural SQL (PSQL) Statements
387

<!-- Original PDF Page: 389 -->

Argument Description
compound_statement A single statement, or statements wrapped in BEGIN…END, that performs
all the processing for this FOR loop
The FOR SELECT statement
• retrieves each row sequentially from the result set, and executes the statement or block of
statements for each row. In each iteration of the loop, the field values of the current row are
copied into pre-declared variables.
Including the AS CURSOR clause enables positioned deletes and updates to be performed — see
notes below
• can embed other FOR SELECT statements
• can contain named parameters that must be previously declared in the DECLARE VARIABLE 
statement or exist as input or output parameters of the procedure
• requires an INTO clause at the end of the SELECT … FROM … specification if AS CURSOR is absent In
each iteration of the loop, the field values of the current row are copied to the list of variables
specified in the INTO clause. The loop repeats until all rows are retrieved, after which it
terminates
• can be terminated before all rows are retrieved by using a BREAK, LEAVE or EXIT statement
The Undeclared Cursor
The optional AS CURSOR clause surfaces the result set of the FOR SELECT structure as an undeclared,
named cursor that can be operated on using the WHERE CURRENT OF  clause inside the statement or
block following the DO command, to delete or update the current row before execution moves to the
next row. In addition, it is possible to use the cursor name as a record variable (similar to OLD and
NEW in triggers), allowing access to the columns of the result set (i.e. cursor_name.columnname).
Rules for Cursor Variables
• When accessing a cursor variable in a DML statement, the colon prefix can be added before the
cursor name (i.e. :cursor_name.columnname) for disambiguation, similar to variables.
The cursor variable can be referenced without colon prefix, but in that case, depending on the
scope of the contexts in the statement, the name may resolve in the statement context instead of
to the cursor (e.g. you select from a table with the same name as the cursor).
• Cursor variables are read-only
• In a FOR SELECT statement without an AS CURSOR clause, you must use the INTO clause. If an AS
CURSOR clause is specified, the INTO clause is allowed, but optional; you can access the fields
through the cursor instead.
• Reading from a cursor variable returns the current field values. This means that an UPDATE
statement (with a WHERE CURRENT OF clause) will update not only the table, but also the fields in
the cursor variable for subsequent reads. Executing a DELETE statement (with a WHERE CURRENT OF
clause) will set all fields in the cursor variable to NULL for subsequent reads
Chapter 7. Procedural SQL (PSQL) Statements
388

<!-- Original PDF Page: 390 -->

Other points to take into account regarding undeclared cursors:
1. The OPEN, FETCH and CLOSE statements cannot be applied to a cursor surfaced by the AS CURSOR
clause
2. The cursor_name argument associated with an AS CURSOR clause must not clash with any names
created by DECLARE VARIABLE or DECLARE CURSOR statements at the top of the module body, nor
with any other cursors surfaced by an AS CURSOR clause
3. The optional FOR UPDATE clause in the SELECT statement is not required for a positioned update
Examples using FOR SELECT
1. A simple loop through query results:
CREATE PROCEDURE SHOWNUMS
RETURNS (
  AA INTEGER,
  BB INTEGER,
  SM INTEGER,
  DF INTEGER)
AS
BEGIN
  FOR SELECT DISTINCT A, B
      FROM NUMBERS
    ORDER BY A, B
    INTO AA, BB
  DO
  BEGIN
    SM = AA + BB;
    DF = AA - BB;
    SUSPEND;
  END
END
2. Nested FOR SELECT loop:
CREATE PROCEDURE RELFIELDS
RETURNS (
  RELATION CHAR(32),
  POS INTEGER,
  FIELD CHAR(32))
AS
BEGIN
  FOR SELECT RDB$RELATION_NAME
      FROM RDB$RELATIONS
      ORDER BY 1
      INTO :RELATION
  DO
  BEGIN
Chapter 7. Procedural SQL (PSQL) Statements
389

<!-- Original PDF Page: 391 -->

FOR SELECT
          RDB$FIELD_POSITION + 1,
          RDB$FIELD_NAME
        FROM RDB$RELATION_FIELDS
        WHERE
          RDB$RELATION_NAME = :RELATION
        ORDER BY RDB$FIELD_POSITION
        INTO :POS, :FIELD
    DO
    BEGIN
      IF (POS = 2) THEN
        RELATION = ' "';
      SUSPEND;
    END
  END
END
 Instead of nesting statements, this is generally better solved by using a single
statements with a join.
3. Using the AS CURSOR clause to surface a cursor for the positioned delete of a record:
CREATE PROCEDURE DELTOWN (
  TOWNTODELETE VARCHAR(24))
RETURNS (
  TOWN VARCHAR(24),
  POP INTEGER)
AS
BEGIN
  FOR SELECT TOWN, POP
      FROM TOWNS
      INTO :TOWN, :POP AS CURSOR TCUR
  DO
  BEGIN
    IF (:TOWN = :TOWNTODELETE) THEN
      -- Positional delete
      DELETE FROM TOWNS
      WHERE CURRENT OF TCUR;
    ELSE
      SUSPEND;
  END
END
4. Using an implicitly declared cursor as a cursor variable
EXECUTE BLOCK
 RETURNS (o CHAR(63))
Chapter 7. Procedural SQL (PSQL) Statements
390

<!-- Original PDF Page: 392 -->

AS
BEGIN
  FOR SELECT rdb$relation_name AS name
    FROM rdb$relations AS CURSOR c
  DO
  BEGIN
    o = c.name;
    SUSPEND;
  END
END
5. Disambiguating cursor variables within queries
EXECUTE BLOCK
  RETURNS (o1 CHAR(63), o2 CHAR(63))
AS
BEGIN
  FOR SELECT rdb$relation_name
    FROM rdb$relations
    WHERE
      rdb$relation_name = 'RDB$RELATIONS' AS CURSOR c
  DO
  BEGIN
    FOR SELECT
        -- with a prefix resolves to the cursor
        :c.rdb$relation_name x1,
        -- no prefix as an alias for the rdb$relations table
        c.rdb$relation_name x2
      FROM rdb$relations c
      WHERE
        rdb$relation_name = 'RDB$DATABASE' AS CURSOR d
    DO
    BEGIN
      o1 = d.x1;
      o2 = d.x2;
      SUSPEND;
    END
  END
END
See also
DECLARE .. CURSOR, BREAK, LEAVE, CONTINUE, EXIT, SELECT, UPDATE, DELETE
7.7.17. FOR EXECUTE STATEMENT
Executes dynamically created SQL statements and loops over its result set
Chapter 7. Procedural SQL (PSQL) Statements
391

<!-- Original PDF Page: 393 -->

Syntax
[label:]
FOR <execute_statement> DO <compound_statement>
Table 109. FOR EXECUTE STATEMENT Statement Parameters
Argument Description
label Optional label for LEAVE and CONTINUE. Follows the rules for identifiers.
execute_stmt An EXECUTE STATEMENT statement
compound_statement A single statement, or statements wrapped in BEGIN … END, that performs
all the processing for this FOR loop
The statement FOR EXECUTE STATEMENT is used, in a manner analogous to FOR SELECT, to loop through
the result set of a dynamically executed query that returns multiple rows.
FOR EXECUTE STATEMENT Examples
Executing a dynamically constructed SELECT query that returns a data set
CREATE PROCEDURE DynamicSampleThree (
   Q_FIELD_NAME VARCHAR(100),
   Q_TABLE_NAME VARCHAR(100)
) RETURNS(
  LINE VARCHAR(32000)
)
AS
  DECLARE VARIABLE P_ONE_LINE VARCHAR(100);
BEGIN
  LINE = '';
  FOR
    EXECUTE STATEMENT
      'SELECT T1.' || :Q_FIELD_NAME ||
      ' FROM ' || :Q_TABLE_NAME || ' T1 '
    INTO :P_ONE_LINE
  DO
    IF (:P_ONE_LINE IS NOT NULL) THEN
      LINE = :LINE || :P_ONE_LINE || ' ';
  SUSPEND;
END
See also
EXECUTE STATEMENT, BREAK, LEAVE, CONTINUE
7.7.18. OPEN
Opens a declared cursor
Chapter 7. Procedural SQL (PSQL) Statements
392

<!-- Original PDF Page: 394 -->

Syntax
OPEN cursor_name;
Table 110. OPEN Statement Parameter
Argument Description
cursor_name Cursor name. A cursor with this name must be previously declared with a
DECLARE CURSOR statement
An OPEN statement opens a previously declared cursor, executes its declared SELECT statement, and
makes the first record of the result data set ready to fetch. OPEN can be applied only to cursors
previously declared in a DECLARE .. CURSOR statement.

If the SELECT statement of the cursor has parameters, they must be declared as
local variables, or input or output parameters before the cursor is declared. When
the cursor is opened, the parameter is assigned the current value of the variable.
OPEN Examples
1. Using the OPEN statement:
SET TERM ^;
CREATE OR ALTER PROCEDURE GET_RELATIONS_NAMES
RETURNS (
  RNAME CHAR(63)
)
AS
  DECLARE C CURSOR FOR (
    SELECT RDB$RELATION_NAME
    FROM RDB$RELATIONS);
BEGIN
  OPEN C;
  WHILE (1 = 1) DO
  BEGIN
    FETCH C INTO :RNAME;
    IF (ROW_COUNT = 0) THEN
      LEAVE;
    SUSPEND;
  END
  CLOSE C;
END^
SET TERM ;^
2. A collection of scripts for creating views using a PSQL block with named cursors:
Chapter 7. Procedural SQL (PSQL) Statements
393

<!-- Original PDF Page: 395 -->

EXECUTE BLOCK
RETURNS (
  SCRIPT BLOB SUB_TYPE TEXT)
AS
  DECLARE VARIABLE FIELDS VARCHAR(8191);
  DECLARE VARIABLE FIELD_NAME TYPE OF RDB$FIELD_NAME;
  DECLARE VARIABLE RELATION RDB$RELATION_NAME;
  DECLARE VARIABLE SOURCE TYPE OF COLUMN RDB$RELATIONS.RDB$VIEW_SOURCE;
  -- named cursor
  DECLARE VARIABLE CUR_R CURSOR FOR (
    SELECT
      RDB$RELATION_NAME,
      RDB$VIEW_SOURCE
    FROM
      RDB$RELATIONS
    WHERE
      RDB$VIEW_SOURCE IS NOT NULL);
  -- named cursor with local variable
  DECLARE CUR_F CURSOR FOR (
    SELECT
      RDB$FIELD_NAME
    FROM
      RDB$RELATION_FIELDS
    WHERE
      -- Important! The variable has to be declared earlier
      RDB$RELATION_NAME = :RELATION);
BEGIN
  OPEN CUR_R;
  WHILE (1 = 1) DO
  BEGIN
    FETCH CUR_R
      INTO :RELATION, :SOURCE;
    IF (ROW_COUNT = 0) THEN
      LEAVE;
    FIELDS = NULL;
    -- The CUR_F cursor will use
    -- variable value of RELATION initialized above
    OPEN CUR_F;
    WHILE (1 = 1) DO
    BEGIN
      FETCH CUR_F
        INTO :FIELD_NAME;
      IF (ROW_COUNT = 0) THEN
        LEAVE;
      IF (FIELDS IS NULL) THEN
        FIELDS = TRIM(FIELD_NAME);
      ELSE
        FIELDS = FIELDS || ', ' || TRIM(FIELD_NAME);
    END
Chapter 7. Procedural SQL (PSQL) Statements
394

<!-- Original PDF Page: 396 -->

CLOSE CUR_F;
    SCRIPT = 'CREATE VIEW ' || RELATION;
    IF (FIELDS IS NOT NULL) THEN
      SCRIPT = SCRIPT || ' (' || FIELDS || ')';
    SCRIPT = SCRIPT || ' AS ' || ASCII_CHAR(13);
    SCRIPT = SCRIPT || SOURCE;
    SUSPEND;
  END
  CLOSE CUR_R;
END
See also
DECLARE .. CURSOR, FETCH, CLOSE
7.7.19. FETCH
Fetches a record from a cursor
Syntax
FETCH [<fetch_scroll> FROM] cursor_name
  [INTO [:]varname [, [:]varname ...]];
<fetch_scroll> ::=
    NEXT | PRIOR | FIRST | LAST
  | RELATIVE n | ABSOLUTE n
Table 111. FETCH Statement Parameters
Argument Description
cursor_name Cursor name. A cursor with this name must be previously declared with a
DECLARE … CURSOR statement and opened by an OPEN statement.
varname Variable name
n Integer expression for the number of rows
The FETCH statement fetches the next row from the result set of the cursor and assigns the column
values to PSQL variables. The FETCH statement can be used only with a cursor declared with the
DECLARE .. CURSOR statement.
Using the optional fetch_scroll part of the FETCH statement, you can specify in which direction and
how many rows to advance the cursor position. The NEXT fetch option can be used for scrollable and
forward-only cursors. Other fetch options are only supported for scrollable cursors.
The Fetch Options
Chapter 7. Procedural SQL (PSQL) Statements
395

<!-- Original PDF Page: 397 -->

NEXT
moves the cursor one row forward; this is the default
PRIOR
moves the cursor one record back
FIRST
moves the cursor to the first record.
LAST
moves the cursor to the last record
RELATIVE n
moves the cursor n rows from the current position; positive numbers move forward, negative
numbers move backwards; using zero ( 0) will not move the cursor, and ROW_COUNT will be set to
zero as no new row was fetched.
ABSOLUTE n
moves the cursor to the specified row; n is an integer expression, where 1 indicates the first row.
For negative values, the absolute position is taken from the end of the result set, so -1 indicates
the last row, -2 the second to last row, etc. A value of zero (0) will position before the first row.
The optional INTO clause gets data from the current row of the cursor and loads them into PSQL
variables. If a fetch moves beyond the bounds of the result set, the variables will be set to NULL.
It is also possible to use the cursor name as a variable of a record type (similar to OLD and NEW in
triggers), allowing access to the columns of the result set (i.e. cursor_name.columnname).
Rules for Cursor Variables
• When accessing a cursor variable in a DML statement, the colon prefix can be added before the
cursor name (i.e. :cursor_name.columnname) for disambiguation, similar to variables.
The cursor variable can be referenced without colon prefix, but in that case, depending on the
scope of the contexts in the statement, the name may resolve in the statement context instead of
to the cursor (e.g. you select from a table with the same name as the cursor).
• Cursor variables are read-only
• In a FOR SELECT statement without an AS CURSOR clause, you must use the INTO clause. If an AS
CURSOR clause is specified, the INTO clause is allowed, but optional; you can access the fields
through the cursor instead.
• Reading from a cursor variable returns the current field values. This means that an UPDATE
statement (with a WHERE CURRENT OF clause) will update not only the table, but also the fields in
the cursor variable for subsequent reads. Executing a DELETE statement (with a WHERE CURRENT OF
clause) will set all fields in the cursor variable to NULL for subsequent reads
• When the cursor is not positioned on a row — it is positioned before the first row, or after the
last row — attempts to read from the cursor variable will result in error “ Cursor cursor_name
is not positioned in a valid record”
Chapter 7. Procedural SQL (PSQL) Statements
396

<!-- Original PDF Page: 398 -->

For checking whether all the rows of the result set have been fetched, the context variable
ROW_COUNT returns the number of rows fetched by the statement. If a record was fetched, then
ROW_COUNT is one (1), otherwise zero (0).
FETCH Examples
1. Using the FETCH statement:
CREATE OR ALTER PROCEDURE GET_RELATIONS_NAMES
  RETURNS (RNAME CHAR(63))
AS
  DECLARE C CURSOR FOR (
    SELECT RDB$RELATION_NAME
    FROM RDB$RELATIONS);
BEGIN
  OPEN C;
  WHILE (1 = 1) DO
  BEGIN
    FETCH C INTO RNAME;
    IF (ROW_COUNT = 0) THEN
      LEAVE;
    SUSPEND;
  END
  CLOSE C;
END
2. Using the FETCH statement with nested cursors:
EXECUTE BLOCK
  RETURNS (SCRIPT BLOB SUB_TYPE TEXT)
AS
  DECLARE VARIABLE FIELDS VARCHAR (8191);
  DECLARE VARIABLE FIELD_NAME TYPE OF RDB$FIELD_NAME;
  DECLARE VARIABLE RELATION RDB$RELATION_NAME;
  DECLARE VARIABLE SRC TYPE OF COLUMN RDB$RELATIONS.RDB$VIEW_SOURCE;
  -- Named cursor declaration
  DECLARE VARIABLE CUR_R CURSOR FOR (
    SELECT
      RDB$RELATION_NAME,
      RDB$VIEW_SOURCE
    FROM RDB$RELATIONS
    WHERE RDB$VIEW_SOURCE IS NOT NULL);
  -- Declaring a named cursor in which
  -- a local variable is used
  DECLARE CUR_F CURSOR FOR (
    SELECT RDB$FIELD_NAME
    FROM RDB$RELATION_FIELDS
    WHERE
    -- the variable must be declared earlier
Chapter 7. Procedural SQL (PSQL) Statements
397

<!-- Original PDF Page: 399 -->

RDB$RELATION_NAME =: RELATION);
BEGIN
  OPEN CUR_R;
  WHILE (1 = 1) DO
  BEGIN
    FETCH CUR_R INTO RELATION, SRC;
    IF (ROW_COUNT = 0) THEN
      LEAVE;
    FIELDS = NULL;
    -- Cursor CUR_F will use the value
    -- the RELATION variable initialized above
    OPEN CUR_F;
    WHILE (1 = 1) DO
    BEGIN
      FETCH CUR_F INTO FIELD_NAME;
      IF (ROW_COUNT = 0) THEN
        LEAVE;
      IF (FIELDS IS NULL) THEN
        FIELDS = TRIM (FIELD_NAME);
      ELSE
        FIELDS = FIELDS || ',' || TRIM(FIELD_NAME);
    END
    CLOSE CUR_F;
    SCRIPT = 'CREATE VIEW' || RELATION;
    IF (FIELDS IS NOT NULL) THEN
      SCRIPT = SCRIPT || '(' || FIELDS || ')' ;
    SCRIPT = SCRIPT || 'AS' || ASCII_CHAR (13);
    SCRIPT = SCRIPT || SRC;
    SUSPEND;
  END
  CLOSE CUR_R;
EN
3. An example of using the FETCH statement with a scrollable cursor
EXECUTE BLOCK
  RETURNS (N INT, RNAME CHAR (63))
AS
  DECLARE C SCROLL CURSOR FOR (
    SELECT
      ROW_NUMBER() OVER (ORDER BY RDB$RELATION_NAME) AS N,
      RDB$RELATION_NAME
    FROM RDB$RELATIONS
    ORDER BY RDB$RELATION_NAME);
BEGIN
  OPEN C;
  -- move to the first record (N = 1)
  FETCH FIRST FROM C;
  RNAME = C.RDB$RELATION_NAME;
  N = C.N;
Chapter 7. Procedural SQL (PSQL) Statements
398

<!-- Original PDF Page: 400 -->

SUSPEND;
  -- move 1 record forward (N = 2)
  FETCH NEXT FROM C;
  RNAME = C.RDB$RELATION_NAME;
  N = C.N;
  SUSPEND;
  -- move to the fifth record (N = 5)
  FETCH ABSOLUTE 5 FROM C;
  RNAME = C.RDB$RELATION_NAME;
  N = C.N;
  SUSPEND;
  -- move 1 record backward (N = 4)
  FETCH PRIOR FROM C;
  RNAME = C.RDB$RELATION_NAME;
  N = C.N;
  SUSPEND;
  -- move 3 records forward (N = 7)
  FETCH RELATIVE 3 FROM C;
  RNAME = C.RDB$RELATION_NAME;
  N = C.N;
  SUSPEND;
  -- move back 5 records (N = 2)
  FETCH RELATIVE -5 FROM C;
  RNAME = C.RDB$RELATION_NAME;
  N = C.N;
  SUSPEND;
  -- move to the first record (N = 1)
  FETCH FIRST FROM C;
  RNAME = C.RDB$RELATION_NAME;
  N = C.N;
  SUSPEND;
  -- move to the last entry
  FETCH LAST FROM C;
  RNAME = C.RDB$RELATION_NAME;
  N = C.N;
  SUSPEND;
  CLOSE C;
END
See also
DECLARE .. CURSOR, OPEN, CLOSE
7.7.20. CLOSE
Closes a declared cursor
Syntax
CLOSE cursor_name;
Chapter 7. Procedural SQL (PSQL) Statements
399

<!-- Original PDF Page: 401 -->

Table 112. CLOSE Statement Parameter
Argument Description
cursor_name Cursor name. A cursor with this name must be previously declared with a
DECLARE … CURSOR statement and opened by an OPEN statement
A CLOSE statement closes an open cursor. Only a cursor that was declared with DECLARE .. CURSOR
can be closed with a CLOSE statement. Any cursors that are still open will be automatically closed
after the module code completes execution.
CLOSE Examples
See FETCH Examples
See also
DECLARE .. CURSOR, OPEN, FETCH
7.7.21. IN AUTONOMOUS TRANSACTION
Executes a statement or a block of statements in an autonomous transaction
Syntax
IN AUTONOMOUS TRANSACTION DO <compound_statement>
Table 113. IN AUTONOMOUS TRANSACTION Statement Parameter
Argument Description
compound_statement A single statement, or statements wrapped in BEGIN … END
The IN AUTONOMOUS TRANSACTION statement enables execution of a statement or a block of statements
in an autonomous transaction. Code running in an autonomous transaction will be committed right
after its successful execution, regardless of the status of its parent transaction. This can be used
when certain operations must not be rolled back, even if an error occurs in the parent transaction.
An autonomous transaction has the same isolation level as its parent transaction. Any exception
that is thrown in the block of the autonomous transaction code will result in the autonomous
transaction being rolled back and all changes made will be undone. If the code executes
successfully, the autonomous transaction will be committed.
IN AUTONOMOUS TRANSACTION Examples
Using an autonomous transaction in a trigger for the database ON CONNECT  event, to log all
connection attempts, including those that failed:
CREATE TRIGGER TR_CONNECT ON CONNECT
AS
BEGIN
  -- Logging all attempts to connect to the database
Chapter 7. Procedural SQL (PSQL) Statements
400

<!-- Original PDF Page: 402 -->

IN AUTONOMOUS TRANSACTION DO
    INSERT INTO LOG(MSG)
    VALUES ('USER ' || CURRENT_USER || ' CONNECTS.');
  IF (EXISTS(SELECT *
             FROM BLOCKED_USERS
             WHERE USERNAME = CURRENT_USER)) THEN
  BEGIN
    -- Logging that the attempt to connect
    -- to the database failed and sending
    -- a message about the event
    IN AUTONOMOUS TRANSACTION DO
    BEGIN
      INSERT INTO LOG(MSG)
      VALUES ('USER ' || CURRENT_USER || ' REFUSED.');
      POST_EVENT 'CONNECTION ATTEMPT BY BLOCKED USER!';
    END
    -- now calling an exception
    EXCEPTION EX_BADUSER;
  END
END
See also
Transaction Control
7.7.22. POST_EVENT
Posts an event for notification to registered clients on commit
Syntax
POST_EVENT event_name;
Table 114. POST_EVENT Statement Parameter
Argument Description
event_name Event name (message) limited to 127 bytes
The POST_EVENT statement notifies the event manager about the event, which saves it to an event
table. When the transaction is committed, the event manager notifies applications that have
registered their interest in the event.
The event name can be a code, or a short message: the choice is open as it is a string of up to 127
bytes. Keep in mind that the application listening for an event must use the exact event name when
registering.
The content of the string can be a string literal, a variable or any valid SQL expression that resolves
to a string.
Chapter 7. Procedural SQL (PSQL) Statements
401

<!-- Original PDF Page: 403 -->

POST_EVENT Examples
Notifying the listening applications about inserting a record into the SALES table:
CREATE TRIGGER POST_NEW_ORDER FOR SALES
ACTIVE AFTER INSERT POSITION 0
AS
BEGIN
  POST_EVENT 'new_order';
END
7.7.23. RETURN
Returns a value from a stored function
Syntax
RETURN value;
Table 115. RETURN Statement Parameter
Argument Description
value Expression with the value to return; Can be any expression type-
compatible with the return type of the function
The RETURN statement ends the execution of a function and returns the value of the expression
value.
RETURN can only be used in PSQL functions (stored functions and local sub-functions).
RETURN Examples
See CREATE FUNCTION Examples
7.8. Trapping and Handling Errors
Firebird has a useful lexicon of PSQL statements and resources for trapping errors in modules and
for handling them. Firebird uses built-in exceptions that are raised for errors occurring when
working DML and DDL statements.
In PSQL code, exceptions are handled by means of the WHEN statement. Handling an exception in the
code involves either fixing the problem in situ, or stepping past it; either solution allows execution
to continue without returning an exception message to the client.
An exception results in execution being terminated in the current block. Instead of passing the
execution to the END statement, the procedure moves outward through levels of nested blocks,
starting from the block where the exception is caught, searching for the code of the handler that
“knows” about this exception. It stops searching when it finds the first WHEN statement that can
Chapter 7. Procedural SQL (PSQL) Statements
402

<!-- Original PDF Page: 404 -->

handle this exception.
7.8.1. System Exceptions
An exception is a message that is generated when an error occurs.
All exceptions handled by Firebird have predefined numeric values for context variables (symbols)
and text messages associated with them. Error messages are output in English by default. Localized
Firebird builds are available, where error messages are translated into other languages.
Complete listings of the system exceptions can be found in Appendix B, Exception Codes and
Messages:
• SQLSTATE Error Codes and Descriptions
• "GDSCODE Error Codes, SQLCODEs and Descriptions"
7.8.2. Custom Exceptions
Custom exceptions can be declared in the database as persistent objects and called in PSQL code to
signal specific errors; for example, to enforce certain business rules. A custom exception consists of
an identifier, and a default message of 1021 bytes. For details, see CREATE EXCEPTION.
7.8.3. EXCEPTION
Throws a user-defined exception or rethrows an exception
Syntax
EXCEPTION [
    exception_name
    [ custom_message
    | USING (<value_list>)]
  ]
<value_list> ::= <val> [, <val> ...]
Table 116. EXCEPTION Statement Parameters
Argument Description
exception_name Exception name
custom_message Alternative message text to be returned to the caller interface when an
exception is thrown. Maximum length of the text message is 1,021 bytes
val Value expression that replaces parameter slots in the exception message
text
The EXCEPTION statement with exception_name throws the user-defined exception with the specified
name. An alternative message text of up to 1,021 bytes can optionally override the exception’s
default message text.
Chapter 7. Procedural SQL (PSQL) Statements
403

<!-- Original PDF Page: 405 -->

The default exception message can contain slots for parameters that can be filled when throwing
an exception. To pass parameter values to an exception, use the USING clause. Considering, in left-to-
right order, each parameter passed in the exception-raising statement as “the Nth”, with N starting
at 1:
• If the Nth parameter is not passed, its slot is not replaced
• If a NULL parameter is passed, the slot will be replaced with the string “*** null ***”
• If more parameters are passed than are defined in the exception message, the surplus ones are
ignored
• The maximum number of parameters is 9
• The maximum message length, including parameter values, is 1053 bytes

The status vector is generated this code combination isc_except, <exception
number>, isc_formatted_exception, <formatted exception message>, <exception
parameters>.
The error code used ( isc_formatted_exception) was introduced in Firebird 3.0, so
the client must be at least version 3.0, or at least use the firebird.msg from version
3.0 or higher, to translate the status vector to a string.

If the message contains a parameter slot number that is greater than 9, the second
and subsequent digits will be treated as literal text. For example @10 will be
interpreted as slot 1 followed by a literal ‘0’.
As an example:
CREATE EXCEPTION ex1
  'something wrong in @1@2@3@4@5@6@7@8@9@10@11';
SET TERM ^;
EXECUTE BLOCK AS
BEGIN
  EXCEPTION ex1 USING ('a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i');
END^
This will produce the following output
Statement failed, SQLSTATE = HY000
exception 1
-EX1
-something wrong in abcdefghia0a1
Exceptions can be handled in a WHEN … DO statement. If an exception is not handled in a module,
then the effects of the actions executed inside this module are cancelled, and the caller program
receives the exception (either the default text, or the custom text).
Within the exception-handling block — and only within it — the caught exception can be re-thrown
Chapter 7. Procedural SQL (PSQL) Statements
404

<!-- Original PDF Page: 406 -->

by executing the EXCEPTION statement without parameters. If located outside the block, the re-
thrown EXCEPTION call has no effect.
Custom exceptions are stored in the system table RDB$EXCEPTIONS.
EXCEPTION Examples
1. Throwing an exception upon a condition in the SHIP_ORDER stored procedure:
CREATE OR ALTER PROCEDURE SHIP_ORDER (
  PO_NUM CHAR(8))
AS
  DECLARE VARIABLE ord_stat  CHAR(7);
  DECLARE VARIABLE hold_stat CHAR(1);
  DECLARE VARIABLE cust_no   INTEGER;
  DECLARE VARIABLE any_po    CHAR(8);
BEGIN
  SELECT
    s.order_status,
    c.on_hold,
    c.cust_no
  FROM
    sales s, customer c
  WHERE
    po_number = :po_num AND
    s.cust_no = c.cust_no
  INTO :ord_stat,
       :hold_stat,
       :cust_no;
  IF (ord_stat = 'shipped') THEN
    EXCEPTION order_already_shipped;
  /* Other statements */
END
2. Throwing an exception upon a condition and replacing the original message with an alternative
message:
CREATE OR ALTER PROCEDURE SHIP_ORDER (
  PO_NUM CHAR(8))
AS
  DECLARE VARIABLE ord_stat  CHAR(7);
  DECLARE VARIABLE hold_stat CHAR(1);
  DECLARE VARIABLE cust_no   INTEGER;
  DECLARE VARIABLE any_po    CHAR(8);
BEGIN
  SELECT
    s.order_status,
    c.on_hold,
Chapter 7. Procedural SQL (PSQL) Statements
405

<!-- Original PDF Page: 407 -->

c.cust_no
  FROM
    sales s, customer c
  WHERE
    po_number = :po_num AND
    s.cust_no = c.cust_no
  INTO :ord_stat,
       :hold_stat,
       :cust_no;
  IF (ord_stat = 'shipped') THEN
    EXCEPTION order_already_shipped
      'Order status is "' || ord_stat || '"';
  /* Other statements */
END
3. Using a parameterized exception:
CREATE EXCEPTION EX_BAD_SP_NAME
  'Name of procedures must start with' '@ 1' ':' '@ 2' '' ;
...
CREATE TRIGGER TRG_SP_CREATE BEFORE CREATE PROCEDURE
AS
  DECLARE SP_NAME VARCHAR(255);
BEGIN
  SP_NAME = RDB$GET_CONTEXT ('DDL_TRIGGER' , 'OBJECT_NAME');
  IF (SP_NAME NOT STARTING 'SP_') THEN
    EXCEPTION EX_BAD_SP_NAME USING ('SP_', SP_NAME);
END
4. Logging an error and re-throwing it in the WHEN block:
CREATE PROCEDURE ADD_COUNTRY (
  ACountryName COUNTRYNAME,
  ACurrency VARCHAR(10))
AS
BEGIN
  INSERT INTO country (country,
                       currency)
  VALUES (:ACountryName,
          :ACurrency);
  WHEN ANY DO
  BEGIN
    -- write an error in log
    IN AUTONOMOUS TRANSACTION DO
      INSERT INTO ERROR_LOG (PSQL_MODULE,
                             GDS_CODE,
                             SQL_CODE,
                             SQL_STATE)
Chapter 7. Procedural SQL (PSQL) Statements
406

<!-- Original PDF Page: 408 -->

VALUES ('ADD_COUNTRY',
              GDSCODE,
              SQLCODE,
              SQLSTATE);
    -- Re-throw exception
    EXCEPTION;
  END
END
See also
CREATE EXCEPTION, WHEN … DO
7.8.4. WHEN … DO
Catches an exception for error handling
Syntax
WHEN {<error> [, <error> ...] | ANY}
DO <compound_statement>
<error> ::=
  { EXCEPTION exception_name
  | SQLCODE number
  | GDSCODE errcode
  | SQLSTATE sqlstate_code }
Table 117. WHEN … DO Statement Parameters
Argument Description
exception_name Exception name
number SQLCODE error code
errcode Symbolic GDSCODE error name
sqlstate_code String literal with the SQLSTATE error code
compound_statement A single statement, or a block of statements
The WHEN …  DO  statement handles Firebird errors and user-defined exceptions. The statement
catches all errors and user-defined exceptions listed after the keyword WHEN keyword. If WHEN is
followed by the keyword ANY, the statement catches any error or user-defined exception, even if
they have already been handled in a WHEN block located higher up.
The WHEN … DO statements must be located at the end of a block of statements, before the block’s END
statement, and after any other statement.
The keyword DO is followed by a single statement, or statements wrapped in a BEGIN … END block,
that handles the exception. The SQLCODE, GDSCODE, and SQLSTATE context variables are available in the
context of this statement or block. Use the RDB$ERROR function to obtain the SQLCODE, GDSCODE,
Chapter 7. Procedural SQL (PSQL) Statements
407

<!-- Original PDF Page: 409 -->

SQLSTATE, custom exception name and exception message. The EXCEPTION statement, without
parameters, can also be used in this context to re-throw the error or exception.
Targeting GDSCODE
The argument for the WHEN GDSCODE clause is the symbolic name associated with the internally-
defined exception, such as grant_obj_notfound for GDS error 335544551.
In a statement or block of statements of the DO clause, a GDSCODE context variable, containing
the numeric code, becomes available. That numeric code is required if you want to compare a
GDSCODE exception with a targeted error. To compare it with a specific error, you need to use a
numeric values, for example 335544551 for grant_obj_notfound.
Similar context variables are available for SQLCODE and SQLSTATE.
The WHEN …  DO  statement or block is only executed when one of the events targeted by its
conditions occurs at run-time. If the WHEN … DO statement is executed, even if it does nothing,
execution will continue as if no error occurred: the error or user-defined exception neither
terminates nor rolls back the operations of the trigger or stored procedure.
However, if the WHEN … DO statement or block does nothing to handle or resolve the error, the DML
statement (SELECT, INSERT, UPDATE, DELETE, MERGE) that caused the error will be rolled back and none of
the statements below it in the same block of statements are executed.

1. If the error is not caused by one of the DML statements ( SELECT, INSERT, UPDATE,
DELETE, MERGE), the entire block of statements will be rolled back, not only the
one that caused an error. Any operations in the WHEN … DO statement will be
rolled back as well. The same limitation applies to the EXECUTE PROCEDURE 
statement. Read an interesting discussion of the phenomenon in Firebird
Tracker ticket firebird#4803.
2. In selectable stored procedures, output rows that were already passed to the
client in previous iterations of a FOR SELECT …  DO …  SUSPEND loop remain
available to the client if an exception is thrown subsequently in the process of
retrieving rows.
Scope of a WHEN … DO Statement
A WHEN … DO statement catches errors and exceptions in the current block of statements. It also
catches exceptions from nested blocks, if those exceptions have not been handled in those blocks.
All changes made before the statement that caused the error are visible to a WHEN … DO statement.
However, if you try to log them in an autonomous transaction, those changes are unavailable,
because the transaction where the changes took place is not committed at the point when the
autonomous transaction is started. Example 4, below, demonstrates this behaviour.
 When handling exceptions, it is sometimes desirable to handle the exception by
writing a log message to mark the fault and having execution continue past the
Chapter 7. Procedural SQL (PSQL) Statements
408

<!-- Original PDF Page: 410 -->

faulty record. Logs can be written to regular tables, but there is a problem with
that: the log records will “disappear” if an unhandled error causes the module to
stop executing, and a rollback is performed. Use of external tables can be useful
here, as data written to them is transaction-independent. The date inserted into a
linked external file will still be there, regardless of whether the overall process
succeeds or not.
Examples using WHEN…DO
1. Replacing the standard error with a custom one:
CREATE EXCEPTION COUNTRY_EXIST '';
SET TERM ^;
CREATE PROCEDURE ADD_COUNTRY (
  ACountryName COUNTRYNAME,
  ACurrency VARCHAR(10) )
AS
BEGIN
  INSERT INTO country (country, currency)
    VALUES (:ACountryName, :ACurrency);
  WHEN SQLCODE -803 DO
    EXCEPTION COUNTRY_EXIST 'Country already exists!';
END^
SET TERM ^;
2. Logging an error and re-throwing it in the WHEN block:
CREATE PROCEDURE ADD_COUNTRY (
  ACountryName COUNTRYNAME,
  ACurrency VARCHAR(10) )
AS
BEGIN
  INSERT INTO country (country,
                       currency)
  VALUES (:ACountryName,
          :ACurrency);
  WHEN ANY DO
  BEGIN
    -- write an error in log
    IN AUTONOMOUS TRANSACTION DO
      INSERT INTO ERROR_LOG (PSQL_MODULE,
                             GDS_CODE,
                             SQL_CODE,
                             SQL_STATE,
                             MESSAGE)
      VALUES ('ADD_COUNTRY',
              GDSCODE,
              SQLCODE,
Chapter 7. Procedural SQL (PSQL) Statements
409