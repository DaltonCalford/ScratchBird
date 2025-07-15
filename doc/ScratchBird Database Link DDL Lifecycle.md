## **In-Depth Analysis of Database Link DDL**

The DATABASE LINK object is a new feature designed to create a persistent, named connection to a remote ScratchBird database. This allows statements in the local database to seamlessly access tables and views on the remote database without specifying connection details in every query.

### **1\. CREATE DATABASE LINK**

This statement creates a new, named database link.

**Syntax:**

CREATE DATABASE LINK \[IF NOT EXISTS\] link\_name  
    TO 'connection\_string'  
    USER 'user\_name'  
    PASSWORD 'password'  
    \[SCHEMA\_MODE {FIXED | CONTEXT\_AWARE | HIERARCHICAL | MIRROR}\];

#### **Clause Breakdown:**

* **link\_name**: The unique name for this database link.  
* **IF NOT EXISTS**: (Optional) Prevents an error if a link with the same name already exists.  
* **TO 'connection\_string'**: The mandatory connection string for the remote database (e.g., 'hostname/3050:/path/to/remote.fdb').  
* **USER 'user\_name'**: The user name for authenticating to the remote database.  
* **PASSWORD 'password'**: The password for the specified user.  
* **SCHEMA\_MODE ...**: (Optional) Defines how schemas are treated when accessed through the link.  
  * FIXED: (Default) All remote access occurs within a single, fixed schema on the remote database, likely specified in the connection string or as a default for the remote user.  
  * CONTEXT\_AWARE: The remote schema context is determined by the session's current schema on the local database.  
  * HIERARCHICAL: Preserves the hierarchical nature of schemas across the link.  
  * MIRROR: Attempts to mirror the schema structure and context from the local to the remote database.

### **2\. ALTER DATABASE LINK**

This statement modifies an existing database link. The primary modifiable property is the schema mode.

**Syntax:**

ALTER DATABASE LINK link\_name  
    \[SCHEMA\_MODE {FIXED | CONTEXT\_AWARE | HIERARCHICAL | MIRROR}\];

* You cannot change the connection string or credentials with ALTER. To do that, you must DROP and RECREATE the link.

### **3\. DROP DATABASE LINK**

Permanently deletes a database link object.

**Syntax:**

DROP DATABASE LINK \[IF EXISTS\] link\_name;

* **IF EXISTS**: Prevents an error if the link does not exist.

### **4\. Using Database Links in DML**

Once a link is created, you can access its tables and views in DML statements by appending @link\_name to the table name.

**Syntax:**

SELECT ... FROM table\_name AT link\_name;  
INSERT INTO table\_name AT link\_name ...;  
UPDATE table\_name AT link\_name SET ...;  
DELETE FROM table\_name AT link\_name WHERE ...;

**Example:**

\-- Create a link to a remote inventory database  
CREATE DATABASE LINK inventory\_db  
    TO 'server2:/dbs/inventory.fdb'  
    USER 'app\_user'  
    PASSWORD 'secret';

\-- Select data from a remote table  
SELECT product\_name, quantity\_on\_hand  
FROM WAREHOUSE\_STOCK AT inventory\_db  
WHERE location \= 'MAIN';  
