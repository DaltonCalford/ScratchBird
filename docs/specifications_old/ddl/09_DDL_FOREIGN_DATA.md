# **Foreign Data Specification**

## **1\. Introduction**

ScratchBird supports querying remote databases and external data sources through its implementation of **SQL/MED (SQL Management of External Data)**, commonly known as **Foreign Data Wrappers (FDW)**. This powerful feature allows you to define connections to external systems and interact with their data as if they were local tables.

The FDW system is comprised of three core DDL objects that work together to establish and manage these connections.

### **FDW Object Hierarchy**

1. **FOREIGN DATA WRAPPER (The Driver)**: A schema-level object that specifies the driver responsible for communicating with a particular type of external data source (e.g., PostgreSQL, Oracle, a CSV file system). *This is often pre-installed.*  
2. **SERVER (The Connection)**: Defines a specific connection to an external server using a particular foreign data wrapper. It contains connection details like host, port, and database name.  
3. **USER MAPPING (The Credentials)**: Stores the authentication credentials (e.g., username and password) for a local user to connect to the remote server.  
4. **FOREIGN TABLE (The Proxy Table)**: A local table definition that maps to a table on the remote server. Queries to this local table are translated and executed on the remote system.

This document details the lifecycle (CREATE, ALTER, DROP) for these objects.

## **Pass-through Execution (sys.\*)**

Foreign data wrappers can be paired with the built-in sys passthrough routines
to run raw SQL or stored routine calls against a configured foreign server.

- `sys.remote_exec` and `sys.remote_call` are defined in `DDL_PROCEDURES.md`
- `sys.remote_query` is defined in `DDL_FUNCTIONS.md`

These routines are used by UDR connector plugins to allow controlled DDL/DML/PSQL
execution on remote databases during migration or operational passthrough.

## **2\. Foreign Data Wrappers**

A Foreign Data Wrapper is the underlying plugin or driver that knows how to communicate with a specific type of remote data source. These are typically managed by administrators.

### **CREATE FOREIGN DATA WRAPPER**

This command registers a new FDW driver with the database.

**Syntax:**

CREATE FOREIGN DATA WRAPPER name  
    \[ HANDLER handler\_function | NO HANDLER \]  
    \[ VALIDATOR validator\_function | NO VALIDATOR \]  
    \[ OPTIONS ( option 'value' \[, ... \] ) \];

**Example:**

\-- Register the built-in PostgreSQL FDW handler  
CREATE FOREIGN DATA WRAPPER postgres\_fdw  
    HANDLER postgres\_fdw\_handler  
    VALIDATOR postgres\_fdw\_validator;

\-- Register a wrapper for reading file-based data  
CREATE FOREIGN DATA WRAPPER file\_fdw  
    HANDLER file\_fdw\_handler;

## **3\. Servers**

A SERVER object defines a specific connection to a remote resource using a registered FDW.

### **CREATE SERVER**

This command defines a new remote server connection.

**Syntax:**

CREATE SERVER server\_name  
    \[ TYPE 'server\_type' \]  
    \[ VERSION 'server\_version' \]  
    FOREIGN DATA WRAPPER fdw\_name  
    \[ OPTIONS ( option 'value' \[, ... \] ) \];

**Common Options:**

* host: The hostname or IP address of the remote server.  
* port: The port number the remote server is listening on.  
* dbname: The name of the database to connect to.

**Example:**

\-- Define a connection to a remote PostgreSQL reporting database  
CREATE SERVER reporting\_db\_pg  
    FOREIGN DATA WRAPPER postgres\_fdw  
    OPTIONS (  
        host 'reports.example.com',  
        port '5432',  
        dbname 'analytics'  
    );

### **ALTER SERVER**

Modifies the definition of a foreign server.

**Syntax:**

ALTER SERVER server\_name  
    \[ VERSION 'server\_version' \]  
    \[ OPTIONS ( \[ ADD | SET | DROP \] option \['value'\] \[, ... \] ) \]  
    OWNER TO { new\_owner | CURRENT\_USER | SESSION\_USER }  
    RENAME TO new\_name;

**Example:**

\-- Change the host for the reporting server to a new address  
ALTER SERVER reporting\_db\_pg  
    OPTIONS (SET host 'new-reports.example.com');

### **DROP SERVER**

Removes a foreign server definition.

**Syntax:**

DROP SERVER \[ IF EXISTS \] server\_name \[ CASCADE | RESTRICT \];

* CASCADE: Automatically drops objects that depend on the server (like user mappings and foreign tables).  
* RESTRICT: (Default) Prevents dropping the server if any objects depend on it.

## **4\. User Mappings**

A USER MAPPING defines the credentials for a specific local user to connect to a specific foreign server.

### **CREATE USER MAPPING**

This command creates the authentication link.

**Syntax:**

CREATE USER MAPPING FOR { user\_name | USER | CURRENT\_USER | PUBLIC }  
    SERVER server\_name  
    \[ OPTIONS ( option 'value' \[, ... \] ) \];

**Common Options:**

* user: The username to use on the remote server.  
* password: The password for the remote user.

**Example:**

\-- Map the local 'reporting\_role' to a remote user and password  
CREATE USER MAPPING FOR reporting\_role  
    SERVER reporting\_db\_pg  
    OPTIONS (  
        user 'remote\_readonly\_user',  
        password 'a\_secure\_password\_here'  
    );

\-- Map the current user to a different remote user  
CREATE USER MAPPING FOR CURRENT\_USER  
    SERVER reporting\_db\_pg  
    OPTIONS (user 'my\_remote\_admin\_user');

### **ALTER USER MAPPING**

Modifies the options for a user mapping, typically to update a password.

**Syntax:**

ALTER USER MAPPING FOR { user\_name | USER | CURRENT\_USER | PUBLIC }  
    SERVER server\_name  
    OPTIONS ( \[ ADD | SET | DROP \] option \['value'\] \[, ... \] );

### **DROP USER MAPPING**

Removes a user mapping.

**Syntax:**

DROP USER MAPPING \[ IF EXISTS \] FOR { user\_name | USER | CURRENT\_USER | PUBLIC }  
    SERVER server\_name;

## **5\. Foreign Tables**

A FOREIGN TABLE is the local representation of a remote table. You query it using standard SELECT statements, and ScratchBird uses the FDW to translate the query for the remote system.

### **CREATE FOREIGN TABLE**

Defines a proxy table. The column definitions must generally match the remote table's structure.

**Syntax:**

CREATE FOREIGN TABLE \[ IF NOT EXISTS \] table\_name (  
    column\_name data\_type \[ OPTIONS ( ... ) \] \[ COLLATE ... \] \[ constraints \]  
    \[, ... \]  
)  
SERVER server\_name  
\[ OPTIONS ( option 'value' \[, ... \] ) \];

**Common Options:**

* schema\_name: The schema of the remote table.  
* table\_name: The name of the remote table (if different from the local name).

**Example:**

\-- Create a local foreign table that points to the 'quarterly\_sales' table on the remote server  
CREATE FOREIGN TABLE local\_sales\_reports (  
    sale\_id UUID,  
    product\_name TEXT,  
    sale\_date DATE,  
    amount DECIMAL(18, 2\)  
)  
SERVER reporting\_db\_pg  
OPTIONS (  
    schema\_name 'public',  
    table\_name 'quarterly\_sales'  
);

\-- Now you can query the foreign table like a local one  
SELECT product\_name, SUM(amount)  
FROM local\_sales\_reports  
WHERE sale\_date \>= '2025-01-01'  
GROUP BY product\_name;

### **ALTER FOREIGN TABLE**

Modifies the local definition of the foreign table.

**Syntax:**

ALTER FOREIGN TABLE \[ IF EXISTS \] name  
    ADD column\_name data\_type ...,  
    DROP COLUMN column\_name ...,  
    ALTER COLUMN column\_name TYPE data\_type ...,  
    OPTIONS ( \[ ADD | SET | DROP \] option \['value'\] \[, ... \] ),  
    OWNER TO new\_owner,  
    RENAME TO new\_name;

### **DROP FOREIGN TABLE**

Removes the local foreign table definition without affecting the remote table.

**Syntax:**

DROP FOREIGN TABLE \[ IF EXISTS \] table\_name;  
