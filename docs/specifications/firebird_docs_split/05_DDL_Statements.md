# 05 DDL Statements



<!-- Original PDF Page: 104 -->

regardless of the operator. This may appear to be contradictory, because every left-
side value will thus be considered both smaller and greater than, both equal to and
unequal to, every element of the right-side stream.
Nevertheless, it aligns perfectly with formal logic: if the set is empty, the predicate
is true for every row in the set.
ANY and SOME
Syntax
<value> <op> {ANY | SOME} (<select_stmt>)
The quantifiers ANY and SOME are identical in their behaviour. Both are specified in the SQL
standard, and they be used interchangeably to improve the readability of operators. When the ANY
or the SOME quantifier is used, the predicate is TRUE if any of the values returned by the subquery
satisfies the condition in the predicate of the main query. If the subquery returns no rows at all, the
predicate is automatically considered as FALSE.
Example
Show only those clients whose ratings are higher than those of one or more clients in Rome.
SELECT *
FROM Customers
WHERE rating > ANY
      (SELECT rating
       FROM Customers
       WHERE city = 'Rome')
Chapter 4. Common Language Elements
103

<!-- Original PDF Page: 105 -->

Chapter 5. Data Definition (DDL) Statements
DDL is the data definition language subset of Firebird’s SQL language. DDL statements are used to
create, alter and drop database objects. When a DDL statement is committed, the metadata for the
object are created, altered or deleted.
5.1. DATABASE
This section describes how to create a database, connect to an existing database, alter the file
structure of a database and how to drop a database. It also shows two methods to back up a
database and how to switch the database to the “copy-safe” mode for performing an external
backup safely.
5.1.1. CREATE DATABASE
Creates a new database
Available in
DSQL, ESQL
Syntax
CREATE {DATABASE | SCHEMA} <filespec>
  [<db_initial_option> [<db_initial_option> ...]]
  [<db_config_option> [<db_config_option> ...]]
<db_initial_option> ::=
    USER username
  | PASSWORD 'password'
  | ROLE rolename
  | PAGE_SIZE [=] size
  | LENGTH [=] num [PAGE[S]]
  | SET NAMES 'charset'
<db_config_option> ::=
    DEFAULT CHARACTER SET default_charset
      [COLLATION collation] -- not supported in ESQL
  | <sec_file>
  | DIFFERENCE FILE 'diff_file' -- not supported in ESQL
<filespec> ::= "'" [server_spec]{filepath | db_alias} "'"
<server_spec> ::=
    host[/{port | service}]:
  | <protocol>://[host[:{port | service}]/]
<protocol> ::= inet | inet4 | inet6 | xnet
<sec_file> ::=
Chapter 5. Data Definition (DDL) Statements
104

<!-- Original PDF Page: 106 -->

FILE 'filepath'
  [LENGTH [=] num [PAGE[S]]
  [STARTING [AT [PAGE]] pagenum]
 Each db_initial_option and db_config_option can occur at most once, except sec_file,
which can occur zero or more times.
Table 27. CREATE DATABASE Statement Parameters
Parameter Description
filespec File specification for primary database file
server_spec Remote server specification. Some protocols require specifying a
hostname. Optionally includes a port number or service name. Required
if the database is created on a remote server.
filepath Full path and file name including its extension. The file name must be
specified according to the rules of the platform file system being used.
db_alias Database alias previously created in the databases.conf file
host Host name or IP address of the server where the database is to be created
port The port number where the remote server is listening (parameter
RemoteServicePort in firebird.conf file)
service Service name. Must match the parameter value of RemoteServiceName in
firebird.conf file)
username Username of the owner of the new database. The maximum length is 63
characters. The username can optionally be enclosed in single or double
quotes. When a username is enclosed in double quotes, it is case-sensitive
following the rules for quoted identifiers. When enclosed in single quotes,
it behaves as if the value was specified without quotes. The user must be
an administrator or have the CREATE DATABASE privilege.
password Password of the user as the database owner. When using the Legacy_Auth
authentication plugin, only the first 8 characters are used. Case-sensitive
rolename The name of the role whose rights should be taken into account when
creating a database. The role name can be enclosed in single or double
quotes. When the role name is enclosed in double quotes, it is case-
sensitive following the rules for quoted identifiers. When enclosed in
single quotes, it behaves as if the value was specified without quotes.
size Page size for the database, in bytes. Possible values are 4096, 8192, 16384
and 32768. The default page size is 8192.
num Maximum size of the primary database file, or a secondary file, in pages
charset Specifies the character set of the connection available to a client
connecting after the database is successfully created. Single quotes are
required.
default_charset Specifies the default character set for string data types
Chapter 5. Data Definition (DDL) Statements
105

<!-- Original PDF Page: 107 -->

Parameter Description
collation Default collation for the default character set
sec_file File specification for a secondary file
pagenum Starting page number for a secondary database file
diff_file File path and name for DIFFERENCE files (.delta files) for backup mode
The CREATE DATABASE statement creates a new database. You can use CREATE DATABASE or CREATE
SCHEMA. They are synonymous, but we recommend to always use CREATE DATABASE as this may change
in a future version of Firebird.
A database consists of one or more files. The first (main) file is called the primary file, subsequent
files are called secondary file(s).

Multi-file Databases
Nowadays, multi-file databases are considered an anachronism. It made sense to
use multi-file databases on old file systems where the size of any file is limited. For
instance, you could not create a file larger than 4 GB on FAT32.
The primary file specification is the name of the database file and its extension with the full path to
it according to the rules of the OS platform file system being used. The database file must not exist
at the moment the database is being created. If it does exist, you will get an error message, and the
database will not be created.
If the full path to the database is not specified, the database will be created in one of the system
directories. The particular directory depends on the operating system. For this reason, unless you
have a strong reason to prefer that situation, always specify either the absolute path or an alias,
when creating a database.
Using a Database Alias
You can use aliases instead of the full path to the primary database file. Aliases are defined in the
databases.conf file in the following format:
alias = filepath

Executing a CREATE DATABASE statement requires special consideration in the client
application or database driver. As a result, it is not always possible to execute a
CREATE DATABASE statement. Some drivers provide other ways to create databases.
For example, Jaybird provides the class org.firebirdsql.management.FBManager to
programmatically create a database.
If necessary, you can always fall back to isql to create a database.
Creating a Database on a Remote Server
If you create a database on a remote server, you need to specify the remote server specification.
Chapter 5. Data Definition (DDL) Statements
106

<!-- Original PDF Page: 108 -->

The remote server specification depends on the protocol being used. If you use the TCP/IP protocol
to create a database, the primary file specification should look like this:
host[/{port|service}]:{filepath | db_alias}
Firebird also has a unified URL-like syntax for the remote server specification. In this syntax, the
first part specifies the name of the protocol, then a host name or IP address, port number, and path
of the primary database file, or an alias.
The following values can be specified as the protocol:
INET
TCP/IP (first tries to connect using the IPv6 protocol, if it fails, then IPv4)
INET4
TCP/IP v4
INET6
TCP/IP v6
XNET
local protocol (does not include a host, port and service name)
<protocol>://[host[:{port | service}]/]{filepath | db_alias}
Optional Parameters for CREATE DATABASE
USER and PASSWORD
The username and the password of an existing user in the security database ( security5.fdb or
whatever is configured in the SecurityDatabase configuration). You do not have to specify the
username and password if the ISC_USER and ISC_PASSWORD environment variables are set. The
user specified in the process of creating the database will be its owner. This will be important
when considering database and object privileges.
ROLE
The name of the role (usually RDB$ADMIN), which will be taken into account when creating the
database. The role must be assigned to the user in the applicable security database.
PAGE_SIZE
The desired database page size. This size will be set for the primary file and all secondary files of
the database. If you specify the database page size less than 4,096, it will be automatically
rounded up to 4,096. Other values not equal to either 4,096, 8,192, 16,384 or 32,768 will be
changed to the closest smaller supported value. If the database page size is not specified, the
default value of 8,192 is used.
 Bigger Isn’t Always Better.
Chapter 5. Data Definition (DDL) Statements
107

<!-- Original PDF Page: 109 -->

Larger page sizes can fit more records on a single page, have wider indexes,
and more indexes, but they will also waste more space for blobs (compare the
wasted space of a 3KB blob on page size 4096 with one on 32768: +/- 1KB vs +/-
29KB), and increase memory consumption of the page cache.
LENGTH
The maximum size of the primary or secondary database file, in pages. When a database is
created, its primary and secondary files will occupy the minimum number of pages necessary to
store the system data, regardless of the value specified in the LENGTH clause. The LENGTH value
does not affect the size of the only (or last, in a multi-file database) file. The file will keep
increasing its size automatically when necessary.
SET NAMES
The character set of the connection available after the database is successfully created. The
character set NONE is used by default. Notice that the character set should be enclosed in a pair of
apostrophes (single quotes).
DEFAULT CHARACTER SET
The default character set for creating data structures of string data types. Character sets are used
for CHAR, VARCHAR and BLOB SUB_TYPE TEXT data types. The character set NONE is used by default. It is
also possible to specify the default COLLATION for the default character set, making that collation
the default for the default character set. The default will be used for the entire database except
where an alternative character set, with or without a specified collation, is used explicitly for a
field, domain, variable, cast expression, etc.
STARTING AT
The database page number at which the next secondary database file should start. When the
previous file is fully filled with data according to the specified page number, the system will start
adding new data to the next database file.
DIFFERENCE FILE
The path and name for the file delta that stores any mutations to the database file after it has
been switched to the “copy-safe” mode by the ALTER DATABASE BEGIN BACKUP  statement. For the
detailed description of this clause, see ALTER DATABASE.
Specifying the Database Dialect
Databases are created in Dialect 3 by default. For the database to be created in Dialect 1, you will
need to execute the statement SET SQL DIALECT 1  from script or the client application, e.g. in isql,
before the CREATE DATABASE statement.
Who Can Create a Database
The CREATE DATABASE statement can be executed by:
• Administrators
• Users with the CREATE DATABASE privilege
Chapter 5. Data Definition (DDL) Statements
108

<!-- Original PDF Page: 110 -->

Examples Using CREATE DATABASE
1. Creating a database in Windows, located on disk D with a page size of 4,096. The owner of the
database will be the user wizard. The database will be in Dialect 1, and will use WIN1251 as its
default character set.
SET SQL DIALECT 1;
CREATE DATABASE 'D:\test.fdb'
USER 'wizard' PASSWORD 'player'
PAGE_SIZE = 4096 DEFAULT CHARACTER SET WIN1251;
2. Creating a database in the Linux operating system with a page size of 8,192 (default). The owner
of the database will be the user wizard. The database will be in Dialect 3 and will use UTF8 as its
default character set, with UNICODE_CI_AI as the default collation.
CREATE DATABASE '/home/firebird/test.fdb'
USER 'wizard' PASSWORD 'player'
DEFAULT CHARACTER SET UTF8 COLLATION UNICODE_CI_AI;
3. Creating a database on the remote server “baseserver” with the path specified in the alias “test”
that has been defined previously in the file databases.conf. The TCP/IP protocol is used. The
owner of the database will be the user wizard. The database will be in Dialect 3 and will use
UTF8 as its default character set.
CREATE DATABASE 'baseserver:test'
USER 'wizard' PASSWORD 'player'
DEFAULT CHARACTER SET UTF8;
4. Creating a database in Dialect 3 with UTF8 as its default character set. The primary file will
contain up to 10,000 pages with a page size of 8,192. As soon as the primary file has reached the
maximum number of pages, Firebird will start allocating pages to the secondary file test.fdb2.
If that file is filled up to its maximum as well, test.fdb3 becomes the recipient of all new page
allocations. As the last file, it has no page limit imposed on it by Firebird. New allocations will
continue for as long as the file system allows it or until the storage device runs out of free space.
If a LENGTH parameter were supplied for this last file, it would be ignored.
SET SQL DIALECT 3;
CREATE DATABASE 'baseserver:D:\test.fdb'
USER 'wizard' PASSWORD 'player'
PAGE_SIZE = 8192
DEFAULT CHARACTER SET UTF8
FILE 'D:\test.fdb2'
STARTING AT PAGE 10001
FILE 'D:\test.fdb3'
STARTING AT PAGE 20001;
Chapter 5. Data Definition (DDL) Statements
109

<!-- Original PDF Page: 111 -->

5. Creating a database in Dialect 3 with UTF8 as its default character set. The primary file will
contain up to 10,000 pages with a page size of 8,192. As far as file size and the use of secondary
files are concerned, this database will behave exactly like the one in the previous example.
SET SQL DIALECT 3;
CREATE DATABASE 'baseserver:D:\test.fdb'
USER 'wizard' PASSWORD 'player'
PAGE_SIZE = 8192
LENGTH 10000 PAGES
DEFAULT CHARACTER SET UTF8
FILE 'D:\test.fdb2'
FILE 'D:\test.fdb3'
STARTING AT PAGE 20001;
See also
ALTER DATABASE, DROP DATABASE
5.1.2. ALTER DATABASE
Alters the file organisation of a database, toggles its “copy-safe” state, manages encryption, and
other database-wide configuration
Available in
DSQL, ESQL — limited feature set
Syntax
ALTER {DATABASE | SCHEMA} <alter_db_option> [<alter_db_option> ...]
<alter_db_option> :==
    <add_sec_clause>
  | {ADD DIFFERENCE FILE 'diff_file' | DROP DIFFERENCE FILE}
  | {BEGIN | END} BACKUP
  | SET DEFAULT CHARACTER SET charset
  | {ENCRYPT WITH plugin_name [KEY key_name] | DECRYPT}
  | SET LINGER TO linger_duration
  | DROP LINGER
  | SET DEFAULT SQL SECURITY {INVOKER | DEFINER}
  | {ENABLE | DISABLE} PUBLICATION
  | INCLUDE <pub_table_filter> TO PUBLICATION
  | EXCLUDE <pub_table_filter> FROM PUBLICATION
<add_sec_clause> ::= ADD <sec_file> [<sec_file> ...]
<sec_file> ::=
  FILE 'filepath'
  [STARTING [AT [PAGE]] pagenum]
  [LENGTH [=] num [PAGE[S]]
Chapter 5. Data Definition (DDL) Statements
110

<!-- Original PDF Page: 112 -->

<pub_table_filter> ::=
    ALL
  | TABLE table_name [, table_name ...]

Multiple files can be added in one ADD clause:
ALTER DATABASE
  ADD FILE x LENGTH 8000
    FILE y LENGTH 8000
    FILE z
Multiple occurrences of add_sec_clause (ADD FILE clauses) are allowed; an ADD FILE
clause that adds multiple files (as in the example above) can be mixed with others
that add only one file.
Table 28. ALTER DATABASE Statement Parameters
Parameter Description
add_sec_clause Adding a secondary database file
sec_file File specification for secondary file
filepath Full path and file name of the delta file or secondary database file
pagenum Page number from which the secondary database file is to start
num Maximum size of the secondary file in pages
diff_file File path and name of the .delta file (difference file)
charset New default character set of the database
linger_duration Duration of linger delay in seconds; must be greater than or equal to 0
(zero)
plugin_name The name of the encryption plugin
key_name The name of the encryption key
pub_table_filter Filter of tables to include to or exclude from publication
table_name Name (identifier) of a table
The ALTER DATABASE statement can:
• add secondary files to a database
• switch a single-file database into and out of the “copy-safe” mode (DSQL only)
• set or unset the path and name of the delta file for physical backups (DSQL only)
 SCHEMA is currently a synonym for DATABASE; this may change in a future version, so
we recommend to always use DATABASE
Chapter 5. Data Definition (DDL) Statements
111

<!-- Original PDF Page: 113 -->

Who Can Alter the Database
The ALTER DATABASE statement can be executed by:
• Administrators
• Users with the ALTER DATABASE privilege
Parameters for ALTER DATABASE
ADD (FILE)
Adds secondary files to the database. It is necessary to specify the full path to the file and the
name of the secondary file. The description for the secondary file is similar to the one given for
the CREATE DATABASE statement.
ADD DIFFERENCE FILE
Specifies the path and name of the difference file (or, delta file) that stores any mutations to the
database whenever it is switched to the “copy-safe” mode. This clause does not add a file, but it
configures name and path of the delta file when the database is in “copy-safe” mode. To change
the existing setting, you should delete the previously specified description of the delta file using
the DROP DIFFERENCE FILE clause before specifying the new description of the delta file. If the path
and name of the delta file are not configured, the file will have the same path and name as the
database, but with the .delta file extension.

If only a filename is specified, the delta file will be created in the current
directory of the server. On Windows, this will be the system directory — a very
unwise location to store volatile user files and contrary to Windows file system
rules.
DROP DIFFERENCE FILE
Deletes the description (path and name) of the difference file specified previously in the ADD
DIFFERENCE FILE clause. This does not delete a file, but DROP DIFFERENCE FILE clears (resets) the
path and name of the delta file from the database header. Next time the database is switched to
the “copy-safe” mode, the default values will be used (i.e. the same path and name as those of the
database, but with the .delta extension).
BEGIN BACKUP
Switches the database to the “copy-safe” mode. ALTER DATABASE with this clause freezes the main
database file, making it possible to back it up safely using file system tools, even if users are
connected and performing operations with data. Until the backup state of the database is
reverted to NORMAL, all changes made to the database will be written to the delta (difference)
file.

Despite its name, the ALTER DATABASE BEGIN BACKUP  statement does not start a
backup process, but only freezes the database, to create the conditions for doing
a task that requires the database file to be read-only temporarily.
END BACKUP
Switches the database from the “copy-safe” mode to the normal mode. A statement with this
Chapter 5. Data Definition (DDL) Statements
112

<!-- Original PDF Page: 114 -->

clause merges the difference file with the main database file and restores the normal operation
of the database. Once the END BACKUP process starts, the conditions no longer exist for creating
safe backups by means of file system tools.

Use of BEGIN BACKUP  and END BACKUP  and copying the database files with
filesystem tools, is not safe with multi-file databases! Use this method only on
single-file databases.
Making a safe backup with the gbak utility remains possible at all times,
although it is not recommended running gbak while the database is in LOCKED
or MERGE state.
SET DEFAULT CHARACTER SET
Changes the default character set of the database. This change does not affect existing data or
columns. The new default character set will only be used in subsequent DDL commands. To
modify the default collation, use ALTER CHARACTER SET  on the default character set of the
database.
ENCRYPT WITH
See Encrypting a Database in the Security chapter.
DECRYPT
See Decrypting a Database in the Security chapter.
SET LINGER TO
Sets the linger-delay. The linger-delay applies only to Firebird SuperServer, and is the number of
seconds the server keeps a database file (and its caches) open after the last connection to that
database was closed. This can help to improve performance at low cost, when the database is
opened and closed frequently, by keeping resources “warm” for the next connection.
 This mode can be useful for web applications — without a connection
pool — where connections to the database usually “live” for a very short time.

The SET LINGER TO  and DROP LINGER  clauses can be combined in a single
statement, but the last clause “wins”. For example, ALTER DATABASE SET LINGER
TO 5 DROP LINGER will set the linger-delay to 0 (no linger), while ALTER DATABASE
DROP LINGER SET LINGER to 5 will set the linger-delay to 5 seconds.
DROP LINGER
Drops the linger-delay (sets it to zero). Using DROP LINGER is equivalent to using SET LINGER TO 0.

Dropping LINGER is not an ideal solution for the occasional need to turn it off for
once-only operations where the server needs a forced shutdown. The gfix utility
now has the -NoLinger switch, which will close the specified database
immediately after the last attachment is gone, regardless of the LINGER setting in
the database. The LINGER setting is retained and works normally the next time.
Chapter 5. Data Definition (DDL) Statements
113

<!-- Original PDF Page: 115 -->

The same one-off override is also available through the Services API, using the
tag isc_spb_prp_nolinger, e.g. (in one line):
fbsvcmgr host:service_mgr user sysdba password xxx
       action_properties dbname employee prp_nolinger
 The DROP LINGER  and SET LINGER TO  clauses can be combined in a single
statement, but the last clause “wins”.
SET DEFAULT SQL SECURITY
Specifies the default SQL SECURITY option to apply at runtime for objects without the SQL Security
property set. See also SQL Security in chapter Security.
ENABLE PUBLICATION
Enables publication of this database for replication. Replication begins (or continues) with the
next transaction started after this transaction commits.
DISABLE PUBLICATION
Enables publication of this database for replication. Replication is disabled immediately after
commit.
EXCLUDE … FROM PUBLICATION
Excludes tables from publication. If the INCLUDE ALL TO PUBLICATION  clause is used, all tables
created afterward will also be replicated, unless overridden explicitly in the CREATE TABLE 
statement.
INCLUDE … TO PUBLICATION
Includes tables to publication. If the INCLUDE ALL TO PUBLICATION clause is used, all tables created
afterward will also be replicated, unless overridden explicitly in the CREATE TABLE statement.

Replication
• Other than the syntax, configuring Firebird for replication is not covered in
this language reference.
• All replication management commands are DDL statements and thus
effectively executed at the transaction commit time.
Examples of ALTER DATABASE Usage
1. Adding a secondary file to the database. As soon as 30000 pages are filled in the previous
primary or secondary file, the Firebird engine will start adding data to the secondary file
test4.fdb.
ALTER DATABASE
  ADD FILE 'D:\test4.fdb'
    STARTING AT PAGE 30001;
Chapter 5. Data Definition (DDL) Statements
114

<!-- Original PDF Page: 116 -->

2. Specifying the path and name of the delta file:
ALTER DATABASE
  ADD DIFFERENCE FILE 'D:\test.diff';
3. Deleting the description of the delta file:
ALTER DATABASE
  DROP DIFFERENCE FILE;
4. Switching the database to the “copy-safe” mode:
ALTER DATABASE
  BEGIN BACKUP;
5. Switching the database back from the “copy-safe” mode to the normal operation mode:
ALTER DATABASE
  END BACKUP;
6. Changing the default character set for a database to WIN1251
ALTER DATABASE
  SET DEFAULT CHARACTER SET WIN1252;
7. Setting a linger-delay of 30 seconds
ALTER DATABASE
  SET LINGER TO 30;
8. Encrypting the database with a plugin called DbCrypt
ALTER DATABASE
  ENCRYPT WITH DbCrypt;
9. Decrypting the database
ALTER DATABASE
  DECRYPT;
See also
Chapter 5. Data Definition (DDL) Statements
115

<!-- Original PDF Page: 117 -->

CREATE DATABASE, DROP DATABASE
5.1.3. DROP DATABASE
Drops (deletes) the database of the current connection
Available in
DSQL, ESQL
Syntax
DROP DATABASE
The DROP DATABASE statement deletes the current database. Before deleting a database, you have to
connect to it. The statement deletes the primary file, all secondary files and all shadow files.
 Contrary to CREATE DATABASE and ALTER DATABASE, DROP SCHEMA is not a valid alias for
DROP DATABASE. This is intentional.
Who Can Drop a Database
The DROP DATABASE statement can be executed by:
• Administrators
• Users with the DROP DATABASE privilege
Example of DROP DATABASE
Deleting the current database
DROP DATABASE;
See also
CREATE DATABASE, ALTER DATABASE
5.2. SHADOW
A shadow is an exact, page-by-page copy of a database. Once a shadow is created, all changes made
in the database are immediately reflected in the shadow. If the primary database file becomes
unavailable for some reason, the DBMS will switch to the shadow.
This section describes how to create and delete shadow files.
5.2.1. CREATE SHADOW
Creates a shadow file for the current database
Available in
Chapter 5. Data Definition (DDL) Statements
116

<!-- Original PDF Page: 118 -->

DSQL, ESQL
Syntax
CREATE SHADOW <sh_num> [{AUTO | MANUAL}] [CONDITIONAL]
  'filepath' [LENGTH [=] num [PAGE[S]]]
  [<secondary_file> ...]
<secondary_file> ::=
  FILE 'filepath'
  [STARTING [AT [PAGE]] pagenum]
  [LENGTH [=] num [PAGE[S]]]
Table 29. CREATE SHADOW Statement Parameters
Parameter Description
sh_num Shadow number — a positive number identifying the shadow set
filepath The name of the shadow file and the path to it, in accord with the rules of
the operating system
num Maximum shadow size, in pages
secondary_file Secondary file specification
page_num The number of the page at which the secondary shadow file should start
The CREATE SHADOW statement creates a new shadow. The shadow starts duplicating the database
right at the moment it is created. It is not possible for a user to connect to a shadow.
Like a database, a shadow may be multi-file. The number and size of a shadow’s files are not
related to the number and size of the files of the shadowed database.
The page size for shadow files is set to be equal to the database page size and cannot be changed.
If a calamity occurs involving the original database, the system converts the shadow to a copy of
the database and switches to it. The shadow is then unavailable. What happens next depends on the
MODE option.
AUTO | MANUAL Modes
When a shadow is converted to a database, it becomes unavailable. A shadow might alternatively
become unavailable because someone accidentally deletes its file, or the disk space where the
shadow files are stored is exhausted or is itself damaged.
• If the AUTO mode is selected (the default value), shadowing ceases automatically, all references
to it are deleted from the database header, and the database continues functioning normally.
If the CONDITIONAL option was set, the system will attempt to create a new shadow to replace the
lost one. It does not always succeed, however, and a new one may need to be created manually.
• If the MANUAL mode attribute is set when the shadow becomes unavailable, all attempts to
connect to the database and to query it will produce error messages. The database will remain
Chapter 5. Data Definition (DDL) Statements
117

<!-- Original PDF Page: 119 -->

inaccessible until either the shadow again becomes available, or the database administrator
deletes it using the DROP SHADOW statement. MANUAL should be selected if continuous shadowing is
more important than uninterrupted operation of the database.
Options for CREATE SHADOW
LENGTH
Specifies the maximum size of the primary or secondary shadow file in pages. The LENGTH value
does not affect the size of the only shadow file, nor the last if it is a set. The last (or only) file will
keep automatically growing as long as it is necessary.
STARTING AT
Specifies the shadow page number at which the next shadow file should start. The system will
start adding new data to the next shadow file when the previous file is filled with data up to the
specified page number.
 You can verify the sizes, names and location of the shadow files by connecting to
the database using isql and running the command SHOW DATABASE;
Who Can Create a Shadow
The CREATE SHADOW statement can be executed by:
• Administrators
• Users with the ALTER DATABASE privilege
Examples Using CREATE SHADOW
1. Creating a shadow for the current database as “shadow number 1”:
CREATE SHADOW 1 'g:\data\test.shd';
2. Creating a multi-file shadow for the current database as “shadow number 2”:
CREATE SHADOW 2 'g:\data\test.sh1'
  LENGTH 8000 PAGES
  FILE 'g:\data\test.sh2';
See also
CREATE DATABASE, DROP SHADOW
5.2.2. DROP SHADOW
Drops (deletes) a shadow file from the current database
Available in
Chapter 5. Data Definition (DDL) Statements
118

<!-- Original PDF Page: 120 -->

DSQL, ESQL
Syntax
DROP SHADOW sh_num
  [{DELETE | PRESERVE} FILE]
Table 30. DROP SHADOW Statement Parameter
Parameter Description
sh_num Shadow number — a positive number identifying the shadow set
The DROP SHADOW statement deletes the specified shadow for the current database. When a shadow is
dropped, all files related to it are deleted and shadowing to the specified sh_num ceases. The
optional DELETE FILE clause makes this behaviour explicit. On the contrary, the PRESERVE FILE clause
will remove the shadow from the database, but the file itself will not be deleted.
Who Can Drop a Shadow
The DROP SHADOW statement can be executed by:
• Administrators
• Users with the ALTER DATABASE privilege
Example of DROP SHADOW
Deleting “shadow number 1”.
DROP SHADOW 1;
See also
CREATE SHADOW
5.3. DOMAIN
DOMAIN is one of the object types in a relational database. A domain is created as a specific data type
with attributes attached to it (think of attributes like length, precision or scale, nullability, check
constraints). Once a domain has been defined in the database, it can be reused repeatedly to define
table columns, PSQL arguments and PSQL local variables. Those objects inherit all attributes of the
domain. Some attributes can be overridden when the new object is defined, if required.
This section describes the syntax of statements used to create, alter and drop domains. A detailed
description of domains and their usage can be found in Custom Data Types — Domains.
5.3.1. CREATE DOMAIN
Creates a new domain
Chapter 5. Data Definition (DDL) Statements
119

<!-- Original PDF Page: 121 -->

Available in
DSQL, ESQL
Syntax
CREATE DOMAIN name [AS] <datatype>
  [DEFAULT {<literal> | NULL | <context_var>}]
  [NOT NULL] [CHECK (<dom_condition>)]
  [COLLATE collation_name]
<datatype> ::=
  <scalar_datatype> | <blob_datatype> | <array_datatype>
<scalar_datatype> ::=
  !! See Scalar Data Types Syntax !!
<blob_datatype> ::=
  !! See BLOB Data Types Syntax !!
<array_datatype> ::=
  !! See Array Data Types Syntax !!
<dom_condition> ::=
    <val> <operator> <val>
  | <val> [NOT] BETWEEN <val> AND <val>
  | <val> [NOT] IN ({<val> [, <val> ...] | <select_list>})
  | <val> IS [NOT] NULL
  | <val> IS [NOT] DISTINCT FROM <val>
  | <val> [NOT] CONTAINING <val>
  | <val> [NOT] STARTING [WITH] <val>
  | <val> [NOT] LIKE <val> [ESCAPE <val>]
  | <val> [NOT] SIMILAR TO <val> [ESCAPE <val>]
  | <val> <operator> {ALL | SOME | ANY} (<select_list>)
  | [NOT] EXISTS (<select_expr>)
  | [NOT] SINGULAR (<select_expr>)
  | (<dom_condition>)
  | NOT <dom_condition>
  | <dom_condition> OR <dom_condition>
  | <dom_condition> AND <dom_condition>
<operator> ::=
    <> | != | ^= | ~= | = | < | > | <= | >=
  | !< | ^< | ~< | !> | ^> | ~>
<val> ::=
    VALUE
  | <literal>
  | <context_var>
  | <expression>
  | NULL
  | NEXT VALUE FOR genname
Chapter 5. Data Definition (DDL) Statements
120

<!-- Original PDF Page: 122 -->

| GEN_ID(genname, <val>)
  | CAST(<val> AS <cast_type>)
  | (<select_one>)
  | func([<val> [, <val> ...]])
<cast_type> ::= <domain_or_non_array_type> | <array_datatype>
<domain_or_non_array_type> ::=
  !! See Scalar Data Types Syntax !!
Table 31. CREATE DOMAIN Statement Parameters
Parameter Description
name Domain name. The maximum length is 63 characters
datatype SQL data type
literal A literal value that is compatible with datatype
context_var Any context variable whose type is compatible with datatype
dom_condition Domain condition
collation_name Name of a collation that is valid for charset_name, if it is supplied with
datatype or, otherwise, is valid for the default character set of the
database
select_one A scalar SELECT statement — selecting one column and returning only one
row
select_list A SELECT statement selecting one column and returning zero or more
rows
select_expr A SELECT statement selecting one or more columns and returning zero or
more rows
expression An expression resolving to a value that is compatible with datatype
genname Sequence (generator) name
func Internal function or UDF
The CREATE DOMAIN statement creates a new domain.
Any SQL data type can be specified as the domain type.
Type-specific Details
Array Types
• If the domain is to be an array, the base type can be any SQL data type except BLOB and array.
• The dimensions of the array are specified between square brackets.
• For each array dimension, one or two integer numbers define the lower and upper
boundaries of its index range:
◦ By default, arrays are 1-based. The lower boundary is implicit and only the upper
Chapter 5. Data Definition (DDL) Statements
121

<!-- Original PDF Page: 123 -->

boundary need be specified. A single number smaller than 1 defines the range num..1
and a number greater than 1 defines the range 1..num.
◦ Two numbers separated by a colon (‘ :’) and optional whitespace, the second greater than
the first, can be used to define the range explicitly. One or both boundaries can be less
than zero, as long as the upper boundary is greater than the lower.
• When the array has multiple dimensions, the range definitions for each dimension must be
separated by commas and optional whitespace.
• Subscripts are validated only if an array actually exists. It means that no error messages
regarding invalid subscripts will be returned if selecting a specific element returns nothing
or if an array field is NULL.
String Types
You can use the CHARACTER SET clause to specify the character set for the CHAR, VARCHAR and BLOB
(SUB_TYPE TEXT) types. If the character set is not specified, the character set specified as DEFAULT
CHARACTER SET of the database will be used. If the database has no default character set, the
character set NONE is applied by default when you create a character domain.

With character set NONE, character data are stored and retrieved the way they
were submitted. Data in any encoding can be added to a column based on such
a domain, but it is impossible to add this data to a column with a different
encoding. Because no transliteration is performed between the source and
destination encodings, errors may result.
DEFAULT Clause
The optional DEFAULT clause allows you to specify a default value for the domain. This value will
be added to the table column that inherits this domain when the INSERT statement is executed, if
no value is specified for it in the DML statement. Local variables and arguments in PSQL
modules that reference this domain will be initialized with the default value. For the default
value, use a literal of a compatible type or a context variable of a compatible type.
NOT NULL Constraint
Columns and variables based on a domain with the NOT NULL constraint will be prevented from
being written as NULL, i.e. a value is required.

When creating a domain, take care to avoid specifying limitations that would
contradict one another. For instance, NOT NULL  and DEFAULT NULL  are
contradictory.
CHECK Constraint(s)
The optional CHECK clause specifies constraints for the domain. A domain constraint specifies
conditions that must be satisfied by the values of table columns or variables that inherit from
the domain. A condition must be enclosed in parentheses. A condition is a logical expression
(also called a predicate) that can return the Boolean results TRUE, FALSE and UNKNOWN. A condition
is considered satisfied if the predicate returns the value TRUE or “unknown value” (equivalent to
NULL). If the predicate returns FALSE, the condition for acceptance is not met.
Chapter 5. Data Definition (DDL) Statements
122

<!-- Original PDF Page: 124 -->

VALUE Keyword
The keyword VALUE in a domain constraint substitutes for the table column that is based on this
domain or for a variable in a PSQL module. It contains the value assigned to the variable or the
table column. VALUE can be used anywhere in the CHECK constraint, though it is usually used in the
left part of the condition.
COLLATE
The optional COLLATE clause allows you to specify the collation if the domain is based on one of
the string data types, including BLOBs with text subtypes. If no collation is specified, the collation
will be the one that is default for the specified character set at the time the domain is created.
Who Can Create a Domain
The CREATE DOMAIN statement can be executed by:
• Administrators
• Users with the CREATE DOMAIN privilege
CREATE DOMAIN Examples
1. Creating a domain that can take values greater than 1,000, with a default value of 10,000.
CREATE DOMAIN CUSTNO AS
  INTEGER DEFAULT 10000
  CHECK (VALUE > 1000);
2. Creating a domain that can take the values 'Yes' and 'No' in the default character set specified
during the creation of the database.
CREATE DOMAIN D_BOOLEAN AS
  CHAR(3) CHECK (VALUE IN ('Yes', 'No'));
3. Creating a domain with the UTF8 character set and the UNICODE_CI_AI collation.
CREATE DOMAIN FIRSTNAME AS
  VARCHAR(30) CHARACTER SET UTF8
  COLLATE UNICODE_CI_AI;
4. Creating a domain of the DATE type that will not accept NULL and uses the current date as the
default value.
CREATE DOMAIN D_DATE AS
  DATE DEFAULT CURRENT_DATE
  NOT NULL;
Chapter 5. Data Definition (DDL) Statements
123

<!-- Original PDF Page: 125 -->

5. Creating a domain defined as an array of 2 elements of the NUMERIC(18, 3) type. The starting
array index is 1.
CREATE DOMAIN D_POINT AS
  NUMERIC(18, 3) [2];
 Domains defined over an array type may be used only to define table columns.
You cannot use array domains to define local variables in PSQL modules.
6. Creating a domain whose elements can be only country codes defined in the COUNTRY table.
CREATE DOMAIN D_COUNTRYCODE AS CHAR(3)
  CHECK (EXISTS(SELECT * FROM COUNTRY
         WHERE COUNTRYCODE = VALUE));

The example is given only to show the possibility of using predicates with
queries in the domain test condition. It is not recommended to create this style
of domain in practice unless the lookup table contains data that are never
deleted.
See also
ALTER DOMAIN, DROP DOMAIN
5.3.2. ALTER DOMAIN
Alters the attributes of a domain or renames a domain
Available in
DSQL, ESQL
Syntax
ALTER DOMAIN domain_name
  [TO new_name]
  [TYPE <datatype>]
  [{SET DEFAULT {<literal> | NULL | <context_var>} | DROP DEFAULT}]
  [{SET | DROP} NOT NULL]
  [{ADD [CONSTRAINT] CHECK (<dom_condition>) | DROP CONSTRAINT}]
<datatype> ::=
   <scalar_datatype> | <blob_datatype>
<scalar_datatype> ::=
  !! See Scalar Data Types Syntax !!
<blob_datatype> ::=
  !! See BLOB Data Types Syntax !!
Chapter 5. Data Definition (DDL) Statements
124

<!-- Original PDF Page: 126 -->

!! See also CREATE DOMAIN Syntax !!
Table 32. ALTER DOMAIN Statement Parameters
Parameter Description
new_name New name for domain. The maximum length is 63 characters
literal A literal value that is compatible with datatype
context_var Any context variable whose type is compatible with datatype
The ALTER DOMAIN statement enables changes to the current attributes of a domain, including its
name. You can make any number of domain alterations in one ALTER DOMAIN statement.
ALTER DOMAIN clauses
TO name
Renames the domain, as long as there are no dependencies on the domain, i.e. table columns,
local variables or procedure arguments referencing it.
SET DEFAULT
Sets a new default value for the domain, replacing any existing default.
DROP DEFAULT
Deletes a previously specified default value and replace it with NULL.
SET NOT NULL
Adds a NOT NULL  constraint to the domain; columns or parameters of this domain will be
prevented from being written as NULL, i.e. a value is required.

Adding a NOT NULL constraint to an existing domain will subject all columns
using this domain to a full data validation, so ensure that the columns have no
nulls before attempting the change.
DROP NOT NULL
Drops the NOT NULL constraint from the domain.

An explicit NOT NULL constraint on a column that depends on a domain prevails
over the domain. In this situation, the modification of the domain to make it
nullable does not propagate to the column.
ADD CONSTRAINT CHECK
Adds a CHECK constraint to the domain. If the domain already has a CHECK constraint, it has to be
deleted first, using an ALTER DOMAIN statement that includes a DROP CONSTRAINT clause.
TYPE
Changes the data type of the domain to a different, compatible one. The system will forbid any
change to the type that could result in data loss. An example would be if the number of
Chapter 5. Data Definition (DDL) Statements
125

<!-- Original PDF Page: 127 -->

characters in the new type were smaller than in the existing type.

When you alter the attributes of a domain, existing PSQL code may become
invalid. For information on how to detect it, read the piece entitled The
RDB$VALID_BLR Field in Appendix A.
What ALTER DOMAIN Cannot Alter
• If the domain was declared as an array, it is not possible to change its type or its dimensions;
nor can any other type be changed to an array type.
• The collation cannot be changed without dropping the domain and recreating it with the
desired attributes.
Who Can Alter a Domain
The ALTER DOMAIN statement can be executed by:
• Administrators
• The owner of the domain
• Users with the ALTER ANY DOMAIN privilege
Domain alterations can be prevented by dependencies from objects to which the user does not have
sufficient privileges.
ALTER DOMAIN Examples
1. Changing the data type to INTEGER and setting or changing the default value to 2,000:
ALTER DOMAIN CUSTNO
  TYPE INTEGER
  SET DEFAULT 2000;
2. Renaming a domain.
ALTER DOMAIN D_BOOLEAN TO D_BOOL;
3. Deleting the default value and adding a constraint for the domain:
ALTER DOMAIN D_DATE
  DROP DEFAULT
  ADD CONSTRAINT CHECK (VALUE >= date '01.01.2000');
4. Changing the CHECK constraint:
ALTER DOMAIN D_DATE
Chapter 5. Data Definition (DDL) Statements
126

<!-- Original PDF Page: 128 -->

DROP CONSTRAINT;
ALTER DOMAIN D_DATE
  ADD CONSTRAINT CHECK
    (VALUE BETWEEN date '01.01.1900' AND date '31.12.2100');
5. Changing the data type to increase the permitted number of characters:
ALTER DOMAIN FIRSTNAME
  TYPE VARCHAR(50) CHARACTER SET UTF8;
6. Adding a NOT NULL constraint:
ALTER DOMAIN FIRSTNAME
  SET NOT NULL;
7. Removing a NOT NULL constraint:
ALTER DOMAIN FIRSTNAME
  DROP NOT NULL;
See also
CREATE DOMAIN, DROP DOMAIN
5.3.3. DROP DOMAIN
Drops an existing domain
Available in
DSQL, ESQL
Syntax
DROP DOMAIN domain_name
The DROP DOMAIN statement deletes a domain that exists in the database. It is not possible to delete a
domain if it is referenced by any database table columns or used in any PSQL module. To delete a
domain that is in use, all columns in all tables that refer to the domain have to be dropped and all
references to the domain have to be removed from PSQL modules.
Who Can Drop a Domain
The DROP DOMAIN statement can be executed by:
• Administrators
Chapter 5. Data Definition (DDL) Statements
127

<!-- Original PDF Page: 129 -->

• The owner of the domain
• Users with the DROP ANY DOMAIN privilege
Example of DROP DOMAIN
Deleting the COUNTRYNAME domain
DROP DOMAIN COUNTRYNAME;
See also
CREATE DOMAIN, ALTER DOMAIN
5.4. TABLE
As a relational DBMS, Firebird stores data in tables. A table is a flat, two-dimensional structure
containing any number of rows. Table rows are often called records.
All rows in a table have the same structure and consist of columns. Table columns are often called
fields. A table must have at least one column. Each column contains a single type of SQL data.
This section describes how to create, alter and drop tables in a database.
5.4.1. CREATE TABLE
Creates a table
Available in
DSQL, ESQL
Syntax
CREATE [GLOBAL TEMPORARY] TABLE tablename
  [EXTERNAL [FILE] 'filespec']
  (<col_def> [, {<col_def> | <tconstraint>} ...])
  [{<table_attrs> | <gtt_table_attrs>}]
<col_def> ::=
    <regular_col_def>
  | <computed_col_def>
  | <identity_col_def>
<regular_col_def> ::=
  colname {<datatype> | domainname}
  [DEFAULT {<literal> | NULL | <context_var>}]
  [<col_constraint> ...]
  [COLLATE collation_name]
<computed_col_def> ::=
  colname [{<datatype> | domainname}]
Chapter 5. Data Definition (DDL) Statements
128

<!-- Original PDF Page: 130 -->

{COMPUTED [BY] | GENERATED ALWAYS AS} (<expression>)
<identity_col_def> ::=
  colname {<datatype> | domainname}
  GENERATED {ALWAYS | BY DEFAULT} AS IDENTITY
  [(<identity_col_option>...)]
  [<col_constraint> ...]
<identity_col_option> ::=
    START WITH start_value
  | INCREMENT [BY] inc_value
<datatype> ::=
    <scalar_datatype> | <blob_datatype> | <array_datatype>
<scalar_datatype> ::=
  !! See Scalar Data Types Syntax !!
<blob_datatype> ::=
  !! See BLOB Data Types Syntax !!
<array_datatype> ::=
  !! See Array Data Types Syntax !!
<col_constraint> ::=
  [CONSTRAINT constr_name]
    { PRIMARY KEY [<using_index>]
    | UNIQUE      [<using_index>]
    | REFERENCES other_table [(colname)] [<using_index>]
        [ON DELETE {NO ACTION | CASCADE | SET DEFAULT | SET NULL}]
        [ON UPDATE {NO ACTION | CASCADE | SET DEFAULT | SET NULL}]
    | CHECK (<check_condition>)
    | NOT NULL }
<tconstraint> ::=
  [CONSTRAINT constr_name]
    { PRIMARY KEY (<col_list>) [<using_index>]
    | UNIQUE      (<col_list>) [<using_index>]
    | FOREIGN KEY (<col_list>)
        REFERENCES other_table [(<col_list>)] [<using_index>]
        [ON DELETE {NO ACTION | CASCADE | SET DEFAULT | SET NULL}]
        [ON UPDATE {NO ACTION | CASCADE | SET DEFAULT | SET NULL}]
    | CHECK (<check_condition>) }
<col_list> ::= colname [, colname ...]
<using_index> ::= USING
  [ASC[ENDING] | DESC[ENDING]] INDEX indexname
<check_condition> ::=
    <val> <operator> <val>
Chapter 5. Data Definition (DDL) Statements
129

<!-- Original PDF Page: 131 -->

| <val> [NOT] BETWEEN <val> AND <val>
  | <val> [NOT] IN (<val> [, <val> ...] | <select_list>)
  | <val> IS [NOT] NULL
  | <val> IS [NOT] DISTINCT FROM <val>
  | <val> [NOT] CONTAINING <val>
  | <val> [NOT] STARTING [WITH] <val>
  | <val> [NOT] LIKE <val> [ESCAPE <val>]
  | <val> [NOT] SIMILAR TO <val> [ESCAPE <val>]
  | <val> <operator> {ALL | SOME | ANY} (<select_list>)
  | [NOT] EXISTS (<select_expr>)
  | [NOT] SINGULAR (<select_expr>)
  | (<check_condition>)
  | NOT <check_condition>
  | <check_condition> OR <check_condition>
  | <check_condition> AND <check_condition>
<operator> ::=
    <> | != | ^= | ~= | = | < | > | <= | >=
  | !< | ^< | ~< | !> | ^> | ~>
<val> ::=
    colname ['['array_idx [, array_idx ...]']']
  | <literal>
  | <context_var>
  | <expression>
  | NULL
  | NEXT VALUE FOR genname
  | GEN_ID(genname, <val>)
  | CAST(<val> AS <cast_type>)
  | (<select_one>)
  | func([<val> [, <val> ...]])
<cast_type> ::= <domain_or_non_array_type> | <array_datatype>
<domain_or_non_array_type> ::=
  !! See Scalar Data Types Syntax !!
<table_attrs> ::= <table_attr> [<table_attr> ...]
<table_attr> ::=
    <sql_security>
  | {ENABLE | DISABLE} PUBLICATION
<sql_security> ::= SQL SECURITY {INVOKER | DEFINER}
<gtt_table_attrs> ::= <gtt_table_attr> [gtt_table_attr> ...]
<gtt_table_attr> ::=
    <sql_security>
  | ON COMMIT {DELETE | PRESERVE} ROWS
Chapter 5. Data Definition (DDL) Statements
130

<!-- Original PDF Page: 132 -->

Table 33. CREATE TABLE Statement Parameters
Parameter Description
tablename Name (identifier) for the table. The maximum length is 63 characters and
must be unique in the database.
filespec File specification (only for external tables). Full file name and path,
enclosed in single quotes, correct for the local file system and located on
a storage device that is physically connected to Firebird’s host computer.
colname Name (identifier) for a column in the table. The maximum length is 63
characters and must be unique in the table.
tconstraint Table constraint
table_attrs Attributes of a normal table
gtt_table_attrs Attributes of a global temporary table
datatype SQL data type
domain_name Domain name
start_value The initial value of the identity column
inc_value The increment (or step) value of the identity column, default is 1; zero (0)
is not allowed.
col_constraint Column constraint
constr_name The name (identifier) of a constraint. The maximum length is 63
characters.
other_table The name of the table referenced by the foreign key constraint
other_col The name of the column in other_table that is referenced by the foreign
key
literal A literal value that is allowed in the given context
context_var Any context variable whose data type is allowed in the given context
check_condition The condition applied to a CHECK constraint, that will resolve as either
true, false or NULL
collation Collation
select_one A scalar SELECT statement — selecting one column and returning only one
row
select_list A SELECT statement selecting one column and returning zero or more
rows
select_expr A SELECT statement selecting one or more columns and returning zero or
more rows
expression An expression resolving to a value that is allowed in the given context
genname Sequence (generator) name
func Internal function or UDF
Chapter 5. Data Definition (DDL) Statements
131

<!-- Original PDF Page: 133 -->

The CREATE TABLE statement creates a new table. Its name must be unique among the names of all
tables, views, and stored procedures in the database.
A table must contain at least one column that is not computed, and the names of columns must be
unique in the table.
A column must have either an explicit SQL data type, the name of a domain whose attributes will be
copied for the column, or be defined as COMPUTED BY an expression (a calculated field).
A table may have any number of table constraints, including none.
Character Columns
You can use the CHARACTER SET clause to specify the character set for the CHAR, VARCHAR and BLOB (text
subtype) types. If the character set is not specified, the default character set of the database — at
time of the creation of the column — will be used.
If the database has no default character set, the NONE character set is applied. Data in any encoding
can be added to such a column, but it is not possible to add this data to a column with a different
encoding. No transliteration is performed between the source and destination encodings, which
may result in errors.
The optional COLLATE clause allows you to specify the collation for character data types, including
BLOB SUB_TYPE TEXT . If no collation is specified, the default collation for the specified character
set — at time of the creation of the column — is applied.
Setting a DEFAULT Value
The optional DEFAULT clause allows you to specify the default value for the table column. This value
will be added to the column when an INSERT statement is executed and that column was omitted
from the INSERT command or DEFAULT was used instead of a value expression. The default value will
also be used in UPDATE when DEFAULT is used instead of a value expression.
The default value can be a literal of a compatible type, a context variable that is type-compatible
with the data type of the column, or NULL, if the column allows it. If no default value is explicitly
specified, NULL is implied.
An expression cannot be used as a default value.
Domain-based Columns
To define a column, you can use a previously defined domain. If the definition of a column is based
on a domain, it may contain a new default value, additional CHECK constraints, and a COLLATE clause
that will override the values specified in the domain definition. The definition of such a column
may contain additional column constraints (for instance, NOT NULL), if the domain does not have it.

It is not possible to define a domain-based column that is nullable if the domain
was defined with the NOT NULL attribute. If you want to have a domain that might
be used for defining both nullable and non-nullable columns and variables, it is
better practice defining the domain nullable and apply NOT NULL  in the
Chapter 5. Data Definition (DDL) Statements
132

<!-- Original PDF Page: 134 -->

downstream column definitions and variable declarations.
Identity Columns (Autoincrement)
Identity columns are defined using the GENERATED {ALWAYS | BY DEFAULT} AS IDENTITY  clause. The
identity column is a column associated with an internal sequence. Its value is set automatically
every time it is not specified in the INSERT statement, or when the column value is specified as
DEFAULT.
Rules
• The data type of an identity column must be an exact number type with zero scale. Allowed
types are SMALLINT, INTEGER, BIGINT, NUMERIC(p[,0]) and DECIMAL(p[,0]) with p <= 18.
◦ The INT128 type and numeric types with a precision higher than 18 are not supported.
• An identity column cannot have a DEFAULT or COMPUTED value.
• An identity column can be altered to become a regular column.
• A regular column cannot be altered to become an identity column.
• Identity columns are implicitly NOT NULL (non-nullable), and cannot be made nullable.
• Uniqueness is not enforced automatically. A UNIQUE or PRIMARY KEY constraint is required to
guarantee uniqueness.
• The use of other methods of generating key values for identity columns, e.g. by trigger-
generator code or by allowing users to change or add them, is discouraged to avoid unexpected
key violations.
• The INCREMENT value cannot be zero (0).
GENERATED ALWAYS
An identity column of type GENERATED ALWAYS  will always generate a column value on insert.
Explicitly inserting a value into a column of this type is not allowed, unless:
1. the specified value is DEFAULT; this generates the identity value as normal.
2. the OVERRIDING SYSTEM VALUE clause is specified in the INSERT statement; this allows a user value
to be inserted;
3. the OVERRIDING USER VALUE clause is specified in the INSERT statement; this allows a user specified
value to be ignored (though in general it makes more sense to not include the column in the
INSERT).
GENERATED BY DEFAULT
An identity column of type GENERATED BY DEFAULT will generate a value on insert if no value — other
than DEFAULT — is specified on insert. When the OVERRIDING USER VALUE  clause is specified in the
INSERT statement, the user-provided value is ignored, and an identity value is generated (as if the
column was not included in the insert, or the value DEFAULT was specified).
Chapter 5. Data Definition (DDL) Statements
133

<!-- Original PDF Page: 135 -->

START WITH Option
The optional START WITH clause allows you to specify an initial value other than 1. This value is the
first value generated when using NEXT VALUE FOR sequence.
INCREMENT Option
The optional INCREMENT clause allows you to specify another non-zero step value than 1.

The SQL standard specifies that if INCREMENT is specified with a negative value, and
START WITH is not specified, that the first value generated should be the maximum
of the column type (e.g. 231
 - 1 for INTEGER). Instead, Firebird will start at 1.
Computed Columns
Computed columns can be defined with the COMPUTED [BY] or GENERATED ALWAYS AS clause (the SQL
standard alternative to COMPUTED [BY]). Specifying the data type is optional; if not specified, the
appropriate type will be derived from the expression.
If the data type is explicitly specified for a calculated field, the calculation result is converted to the
specified type. This means, for instance, that the result of a numeric expression could be converted
to a string.
In a query that selects a computed column, the expression is evaluated for each row of the selected
data.

Instead of a computed column, in some cases it makes sense to use a regular
column whose value is calculated in triggers for adding and updating data. It may
reduce the performance of inserting/updating records, but it will increase the
performance of data selection.
Defining an Array Column
• If the column is to be an array, the base type can be any SQL data type except BLOB and array.
• The dimensions of the array are specified between square brackets.
• For each array dimension, one or two integer numbers define the lower and upper boundaries
of its index range:
◦ By default, arrays are 1-based. The lower boundary is implicit and only the upper boundary
need be specified. A single number smaller than 1 defines the range num…1 and a number
greater than 1 defines the range 1…num.
◦ Two numbers separated by a colon (‘:’) and optional whitespace, the second greater than the
first, can be used to define the range explicitly. One or both boundaries can be less than
zero, as long as the upper boundary is greater than the lower.
• When the array has multiple dimensions, the range definitions for each dimension must be
separated by commas and optional whitespace.
• Subscripts are validated only if an array actually exists. It means that no error messages
regarding invalid subscripts will be returned if selecting a specific element returns nothing or if
Chapter 5. Data Definition (DDL) Statements
134

<!-- Original PDF Page: 136 -->

an array field is NULL.
Constraints
Five types of constraints can be specified. They are:
• Primary key (PRIMARY KEY)
• Unique key (UNIQUE)
• Foreign key (REFERENCES)
• CHECK constraint (CHECK)
• NOT NULL constraint (NOT NULL)
Constraints can be specified at column level (“column constraints”) or at table level (“table
constraints”). Table-level constraints are required when keys (unique constraint, primary key,
foreign key) consist of multiple columns and when a CHECK constraint involves other columns in the
row besides the column being defined. The NOT NULL constraint can only be specified as a column
constraint. Syntax for some types of constraint may differ slightly according to whether the
constraint is defined at the column or table level.
• A column-level constraint is specified during a column definition, after all column attributes
except COLLATION are specified, and can involve only the column specified in that definition
• A table-level constraints can only be specified after the definitions of the columns used in the
constraint.
• Table-level constraints are a more flexible way to set constraints, since they can cater for
constraints involving multiple columns
• You can mix column-level and table-level constraints in the same CREATE TABLE statement
The system automatically creates the corresponding index for a primary key ( PRIMARY KEY ), a
unique key ( UNIQUE), and a foreign key ( REFERENCES for a column-level constraint, FOREIGN KEY
REFERENCES for table-level).
Names for Constraints and Their Indexes
Constraints and their indexes are named automatically if no name was specified using the
CONSTRAINT clause:
• The constraint name has the form INTEG_n, where n represents one or more digits
• The index name has the form RDB$PRIMARYn (for a primary key index), RDB$FOREIGNn (for a foreign
key index) or RDB$n (for a unique key index).
Named Constraints
A constraint can be named explicitly if the CONSTRAINT clause is used for its definition. By default,
the constraint index will have the same name as the constraint. If a different name is wanted for
the constraint index, a USING clause can be included.
Chapter 5. Data Definition (DDL) Statements
135

<!-- Original PDF Page: 137 -->

The USING Clause
The USING clause allows you to specify a user-defined name for the index that is created
automatically and, optionally, to define the direction of the index — either ascending (the default)
or descending.
PRIMARY KEY
The PRIMARY KEY constraint is built on one or more key columns, where each column has the NOT
NULL constraint specified. The values across the key columns in any row must be unique. A table can
have only one primary key.
• A single-column primary key can be defined as a column-level or a table-level constraint
• A multi-column primary key must be specified as a table-level constraint
The UNIQUE Constraint
The UNIQUE constraint defines the requirement of content uniqueness for the values in a key
throughout the table. A table can contain any number of unique key constraints.
As with the primary key, the unique constraint can be multi-column. If so, it must be specified as a
table-level constraint.
NULL in Unique Keys
Firebird’s SQL-compliant rules for UNIQUE constraints allow one or more NULLs in a column with a
UNIQUE constraint. This makes it possible to define a UNIQUE constraint on a column that does not
have the NOT NULL constraint.
For UNIQUE keys that span multiple columns, the logic is a little complicated:
• Multiple rows having null in all the columns of the key are allowed
• Multiple rows having keys with different combinations of nulls and non-null values are allowed
• Multiple rows having the same key columns null and the rest filled with non-null values are
allowed, provided the non-null values differ in at least one column
• Multiple rows having the same key columns null and the rest filled with non-null values that
are the same in every column will violate the constraint
The rules for uniqueness can be summarised thus:
In principle, all nulls are considered distinct. However, if two rows have
exactly the same key columns filled with non-null values, the NULL columns
are ignored and the uniqueness is determined on the non-null columns as
though they constituted the entire key.
Illustration
RECREATE TABLE t( x int, y int, z int, unique(x,y,z));
INSERT INTO t values( NULL, 1, 1 );
Chapter 5. Data Definition (DDL) Statements
136

<!-- Original PDF Page: 138 -->

INSERT INTO t values( NULL, NULL, 1 );
INSERT INTO t values( NULL, NULL, NULL );
INSERT INTO t values( NULL, NULL, NULL ); -- Permitted
INSERT INTO t values( NULL, NULL, 1 );    -- Not permitted
FOREIGN KEY
A foreign key ensures that the participating column(s) can contain only values that also exist in the
referenced column(s) in the master table. These referenced columns are often called target
columns. They must be the primary key or a unique key in the target table. They need not have a
NOT NULL constraint defined on them although, if they are the primary key, they will, of course, have
that constraint.
The foreign key columns in the referencing table itself do not require a NOT NULL constraint.
A single-column foreign key can be defined in the column declaration, using the keyword
REFERENCES:
... ,
  ARTIFACT_ID INTEGER REFERENCES COLLECTION (ARTIFACT_ID),
The column ARTIFACT_ID in the example references a column of the same name in the table
COLLECTIONS.
Both single-column and multi-column foreign keys can be defined at the table level . For a multi-
column foreign key, the table-level declaration is the only option.
...
  CONSTRAINT FK_ARTSOURCE FOREIGN KEY(DEALER_ID, COUNTRY)
    REFERENCES DEALER (DEALER_ID, COUNTRY),
Notice that the column names in the referenced (“master”) table may differ from those in the
foreign key.
 If no target columns are specified, the foreign key automatically references the
target table’s primary key.
Foreign Key Actions
With the sub-clauses ON UPDATE and ON DELETE it is possible to specify an action to be taken on the
affected foreign key column(s) when referenced values in the master table are changed:
NO ACTION
(the default) — Nothing is done
CASCADE
The change in the master table is propagated to the corresponding row(s) in the child table. If a
Chapter 5. Data Definition (DDL) Statements
137

<!-- Original PDF Page: 139 -->

key value changes, the corresponding key in the child records changes to the new value; if the
master row is deleted, the child records are deleted.
SET DEFAULT
The foreign key columns in the affected rows will be set to their default values as they were when
the foreign key constraint was defined.
SET NULL
The foreign key columns in the affected rows will be set to NULL.
The specified action, or the default NO ACTION, could cause a foreign key column to become invalid.
For example, it could get a value that is not present in the master table. Such condition will cause
the operation on the master table to fail with an error message.
Example
...
  CONSTRAINT FK_ORDERS_CUST
    FOREIGN KEY (CUSTOMER) REFERENCES CUSTOMERS (ID)
      ON UPDATE CASCADE ON DELETE SET NULL
CHECK Constraint
The CHECK constraint defines the condition the values inserted in this column or row must satisfy. A
condition is a logical expression (also called a predicate) that can return the TRUE, FALSE and UNKNOWN
values. A condition is considered satisfied if the predicate returns TRUE or value UNKNOWN (equivalent
to NULL). If the predicate returns FALSE, the value will not be accepted. This condition is used for
inserting a new row into the table (the INSERT statement) and for updating the existing value of the
table column (the UPDATE statement) and also for statements where one of these actions may take
place (UPDATE OR INSERT, MERGE).

A CHECK constraint on a domain-based column does not replace an existing CHECK
condition on the domain, but becomes an addition to it. The Firebird engine has no
way, during definition, to verify that the extra CHECK does not conflict with the
existing one.
CHECK constraints — whether defined at table level or column level — refer to table columns by their
names. The use of the keyword VALUE as a placeholder — as in domain CHECK constraints — is not
valid in the context of defining constraints in a table.
Example
with two column-level constraints and one at table-level:
CREATE TABLE PLACES (
  ...
  LAT DECIMAL(9, 6) CHECK (ABS(LAT) <=  90),
  LON DECIMAL(9, 6) CHECK (ABS(LON) <= 180),
  ...
Chapter 5. Data Definition (DDL) Statements
138

<!-- Original PDF Page: 140 -->

CONSTRAINT CHK_POLES CHECK (ABS(LAT) < 90 OR LON = 0)
);
NOT NULL Constraint
In Firebird, columns are nullable by default. The NOT NULL constraint specifies that the column
cannot take NULL in place of a value.
A NOT NULL constraint can only be defined as a column constraint, not as a table constraint.
SQL SECURITY Clause
The SQL SECURITY  clause specifies the security context for executing functions referenced in
computed columns, and check constraints, and the default context used for triggers fired for this
table. When SQL Security is not specified, the default value of the database is applied at runtime.
See also SQL Security in chapter Security.
Replication Management
When the database has been configured using ALTER DATABASE INCLUDE ALL TO PUBLICATION , new
tables will automatically be added for publication, unless overridden using the DISABLE PUBLICATION
clause.
If the database has not been configured for INCLUDE ALL (or has later been reconfigured using ALTER
DATABASE EXCLUDE ALL FROM PUBLICATION), new tables will not automatically be added for publication.
To include tables for publication, the ENABLE PUBLICATION clause must be used.
Who Can Create a Table
The CREATE TABLE statement can be executed by:
• Administrators
• Users with the CREATE TABLE privilege
The user executing the CREATE TABLE statement becomes the owner of the table.
CREATE TABLE Examples
1. Creating the COUNTRY table with the primary key specified as a column constraint.
CREATE TABLE COUNTRY (
  COUNTRY COUNTRYNAME NOT NULL PRIMARY KEY,
  CURRENCY VARCHAR(10) NOT NULL
);
2. Creating the STOCK table with the named primary key specified at the column level and the
named unique key specified at the table level.
Chapter 5. Data Definition (DDL) Statements
139

<!-- Original PDF Page: 141 -->

CREATE TABLE STOCK (
  MODEL     SMALLINT NOT NULL CONSTRAINT PK_STOCK PRIMARY KEY,
  MODELNAME CHAR(10) NOT NULL,
  ITEMID    INTEGER NOT NULL,
  CONSTRAINT MOD_UNIQUE UNIQUE (MODELNAME, ITEMID)
);
3. Creating the JOB table with a primary key constraint spanning two columns, a foreign key
constraint for the COUNTRY table and a table-level CHECK constraint. The table also contains an
array of 5 elements.
CREATE TABLE JOB (
  JOB_CODE        JOBCODE NOT NULL,
  JOB_GRADE       JOBGRADE NOT NULL,
  JOB_COUNTRY     COUNTRYNAME,
  JOB_TITLE       VARCHAR(25) NOT NULL,
  MIN_SALARY      NUMERIC(18, 2) DEFAULT 0 NOT NULL,
  MAX_SALARY      NUMERIC(18, 2) NOT NULL,
  JOB_REQUIREMENT BLOB SUB_TYPE 1,
  LANGUAGE_REQ    VARCHAR(15) [1:5],
  PRIMARY KEY (JOB_CODE, JOB_GRADE),
  FOREIGN KEY (JOB_COUNTRY) REFERENCES COUNTRY (COUNTRY)
  ON UPDATE CASCADE
  ON DELETE SET NULL,
  CONSTRAINT CHK_SALARY CHECK (MIN_SALARY < MAX_SALARY)
);
4. Creating the PROJECT table with primary, foreign and unique key constraints with custom index
names specified with the USING clause.
CREATE TABLE PROJECT (
  PROJ_ID     PROJNO NOT NULL,
  PROJ_NAME   VARCHAR(20) NOT NULL UNIQUE USING DESC INDEX IDX_PROJNAME,
  PROJ_DESC   BLOB SUB_TYPE 1,
  TEAM_LEADER EMPNO,
  PRODUCT     PRODTYPE,
  CONSTRAINT PK_PROJECT PRIMARY KEY (PROJ_ID) USING INDEX IDX_PROJ_ID,
  FOREIGN KEY (TEAM_LEADER) REFERENCES EMPLOYEE (EMP_NO)
    USING INDEX IDX_LEADER
);
5. Creating a table with an identity column
create table objects (
  id integer generated by default as identity primary key,
  name varchar(15)
Chapter 5. Data Definition (DDL) Statements
140

<!-- Original PDF Page: 142 -->

);
insert into objects (name) values ('Table');
insert into objects (id, name) values (10, 'Computer');
insert into objects (name) values ('Book');
select * from objects order by id;
          ID NAME
============ ===============
           1 Table
           2 Book
          10 Computer
6. Creating the SALARY_HISTORY table with two computed fields. The first one is declared according
to the SQL standard, while the second one is declared according to the traditional declaration of
computed fields in Firebird.
CREATE TABLE SALARY_HISTORY (
  EMP_NO         EMPNO NOT NULL,
  CHANGE_DATE    TIMESTAMP DEFAULT 'NOW' NOT NULL,
  UPDATER_ID     VARCHAR(20) NOT NULL,
  OLD_SALARY     SALARY NOT NULL,
  PERCENT_CHANGE DOUBLE PRECISION DEFAULT 0 NOT NULL,
  SALARY_CHANGE  GENERATED ALWAYS AS
    (OLD_SALARY * PERCENT_CHANGE / 100),
  NEW_SALARY     COMPUTED BY
    (OLD_SALARY + OLD_SALARY * PERCENT_CHANGE / 100)
);
7. With DEFINER set for table t, user US needs only the SELECT privilege on t. If it were set for
INVOKER, the user would also need the EXECUTE privilege on function f.
set term ^;
create function f() returns int
as
begin
    return 3;
end^
set term ;^
create table t (i integer, c computed by (i + f())) SQL SECURITY DEFINER;
insert into t values (2);
grant select on table t to user us;
commit;
connect 'localhost:/tmp/7.fdb' user us password 'pas';
select * from t;
Chapter 5. Data Definition (DDL) Statements
141

<!-- Original PDF Page: 143 -->

8. With DEFINER set for table tr, user US needs only the INSERT privilege on tr. If it were set for
INVOKER, either the user or the trigger would also need the INSERT privilege on table t. The result
would be the same if SQL SECURITY DEFINER were specified for trigger tr_ins:
create table tr (i integer) SQL SECURITY DEFINER;
create table t (i integer);
set term ^;
create trigger tr_ins for tr after insert
as
begin
  insert into t values (NEW.i);
end^
set term ;^
grant insert on table tr to user us;
commit;
connect 'localhost:/tmp/29.fdb' user us password 'pas';
insert into tr values(2);
Global Temporary Tables (GTT)
Global temporary tables have persistent metadata, but their contents are transaction-bound (the
default) or connection-bound. Every transaction or connection has its own private instance of a
GTT, isolated from all the others. Instances are only created if and when the GTT is referenced. They
are destroyed when the transaction ends or on disconnect. The metadata of a GTT can be modified
or removed using ALTER TABLE and DROP TABLE, respectively.
Syntax
CREATE GLOBAL TEMPORARY TABLE tablename
  (<column_def> [, {<column_def> | <table_constraint>} ...])
  [<gtt_table_attrs>]
<gtt_table_attrs> ::= <gtt_table_attr> [gtt_table_attr> ...]
<gtt_table_attr> ::=
    <sql_security>
  | ON COMMIT {DELETE | PRESERVE} ROWS

Syntax notes
• ON COMMIT DELETE ROWS  creates a transaction-level GTT (the default), ON COMMIT
PRESERVE ROWS a connection-level GTT
• The EXTERNAL [FILE]  clause is not allowed in the definition of a global
temporary table
GTTs are writable in read-only transactions. The effect is as follows:
Chapter 5. Data Definition (DDL) Statements
142

<!-- Original PDF Page: 144 -->

Read-only transaction in read-write database
Writable in both ON COMMIT PRESERVE ROWS and ON COMMIT DELETE ROWS
Read-only transaction in read-only database
Writable in ON COMMIT DELETE ROWS only
Restrictions on GTTs
GTTs can be “dressed up” with all the features of ordinary tables (keys, references, indexes, triggers
and so on), but there are a few restrictions:
• GTTs and regular tables cannot reference one another
• A connection-bound (“PRESERVE ROWS”) GTT cannot reference a transaction-bound (“DELETE ROWS”)
GTT
• Domain constraints cannot reference any GTT
• The destruction of a GTT instance at the end of its lifecycle does not cause any BEFORE/AFTER
delete triggers to fire

In an existing database, it is not always easy to distinguish a regular table from a
GTT, or a transaction-level GTT from a connection-level GTT. Use this query to find
out what type of table you are looking at:
select t.rdb$type_name
from rdb$relations r
join rdb$types t on r.rdb$relation_type = t.rdb$type
where t.rdb$field_name = 'RDB$RELATION_TYPE'
and r.rdb$relation_name = 'TABLENAME'
For an overview of the types of all the relations in the database:
select r.rdb$relation_name, t.rdb$type_name
from rdb$relations r
join rdb$types t on r.rdb$relation_type = t.rdb$type
where t.rdb$field_name = 'RDB$RELATION_TYPE'
and coalesce (r.rdb$system_flag, 0) = 0
The RDB$TYPE_NAME field will show PERSISTENT for a regular table, VIEW for a view,
GLOBAL_TEMPORARY_PRESERVE for a connection-bound GTT and
GLOBAL_TEMPORARY_DELETE for a transaction_bound GTT.
Examples of Global Temporary Tables
1. Creating a connection-scoped global temporary table.
CREATE GLOBAL TEMPORARY TABLE MYCONNGTT (
  ID  INTEGER NOT NULL PRIMARY KEY,
Chapter 5. Data Definition (DDL) Statements
143

<!-- Original PDF Page: 145 -->

TXT VARCHAR(32),
  TS  TIMESTAMP DEFAULT CURRENT_TIMESTAMP)
ON COMMIT PRESERVE ROWS;
2. Creating a transaction-scoped global temporary table that uses a foreign key to reference a
connection-scoped global temporary table. The ON COMMIT sub-clause is optional because DELETE
ROWS is the default.
CREATE GLOBAL TEMPORARY TABLE MYTXGTT (
  ID        INTEGER NOT NULL PRIMARY KEY,
  PARENT_ID INTEGER NOT NULL REFERENCES MYCONNGTT(ID),
  TXT       VARCHAR(32),
  TS        TIMESTAMP DEFAULT CURRENT_TIMESTAMP
) ON COMMIT DELETE ROWS;
External Tables
The optional EXTERNAL [FILE] clause specifies that the table is stored outside the database in an
external text file of fixed-length records. The columns of a table stored in an external file can be of
any type except BLOB or ARRAY, although for most purposes, only columns of CHAR types would be
useful.
All you can do with a table stored in an external file is insert new rows ( INSERT) and query the data
(SELECT). Updating existing data (UPDATE) and deleting rows (DELETE) are not possible.
A file that is defined as an external table must be located on a storage device that is physically
present on the machine where the Firebird server runs and, if the parameter ExternalFileAccess in
the firebird.conf configuration file is Restrict, it must be in one of the directories listed there as
the argument for Restrict. If the file does not exist yet, Firebird will create it on first access.

The ability to use external files for a table depends on the value set for the
ExternalFileAccess parameter in firebird.conf:
• If it is set to None (the default), any attempt to access an external file will be
denied.
• The Restrict setting is recommended, for restricting external file access to
directories created explicitly for the purpose by the server administrator. For
example:
◦ ExternalFileAccess = Restrict externalfiles  will restrict access to a
directory named externalfiles directly beneath the Firebird root directory
◦ ExternalFileAccess = d:\databases\outfiles; e:\infiles will restrict access
to just those two directories on the Windows host server. Note that any
path that is a network mapping will not work. Paths enclosed in single or
double quotes will not work, either.
• If this parameter is set to Full, external files may be accessed anywhere on the
host file system. This creates a security vulnerability and is not recommended.
Chapter 5. Data Definition (DDL) Statements
144

<!-- Original PDF Page: 146 -->

External File Format
The “row” format of the external table is fixed length and binary. There are no field delimiters: both
field and row boundaries are determined by maximum sizes, in bytes, of the field definitions. Keep
this in mind, both when defining the structure of the external table and when designing an input
file for an external table that is to import (or export) data from another application. The ubiquitous
CSV format, for example, is of no use as an input file and cannot be generated directly into an
external file.
The most useful data type for the columns of external tables is the fixed-length CHAR type, of suitable
lengths for the data they are to carry. Date and number types are easily cast to and from strings
whereas the native data types — binary data — will appear to external applications as unparseable
“alphabetti”.
Of course, there are ways to manipulate typed data to generate output files from Firebird that can
be read directly as input files to other applications, using stored procedures, with or without
employing external tables. Such techniques are beyond the scope of a language reference. Here, we
provide guidelines and tips for producing and working with simple text files, since the external
table feature is often used as an easy way to produce or read transaction-independent logs that can
be studied off-line in a text editor or auditing application.
Row Delimiters
Generally, external files are more useful if rows are separated by a delimiter, in the form of a
“newline” sequence that is recognised by reader applications on the intended platform. For most
contexts on Windows, it is the two-byte 'CRLF' sequence, carriage return (ASCII code decimal 13)
and line feed (ASCII code decimal 10). On POSIX, LF on its own is usual. There are various ways to
populate this delimiter column. In our example below, it is done by using a BEFORE INSERT trigger
and the internal function ASCII_CHAR.
External Table Example
For our example, we will define an external log table that might be used by an exception handler in
a stored procedure or trigger. The external table is chosen because the messages from any handled
exceptions will be retained in the log, even if the transaction that launched the process is
eventually rolled back because of another, unhandled exception. For demonstration purposes, it
has two data columns, a timestamp and a message. The third column stores the row delimiter:
CREATE TABLE ext_log
  EXTERNAL FILE 'd:\externals\log_me.txt' (
  stamp CHAR (24),
  message CHAR(100),
  crlf CHAR(2) -- for a Windows context
);
COMMIT;
Now, a trigger, to write the timestamp and the row delimiter each time a message is written to the
file:
Chapter 5. Data Definition (DDL) Statements
145

<!-- Original PDF Page: 147 -->

SET TERM ^;
CREATE TRIGGER bi_ext_log FOR ext_log
ACTIVE BEFORE INSERT
AS
BEGIN
  IF (new.stamp is NULL) then
    new.stamp = CAST (CURRENT_TIMESTAMP as CHAR(24));
  new.crlf = ASCII_CHAR(13) || ASCII_CHAR(10);
END ^
COMMIT ^
SET TERM ;^
Inserting some records (which could have been done by an exception handler or a fan of
Shakespeare):
insert into ext_log (message)
values('Shall I compare thee to a summer''s day?');
insert into ext_log (message)
values('Thou art more lovely and more temperate');
The output:
2015-10-07 15:19:03.4110Shall I compare thee to a summer's day?
2015-10-07 15:19:58.7600Thou art more lovely and more temperate
5.4.2. ALTER TABLE
Alters a table
Available in
DSQL, ESQL
Syntax
ALTER TABLE tablename
  <operation> [, <operation> ...]
<operation> ::=
    ADD <col_def>
  | ADD <tconstraint>
  | DROP colname
  | DROP CONSTRAINT constr_name
  | ALTER [COLUMN] colname <col_mod>
  | ALTER SQL SECURITY {INVOKER | DEFINER}
  | DROP SQL SECURITY
  | {ENABLE | DISABLE} PUBLICATION
Chapter 5. Data Definition (DDL) Statements
146

<!-- Original PDF Page: 148 -->

<col_mod> ::=
    TO newname
  | POSITION newpos
  | <regular_col_mod>
  | <computed_col_mod>
  | <identity_col_mod>
<regular_col_mod> ::=
    TYPE {<datatype> | domainname}
  | SET DEFAULT {<literal> | NULL | <context_var>}
  | DROP DEFAULT
  | {SET | DROP} NOT NULL
<computed_col_mod> ::=
    [TYPE <datatype>] {COMPUTED [BY] | GENERATED ALWAYS AS} (<expression>)
<identity_col_mod> ::=
    SET GENERATED {ALWAYS | BY DEFAULT} [<identity_mod_option>...]
  | <identity_mod_options>...
  | DROP IDENTITY
<identity_mod_options> ::=
    RESTART [WITH restart_value]
  | SET INCREMENT [BY] inc_value
!! See CREATE TABLE syntax for further rules !!
Table 34. ALTER TABLE Statement Parameters
Parameter Description
tablename Name (identifier) of the table
operation One of the available operations altering the structure of the table
colname Name (identifier) for a column in the table. The maximum length is 63
characters. Must be unique in the table.
domain_name Domain name
newname New name (identifier) for the column. The maximum length is 63
characters. Must be unique in the table.
newpos The new column position (an integer between 1 and the number of
columns in the table)
other_table The name of the table referenced by the foreign key constraint
literal A literal value that is allowed in the given context
context_var A context variable whose type is allowed in the given context
check_condition The condition of a CHECK constraint that will be satisfied if it evaluates to
TRUE or UNKNOWN/NULL
restart_value The first value of the identity column after restart
Chapter 5. Data Definition (DDL) Statements
147

<!-- Original PDF Page: 149 -->

Parameter Description
inc_value The increment (or step) value of the identity column; zero (0) is not
allowed.
The ALTER TABLE  statement changes the structure of an existing table. With one ALTER TABLE 
statement it is possible to perform multiple operations, adding/dropping columns and constraints
and also altering column specifications.
Multiple operations in an ALTER TABLE statement are separated with commas.
Version Count Increments
Some changes in the structure of a table increment the metadata change counter (“version count”)
assigned to every table. The number of metadata changes is limited to 255 for each table, or 32,000
for each view. Once the counter reaches this limit, you will not be able to make any further changes
to the structure of the table or view without resetting the counter.
To reset the metadata change counter
You need to back up and restore the database using the gbak utility.
The ADD Clause
With the ADD clause you can add a new column or a new table constraint. The syntax for defining
the column and the syntax of defining the table constraint correspond with those described for
CREATE TABLE statement.
Effect on Version Count
• Each time a new column is added, the metadata change counter is increased by one
• Adding a new table constraint does not increase the metadata change counter

Points to Be Aware of
1. Adding a column with a NOT NULL constraint without a DEFAULT value will fail if
the table has existing rows. When adding a non-nullable column, it is
recommended either to set a default value for it, or to create it as nullable,
update the column in existing rows with a non-null value, and then add a NOT
NULL constraint.
2. When a new CHECK constraint is added, existing data is not tested for
compliance. Prior testing of existing data against the new CHECK expression is
recommended.
3. Although adding an identity column is supported, this will only succeed if the
table is empty. Adding an identity column will fail if the table has one or more
rows.
Chapter 5. Data Definition (DDL) Statements
148

<!-- Original PDF Page: 150 -->

The DROP Clause
The DROP colname clause deletes the specified column from the table. An attempt to drop a column
will fail if anything references it. Consider the following items as sources of potential dependencies:
• column or table constraints
• indexes
• stored procedures, functions and triggers
• views
Effect on Version Count
• Each time a column is dropped, the table’s metadata change counter is increased by one.
The DROP CONSTRAINT Clause
The DROP CONSTRAINT clause deletes the specified column-level or table-level constraint.
A PRIMARY KEY  or UNIQUE key constraint cannot be deleted if it is referenced by a FOREIGN KEY 
constraint in another table. It will be necessary to drop that FOREIGN KEY  constraint before
attempting to drop the PRIMARY KEY or UNIQUE key constraint it references.
Effect on Version Count
• Deleting a column constraint or a table constraint does not increase the metadata change
counter.
The ALTER [COLUMN] Clause
With the ALTER [COLUMN] clause, attributes of existing columns can be modified without the need to
drop and re-add the column. Permitted modifications are:
• change the name (does not affect the metadata change counter)
• change the data type (increases the metadata change counter by one)
• change the column position in the column list of the table (does not affect the metadata change
counter)
• delete the default column value (does not affect the metadata change counter)
• set a default column value or change the existing default (does not affect the metadata change
counter)
• change the type and expression for a computed column (does not affect the metadata change
counter)
• set the NOT NULL constraint (does not affect the metadata change counter)
• drop the NOT NULL constraint (does not affect the metadata change counter)
• change the type of an identity column, or change an identity column to a regular column
• restart an identity column
• change the increment of an identity column
Chapter 5. Data Definition (DDL) Statements
149

<!-- Original PDF Page: 151 -->

Renaming a Column: the TO Clause
The TO keyword with a new identifier renames an existing column. The table must not have an
existing column that has the same identifier.
It will not be possible to change the name of a column that is included in any constraint: primary
key, unique key, foreign key, or CHECK constraints of the table.
Renaming a column will also be disallowed if the column is used in any stored PSQL module or
view.
Changing the Data Type of a Column: the TYPE Clause
The keyword TYPE changes the data type of an existing column to another, allowable type. A type
change that might result in data loss will be disallowed. As an example, the number of characters in
the new type for a CHAR or VARCHAR column cannot be smaller than the existing specification for it.
If the column was declared as an array, no change to its type or its number of dimensions is
permitted.
The data type of a column that is involved in a foreign key, primary key or unique constraint cannot
be changed at all.
Changing the Position of a Column: the POSITION Clause
The POSITION keyword changes the position of an existing column in the notional “left-to-right”
layout of the record.
Numbering of column positions starts at 1.
• If a position less than 1 is specified, an error message will be returned
• If a position number is greater than the number of columns in the table, its new position will be
adjusted silently to match the number of columns.
The DROP DEFAULT and SET DEFAULT Clauses
The optional DROP DEFAULT clause deletes the current default value for the column.
• If the column is based on a domain with a default value, the default value will revert to the
domain default
• An error will be raised if an attempt is made to delete the default value of a column which has
no default value or whose default value is domain-based
The optional SET DEFAULT clause sets a default value for the column. If the column already has a
default value, it will be replaced with the new one. The default value applied to a column always
overrides one inherited from a domain.
The SET NOT NULL and DROP NOT NULL Clauses
The SET NOT NULL  clause adds a NOT NULL  constraint on an existing table column. Contrary to
definition in CREATE TABLE, it is not possible to specify a constraint name.
Chapter 5. Data Definition (DDL) Statements
150

<!-- Original PDF Page: 152 -->


The successful addition of the NOT NULL constraint is subject to a full data validation
on the table, so ensure that the column has no nulls before attempting the change.
An explicit NOT NULL  constraint on domain-based column overrides domain
settings. In this scenario, changing the domain to be nullable does not extend to a
table column.
Dropping the NOT NULL constraint from the column if its type is a domain that also has a NOT NULL
constraint, has no observable effect until the NOT NULL constraint is dropped from the domain as
well.
The COMPUTED [BY] or GENERATED ALWAYS AS Clauses
The data type and expression underlying a computed column can be modified using a COMPUTED
[BY] or GENERATED ALWAYS AS  clause in the ALTER TABLE ALTER [COLUMN]  statement. Conversion of a
regular column to a computed one and vice versa is not permitted.
Changing Identity Columns
For identity columns ( SET GENERATED {ALWAYS | BY DEFAULT} ) it is possible to modify several
properties using the following clauses.
Identity Type
The SET GENERATED {ALWAYS | BY DEFAULT} changes an identity column from ALWAYS to BY DEFAULT and
vice versa. It is not possible to use this to change a regular column to an identity column.
RESTART
The RESTART clause restarts the sequence used for generating identity values. If only the RESTART
clause is specified, then the sequence resets to the initial value specified when the identity column
was defined. If the optional WITH restart_value clause is specified, the sequence will restart with the
specified value.

In Firebird 3.0, RESTART WITH restart_value would also change the configured
initial value to restart_value. This was not compliant with the SQL standard, so
since Firebird 4.0, RESTART WITH restart_value will only restart the sequence with
the specified value. Subsequent RESTARTs (without WITH) will use the START WITH
value specified when the identity column was defined.
It is currently not possible to change the configured start value.
SET INCREMENT
The SET INCREMENT clause changes the increment of the identity column.
DROP IDENTITY
The DROP IDENTITY clause will change an identity column to a regular column.
Chapter 5. Data Definition (DDL) Statements
151

<!-- Original PDF Page: 153 -->

 It is not possible to change a regular column to an identity column.
Changing SQL Security
Using the ALTER SQL SECURITY or DROP SQL SECURITY clauses, it is possible to change or drop the SQL
Security property of a table. After dropping SQL Security, the default value of the database is
applied at runtime.

If the SQL Security property is changed for a table, triggers that do not have an
explicit SQL Security property will not see the effect of the change until the next
time the trigger is loaded into the metadata cache.
Replication Management
To stop replicating a table, use the DISABLE PUBLICATION clause. To start replicating a table, use the
ENABLE PUBLICATION clause.
The change in publication status takes effect at commit.
Attributes that Cannot Be Altered
The following alterations are not supported:
• Changing the collation of a character type column
Who Can Alter a Table?
The ALTER TABLE statement can be executed by:
• Administrators
• The owner of the table
• Users with the ALTER ANY TABLE privilege
Examples Using ALTER TABLE
1. Adding the CAPITAL column to the COUNTRY table.
ALTER TABLE COUNTRY
  ADD CAPITAL VARCHAR(25);
2. Adding the CAPITAL column with the NOT NULL and UNIQUE constraint and deleting the CURRENCY
column.
ALTER TABLE COUNTRY
  ADD CAPITAL VARCHAR(25) NOT NULL UNIQUE,
  DROP CURRENCY;
Chapter 5. Data Definition (DDL) Statements
152

<!-- Original PDF Page: 154 -->

3. Adding the CHK_SALARY check constraint and a foreign key to the JOB table.
ALTER TABLE JOB
  ADD CONSTRAINT CHK_SALARY CHECK (MIN_SALARY < MAX_SALARY),
  ADD FOREIGN KEY (JOB_COUNTRY) REFERENCES COUNTRY (COUNTRY);
4. Setting default value for the MODEL field, changing the type of the ITEMID column and renaming
the MODELNAME column.
ALTER TABLE STOCK
  ALTER COLUMN MODEL SET DEFAULT 1,
  ALTER COLUMN ITEMID TYPE BIGINT,
  ALTER COLUMN MODELNAME TO NAME;
5. Restarting the sequence of an identity column.
ALTER TABLE objects
  ALTER ID RESTART WITH 100;
6. Changing the computed columns NEW_SALARY and SALARY_CHANGE.
ALTER TABLE SALARY_HISTORY
  ALTER NEW_SALARY GENERATED ALWAYS AS
    (OLD_SALARY + OLD_SALARY * PERCENT_CHANGE / 100),
  ALTER SALARY_CHANGE COMPUTED BY
    (OLD_SALARY * PERCENT_CHANGE / 100);
See also
CREATE TABLE, DROP TABLE, CREATE DOMAIN
5.4.3. DROP TABLE
Drops a table
Available in
DSQL, ESQL
Syntax
DROP TABLE tablename
Table 35. DROP TABLE Statement Parameter
Parameter Description
tablename Name (identifier) of the table
Chapter 5. Data Definition (DDL) Statements
153

<!-- Original PDF Page: 155 -->

The DROP TABLE statement drops (deletes) an existing table. If the table has dependencies, the DROP
TABLE statement will fail with an error.
When a table is dropped, all its triggers and indexes will be deleted as well.
Who Can Drop a Table?
The DROP TABLE statement can be executed by:
• Administrators
• The owner of the table
• Users with the DROP ANY TABLE privilege
Example of DROP TABLE
Dropping the COUNTRY table.
DROP TABLE COUNTRY;
See also
CREATE TABLE, ALTER TABLE, RECREATE TABLE
5.4.4. RECREATE TABLE
Drops a table if it exists, and creates a table
Available in
DSQL
Syntax
RECREATE [GLOBAL TEMPORARY] TABLE tablename
  [EXTERNAL [FILE] 'filespec']
  (<col_def> [, {<col_def> | <tconstraint>} ...])
  [{<table_attrs> | <gtt_table_attrs>}]
See the CREATE TABLE section for the full syntax of CREATE TABLE and descriptions of defining tables,
columns and constraints.
RECREATE TABLE creates or recreates a table. If a table with this name already exists, the RECREATE
TABLE statement will try to drop it and create a new one. Existing dependencies will prevent the
statement from executing.
Example of RECREATE TABLE
Creating or recreating the COUNTRY table.
RECREATE TABLE COUNTRY (
  COUNTRY COUNTRYNAME NOT NULL PRIMARY KEY,
Chapter 5. Data Definition (DDL) Statements
154

<!-- Original PDF Page: 156 -->

CURRENCY VARCHAR(10) NOT NULL
);
See also
CREATE TABLE, DROP TABLE
5.5. INDEX
An index is a database object used for faster data retrieval from a table or for speeding up the
sorting in a query. Indexes are also used to enforce the referential integrity constraints PRIMARY KEY,
FOREIGN KEY and UNIQUE.
This section describes how to create indexes, activate and deactivate them, drop them and collect
statistics (recalculate selectivity) for them.
5.5.1. CREATE INDEX
Creates an index
Available in
DSQL, ESQL
Syntax
CREATE [UNIQUE] [ASC[ENDING] | DESC[ENDING]]
  INDEX indexname ON tablename
  {(col [, col ...]) | COMPUTED BY (<expression>)}
  [WHERE <search_condition>]
Table 36. CREATE INDEX Statement Parameters
Parameter Description
indexname Index name. The maximum length is 63 characters
tablename The name of the table for which the index is to be built
col Name of a column in the table. Columns of the types BLOB and ARRAY and
computed fields cannot be used in an index.
expression The expression that will compute the values for a computed index, also
known as an “expression index”
search_condition Conditional expression of a partial index, to filter the rows to include in
the index.
The CREATE INDEX statement creates an index for a table that can be used to speed up searching,
sorting and grouping. Indexes are created automatically in the process of defining constraints, such
as primary key, foreign key or unique constraints.
An index can be built on the content of columns of any data type except for BLOB and arrays. The
Chapter 5. Data Definition (DDL) Statements
155

<!-- Original PDF Page: 157 -->

name (identifier) of an index must be unique among all index names.
Key Indexes
When a primary key, foreign key or unique constraint is added to a table or column, an index
with the same name is created automatically, without an explicit directive from the designer.
For example, the PK_COUNTRY index will be created automatically when you execute and
commit the following statement:
ALTER TABLE COUNTRY ADD CONSTRAINT PK_COUNTRY
  PRIMARY KEY (ID);
Who Can Create an Index?
The CREATE INDEX statement can be executed by:
• Administrators
• The owner of the table
• Users with the ALTER ANY TABLE privilege
Unique Indexes
Specifying the keyword UNIQUE in the index creation statement creates an index in which
uniqueness will be enforced throughout the table. The index is referred to as a “unique index”. A
unique index is not a constraint.
Unique indexes cannot contain duplicate key values (or duplicate key value combinations, in the
case of compound, or multi-column, or multi-segment) indexes. Duplicated NULLs are permitted, in
accordance with the SQL standard, in both single-segment and multi-segment indexes.
Partial Indexes
Specifying the WHERE clause in the index creation statement creates a partial index (also knows as
filtered index). A partial index contains only rows that match the search condition of the WHERE.
A partial index definition may include the UNIQUE clause. In this case, every key in the index is
required to be unique. This allows enforcing uniqueness for a subset of table rows.
A partial index is usable only in the following cases:
• The WHERE clause of the statement includes exactly the same boolean expression as the one
defined for the index;
• The search condition defined for the index contains ORed boolean expressions and one of them
is explicitly included in the WHERE clause of the statement;
• The search condition defined for the index specifies IS NOT NULL  and the WHERE clause of the
statement includes an expression on the same field that is known to exclude NULLs.
Chapter 5. Data Definition (DDL) Statements
156

<!-- Original PDF Page: 158 -->

Index Direction
All indexes in Firebird are uni-directional. An index may be constructed from the lowest value to
the highest (ascending order) or from the highest value to the lowest (descending order). The
keywords ASC[ENDING] and DESC[ENDING] are used to specify the direction of the index. The default
index order is ASC[ENDING]. It is valid to define both an ascending and a descending index on the
same column or key set.
 A descending index can be useful on a column that will be subjected to searches on
the high values (“newest”, maximum, etc.)
Firebird uses B-tree indexes, which are bidirectional. However, due to technical limitations,
Firebird uses an index in one direction only.
See also Firebird for the Database Expert: Episode 3 - On disk consistency
Computed (Expression) Indexes
In creating an index, you can use the COMPUTED BY clause to specify an expression instead of one or
more columns. Computed indexes are used in queries where the condition in a WHERE, ORDER BY or
GROUP BY  clause exactly matches the expression in the index definition. The expression in a
computed index may involve several columns in the table.
 Expression indexes can also be used as a workaround for indexing computed
columns: use the name of the computed column as the expression.
Limits on Indexes
Certain limits apply to indexes.
The maximum length of a key in an index is limited to a quarter of the page size.
Maximum Indexes per Table
The number of indexes that can be accommodated for each table is limited. The actual maximum
for a specific table depends on the page size and the number of columns in the indexes.
Table 37. Maximum Indexes per Table
Page Size Number of Indexes Depending on Column Count
Single 2-Column 3-Column
4096 203 145 113
8192 408 291 227
16384 818 584 454
32768 1637 1169 909
Chapter 5. Data Definition (DDL) Statements
157

<!-- Original PDF Page: 159 -->

Character Index Limits
The maximum indexed string length is 9 bytes less than the maximum key length. The maximum
indexable string length depends on the page size, the character set, and the collation.
Table 38. Maximum indexable (VAR)CHAR length
Page Size Maximum Indexable String Length by Charset Type
1 byte/char 2 byte/char 3 byte/char 4 byte/char
4096 1015 507 338 253
8192 2039 1019 679 509
16384 4087 2043 1362 1021
32768 8183 4091 2727 2045

Depending on the collation, the maximum size can be further reduced as case-
insensitive and accent-insensitive collations require more bytes per character in
an index. See also Character Indexes in Chapter Data Types and Subtypes.
Parallelized Index Creation
Since Firebird 5.0, index creation can be parallelized. Parallelization happens automatically if the
current connection has two or more parallel workers — configured through ParallelWorkers in
firebird.conf or isc_dpb_parallel_workers — and the server has parallel workers available.
Examples Using CREATE INDEX
1. Creating an index for the UPDATER_ID column in the SALARY_HISTORY table
CREATE INDEX IDX_UPDATER
  ON SALARY_HISTORY (UPDATER_ID);
2. Creating an index with keys sorted in the descending order for the CHANGE_DATE column in the
SALARY_HISTORY table
CREATE DESCENDING INDEX IDX_CHANGE
  ON SALARY_HISTORY (CHANGE_DATE);
3. Creating a multi-segment index for the ORDER_STATUS, PAID columns in the SALES table
CREATE INDEX IDX_SALESTAT
  ON SALES (ORDER_STATUS, PAID);
4. Creating an index that does not permit duplicate values for the NAME column in the COUNTRY table
CREATE UNIQUE INDEX UNQ_COUNTRY_NAME
Chapter 5. Data Definition (DDL) Statements
158

<!-- Original PDF Page: 160 -->

ON COUNTRY (NAME);
5. Creating a computed index for the PERSONS table
CREATE INDEX IDX_NAME_UPPER ON PERSONS
  COMPUTED BY (UPPER (NAME));
An index like this can be used for a case-insensitive search:
SELECT *
FROM PERSONS
WHERE UPPER(NAME) STARTING WITH UPPER('Iv');
6. Creating a partial index and using its condition:
CREATE INDEX IT1_COL ON T1 (COL) WHERE COL < 100;
SELECT * FROM T1 WHERE COL < 100;
-- PLAN (T1 INDEX (IT1_COL))
7. Creating a partial index which excludes NULL
CREATE INDEX IT1_COL2 ON T1 (COL) WHERE COL IS NOT NULL;
SELECT * FROM T1 WHERE COL > 100;
PLAN (T1 INDEX IT1_COL2)
8. Creating a partial index with ORed conditions
CREATE INDEX IT1_COL3 ON T1 (COL) WHERE COL = 1 OR COL = 2;
SELECT * FROM T1 WHERE COL = 2;
-- PLAN (T1 INDEX IT1_COL3)
9. Using a partial index to enforce uniqueness for a subset of rows
create table OFFER (
  OFFER_ID bigint generated always as identity primary key,
  PRODUCT_ID bigint not null,
  ARCHIVED boolean default false not null,
  PRICE decimal(9,2) not null
);
create unique index IDX_OFFER_UNIQUE_PRODUCT
  on OFFER (PRODUCT_ID)
  where not ARCHIVED;
Chapter 5. Data Definition (DDL) Statements
159

<!-- Original PDF Page: 161 -->

insert into OFFER (PRODUCT_ID, ARCHIVED, PRICE) values (1, false, 18.95);
insert into OFFER (PRODUCT_ID, ARCHIVED, PRICE) values (1, true, 17.95);
insert into OFFER (PRODUCT_ID, ARCHIVED, PRICE) values (1, true, 16.95);
-- Next fails due to second record for PRODUCT_ID=1 and ARCHIVED=false:
insert into OFFER (PRODUCT_ID, ARCHIVED, PRICE) values (1, false, 19.95);
-- Statement failed, SQLSTATE = 23000
-- attempt to store duplicate value (visible to active transactions) in unique
index "IDX_OFFER_UNIQUE_PRODUCT"
-- -Problematic key value is ("PRODUCT_ID" = 1)
See also
ALTER INDEX, DROP INDEX
5.5.2. ALTER INDEX
Activates or deactivates an index, and rebuilds an index
Available in
DSQL, ESQL
Syntax
ALTER INDEX indexname {ACTIVE | INACTIVE}
Table 39. ALTER INDEX Statement Parameter
Parameter Description
indexname Index name
The ALTER INDEX statement activates or deactivates an index. There is no facility on this statement
for altering any attributes of the index.
INACTIVE
With the INACTIVE option, the index is switched from the active to inactive state. The effect is
similar to the DROP INDEX statement except that the index definition remains in the database.
Altering a constraint index to the inactive state is not permitted.
An active index can be deactivated if there are no queries prepared using that index; otherwise,
an “object in use” error is returned.
Activating an inactive index is also safe. However, if there are active transactions modifying the
table, the transaction containing the ALTER INDEX statement will fail if it has the NOWAIT attribute.
If the transaction is in WAIT mode, it will wait for completion of concurrent transactions.
On the other side of the coin, if our ALTER INDEX succeeds and starts to rebuild the index at
COMMIT, other transactions modifying that table will fail or wait, according to their WAIT/NO WAIT
attributes. The situation is the same for CREATE INDEX.
 How is it Useful?
Chapter 5. Data Definition (DDL) Statements
160

<!-- Original PDF Page: 162 -->

It might be useful to switch an index to the inactive state whilst inserting,
updating or deleting a large batch of records in the table that owns the index.
ACTIVE
Rebuilds the index (even if already active), and marks it as active.

How is it Useful?
Even if the index is active when ALTER INDEX … ACTIVE is executed, the index
will be rebuilt. Rebuilding indexes can be a useful piece of housekeeping to do,
occasionally, on the indexes of a large table in a database that has frequent
inserts, updates or deletes but is infrequently restored.
Who Can Alter an Index?
The ALTER INDEX statement can be executed by:
• Administrators
• The owner of the table
• Users with the ALTER ANY TABLE privilege
Use of ALTER INDEX on a Constraint Index
Altering the index of a PRIMARY KEY, FOREIGN KEY or UNIQUE constraint to INACTIVE is not permitted.
However, ALTER INDEX … ACTIVE works just as well with constraint indexes as it does with others, as
an index rebuilding tool.
ALTER INDEX Examples
1. Deactivating the IDX_UPDATER index
ALTER INDEX IDX_UPDATER INACTIVE;
2. Switching the IDX_UPDATER index back to the active state and rebuilding it
ALTER INDEX IDX_UPDATER ACTIVE;
See also
CREATE INDEX, DROP INDEX, SET STATISTICS
5.5.3. DROP INDEX
Drops an index
Available in
DSQL, ESQL
Chapter 5. Data Definition (DDL) Statements
161

<!-- Original PDF Page: 163 -->

Syntax
DROP INDEX indexname
Table 40. DROP INDEX Statement Parameter
Parameter Description
indexname Index name
The DROP INDEX statement drops (deletes) the named index from the database.

A constraint index cannot be dropped using DROP INDEX. Constraint indexes are
dropped during the process of executing the command ALTER TABLE …  DROP
CONSTRAINT ….
Who Can Drop an Index?
The DROP INDEX statement can be executed by:
• Administrators
• The owner of the table
• Users with the ALTER ANY TABLE privilege
DROP INDEX Example
Dropping the IDX_UPDATER index
DROP INDEX IDX_UPDATER;
See also
CREATE INDEX, ALTER INDEX
5.5.4. SET STATISTICS
Recalculates the selectivity of an index
Available in
DSQL, ESQL
Syntax
SET STATISTICS INDEX indexname
Table 41. SET STATISTICS Statement Parameter
Parameter Description
indexname Index name
Chapter 5. Data Definition (DDL) Statements
162

<!-- Original PDF Page: 164 -->

The SET STATISTICS statement recalculates the selectivity of the specified index.
Who Can Update Index Statistics?
The SET STATISTICS statement can be executed by:
• Administrators
• The owner of the table
• Users with the ALTER ANY TABLE privilege
Index Selectivity
The selectivity of an index is the result of evaluating the number of rows that can be selected in a
search on every index value. A unique index has the maximum selectivity because it is impossible
to select more than one row for each value of an index key if it is used. Keeping the selectivity of an
index up to date is important for the optimizer’s choices in seeking the most optimal query plan.
Index statistics in Firebird are not automatically recalculated in response to large batches of
inserts, updates or deletions. It may be beneficial to recalculate the selectivity of an index after such
operations because the selectivity tends to become outdated.
 The statements CREATE INDEX and ALTER INDEX ACTIVE both store index statistics that
correspond to the contents of the newly-[re]built index.
It can be performed under concurrent load without risk of corruption. However, under concurrent
load, the newly calculated statistics could become outdated as soon as SET STATISTICS finishes.
Example Using SET STATISTICS
Recalculating the selectivity of the index IDX_UPDATER
SET STATISTICS INDEX IDX_UPDATER;
See also
CREATE INDEX, ALTER INDEX
5.6. VIEW
A view is a virtual table that is a stored and named SELECT query for retrieving data of any
complexity. Data can be retrieved from one or more tables, from other views and also from
selectable stored procedures.
Unlike regular tables in relational databases, a view is not an independent data set stored in the
database. The result is dynamically created as a data set when the view is selected.
The metadata of a view are available to the process that generates the binary code for stored
procedures and triggers, as though they were concrete tables storing persistent data.
Chapter 5. Data Definition (DDL) Statements
163

<!-- Original PDF Page: 165 -->

Firebird does not support materialized views.
5.6.1. CREATE VIEW
Creates a view
Available in
DSQL
Syntax
CREATE VIEW viewname [<full_column_list>]
  AS <select_statement>
  [WITH CHECK OPTION]
<full_column_list> ::= (colname [, colname ...])
Table 42. CREATE VIEW Statement Parameters
Parameter Description
viewname View name. The maximum length is 63 characters
select_statement SELECT statement
full_column_list The list of columns in the view
colname View column name. Duplicate column names are not allowed.
The CREATE VIEW statement creates a new view. The identifier (name) of a view must be unique
among the names of all views, tables, and stored procedures in the database.
The name of the new view can be followed by the list of column names that should be returned to
the caller when the view is invoked. Names in the list do not have to be related to the names of the
columns in the base tables from which they derive.
If the view column list is omitted, the system will use the column names and/or aliases from the
SELECT statement. If duplicate names or non-aliased expression-derived columns make it impossible
to obtain a valid list, creation of the view fails with an error.
The number of columns in the view’s list must match the number of columns in the selection list of
the underlying SELECT statement in the view definition.

Additional Points
• If the full list of columns is specified, it makes no sense to specify aliases in the
SELECT statement because the names in the column list will override them
• The column list is optional if all the columns in the SELECT are explicitly named
and are unique in the selection list
Chapter 5. Data Definition (DDL) Statements
164

<!-- Original PDF Page: 166 -->

Updatable Views
A view can be updatable or read-only. If a view is updatable, the data retrieved when this view is
called can be changed by the DML statements INSERT, UPDATE, DELETE, UPDATE OR INSERT  or MERGE.
Changes made in an updatable view are applied to the underlying table(s).
A read-only view can be made updatable with the use of triggers. Once triggers have been defined
on a view, changes posted to it will never be written automatically to the underlying table, even if
the view was updatable to begin with. It is the responsibility of the programmer to ensure that the
triggers update (or delete from, or insert into) the base tables as needed.
A view will be automatically updatable if all the following conditions are met:
• the SELECT statement queries only one table or one updatable view
• the SELECT statement does not call any stored procedures
• each base table (or base view) column not present in the view definition meets one of the
following conditions:
◦ it is nullable
◦ it has a non-NULL default value
◦ it has a trigger that supplies a permitted value
• the SELECT statement contains no fields derived from subqueries or other expressions
• the SELECT statement does not contain fields defined through aggregate functions ( MIN, MAX, AVG,
SUM, COUNT, LIST, etc.), statistical functions ( CORR, COVAR_POP, COVAR_SAMP, etc.), linear regression
functions (REGR_AVGX, REGR_AVGY, etc.) or any type of window function
• the SELECT statement contains no ORDER BY, GROUP BY or HAVING clause
• the SELECT statement does not include the keyword DISTINCT or row-restrictive keywords such as
ROWS, FIRST, SKIP, OFFSET or FETCH

The RETURNING clause and updatable views
The RETURNING clause of a DML statement used on a view made updatable using
triggers may not always report the correct values. For example, values of identity
column, computed columns, default values, or other expressions performed by the
trigger will not be automatically reflected in the RETURNING columns.
To report the right values in RETURNING, the trigger will need to explicitly assign
those values to the columns of the NEW record.
WITH CHECK OPTION
The optional WITH CHECK OPTION  clause requires an updatable view to check whether new or
updated data meet the condition specified in the WHERE clause of the SELECT statement. Every attempt
to insert a new record or to update an existing one is checked whether the new or updated record
would meet the WHERE criteria. If they fail the check, the operation is not performed and an error is
raised.
WITH CHECK OPTION  can be specified only in a CREATE VIEW statement in which a WHERE clause is
Chapter 5. Data Definition (DDL) Statements
165

<!-- Original PDF Page: 167 -->

present to restrict the output of the main SELECT statement. An error message is returned otherwise.

Please note:
If WITH CHECK OPTION is used, the engine checks the input against the WHERE clause
before passing anything to the base relation. Therefore, if the check on the input
fails, any default clauses or triggers on the base relation that might have been
designed to correct the input will never come into action.
Furthermore, view fields omitted from the INSERT statement are passed as NULLs to
the base relation, regardless of their presence or absence in the WHERE clause. As a
result, base table defaults defined on such fields will not be applied. Triggers, on
the other hand, will fire and work as expected.
For views that do not have WITH CHECK OPTION , fields omitted from the INSERT
statement are not passed to the base relation at all, so any defaults will be applied.
Who Can Create a View?
The CREATE VIEW statement can be executed by:
• Administrators
• Users with the CREATE VIEW privilege
The creator of a view becomes its owner.
To create a view, a non-admin user also needs at least SELECT access to the underlying table(s)
and/or view(s), and the EXECUTE privilege on any selectable stored procedures involved.
To enable insertions, updates and deletions through the view, the creator/owner must also possess
the corresponding INSERT, UPDATE and DELETE rights on the underlying object(s).
Granting other users privileges on the view is only possible if the view owner has these privileges
on the underlying objects WITH GRANT OPTION. This will always be the case if the view owner is also
the owner of the underlying objects.
Examples of Creating Views
1. Creating view returning the JOB_CODE and JOB_TITLE columns only for those jobs where
MAX_SALARY is less than $15,000.
CREATE VIEW ENTRY_LEVEL_JOBS AS
SELECT JOB_CODE, JOB_TITLE
FROM JOB
WHERE MAX_SALARY < 15000;
2. Creating a view returning the JOB_CODE and JOB_TITLE columns only for those jobs where
MAX_SALARY is less than $15,000. Whenever a new record is inserted or an existing record is
updated, the MAX_SALARY < 15000  condition will be checked. If the condition is not true, the
Chapter 5. Data Definition (DDL) Statements
166

<!-- Original PDF Page: 168 -->

insert/update operation will be rejected.
CREATE VIEW ENTRY_LEVEL_JOBS AS
SELECT JOB_CODE, JOB_TITLE
FROM JOB
WHERE MAX_SALARY < 15000
WITH CHECK OPTION;
3. Creating a view with an explicit column list.
CREATE VIEW PRICE_WITH_MARKUP (
  CODE_PRICE,
  COST,
  COST_WITH_MARKUP
) AS
SELECT
  CODE_PRICE,
  COST,
  COST * 1.1
FROM PRICE;
4. Creating a view with the help of aliases for fields in the SELECT statement (the same result as in
Example 3).
CREATE VIEW PRICE_WITH_MARKUP AS
SELECT
  CODE_PRICE,
  COST,
  COST * 1.1 AS COST_WITH_MARKUP
FROM PRICE;
5. Creating a read-only view based on two tables and a stored procedure.
CREATE VIEW GOODS_PRICE AS
SELECT
  goods.name AS goodsname,
  price.cost AS cost,
  b.quantity AS quantity
FROM
  goods
  JOIN price ON goods.code_goods = price.code_goods
  LEFT JOIN sp_get_balance(goods.code_goods) b ON 1 = 1;
See also
ALTER VIEW, CREATE OR ALTER VIEW, RECREATE VIEW, DROP VIEW
Chapter 5. Data Definition (DDL) Statements
167

<!-- Original PDF Page: 169 -->

5.6.2. ALTER VIEW
Alters a view
Available in
DSQL
Syntax
ALTER VIEW viewname [<full_column_list>]
    AS <select_statement>
    [WITH CHECK OPTION]
<full_column_list> ::= (colname [, colname ...])
Table 43. ALTER VIEW Statement Parameters
Parameter Description
viewname Name of an existing view
select_statement SELECT statement
full_column_list The list of columns in the view
colname View column name. Duplicate column names are not allowed.
Use the ALTER VIEW statement for changing the definition of an existing view. Privileges for views
remain intact and dependencies are not affected.
The syntax of the ALTER VIEW statement corresponds with that of CREATE VIEW.

Be careful when you change the number of columns in a view. Existing application
code and PSQL modules that access the view may become invalid. For information
on how to detect this kind of problem in stored procedures and trigger, see The
RDB$VALID_BLR Field in the Appendix.
Who Can Alter a View?
The ALTER VIEW statement can be executed by:
• Administrators
• The owner of the view
• Users with the ALTER ANY VIEW privilege
Example using ALTER VIEW
Altering the view PRICE_WITH_MARKUP
ALTER VIEW PRICE_WITH_MARKUP (
  CODE_PRICE,
  COST,
Chapter 5. Data Definition (DDL) Statements
168

<!-- Original PDF Page: 170 -->

COST_WITH_MARKUP
) AS
SELECT
  CODE_PRICE,
  COST,
  COST * 1.15
FROM PRICE;
See also
CREATE VIEW, CREATE OR ALTER VIEW, RECREATE VIEW
5.6.3. CREATE OR ALTER VIEW
Creates a view if it doesn’t exist, or alters a view
Available in
DSQL
Syntax
CREATE OR ALTER VIEW viewname [<full_column_list>]
  AS <select_statement>
  [WITH CHECK OPTION]
<full_column_list> ::= (colname [, colname ...])
Table 44. CREATE OR ALTER VIEW Statement Parameters
Parameter Description
viewname Name of a view which may or may not exist
select_statement SELECT statement
full_column_list The list of columns in the view
colname View column name. Duplicate column names are not allowed.
Use the CREATE OR ALTER VIEW statement for changing the definition of an existing view or creating it
if it does not exist. Privileges for an existing view remain intact and dependencies are not affected.
The syntax of the CREATE OR ALTER VIEW statement corresponds with that of CREATE VIEW.
Example of CREATE OR ALTER VIEW
Creating the new view PRICE_WITH_MARKUP view or altering it if it already exists
CREATE OR ALTER VIEW PRICE_WITH_MARKUP (
  CODE_PRICE,
  COST,
  COST_WITH_MARKUP
) AS
Chapter 5. Data Definition (DDL) Statements
169

<!-- Original PDF Page: 171 -->

SELECT
  CODE_PRICE,
  COST,
  COST * 1.15
FROM PRICE;
See also
CREATE VIEW, ALTER VIEW, RECREATE VIEW
5.6.4. DROP VIEW
Drops a view
Available in
DSQL
Syntax
DROP VIEW viewname
Table 45. DROP VIEW Statement Parameter
Parameter Description
viewname View name
The DROP VIEW statement drops (deletes) an existing view. The statement will fail if the view has
dependencies.
Who Can Drop a View?
The DROP VIEW statement can be executed by:
• Administrators
• The owner of the view
• Users with the DROP ANY VIEW privilege
Example
Deleting the PRICE_WITH_MARKUP view
DROP VIEW PRICE_WITH_MARKUP;
See also
CREATE VIEW, RECREATE VIEW, CREATE OR ALTER VIEW
Chapter 5. Data Definition (DDL) Statements
170

<!-- Original PDF Page: 172 -->

5.6.5. RECREATE VIEW
Drops a view if it exists, and creates a view
Available in
DSQL
Syntax
RECREATE VIEW viewname [<full_column_list>]
  AS <select_statement>
  [WITH CHECK OPTION]
<full_column_list> ::= (colname [, colname ...])
Table 46. RECREATE VIEW Statement Parameters
Parameter Description
viewname View name. The maximum length is 63 characters
select_statement SELECT statement
full_column_list The list of columns in the view
colname View column name. Duplicate column names are not allowed.
Creates or recreates a view. If there is a view with this name already, the engine will try to drop it
before creating the new instance. If the existing view cannot be dropped, because of dependencies
or insufficient rights, for example, RECREATE VIEW fails with an error.
Example of RECREATE VIEW
Creating the new view PRICE_WITH_MARKUP view or recreating it, if it already exists
RECREATE VIEW PRICE_WITH_MARKUP (
  CODE_PRICE,
  COST,
  COST_WITH_MARKUP
) AS
SELECT
  CODE_PRICE,
  COST,
  COST * 1.15
FROM PRICE;
See also
CREATE VIEW, DROP VIEW, CREATE OR ALTER VIEW
Chapter 5. Data Definition (DDL) Statements
171

<!-- Original PDF Page: 173 -->

5.7. TRIGGER
A trigger is a special type of stored procedure that is not called directly, instead it is executed when
a specified event occurs. A DML trigger is specific to a single relation (table or view) and one phase
in the timing of the event ( BEFORE or AFTER). A DML trigger can be specified to execute for one
specific event (insert, update, delete) or for a combination of those events.
Two other forms of trigger exist:
1. a “database trigger” can be specified to fire at the start or end of a user session (connection) or a
user transaction.
2. a “DDL trigger” can be specified to fire before or after execution of one or more types of DDL
statements.
5.7.1. CREATE TRIGGER
Creates a trigger
Available in
DSQL, ESQL
Syntax
CREATE TRIGGER trigname
  { <relation_trigger_legacy>
  | <relation_trigger_sql>
  | <database_trigger>
  | <ddl_trigger> }
  {<psql_trigger> | <external-module-body>}
<relation_trigger_legacy> ::=
  FOR {tablename | viewname}
  [ACTIVE | INACTIVE]
  {BEFORE | AFTER} <mutation_list>
  [POSITION number]
<relation_trigger_sql> ::=
  [ACTIVE | INACTIVE]
  {BEFORE | AFTER} <mutation_list>
  ON {tablename | viewname}
  [POSITION number]
<database_trigger> ::=
  [ACTIVE | INACTIVE] ON <db_event>
  [POSITION number]
<ddl_trigger> ::=
  [ACTIVE | INACTIVE]
  {BEFORE | AFTER} <ddl_event>
  [POSITION number]
Chapter 5. Data Definition (DDL) Statements
172

<!-- Original PDF Page: 174 -->

<mutation_list> ::=
  <mutation> [OR <mutation> [OR <mutation>]]
<mutation> ::= INSERT | UPDATE | DELETE
<db_event> ::=
    CONNECT | DISCONNECT
  | TRANSACTION {START | COMMIT | ROLLBACK}
<ddl_event> ::=
    ANY DDL STATEMENT
  | <ddl_event_item> [{OR <ddl_event_item>} ...]
<ddl_event_item> ::=
    {CREATE | ALTER | DROP} TABLE
  | {CREATE | ALTER | DROP} PROCEDURE
  | {CREATE | ALTER | DROP} FUNCTION
  | {CREATE | ALTER | DROP} TRIGGER
  | {CREATE | ALTER | DROP} EXCEPTION
  | {CREATE | ALTER | DROP} VIEW
  | {CREATE | ALTER | DROP} DOMAIN
  | {CREATE | ALTER | DROP} ROLE
  | {CREATE | ALTER | DROP} SEQUENCE
  | {CREATE | ALTER | DROP} USER
  | {CREATE | ALTER | DROP} INDEX
  | {CREATE | DROP} COLLATION
  | ALTER CHARACTER SET
  | {CREATE | ALTER | DROP} PACKAGE
  | {CREATE | DROP} PACKAGE BODY
  | {CREATE | ALTER | DROP} MAPPING
<psql_trigger> ::=
  [SQL SECURITY {INVOKER | DEFINER}]
  <psql-module-body>
<psql-module-body> ::=
  !! See Syntax of Module Body !!
<external-module-body> ::=
  !! See Syntax of Module Body !!
Table 47. CREATE TRIGGER Statement Parameters
Parameter Description
trigname Trigger name. The maximum length is 63 characters. It must be unique
among all trigger names in the database.
relation_trigger_legacy Legacy style of trigger declaration for a relation trigger
relation_trigger_sql Relation trigger declaration compliant with the SQL standard
Chapter 5. Data Definition (DDL) Statements
173

<!-- Original PDF Page: 175 -->

Parameter Description
database_trigger Database trigger declaration
tablename Name of the table with which the relation trigger is associated
viewname Name of the view with which the relation trigger is associated
mutation_list List of relation (table | view) events
number Position of the trigger in the firing order. From 0 to 32,767
db_event Connection or transaction event
ddl_event List of metadata change events
ddl_event_item One of the metadata change events
The CREATE TRIGGER statement is used for creating a new trigger. A trigger can be created either for a
relation (table | view) event (or a combination of relation events), for a database event, or for a DDL
event.
CREATE TRIGGER , along with its associates ALTER TRIGGER , CREATE OR ALTER TRIGGER  and RECREATE
TRIGGER, is a compound statement, consisting of a header and a body. The header specifies the name
of the trigger, the name of the relation (for a DML trigger), the phase of the trigger, the event(s) it
applies to, and the position to determine an order between triggers.
The trigger body consists of optional declarations of local variables and named cursors followed by
one or more statements, or blocks of statements, all enclosed in an outer block that begins with the
keyword BEGIN and ends with the keyword END. Declarations and embedded statements are
terminated with semicolons (‘;’).
The name of the trigger must be unique among all trigger names.
Statement Terminators
Some SQL statement editors — specifically the isql utility that comes with Firebird, and possibly
some third-party editors — employ an internal convention that requires all statements to be
terminated with a semicolon. This creates a conflict with PSQL syntax when coding in these
environments. If you are unacquainted with this problem and its solution, please study the details
in the PSQL chapter in the section entitled Switching the Terminator in isql.
SQL Security
The SQL SECURITY clause specifies the security context for executing other routines or inserting into
other tables.
By default, a trigger applies the SQL Security property defined on its table (or — if the table doesn’t
have the SQL Security property set — the database default), but it can be overridden by specifying it
explicitly.

If the SQL Security property is changed for the table, triggers that do not have an
explicit SQL Security property will not see the effect of the change until the next
time the trigger is loaded into the metadata cache.
Chapter 5. Data Definition (DDL) Statements
174

<!-- Original PDF Page: 176 -->

See also SQL Security in chapter Security.
The Trigger Body
The trigger body is either a PSQL body, or an external UDR module body.
See The Module Body in the PSQL chapter for details.
DML Triggers (on Tables or Views)
DML — or “relation” — triggers are executed at the row (record) level, every time a row is changed.
A trigger can be either ACTIVE or INACTIVE. Only active triggers are executed. Triggers are created
ACTIVE by default.
Who Can Create a DML Trigger?
DML triggers can be created by:
• Administrators
• The owner of the table (or view)
• Users with — for a table — the ALTER ANY TABLE, or — for a view — ALTER ANY VIEW privilege
Forms of Declaration
Firebird supports two forms of declaration for relation triggers:
• The legacy syntax
• The SQL standard-compliant form (recommended)
A relation trigger specifies — among other things — a phase and one or more events.
Phase
Phase concerns the timing of the trigger with regard to the change-of-state event in the row of data:
• A BEFORE trigger is fired before the specified database operation (insert, update or delete) is
carried out
• An AFTER trigger is fired after the database operation has been completed
Row Events
A relation trigger definition specifies at least one of the DML operations INSERT, UPDATE and DELETE,
to indicate one or more events on which the trigger should fire. If multiple operations are specified,
they must be separated by the keyword OR. No operation may occur more than once.
Within the statement block, the Boolean context variables INSERTING, UPDATING and DELETING can be
used to test which operation is currently executing.
Chapter 5. Data Definition (DDL) Statements
175

<!-- Original PDF Page: 177 -->

Firing Order of Triggers
The keyword POSITION allows an optional execution order (“firing order”) to be specified for a series
of triggers that have the same phase and event as their target. The default position is 0. If no
positions are specified, or if several triggers have a single position number, the triggers will be
executed in the alphabetical order of their names.
Examples of CREATE TRIGGER for Tables and Views
1. Creating a trigger in the “legacy” form, firing before the event of inserting a new record into the
CUSTOMER table occurs.
CREATE TRIGGER SET_CUST_NO FOR CUSTOMER
ACTIVE BEFORE INSERT POSITION 0
AS
BEGIN
  IF (NEW.CUST_NO IS NULL) THEN
    NEW.CUST_NO = GEN_ID(CUST_NO_GEN, 1);
END
2. Creating a trigger firing before the event of inserting a new record into the CUSTOMER table in the
SQL standard-compliant form.
CREATE TRIGGER set_cust_no
ACTIVE BEFORE INSERT ON customer POSITION 0
AS
BEGIN
  IF (NEW.cust_no IS NULL) THEN
    NEW.cust_no = GEN_ID(cust_no_gen, 1);
END
3. Creating a trigger that will file after either inserting, updating or deleting a record in the
CUSTOMER table.
CREATE TRIGGER TR_CUST_LOG
ACTIVE AFTER INSERT OR UPDATE OR DELETE
ON CUSTOMER POSITION 10
AS
BEGIN
  INSERT INTO CHANGE_LOG (LOG_ID,
                          ID_TABLE,
                          TABLE_NAME,
                          MUTATION)
  VALUES (NEXT VALUE FOR SEQ_CHANGE_LOG,
          OLD.CUST_NO,
          'CUSTOMER',
          CASE
            WHEN INSERTING THEN 'INSERT'
Chapter 5. Data Definition (DDL) Statements
176

<!-- Original PDF Page: 178 -->

WHEN UPDATING  THEN 'UPDATE'
            WHEN DELETING  THEN 'DELETE'
          END);
END
4. With DEFINER set for trigger tr_ins, user US needs only the INSERT privilege on tr. If it were set for
INVOKER, either the user or the trigger would also need the INSERT privilege on table t.
create table tr (i integer);
create table t (i integer);
set term ^;
create trigger tr_ins for tr after insert SQL SECURITY DEFINER
as
begin
  insert into t values (NEW.i);
end^
set term ;^
grant insert on table tr to user us;
commit;
connect 'localhost:/tmp/29.fdb' user us password 'pas';
insert into tr values(2);
The result would be the same if SQL SECURITY DEFINER were specified for table TR:
create table tr (i integer) SQL SECURITY DEFINER;
create table t (i integer);
set term ^;
create trigger tr_ins for tr after insert
as
begin
  insert into t values (NEW.i);
end^
set term ;^
grant insert on table tr to user us;
commit;
connect 'localhost:/tmp/29.fdb' user us password 'pas';
insert into tr values(2);
Database Triggers
Triggers can be defined to fire upon “database events”; a mixture of events that act across the scope
of a session (connection), and events that act across the scope of an individual transaction:
• CONNECT
Chapter 5. Data Definition (DDL) Statements
177

<!-- Original PDF Page: 179 -->

• DISCONNECT
• TRANSACTION START
• TRANSACTION COMMIT
• TRANSACTION ROLLBACK
DDL Triggers are a subtype of database triggers, covered in a separate section.
Who Can Create a Database Trigger?
Database triggers can be created by:
• Administrators
• Users with the ALTER DATABASE privilege
Execution of Database Triggers and Exception Handling
CONNECT and DISCONNECT triggers are executed in a transaction created specifically for this purpose.
This transaction uses the default isolation level, i.e. snapshot (concurrency), write and wait. If all
goes well, the transaction is committed. Uncaught exceptions cause the transaction to roll back, and
• for a CONNECT trigger, the connection is then broken and the exception is returned to the client
• for a DISCONNECT trigger, exceptions are not reported. The connection is broken as intended
TRANSACTION triggers are executed within the transaction whose start, commit or rollback evokes
them. The action taken after an uncaught exception depends on the event:
• In a TRANSACTION START trigger, the exception is reported to the client and the transaction is
rolled back
• In a TRANSACTION COMMIT trigger, the exception is reported, the trigger’s actions so far are undone
and the commit is cancelled
• In a TRANSACTION ROLLBACK trigger, the exception is not reported and the transaction is rolled
back as intended.
Traps
There is no direct way of knowing if a DISCONNECT or TRANSACTION ROLLBACK  trigger caused an
exception. It also follows that the connection to the database cannot happen if a CONNECT trigger
causes an exception and a transaction cannot start if a TRANSACTION START trigger causes one, either.
Both phenomena effectively lock you out of your database until you get in there with database
triggers suppressed and fix the bad code.
Suppressing Database Triggers
Some Firebird command-line tools have been supplied with switches that an administrator can use
to suppress the automatic firing of database triggers. So far, they are:
gbak -nodbtriggers
Chapter 5. Data Definition (DDL) Statements
178

<!-- Original PDF Page: 180 -->

isql -nodbtriggers
nbackup -T
Two-phase Commit
In a two-phase commit scenario, TRANSACTION COMMIT triggers fire in the prepare phase, not at the
commit.
Some Caveats
1. The use of the IN AUTONOMOUS TRANSACTION DO statement in the database event triggers related to
transactions ( TRANSACTION START , TRANSACTION ROLLBACK , TRANSACTION COMMIT ) may cause the
autonomous transaction to enter an infinite loop
2. The DISCONNECT and TRANSACTION ROLLBACK event triggers will not be executed when clients are
disconnected via monitoring tables (DELETE FROM MON$ATTACHMENTS)
Examples of CREATE TRIGGER for “Database Triggers”
1. Creating a trigger for the event of connecting to the database that logs users logging into the
system. The trigger is created as inactive.
CREATE TRIGGER tr_log_connect
INACTIVE ON CONNECT POSITION 0
AS
BEGIN
  INSERT INTO LOG_CONNECT (ID,
                           USERNAME,
                           ATIME)
  VALUES (NEXT VALUE FOR SEQ_LOG_CONNECT,
          CURRENT_USER,
          CURRENT_TIMESTAMP);
END
2. Creating a trigger for the event of connecting to the database that does not permit any users,
except for SYSDBA, to log in during off hours.
CREATE EXCEPTION E_INCORRECT_WORKTIME 'The working day has not started yet.';
CREATE TRIGGER TR_LIMIT_WORKTIME ACTIVE
ON CONNECT POSITION 1
AS
BEGIN
  IF ((CURRENT_USER <> 'SYSDBA') AND
      NOT (CURRENT_TIME BETWEEN time '9:00' AND time '17:00')) THEN
    EXCEPTION E_INCORRECT_WORKTIME;
END
Chapter 5. Data Definition (DDL) Statements
179

<!-- Original PDF Page: 181 -->

DDL Triggers
DDL triggers allow restrictions to be placed on users who attempt to create, alter or drop a DDL
object. Their other purposes is to keep a metadata change log.
DDL triggers fire on specified metadata changes events in a specified phase. BEFORE triggers run
before changes to system tables. AFTER triggers run after changes in system tables.
 The event type [BEFORE | AFTER] of a DDL trigger cannot be changed.
In a sense, DDL triggers are a sub-type of database triggers.
Who Can Create a DDL Trigger?
DDL triggers can be created by:
• Administrators
• Users with the ALTER DATABASE privilege
Suppressing DDL Triggers
A DDL trigger is a type of database trigger. See Suppressing Database Triggers  how to suppress
DDL — and database — triggers.
Examples of DDL Triggers
1. Here is how you might use a DDL trigger to enforce a consistent naming scheme, in this case,
stored procedure names should begin with the prefix “SP_”:
set auto on;
create exception e_invalid_sp_name 'Invalid SP name (should start with SP_)';
set term !;
create trigger trig_ddl_sp before CREATE PROCEDURE
as
begin
  if (rdb$get_context('DDL_TRIGGER', 'OBJECT_NAME') not starting 'SP_') then
    exception e_invalid_sp_name;
end!
Test
create procedure sp_test
as
begin
end!
create procedure test
Chapter 5. Data Definition (DDL) Statements
180

<!-- Original PDF Page: 182 -->

as
begin
end!
-- The last command raises this exception and procedure TEST is not created
-- Statement failed, SQLSTATE = 42000
-- exception 1
-- -E_INVALID_SP_NAME
-- -Invalid SP name (should start with SP_)
-- -At trigger 'TRIG_DDL_SP' line: 4, col: 5
set term ;!
2. Implement custom DDL security, in this case restricting the running of DDL commands to
certain users:
create exception e_access_denied 'Access denied';
set term !;
create trigger trig_ddl before any ddl statement
as
begin
  if (current_user <> 'SUPER_USER') then
    exception e_access_denied;
end!
Test
create procedure sp_test
as
begin
end!
-- The last command raises this exception and procedure SP_TEST is not created
-- Statement failed, SQLSTATE = 42000
-- exception 1
-- -E_ACCESS_DENIED
-- -Access denied
-- -At trigger 'TRIG_DDL' line: 4, col: 5
set term ;!

Firebird has privileges for executing DDL statements, so writing a DDL trigger
for this should be a last resort, if the same effect cannot be achieved using
privileges.
Chapter 5. Data Definition (DDL) Statements
181

<!-- Original PDF Page: 183 -->

3. Use a trigger to log DDL actions and attempts:
create sequence ddl_seq;
create table ddl_log (
  id bigint not null primary key,
  moment timestamp not null,
  user_name varchar(63) not null,
  event_type varchar(25) not null,
  object_type varchar(25) not null,
  ddl_event varchar(25) not null,
  object_name varchar(63) not null,
  sql_text blob sub_type text not null,
  ok char(1) not null
);
set term !;
create trigger trig_ddl_log_before before any ddl statement
as
  declare id type of column ddl_log.id;
begin
  -- We do the changes in an AUTONOMOUS TRANSACTION, so if an exception happens
  -- and the command didn't run, the log will survive.
  in autonomous transaction do
  begin
    insert into ddl_log (id, moment, user_name, event_type, object_type,
                         ddl_event, object_name, sql_text, ok)
      values (next value for ddl_seq, current_timestamp, current_user,
              rdb$get_context('DDL_TRIGGER', 'EVENT_TYPE'),
              rdb$get_context('DDL_TRIGGER', 'OBJECT_TYPE'),
              rdb$get_context('DDL_TRIGGER', 'DDL_EVENT'),
              rdb$get_context('DDL_TRIGGER', 'OBJECT_NAME'),
              rdb$get_context('DDL_TRIGGER', 'SQL_TEXT'),
              'N')
      returning id into id;
    rdb$set_context('USER_SESSION', 'trig_ddl_log_id', id);
  end
end!
The above trigger will fire for this DDL command. It’s a good idea to use -nodbtriggers when
working with them!
create trigger trig_ddl_log_after after any ddl statement
as
begin
  -- Here we need an AUTONOMOUS TRANSACTION because the original transaction
  -- will not see the record inserted on the BEFORE trigger autonomous
  -- transaction if user transaction is not READ COMMITTED.
Chapter 5. Data Definition (DDL) Statements
182

<!-- Original PDF Page: 184 -->

in autonomous transaction do
     update ddl_log set ok = 'Y'
     where id = rdb$get_context('USER_SESSION', 'trig_ddl_log_id');
end!
commit!
set term ;!
-- Delete the record about trig_ddl_log_after creation.
delete from ddl_log;
commit;
Test
-- This will be logged one time
-- (as T1 did not exist, RECREATE acts as CREATE) with OK = Y.
recreate table t1 (
  n1 integer,
  n2 integer
);
-- This will fail as T1 already exists, so OK will be N.
create table t1 (
  n1 integer,
  n2 integer
);
-- T2 does not exist. There will be no log.
drop table t2;
-- This will be logged twice
-- (as T1 exists, RECREATE acts as DROP and CREATE) with OK = Y.
recreate table t1 (
  n integer
);
commit;
select id, ddl_event, object_name, sql_text, ok
  from ddl_log order by id;
 ID DDL_EVENT                 OBJECT_NAME                      SQL_TEXT OK
=== ========================= ======================= ================= ======
  2 CREATE TABLE              T1                                   80:3 Y
====================================================
SQL_TEXT:
recreate table t1 (
Chapter 5. Data Definition (DDL) Statements
183

<!-- Original PDF Page: 185 -->

n1 integer,
    n2 integer
)
====================================================
  3 CREATE TABLE              T1                                   80:2 N
====================================================
SQL_TEXT:
create table t1 (
    n1 integer,
    n2 integer
)
====================================================
  4 DROP TABLE                T1                                   80:6 Y
====================================================
SQL_TEXT:
recreate table t1 (
    n integer
)
====================================================
  5 CREATE TABLE              T1                                   80:9 Y
====================================================
SQL_TEXT:
recreate table t1 (
    n integer
)
====================================================
See also
ALTER TRIGGER, CREATE OR ALTER TRIGGER , RECREATE TRIGGER, DROP TRIGGER, DDL Triggers  in Chapter
Procedural SQL (PSQL) Statements
5.7.2. ALTER TRIGGER
Alters a trigger
Available in
DSQL, ESQL
Syntax
ALTER TRIGGER trigname
  [ACTIVE | INACTIVE]
  [{BEFORE | AFTER} <mutation_list>]
  [POSITION number]
  {<psql_trigger> | <external-module-body>}
<psql_trigger> ::=
  [<sql_security>]
  [<psql-module-body>]
Chapter 5. Data Definition (DDL) Statements
184

<!-- Original PDF Page: 186 -->

<sql_security> ::=
    SQL SECURITY {INVOKER | DEFINER}
  | DROP SQL SECURITY
!! See syntax of CREATE TRIGGER for further rules !!
The ALTER TRIGGER statement only allows certain changes to the header and body of a trigger.
Permitted Changes to Triggers
• Status (ACTIVE | INACTIVE)
• Phase (BEFORE | AFTER) (of DML triggers)
• Events (of DML triggers)
• Position in the firing order
• Modifications to code in the trigger body
If an element is not specified, it remains unchanged.
 A DML trigger cannot be changed to a database or DDL trigger.
It is not possible to change the event(s) or phase of a database or DDL trigger.

Reminders
The BEFORE keyword directs that the trigger be executed before the associated
event occurs; the AFTER keyword directs that it be executed after the event.
More than one DML event —  INSERT, UPDATE, DELETE — can be covered in a single
trigger. The events should be separated with the keyword OR. No event should be
mentioned more than once.
The keyword POSITION allows an optional execution order (“firing order”) to be
specified for a series of triggers that have the same phase and event as their target.
The default position is 0. If no positions are specified, or if several triggers have a
single position number, the triggers will be executed in the alphabetical order of
their names.
Who Can Alter a Trigger?
DML triggers can be altered by:
• Administrators
• The owner of the table (or view)
• Users with — for a table — the ALTER ANY TABLE, or — for a view — ALTER ANY VIEW privilege
Database and DDL triggers can be altered by:
• Administrators
Chapter 5. Data Definition (DDL) Statements
185

<!-- Original PDF Page: 187 -->

• Users with the ALTER DATABASE privilege
Examples using ALTER TRIGGER
1. Deactivating the set_cust_no trigger (switching it to the inactive status).
ALTER TRIGGER set_cust_no INACTIVE;
2. Changing the firing order position of the set_cust_no trigger.
ALTER TRIGGER set_cust_no POSITION 14;
3. Switching the TR_CUST_LOG trigger to the inactive status and modifying the list of events.
ALTER TRIGGER TR_CUST_LOG
INACTIVE AFTER INSERT OR UPDATE;
4. Switching the tr_log_connect trigger to the active status, changing its position and body.
ALTER TRIGGER tr_log_connect
ACTIVE POSITION 1
AS
BEGIN
  INSERT INTO LOG_CONNECT (ID,
                           USERNAME,
                           ROLENAME,
                           ATIME)
  VALUES (NEXT VALUE FOR SEQ_LOG_CONNECT,
          CURRENT_USER,
          CURRENT_ROLE,
          CURRENT_TIMESTAMP);
END
See also
CREATE TRIGGER, CREATE OR ALTER TRIGGER, RECREATE TRIGGER, DROP TRIGGER
5.7.3. CREATE OR ALTER TRIGGER
Creates a trigger if it doesn’t exist, or alters a trigger
Available in
DSQL
Syntax
CREATE OR ALTER TRIGGER trigname
Chapter 5. Data Definition (DDL) Statements
186

<!-- Original PDF Page: 188 -->

{ <relation_trigger_legacy>
  | <relation_trigger_sql>
  | <database_trigger>
  | <ddl_trigger> }
  {<psql_trigger> | <external-module-body>}
!! See syntax of CREATE TRIGGER for further rules !!
The CREATE OR ALTER TRIGGER statement creates a new trigger if it does not exist; otherwise it alters
and recompiles it with the privileges intact and dependencies unaffected.
Example of CREATE OR ALTER TRIGGER
Creating a new trigger if it does not exist or altering it if it does exist
CREATE OR ALTER TRIGGER set_cust_no
ACTIVE BEFORE INSERT ON customer POSITION 0
AS
BEGIN
  IF (NEW.cust_no IS NULL) THEN
    NEW.cust_no = GEN_ID(cust_no_gen, 1);
END
See also
CREATE TRIGGER, ALTER TRIGGER, RECREATE TRIGGER
5.7.4. DROP TRIGGER
Drops a trigger
Available in
DSQL, ESQL
Syntax
DROP TRIGGER trigname
Table 48. DROP TRIGGER Statement Parameter
Parameter Description
trigname Trigger name
The DROP TRIGGER statement drops (deletes) an existing trigger.
Who Can Drop a Trigger?
DML triggers can be dropped by:
• Administrators
Chapter 5. Data Definition (DDL) Statements
187

<!-- Original PDF Page: 189 -->

• The owner of the table (or view)
• Users with — for a table — the ALTER ANY TABLE, or — for a view — ALTER ANY VIEW privilege
Database and DDL triggers can be dropped by:
• Administrators
• Users with the ALTER DATABASE privilege
Example of DROP TRIGGER
Deleting the set_cust_no trigger
DROP TRIGGER set_cust_no;
See also
CREATE TRIGGER, RECREATE TRIGGER
5.7.5. RECREATE TRIGGER
Drops a trigger if it exists, and creates a trigger
Available in
DSQL
Syntax
RECREATE TRIGGER trigname
  { <relation_trigger_legacy>
  | <relation_trigger_sql>
  | <database_trigger>
  | <ddl_trigger> }
  {<psql_trigger> | <external-module-body>}
!! See syntax of CREATE TRIGGER for further rules !!
The RECREATE TRIGGER statement creates a new trigger if no trigger with the specified name exists;
otherwise the RECREATE TRIGGER statement tries to drop the existing trigger and create a new one.
The operation will fail on COMMIT if the trigger is in use.
 Be aware that dependency errors are not detected until the COMMIT phase of this
operation.
Example of RECREATE TRIGGER
Creating or recreating the set_cust_no trigger.
RECREATE TRIGGER set_cust_no
Chapter 5. Data Definition (DDL) Statements
188

<!-- Original PDF Page: 190 -->

ACTIVE BEFORE INSERT ON customer POSITION 0
AS
BEGIN
  IF (NEW.cust_no IS NULL) THEN
    NEW.cust_no = GEN_ID(cust_no_gen, 1);
END
See also
CREATE TRIGGER, DROP TRIGGER, CREATE OR ALTER TRIGGER
5.8. PROCEDURE
A stored procedure is a software module that can be called from a client, another procedure,
function, executable block or trigger. Stored procedures are written in procedural SQL (PSQL) or
defined using a UDR (User-Defined Routine). Most SQL statements are available in PSQL as well,
sometimes with limitations or extensions. Notable limitations are the prohibition on DDL and
transaction control statements in PSQL.
Stored procedures can have many input and output parameters.
5.8.1. CREATE PROCEDURE
Creates a stored procedure
Available in
DSQL, ESQL
Syntax
CREATE PROCEDURE procname [ ( [ <in_params> ] ) ]
  [RETURNS (<out_params>)]
  {<psql_procedure> | <external-module-body>}
<in_params> ::= <inparam> [, <inparam> ...]
<inparam> ::= <param_decl> [{= | DEFAULT} <value>]
<out_params> ::= <outparam> [, <outparam> ...]
<outparam> ::= <param_decl>
<value> ::= {<literal> | NULL | <context_var>}
<param_decl> ::= paramname <domain_or_non_array_type> [NOT NULL]
  [COLLATE collation]
<type> ::=
    <datatype>
  | [TYPE OF] domain
Chapter 5. Data Definition (DDL) Statements
189

<!-- Original PDF Page: 191 -->

| TYPE OF COLUMN rel.col
<domain_or_non_array_type> ::=
  !! See Scalar Data Types Syntax !!
<psql_procedure> ::=
  [SQL SECURITY {INVOKER | DEFINER}]
  <psql-module-body>
<psql-module-body> ::=
  !! See Syntax of Module Body !!
<external-module-body> ::=
  !! See Syntax of Module Body !!
Table 49. CREATE PROCEDURE Statement Parameters
Parameter Description
procname Stored procedure name. The maximum length is 63 characters. Must be
unique among all table, view and procedure names in the database
inparam Input parameter description
outparam Output parameter description
literal A literal value that is assignment-compatible with the data type of the
parameter
context_var Any context variable whose type is compatible with the data type of the
parameter
paramname The name of an input or output parameter of the procedure. The
maximum length is 63 characters. The name of the parameter must be
unique among input and output parameters of the procedure and its local
variables
collation Collation
The CREATE PROCEDURE statement creates a new stored procedure. The name of the procedure must
be unique among the names of all stored procedures, tables, and views in the database.
CREATE PROCEDURE is a compound statement, consisting of a header and a body. The header specifies
the name of the procedure and declares input parameters and the output parameters, if any, that
are to be returned by the procedure.
The procedure body consists of declarations for any local variables, named cursors, and
subroutines that will be used by the procedure, followed by one or more statements, or blocks of
statements, all enclosed in an outer block that begins with the keyword BEGIN and ends with the
keyword END. Declarations and embedded statements are terminated with semicolons (‘;’).
Statement Terminators
Some SQL statement editors — specifically the isql utility that comes with Firebird, and possibly
Chapter 5. Data Definition (DDL) Statements
190

<!-- Original PDF Page: 192 -->

some third-party editors — employ an internal convention that requires all statements to be
terminated with a semicolon. This creates a conflict with PSQL syntax when coding in these
environments. If you are unacquainted with this problem and its solution, please study the details
in the PSQL chapter in the section entitled Switching the Terminator in isql.
Parameters
Each parameter has a data type. The NOT NULL constraint can also be specified for any parameter, to
prevent NULL being passed or assigned to it.
A collation can be specified for string-type parameters, using the COLLATE clause.
Input Parameters
Input parameters are presented as a parenthesized list following the name of the function. They
are passed by value into the procedure, so any changes inside the procedure has no effect on the
parameters in the caller. Input parameters may have default values. Parameters with default
values specified must be added at the end of the list of parameters.
Output Parameters
The optional RETURNS clause is for specifying a parenthesised list of output parameters for the
stored procedure.
SQL Security
The SQL SECURITY clause specifies the security context for executing other routines or inserting into
other tables. When SQL Security is not specified, the default value of the database is applied at
runtime.
The SQL SECURITY clause can only be specified for PSQL procedures, and is not valid for procedures
defined in a package.
See also SQL Security in chapter Security.
Variable, Cursor and Subroutine Declarations
The optional declarations section, located at the start of the body of the procedure definition,
defines variables (including cursors) and subroutines local to the procedure. Local variable
declarations follow the same rules as parameters regarding specification of the data type. See
details in the PSQL chapter  for DECLARE VARIABLE, DECLARE CURSOR, DECLARE FUNCTION, and DECLARE
PROCEDURE.
External UDR Procedures
A stored procedure can also be located in an external module. In this case, instead of a procedure
body, the CREATE PROCEDURE specifies the location of the procedure in the external module using the
EXTERNAL clause. The optional NAME clause specifies the name of the external module, the name of the
procedure inside the module, and — optionally — user-defined information. The required ENGINE
clause specifies the name of the UDR engine that handles communication between Firebird and the
external module. The optional AS clause accepts a string literal “body”, which can be used by the
engine or module for various purposes.
Chapter 5. Data Definition (DDL) Statements
191

<!-- Original PDF Page: 193 -->

Who Can Create a Procedure
The CREATE PROCEDURE statement can be executed by:
• Administrators
• Users with the CREATE PROCEDURE privilege
The user executing the CREATE PROCEDURE statement becomes the owner of the table.
Examples
1. Creating a stored procedure that inserts a record into the BREED table and returns the code of the
inserted record:
CREATE PROCEDURE ADD_BREED (
  NAME D_BREEDNAME, /* Domain attributes are inherited */
  NAME_EN TYPE OF D_BREEDNAME, /* Only the domain type is inherited */
  SHORTNAME TYPE OF COLUMN BREED.SHORTNAME,
    /* The table column type is inherited */
  REMARK VARCHAR(120) CHARACTER SET WIN1251 COLLATE PXW_CYRL,
  CODE_ANIMAL INT NOT NULL DEFAULT 1
)
RETURNS (
  CODE_BREED INT
)
AS
BEGIN
  INSERT INTO BREED (
    CODE_ANIMAL, NAME, NAME_EN, SHORTNAME, REMARK)
  VALUES (
    :CODE_ANIMAL, :NAME, :NAME_EN, :SHORTNAME, :REMARK)
  RETURNING CODE_BREED INTO CODE_BREED;
END
2. Creating a selectable stored procedure that generates data for mailing labels (from
employee.fdb):
CREATE PROCEDURE mail_label (cust_no INTEGER)
RETURNS (line1 CHAR(40), line2 CHAR(40), line3 CHAR(40),
         line4 CHAR(40), line5 CHAR(40), line6 CHAR(40))
AS
  DECLARE VARIABLE customer VARCHAR(25);
  DECLARE VARIABLE first_name VARCHAR(15);
  DECLARE VARIABLE last_name VARCHAR(20);
  DECLARE VARIABLE addr1 VARCHAR(30);
  DECLARE VARIABLE addr2 VARCHAR(30);
  DECLARE VARIABLE city VARCHAR(25);
  DECLARE VARIABLE state VARCHAR(15);
  DECLARE VARIABLE country VARCHAR(15);
Chapter 5. Data Definition (DDL) Statements
192

<!-- Original PDF Page: 194 -->

DECLARE VARIABLE postcode VARCHAR(12);
  DECLARE VARIABLE cnt INTEGER;
BEGIN
  line1 = '';
  line2 = '';
  line3 = '';
  line4 = '';
  line5 = '';
  line6 = '';
  SELECT customer, contact_first, contact_last, address_line1,
    address_line2, city, state_province, country, postal_code
  FROM CUSTOMER
  WHERE cust_no = :cust_no
  INTO :customer, :first_name, :last_name, :addr1, :addr2,
    :city, :state, :country, :postcode;
  IF (customer IS NOT NULL) THEN
    line1 = customer;
  IF (first_name IS NOT NULL) THEN
    line2 = first_name || ' ' || last_name;
  ELSE
    line2 = last_name;
  IF (addr1 IS NOT NULL) THEN
    line3 = addr1;
  IF (addr2 IS NOT NULL) THEN
    line4 = addr2;
  IF (country = 'USA') THEN
  BEGIN
    IF (city IS NOT NULL) THEN
      line5 = city || ', ' || state || '  ' || postcode;
    ELSE
      line5 = state || '  ' || postcode;
  END
  ELSE
  BEGIN
    IF (city IS NOT NULL) THEN
      line5 = city || ', ' || state;
    ELSE
      line5 = state;
    line6 = country || '    ' || postcode;
  END
  SUSPEND; -- the statement that sends an output row to the buffer
           -- and makes the procedure "selectable"
END
3. With DEFINER set for procedure p, user US needs only the EXECUTE privilege on p. If it were set for
INVOKER, either the user or the procedure would also need the INSERT privilege on table t.
Chapter 5. Data Definition (DDL) Statements
193

<!-- Original PDF Page: 195 -->

set term ^;
create procedure p (i integer) SQL SECURITY DEFINER
as
begin
  insert into t values (:i);
end^
set term ;^
grant execute on procedure p to user us;
commit;
connect 'localhost:/tmp/17.fdb' user us password 'pas';
execute procedure p(1);
See also
CREATE OR ALTER PROCEDURE, ALTER PROCEDURE, RECREATE PROCEDURE, DROP PROCEDURE
5.8.2. ALTER PROCEDURE
Alters a stored procedure
Available in
DSQL, ESQL
Syntax
ALTER PROCEDURE procname [ ( [ <in_params> ] ) ]
  [RETURNS (<out_params>)]
  {<psql_procedure> | <external-module-body>}
!! See syntax of CREATE PROCEDURE for further rules !!
The ALTER PROCEDURE statement allows the following changes to a stored procedure definition:
• the set and characteristics of input and output parameters
• local variables
• code in the body of the stored procedure
After ALTER PROCEDURE executes, existing privileges remain intact and dependencies are not affected.
Altering a procedure without specifying the SQL SECURITY clause will remove the SQL Security
property if currently set for this procedure. This means the behaviour will revert to the database
default.

Take care about changing the number and type of input and output parameters in
stored procedures. Existing application code and procedures and triggers that call
it could become invalid because the new description of the parameters is
Chapter 5. Data Definition (DDL) Statements
194

<!-- Original PDF Page: 196 -->

incompatible with the old calling format. For information on how to troubleshoot
such a situation, see the article The RDB$VALID_BLR Field in the Appendix.
Who Can Alter a Procedure
The ALTER PROCEDURE statement can be executed by:
• Administrators
• The owner of the stored procedure
• Users with the ALTER ANY PROCEDURE privilege
ALTER PROCEDURE Example
Altering the GET_EMP_PROJ stored procedure.
ALTER PROCEDURE GET_EMP_PROJ (
  EMP_NO SMALLINT)
RETURNS (
  PROJ_ID VARCHAR(20))
AS
BEGIN
  FOR SELECT
      PROJ_ID
    FROM
      EMPLOYEE_PROJECT
    WHERE
      EMP_NO = :emp_no
    INTO :proj_id
  DO
    SUSPEND;
END
See also
CREATE PROCEDURE, CREATE OR ALTER PROCEDURE, RECREATE PROCEDURE, DROP PROCEDURE
5.8.3. CREATE OR ALTER PROCEDURE
Creates a stored procedure if it does not exist, or alters a stored procedure
Available in
DSQL
Syntax
CREATE OR ALTER PROCEDURE procname [ ( [ <in_params> ] ) ]
  [RETURNS (<out_params>)]
  {<psql_procedure> | <external-module-body>}
Chapter 5. Data Definition (DDL) Statements
195

<!-- Original PDF Page: 197 -->

!! See syntax of CREATE PROCEDURE for further rules !!
The CREATE OR ALTER PROCEDURE statement creates a new stored procedure or alters an existing one.
If the stored procedure does not exist, it will be created by invoking a CREATE PROCEDURE statement
transparently. If the procedure already exists, it will be altered and compiled without affecting its
existing privileges and dependencies.
CREATE OR ALTER PROCEDURE Example
Creating or altering the GET_EMP_PROJ procedure.
CREATE OR ALTER PROCEDURE GET_EMP_PROJ (
    EMP_NO SMALLINT)
RETURNS (
    PROJ_ID VARCHAR(20))
AS
BEGIN
  FOR SELECT
      PROJ_ID
    FROM
      EMPLOYEE_PROJECT
    WHERE
      EMP_NO = :emp_no
    INTO :proj_id
  DO
    SUSPEND;
END
See also
CREATE PROCEDURE, ALTER PROCEDURE, RECREATE PROCEDURE
5.8.4. DROP PROCEDURE
Drops a stored procedure
Available in
DSQL, ESQL
Syntax
DROP PROCEDURE procname
Table 50. DROP PROCEDURE Statement Parameter
Parameter Description
procname Name of an existing stored procedure
The DROP PROCEDURE statement deletes an existing stored procedure. If the stored procedure has any
Chapter 5. Data Definition (DDL) Statements
196

<!-- Original PDF Page: 198 -->

dependencies, the attempt to delete it will fail and raise an error.
Who Can Drop a Procedure
The DROP PROCEDURE statement can be executed by:
• Administrators
• The owner of the stored procedure
• Users with the DROP ANY PROCEDURE privilege
DROP PROCEDURE Example
Deleting the GET_EMP_PROJ stored procedure.
DROP PROCEDURE GET_EMP_PROJ;
See also
CREATE PROCEDURE, RECREATE PROCEDURE
5.8.5. RECREATE PROCEDURE
Drops a stored procedure if it exists, and creates a stored procedure
Available in
DSQL
Syntax
RECREATE PROCEDURE procname [ ( [ <in_params> ] ) ]
  [RETURNS (<out_params>)]
  {<psql_procedure> | <external-module-body>}
!! See syntax of CREATE PROCEDURE for further rules !!
The RECREATE PROCEDURE statement creates a new stored procedure or recreates an existing one. If a
procedure with this name already exists, the engine will try to drop it and create a new one.
Recreating an existing procedure will fail at the COMMIT request if the procedure has dependencies.
 Be aware that dependency errors are not detected until the COMMIT phase of this
operation.
After a procedure is successfully recreated, privileges to execute the stored procedure, and the
privileges of the stored procedure itself are dropped.
RECREATE PROCEDURE Example
Chapter 5. Data Definition (DDL) Statements
197

<!-- Original PDF Page: 199 -->

Creating the new GET_EMP_PROJ stored procedure or recreating the existing GET_EMP_PROJ stored procedure.
RECREATE PROCEDURE GET_EMP_PROJ (
  EMP_NO SMALLINT)
RETURNS (
  PROJ_ID VARCHAR(20))
AS
BEGIN
  FOR SELECT
      PROJ_ID
    FROM
      EMPLOYEE_PROJECT
    WHERE
      EMP_NO = :emp_no
    INTO :proj_id
  DO
    SUSPEND;
END
See also
CREATE PROCEDURE, DROP PROCEDURE, CREATE OR ALTER PROCEDURE
5.9. FUNCTION
A stored function is a user-defined function stored in the metadata of a database, and running on
the server. Stored functions can be called by stored procedures, stored functions (including the
function itself), triggers and DSQL. When a stored function calls itself, such a stored function is
called a recursive function.
Unlike stored procedures, stored functions always return a single scalar value. To return a value
from a stored functions, use the RETURN statement, which immediately ends the function.
See also
EXTERNAL FUNCTION
5.9.1. CREATE FUNCTION
Creates a stored function
Available in
DSQL
Syntax
CREATE FUNCTION funcname [ ( [ <in_params> ] ) ]
  RETURNS <domain_or_non_array_type> [COLLATE collation]
  [DETERMINISTIC]
  {<psql_function> | <external-module-body>}
Chapter 5. Data Definition (DDL) Statements
198

<!-- Original PDF Page: 200 -->

<in_params> ::= <inparam> [, <inparam> ... ]
<inparam> ::= <param-decl> [ { = | DEFAULT } <value> ]
<value> ::= { <literal> | NULL | <context-var> }
<param-decl> ::= paramname <domain_or_non_array_type> [NOT NULL]
  [COLLATE collation]
<domain_or_non_array_type> ::=
  !! See Scalar Data Types Syntax !!
<psql_function> ::=
  [SQL SECURITY {INVOKER | DEFINER}]
  <psql-module-body>
<psql-module-body> ::=
  !! See Syntax of Module Body !!
<external-module-body> ::=
  !! See Syntax of Module Body !!
Table 51. CREATE FUNCTION Statement Parameters
Parameter Description
funcname Stored function name. The maximum length is 63 characters. Must be
unique among all function names in the database.
inparam Input parameter description
collation Collation
literal A literal value that is assignment-compatible with the data type of the
parameter
context-var Any context variable whose type is compatible with the data type of the
parameter
paramname The name of an input parameter of the function. The maximum length is
63 characters. The name of the parameter must be unique among input
parameters of the function and its local variables.
The CREATE FUNCTION statement creates a new stored function. The stored function name must be
unique among the names of all stored and external (legacy) functions, excluding sub-functions or
functions in packages. For sub-functions or functions in packages, the name must be unique within
its module (package, stored procedure, stored function, trigger).

It is advisable to not reuse function names between global stored functions and
stored functions in packages, although this is legal. At the moment, it is not
possible to call a function or procedure from the global namespace from inside a
package, if that package defines a function or procedure with the same name. In
that situation, the function or procedure of the package will be called.
Chapter 5. Data Definition (DDL) Statements
199

<!-- Original PDF Page: 201 -->

CREATE FUNCTION is a compound statement with a header and a body. The header defines the name
of the stored function, and declares input parameters and return type.
The function body consists of optional declarations of local variables, named cursors, and
subroutines (sub-functions and sub-procedures), and one or more statements or statement blocks,
enclosed in an outer block that starts with the keyword BEGIN and ends with the keyword END.
Declarations and statements inside the function body must be terminated with a semicolon (‘;’).
Statement Terminators
Some SQL statement editors — specifically the isql utility that comes with Firebird, and possibly
some third-party editors — employ an internal convention that requires all statements to be
terminated with a semicolon. This creates a conflict with PSQL syntax when coding in these
environments. If you are unacquainted with this problem and its solution, please study the details
in the PSQL chapter in the section entitled Switching the Terminator in isql.
Parameters
Each parameter has a data type.
A collation can be specified for string-type parameters, using the COLLATE clause.
Input Parameters
Input parameters are presented as a parenthesized list following the name of the function. They
are passed by value into the function, so any changes inside the function has no effect on the
parameters in the caller. The NOT NULL constraint can also be specified for any input parameter,
to prevent NULL being passed or assigned to it. Input parameters may have default values.
Parameters with default values specified must be added at the end of the list of parameters.
Output Parameter
The RETURNS clause specifies the return type of the stored function. If a function returns a string
value, then it is possible to specify the collation using the COLLATE clause. As a return type, you
can specify a data type, a domain, the type of a domain (using TYPE OF), or the type of a column
of a table or view (using TYPE OF COLUMN).
Deterministic functions
The optional DETERMINISTIC clause indicates that the function is deterministic. Deterministic
functions always return the same result for the same set of inputs. Non-deterministic functions can
return different results for each invocation, even for the same set of inputs. If a function is specified
as deterministic, then such a function might not be called again if it has already been called once
with the given set of inputs, and instead takes the result from a metadata cache.

Current versions of Firebird do not cache results of deterministic functions.
Specifying the DETERMINISTIC clause is comparable to a “promise” that the function
will return the same thing for equal inputs. At the moment, a deterministic
function is considered an invariant, and works like other invariants. That is, they
are computed and cached at the current execution level of a given statement.
Chapter 5. Data Definition (DDL) Statements
200

<!-- Original PDF Page: 202 -->

This is easily demonstrated with an example:
CREATE FUNCTION FN_T
RETURNS DOUBLE PRECISION DETERMINISTIC
AS
BEGIN
  RETURN rand();
END;
-- the function will be evaluated twice and will return 2 different
values
SELECT fn_t() FROM rdb$database
UNION ALL
SELECT fn_t() FROM rdb$database;
-- the function will be evaluated once and will return 2 identical
values
WITH t (n) AS (
  SELECT 1 FROM rdb$database
  UNION ALL
  SELECT 2 FROM rdb$database
)
SELECT n, fn_t() FROM t;
SQL Security
The SQL SECURITY clause specifies the security context for executing other routines or inserting into
other tables. When SQL Security is not specified, the default value of the database is applied at
runtime.
The SQL SECURITY clause can only be specified for PSQL functions, and is not valid for functions
defined in a package.
See also SQL Security in chapter Security.
Variable, Cursor and Subroutine Declarations
The optional declarations section, located at the start of the body of the function definition, defines
variables (including cursors) and subroutines local to the function. Local variable declarations
follow the same rules as parameters regarding specification of the data type. See details in the PSQL
chapter for DECLARE VARIABLE, DECLARE CURSOR, DECLARE FUNCTION, and DECLARE PROCEDURE.
Function Body
The header section is followed by the function body, consisting of one or more PSQL statements
enclosed between the outer keywords BEGIN and END. Multiple BEGIN … END blocks of terminated
statements may be embedded inside the procedure body.
Chapter 5. Data Definition (DDL) Statements
201

<!-- Original PDF Page: 203 -->

External UDR Functions
A stored function can also be located in an external module. In this case, instead of a function body,
the CREATE FUNCTION specifies the location of the function in the external module using the EXTERNAL
clause. The optional NAME clause specifies the name of the external module, the name of the function
inside the module, and — optionally — user-defined information. The required ENGINE clause
specifies the name of the UDR engine that handles communication between Firebird and the
external module. The optional AS clause accepts a string literal “body”, which can be used by the
engine or module for various purposes.

External UDR (User Defined Routine) functions created using CREATE FUNCTION … 
EXTERNAL … should not be confused with legacy UDFs (User Defined Functions)
declared using DECLARE EXTERNAL FUNCTION.
UDFs are deprecated, and a legacy from previous Firebird functions. Their
capabilities are significantly inferior to the capabilities to the new type of external
UDR functions.
Who Can Create a Function
The CREATE FUNCTION statement can be executed by:
• Administrators
• Users with the CREATE FUNCTION privilege
The user who created the stored function becomes its owner.
CREATE FUNCTION Examples
1. Creating a stored function
CREATE FUNCTION ADD_INT (A INT, B INT DEFAULT 0)
RETURNS INT
AS
BEGIN
  RETURN A + B;
END
Calling in a select:
SELECT ADD_INT(2, 3) AS R FROM RDB$DATABASE
Call inside PSQL code, the second optional parameter is not specified:
MY_VAR = ADD_INT(A);
2. Creating a deterministic stored function
Chapter 5. Data Definition (DDL) Statements
202

<!-- Original PDF Page: 204 -->

CREATE FUNCTION FN_E()
RETURNS DOUBLE PRECISION DETERMINISTIC
AS
BEGIN
  RETURN EXP(1);
END
3. Creating a stored function with table column type parameters
Returns the name of a type by field name and value
CREATE FUNCTION GET_MNEMONIC (
  AFIELD_NAME TYPE OF COLUMN RDB$TYPES.RDB$FIELD_NAME,
  ATYPE TYPE OF COLUMN RDB$TYPES.RDB$TYPE)
RETURNS TYPE OF COLUMN RDB$TYPES.RDB$TYPE_NAME
AS
BEGIN
  RETURN (SELECT RDB$TYPE_NAME
          FROM RDB$TYPES
          WHERE RDB$FIELD_NAME = :AFIELD_NAME
          AND RDB$TYPE = :ATYPE);
END
4. Creating an external stored function
Create a function located in an external module (UDR). Function implementation is located in
the external module udrcpp_example. The name of the function inside the module is wait_event.
CREATE FUNCTION wait_event (
  event_name varchar (31) CHARACTER SET ascii
) RETURNS INTEGER
EXTERNAL NAME 'udrcpp_example!Wait_event'
ENGINE udr
5. Creating a stored function containing a sub-function
Creating a function to convert a number to hexadecimal format.
CREATE FUNCTION INT_TO_HEX (
  ANumber BIGINT ,
  AByte_Per_Number SMALLINT = 8)
RETURNS CHAR (66)
AS
DECLARE VARIABLE xMod SMALLINT ;
DECLARE VARIABLE xResult VARCHAR (64);
DECLARE FUNCTION TO_HEX (ANum SMALLINT ) RETURNS CHAR
  AS
Chapter 5. Data Definition (DDL) Statements
203

<!-- Original PDF Page: 205 -->

BEGIN
    RETURN CASE ANum
      WHEN 0 THEN '0'
      WHEN 1 THEN '1'
      WHEN 2 THEN '2'
      WHEN 3 THEN '3'
      WHEN 4 THEN '4'
      WHEN 5 THEN '5'
      WHEN 6 THEN '6'
      WHEN 7 THEN '7'
      WHEN 8 THEN '8'
      WHEN 9 THEN '9'
      WHEN 10 THEN 'A'
      WHEN 11 THEN 'B'
      WHEN 12 THEN 'C'
      WHEN 13 THEN 'D'
      WHEN 14 THEN 'E'
      WHEN 15 THEN 'F'
      ELSE NULL
    END;
  END
BEGIN
  xMod = MOD (ANumber, 16);
  ANumber = ANumber / 16;
  xResult = TO_HEX (xMod);
  WHILE (ANUMBER> 0) DO
  BEGIN
    xMod = MOD (ANumber, 16);
    ANumber = ANumber / 16;
    xResult = TO_HEX (xMod) || xResult;
  END
  RETURN '0x' || LPAD (xResult, AByte_Per_Number * 2, '0' );
END
6. With DEFINER set for function f, user US needs only the EXECUTE privilege on f. If it were set for
INVOKER, the user would also need the INSERT privilege on table t.
set term ^;
create function f (i integer) returns int SQL SECURITY DEFINER
as
begin
  insert into t values (:i);
  return i + 1;
end^
set term ;^
grant execute on function f to user us;
commit;
connect 'localhost:/tmp/59.fdb' user us password 'pas';
Chapter 5. Data Definition (DDL) Statements
204

<!-- Original PDF Page: 206 -->

select f(3) from rdb$database;
See also
CREATE OR ALTER FUNCTION , ALTER FUNCTION , RECREATE FUNCTION , DROP FUNCTION , DECLARE EXTERNAL
FUNCTION
5.9.2. ALTER FUNCTION
Alters a stored function
Available in
DSQL
Syntax
ALTER FUNCTION funcname
  [ ( [ <in_params> ] ) ]
  RETURNS <domain_or_non_array_type> [COLLATE collation]
  [DETERMINISTIC]
  {<psql_function> | <external-module-body>}
!! See syntax of CREATE FUNCTION for further rules !!
The ALTER FUNCTION statement allows the following changes to a stored function definition:
• the set and characteristics of input and output type
• local variables, named cursors, and subroutines
• code in the body of the stored procedure
For external functions (UDR), you can change the entry point and engine name. For legacy external
functions declared using DECLARE EXTERNAL FUNCTION  — also known as UDFs — it is not possible to
convert to PSQL and vice versa.
After ALTER FUNCTION executes, existing privileges remain intact and dependencies are not affected.
Altering a function without specifying the SQL SECURITY  clause will remove the SQL Security
property if currently set for this function. This means the behaviour will revert to the database
default.

Take care about changing the number and type of input parameters and the
output type of a stored function. Existing application code and procedures,
functions and triggers that call it could become invalid because the new
description of the parameters is incompatible with the old calling format. For
information on how to troubleshoot such a situation, see the article The
RDB$VALID_BLR Field in the Appendix.
Chapter 5. Data Definition (DDL) Statements
205

<!-- Original PDF Page: 207 -->

Who Can Alter a Function
The ALTER FUNCTION statement can be executed by:
• Administrators
• Owner of the stored function
• Users with the ALTER ANY FUNCTION privilege
Examples of ALTER FUNCTION
Altering a stored function
ALTER FUNCTION ADD_INT(A INT, B INT, C INT)
RETURNS INT
AS
BEGIN
  RETURN A + B + C;
END
See also
CREATE FUNCTION, CREATE OR ALTER FUNCTION, RECREATE FUNCTION, DROP FUNCTION
5.9.3. CREATE OR ALTER FUNCTION
Creates a stored function if it does not exist, or alters a stored function
Available in
DSQL
Syntax
CREATE OR ALTER FUNCTION funcname
  [ ( [ <in_params> ] ) ]
  RETURNS <domain_or_non_array_type> [COLLATE collation]
  [DETERMINISTIC]
  {<psql_function> | <external-module-body>}
!! See syntax of CREATE FUNCTION for further rules !!
The CREATE OR ALTER FUNCTION statement creates a new stored function or alters an existing one. If
the stored function does not exist, it will be created by invoking a CREATE FUNCTION  statement
transparently. If the function already exists, it will be altered and compiled (through ALTER
FUNCTION) without affecting its existing privileges and dependencies.
Examples of CREATE OR ALTER FUNCTION
Create a new or alter an existing stored function
CREATE OR ALTER FUNCTION ADD_INT(A INT, B INT DEFAULT 0)
Chapter 5. Data Definition (DDL) Statements
206

<!-- Original PDF Page: 208 -->

RETURNS INT
AS
BEGIN
  RETURN A + B;
END
See also
CREATE FUNCTION, ALTER FUNCTION, DROP FUNCTION
5.9.4. DROP FUNCTION
Drops a stored function
Available in
DSQL
Syntax
DROP FUNCTION funcname
Table 52. DROP FUNCTION Statement Parameters
Parameter Description
funcname Stored function name. The maximum length is 63 characters. Must be
unique among all function names in the database.
The DROP FUNCTION statement deletes an existing stored function. If the stored function has any
dependencies, the attempt to delete it will fail, and raise an error.
Who Can Drop a Function
The DROP FUNCTION statement can be executed by:
• Administrators
• Owner of the stored function
• Users with the DROP ANY FUNCTION privilege
Examples of DROP FUNCTION
DROP FUNCTION ADD_INT;
See also
CREATE FUNCTION, CREATE OR ALTER FUNCTION, RECREATE FUNCTION
Chapter 5. Data Definition (DDL) Statements
207

<!-- Original PDF Page: 209 -->

5.9.5. RECREATE FUNCTION
Drops a stored function if it exists, and creates a stored function
Available in
DSQL
Syntax
RECREATE FUNCTION funcname
  [ ( [ <in_params> ] ) ]
  RETURNS <domain_or_non_array_type> [COLLATE collation]
  [DETERMINISTIC]
  {<psql_function> | <external-module-body>}
!! See syntax of CREATE FUNCTION for further rules !!
The RECREATE FUNCTION statement creates a new stored function or recreates an existing one. If there
is a function with this name already, the engine will try to drop it and then create a new one.
Recreating an existing function will fail at COMMIT if the function has dependencies.
 Be aware that dependency errors are not detected until the COMMIT phase of this
operation.
After a procedure is successfully recreated, existing privileges to execute the stored function and
the privileges of the stored function itself are dropped.
Examples of RECREATE FUNCTION
Creating or recreating a stored function
RECREATE FUNCTION ADD_INT(A INT, B INT DEFAULT 0)
RETURNS INT
AS
BEGIN
  RETURN A + B;
EN
See also
CREATE FUNCTION, DROP FUNCTION
5.10. EXTERNAL FUNCTION

External functions (UDFs) have been aggressively deprecated in Firebird 4.0:
• The default setting for the configuration parameter UdfAccess is None. To use
UDFs now requires an explicit configuration of Restrict path-list
• The UDF libraries ( ib_udf, fbudf) are no longer distributed in the installation
Chapter 5. Data Definition (DDL) Statements
208

<!-- Original PDF Page: 210 -->

kits
• Most of the functions in the libraries previously distributed in the shared
(dynamic) libraries ib_udf and fbudf have already been replaced with built-in
functions. A few remaining UDFs have been replaced with either compatible
routines in a new library of UDRs named udf_compat or converted to stored
functions.
Refer to Deprecation of External Functions (UDFs)  in the Compatibility chapter
of the Firebird 4.0 Release notes for details and instructions about upgrading to
use the safe functions.
• Replacement of UDFs with UDRs or stored functions is strongly recommended
External functions, also known as “User-Defined Functions” (UDFs) are programs written in an
external programming language and stored in dynamically loaded libraries. Once declared in a
database, they become available in dynamic and procedural statements as though they were
implemented in the SQL language.
External functions extend the possibilities for processing data with SQL considerably. To make a
function available to a database, it is declared using the statement DECLARE EXTERNAL FUNCTION.
The library containing a function is loaded when any function included in it is called.

External functions declared as DECLARE EXTERNAL FUNCTION  are a legacy from
previous versions of Firebird. Their capabilities are inferior to the capabilities of
the new type of external functions, UDR (User-Defined Routine). Such functions are
declared as CREATE FUNCTION … EXTERNAL …. See CREATE FUNCTION for details.
 External functions may be contained in more than one library — or “module”, as it
is referred to in the syntax.

UDFs are fundamentally insecure. We recommend avoiding their use whenever
possible, and disabling UDFs in your database configuration ( UdfAccess = None in
firebird.conf; this is the default since Firebird 4). If you do need to call native code
from your database, use a UDR external engine instead.
See also
FUNCTION
5.10.1. DECLARE EXTERNAL FUNCTION
Declares a user-defined function (UDF) in the current database
Available in
DSQL, ESQL
Chapter 5. Data Definition (DDL) Statements
209

<!-- Original PDF Page: 211 -->

Syntax
DECLARE EXTERNAL FUNCTION funcname
  [{ <arg_desc_list> | ( <arg_desc_list> ) }]
  RETURNS { <return_value> | ( <return_value> ) }
  ENTRY_POINT 'entry_point' MODULE_NAME 'library_name'
<arg_desc_list> ::=
  <arg_type_decl> [, <arg_type_decl> ...]
<arg_type_decl> ::=
  <udf_data_type> [BY {DESCRIPTOR | SCALAR_ARRAY} | NULL]
<udf_data_type> ::=
    <scalar_datatype>
  | BLOB
  | CSTRING(length) [ CHARACTER SET charset ]
<scalar_datatype> ::=
  !! See Scalar Data Types Syntax !!
<return_value> ::=
  { <udf_data_type> | PARAMETER param_num }
  [{ BY VALUE | BY DESCRIPTOR [FREE_IT] | FREE_IT }]
Table 53. DECLARE EXTERNAL FUNCTION Statement Parameters
Parameter Description
funcname Function name in the database. The maximum length is 63 characters. It
should be unique among all internal and external function names in the
database and need not be the same name as the name exported from the
UDF library via ENTRY_POINT.
entry_point The exported name of the function
library_name The name of the module (MODULE_NAME) from which the function is
exported. This will be the name of the file, without the “.dll” or “.so” file
extension.
length The maximum length of a null-terminated string, specified in bytes
charset Character set of the CSTRING
param_num The number of the input parameter, numbered from 1 in the list of input
parameters in the declaration, describing the data type that will be
returned by the function
The DECLARE EXTERNAL FUNCTION statement makes a user-defined function available in the database.
UDF declarations must be made in each database  that is going to use them. There is no need to
declare UDFs that will never be used.
The name of the external function must be unique among all function names. It may be different
Chapter 5. Data Definition (DDL) Statements
210

<!-- Original PDF Page: 212 -->

from the exported name of the function, as specified in the ENTRY_POINT argument.
DECLARE EXTERNAL FUNCTION Input Parameters
The input parameters of the function follow the name of the function and are separated with
commas. Each parameter has an SQL data type specified for it. Arrays cannot be used as function
parameters. In addition to the SQL types, the CSTRING type is available for specifying a null-
terminated string with a maximum length of LENGTH bytes. There are several mechanisms for
passing a parameter from the Firebird engine to an external function, each of these mechanisms
will be discussed below.
By default, input parameters are passed by reference . There is no separate clause to explicitly
indicate that parameters are passed by reference.
When passing a NULL value by reference, it is converted to the equivalent of zero, for example, a
number ‘0’ or an empty string (“ ''”). If the keyword NULL is specified after a parameter, then with
passing a NULL values, the null pointer will be passed to the external function.

Declaring a function with the NULL keyword does not guarantee that the function
will correctly handle a NULL input parameter. Any function must be written or
rewritten to correctly handle NULL values. Always use the function declaration as
provided by its developer.
If BY DESCRIPTOR is specified, then the input parameter is passed by descriptor. In this case, the UDF
parameter will receive a pointer to an internal structure known as a descriptor. The descriptor
contains information about the data type, subtype, precision, character set and collation, scale, a
pointer to the data itself and some flags, including the NULL indicator. This declaration only works if
the external function is written using a handle.
 When passing a function parameter by descriptor, the passed value is not cast to
the declared data type.
The BY SCALAR_ARRAY clause is used when passing arrays as input parameters. Unlike other types,
you cannot return an array from a UDF.
Clauses and Keywords
RETURNS clause
(Required) specifies the output parameter returned by the function. A function is scalar, it
returns one value (output parameter). The output parameter can be of any SQL type (except an
array or an array element) or a null-terminated string ( CSTRING). The output parameter can be
passed by reference (the default), by descriptor or by value. If the BY DESCRIPTOR  clause is
specified, the output parameter is passed by descriptor. If the BY VALUE clause is specified, the
output parameter is passed by value.
PARAMETER keyword
specifies that the function returns the value from the parameter under number param_num. It is
necessary if you need to return a value of data type BLOB.
Chapter 5. Data Definition (DDL) Statements
211

<!-- Original PDF Page: 213 -->

FREE_IT keyword
means that the memory allocated for storing the return value will be freed after the function is
executed. It is used only if the memory was allocated dynamically in the UDF. In such a UDF, the
memory must be allocated with the help of the ib_util_malloc function from the ib_util module,
a requirement for compatibility with the functions used in Firebird code and in the code of the
shipped UDF modules, for allocating and freeing memory.
ENTRY_POINT clause
specifies the name of the entry point (the name of the imported function), as exported from the
module.
MODULE_NAME clause
defines the name of the module where the exported function is located. The link to the module
should not be the full path and extension of the file, if that can be avoided. If the module is
located in the default location (in the ../UDF subdirectory of the Firebird server root) or in a
location explicitly configured in firebird.conf, it makes it easier to move the database between
different platforms. The UDFAccess parameter in the firebird.conf file allows access restrictions to
external functions modules to be configured.
Any user connected to the database can declare an external function (UDF).
Who Can Create an External Function
The DECLARE EXTERNAL FUNCTION statement can be executed by:
• Administrators
• Users with the CREATE FUNCTION privilege
The user who created the function becomes its owner.
Examples using DECLARE EXTERNAL FUNCTION
1. Declaring the addDay external function located in the fbudf module. The input and output
parameters are passed by reference.
DECLARE EXTERNAL FUNCTION addDay
  TIMESTAMP, INT
  RETURNS TIMESTAMP
  ENTRY_POINT 'addDay' MODULE_NAME 'fbudf';
2. Declaring the invl external function located in the fbudf module. The input and output
parameters are passed by descriptor.
DECLARE EXTERNAL FUNCTION invl
  INT BY DESCRIPTOR, INT BY DESCRIPTOR
  RETURNS INT BY DESCRIPTOR
  ENTRY_POINT 'idNvl' MODULE_NAME 'fbudf';
Chapter 5. Data Definition (DDL) Statements
212

<!-- Original PDF Page: 214 -->

3. Declaring the isLeapYear external function located in the fbudf module. The input parameter is
passed by reference, while the output parameter is passed by value.
DECLARE EXTERNAL FUNCTION isLeapYear
  TIMESTAMP
  RETURNS INT BY VALUE
  ENTRY_POINT 'isLeapYear' MODULE_NAME 'fbudf';
4. Declaring the i64Truncate external function located in the fbudf module. The input and output
parameters are passed by descriptor. The second parameter of the function is used as the return
value.
DECLARE EXTERNAL FUNCTION i64Truncate
  NUMERIC(18) BY DESCRIPTOR, NUMERIC(18) BY DESCRIPTOR
  RETURNS PARAMETER 2
  ENTRY_POINT 'fbtruncate' MODULE_NAME 'fbudf';
See also
ALTER EXTERNAL FUNCTION, DROP EXTERNAL FUNCTION, CREATE FUNCTION
5.10.2. ALTER EXTERNAL FUNCTION
Alters the entry point and/or the module name of a user-defined function (UDF)
Available in
DSQL
Syntax
ALTER EXTERNAL FUNCTION funcname
  [ENTRY_POINT 'new_entry_point']
  [MODULE_NAME 'new_library_name']
Table 54. ALTER EXTERNAL FUNCTION Statement Parameters
Parameter Description
funcname Function name in the database
new_entry_point The new exported name of the function
new_library_name The new name of the module (MODULE_NAME from which the function is
exported). This will be the name of the file, without the “.dll” or “.so” file
extension.
The ALTER EXTERNAL FUNCTION statement changes the entry point and/or the module name for a user-
defined function (UDF). Existing dependencies remain intact after the statement containing the
change(s) is executed.
Chapter 5. Data Definition (DDL) Statements
213

<!-- Original PDF Page: 215 -->

The ENTRY_POINT clause
is for specifying the new entry point (the name of the function as exported from the module).
The MODULE_NAME clause
is for specifying the new name of the module where the exported function is located.
Any user connected to the database can change the entry point and the module name.
Who Can Alter an External Function
The ALTER EXTERNAL FUNCTION statement can be executed by:
• Administrators
• Owner of the external function
• Users with the ALTER ANY FUNCTION privilege
Examples using ALTER EXTERNAL FUNCTION
Changing the entry point for an external function
ALTER EXTERNAL FUNCTION invl ENTRY_POINT 'intNvl';
Changing the module name for an external function
ALTER EXTERNAL FUNCTION invl MODULE_NAME 'fbudf2';
See also
DECLARE EXTERNAL FUNCTION, DROP EXTERNAL FUNCTION
5.10.3. DROP EXTERNAL FUNCTION
Drops a user-defined function (UDF) from the current database
Available in
DSQL, ESQL
Syntax
DROP EXTERNAL FUNCTION funcname
Table 55. DROP EXTERNAL FUNCTION Statement Parameter
Parameter Description
funcname Function name in the database
The DROP EXTERNAL FUNCTION statement deletes the declaration of a user-defined function from the
database. If there are any dependencies on the external function, the statement will fail and raise
Chapter 5. Data Definition (DDL) Statements
214

<!-- Original PDF Page: 216 -->

an error.
Any user connected to the database can delete the declaration of an internal function.
Who Can Drop an External Function
The DROP EXTERNAL FUNCTION statement can be executed by:
• Administrators
• Owner of the external function
• Users with the DROP ANY FUNCTION privilege
Example using DROP EXTERNAL FUNCTION
Deleting the declaration of the addDay function.
DROP EXTERNAL FUNCTION addDay;
See also
DECLARE EXTERNAL FUNCTION
5.11. PACKAGE
A package is a group of procedures and functions managed as one entity.
5.11.1. CREATE PACKAGE
Creates a package header
Available in
DSQL
Syntax
CREATE PACKAGE package_name
[SQL SECURITY {INVOKER | DEFINER}]
AS
BEGIN
  [ <package_item> ... ]
END
<package_item> ::=
    <function_decl>;
  | <procedure_decl>;
<function_decl> ::=
  FUNCTION funcname [ ( [ <in_params> ] ) ]
  RETURNS <domain_or_non_array_type> [COLLATE collation]
  [DETERMINISTIC]
Chapter 5. Data Definition (DDL) Statements
215

<!-- Original PDF Page: 217 -->

<procedure_decl> ::=
  PROCEDURE procname [ ( [ <in_params> ] ) ]
  [RETURNS (<out_params>)]
<in_params> ::= <inparam> [, <inparam> ... ]
<inparam> ::= <param_decl> [ { = | DEFAULT } <value> ]
<out_params> ::= <outparam> [, <outparam> ...]
<outparam> ::= <param_decl>
<value> ::= { literal | NULL | context_var }
<param-decl> ::= paramname <domain_or_non_array_type> [NOT NULL]
  [COLLATE collation]
<domain_or_non_array_type> ::=
  !! See Scalar Data Types Syntax !!
Table 56. CREATE PACKAGE Statement Parameters
Parameter Description
package_name Package name. The maximum length is 63 characters. The package name
must be unique among all package names.
function_decl Function declaration
procedure_decl Procedure declaration
func_name Function name. The maximum length is 63 characters. The function name
must be unique within the package.
proc_name Procedure name. The maximum length is 63 characters. The function
name must be unique within the package.
collation Collation
inparam Input parameter declaration
outparam Output parameter declaration
literal A literal value that is assignment-compatible with the data type of the
parameter
context_var Any context variable that is assignment-compatible with the data type of
the parameter
paramname The name of an input parameter of a procedure or function, or an output
parameter of a procedure. The maximum length is 63 characters. The
name of the parameter must be unique among input and output
parameters of the procedure or function.
The CREATE PACKAGE statement creates a new package header. Routines (procedures and functions)
Chapter 5. Data Definition (DDL) Statements
216

<!-- Original PDF Page: 218 -->

declared in the package header are available outside the package using the full identifier
(package_name.proc_name or package_name.func_name). Routines defined only in the package
body — but not in the package header — are not visible outside the package.

Package procedure and function names may shadow global routines
If a package header or package body declares a procedure or function with the
same name as a stored procedure or function in the global namespace, it is not
possible to call that global procedure or function from the package body. In this
case, the procedure or function of the package will always be called.
For this reason, it is recommended that the names of stored procedures and
functions in packages do not overlap with names of stored procedures and
functions in the global namespace.
Statement Terminators
Some SQL statement editors — specifically the isql utility that comes with Firebird, and possibly
some third-party editors — employ an internal convention that requires all statements to be
terminated with a semicolon. This creates a conflict with PSQL syntax when coding in these
environments. If you are unacquainted with this problem and its solution, please study the details
in the PSQL chapter in the section entitled Switching the Terminator in isql.
SQL Security
The SQL SECURITY clause specifies the security context for executing other routines or inserting into
other tables from functions or procedures defined in this package. When SQL Security is not
specified, the default value of the database is applied at runtime.
The SQL SECURITY clause can only be specified for the package, not for individual procedures and
functions of the package.
See also SQL Security in chapter Security.
Procedure and Function Parameters
For details on stored procedure parameters, see Parameters in CREATE PROCEDURE.
For details on function parameters, see Parameters in CREATE FUNCTION.
Who Can Create a Package
The CREATE PACKAGE statement can be executed by:
• Administrators
• Users with the CREATE PACKAGE privilege
The user who created the package header becomes its owner.
Chapter 5. Data Definition (DDL) Statements
217

<!-- Original PDF Page: 219 -->

Examples of CREATE PACKAGE
1. Create a package header
CREATE PACKAGE APP_VAR
AS
BEGIN
  FUNCTION GET_DATEBEGIN() RETURNS DATE DETERMINISTIC;
  FUNCTION GET_DATEEND() RETURNS DATE DETERMINISTIC;
  PROCEDURE SET_DATERANGE(ADATEBEGIN DATE,
      ADATEEND DATE DEFAULT CURRENT_DATE);
END
1. With DEFINER set for package pk, user US needs only the EXECUTE privilege on pk. If it were set for
INVOKER, either the user or the package would also need the INSERT privilege on table t.
create table t (i integer);
set term ^;
create package pk SQL SECURITY DEFINER
as
begin
    function f(i integer) returns int;
end^
create package body pk
as
begin
    function f(i integer) returns int
    as
    begin
      insert into t values (:i);
      return i + 1;
    end
end^
set term ;^
grant execute on package pk to user us;
commit;
connect 'localhost:/tmp/69.fdb' user us password 'pas';
select pk.f(3) from rdb$database;
See also
CREATE PACKAGE BODY, RECREATE PACKAGE BODY, ALTER PACKAGE, DROP PACKAGE, RECREATE PACKAGE
5.11.2. ALTER PACKAGE
Alters a package header
Chapter 5. Data Definition (DDL) Statements
218

<!-- Original PDF Page: 220 -->

Available in
DSQL
Syntax
ALTER PACKAGE package_name
[SQL SECURITY {INVOKER | DEFINER}]
AS
BEGIN
  [ <package_item> ... ]
END
!! See syntax of CREATE PACKAGE for further rules!!
The ALTER PACKAGE statement modifies the package header. It can be used to change the number and
definition of procedures and functions, including their input and output parameters. However, the
source and compiled form of the package body is retained, though the body might be incompatible
after the change to the package header. The validity of a package body for the defined header is
stored in the column RDB$PACKAGES.RDB$VALID_BODY_FLAG.
Altering a package without specifying the SQL SECURITY  clause will remove the SQL Security
property if currently set for this package. This means the behaviour will revert to the database
default.
Who Can Alter a Package
The ALTER PACKAGE statement can be executed by:
• Administrators
• The owner of the package
• Users with the ALTER ANY PACKAGE privilege
Examples of ALTER PACKAGE
Modifying a package header
ALTER PACKAGE APP_VAR
AS
BEGIN
  FUNCTION GET_DATEBEGIN() RETURNS DATE DETERMINISTIC;
  FUNCTION GET_DATEEND() RETURNS DATE DETERMINISTIC;
  PROCEDURE SET_DATERANGE(ADATEBEGIN DATE,
      ADATEEND DATE DEFAULT CURRENT_DATE);
END
See also
CREATE PACKAGE, DROP PACKAGE, ALTER PACKAGE BODY, RECREATE PACKAGE BODY
Chapter 5. Data Definition (DDL) Statements
219

<!-- Original PDF Page: 221 -->

5.11.3. CREATE OR ALTER PACKAGE
Creates a package header if it does not exist, or alters a package header
Available in
DSQL
Syntax
CREATE OR ALTER PACKAGE package_name
[SQL SECURITY {INVOKER | DEFINER}]
AS
BEGIN
  [ <package_item> ... ]
END
!! See syntax of CREATE PACKAGE for further rules!!
The CREATE OR ALTER PACKAGE  statement creates a new package or modifies an existing package
header. If the package header does not exist, it will be created using CREATE PACKAGE. If it already
exists, then it will be modified using ALTER PACKAGE  while retaining existing privileges and
dependencies.
Examples of CREATE OR ALTER PACKAGE
Creating a new or modifying an existing package header
CREATE OR ALTER PACKAGE APP_VAR
AS
BEGIN
  FUNCTION GET_DATEBEGIN() RETURNS DATE DETERMINISTIC;
  FUNCTION GET_DATEEND() RETURNS DATE DETERMINISTIC;
  PROCEDURE SET_DATERANGE(ADATEBEGIN DATE,
      ADATEEND DATE DEFAULT CURRENT_DATE);
END
See also
CREATE PACKAGE, ALTER PACKAGE, RECREATE PACKAGE, ALTER PACKAGE BODY, RECREATE PACKAGE BODY
5.11.4. DROP PACKAGE
Drops a package header
Available in
DSQL
Chapter 5. Data Definition (DDL) Statements
220

<!-- Original PDF Page: 222 -->

Syntax
DROP PACKAGE package_name
Table 57. DROP PACKAGE Statement Parameters
Parameter Description
package_name Package name
The DROP PACKAGE statement deletes an existing package header. If a package body exists, it will be
dropped together with the package header. If there are still dependencies on the package, an error
will be raised.
Who Can Drop a Package
The DROP PACKAGE statement can be executed by:
• Administrators
• The owner of the package
• Users with the DROP ANY PACKAGE privilege
Examples of DROP PACKAGE
Dropping a package header
DROP PACKAGE APP_VAR
See also
CREATE PACKAGE, DROP PACKAGE BODY
5.11.5. RECREATE PACKAGE
Drops a package header if it exists, and creates a package header
Available in
DSQL
Syntax
RECREATE PACKAGE package_name
[SQL SECURITY {INVOKER | DEFINER}]
AS
BEGIN
  [ <package_item> ... ]
END
!! See syntax of CREATE PACKAGE for further rules!!
Chapter 5. Data Definition (DDL) Statements
221

<!-- Original PDF Page: 223 -->

The RECREATE PACKAGE statement creates a new package or recreates an existing package header. If a
package header with the same name already exists, then this statement will first drop it and then
create a new package header. It is not possible to recreate the package header if there are still
dependencies on the existing package, or if the body of the package exists. Existing privileges of the
package itself are not preserved, nor are privileges to execute the procedures or functions of the
package.
Examples of RECREATE PACKAGE
Creating a new or recreating an existing package header
RECREATE PACKAGE APP_VAR
AS
BEGIN
  FUNCTION GET_DATEBEGIN() RETURNS DATE DETERMINISTIC;
  FUNCTION GET_DATEEND() RETURNS DATE DETERMINISTIC;
  PROCEDURE SET_DATERANGE(ADATEBEGIN DATE,
      ADATEEND DATE DEFAULT CURRENT_DATE);
END
See also
CREATE PACKAGE, DROP PACKAGE, CREATE PACKAGE BODY, RECREATE PACKAGE BODY
5.12. PACKAGE BODY
5.12.1. CREATE PACKAGE BODY
Creates a package body
Available in
DSQL
Syntax
CREATE PACKAGE BODY name
AS
BEGIN
  [ <package_item> ... ]
  [ <package_body_item> ... ]
END
<package_item> ::=
  !! See CREATE PACKAGE syntax !!
<package_body_item> ::=
  <function_impl> |
  <procedure_impl>
<function_impl> ::=
Chapter 5. Data Definition (DDL) Statements
222

<!-- Original PDF Page: 224 -->

FUNCTION funcname [ ( [ <in_params> ] ) ]
  RETURNS <domain_or_non_array_type> [COLLATE collation]
  [DETERMINISTIC]
  <module-body>
<procedure_impl> ::=
  PROCEDURE procname [ ( [ <in_params> ] ) ]
  [RETURNS (<out_params>)]
  <module-body>
<module-body> ::=
  !! See Syntax of Module Body !!
<in_params> ::=
  !! See CREATE PACKAGE syntax !!
  !! See also Rules below !!
<out_params> ::=
  !! See CREATE PACKAGE syntax !!
<domain_or_non_array_type> ::=
  !! See Scalar Data Types Syntax !!
Table 58. CREATE PACKAGE BODY Statement Parameters
Parameter Description
package_name Package name. The maximum length is 63 characters. The package name
must be unique among all package names.
function_impl Function implementation. Essentially a CREATE FUNCTION statement
without CREATE.
procedure_impl Procedure implementation Essentially a CREATE PROCEDURE statement
without CREATE.
func_name Function name. The maximum length is 63 characters. The function name
must be unique within the package.
collation Collation
proc_name Procedure name. The maximum length is 63 characters. The function
name must be unique within the package.
The CREATE PACKAGE BODY  statement creates a new package body. The package body can only be
created after the package header has been created. If there is no package header with name
package_name, an error is raised.
All procedures and functions declared in the package header must be implemented in the package
body. Additional procedures and functions may be defined and implemented in the package body
only. Procedure and functions defined in the package body, but not defined in the package header,
are not visible outside the package body.
Chapter 5. Data Definition (DDL) Statements
223

<!-- Original PDF Page: 225 -->

The names of procedures and functions defined in the package body must be unique among the
names of procedures and functions defined in the package header and implemented in the package
body.

Package procedure and function names may shadow global routines
If a package header or package body declares a procedure or function with the
same name as a stored procedure or function in the global namespace, it is not
possible to call that global procedure or function from the package body. In this
case, the procedure or function of the package will always be called.
For this reason, it is recommended that the names of stored procedures and
functions in packages do not overlap with names of stored procedures and
functions in the global namespace.
Rules
• In the package body, all procedures and functions must be implemented with the same
signature as declared in the header and at the beginning of the package body
• The default values for procedure or function parameters cannot be overridden (as specified in
the package header or in <package_item>). This means default values can only be defined in
<package_body_item> for procedures or functions that have not been defined in the package
header or earlier in the package body.
 UDF declarations (DECLARE EXTERNAL FUNCTION) are not supported for packages. Use
UDR instead.
Who Can Create a Package Body
The CREATE PACKAGE BODY statement can be executed by:
• Administrators
• The owner of the package
• Users with the ALTER ANY PACKAGE privilege
Examples of CREATE PACKAGE BODY
Creating the package body
CREATE PACKAGE BODY APP_VAR
AS
BEGIN
  - Returns the start date of the period
  FUNCTION GET_DATEBEGIN() RETURNS DATE DETERMINISTIC
  AS
  BEGIN
    RETURN RDB$GET_CONTEXT('USER_SESSION', 'DATEBEGIN');
  END
  - Returns the end date of the period
  FUNCTION GET_DATEEND() RETURNS DATE DETERMINISTIC
Chapter 5. Data Definition (DDL) Statements
224

<!-- Original PDF Page: 226 -->

AS
  BEGIN
    RETURN RDB$GET_CONTEXT('USER_SESSION', 'DATEEND');
  END
  - Sets the date range of the working period
  PROCEDURE SET_DATERANGE(ADATEBEGIN DATE, ADATEEND DATE)
  AS
  BEGIN
    RDB$SET_CONTEXT('USER_SESSION', 'DATEBEGIN', ADATEBEGIN);
    RDB$SET_CONTEXT('USER_SESSION', 'DATEEND', ADATEEND);
  END
END
See also
ALTER PACKAGE BODY, DROP PACKAGE BODY, RECREATE PACKAGE BODY, CREATE PACKAGE
5.12.2. ALTER PACKAGE BODY
Alters a package body
Available in
DSQL
Syntax
ALTER PACKAGE BODY name
AS
BEGIN
  [ <package_item> ... ]
  [ <package_body_item> ... ]
END
!! See syntax of CREATE PACKAGE BODY for further rules !!
The ALTER PACKAGE BODY  statement modifies the package body. It can be used to change the
definition and implementation of procedures and functions of the package body.
See CREATE PACKAGE BODY for more details.
Who Can Alter a Package Body
The ALTER PACKAGE BODY statement can be executed by:
• Administrators
• The owner of the package
• Users with the ALTER ANY PACKAGE privilege
Chapter 5. Data Definition (DDL) Statements
225

<!-- Original PDF Page: 227 -->

Examples of ALTER PACKAGE BODY
Modifying the package body
ALTER PACKAGE BODY APP_VAR
AS
BEGIN
  - Returns the start date of the period
  FUNCTION GET_DATEBEGIN() RETURNS DATE DETERMINISTIC
  AS
  BEGIN
    RETURN RDB$GET_CONTEXT('USER_SESSION', 'DATEBEGIN');
  END
  - Returns the end date of the period
  FUNCTION GET_DATEEND() RETURNS DATE DETERMINISTIC
  AS
  BEGIN
    RETURN RDB$GET_CONTEXT('USER_SESSION', 'DATEEND');
  END
  - Sets the date range of the working period
  PROCEDURE SET_DATERANGE(ADATEBEGIN DATE, ADATEEND DATE)
  AS
  BEGIN
    RDB$SET_CONTEXT('USER_SESSION', 'DATEBEGIN', ADATEBEGIN);
    RDB$SET_CONTEXT('USER_SESSION', 'DATEEND', ADATEEND);
  END
END
See also
CREATE PACKAGE BODY, DROP PACKAGE BODY, RECREATE PACKAGE BODY, ALTER PACKAGE
5.12.3. DROP PACKAGE BODY
Drops a package body
Available in
DSQL
Syntax
DROP PACKAGE package_name
Table 59. DROP PACKAGE BODY Statement Parameters
Parameter Description
package_name Package name
The DROP PACKAGE BODY statement deletes the package body.
Chapter 5. Data Definition (DDL) Statements
226

<!-- Original PDF Page: 228 -->

Who Can Drop a Package Body
The DROP PACKAGE BODY statement can be executed by:
• Administrators
• The owner of the package
• Users with the ALTER ANY PACKAGE privilege
Examples of DROP PACKAGE BODY
Dropping the package body
DROP PACKAGE BODY APP_VAR;
See also
CREATE PACKAGE BODY, ALTER PACKAGE BODY, DROP PACKAGE
5.12.4. RECREATE PACKAGE BODY
Drops a package body if it exists, and creates a package body
Available in
DSQL
Syntax
RECREATE PACKAGE BODY name
AS
BEGIN
  [ <package_item> ... ]
  [ <package_body_item> ... ]
END
!! See syntax of CREATE PACKAGE BODY for further rules !!
The RECREATE PACKAGE BODY  statement creates a new or recreates an existing package body. If a
package body with the same name already exists, the statement will try to drop it and then create a
new package body. After recreating the package body, privileges of the package and its routines are
preserved.
See CREATE PACKAGE BODY for more details.
Examples of RECREATE PACKAGE BODY
Recreating the package body
RECREATE PACKAGE BODY APP_VAR
AS
BEGIN
Chapter 5. Data Definition (DDL) Statements
227

<!-- Original PDF Page: 229 -->

- Returns the start date of the period
  FUNCTION GET_DATEBEGIN() RETURNS DATE DETERMINISTIC
  AS
  BEGIN
    RETURN RDB$GET_CONTEXT('USER_SESSION', 'DATEBEGIN');
  END
  - Returns the end date of the period
  FUNCTION GET_DATEEND() RETURNS DATE DETERMINISTIC
  AS
  BEGIN
    RETURN RDB$GET_CONTEXT('USER_SESSION', 'DATEEND');
  END
  - Sets the date range of the working period
  PROCEDURE SET_DATERANGE(ADATEBEGIN DATE, ADATEEND DATE)
  AS
  BEGIN
    RDB$SET_CONTEXT('USER_SESSION', 'DATEBEGIN', ADATEBEGIN);
    RDB$SET_CONTEXT('USER_SESSION', 'DATEEND', ADATEEND);
  END
END
See also
CREATE PACKAGE BODY, ALTER PACKAGE BODY, DROP PACKAGE BODY, ALTER PACKAGE
5.13. FILTER
A BLOB FILTER is a database object that is a special type of external function, with the sole purpose
of taking a BLOB object in one format and converting it to a BLOB object in another format. The
formats of the BLOB objects are specified with user-defined BLOB subtypes.
External functions for converting BLOB types are stored in dynamic libraries and loaded when
necessary.
For more details on BLOB subtypes, see Binary Data Types.
5.13.1. DECLARE FILTER
Declares a BLOB filter in the current database
Available in
DSQL, ESQL
Syntax
DECLARE FILTER filtername
  INPUT_TYPE <sub_type> OUTPUT_TYPE <sub_type>
  ENTRY_POINT 'function_name' MODULE_NAME 'library_name'
<sub_type> ::= number | <mnemonic>
Chapter 5. Data Definition (DDL) Statements
228

<!-- Original PDF Page: 230 -->

<mnemonic> ::=
    BINARY | TEXT | BLR | ACL | RANGES
  | SUMMARY | FORMAT | TRANSACTION_DESCRIPTION
  | EXTERNAL_FILE_DESCRIPTION | user_defined
Table 60. DECLARE FILTER Statement Parameters
Parameter Description
filtername Filter name in the database. The maximum length is 63 characters. It
need not be the same name as the name exported from the filter library
via ENTRY_POINT.
sub_type BLOB subtype
number BLOB subtype number (must be negative)
mnemonic BLOB subtype mnemonic name
function_name The exported name (entry point) of the function
library_name The name of the module where the filter is located
user_defined User-defined BLOB subtype mnemonic name
The DECLARE FILTER statement makes a BLOB filter available to the database. The name of the BLOB
filter must be unique among the names of BLOB filters.
Specifying the Subtypes
The subtypes can be specified as the subtype number or as the subtype mnemonic name. Custom
subtypes must be represented by negative numbers (from -1 to -32,768), or their user-defined name
from the RDB$TYPES table. An attempt to declare more than one BLOB filter with the same
combination of the input and output types will fail with an error.
INPUT_TYPE
clause defining the BLOB subtype of the object to be converted
OUTPUT_TYPE
clause defining the BLOB subtype of the object to be created.

Mnemonic names can be defined for custom BLOB subtypes and inserted manually
into the system table RDB$TYPES system table:
INSERT INTO RDB$TYPES (RDB$FIELD_NAME, RDB$TYPE, RDB$TYPE_NAME)
VALUES ('RDB$FIELD_SUB_TYPE', -33, 'MIDI');
After the transaction is committed, the mnemonic names can be used in
declarations when you create new filters.
The value of the column RDB$FIELD_NAME must always be 'RDB$FIELD_SUB_TYPE'. If a
Chapter 5. Data Definition (DDL) Statements
229

<!-- Original PDF Page: 231 -->

mnemonic names was defined in upper case, they can be used case-insensitively
and without quotation marks when a filter is declared, following the rules for
other object names.
Warning
In general, the system tables are not writable by users. However, inserting custom
types into RDB$TYPES is still possible if the user is an administrator, or has the
system privilege CREATE_USER_TYPES.
Parameters
ENTRY_POINT
clause defining the name of the entry point (the name of the imported function) in the module.
MODULE_NAME
The clause defining the name of the module where the exported function is located. By default,
modules must be located in the UDF folder of the root directory on the server. The UDFAccess
parameter in firebird.conf allows editing of access restrictions to filter libraries.
Any user connected to the database can declare a BLOB filter.
Who Can Create a BLOB Filter?
The DECLARE FILTER statement can be executed by:
• Administrators
• Users with the CREATE FILTER privilege
The user executing the DECLARE FILTER statement becomes the owner of the filter.
Examples of DECLARE FILTER
1. Creating a BLOB filter using subtype numbers.
DECLARE FILTER DESC_FILTER
  INPUT_TYPE 1
  OUTPUT_TYPE -4
  ENTRY_POINT 'desc_filter'
  MODULE_NAME 'FILTERLIB';
2. Creating a BLOB filter using subtype mnemonic names.
DECLARE FILTER FUNNEL
  INPUT_TYPE blr OUTPUT_TYPE text
  ENTRY_POINT 'blr2asc' MODULE_NAME 'myfilterlib';
See also
Chapter 5. Data Definition (DDL) Statements
230

<!-- Original PDF Page: 232 -->

DROP FILTER
5.13.2. DROP FILTER
Drops a BLOB filter declaration from the current database
Available in
DSQL, ESQL
Syntax
DROP FILTER filtername
Table 61. DROP FILTER Statement Parameter
Parameter Description
filtername Filter name in the database
The DROP FILTER statement removes the declaration of a BLOB filter from the database. Removing a
BLOB filter from a database makes it unavailable for use from that database. The dynamic library
where the conversion function is located remains intact and the removal from one database does
not affect other databases in which the same BLOB filter is still declared.
Who Can Drop a BLOB Filter?
The DROP FILTER statement can be executed by:
• Administrators
• The owner of the filter
• Users with the DROP ANY FILTER privilege
DROP FILTER Example
Dropping a BLOB filter.
DROP FILTER DESC_FILTER;
See also
DECLARE FILTER
5.14. SEQUENCE (GENERATOR)
A sequence — or generator — is a database object used to get unique number values to fill a series.
“Sequence” is the SQL-compliant term for the same thing which — in Firebird — has traditionally
been known as “generator”. Firebird has syntax for both terms.
Sequences are always stored as 64-bit integers, regardless of the SQL dialect of the database.
Chapter 5. Data Definition (DDL) Statements
231

<!-- Original PDF Page: 233 -->


If a client is connected using Dialect 1, the server handles sequence values as 32-bit
integers. Passing a sequence value to a 32-bit field or variable will not cause errors
as long as the current value of the sequence does not exceed the limits of a 32-bit
number. However, as soon as the sequence value exceeds this limit, a database in
Dialect 3 will produce an error. A database in Dialect 1 will truncate (overflow) the
value, which could compromise the uniqueness of the series.
This section describes how to create, alter, set and drop sequences.
5.14.1. CREATE SEQUENCE
Creates a SEQUENCE (GENERATOR)
Available in
DSQL, ESQL
Syntax
CREATE {SEQUENCE | GENERATOR} seq_name
  [START WITH start_value]
  [INCREMENT [BY] increment]
Table 62. CREATE SEQUENCE Statement Parameters
Parameter Description
seq_name Sequence (generator) name. The maximum length is 63 characters
start_value Initial value of the sequence. Default is 1.
increment Increment of the sequence (when using NEXT VALUE FOR seq_name); cannot
be 0. Default is 1.
The statements CREATE SEQUENCE  and CREATE GENERATOR  are synonymous — both create a new
sequence. Either can be used, but CREATE SEQUENCE is recommended as that is the syntax defined in
the SQL standard.
When a sequence is created, its current value is set so that the next value obtained from NEXT VALUE
FOR seq_name is equal to start_value. In other words, the current value of the sequence is set to
(start_value - increment). By default, the start_value is 1 (one).
The optional INCREMENT [BY] clause allows you to specify an increment for the NEXT VALUE FOR
seq_name expression. By default, the increment is 1 (one). The increment cannot be set to 0 (zero).
The GEN_ID(seq_name, <step>) function can be called instead, to “step” the series by a different
integer number. The increment specified through INCREMENT [BY] is not used for GEN_ID.

Non-standard behaviour for negative increments
The SQL standard specifies that sequences with a negative increment should start
at the maximum value of the sequence (2 63
 - 1) and count down. Firebird does not
do that, and instead starts at 1.
Chapter 5. Data Definition (DDL) Statements
232

<!-- Original PDF Page: 234 -->

This may change in a future Firebird version.
Who Can Create a Sequence?
The CREATE SEQUENCE (CREATE GENERATOR) statement can be executed by:
• Administrators
• Users with the CREATE SEQUENCE (CREATE GENERATOR) privilege
The user executing the CREATE SEQUENCE (CREATE GENERATOR) statement becomes its owner.
Examples of CREATE SEQUENCE
1. Creating the EMP_NO_GEN sequence using CREATE SEQUENCE.
CREATE SEQUENCE EMP_NO_GEN;
2. Creating the EMP_NO_GEN sequence using CREATE GENERATOR.
CREATE GENERATOR EMP_NO_GEN;
3. Creating the EMP_NO_GEN sequence with an initial value of 5 and an increment of 1.
CREATE SEQUENCE EMP_NO_GEN START WITH 5;
4. Creating the EMP_NO_GEN sequence with an initial value of 1 and an increment of 10.
CREATE SEQUENCE EMP_NO_GEN INCREMENT BY 10;
5. Creating the EMP_NO_GEN sequence with an initial value of 5 and an increment of 10.
CREATE SEQUENCE EMP_NO_GEN START WITH 5 INCREMENT BY 10;
See also
ALTER SEQUENCE, CREATE OR ALTER SEQUENCE , DROP SEQUENCE, RECREATE SEQUENCE, SET GENERATOR, NEXT
VALUE FOR, GEN_ID() function
5.14.2. ALTER SEQUENCE
Sets the next value of a sequence, or changes its increment
Available in
DSQL
Chapter 5. Data Definition (DDL) Statements
233

<!-- Original PDF Page: 235 -->

Syntax
ALTER {SEQUENCE | GENERATOR} seq_name
  [RESTART [WITH newvalue]]
  [INCREMENT [BY] increment]
Table 63. ALTER SEQUENCE Statement Parameters
Parameter Description
seq_name Sequence (generator) name
newvalue New sequence (generator) value. A 64-bit integer from -2-63
 to 263
-1.
increment Increment of the sequence (when using NEXT VALUE FOR seq_name); cannot
be 0.
The ALTER SEQUENCE statement sets the current value of a sequence to the specified value and/or
changes the increment of the sequence.
The RESTART WITH newvalue clause allows you to set the next value generated by NEXT VALUE FOR
seq_name. To achieve this, the current value of the sequence is set to ( newvalue - increment) with
increment either as specified in the statement, or stored in the metadata of the sequence. The
RESTART clause (without WITH) restarts the sequence with the initial value stored in the metadata of
the sequence.

Contrary to Firebird 3.0, since Firebird 4.0 RESTART WITH newvalue only restarts the
sequence with the specified value, and does not store newvalue as the new initial
value of the sequence. A subsequent ALTER SEQUENCE RESTART  will use the initial
value specified when the sequence was created, and not the newvalue of this
statement. This behaviour is specified in the SQL standard.
It is currently not possible to change the initial value stored in the metadata.

Incorrect use of the ALTER SEQUENCE statement (changing the current value of the
sequence or generator) is likely to break the logical integrity of data, or result in
primary key or unique constraint violations.
INCREMENT [BY] allows you to change the sequence increment for the NEXT VALUE FOR expression.

Changing the increment value takes effect for all queries that run after the
transaction commits. Procedures that are called for the first time after changing
the commit, will use the new value if they use NEXT VALUE FOR . Procedures that
were already used (and cached in the metadata cache) will continue to use the old
increment. You may need to close all connections to the database for the metadata
cache to clear, and the new increment to be used. Procedures using NEXT VALUE FOR
do not need to be recompiled to see the new increment. Procedures using
GEN_ID(gen, expression) are not affected when the increment is changed.
Chapter 5. Data Definition (DDL) Statements
234

<!-- Original PDF Page: 236 -->

Who Can Alter a Sequence?
The ALTER SEQUENCE (ALTER GENERATOR) statement can be executed by:
• Administrators
• The owner of the sequence
• Users with the ALTER ANY SEQUENCE (ALTER ANY GENERATOR) privilege
Examples of ALTER SEQUENCE
1. Setting the value of the EMP_NO_GEN sequence so the next value is 145.
ALTER SEQUENCE EMP_NO_GEN RESTART WITH 145;
2. Resetting the base value of the sequence EMP_NO_GEN to the initial value stored in the metadata
ALTER SEQUENCE EMP_NO_GEN RESTART;
3. Changing the increment of sequence EMP_NO_GEN to 10
ALTER SEQUENCE EMP_NO_GEN INCREMENT BY 10;
See also
SET GENERATOR, CREATE SEQUENCE, CREATE OR ALTER SEQUENCE , DROP SEQUENCE, RECREATE SEQUENCE, NEXT
VALUE FOR, GEN_ID() function
5.14.3. CREATE OR ALTER SEQUENCE
Creates a sequence if it doesn’t exist, or alters a sequence
Available in
DSQL, ESQL
Syntax
CREATE OR ALTER {SEQUENCE | GENERATOR} seq_name
  {RESTART | START WITH start_value}
  [INCREMENT [BY] increment]
Table 64. CREATE OR ALTER SEQUENCE Statement Parameters
Parameter Description
seq_name Sequence (generator) name. The maximum length is 63 characters
start_value Initial value of the sequence. Default is 1.
Chapter 5. Data Definition (DDL) Statements
235

<!-- Original PDF Page: 237 -->

Parameter Description
increment Increment of the sequence (when using NEXT VALUE FOR seq_name); cannot
be 0. Default is 1.
If the sequence does not exist, it will be created. An existing sequence will be changed:
• If RESTART is specified, the sequence will restart with the initial value stored in the metadata
• If the START WITH  clause is specified, the sequence is restarted with start_value, but the
start_value is not stored. In other words, it behaves as RESTART WITH in ALTER SEQUENCE.
• If the INCREMENT [BY] clause is specified, increment is stored as the increment in the metadata,
and used for subsequent calls to NEXT VALUE FOR
Example of CREATE OR ALTER SEQUENCE
Create a new or modify an existing sequence EMP_NO_GEN
CREATE OR ALTER SEQUENCE EMP_NO_GEN
  START WITH 10
  INCREMENT BY 1
See also
CREATE SEQUENCE, ALTER SEQUENCE, DROP SEQUENCE, RECREATE SEQUENCE, SET GENERATOR, NEXT VALUE FOR ,
GEN_ID() function
5.14.4. DROP SEQUENCE
Drops a SEQUENCE (GENERATOR)
Available in
DSQL, ESQL
Syntax
DROP {SEQUENCE | GENERATOR} seq_name
Table 65. DROP SEQUENCE Statement Parameter
Parameter Description
seq_name Sequence (generator) name. The maximum length is 63 characters
The statements DROP SEQUENCE and DROP GENERATOR statements are equivalent: both drop (delete) an
existing sequence (generator). Either is valid but DROP SEQUENCE, being defined in the SQL standard,
is recommended.
The statements will fail if the sequence (generator) has dependencies.
Chapter 5. Data Definition (DDL) Statements
236

<!-- Original PDF Page: 238 -->

Who Can Drop a Sequence?
The DROP SEQUENCE (DROP GENERATOR) statement can be executed by:
• Administrators
• The owner of the sequence
• Users with the DROP ANY SEQUENCE (DROP ANY GENERATOR) privilege
Example of DROP SEQUENCE
Dropping the EMP_NO_GEN series:
DROP SEQUENCE EMP_NO_GEN;
See also
CREATE SEQUENCE, CREATE OR ALTER SEQUENCE, RECREATE SEQUENCE
5.14.5. RECREATE SEQUENCE
Drops a sequence if it exists, and creates a sequence (generator)
Available in
DSQL, ESQL
Syntax
RECREATE {SEQUENCE | GENERATOR} seq_name
  [START WITH start_value]
  [INCREMENT [BY] increment]
Table 66. RECREATE SEQUENCE Statement Parameters
Parameter Description
seq_name Sequence (generator) name. The maximum length is 63 characters
start_value Initial value of the sequence
increment Increment of the sequence (when using NEXT VALUE FOR seq_name); cannot
be 0
See CREATE SEQUENCE for the full syntax of CREATE SEQUENCE and descriptions of defining a sequences
and its options.
RECREATE SEQUENCE creates or recreates a sequence. If a sequence with this name already exists, the
RECREATE SEQUENCE statement will try to drop it and create a new one. Existing dependencies will
prevent the statement from executing.
Chapter 5. Data Definition (DDL) Statements
237

<!-- Original PDF Page: 239 -->

Example of RECREATE SEQUENCE
Recreating sequence EMP_NO_GEN
RECREATE SEQUENCE EMP_NO_GEN
  START WITH 10
  INCREMENT BY 2;
See also
CREATE SEQUENCE, ALTER SEQUENCE, CREATE OR ALTER SEQUENCE, DROP SEQUENCE, SET GENERATOR, NEXT VALUE
FOR, GEN_ID() function
5.14.6. SET GENERATOR
Sets the current value of a sequence (generator)
Available in
DSQL, ESQL
Syntax
SET GENERATOR seq_name TO new_val
Table 67. SET GENERATOR Statement Parameters
Parameter Description
seq_name Generator (sequence) name
new_val New sequence (generator) value. A 64-bit integer from -2-63
 to 263
-1.
The SET GENERATOR statement sets the current value of a sequence or generator to the specified
value.
 Although SET GENERATOR  is considered outdated, it is retained for backward
compatibility. Use of the standards-compliant ALTER SEQUENCE is recommended.
Who Can Use a SET GENERATOR?
The SET GENERATOR statement can be executed by:
• Administrators
• The owner of the sequence (generator)
• Users with the ALTER ANY SEQUENCE (ALTER ANY GENERATOR) privilege
Example of SET GENERATOR
Chapter 5. Data Definition (DDL) Statements
238

<!-- Original PDF Page: 240 -->

Setting the value of the EMP_NO_GEN sequence to 145:
SET GENERATOR EMP_NO_GEN TO 145;

Similar effects can be achieved with ALTER SEQUENCE:
ALTER SEQUENCE EMP_NO_GEN
  RESTART WITH 145 + increment;
Here, the value of increment is the current increment of the sequence. We need
add it as ALTER SEQUENCE calculates the current value to set based on the next value
it should produce.
See also
ALTER SEQUENCE, CREATE SEQUENCE, CREATE OR ALTER SEQUENCE, DROP SEQUENCE, NEXT VALUE FOR, GEN_ID()
function
5.15. EXCEPTION
This section describes how to create, modify and delete custom exceptions for use in error handlers
in PSQL modules.
5.15.1. CREATE EXCEPTION
Creates a custom exception for use in PSQL modules
Available in
DSQL, ESQL
Syntax
CREATE EXCEPTION exception_name '<message>'
<message> ::= <message-part> [<message-part> ...]
<message-part> ::=
    <text>
  | @<slot>
<slot> ::= one of 1..9
Table 68. CREATE EXCEPTION Statement Parameters
Parameter Description
exception_name Exception name. The maximum length is 63 characters
message Default error message. The maximum length is 1,021 characters
Chapter 5. Data Definition (DDL) Statements
239

<!-- Original PDF Page: 241 -->

Parameter Description
text Text of any character
slot Slot number of a parameter. Numbering starts at 1. Maximum slot
number is 9.
The statement CREATE EXCEPTION creates a new exception for use in PSQL modules. If an exception
with the same name exists, the statement will raise an error.
The exception name is an identifier, see Identifiers for more information.
The default message is stored in character set NONE, i.e. in characters of any single-byte character set.
The text can be overridden in the PSQL code when the exception is thrown.
The error message may contain “parameter slots” that can be filled when raising the exception.

If the message contains a parameter slot number that is greater than 9, the second
and subsequent digits will be treated as literal text. For example @10 will be
interpreted as slot 1 followed by a literal ‘0’.
 Custom exceptions are stored in the system table RDB$EXCEPTIONS.
Who Can Create an Exception
The CREATE EXCEPTION statement can be executed by:
• Administrators
• Users with the CREATE EXCEPTION privilege
The user executing the CREATE EXCEPTION statement becomes the owner of the exception.
CREATE EXCEPTION Examples
Creating an exception named E_LARGE_VALUE
CREATE EXCEPTION E_LARGE_VALUE
  'The value is out of range';
Creating a parameterized exception E_INVALID_VALUE
CREATE EXCEPTION E_INVALID_VALUE
  'Invalid value @1 for field @2';
See also
ALTER EXCEPTION, CREATE OR ALTER EXCEPTION, DROP EXCEPTION, RECREATE EXCEPTION
Chapter 5. Data Definition (DDL) Statements
240

<!-- Original PDF Page: 242 -->

5.15.2. ALTER EXCEPTION
Alters the default message of a custom exception
Available in
DSQL, ESQL
Syntax
ALTER EXCEPTION exception_name '<message>'
!! See syntax of CREATE EXCEPTION for further rules !!
Who Can Alter an Exception
The ALTER EXCEPTION statement can be executed by:
• Administrators
• The owner of the exception
• Users with the ALTER ANY EXCEPTION privilege
ALTER EXCEPTION Examples
Changing the default message for the exception E_LARGE_VALUE
ALTER EXCEPTION E_LARGE_VALUE
  'The value exceeds the prescribed limit of 32,765 bytes';
See also
CREATE EXCEPTION, CREATE OR ALTER EXCEPTION, DROP EXCEPTION, RECREATE EXCEPTION
5.15.3. CREATE OR ALTER EXCEPTION
Creates a custom exception if it doesn’t exist, or alters a custom exception
Available in
DSQL
Syntax
CREATE OR ALTER EXCEPTION exception_name '<message>'
!! See syntax of CREATE EXCEPTION for further rules !!
The statement CREATE OR ALTER EXCEPTION  is used to create the specified exception if it does not
exist, or to modify the text of the error message returned from it if it exists already. If an existing
exception is altered by this statement, any existing dependencies will remain intact.
Chapter 5. Data Definition (DDL) Statements
241

<!-- Original PDF Page: 243 -->

CREATE OR ALTER EXCEPTION Example
Changing the message for the exception E_LARGE_VALUE
CREATE OR ALTER EXCEPTION E_LARGE_VALUE
  'The value is higher than the permitted range 0 to 32,765';
See also
CREATE EXCEPTION, ALTER EXCEPTION, RECREATE EXCEPTION
5.15.4. DROP EXCEPTION
Drops a custom exception
Available in
DSQL, ESQL
Syntax
DROP EXCEPTION exception_name
Table 69. DROP EXCEPTION Statement Parameter
Parameter Description
exception_name Exception name
The statement DROP EXCEPTION is used to delete an exception. Any dependencies on the exception
will cause the statement to fail, and the exception will not be deleted.
Who Can Drop an Exception
The DROP EXCEPTION statement can be executed by:
• Administrators
• The owner of the exception
• Users with the DROP ANY EXCEPTION privilege
DROP EXCEPTION Examples
Dropping exception E_LARGE_VALUE
DROP EXCEPTION E_LARGE_VALUE;
See also
CREATE EXCEPTION, RECREATE EXCEPTION
Chapter 5. Data Definition (DDL) Statements
242

<!-- Original PDF Page: 244 -->

5.15.5. RECREATE EXCEPTION
Drops a custom exception if it exists, and creates a custom exception
Available in
DSQL
Syntax
RECREATE EXCEPTION exception_name '<message>'
!! See syntax of CREATE EXCEPTION for further rules !!
The statement RECREATE EXCEPTION creates a new exception for use in PSQL modules. If an exception
with the same name exists already, the RECREATE EXCEPTION statement will try to drop it and create a
new one. If there are any dependencies on the existing exception, the attempted deletion fails and
RECREATE EXCEPTION is not executed.
RECREATE EXCEPTION Example
Recreating the E_LARGE_VALUE exception
RECREATE EXCEPTION E_LARGE_VALUE
  'The value exceeds its limit';
See also
CREATE EXCEPTION, DROP EXCEPTION, CREATE OR ALTER EXCEPTION
5.16. COLLATION
In SQL, text strings are sortable objects. This means that they obey ordering rules, such as
alphabetical order. Comparison operations can be applied to such text strings (for example, “less
than” or “greater than”), where the comparison must apply a certain sort order or collation. For
example, the expression “'a' < 'b'” means that ‘'a'’ precedes ‘'b'’ in the collation. The expression
“'c' > 'b'” means that ‘ 'c'’ follows ‘ 'b'’ in the collation. Text strings of more than one character
are sorted using sequential character comparisons: first the first characters of the two strings are
compared, then the second characters, and so on, until a difference is found between the two
strings. This difference defines the sort order.
A COLLATION is the schema object that defines a collation (or sort order).
5.16.1. CREATE COLLATION
Defines a new collation for a character set
Available in
DSQL
Chapter 5. Data Definition (DDL) Statements
243

<!-- Original PDF Page: 245 -->

Syntax
CREATE COLLATION collname
    FOR charset
    [FROM {basecoll | EXTERNAL ('extname')}]
    [NO PAD | PAD SPACE]
    [CASE [IN]SENSITIVE]
    [ACCENT [IN]SENSITIVE]
    ['<specific-attributes>']
<specific-attributes> ::= <attribute> [; <attribute> ...]
<attribute> ::= attrname=attrvalue
Table 70. CREATE COLLATION Statement Parameters
Parameter Description
collname The name to use for the new collation. The maximum length is 63
characters
charset A character set present in the database
basecoll A collation already present in the database
extname The collation name used in the .conf file
The CREATE COLLATION  statement does not “create” anything, its purpose is to make a collation
known to a database. The collation must already be present on the system, typically in a library file,
and must be properly registered in a .conf file in the intl subdirectory of the Firebird installation.
The collation may alternatively be based on one that is already present in the database.
How the Engine Detects the Collation
The optional FROM clause specifies the base collation that is used to derive a new collation. This
collation must already be present in the database. If the keyword EXTERNAL is specified, then
Firebird will scan the .conf files in $fbroot/intl/, where extname must exactly match the name in
the configuration file (case-sensitive).
If no FROM clause is present, Firebird will scan the .conf file(s) in the intl subdirectory for a collation
with the collation name specified in CREATE COLLATION. In other words, omitting the FROM basecoll
clause is equivalent to specifying FROM EXTERNAL ('collname').
The — single-quoted — extname is case-sensitive and must correspond exactly with the collation
name in the .conf file. The collname, charset and basecoll parameters are case-insensitive unless
enclosed in double-quotes.
When creating a collation, you can specify whether trailing spaces are included in the comparison.
If the NO PAD clause is specified, trailing spaces are taken into account in the comparison. If the PAD
SPACE clause is specified, trailing spaces are ignored in the comparison.
The optional CASE clause allows you to specify whether the comparison is case-sensitive or case-
Chapter 5. Data Definition (DDL) Statements
244

<!-- Original PDF Page: 246 -->

insensitive.
The optional ACCENT clause allows you to specify whether the comparison is accent-sensitive or
accent-insensitive (e.g. if ‘'e'’ and ‘'é'’ are considered equal or unequal).
Specific Attributes
The CREATE COLLATION statement can also include specific attributes to configure the collation. The
available specific attributes are listed in the table below. Not all specific attributes apply to every
collation. If the attribute is not applicable to the collation, but is specified when creating it, it will
not cause an error.
 Specific attribute names are case-sensitive.
In the table, “1 bpc” indicates that an attribute is valid for collations of character sets using 1 byte
per character (so-called narrow character sets), and “UNI” for “Unicode collations”.
Table 71. Specific Collation Attributes
Atrribute Values Valid for Comment
DISABLE-COMPRESSIONS 0, 1 1 bpc, UNI Disables compressions (a.k.a.
contractions). Compressions cause
certain character sequences to be sorted
as atomic units, e.g. Spanish c+h as a
single character ch
DISABLE-EXPANSIONS 0, 1 1 bpc Disables expansions. Expansions cause
certain characters (e.g. ligatures or
umlauted vowels) to be treated as
character sequences and sorted
accordingly
ICU-VERSION default or
M.m
UNI Specifies the ICU library version to use.
Valid values are the ones defined in the
applicable <intl_module> element in
intl/fbintl.conf. Format: either the
string literal “default” or a major+minor
version number like “3.0” (both
unquoted).
LOCALE xx_YY UNI Specifies the collation locale. Requires
complete version of ICU libraries.
Format: a locale string like “du_NL”
(unquoted)
MULTI-LEVEL 0, 1 1 bpc Uses more than one ordering level
NUMERIC-SORT 0, 1 UNI Treats contiguous groups of decimal
digits in the string as atomic units and
sorts them numerically. (This is also
known as natural sorting)
Chapter 5. Data Definition (DDL) Statements
245

<!-- Original PDF Page: 247 -->

Atrribute Values Valid for Comment
SPECIALS-FIRST 0, 1 1 bpc Orders special characters (spaces,
symbols etc.) before alphanumeric
characters

If you want to add a new character set with its default collation into your database,
declare and run the stored procedure sp_register_character_set(name,
max_bytes_per_character), found in misc/intl.sql under the Firebird installation
directory.
In order for this to work, the character set must be present on the system and
registered in a .conf file in the intl subdirectory.
Who Can Create a Collation
The CREATE COLLATION statement can be executed by:
• Administrators
• Users with the CREATE COLLATION privilege
The user executing the CREATE COLLATION statement becomes the owner of the collation.
Examples using CREATE COLLATION
1. Creating a collation using the name found in the fbintl.conf file (case-sensitive)
CREATE COLLATION ISO8859_1_UNICODE FOR ISO8859_1;
2. Creating a collation using a special (user-defined) name (the “external” name must match the
name in the fbintl.conf file)
CREATE COLLATION LAT_UNI
  FOR ISO8859_1
  FROM EXTERNAL ('ISO8859_1_UNICODE');
3. Creating a case-insensitive collation based on one already existing in the database
CREATE COLLATION ES_ES_NOPAD_CI
  FOR ISO8859_1
  FROM ES_ES
  NO PAD
  CASE INSENSITIVE;
4. Creating a case-insensitive collation based on one already existing in the database with specific
attributes
Chapter 5. Data Definition (DDL) Statements
246

<!-- Original PDF Page: 248 -->

CREATE COLLATION ES_ES_CI_COMPR
  FOR ISO8859_1
  FROM ES_ES
  CASE INSENSITIVE
  'DISABLE-COMPRESSIONS=0';
5. Creating a case-insensitive collation by the value of numbers (the so-called natural collation)
CREATE COLLATION nums_coll FOR UTF8
  FROM UNICODE
  CASE INSENSITIVE 'NUMERIC-SORT=1';
CREATE DOMAIN dm_nums AS varchar(20)
  CHARACTER SET UTF8 COLLATE nums_coll; -- original (manufacturer) numbers
CREATE TABLE wares(id int primary key, articul dm_nums ...);
See also
DROP COLLATION
5.16.2. DROP COLLATION
Drops a collation from the database
Available in
DSQL
Syntax
DROP COLLATION collname
Table 72. DROP COLLATION Statement Parameters
Parameter Description
collname The name of the collation
The DROP COLLATION statement removes the specified collation from the database, if it exists. An
error will be raised if the specified collation is not present.

If you want to remove an entire character set with all its collations from the
database, declare and execute the stored procedure
sp_unregister_character_set(name) from the misc/intl.sql subdirectory of the
Firebird installation.
Who Can Drop a Collation
The Drop COLLATION statement can be executed by:
Chapter 5. Data Definition (DDL) Statements
247

<!-- Original PDF Page: 249 -->

• Administrators
• The owner of the collation
• Users with the DROP ANY COLLATION privilege
Example using DROP COLLATION
Deleting the ES_ES_NOPAD_CI collation.
DROP COLLATION ES_ES_NOPAD_CI;
See also
CREATE COLLATION
5.17. CHARACTER SET
5.17.1. ALTER CHARACTER SET
Sets the default collation of a character set
Available in
DSQL
Syntax
ALTER CHARACTER SET charset
  SET DEFAULT COLLATION collation
Table 73. ALTER CHARACTER SET Statement Parameters
Parameter Description
charset Character set identifier
collation The name of the collation
This will affect the future usage of the character set, except for cases where the COLLATE clause is
explicitly overridden. In that case, the collation of existing domains, columns and PSQL variables
will remain intact after the change to the default collation of the underlying character set.

If you change the default collation for the database character set (the one defined
when the database was created), it will change the default collation for the
database.
If you change the default collation for the character set that was specified during
the connection, string constants will be interpreted according to the new collation
value, except in those cases where the character set and/or the collation have been
overridden.
Chapter 5. Data Definition (DDL) Statements
248

<!-- Original PDF Page: 250 -->

Who Can Alter a Character Set
The ALTER CHARACTER SET statement can be executed by:
• Administrators
• Users with the ALTER ANY CHARACTER SET privilege
ALTER CHARACTER SET Example
Setting the default UNICODE_CI_AI collation for the UTF8 encoding
ALTER CHARACTER SET UTF8
  SET DEFAULT COLLATION UNICODE_CI_AI;
5.18. Comments
Database objects and a database itself may be annotated with comments. It is a convenient
mechanism for documenting the development and maintenance of a database. Comments created
with COMMENT ON will survive a gbak backup and restore.
5.18.1. COMMENT ON
Adds a comment to a metadata object
Available in
DSQL
Syntax
COMMENT ON <object> IS {'sometext' | NULL}
<object> ::=
    {DATABASE | SCHEMA}
  | <basic-type> objectname
  | USER username [USING PLUGIN pluginname]
  | COLUMN relationname.fieldname
  | [{PROCEDURE | FUNCTION}] PARAMETER
      [packagename.]routinename.paramname
  | {PROCEDURE | [EXTERNAL] FUNCTION}
      [package_name.]routinename
  | [GLOBAL] MAPPING mappingname
<basic-type> ::=
    CHARACTER SET | COLLATION | DOMAIN
  | EXCEPTION     | FILTER    | GENERATOR
  | INDEX         | PACKAGE   | ROLE
  | SEQUENCE      | TABLE     | TRIGGER
  | VIEW
Chapter 5. Data Definition (DDL) Statements
249

<!-- Original PDF Page: 251 -->

Table 74. COMMENT ON Statement Parameters
Parameter Description
sometext Comment text
basic-type Metadata object type
objectname Metadata object name
username Username
pluginname User manager plugin name
relationname Name of table or view
fieldname Name of the column
package_name Name of the package
routinename Name of stored procedure or function
paramname Name of a stored procedure or function parameter
mappingname Name of a mapping
The COMMENT ON statement adds comments for database objects (metadata). Comments are saved to
the RDB$DESCRIPTION column of the corresponding system tables. Client applications can view
comments from these fields.

1. If you add an empty comment (“''”), it will be saved as NULL in the database.
2. By default, the COMMENT ON USER  statement will create comments on users
managed by the default user manager (the first plugin listed in the UserManager
config option). The USING PLUGIN can be used to comment on a user managed by
a different user manager.
3. Comments on users are not stored for the Legacy_UserManager.
4. Comments on users are stored in the security database.
5. Comments on global mappings are stored in the security database.
6. SCHEMA is currently a synonym for DATABASE; this may change in a future
version, so we recommend to always use DATABASE
 Comments on users are visible to that user through the SEC$USERS virtual table.
Who Can Add a Comment
The COMMENT ON statement can be executed by:
• Administrators
• The owner of the object that is commented on
• Users with the ALTER ANY object_type privilege, where object_type is the type of object
commented on (e.g. PROCEDURE)
Chapter 5. Data Definition (DDL) Statements
250