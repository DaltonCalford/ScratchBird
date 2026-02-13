# App F Security Tables



<!-- Original PDF Page: 772 -->

Column Name Data Type Description
MON$READ_ONLY SMALLINT Flag indicating whether the transaction
is read-only (value 1) or read-write
(value 0)
MON$AUTO_COMMIT SMALLINT Flag indicating whether automatic
commit is used for the transaction
(value 1) or not (value 0)
MON$AUTO_UNDO SMALLINT Flag indicating whether the logging
mechanism automatic undo is used for
the transaction (value 1) or not (value 0)
MON$STAT_ID INTEGER Statistics identifier
Getting all connections that started Read Write transactions with isolation level above Read Commited
SELECT DISTINCT a. *
FROM mon$attachments a
JOIN mon$transactions t ON a.mon$attachment_id = t.mon$attachment_id
WHERE NOT (t.mon$read_only = 1 AND t.mon$isolation_mode >= 2)
Appendix E: Monitoring Tables
771

<!-- Original PDF Page: 773 -->

Appendix F: Security tables
The names of the security tables have SEC$ as prefix. They display data from the current security
database. These tables are virtual in the sense that before access by the user, no data is recorded in
them. They are filled with data at the time of user request. However, the descriptions of these tables
are constantly present in the database. In this sense, these virtual tables are like tables of the MON$
-family used to monitor the server.
Security
• SYSDBA, users with the RDB$ADMIN role in the security database and the current database, and
the owner of the security database have full access to all information provided by the security
tables.
• Regular users can only see information on themselves, other users are not visible.
 These features are highly dependent on the user management plugin. Keep in
mind that some options are ignored when using a legacy control plugin users.
List of security tables
SEC$DB_CREATORS
Lists users and roles granted the CREATE DATABASE privilege
SEC$GLOBAL_AUTH_MAPPING
Information about global authentication mappings
SEC$USERS
Lists users in the current security database
SEC$USER_ATTRIBUTES
Additional attributes of users
SEC$DB_CREATORS
Lists users and roles granted the CREATE DATABASE privilege.
Column Name Data Type Description
SEC$USER CHAR(63) Name of the user or role
SEC$USER_TYPE SMALLINT Type of user:
8 - user
13 - role
SEC$GLOBAL_AUTH_MAPPING
Lists users and roles granted the CREATE DATABASE privilege.
Appendix F: Security tables
772

<!-- Original PDF Page: 774 -->

Column Name Data Type Description
SEC$MAP_NAME CHAR(63) Name of the mapping
SEC$MAP_USING CHAR(1) Using definition:
P - plugin (specific or any)
S - any plugin serverwide
M - mapping
* - any method
SEC$MAP_PLUGIN CHAR(63) Mapping applies for authentication
information from this specific plugin
SEC$MAP_DB CHAR(63) Mapping applies for authentication
information from this specific database
SEC$MAP_FROM_TYPE CHAR(63) The type of authentication object
(defined by plugin) to map from, or * for
any type
SEC$MAP_FROM CHAR(255) The name of the authentication object to
map from
SEC$MAP_TO_TYPE SMALLINT Nullable The type to map to
0 - USER
1 - ROLE
SEC$MAP_TO CHAR(63) The name to map to
SEC$DESCRIPTION BLOB TEXT Comment on the mapping
SEC$USERS
Lists users in the current security database.
Column Name Data Type Description
SEC$USER_NAME CHAR(63) Username
SEC$FIRST_NAME VARCHAR(32) First name
SEC$MIDDLE_NAME VARCHAR(32) Middle name
SEC$LAST_NAME VARCHAR(32) Last name
SEC$ACTIVE BOOLEAN true - active, false - inactive
SEC$ADMIN BOOLEAN true - user has admin role in security
database, false otherwise
SEC$DESCRIPTION BLOB TEXT Description (comment) on the user
SEC$PLUGIN CHAR(63) Authentication plugin name that
manages this user
 Multiple users van exist with the same username, each managed by a different
Appendix F: Security tables
773