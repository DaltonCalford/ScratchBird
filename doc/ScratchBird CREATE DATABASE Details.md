## **CREATE DATABASE Statement Details**

The CREATE DATABASE statement in ScratchBird is used to create a new database. The command structure allows for specifying the database file path, along with a variety of initial and server-side options.

The general syntax is:

CREATE DATABASE 'database\_spec'  
    \[db\_initial\_option \[db\_initial\_option ...\]\]  
    \[db\_remote\_option \[db\_remote\_option ...\]\];

### **Database Specification**

* **'database\_spec'**: This is a mandatory string literal specifying the path and filename for the new primary database file.

### **Initial Options (Client-Side)**

These options are typically processed by the client tool (isql, for example) before the command is sent to the database server.

* **USER 'user\_name'**: Specifies the user name to connect with.  
* **OWNER 'owner\_name'**: Specifies the owner of the database. This is a synonym for USER.  
* **PASSWORD 'password'**: Provides the password for the specified user.  
* **ROLE 'role\_name'**: Specifies a role to be used for the connection that creates the database.  
* **PAGE\_SIZE \[=\] size**: Sets the database page size in bytes. Common values are 4096, 8192, 16384, and 32768\.  
* **SET NAMES 'charset\_name'**: Sets the default character set for the connection.  
* **PASCAL CASE IDENTIFIERS**: A legacy option to handle identifiers using Pascal casing rules.

### **Remote Options (Server-Side)**

These options are processed by the ScratchBird server during database creation.

* **DEFAULT CHARACTER SET 'charset\_name' \[COLLATE 'collation\_name'\]**: Sets the default character set and, optionally, the default collation for the entire database. All subsequent character data columns will use this setting unless explicitly overridden.  
* **DIFFERENCE FILE 'path'**: Specifies a path for the database's delta file, which is used for nBackup purposes.
