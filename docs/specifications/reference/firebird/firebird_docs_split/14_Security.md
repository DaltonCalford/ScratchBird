# 14 Security



<!-- Original PDF Page: 575 -->

out via a transaction-level savepoint, and the transaction is then committed. This logic reduces the
amount of garbage collection caused by rolled back transactions.
When the volume of changes performed under a transaction-level savepoint is getting large (~50000
records affected), the engine releases the transaction-level savepoint and uses the Transaction
Inventory Page (TIP) as a mechanism to roll back the transaction if needed.

If you expect the volume of changes in your transaction to be large, you can
specify the NO AUTO UNDO  option in your SET TRANSACTION statement to block the
creation of the transaction-level savepoint. Using the API, you can set this with the
TPB flag isc_tpb_no_auto_undo.
13.1.7. Savepoints and PSQL
Transaction control statements are not allowed in PSQL, as that would break the atomicity of the
statement that calls the procedure. However, Firebird does support the raising and handling of
exceptions in PSQL, so that actions performed in stored procedures and triggers can be selectively
undone without the entire procedure failing.
Internally, automatic savepoints are used to:
• undo all actions in the BEGIN…END block where an exception occurs
• undo all actions performed by the procedure or trigger or, in a selectable procedure, all actions
performed since the last SUSPEND, when execution terminates prematurely because of an
uncaught error or exception
Each PSQL exception handling block is also bounded by automatic system savepoints.
 A BEGIN…END block does not itself create an automatic savepoint. A savepoint is
created only in blocks that contain the WHEN statement for handling exceptions.
Chapter 13. Transaction Control
574

<!-- Original PDF Page: 576 -->

Chapter 14. Security
Databases must be secure and so must the data stored in them. Firebird provides three levels of
data security: user authentication at the server level, SQL privileges within databases,
and — optionally — database encryption. This chapter describes how to manage security at these
three levels.

There is also a fourth level of data security: wire protocol encryption, which
encrypts data in transit between client and server. Wire protocol encryption is out
of scope for this Language Reference.
14.1. User Authentication
The security of the entire database depends on identifying a user and verifying its authority, a
procedure known as authentication. User authentication can be performed in several ways,
depending on the setting of the AuthServer parameter in the firebird.conf configuration file. This
parameter contains the list of authentication plugins that can be used when connecting to the
server. If the first plugin fails when authenticating, then the client can proceed with the next
plugin, etc. When no plugin could authenticate the user, the user receives an error message.
The information about users authorised to access a specific Firebird server is stored in a special
security database named security5.fdb. Each record in security5.fdb is a user account for one user.
For each database, the security database can be overridden in the databases.conf file (parameter
SecurityDatabase). Any database can be a security database, even for that database itself.
A username, with a maximum length of 63 characters, is an identifier, following the normal rules
for identifiers (unquoted case-insensitive, double-quoted case-sensitive). For backwards
compatibility, some statements (e.g. isqls CONNECT) accept usernames enclosed in single quotes,
which will behave as normal, unquoted identifiers.
The maximum password length depends on the user manager plugin (parameter UserManager, in
firebird.conf or databases.conf). Passwords are case-sensitive. The default user manager is the first
plugin in the UserManager list, but this can be overridden in the SQL user management statements.
For the Srp plugin, the maximum password length is 255 characters, for an effective length of 20
bytes (see also Why is the effective password length of SRP 20 bytes? ). For the Legacy_UserManager
plugin only the first eight bytes of a password are significant; whilst it is valid to enter a password
longer than eight bytes for Legacy_UserManager, any subsequent characters are ignored.
Why is the effective password length of SRP 20 bytes?
The SRP plugin does not actually have a 20 byte limit on password length, and longer
passwords can be used (with an implementation limit of 255 characters). Hashes of different
passwords longer than 20 bytes are also — usually — different. This effective limit comes
from the limited hash length in SHA1 (used inside Firebird’s SRP implementation), 20 bytes or
160 bits, and the “pigeonhole principle”. Sooner or later, there will be a shorter (or longer)
password that has the same hash (e.g. in a brute force attack). That is why often the effective
Chapter 14. Security
575

<!-- Original PDF Page: 577 -->

password length for the SHA1 algorithm is said to be 20 bytes.
The embedded version of the server does not use authentication; for embedded, the filesystem
permissions to open the database file are used as authorization to access the database. However,
the username, and — if necessary — the role, must be specified in the connection parameters, as
they control access to database objects.
SYSDBA or the owner of the database have unrestricted access to all objects of the database. Users
with the RDB$ADMIN role have similar unrestricted access if they specify that role when connecting or
with SET ROLE.
14.1.1. Specially Privileged Users
In Firebird, the SYSDBA account is a “superuser” that exists beyond any security restrictions. It has
complete access to all objects in all regular databases on the server, and full read/write access to the
accounts in the security database security5.fdb. No user has remote access to the metadata of the
security database.
For Srp, the SYSDBA account does not exist by default; it will need to be created using an embedded
connection. For Legacy_Auth, the default SYSDBA password on Windows and macOS is
“masterkey” — or “masterke”, to be exact, because of the 8-character length limit.
 The default password “masterkey” is known across the universe. It should be
changed as soon as the Firebird server installation is complete.
Other users can acquire elevated privileges in several ways, some of which depend on the
operating system platform. These are discussed in the sections that follow and are summarised in
Administrators and Fine-grained System Privileges.
POSIX Hosts
On POSIX systems, including macOS, the POSIX username will be used as the Firebird Embedded
username if username is not explicitly specified.
The SYSDBA User on POSIX
On POSIX hosts, other than macOS, the SYSDBA user does not have a default password. If the full
installation is done using the standard scripts, a one-off password will be created and stored in a
text file in the same directory as security5.fdb, commonly /opt/firebird/. The name of the
password file is SYSDBA.password.
 In an installation performed by a distribution-specific installer, the location of the
security database and the password file may be different from the standard one.
The root User
The root user can act directly as SYSDBA on Firebird Embedded. Firebird will treat root as though
it were SYSDBA, and it provides access to all databases on the server.
Chapter 14. Security
576

<!-- Original PDF Page: 578 -->

Windows Hosts
On the Windows Server operating systems, operating system accounts can be used. Windows
authentication (also known as “trusted authentication”) can be enabled by including the Win_Sspi
plugin in the AuthServer list in firebird.conf. The plugin must also be present in the AuthClient
setting at the client-side.
Windows operating system administrators are not automatically granted SYSDBA privileges when
connecting to a database. To make that happen, the internally-created role RDB$ADMIN must be
altered by SYSDBA or the database owner, to enable it. For details, refer to the later section entitled
AUTO ADMIN MAPPING.

Prior to Firebird 3.0, with trusted authentication enabled, users who passed the
default checks were automatically mapped to CURRENT_USER. In Firebird 3.0 and
later, the mapping must be done explicitly using CREATE MAPPING.
The Database Owner
The “owner” of a database is either the user who was CURRENT_USER at the time of creation (or
restore) of the database or, if the USER parameter was supplied in the CREATE DATABASE statement, the
specified user.
“Owner” is not a username. The user who is the owner of a database has full administrator
privileges with respect to that database, including the right to drop it, to restore it from a backup
and to enable or disable the AUTO ADMIN MAPPING capability.
Users with the USER_MANAGEMENT System Privilege
A user with the USER_MANAGEMENT system privilege in the security database can create, alter and drop
users. To receive the USER_MANAGEMENT privilege, the security database must have a role with that
privilege:
create role MANAGE_USERS
  set system privileges to USER_MANAGEMENT;
There are two options for the user to exercise these privileges:
1. Grant the role as a default role. The user will always be able to create, alter or drop users.
grant default MANAGE_USERS to user ALEX;
2. Grant the role as a normal role. The user will only be able to create, alter or drop users when
the role is specified explicitly on login or using SET ROLE.
grant MANAGE_USERS to user ALEX;
If the security database is a different database than the user connects to — which is usually the
Chapter 14. Security
577

<!-- Original PDF Page: 579 -->

case when using security5.fdb — then a role with the same name must also exist and be granted
to the user in that database for the user to be able to activate the role. The role in the other
database does not need any system privileges or other privileges.
 The USER_MANAGEMENT system privilege does not allow a user to grant or revoke the
admin role. This requires the RDB$ADMIN role.
14.1.2. RDB$ADMIN Role
The internally-created role RDB$ADMIN is present in all databases. Assigning the RDB$ADMIN role to a
regular user in a database grants that user the privileges of the SYSDBA, in that database only.
The elevated privileges take effect when the user is logged in to that regular database under the
RDB$ADMIN role, and gives full control over all objects in that database.
Being granted the RDB$ADMIN role in the security database confers the authority to create, alter and
drop user accounts.
In both cases, the user with the elevated privileges can assign RDB$ADMIN role to any other user. In
other words, specifying WITH ADMIN OPTION is unnecessary because it is built into the role.
Granting the RDB$ADMIN Role in the Security Database
Since nobody — not even SYSDBA — can connect to the security database remotely, the GRANT and
REVOKE statements are of no use for this task. Instead, the RDB$ADMIN role is granted and revoked
using the SQL statements for user management:
CREATE USER new_user
  PASSWORD 'password'
  GRANT ADMIN ROLE;
ALTER USER existing_user
  GRANT ADMIN ROLE;
ALTER USER existing_user
  REVOKE ADMIN ROLE;
 GRANT ADMIN ROLE and REVOKE ADMIN ROLE are not statements in the GRANT and REVOKE
lexicon. They are three-word clauses to the statements CREATE USER and ALTER USER.
Table 255. Parameters for RDB$ADMIN Role GRANT and REVOKE
Parameter Description
new_user Name for the new user
existing_user Name of an existing user
password User password
Chapter 14. Security
578

<!-- Original PDF Page: 580 -->

The grantor must be logged in as an administrator.
See also
CREATE USER, ALTER USER, GRANT, REVOKE
Doing the Same Task Using gsec
 With Firebird 3.0, gsec was deprecated. It is recommended to use the SQL user
management statements instead.
An alternative is to use gsec with the -admin parameter to store the RDB$ADMIN attribute on the user’s
record:
gsec -add new_user -pw password -admin yes
gsec -mo existing_user -admin yes
gsec -mo existing_user -admin no
 Depending on the administrative status of the current user, more parameters may
be needed when invoking gsec, e.g. -user and -pass, -role, or -trusted.
Using the RDB$ADMIN Role in the Security Database
To manage user accounts through SQL, the user must have the RDB$ADMIN role in the security
database. No user can connect to the security database remotely, so the solution is that the user
connects to a regular database. From there, they can submit any SQL user management command.
Contrary to Firebird 3.0 or earlier, the user does not need to specify the RDB$ADMIN role on connect,
nor do they need to have the RDB$ADMIN role in the database used to connect.
Using gsec with RDB$ADMIN Rights
To perform user management with gsec, the user must provide the extra switch -role rdb$admin.
Granting the RDB$ADMIN Role in a Regular Database
In a regular database, the RDB$ADMIN role is granted and revoked with the usual syntax for granting
and revoking roles:
GRANT [DEFAULT] RDB$ADMIN TO username
REVOKE [DEFAULT] RDB$ADMIN FROM username
Table 256. Parameters for RDB$ADMIN Role GRANT and REVOKE
Parameter Description
username Name of the user
To grant and revoke the RDB$ADMIN role, the grantor must be logged in as an administrator.
Chapter 14. Security
579

<!-- Original PDF Page: 581 -->

See also
GRANT, REVOKE
Using the RDB$ADMIN Role in a Regular Database
To exercise their RDB$ADMIN privileges, the role must either have been granted as a default role, or
the grantee has to include the role in the connection attributes when connecting to the database, or
specify it later using SET ROLE.
AUTO ADMIN MAPPING
Windows Administrators are not automatically granted RDB$ADMIN privileges when connecting to a
database (when Win_Sspi is enabled). The AUTO ADMIN MAPPING  switch determines whether
Administrators have automatic RDB$ADMIN rights, on a database-by-database basis. By default, when
a database is created, it is disabled.
If AUTO ADMIN MAPPING  is enabled in the database, it will take effect whenever a Windows
Administrator connects:
a. using Win_Sspi authentication, and
b. without specifying any role
After a successful “auto admin” connection, the current role is set to RDB$ADMIN.
If an explicit role was specified on connect, the RDB$ADMIN role can be assumed later in the session
using SET TRUSTED ROLE.
Auto Admin Mapping in Regular Databases
To enable and disable automatic mapping in a regular database:
ALTER ROLE RDB$ADMIN
  SET AUTO ADMIN MAPPING;  -- enable it
ALTER ROLE RDB$ADMIN
  DROP AUTO ADMIN MAPPING; -- disable it
Either statement must be issued by a user with sufficient rights, that is:
• The database owner
• An administrator
• A user with the ALTER ANY ROLE privilege

The statement
ALTER ROLE RDB$ADMIN
  SET AUTO ADMIN MAPPING;
Chapter 14. Security
580

<!-- Original PDF Page: 582 -->

is a simplified form of a CREATE MAPPING statement to create a mapping of the
predefined group DOMAIN_ANY_RID_ADMINS to the role of RDB$ADMIN:
CREATE MAPPING WIN_ADMINS
  USING PLUGIN WIN_SSPI
  FROM Predefined_Group DOMAIN_ANY_RID_ADMINS
  TO ROLE RDB$ADMIN;
Accordingly, the statement
ALTER ROLE RDB$ADMIN
  DROP AUTO ADMIN MAPPING
is equivalent to the statement
DROP MAPPING WIN_ADMINS;
For details, see Mapping of Users to Objects
In a regular database, the status of AUTO ADMIN MAPPING  is checked only at connect time. If an
Administrator has the RDB$ADMIN role because auto-mapping was on when they logged in, they will
keep that role for the duration of the session, even if they or someone else turns off the mapping in
the meantime.
Likewise, switching on AUTO ADMIN MAPPING  will not change the current role to RDB$ADMIN for
Administrators who were already connected.
Auto Admin Mapping in the Security Database
The ALTER ROLE RDB$ADMIN  statement cannot enable or disable AUTO ADMIN MAPPING  in the security
database. However, you can create a global mapping for the predefined group
DOMAIN_ANY_RID_ADMINS to the role RDB$ADMIN in the following way:
CREATE GLOBAL MAPPING WIN_ADMINS
  USING PLUGIN WIN_SSPI
  FROM Predefined_Group DOMAIN_ANY_RID_ADMINS
  TO ROLE RDB$ADMIN;
Additionally, you can use gsec:
gsec -mapping set
gsec -mapping drop
Chapter 14. Security
581

<!-- Original PDF Page: 583 -->

 Depending on the administrative status of the current user, more parameters may
be needed when invoking gsec, e.g. -user and -pass, -role, or -trusted.
Only SYSDBA can enable AUTO ADMIN MAPPING if it is disabled, but any administrator can turn it off.
When turning off AUTO ADMIN MAPPING in gsec, the user turns off the mechanism itself which gave
them access, and thus they would not be able to re-enable AUTO ADMIN MAPPING . Even in an
interactive gsec session, the new flag setting takes effect immediately.
14.1.3. Administrators
An administrator is a user that has sufficient rights to read, write to, create, alter or delete any
object in a database to which that user’s administrator status applies. The table summarises how
“superuser” privileges are enabled in the various Firebird security contexts.
Table 257. Administrator (“Superuser”) Characteristics
User RDB$ADMIN Role Comments
SYSDBA Auto Exists automatically at server level. Has full privileges to
all objects in all databases. Can create, alter and drop
users, but has no direct remote access to the security
database
root user on POSIX Auto Exactly like SYSDBA. Firebird Embedded only.
Superuser on
POSIX
Auto Exactly like SYSDBA. Firebird Embedded only.
Windows
Administrator
Set as CURRENT_ROLE
if login succeeds
Exactly like SYSDBA if the following are all true:
• In firebird.conf file, AuthServer includes Win_Sspi, and
Win_Sspi is present in the client-side plugins
(AuthClient) configuration
• In databases where AUTO ADMIN MAPPING  is enabled, or
an equivalent mapping of the predefined group
DOMAIN_ANY_RID_ADMINS for the role RDB$ADMIN exists
• No role is specified at login
Database owner Auto Like SYSDBA, but only in the databases they own
Regular user Must be
previously
granted; must be
supplied at login
or have been
granted as a
default role
Like SYSDBA, but only in the databases where the role is
granted
Chapter 14. Security
582

<!-- Original PDF Page: 584 -->

User RDB$ADMIN Role Comments
POSIX OS user Must be
previously
granted; must be
supplied at login
or have been
granted as a
default role
Like SYSDBA, but only in the databases where the role is
granted. Firebird Embedded only.
Windows user Must be
previously
granted; must be
supplied at login
Like SYSDBA, but only in the databases where the role is
granted. Only available if in firebird.conf file, AuthServer
includes Win_Sspi, and Win_Sspi is present in the client-side
plugins (AuthClient) configuration
14.1.4. Fine-grained System Privileges
In addition to granting users full administrative privileges, system privileges make it possible to
grant regular users a subset of administrative privileges that have historically been limited to
SYSDBA and administrators only. For example:
• Run utilities such as gbak, gfix, nbackup and so on
• Shut down a database and bring it online
• Trace other users' attachments
• Access the monitoring tables
• Run management statements
The implementation defines a set of system privileges, analogous to object privileges, from which
lists of privileged tasks can be assigned to roles.
It is also possible to grant normal privileges to a system privilege, making the system privilege act
like a special role type.
The system privileges are assigned through CREATE ROLE and ALTER ROLE.

Be aware that each system privilege provides a very thin level of control. For some
tasks it may be necessary to give the user more than one privilege to perform some
task. For example, add IGNORE_DB_TRIGGERS to USE_GSTAT_UTILITY because gstat
needs to ignore database triggers.
List of Valid System Privileges
The following table lists the names of the valid system privileges that can be granted to and revoked
from roles.
USER_MANAGEMENT Manage users (given in the security database)
READ_RAW_PAGES Read pages in raw format using Attachment::getInfo()
Chapter 14. Security
583

<!-- Original PDF Page: 585 -->

CREATE_USER_TYPES Add/change/delete non-system records in RDB$TYPES
USE_NBACKUP_UTILITY Use nbackup to create database copies
CHANGE_SHUTDOWN_MODE Shut down database and bring online
TRACE_ANY_ATTACHMENT Trace other users' attachments
MONITOR_ANY_ATTACHMENT Monitor (tables MON$) other users' attachments
ACCESS_SHUTDOWN_DATABASE Access database when it is shut down
CREATE_DATABASE Create new databases (given in the security database)
DROP_DATABASE Drop this database
USE_GBAK_UTILITY Use gbak utility
USE_GSTAT_UTILITY Use gstat utility
USE_GFIX_UTILITY Use gfix utility
IGNORE_DB_TRIGGERS Instruct engine not to run DB-level triggers
CHANGE_HEADER_SETTINGS Modify parameters in DB header page
SELECT_ANY_OBJECT_IN_DATABASE Use SELECT for any selectable object
ACCESS_ANY_OBJECT_IN_DATABASE Access (in any possible way) any object
MODIFY_ANY_OBJECT_IN_DATABASE Modify (up to drop) any object
CHANGE_MAPPING_RULES Change authentication mappings
USE_GRANTED_BY_CLAUSE Use GRANTED BY in GRANT and REVOKE statements
GRANT_REVOKE_ON_ANY_OBJECT GRANT and REVOKE rights on any object in database
GRANT_REVOKE_ANY_DDL_RIGHT GRANT and REVOKE any DDL rights
CREATE_PRIVILEGED_ROLES Use SET SYSTEM PRIVILEGES in roles
GET_DBCRYPT_INFO Get database encryption information
MODIFY_EXT_CONN_POOL Use command ALTER EXTERNAL CONNECTIONS POOL
REPLICATE_INTO_DATABASE Use replication API to load change sets into database
PROFILE_ANY_ATTACHMENT Profile attachments of other users
14.2. SQL Statements for User Management
This section describes the SQL statements for creating, altering and dropping Firebird user
Chapter 14. Security
584

<!-- Original PDF Page: 586 -->

accounts. These statements can be executed by the following users:
• SYSDBA
• Any user with the RDB$ADMIN role in the security database
• When the AUTO ADMIN MAPPING  flag is enabled in the security database ( security5.fdb or the
security database configured for the current database in the databases.conf), any Windows
Administrator — assuming Win_Sspi was used to connect without specifying roles.
• Any user with the system privilege USER_MANAGEMENT in the security database

For a Windows Administrator, AUTO ADMIN MAPPING  enabled only in a regular
database is not sufficient to permit management of other users. For
instructions to enable it in the security database, see Auto Admin Mapping in
the Security Database.
Non-privileged users can use only the ALTER USER statement, and then only to modify some data of
their own account.
14.2.1. CREATE USER
Creates a Firebird user account
Available in
DSQL
Syntax
CREATE USER username
  <user_option> [<user_option> ...]
  [TAGS (<user_var> [, <user_var> ...]]
<user_option> ::=
    PASSWORD 'password'
  | FIRSTNAME 'firstname'
  | MIDDLENAME 'middlename'
  | LASTNAME 'lastname'
  | {GRANT | REVOKE} ADMIN ROLE
  | {ACTIVE | INACTIVE}
  | USING PLUGIN plugin_name
<user_var> ::=
    tag_name = 'tag_value'
  | DROP tag_name
Table 258. CREATE USER Statement Parameters
Parameter Description
username Username. The maximum length is 63 characters, following the rules for
Firebird identifiers.
Chapter 14. Security
585

<!-- Original PDF Page: 587 -->

Parameter Description
password User password. Valid or effective password length depends on the user
manager plugin. Case-sensitive.
firstname Optional: User’s first name. Maximum length 32 characters
middlename Optional: User’s middle name. Maximum length 32 characters
lastname Optional: User’s last name. Maximum length 32 characters.
plugin_name Name of the user manager plugin.
tag_name Name of a custom attribute. The maximum length is 63 characters,
following the rules for Firebird regular identifiers.
tag_value Value of the custom attribute. The maximum length is 255 characters.
If the user already exist in the Firebird security database for the specified user manager plugin, an
error is raised. It is possible to create multiple users with the same name: one per user manager
plugin.
The username argument must follow the rules for Firebird regular identifiers: see Identifiers in the
Structure chapter. Usernames are case-sensitive when double-quoted (in other words, they follow
the same rules as other delimited identifiers).

Usernames follow the general rules and syntax of identifiers. Thus, a user named
"Alex" is distinct from a user named "ALEX"
CREATE USER ALEX PASSWORD 'bz23ds';
- this user is the same as the first one
CREATE USER Alex PASSWORD 'bz23ds';
- this user is the same as the first one
CREATE USER "ALEX" PASSWORD 'bz23ds';
- and this is a different user
CREATE USER "Alex" PASSWORD 'bz23ds';
The PASSWORD clause specifies the user’s password, and is required. The valid or effective password
length depends on the user manager plugin, see also User Authentication.
The optional FIRSTNAME, MIDDLENAME and LASTNAME clauses can be used to specify additional user
properties, such as the person’s first name, middle name and last name, respectively. These are
VARCHAR(32) fields and can be used to store anything you prefer.
If the GRANT ADMIN ROLE clause is specified, the new user account is created with the privileges of the
RDB$ADMIN role in the security database ( security5.fdb or database-specific). It allows the new user
to manage user accounts from any regular database they log into, but it does not grant the user any
special privileges on objects in those databases.
Chapter 14. Security
586

<!-- Original PDF Page: 588 -->

The REVOKE ADMIN ROLE clause is syntactically valid in a CREATE USER statement, but has no effect. It is
not possible to specify GRANT ADMIN ROLE and REVOKE ADMIN ROLE in one statement.
The ACTIVE clause specifies the user is active and can log in, this is the default.
The INACTIVE clause specifies the user is inactive and cannot log in. It is not possible to specify
ACTIVE and INACTIVE in one statement. The ACTIVE/INACTIVE option is not supported by the
Legacy_UserManager and will be ignored.
The USING PLUGIN clause explicitly specifies the user manager plugin to use for creating the user.
Only plugins listed in the UserManager configuration for this database ( firebird.conf, or overridden
in databases.conf) are valid. The default user manager (first in the UserManager configuration) is
applied when this clause is not specified.

Users of the same name created using different user manager plugins are different
objects. Therefore, the user created with one user manager plugin can only be
altered or dropped by that same plugin.
From the perspective of ownership, and privileges and roles granted in a database,
different user objects with the same name are considered one and the same user.
The TAGS clause can be used to specify additional user attributes. Custom attributes are not
supported (silently ignored) by the Legacy_UserManager. Custom attributes names follow the rules of
Firebird identifiers, but are handled case-insensitive (for example, specifying both "A BC" and "a
bc" will raise an error). The value of a custom attribute can be a string of maximum 255 characters.
The DROP tag_name option is syntactically valid in CREATE USER, but behaves as if the property is not
specified.
 Users can view and alter their own custom attributes. Do not use this for sensitive
or security related information.

CREATE/ALTER/DROP USER  are DDL statements, and only take effect at commit.
Remember to COMMIT your work. In isql, the command SET AUTO ON  will enable
autocommit on DDL statements. In third-party tools and other user applications,
this may not be the case.
Who Can Create a User
To create a user account, the current user must have
• administrator privileges in the security database
• the USER_MANAGEMENT system privilege in the security database. Users with the USER_MANAGEMENT
system privilege can not grant or revoke the admin role.
CREATE USER Examples
1. Creating a user with the username bigshot:
Chapter 14. Security
587

<!-- Original PDF Page: 589 -->

CREATE USER bigshot PASSWORD 'buckshot';
2. Creating a user with the Legacy_UserManager user manager plugin
CREATE USER godzilla PASSWORD 'robot'
  USING PLUGIN Legacy_UserManager;
3. Creating the user john with custom attributes:
CREATE USER john PASSWORD 'fYe_3Ksw'
  FIRSTNAME 'John' LASTNAME 'Doe'
  TAGS (BIRTHYEAR='1970', CITY='New York');
4. Creating an inactive user:
CREATE USER john PASSWORD 'fYe_3Ksw'
  INACTIVE;
5. Creating the user superuser with user management privileges:
CREATE USER superuser PASSWORD 'kMn8Kjh'
GRANT ADMIN ROLE;
See also
ALTER USER, CREATE OR ALTER USER, DROP USER
14.2.2. ALTER USER
Alters a Firebird user account
Available in
DSQL
Syntax
ALTER {USER username | CURRENT USER}
  [SET] [<user_option> [<user_option> ...]]
  [TAGS (<user_var> [, <user_var> ...]]
<user_option> ::=
    PASSWORD 'password'
  | FIRSTNAME 'firstname'
  | MIDDLENAME 'middlename'
  | LASTNAME 'lastname'
Chapter 14. Security
588

<!-- Original PDF Page: 590 -->

| {GRANT | REVOKE} ADMIN ROLE
  | {ACTIVE | INACTIVE}
  | USING PLUGIN plugin_name
<user_var> ::=
    tag_name = 'tag_value'
  | DROP tag_name
See CREATE USER for details on the statement parameters.
Any user can alter their own account, except that only an administrator may use GRANT/REVOKE
ADMIN ROLE and ACTIVE/INACTIVE.
All clauses are optional, but at least one other than USING PLUGIN must be present:
• The PASSWORD parameter is for changing the password for the user
• FIRSTNAME, MIDDLENAME and LASTNAME update these optional user properties, such as the person’s
first name, middle name and last name respectively
• GRANT ADMIN ROLE  grants the user the privileges of the RDB$ADMIN role in the security database
(security5.fdb), enabling them to manage the accounts of other users. It does not grant the user
any special privileges in regular databases.
• REVOKE ADMIN ROLE  removes the user’s administrator in the security database which, once the
transaction is committed, will deny that user the ability to alter any user account except their
own
• ACTIVE will enable a disabled account (not supported for Legacy_UserManager)
• INACTIVE will disable an account (not supported for Legacy_UserManager). This is convenient to
temporarily disable an account without deleting it.
• USING PLUGIN specifies the user manager plugin to use
• TAGS can be used to add, update or remove (DROP) additional custom attributes (not supported for
Legacy_UserManager). Attributes not listed will not be changed.
See CREATE USER for more details on the clauses.
If you need to change your own account, then instead of specifying the name of the current user,
you can use the CURRENT USER clause.

The ALTER CURRENT USER statement follows the normal rules for selecting the user
manager plugin. If the current user was created with a non-default user manager
plugin, they will need to explicitly specify the user manager plugins with USING
PLUGIN plugin_name, or they will receive an error that the user is not found. Or, if a
user with the same name exists for the default user manager, they will alter that
user instead.
 Remember to commit your work if you are working in an application that does not
auto-commit DDL.
Chapter 14. Security
589

<!-- Original PDF Page: 591 -->

Who Can Alter a User?
To modify the account of another user, the current user must have
• administrator privileges in the security database
• the USER_MANAGEMENT system privilege in the security database Users with the USER_MANAGEMENT
system privilege can not grant or revoke the admin role.
Anyone can modify their own account, except for the GRANT/REVOKE ADMIN ROLE and ACTIVE/INACTIVE
options, which require administrative privileges to change.
ALTER USER Examples
1. Changing the password for the user bobby and granting them user management privileges:
ALTER USER bobby PASSWORD '67-UiT_G8'
GRANT ADMIN ROLE;
2. Editing the optional properties (the first and last names) of the user dan:
ALTER USER dan
FIRSTNAME 'No_Jack'
LASTNAME 'Kennedy';
3. Revoking user management privileges from user dumbbell:
ALTER USER dumbbell
DROP ADMIN ROLE;
See also
CREATE USER, DROP USER
14.2.3. CREATE OR ALTER USER
Creates a Firebird user account if it doesn’t exist, or alters a Firebird user account
Available in
DSQL
Syntax
CREATE OR ALTER USER username
  [SET] [<user_option> [<user_option> ...]]
  [TAGS (<user_var> [, <user_var> ...]]
<user_option> ::=
    PASSWORD 'password'
Chapter 14. Security
590

<!-- Original PDF Page: 592 -->

| FIRSTNAME 'firstname'
  | MIDDLENAME 'middlename'
  | LASTNAME 'lastname'
  | {GRANT | REVOKE} ADMIN ROLE
  | {ACTIVE | INACTIVE}
  | USING PLUGIN plugin_name
<user_var> ::=
    tag_name = 'tag_value'
  | DROP tag_name
See CREATE USER and ALTER USER for details on the statement parameters.
If the user does not exist, it will be created as if executing a CREATE USER statement. If the user
already exists, it will be modified as if executing an ALTER USER statement. The CREATE OR ALTER USER
statement must contain at least one of the optional clauses other than USING PLUGIN. If the user does
not exist yet, the PASSWORD clause is required.
 Remember to commit your work if you are working in an application that does not
auto-commit DDL.
CREATE OR ALTER USER Examples
Creating or altering a user
CREATE OR ALTER USER john PASSWORD 'fYe_3Ksw'
FIRSTNAME 'John'
LASTNAME 'Doe'
INACTIVE;
See also
CREATE USER, ALTER USER, DROP USER
14.2.4. DROP USER
Drops a Firebird user account
Available in
DSQL
Syntax
DROP USER username
  [USING PLUGIN plugin_name]
Table 259. DROP USER Statement Parameter
Chapter 14. Security
591

<!-- Original PDF Page: 593 -->

Parameter Description
username Username
plugin_name Name of the user manager plugin
The optional USING PLUGIN clause explicitly specifies the user manager plugin to use for dropping
the user. Only plugins listed in the UserManager configuration for this database ( firebird.conf, or
overridden in databases.conf) are valid. The default user manager (first in the UserManager
configuration) is applied when this clause is not specified.

Users of the same name created using different user manager plugins are different
objects. Therefore, the user created with one user manager plugin can only be
dropped by that same plugin.
 Remember to commit your work if you are working in an application that does not
auto-commit DDL.
Who Can Drop a User?
To drop a user, the current user must have
• administrator privileges in the security database
• the USER_MANAGEMENT system privilege in the security database
DROP USER Example
1. Deleting the user bobby:
DROP USER bobby;
2. Removing a user created with the Legacy_UserManager plugin:
DROP USER Godzilla
  USING PLUGIN Legacy_UserManager;
See also
CREATE USER, ALTER USER
14.3. SQL Privileges
The second level of Firebird’s security model is SQL privileges. Whilst a successful login — the first
level — authorises a user’s access to the server and to all databases under that server, it does not
imply that the user has access to any objects in any databases. When an object is created, only the
user that created it (its owner) and administrators have access to it. The user needs privileges on
each object they need to access. As a general rule, privileges must be granted explicitly to a user by
the object owner or an administrator of the database.
Chapter 14. Security
592

<!-- Original PDF Page: 594 -->

A privilege comprises a DML access type ( SELECT, INSERT, UPDATE, DELETE, EXECUTE and REFERENCES), the
name of a database object (table, view, procedure, role) and the name of the grantee (user,
procedure, trigger, role). Various means are available to grant multiple types of access on an object
to multiple users in a single GRANT statement. Privileges may be revoked from a user with REVOKE
statements.
An additional type of privileges, DDL privileges, provide rights to create, alter or drop specific types
of metadata objects. System privileges provide a subset of administrator permissions to a role (and
indirectly, to a user).
Privileges are stored in the database to which they apply and are not applicable to any other
database, except the DATABASE DDL privileges, which are stored in the security database.
14.3.1. The Object Owner
The user who created a database object becomes its owner. Only the owner of an object and users
with administrator privileges in the database, including the database owner, can alter or drop the
database object.
Administrators, the database owner or the object owner can grant privileges to and revoke them
from other users, including privileges to grant privileges to other users. The process of granting and
revoking SQL privileges is implemented with two statements, GRANT and REVOKE.
14.4. ROLE
A role is a database object that packages a set of privileges. Roles implement the concept of access
control at a group level. Multiple privileges are granted to the role and then that role can be
granted to or revoked from one or many users, or even other roles.
A role that has been granted as a “default” role will be activated automatically. Otherwise, a user
must supply that role in their login credentials — or with SET ROLE — to exercise the associated
privileges. Any other privileges granted to the user directly are not affected by their login with the
role.
Logging in with multiple explicit roles simultaneously is not supported, but a user can have
multiple default roles active at the same time.
In this section the tasks of creating and dropping roles are discussed.
14.4.1. CREATE ROLE
Creates a role
Available in
DSQL, ESQL
Syntax
CREATE ROLE rolename
Chapter 14. Security
593

<!-- Original PDF Page: 595 -->

[SET SYSTEM PRIVILEGES TO <sys_privileges>]
<sys_privileges> ::=
  <sys_privilege> [, <sys_privilege> ...]
<sys_privilege> ::=
    USER_MANAGEMENT | READ_RAW_PAGES
  | CREATE_USER_TYPES | USE_NBACKUP_UTILITY
  | CHANGE_SHUTDOWN_MODE | TRACE_ANY_ATTACHMENT
  | MONITOR_ANY_ATTACHMENT | ACCESS_SHUTDOWN_DATABASE
  | CREATE_DATABASE | DROP_DATABASE
  | USE_GBAK_UTILITY | USE_GSTAT_UTILITY
  | USE_GFIX_UTILITY | IGNORE_DB_TRIGGERS
  | CHANGE_HEADER_SETTINGS
  | SELECT_ANY_OBJECT_IN_DATABASE
  | ACCESS_ANY_OBJECT_IN_DATABASE
  | MODIFY_ANY_OBJECT_IN_DATABASE
  | CHANGE_MAPPING_RULES | USE_GRANTED_BY_CLAUSE
  | GRANT_REVOKE_ON_ANY_OBJECT
  | GRANT_REVOKE_ANY_DDL_RIGHT
  | CREATE_PRIVILEGED_ROLES | GET_DBCRYPT_INFO
  | MODIFY_EXT_CONN_POOL | REPLICATE_INTO_DATABASE
  | PROFILE_ANY_ATTACHMENT
Table 260. CREATE ROLE Statement Parameter
Parameter Description
rolename Role name. The maximum length is 63 characters
sys_privilege System privilege to grant
The statement CREATE ROLE creates a new role object, to which one or more privileges can be
granted subsequently. The name of a role must be unique among the names of roles in the current
database.

It is advisable to make the name of a role unique among usernames as well. The
system will not prevent the creation of a role whose name clashes with an existing
username, but if it happens, the user will be unable to connect to the database.
Who Can Create a Role
The CREATE ROLE statement can be executed by:
• Administrators
• Users with the CREATE ROLE privilege
◦ Setting system privileges also requires the system privilege CREATE_PRIVILEGED_ROLES
The user executing the CREATE ROLE statement becomes the owner of the role.
Chapter 14. Security
594

<!-- Original PDF Page: 596 -->

CREATE ROLE Examples
Creating a role named SELLERS
CREATE ROLE SELLERS;
Creating a role SELECT_ALL with the system privilege to select from any selectable object
CREATE ROLE SELECT_ALL
  SET SYSTEM PRIVILEGES TO SELECT_ANY_OBJECT_IN_DATABASE;
See also
ALTER ROLE, DROP ROLE, GRANT, REVOKE, Fine-grained System Privileges
14.4.2. ALTER ROLE
Alters a role
Available in
DSQL
Syntax
ALTER ROLE rolename
 { SET SYSTEM PRIVILEGES TO <sys_privileges>
 | DROP SYSTEM PRIVILEGES
 | {SET | DROP} AUTO ADMIN MAPPING }
<sys_privileges> ::=
  !! See CREATE ROLE !!
Table 261. ALTER ROLE Statement Parameter
Parameter Description
rolename Role name; specifying anything other than RDB$ADMIN will fail
sys_privilege System privilege to grant
ALTER ROLE can be used to grant or revoke system privileges from a role, or enable and disable the
capability for Windows Administrators to assume administrator privileges  automatically when
logging in.
This last capability can affect only one role: the system-generated role RDB$ADMIN.
For details on auto admin mapping, see AUTO ADMIN MAPPING.
It is not possible to selectively grant or revoke system privileges. Only the privileges listed in the SET
SYSTEM PRIVILEGES clause will be available to the role after commit, and DROP SYSTEM PRIVILEGES will
remove all system privileges from this role.
Chapter 14. Security
595

<!-- Original PDF Page: 597 -->

Who Can Alter a Role
The ALTER ROLE statement can be executed by:
• Administrators
• Users with the ALTER ANY ROLE privilege, with the following caveats
◦ Setting or dropping system privileges also requires the system privilege
CREATE_PRIVILEGED_ROLES
◦ Setting or dropping auto admin mapping also requires the system privilege
CHANGE_MAPPING_RULES
ALTER ROLE Examples
Drop all system privileges from a role named SELECT_ALL
ALTER ROLE SELLERS
  DROP SYSTEM PRIVILEGES;
Grant a role SELECT_ALL the system privilege to select from any selectable object
ALTER ROLE SELECT_ALL
  SET SYSTEM PRIVILEGES TO SELECT_ANY_OBJECT_IN_DATABASE;
See also
CREATE ROLE, GRANT, REVOKE, Fine-grained System Privileges
14.4.3. DROP ROLE
Drops a role
Available in
DSQL, ESQL
Syntax
DROP ROLE rolename
The statement DROP ROLE deletes an existing role. It takes a single argument, the name of the role.
Once the role is deleted, the entire set of privileges is revoked from all users and objects that were
granted the role.
Who Can Drop a Role
The DROP ROLE statement can be executed by:
• Administrators
• The owner of the role
Chapter 14. Security
596

<!-- Original PDF Page: 598 -->

• Users with the DROP ANY ROLE privilege
DROP ROLE Examples
Deleting the role SELLERS
DROP ROLE SELLERS;
See also
CREATE ROLE, GRANT, REVOKE
14.5. Statements for Granting Privileges
A GRANT statement is used for granting privileges — including roles — to users and other database
objects.
14.5.1. GRANT
Grants privileges and assigns roles
Available in
DSQL, ESQL
Syntax (granting privileges)
GRANT <privileges>
  TO <grantee_list>
  [WITH GRANT OPTION]
  [{GRANTED BY | AS} [USER] grantor]
<privileges> ::=
    <table_privileges> | <execute_privilege>
  | <usage_privilege>  | <ddl_privileges>
  | <db_ddl_privilege>
<table_privileges> ::=
  {ALL [PRIVILEGES] | <table_privilege_list> }
    ON [TABLE] {table_name | view_name}
<table_privilege_list> ::=
  <table_privilege> [, <tableprivilege> ...]
<table_privilege> ::=
    SELECT | DELETE | INSERT
  | UPDATE [(col [, col ...])]
  | REFERENCES [(col [, col ...)]
<execute_privilege> ::= EXECUTE ON
  { PROCEDURE proc_name | FUNCTION func_name
Chapter 14. Security
597

<!-- Original PDF Page: 599 -->

| PACKAGE package_name }
<usage_privilege> ::= USAGE ON
  { EXCEPTION exception_name
  | {GENERATOR | SEQUENCE} sequence_name }
<ddl_privileges> ::=
  {ALL [PRIVILEGES] | <ddl_privilege_list>} <object_type>
<ddl_privilege_list> ::=
  <ddl_privilege> [, <ddl_privilege> ...]
<ddl_privilege> ::= CREATE | ALTER ANY | DROP ANY
<object_type> ::=
    CHARACTER SET | COLLATION | DOMAIN | EXCEPTION
  | FILTER | FUNCTION | GENERATOR | PACKAGE
  | PROCEDURE | ROLE | SEQUENCE | TABLE | VIEW
<db_ddl_privileges> ::=
  {ALL [PRIVILEGES] | <db_ddl_privilege_list>} {DATABASE | SCHEMA}
<db_ddl_privilege_list> ::=
  <db_ddl_privilege> [, <db_ddl_privilege> ...]
<db_ddl_privilege> ::= CREATE | ALTER | DROP
<grantee_list> ::= <grantee> [, <grantee> ...]
<grantee> ::=
    PROCEDURE proc_name  | FUNCTION func_name
  | PACKAGE package_name | TRIGGER trig_name
  | VIEW view_name       | ROLE role_name
  | [USER] username      | GROUP Unix_group
  | SYSTEM PRIVILEGE <sys_privilege>
<sys_privilege> ::=
 !! See CREATE ROLE !!
Syntax (granting roles)
GRANT <role_granted_list>
  TO <role_grantee_list>
  [WITH ADMIN OPTION]
  [{GRANTED BY | AS} [USER] grantor]
<role_granted_list> ::=
  <role_granted> [, <role_granted ...]
<role_granted> ::= [DEFAULT] role_name
Chapter 14. Security
598

<!-- Original PDF Page: 600 -->

<role_grantee_list> ::=
  <role_grantee> [, <role_grantee> ...]
<role_grantee> ::=
    user_or_role_name
  | USER username
  | ROLE role_name
Table 262. GRANT Statement Parameters
Parameter Description
grantor The user granting the privilege(s)
table_name The name of a table
view_name The name of a view
col The name of table column
proc_name The name of a stored procedure
func_name The name of a stored function (or UDF)
package_name The name of a package
exception_name The name of an exception
sequence_name The name of a sequence (generator)
object_type The type of metadata object
trig_name The name of a trigger
role_name Role name
username The username to which the privileges are granted to or to which the role
is assigned. If the USER keyword is absent, it can also be a role.
Unix_group The name of a user group in a POSIX operating system
sys_privilege A system privilege
user_or_role_name Name of a user or role
The GRANT statement grants one or more privileges on database objects to users, roles, or other
database objects.
A regular, authenticated user has no privileges on any database object until they are explicitly
granted to that individual user, to a role granted to the user as a default role, or to all users bundled
as the user PUBLIC. When an object is created, only its creator (the owner) and administrators have
privileges to it, and can grant privileges to other users, roles, or objects.
Different sets of privileges apply to different types of metadata objects. The different types of
privileges will be described separately later in this section.
 SCHEMA is currently a synonym for DATABASE; this may change in a future version, so
Chapter 14. Security
599

<!-- Original PDF Page: 601 -->

we recommend to always use DATABASE
The TO Clause
The TO clause specifies the users, roles, and other database objects that are to be granted the
privileges enumerated in privileges. The clause is mandatory.
The optional USER keyword in the TO clause allow you to specify exactly who or what is granted the
privilege. If a USER (or ROLE) keyword is not specified, the server first checks for a role with this
name and, if there is no such role, the privileges are granted to the user with that name without
further checking.
 It is recommended to always explicitly specify USER and ROLE to avoid ambiguity.
Future versions of Firebird may make USER mandatory.

• When a GRANT statement is executed, the security database is not checked for
the existence of the grantee user. This is not a bug: SQL permissions are
concerned with controlling data access for authenticated users, both native
and trusted, and trusted operating system users are not stored in the security
database.
• When granting a privilege to a database object other than user or role, such as
a procedure, trigger or view, you must specify the object type.
• Although the USER keyword is optional, it is advisable to use it, to avoid
ambiguity with roles.
• Privileges granted to a system privilege will be applied when the user is logged
in with a role that has that system privilege.
Packaging Privileges in a ROLE Object
A role is a “container” object that can be used to package a collection of privileges. Use of the role is
then granted to each user or role that requires those privileges. A role can also be granted to a list
of users or roles.
The role must exist before privileges can be granted to it. See CREATE ROLE for the syntax and rules.
The role is maintained by granting privileges to it and, when required, revoking privileges from it.
When a role is dropped  — see DROP ROLE — all users lose the privileges acquired through the role.
Any privileges that were granted additionally to an affected user by way of a different grant
statement are retained.
Unless the role is granted as a default role, a user that is granted a role must explicitly specify that
role, either with their login credentials or activating it using SET ROLE, to exercise the associated
privileges. Any other privileges granted to the user or received through default roles are not
affected by explicitly specifying a role.
More than one role can be granted to the same user. Although only one role can be explicitly
specified, multiple roles can be active for a user, either as default roles, or as roles granted to the
current role.
Chapter 14. Security
600

<!-- Original PDF Page: 602 -->

A role can be granted to a user or to another role.
Cumulative Roles
The ability to grant roles to other roles and default roles results in so-called cumulative roles.
Multiple roles can be active for a user, and the user receives the cumulative privileges of all those
roles.
When a role is explicitly specified on connect or using SET ROLE, the user will assume all privileges
granted to that role, including those privileges granted to the secondary roles (including roles
granted on that secondary role, etc). Or in other words, when the primary role is explicitly
specified, the secondary roles are also activated. The function RDB$ROLE_IN_USE can be used to check
if a role is currently active.
See also Default Roles for the effects of DEFAULT with cumulative roles, and The WITH ADMIN OPTION
Clause for effects on granting.
Default Roles
A role can be granted as a default role by prefixing the role with DEFAULT in the GRANT statement.
Granting roles as a default role to users simplifies management of privileges, as this makes it
possible to group privileges on a role and granting that group of privileges to a user without
requiring the user to explicitly specify the role. Users can receive multiple default roles, granting
them all privileges of those default roles.
The effects of a default role depend on whether the role is granted to a user or to another role:
• When a role is granted to a user as a default role, the role will be activated automatically, and its
privileges will be applied to the user without the need to explicitly specify the role.
Roles that are active by default are not returned from CURRENT_ROLE, but the function
RDB$ROLE_IN_USE can be used to check if a role is currently active.
• When a role is granted to another role as a default role, the rights of that role will only be
automatically applied to the user if the primary role is granted as a default role to the user,
otherwise the primary role needs to be specified explicitly (in other words, it behaves the same
as when the secondary role was granted without the DEFAULT clause).
For a linked list of granted roles, all roles need to be granted as a default role for them to be
applied automatically. That is, for GRANT DEFAULT ROLEA TO ROLE ROLEB , GRANT ROLEB TO ROLE
ROLEC, GRANT DEFAULT ROLEC TO USER USER1 only ROLEC is active by default for USER1. To assume the
privileges of ROLEA and ROLEB, ROLEC needs to be explicitly specified, or ROLEB needs to be granted
DEFAULT to ROLEC.
The User PUBLIC
Firebird has a predefined user named PUBLIC, that represents all users. Privileges for operations on
a particular object that are granted to the user PUBLIC can be exercised by any authenticated user.
 If privileges are granted to the user PUBLIC, they should be revoked from the user
Chapter 14. Security
601

<!-- Original PDF Page: 603 -->

PUBLIC as well.
The WITH GRANT OPTION Clause
The optional WITH GRANT OPTION  clause allows the users specified in the user list to grant the
privileges specified in the privilege list to other users.
 It is possible to assign this option to the user PUBLIC. Do not do this!
The GRANTED BY Clause
By default, when privileges are granted in a database, the current user is recorded as the grantor.
The GRANTED BY clause enables the current user to grant those privileges as another user.
When using the REVOKE statement, it will fail if the current user is not the user that was named in
the GRANTED BY clause.
The GRANTED BY (and AS) clause can be used only by the database owner and other administrators.
The object owner cannot use GRANTED BY unless they also have administrator privileges.
Alternative Syntax Using AS username
The non-standard AS clause is supported as a synonym of the GRANTED BY  clause to simplify
migration from other database systems.
Privileges on Tables and Views
For tables and views, unlike other metadata objects, it is possible to grant several privileges at once.
List of Privileges on Tables
SELECT
Permits the user or object to SELECT data from the table or view
INSERT
Permits the user or object to INSERT rows into the table or view
DELETE
Permits the user or object to DELETE rows from the table or view
UPDATE
Permits the user or object to UPDATE rows in the table or view, optionally restricted to specific
columns
REFERENCES
Permits the user or object to reference the table via a foreign key, optionally restricted to the
specified columns. If the primary or unique key referenced by the foreign key of the other table
is composite then all columns of the key must be specified.
Chapter 14. Security
602

<!-- Original PDF Page: 604 -->

ALL [PRIVILEGES]
Combines SELECT, INSERT, UPDATE, DELETE and REFERENCES privileges in a single package
Examples of GRANT <privilege> on Tables
1. SELECT and INSERT privileges to the user ALEX:
GRANT SELECT, INSERT ON TABLE SALES
  TO USER ALEX;
2. The SELECT privilege to the MANAGER, ENGINEER roles and to the user IVAN:
GRANT SELECT ON TABLE CUSTOMER
  TO ROLE MANAGER, ROLE ENGINEER, USER IVAN;
3. All privileges to the ADMINISTRATOR role, together with the authority to grant the same privileges
to others:
GRANT ALL ON TABLE CUSTOMER
  TO ROLE ADMINISTRATOR
  WITH GRANT OPTION;
4. The SELECT and REFERENCES privileges on the NAME column to all users and objects:
GRANT SELECT, REFERENCES (NAME) ON TABLE COUNTRY
TO PUBLIC;
5. The SELECT privilege being granted to the user IVAN by the user ALEX:
GRANT SELECT ON TABLE EMPLOYEE
  TO USER IVAN
  GRANTED BY ALEX;
6. Granting the UPDATE privilege on the FIRST_NAME, LAST_NAME columns:
GRANT UPDATE (FIRST_NAME, LAST_NAME) ON TABLE EMPLOYEE
  TO USER IVAN;
7. Granting the INSERT privilege to the stored procedure ADD_EMP_PROJ:
GRANT INSERT ON EMPLOYEE_PROJECT
  TO PROCEDURE ADD_EMP_PROJ;
Chapter 14. Security
603

<!-- Original PDF Page: 605 -->

The EXECUTE Privilege
The EXECUTE privilege applies to stored procedures, stored functions (including UDFs), and packages.
It allows the grantee to execute the specified object, and, if applicable, to retrieve its output.
In the case of selectable stored procedures, it acts somewhat like a SELECT privilege, insofar as this
style of stored procedure is executed in response to a SELECT statement.
 For packages, the EXECUTE privilege can only be granted for the package as a whole,
not for individual subroutines.
Examples of Granting the EXECUTE Privilege
1. Granting the EXECUTE privilege on a stored procedure to a role:
GRANT EXECUTE ON PROCEDURE ADD_EMP_PROJ
  TO ROLE MANAGER;
2. Granting the EXECUTE privilege on a stored function to a role:
GRANT EXECUTE ON FUNCTION GET_BEGIN_DATE
  TO ROLE MANAGER;
3. Granting the EXECUTE privilege on a package to user PUBLIC:
GRANT EXECUTE ON PACKAGE APP_VAR
  TO USER PUBLIC;
4. Granting the EXECUTE privilege on a function to a package:
GRANT EXECUTE ON FUNCTION GET_BEGIN_DATE
  TO PACKAGE APP_VAR;
The USAGE Privilege
To be able to use metadata objects other than tables, views, stored procedures or functions, triggers
and packages, it is necessary to grant the user (or database object like trigger, procedure or
function) the USAGE privilege on these objects.
By default, Firebird executes PSQL modules with the privileges of the caller, so it is necessary that
either the user or otherwise the routine itself has been granted the USAGE privilege. This can be
changed with the SQL SECURITY clause of the DDL statements of those objects.

The USAGE privilege is currently only available for exceptions and sequences (in
gen_id(gen_name, n) or next value for gen_name). Support for the USAGE privilege
Chapter 14. Security
604

<!-- Original PDF Page: 606 -->

for other metadata objects may be added in future releases.

For sequences (generators), the USAGE privilege only grants the right to increment
the sequence using the GEN_ID function or NEXT VALUE FOR . The SET GENERATOR 
statement is a synonym for ALTER SEQUENCE … RESTART WITH …, and is considered a
DDL statement. By default, only the owner of the sequence and administrators
have the rights to such operations. The right to set the initial value of any sequence
can be granted with GRANT ALTER ANY SEQUENCE , which is not recommend for
general users.
Examples of Granting the USAGE Privilege
1. Granting the USAGE privilege on a sequence to a role:
GRANT USAGE ON SEQUENCE GEN_AGE
  TO ROLE MANAGER;
2. Granting the USAGE privilege on a sequence to a trigger:
GRANT USAGE ON SEQUENCE GEN_AGE
  TO TRIGGER TR_AGE_BI;
3. Granting the USAGE privilege on an exception to a package:
GRANT USAGE ON EXCEPTION
  TO PACKAGE PKG_BILL;
DDL Privileges
By default, only administrators can create new metadata objects. Altering or dropping these objects
is restricted to the owner of the object (its creator) and administrators. DDL privileges can be used
to grant privileges for these operations to other users.
Available DDL Privileges
CREATE
Allows creation of an object of the specified type
ALTER ANY
Allows modification of any object of the specified type
DROP ANY
Allows deletion of any object of the specified type
ALL [PRIVILEGES]
Combines the CREATE, ALTER ANY and DROP ANY privileges for the specified type
Chapter 14. Security
605

<!-- Original PDF Page: 607 -->


There are no separate DDL privileges for triggers and indexes. The necessary
privileges are inherited from the table or view. Creating, altering or dropping a
trigger or index requires the ALTER ANY TABLE or ALTER ANY VIEW privilege.
Examples of Granting DDL Privileges
1. Allow user JOE to create tables
GRANT CREATE TABLE
  TO USER Joe;
2. Allow user JOE to alter any procedure
GRANT ALTER ANY PROCEDURE
  TO USER Joe;
Database DDL Privileges
The syntax for granting privileges to create, alter or drop a database deviates from the normal
syntax of granting DDL privileges for other object types.
Available Database DDL Privileges
CREATE
Allows creation of a database
ALTER
Allows modification of the current database
DROP
Allows deletion of the current database
ALL [PRIVILEGES]
Combines the ALTER and DROP privileges. ALL does not include the CREATE privilege.
The ALTER DATABASE and DROP DATABASE privileges apply only to the current database, whereas DDL
privileges ALTER ANY and DROP ANY on other object types apply to all objects of the specified type in
the current database. The privilege to alter or drop the current database can only be granted by
administrators.
The CREATE DATABASE privilege is a special kind of privilege as it is saved in the security database. A
list of users with the CREATE DATABASE privilege is available from the virtual table SEC$DB_CREATORS.
Only administrators in the security database can grant the privilege to create a new database.
 SCHEMA is currently a synonym for DATABASE; this may change in a future version, so
we recommend to always use DATABASE
Chapter 14. Security
606

<!-- Original PDF Page: 608 -->

Examples of Granting Database DDL Privileges
1. Granting SUPERUSER the privilege to create databases:
GRANT CREATE DATABASE
  TO USER Superuser;
2. Granting JOE the privilege to execute ALTER DATABASE for the current database:
GRANT ALTER DATABASE
  TO USER Joe;
3. Granting FEDOR the privilege to drop the current database:
GRANT DROP DATABASE
  TO USER Fedor;
Assigning Roles
Assigning a role is similar to granting a privilege. One or more roles can be assigned to one or more
users, including the user PUBLIC, using one GRANT statement.
The WITH ADMIN OPTION Clause
The optional WITH ADMIN OPTION clause allows the users specified in the user list to grant the role(s)
specified to other users or roles.
 It is possible to assign this option to PUBLIC. Do not do this!
For cumulative roles, a user can only exercise the WITH ADMIN OPTION  of a secondary role if all
intermediate roles are also granted WITH ADMIN OPTION . That is, GRANT ROLEA TO ROLE ROLEB WITH
ADMIN OPTION, GRANT ROLEB TO ROLE ROLEC, GRANT ROLEC TO USER USER1 WITH ADMIN OPTION  only allows
USER1 to grant ROLEC to other users or roles, while using GRANT ROLEB TO ROLE ROLEC WITH ADMIN
OPTION allows USER1 to grant ROLEA, ROLEB and ROLEC to other users.
Examples of Role Assignment
1. Assigning the DIRECTOR and MANAGER roles to the user IVAN:
GRANT DIRECTOR, MANAGER
  TO USER IVAN;
2. Assigning the MANAGER role to the user ALEX with the authority to assign this role to other users:
GRANT MANAGER
Chapter 14. Security
607

<!-- Original PDF Page: 609 -->

TO USER ALEX WITH ADMIN OPTION;
3. Assigning the DIRECTOR role to user ALEX as a default role:
GRANT DEFAULT DIRECTOR
  TO USER ALEX;
4. Assigning the MANAGER role to role DIRECTOR:
GRANT MANAGER
  TO ROLE DIRECTOR;
See also
REVOKE
14.6. Statements for Revoking Privileges
A REVOKE statement is used for revoking privileges — including roles — from users and other
database objects.
14.6.1. REVOKE
Revokes privileges or role assignments
Available in
DSQL, ESQL
Syntax (revoking privileges)
REVOKE [GRANT OPTION FOR] <privileges>
  FROM <grantee_list>
  [{GRANTED BY | AS} [USER] grantor]
<privileges> ::=
  !! See GRANT syntax !!
Syntax (revoking roles)
REVOKE [ADMIN OPTION FOR] <role_granted_list>
  FROM <role_grantee_list>
  [{GRANTED BY | AS} [USER] grantor]
<role_granted_list> ::=
  !! See GRANT syntax !!
<role_grantee_list> ::=
Chapter 14. Security
608

<!-- Original PDF Page: 610 -->

!! See GRANT syntax !!
Syntax (revoking all)
REVOKE ALL ON ALL FROM <grantee_list>
<grantee_list> ::=
  !! See GRANT syntax !!
Table 263. REVOKE Statement Parameters
Parameter Description
grantor The grantor user on whose behalf the privilege(s) are being revoked
The REVOKE statement revokes privileges that were granted using the GRANT statement from users,
roles, and other database objects. See GRANT for detailed descriptions of the various types of
privileges.
Only the user who granted the privilege can revoke it.
The DEFAULT Clause
When the DEFAULT clause is specified, the role itself is not revoked, only its DEFAULT property is
removed without revoking the role itself.
The FROM Clause
The FROM clause specifies a list of users, roles and other database objects that will have the
enumerated privileges revoked. The optional USER keyword in the FROM clause allow you to specify
exactly which type is to have the privilege revoked. If a USER (or ROLE) keyword is not specified, the
server first checks for a role with this name and, if there is no such role, the privileges are revoked
from the user with that name without further checking.

• Although the USER keyword is optional, it is advisable to use them to avoid
ambiguity with roles.
• The REVOKE statement does not check for the existence of the user from which
the privileges are being revoked.
• When revoking a privilege from a database object other than USER or ROLE, you
must specify its object type

Revoking Privileges from user PUBLIC
Privileges that were granted to the special user named PUBLIC must be revoked
from the user PUBLIC. User PUBLIC provides a way to grant privileges to all users at
once, but it is not “a group of users”.
Chapter 14. Security
609

<!-- Original PDF Page: 611 -->

Revoking the GRANT OPTION
The optional GRANT OPTION FOR clause revokes the user’s privilege to grant the specified privileges to
other users, roles, or database objects (as previously granted with the WITH GRANT OPTION). It does
not revoke the specified privilege itself.
Removing the Privilege to One or More Roles
One usage of the REVOKE statement is to remove roles that were assigned to a user, or a group of
users, by a GRANT statement. In the case of multiple roles and/or multiple grantees, the REVOKE verb is
followed by the list of roles that will be removed from the list of users specified after the FROM
clause.
The optional ADMIN OPTION FOR clause provides the means to revoke the grantee’s “administrator”
privilege, the ability to assign the same role to other users, without revoking the grantee’s privilege
to the role.
Multiple roles and grantees can be processed in a single statement.
Revoking Privileges That Were GRANTED BY
A privilege that has been granted using the GRANTED BY clause is internally attributed explicitly to
the grantor designated by that original GRANT statement. Only that user can revoke the granted
privilege. Using the GRANTED BY clause you can revoke privileges as if you are the specified user. To
revoke a privilege with GRANTED BY , the current user must be logged in either with full
administrative privileges, or as the user designated as grantor by that GRANTED BY clause.
 Not even the owner of a role can use GRANTED BY unless they have administrative
privileges.
The non-standard AS clause is supported as a synonym of the GRANTED BY  clause to simplify
migration from other database systems.
Revoking ALL ON ALL
The REVOKE ALL ON ALL statement allows a user to revoke all privileges (including roles) on all object
from one or more users, roles or other database objects. It is a quick way to “clear” privileges when
access to the database must be blocked for a particular user or role.
When the current user is logged in with full administrator privileges in the database, the REVOKE ALL
ON ALL will remove all privileges, no matter who granted them. Otherwise, only the privileges
granted by the current user are removed.
 The GRANTED BY clause is not supported with ALL ON ALL.
Examples using REVOKE
1. Revoking the privileges for selecting and inserting into the table (or view) SALES
Chapter 14. Security
610

<!-- Original PDF Page: 612 -->

REVOKE SELECT, INSERT ON TABLE SALES
  FROM USER ALEX;
2. Revoking the privilege for selecting from the CUSTOMER table from the MANAGER and ENGINEER roles
and from the user IVAN:
REVOKE SELECT ON TABLE CUSTOMER
  FROM ROLE MANAGER, ROLE ENGINEER, USER IVAN;
3. Revoking from the ADMINISTRATOR role the privilege to grant any privileges on the CUSTOMER table
to other users or roles:
REVOKE GRANT OPTION FOR ALL ON TABLE CUSTOMER
  FROM ROLE ADMINISTRATOR;
4. Revoking the privilege for selecting from the COUNTRY table and the privilege to reference the
NAME column of the COUNTRY table from any user, via the special user PUBLIC:
REVOKE SELECT, REFERENCES (NAME) ON TABLE COUNTRY
  FROM PUBLIC;
5. Revoking the privilege for selecting form the EMPLOYEE table from the user IVAN, that was granted
by the user ALEX:
REVOKE SELECT ON TABLE EMPLOYEE
  FROM USER IVAN GRANTED BY ALEX;
6. Revoking the privilege for updating the FIRST_NAME and LAST_NAME columns of the EMPLOYEE table
from the user IVAN:
REVOKE UPDATE (FIRST_NAME, LAST_NAME) ON TABLE EMPLOYEE
  FROM USER IVAN;
7. Revoking the privilege for inserting records into the EMPLOYEE_PROJECT table from the
ADD_EMP_PROJ procedure:
REVOKE INSERT ON EMPLOYEE_PROJECT
  FROM PROCEDURE ADD_EMP_PROJ;
8. Revoking the privilege for executing the procedure ADD_EMP_PROJ from the MANAGER role:
Chapter 14. Security
611

<!-- Original PDF Page: 613 -->

REVOKE EXECUTE ON PROCEDURE ADD_EMP_PROJ
  FROM ROLE MANAGER;
9. Revoking the privilege to grant the EXECUTE privilege for the function GET_BEGIN_DATE to other
users from the role MANAGER:
REVOKE GRANT OPTION FOR EXECUTE
  ON FUNCTION GET_BEGIN_DATE
  FROM ROLE MANAGER;
10. Revoking the EXECUTE privilege on the package DATE_UTILS from user ALEX:
REVOKE EXECUTE ON PACKAGE DATE_UTILS
  FROM USER ALEX;
11. Revoking the USAGE privilege on the sequence GEN_AGE from the role MANAGER:
REVOKE USAGE ON SEQUENCE GEN_AGE
  FROM ROLE MANAGER;
12. Revoking the USAGE privilege on the sequence GEN_AGE from the trigger TR_AGE_BI:
REVOKE USAGE ON SEQUENCE GEN_AGE
  FROM TRIGGER TR_AGE_BI;
13. Revoking the USAGE privilege on the exception E_ACCESS_DENIED from the package PKG_BILL:
REVOKE USAGE ON EXCEPTION E_ACCESS_DENIED
  FROM PACKAGE PKG_BILL;
14. Revoking the privilege to create tables from user JOE:
REVOKE CREATE TABLE
  FROM USER Joe;
15. Revoking the privilege to alter any procedure from user JOE:
REVOKE ALTER ANY PROCEDURE
  FROM USER Joe;
16. Revoking the privilege to create databases from user SUPERUSER:
Chapter 14. Security
612

<!-- Original PDF Page: 614 -->

REVOKE CREATE DATABASE
  FROM USER Superuser;
17. Revoking the DIRECTOR and MANAGER roles from the user IVAN:
REVOKE DIRECTOR, MANAGER FROM USER IVAN;
18. Revoke from the user ALEX the privilege to grant the MANAGER role to other users:
REVOKE ADMIN OPTION FOR MANAGER FROM USER ALEX;
19. Revoking all privileges (including roles) on all objects from the user IVAN:
REVOKE ALL ON ALL
  FROM USER IVAN;
After this statement is executed by an administrator, the user IVAN will have no privileges
whatsoever, except those granted through PUBLIC.
20. Revoking the DEFAULT property of the DIRECTOR role from user ALEX, while the role itself remains
granted:
REVOKE DEFAULT DIRECTOR
  FROM USER ALEX;
See also
GRANT
14.7. Mapping of Users to Objects
Now Firebird support multiple security databases, new problems arise that could not occur with a
single, global security database. Clusters of databases using the same security database are
effectively separated. Mappings provide the means to achieve the same effect when multiple
databases are using their own security databases. Some cases require control for limited
interaction between such clusters. For example:
• when EXECUTE STATEMENT ON EXTERNAL DATA SOURCE requires data exchange between clusters
• when server-wide SYSDBA access to databases is needed from other clusters, using services.
• On Windows, due to support for Trusted User authentication: to map Windows users to a
Firebird user and/or role. An example is the need for a ROLE granted to a Windows group to be
assigned automatically to members of that group.
The single solution for all such cases is mapping the login information assigned to a user when it
Chapter 14. Security
613

<!-- Original PDF Page: 615 -->

connects to a Firebird server to internal security objects in a database —  CURRENT_USER and
CURRENT_ROLE.
14.7.1. The Mapping Rule
The mapping rule consists of four pieces of information:
1. mapping scope — whether the mapping is local to the current database or whether its effect is
to be global, affecting all databases in the cluster, including security databases
2. mapping name — an SQL identifier, since mappings are objects in a database, like any other
3. the object FROM which the mapping maps. It consists of four items:
◦ The authentication source
▪ plugin name or
▪ the product of a mapping in another database or
▪ use of server-wide authentication or
▪ any method
◦ The name of the database where authentication succeeded
◦ The name of the object from which mapping is performed
◦ The type of that name — username, role, or OS group — depending upon the plugin that
added that name during authentication.
Any item is accepted but only type is required.
4. the object TO which the mapping maps. It consists of two items:
◦ The name of the object TO which mapping is performed
◦ The type, for which only USER or ROLE is valid
14.7.2. CREATE MAPPING
Creates a mapping of a security object
Available in
DSQL
Syntax
CREATE [GLOBAL] MAPPING name
  USING
    { PLUGIN plugin_name [IN database]
    | ANY PLUGIN [IN database | SERVERWIDE]
    | MAPPING [IN database] | '*' [IN database] }
  FROM {ANY type | type from_name}
  TO {USER | ROLE} [to_name]
Chapter 14. Security
614

<!-- Original PDF Page: 616 -->

Table 264. CREATE MAPPING Statement Parameter
Parameter Description
name Mapping name The maximum length is 63 characters. Must be unique
among all mapping names in the context (local or GLOBAL).
plugin_name Authentication plugin name
database Name of the database that authenticated against
type The type of object to be mapped. Possible types are plugin-specific.
from_name The name of the object to be mapped
to_name The name of the user or role to map to
The CREATE MAPPING statement creates a mapping of security objects (e.g. users, groups, roles) of one
or more authentication plugins to internal security objects — CURRENT_USER and CURRENT_ROLE.
If the GLOBAL clause is present, then the mapping will be applied not only for the current database,
but for all databases in the same cluster, including security databases.
 There can be global and local mappings with the same name. They are distinct
objects.

Global mapping works best if a Firebird 3.0 or higher version database is used as
the security database. If you plan to use another database for this purpose — using
your own provider, for example — then you should create a table in it named
RDB$MAP, with the same structure as RDB$MAP in a Firebird 3.0 or higher database
and with SYSDBA-only write access.
The USING clause describes the mapping source. It has a complex set of options:
• an explicit plugin name (PLUGIN plugin_name) means it applies only for that plugin
• it can use any available plugin ( ANY PLUGIN); although not if the source is the product of a
previous mapping
• it can be made to work only with server-wide plugins (SERVERWIDE)
• it can be made to work only with previous mapping results (MAPPING)
• you can omit to use of a specific method by using the asterisk (*) argument
• it can specify the name of the database that defined the mapping for the FROM object ( IN
database)
 This argument is not valid for mapping server-wide authentication.
The FROM clause describes the object to map. The FROM clause has a mandatory argument, the type of
the object named. It has the following options:
• When mapping names from plugins, type is defined by the plugin
• When mapping the product of a previous mapping, type can be only USER or ROLE
Chapter 14. Security
615

<!-- Original PDF Page: 617 -->

• If an explicit from_name is provided, it will be taken into account by this mapping
• Use the ANY keyword to work with any name of the given type.
The TO clause specifies the user or role that is the result of the mapping. The to_name is optional. If
it is not specified, then the original name of the mapped object will be used.
For roles, the role defined by a mapping rule is only applied when the user does not explicitly
specify a role on connect. The mapped role can be assumed later in the session using SET TRUSTED
ROLE, even when the mapped role is not explicitly granted to the user.
Who Can Create a Mapping
The CREATE MAPPING statement can be executed by:
• Administrators
• The database owner — if the mapping is local
• Users with the CHANGE_MAPPING_RULES system privilege — if the mapping is local
CREATE MAPPING examples
1. Enable use of Windows trusted authentication in all databases that use the current security
database:
CREATE GLOBAL MAPPING TRUSTED_AUTH
  USING PLUGIN WIN_SSPI
  FROM ANY USER
  TO USER;
2. Enable RDB$ADMIN access for windows admins in the current database:
CREATE MAPPING WIN_ADMINS
  USING PLUGIN WIN_SSPI
  FROM Predefined_Group
  DOMAIN_ANY_RID_ADMINS
  TO ROLE RDB$ADMIN;

The group DOMAIN_ANY_RID_ADMINS does not exist in Windows, but such a name
would be added by the Win_Sspi plugin to provide exact backwards
compatibility.
3. Enable a particular user from another database to access the current database with another
name:
CREATE MAPPING FROM_RT
  USING PLUGIN SRP IN "rt"
Chapter 14. Security
616

<!-- Original PDF Page: 618 -->

FROM USER U1 TO USER U2;
 Database names or aliases will need to be enclosed in double quotes on
operating systems that have case-sensitive file names.
4. Enable the server’s SYSDBA (from the main security database) to access the current database.
(Assume that the database is using a non-default security database):
CREATE MAPPING DEF_SYSDBA
  USING PLUGIN SRP IN "security.db"
  FROM USER SYSDBA
  TO USER;
5. Ensure users who logged in using the legacy authentication plugin do not have too many
privileges:
CREATE MAPPING LEGACY_2_GUEST
  USING PLUGIN legacy_auth
  FROM ANY USER
  TO USER GUEST;
See also
ALTER MAPPING, CREATE OR ALTER MAPPING, DROP MAPPING
14.7.3. ALTER MAPPING
Alters a mapping of a security object
Available in
DSQL
Syntax
ALTER [GLOBAL] MAPPING name
  USING
    { PLUGIN plugin_name [IN database]
    | ANY PLUGIN [IN database | SERVERWIDE]
    | MAPPING [IN database] | '*' [IN database] }
  FROM {ANY type | type from_name}
  TO {USER | ROLE} [to_name]
For details on the options, see CREATE MAPPING.
The ALTER MAPPING statement allows you to modify any of the existing mapping options, but a local
mapping cannot be changed to GLOBAL or vice versa.
Chapter 14. Security
617

<!-- Original PDF Page: 619 -->

 Global and local mappings of the same name are different objects.
Who Can Alter a Mapping
The ALTER MAPPING statement can be executed by:
• Administrators
• The database owner — if the mapping is local
• Users with the CHANGE_MAPPING_RULES system privilege — if the mapping is local
ALTER MAPPING examples
Alter mapping
ALTER MAPPING FROM_RT
  USING PLUGIN SRP IN "rt"
  FROM USER U1 TO USER U3;
See also
CREATE MAPPING, CREATE OR ALTER MAPPING, DROP MAPPING
14.7.4. CREATE OR ALTER MAPPING
Creates a mapping of a security object if it doesn’t exist, or alters a mapping
Available in
DSQL
Syntax
CREATE OR ALTER [GLOBAL] MAPPING name
  USING
    { PLUGIN plugin_name [IN database]
    | ANY PLUGIN [IN database | SERVERWIDE]
    | MAPPING [IN database] | '*' [IN database] }
  FROM {ANY type | type from_name}
  TO {USER | ROLE} [to_name]
For details on the options, see CREATE MAPPING.
The CREATE OR ALTER MAPPING statement creates a new or modifies an existing mapping.
 Global and local mappings of the same name are different objects.
CREATE OR ALTER MAPPING examples
Chapter 14. Security
618

<!-- Original PDF Page: 620 -->

Creating or altering a mapping
CREATE OR ALTER MAPPING FROM_RT
  USING PLUGIN SRP IN "rt"
  FROM USER U1 TO USER U4;
See also
CREATE MAPPING, ALTER MAPPING, DROP MAPPING
14.7.5. DROP MAPPING
Drops a mapping of a security object
Available in
DSQL
Syntax
DROP [GLOBAL] MAPPING name
Table 265. DROP MAPPING Statement Parameter
Parameter Description
name Mapping name
The DROP MAPPING statement removes an existing mapping. If GLOBAL is specified, then a global
mapping will be removed.
 Global and local mappings of the same name are different objects.
Who Can Drop a Mapping
The DROP MAPPING statement can be executed by:
• Administrators
• The database owner — if the mapping is local
• Users with the CHANGE_MAPPING_RULES system privilege — if the mapping is local
DROP MAPPING examples
Alter mapping
DROP MAPPING FROM_RT;
See also
CREATE MAPPING
Chapter 14. Security
619

<!-- Original PDF Page: 621 -->

14.8. Database Encryption
Firebird provides a plugin mechanism to encrypt the data stored in the database. This mechanism
does not encrypt the entire database, but only data pages, index pages, and blob pages.
To make database encryption possible, you need to obtain or write a database encryption plugin.

Out of the box, Firebird does not include a database encryption plugin.
The encryption plugin example in examples/dbcrypt does not perform real
encryption, it is only intended as an example how such a plugin can be written.
On Linux, an example plugin named libDbCrypt_example.so can be found in
plugins/.
The main problem with database encryption is how to store the secret key. Firebird provides
support for transferring the key from the client, but this does not mean that storing the key on the
client is the best way; it is one of several alternatives. However, keeping encryption keys on the
same disk as the database is an insecure option.
For efficient separation of encryption and key access, the database encryption plugin data is
divided into two parts, the encryption itself and the holder of the secret key. This can be an efficient
approach when you want to use a good encryption algorithm, but you have your own custom
method of storing the keys.
Once you have decided on the plugin and key-holder, you can perform the encryption.
14.8.1. Encrypting a Database
Encrypts the database using the specified encryption plugin
Syntax
ALTER {DATABASE | SCHEMA}
  ENCRYPT WITH plugin_name [KEY key_name]
Table 266. ALTER DATABASE ENCRYPT Statement Parameters
Parameter Description
plugin_name The name of the encryption plugin
key_name The name of the encryption key
Encryption starts immediately after this statement completes, and will be performed in the
background. Normal operations of the database are not disturbed during encryption.
The optional KEY clause specifies the name of the key for the encryption plugin. The plugin decides
what to do with this key name.
 The encryption process can be monitored using the MON$CRYPT_PAGE field in the
Chapter 14. Security
620

<!-- Original PDF Page: 622 -->

MON$DATABASE virtual table, or viewed in the database header page using gstat -e.
gstat -h will also provide limited information about the encryption status.
For example, the following query will display the progress of the encryption
process as a percentage.
select MON$CRYPT_PAGE * 100 / MON$PAGES
  from MON$DATABASE;
 SCHEMA is currently a synonym for DATABASE; this may change in a future version, so
we recommend to always use DATABASE
See also
Decrypting a Database, ALTER DATABASE
14.8.2. Decrypting a Database
Decrypts the database using the configured plugin and key
Syntax
ALTER {DATABASE | SCHEMA} DECRYPT
Decryption starts immediately after this statement completes, and will be performed in the
background. Normal operations of the database are not disturbed during decryption.
 SCHEMA is currently a synonym for DATABASE; this may change in a future version, so
we recommend to always use DATABASE
See also
Encrypting a Database, ALTER DATABASE
14.9. SQL Security
The SQL SECURITY clause of various DDL statements enables executable objects (triggers, stored
procedures, stored functions) to be defined to run in a specific context of privileges.
The SQL Security feature has two contexts: INVOKER and DEFINER. The INVOKER context corresponds to
the privileges available to the current user or the calling object, while DEFINER corresponds to those
available to the owner of the object.
The SQL SECURITY property is an optional part of an object’s definition that can be applied to the
object with DDL statements. The property cannot be dropped, but it can be changed from INVOKER to
DEFINER and vice versa.
This is not the same thing as SQL privileges, which are applied to users and database objects to give
them various types of access to other database objects. When an executable object in Firebird needs
Chapter 14. Security
621