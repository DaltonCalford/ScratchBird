# **DDL Specification: Databases**

## **Overview**

A database is the top-level container for all schemas, tables, and other objects in a ScratchBird instance. The CREATE DATABASE command initializes the physical files, default tablespaces, and system catalogs required for the database to operate.

**Emulation Note:** For emulated dialects, CREATE DATABASE is metadata-only and maps to schema creation; no physical database files are created.

## **CREATE DATABASE**

Initializes a new database with specified properties.

### **Syntax**

CREATE DATABASE \[ IF NOT EXISTS \] \<database\_name\>  
    \[ PAGE\_SIZE \= {8K|16K|32K|64K|128K} \]  
    \[ DEFAULT CHARACTER SET \<charset\_name\> \]  
    \[ DEFAULT COLLATE \<collation\_name\> \]  
    \[ ENCRYPTED \[ WITH PASSWORD \<string\_literal\> \] \]  
    \[ OWNER \<user\_name\> \];

### **Parameters**

* IF NOT EXISTS: Prevents an error if the database already exists.  
* PAGE\_SIZE: Sets the fundamental block size for data storage. This cannot be changed after creation. Default is 16K.  
* DEFAULT CHARACTER SET: Specifies the default character set for all text-based columns.  
* DEFAULT COLLATE: Specifies the default collation (sort order) for string comparisons.  
* ENCRYPTED: Enables transparent data encryption (TDE) for the entire database at rest.  
* OWNER: Assigns ownership of the database to a specific user.

### **Examples**

**1\. Create a simple database:**

CREATE DATABASE my\_application\_db;

**2\. Create a database with specific options for internationalization:**

CREATE DATABASE ecommerce\_platform  
    PAGE\_SIZE \= 32K  
    DEFAULT CHARACTER SET 'UTF8'  
    DEFAULT COLLATE 'en\_US.UTF-8';

**3\. Create an encrypted, user-owned database:**

CREATE DATABASE secure\_hr\_data  
    ENCRYPTED WITH PASSWORD 'a\_very\_strong\_secret'  
    OWNER 'db\_admin';

## **CREATE DATABASE EMULATED**

Creates a metadata-only emulated database and the emulation catalog views under
`emulation.<dialect>.<server>.<path>.<database>`. No physical database file is created.

### **Syntax**

CREATE DATABASE \[ IF NOT EXISTS \] EMULATED \<dialect\>  
    \[ ON SERVER \<server\_name\> \] \<source\_spec\>  
    \[ ALIAS \<alias\_name\> \[, \<alias\_name\> ... \] \]  
    \[ WITH \[ OPTIONS \] ( \<option\_key\> \= \<option\_value\> \[, ... \] ) \];

### **Parameters**

* EMULATED: Marks this as a metadata-only emulated database.  
* \<dialect\>: Emulated engine name (e.g., `mysql`, `postgresql`, `firebird`).  
* ON SERVER: Optional server name. Defaults to `localhost`.  
* \<source\_spec\>: Identifier or string literal describing the source database.  
  * String literals may include OS paths or `server:/path` specs.  
  * Path components become schema path components, and the file name becomes the database name (extension stripped).  
* ALIAS: Optional list of alias names to create as synonyms under `emulated.<dialect>`.  
* WITH OPTIONS: Arbitrary key/value metadata stored with the emulated database record.

### **Examples**

**1\. Create a MySQL emulated database with defaults:**

CREATE DATABASE EMULATED mysql mydb;

**2\. Create a Firebird emulated database from a file path with aliases and options:**

CREATE DATABASE EMULATED firebird 'srv:/var/db/employee.fdb'  
    ALIAS legacy, emp  
    WITH OPTIONS (user = 'SYSDBA', password = 'masterkey');

## **ALTER DATABASE**

Modifies the properties of an existing database.

### **Syntax**

ALTER DATABASE \<database\_name\>  
    { RENAME TO \<new\_database\_name\>  
    | OWNER TO \<new\_owner\>  
    | SET DEFAULT CHARACTER SET \<charset\_name\>  
    | SET DEFAULT COLLATE \<collation\_name\>  
    | SET SWEEP INTERVAL \<integer\> };

### **Parameters**

* RENAME TO: Changes the name of the database.  
* OWNER TO: Transfers ownership to a different user.  
* SET DEFAULT ...: Changes the default character set or collation for *newly created* objects.  
* SET SWEEP INTERVAL: Configures the automatic garbage collection interval.

### **Examples**

**1\. Rename a database:**

ALTER DATABASE my\_app\_db\_v1 RENAME TO my\_app\_db;

**2\. Change the owner of a database:**

ALTER DATABASE secure\_hr\_data OWNER TO 'hr\_admin\_user';

## **DROP DATABASE**

Removes an entire database, including all its objects and data files. This operation is irreversible.

### **Syntax**

DROP DATABASE \[ IF EXISTS \] \<database\_name\> \[ CASCADE | RESTRICT \];

### **Parameters**

* IF EXISTS: Prevents an error if the database does not exist.  
* CASCADE: (Not typically used for databases, more for schemas/objects)  
* RESTRICT: (Default) Prevents dropping if there are active connections.

### **Examples**

**1\. Drop a database:**

DROP DATABASE old\_test\_db;

**2\. Safely drop a database that might not exist:**

DROP DATABASE IF EXISTS temp\_reporting\_db;  
